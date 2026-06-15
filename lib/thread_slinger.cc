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

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "thread_slinger.h"

namespace ThreadSlinger {

const char *
ThreadSlingerError::errStrings[__NUMERRS] = {
    "message still on list in destructor",
    "message not from this pool",
    "dereference hit 0 but pool is null",
};

//virtual
const std::string
ThreadSlingerError::_Format(void) const
{
    std::string ret = "ThreadSlingerError: ";
    ret += errStrings[err];
    ret += " at:\n";
    return ret;
}

//

_thread_slinger_queue :: _thread_slinger_queue(
    pthread_mutexattr_t *mattr /*= NULL*/,
    pthread_condattr_t  *cattr /*= NULL*/)
    : _waiter_sem(mattr, cattr)
{
    pthread_mutexattr_t  _mattr;
    pthread_mutexattr_t *pmattr;
    pthread_condattr_t   _cattr;
    pthread_condattr_t  *pcattr;
    waiter = NULL;
    waiter_sem = NULL;
    if (mattr == NULL)
    {
        pmattr = &_mattr;
        pthread_mutexattr_init( pmattr );
    }
    else
        pmattr = mattr;
    pthread_mutex_init( &mutex, pmattr );
    if (mattr == NULL)
        pthread_mutexattr_destroy( pmattr );
    if (cattr == NULL)
    {
        pcattr = &_cattr;
        pthread_condattr_init( pcattr );
    }
    else
        pcattr = cattr;
    pthread_cond_init( &_waiter, pcattr );
    if (cattr == NULL)
        pthread_condattr_destroy( pcattr );
}

_thread_slinger_queue :: ~_thread_slinger_queue(void)
{
    // cleanup the queue?
    pthread_mutex_destroy( &mutex );
    pthread_cond_destroy( &_waiter );
}

void 
_thread_slinger_queue :: _enqueue(thread_slinger_message * pMsg)
{
    lock();
    msgs.add_tail(pMsg);
    pthread_cond_t * w = waiter;
    WaitUtil::Semaphore * sem = waiter_sem;
    unlock();
    // a given queue will have a non-null waiter 
    // iff the receiver thread is blocked in the single-queue
    // _dequeue.  it will have a non-null sem
    // iff the receiver thread is blocked in the multi-queue
    // _dequeue.
    if (w)
        pthread_cond_signal(w);
    if (sem)
        sem->give();
}

void 
_thread_slinger_queue :: _enqueue_head(thread_slinger_message * pMsg)
{
    lock();
    msgs.add_head(pMsg);
    pthread_cond_t * w = waiter;
    WaitUtil::Semaphore * sem = waiter_sem;
    unlock();
    // a given queue will have a non-null waiter 
    // iff the receiver thread is blocked in the single-queue
    // _dequeue.  it will have a non-null sem
    // iff the receiver thread is blocked in the multi-queue
    // _dequeue.
    if (w)
        pthread_cond_signal(w);
    if (sem)
        sem->give();
}

//static
thread_slinger_message *
_thread_slinger_queue :: _dequeue(_thread_slinger_queue ** queues,
                                  int num_queues, int uSecs,
                                  int *which_queue)
{
    thread_slinger_message * pMsg = NULL;
    WaitUtil::Semaphore * sem = &queues[0]->_waiter_sem;
    int ind;
    struct timespec abstime;
    struct timespec * pTime = setup_abstime(uSecs, &abstime);
    // add sem to all queues
    for (ind = 0; ind < num_queues; ind++)
        queues[ind]->waiter_sem = sem;
    sem->init(0);
    // wait for msg
    do {
        for (ind = 0; ind < num_queues; ind++)
        {
            queues[ind]->lock();
            pMsg = queues[ind]->__dequeue();
            queues[ind]->unlock();
            if (pMsg)
            {
                if (which_queue)
                    *which_queue = ind;
                break;
            }
        }
        if (pMsg == NULL)
            if (sem->take(pTime) == false)
                break;
    } while (pMsg == NULL);
    // remove sem from all queues
    for (ind = 0; ind < num_queues; ind++)
        queues[ind]->waiter_sem = NULL;
    return pMsg;
}

// NOTE thread_slinger_pools must NOT have a constructor, and it
//      MUST rely on compile-time init of .data segment and/or
//      exec-time zeroing of .bss segment; this is because there may
//      be pools declared as global variables which must be able to
//      register during the ".init" section of the executable (i.e.
//      even before "main" has started or before all the .init sections
//      of all the .o's have run).


poolList_t *thread_slinger_pools::lst = NULL;

//static
void
thread_slinger_pools::register_pool(thread_slinger_pool_base * p)
{
    // if there's a race in multiple threads trying to register pools,
    // we need to make sure only the first one does 'new poolList_t'.
    static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mut);
    if (thread_slinger_pools::lst == NULL)
        thread_slinger_pools::lst = new poolList_t;
    pthread_mutex_unlock(&mut);

    WaitUtil::Lock  lock(thread_slinger_pools::lst);
    thread_slinger_pools::lst->add_tail(p);
}

//static
void
thread_slinger_pools::unregister_pool(thread_slinger_pool_base * p)
{
    WaitUtil::Lock  lock(thread_slinger_pools::lst);
    thread_slinger_pools::lst->remove(p);
}

//static
void
thread_slinger_pools::report_pools(poolReportList_t &report)
{
    thread_slinger_pool_base * p;
    report.clear();
    WaitUtil::Lock  lock(thread_slinger_pools::lst);
    for (p = thread_slinger_pools::lst->get_head();
         p != NULL;
         p = thread_slinger_pools::lst->get_next(p))
    {
        poolReport  r;
        p->getCounts(r.usedCount,
                     r.freeCount,
                     r.name);
        report.push_back(r);
    }
}

}; // namespace ThreadSlinger

std::ostream &operator<<(std::ostream &strm,
                         const ThreadSlinger::poolReportList_t &prs)
{
    for (auto item : prs)
        strm << item;
    return strm;
}

std::ostream &operator<<(std::ostream &strm,
                         const ThreadSlinger::poolReport &pr)
{
    strm << "pool " << pr.name << ": "
         << "used " << pr.usedCount << ", "
         << "free " << pr.freeCount << "\n";
    return strm;
}
