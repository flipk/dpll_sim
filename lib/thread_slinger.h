/* -*- Mode:c++; eval:(c-set-style "BSD"); c-basic-offset:4; indent-tabs-mode:nil; tab-width:8 -*- */

#ifndef __THREADSLINGER_H__
#define __THREADSLINGER_H__
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

#include <pthread.h>
#include <inttypes.h>
#include <vector>

#include "LockWait.h"
#include "dll3.h"

namespace ThreadSlinger {

/** errors from this library */
struct ThreadSlingerError : BackTraceUtil::BackTrace {
    /** errors that this library may throw */
    enum errValue {
        MessageOnListDestructor, //!< message still on a list during destructor
        MessageNotFromThisPool,  //!< message freed to wrong pool
        DerefNoPool,             //!< refcount down to zero but poolptr is null
        __NUMERRS
    } err;
    // this can't be a std::string because it may get invoked
    // by very early global constructors prior to main() starting,
    // which means any std::strings present are not guaranteed to
    // be fully constructed yet.
    static const char * errStrings[__NUMERRS];
    ThreadSlingerError(errValue _e) : err(_e) {
        std::cerr << "ThreadSlingerError:\n" << Format();
    }
    /** handy utility function for printing error and stack backtrace */
    /*virtual*/ const std::string _Format(void) const;
};

class thread_slinger_pool_base;
typedef DLL3::List<thread_slinger_pool_base,1>  poolList_t;

class thread_slinger_message;

class thread_slinger_pool_base : public poolList_t::Links {
public:
    virtual ~thread_slinger_pool_base(void) { }
    virtual void release(thread_slinger_message * m) = 0;
    virtual void getCounts(int &used, int &free, std::string &name) = 0;
};

typedef DLL3::List<thread_slinger_message,1,false,false> messageList_t;

/** base class for all user messages to go through thread_slinger.
 * \note Any class which derives from this must implement
 * "void init(args) { }" which will be called by thread_slinger_pool
 * during alloc(), and "void cleanup(void) { }" which will be called
 * during release(). The InitArgs parameter to thread_slinger_pool
 * determines the arg types passed to "init". */
class thread_slinger_message : public messageList_t::Links
{
    int refcount;
    WaitUtil::Lockable refcountlock;
public:
    thread_slinger_pool_base * _slinger_pool;
    thread_slinger_message(void);
    virtual ~thread_slinger_message(void);
    /** return the message's name, user of this class may override this */
    virtual const std::string msgName(void) {
        return "thread_slinger_message";
    }
    /** increase reference count */
    void ref(void);
    /** decrease reference count; if it hits zero, this buffer will be
     * automatically returned to the pool, returns true if this deref
     * was the one that freed it */
    bool deref(void);
};

class _thread_slinger_queue
{
    pthread_mutex_t   mutex;
    pthread_cond_t  _waiter;
    pthread_cond_t  *waiter;
    // it's a little bit of a waste to create a sempahore
    // in every queue; when using the multiple-queue version
    // of _dequeue, only one of the semaphores is used. however,
    // this is cheaper IMHO than creating and destroying a semaphore
    // every time you enter and leave _dequeue(), which is the alternative.
    WaitUtil::Semaphore   _waiter_sem;
    WaitUtil::Semaphore *  waiter_sem;
    messageList_t  msgs;
    void   lock( void ) { pthread_mutex_lock  ( &mutex ); }
    void unlock( void ) { pthread_mutex_unlock( &mutex ); }
    inline thread_slinger_message * __dequeue(void);
    inline thread_slinger_message * __dequeue_tail(void);
    inline thread_slinger_message * _dequeue_int(int uSecs, bool dotail);
    static inline struct timespec * setup_abstime(int uSecs,
                                                  struct timespec *abstime);
protected:
    _thread_slinger_queue(pthread_mutexattr_t *mattr = NULL,
                          pthread_condattr_t  *cattr = NULL);
    ~_thread_slinger_queue(void);
    void _enqueue(thread_slinger_message *);
    void _enqueue_head(thread_slinger_message *);
    thread_slinger_message * _dequeue(int uSecs);
    thread_slinger_message * _dequeue_tail(int uSecs);
    int _get_count(void) const { return msgs.get_cnt(); }
    static thread_slinger_message * _dequeue(
        _thread_slinger_queue ** queues,
        int num_queues, int uSecs,
        int *which_queue=NULL);
    void * _get_head(void) { return msgs.get_head(); }
};

/** a message queue of user objects, declare a derived type from
 * thread_slinger_message and specify that as T.
 * \param  T  the type to send through the queue, derived
 *          from thread_slinger_message base type */
template <class T>
class thread_slinger_queue : public _thread_slinger_queue
{
public:
    thread_slinger_queue(pthread_mutexattr_t *mattr = NULL,
                         pthread_condattr_t  *cattr = NULL)
        : _thread_slinger_queue(mattr,cattr) { }
    /** send a message to the receiver.
     * \param msg a user's message derived from thread_slinger_message */
    void enqueue(T * msg);
    void enqueue_head(T * msg);
    /** fetch a message from the sender.
     * \param uSecs  if >0, wait for that time and return NULL if no
     *         message; if <0, wait forever for a message; if 0,
     *         return NULL immediately if no message.
     * \return NULL if timeout, or a message pointer */
    T * dequeue(int uSecs=0);
    T * dequeue_tail(int uSecs=0);
    /** find out how many messages are currently queued */
    int get_count(void) const;
    /** dequeue from a set of queues in priority order.
     * \param queues  the list of queues to check, the priority is
     *      specified by the order in this list (first queue in this
     *      list is checked first).
     * \param num_queues  the dimension of the queues array
     * \param uSecs  the time to wait for a message: >0 wait up to
     *       that many usecs; <0 wait forever; ==0 return NULL
     *       immediately if all queues empty.
     * \param which_queue  an optional pointer to an integer, which
     *       on return of a valid message pointer will indicate which
     *       queue this message came from (an index in queues array)
     * \return a message pointer or NULL if timeout */
    static T * dequeue(thread_slinger_queue<T> * queues[],
                       int num_queues,
                       int uSecs, int *which_queue=NULL);
    T * get_head(void) { return (T*) _get_head(); }
    bool empty(void) const { return _get_count() == 0; }
};

/** data for a pool, retrieved from thread_slinger_pools::report_pools */
struct poolReport {
    int usedCount;   //!< number of buffers currently used by application
    int freeCount;   //!< number of buffers free in the pool
    std::string name;  //!< name of the pool according to
                       //!< thread_slinger_message::msgName
};
/** data for all pools from thread_slinger_pools::report_pools */
typedef std::vector<poolReport> poolReportList_t;

/** a static class which manages list of all known pools */
class thread_slinger_pools
{
    static poolList_t * lst;
public:
    static void register_pool(thread_slinger_pool_base * p);
    static void unregister_pool(thread_slinger_pool_base * p);
    /** retrieve stats about all pools */
    static void report_pools(poolReportList_t &);
};

/** a convenient buffer pool for storing objects of any kind.
 * \param T   the type of object being stored in the queue,
 *          derived from thread_slinger_message. */
template <class T,typename... InitArgs>
class thread_slinger_pool : public thread_slinger_pool_base
{
    thread_slinger_queue<T>  q;
    /*virtual*/ void release(thread_slinger_message *);
    WaitUtil::Lockable  statsLockable;
    int usedCount;
    int freeCount;
    bool nameSet;
    std::string msgName;
public:
    thread_slinger_pool(pthread_mutexattr_t *mattr = NULL,
                        pthread_condattr_t  *cattr = NULL);
    virtual ~thread_slinger_pool(void);
    /** add more items to this pool. */
    void add(int items);
    /** allocate a buffer from the pool.
     * \param uSecs  how long to wait if the pool is empty:
     *          <0 means wait forever, ==0 means return NULL 
     *          immediately, >0 means wait that number of usecs.
     * \param grow   if true, allocate new buffers if pool is empty.
     * \return a buffer pointer, or NULL if empty and timeout */
    T * alloc(int uSecs=0, bool grow=false,
              InitArgs&&... args);
    /** release a buffer into the pool
     * \param buf  the buffer pointer to release */
    void release(T * buf);
    /** fetch number of buffers currently in the pool */
    void getCounts(int &used, int &free, std::string &name);
    /** return true if no items in pool */
    bool empty(void) const;
    int get_count(void) const { return freeCount; }
};

#include "thread_slinger.tcc"

}; // namespace

