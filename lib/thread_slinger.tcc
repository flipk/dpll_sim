/* -*- Mode:c++; eval:(c-set-style "BSD"); c-basic-offset:4; indent-tabs-mode:nil; tab-width:8 -*- */
/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>
*/

inline thread_slinger_message::thread_slinger_message(void)
{
    // if this object is allocated via 'new', then the pool ptr
    // shouldn't point anywhere!
    _slinger_pool = NULL;
    refcount = 0;
}

inline thread_slinger_message::~thread_slinger_message(void)
{
    if (onlist())
    {
        ThreadSlingerError tse(ThreadSlingerError::MessageOnListDestructor);
    }
}

inline void
thread_slinger_message::ref(void)
{
    WaitUtil::Lock lock(&refcountlock);
    refcount++;
}

inline bool
thread_slinger_message::deref(void)
{
    bool ret = false;
    WaitUtil::Lock lock(&refcountlock);
    refcount--;
    if (refcount <= 0)
    {
        if (_slinger_pool != NULL)
        {
            refcount = 0;
            _slinger_pool->release(this);
            ret = true;
        }
        else {
            ThreadSlingerError tse(ThreadSlingerError::DerefNoPool);
        }
    }
    return ret;
}

//

template <class T>
void thread_slinger_queue<T>::enqueue(T * msg)
{
    _enqueue(msg);
}

template <class T>
void thread_slinger_queue<T>::enqueue_head(T * msg)
{
    _enqueue_head(msg);
}

template <class T>
T * thread_slinger_queue<T>::dequeue(int uSecs /*=0*/)
{
    return (T *) _dequeue(uSecs);
}

template <class T>
T * thread_slinger_queue<T>::dequeue_tail(int uSecs /*=0*/)
{
    return (T *) _dequeue_tail(uSecs);
}

template <class T>
int thread_slinger_queue<T>::get_count(void) const
{
    return _get_count();
}

//static
template <class T>
T * thread_slinger_queue<T>::dequeue(
    thread_slinger_queue<T> * queues[],
    int num_queues,
    int uSecs, int *which_queue /*=NULL*/)
{
    return (T *) _dequeue((_thread_slinger_queue **) queues,
                          num_queues, uSecs, which_queue);
}

//

template <class T,typename... InitArgs>
thread_slinger_pool<T,InitArgs...>::thread_slinger_pool(
    pthread_mutexattr_t *mattr /*= NULL*/,
    pthread_condattr_t  *cattr /*= NULL*/)
    : q(mattr, cattr), statsLockable(mattr)
{
    usedCount = freeCount = 0;
    thread_slinger_pools::register_pool(this);
}

//virtual
template <class T,typename... InitArgs>
thread_slinger_pool<T,InitArgs...>::~thread_slinger_pool(void)
{
    thread_slinger_pools::unregister_pool(this);
}

template <class T,typename... InitArgs>
void thread_slinger_pool<T,InitArgs...>::add(int items)
{
    while (items-- > 0)
    {
        release(new T);
    }
}

template <class T,typename... InitArgs>
T * thread_slinger_pool<T,InitArgs...>
    ::alloc(int uSecs /*=0*/, bool grow /*=false*/,
            InitArgs&&... args)
{
    T * ret = q.dequeue(uSecs);
    if (ret)
    {
        WaitUtil::Lock  lock(&statsLockable);
        freeCount--;
    }
    if (ret == NULL && grow)
    {
        ret = new T;
    }
    if (ret)
    {
        ret->_slinger_pool = this;
        ret->init(std::forward<InitArgs>(args)...);
        WaitUtil::Lock  lock(&statsLockable);
        usedCount++;
        if (nameSet == false)
        {
            msgName = ret->msgName();
            nameSet = true;
        }
    }
    return ret;
}

template <class T,typename... InitArgs>
void thread_slinger_pool<T,InitArgs...>::release(thread_slinger_message * m)
{
    T * derived = dynamic_cast<T*>(m);
    release(derived);
}

template <class T,typename... InitArgs>
void thread_slinger_pool<T,InitArgs...>::release(T * buf)
{
    // if poolptr is null, this probably came from 'new'
    // and the user wants to add it to the pool.
    bool notnull = buf->_slinger_pool != NULL;
    if (buf->_slinger_pool != this && notnull)
    {
        ThreadSlingerError tse(ThreadSlingerError::MessageNotFromThisPool);
        return;
    }
    buf->cleanup();
    q.enqueue_head(buf);
    WaitUtil::Lock  lock(&statsLockable);
    if (notnull)
        usedCount--;
    freeCount++;
}

template <class T,typename... InitArgs>
void thread_slinger_pool<T,InitArgs...>::getCounts(int &used, int &free,
                                       std::string &name)
{
    WaitUtil::Lock  lock(&statsLockable);
    used = usedCount;
    free = freeCount;
    name = msgName;
}

template <class T,typename... InitArgs>
bool thread_slinger_pool<T,InitArgs...>::empty(void) const
{
    return freeCount == 0;
}

// the mutex must be locked before calling this.
inline thread_slinger_message *
_thread_slinger_queue :: __dequeue(void)
{
    return msgs.dequeue_head();
}

// the mutex must be locked before calling this.
inline thread_slinger_message *
_thread_slinger_queue :: __dequeue_tail(void)
{
    return msgs.dequeue_tail();
}

//static
inline struct timespec *
_thread_slinger_queue :: setup_abstime(int uSecs, struct timespec *abstime)
{
    if (uSecs < 0)
        return NULL;
    clock_gettime( CLOCK_REALTIME, abstime );
    abstime->tv_sec  +=  uSecs / 1000000;
    abstime->tv_nsec += (uSecs % 1000000) * 1000;
    if ( abstime->tv_nsec > 1000000000 )
    {
        abstime->tv_nsec -= 1000000000;
        abstime->tv_sec ++;
    }
    return abstime;
}

inline thread_slinger_message *
_thread_slinger_queue :: _dequeue_int(int uSecs, bool dotail)
{
    thread_slinger_message * pMsg = NULL;
    struct timespec abstime;
    abstime.tv_sec = 0;
    lock();
    if (msgs.get_head() == NULL)
    {
        if (uSecs == 0)
        {
            unlock();
            return NULL;
        }
        while (msgs.get_head() == NULL)
        {
            if (uSecs == -1)
            {
                waiter = &_waiter;
                pthread_cond_wait( waiter, &mutex );
// wrong        waiter = NULL;
            }
            else
            {
                if (abstime.tv_sec == 0)
                    setup_abstime(uSecs, &abstime);
                waiter = &_waiter;
                int ret = pthread_cond_timedwait( waiter, &mutex, &abstime );
// wrong        waiter = NULL;
                if ( ret != 0 )
                    break;
            }
        }
    }
    if (dotail)
        pMsg = __dequeue_tail();
    else
        pMsg = __dequeue();
    unlock();
    return pMsg;
}

inline thread_slinger_message *
_thread_slinger_queue :: _dequeue(int uSecs)
{
    return _dequeue_int(uSecs, false);
}

inline thread_slinger_message *
_thread_slinger_queue :: _dequeue_tail(int uSecs)
{
    return _dequeue_int(uSecs, true);
}