std::ostream &operator<<(std::ostream &strm,
                         const ThreadSlinger::poolReport &pr);
std::ostream &operator<<(std::ostream &strm,
                         const ThreadSlinger::poolReportList_t &prs);

/** \mainpage threadslinger user's API documentation

This is the user's manual for the threadslinger API.

Interesting classes:

<ul>
<li> \ref thread_slinger_message
<li> \ref thread_slinger_queue
<li> \ref thread_slinger_pool
</ul>

\code

struct myMessage : public thread_slinger_message
{
    int a; // some field
    int b; // some other field
};

thread_slinger_pool<myMessage,5>  p;

typedef thread_slinger_queue<myMessage> myMsgQ;

myMsgQ q;
myMsgQ q2;

void * t3( void * dummy ) {
    uintptr_t  val = (uintptr_t) dummy;
    while (1) {
        myMessage * m = p.alloc(random()%5000);
        if (m) {
            if (val == 0)
                q.enqueue(m);
            else
                q2.enqueue(m);
            if (val == 0)
                printf("+");
            else
                printf("=");
        } else {
            if (val == 0)
                printf("-");
            else
                printf("_");
        }
        fflush(stdout);
        usleep(random()%30000);
    }
    return NULL;
}

void * t4(void * dummy) {
    myMsgQ * qs[2];
    qs[0] = &q;
    qs[1] = &q2;
    while (1) {
        int which;
        myMessage * m = myMsgQ::dequeue(qs,2,(int)(random()%1000),&which);
        if (m) {
            if (which == 0)
                printf(".");
            else
                printf(",");
            p.release(m);
        } else {
            printf("!");
        }
        fflush(stdout);
        usleep(random()%10000);
    }
    return NULL;
}

int main() {
    pthread_t id;
    pthread_create( &id, NULL, t3, (void*) 0 );
    pthread_create( &id, NULL, t3, (void*) 1 );
    pthread_create( &id, NULL, t4, (void*) 0 );
    pthread_join(id,NULL);
    return 0;
}

\endcode


*/

#endif /* __THREADSLINGER_H__ */
