#if 0
set -e -x
g++ -O3 -I lib -o cs \
    lib/signal_backtrace.cc \
    lib/thread_slinger.cc \
    clock_source.cc
sudo ./cs
exit 0
#endif

#if 0
plot "plot.dat" using 1:2 with lines
bind "q" "stop=1"

stop=0
while (!stop) {
    pause 0.1
    replot
}
#endif


#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <vector>
#include "posix_fe.h"
#include "thread_slinger.h"

using namespace ThreadSlinger;

#define LOGFILE "plot.dat"

#define INTERVAL 0.050000
//#define INTERVAL 0.1
//#define INTERVAL 1.0
#define JITTER 0
//#define JITTER 1
//#define JITTER 1000
//#define JITTER 10000

#define MIN_INTERVAL (INTERVAL - (INTERVAL * 0.1))
#define MAX_INTERVAL (INTERVAL + (INTERVAL * 0.1))

bool done = false;
pxfe_pthread_cond  clock_source_cond;

struct mymsg : public thread_slinger_message
{
    typedef enum { NONE, REF, OSC } which_t;
    which_t which;
    void init(which_t w) { which = w; }
    void cleanup(void) { }
};

thread_slinger_pool<mymsg, mymsg::which_t>   p;
thread_slinger_queue<mymsg>  q;


void *clock_source_thread(void * arg)
{
    pxfe_timeval  interval((double)INTERVAL);
    pxfe_timeval  desired;
    pxfe_timeval  now;

    desired.getNow();
    desired.tv_usec = 0;
    
    while (!done)
    {
        desired += interval;
        now.getNow();
        pxfe_timeval s = desired - now;
        (void) select(0, NULL, NULL, NULL, s());
        clock_source_cond.signal();
    }
    return NULL;
}

void *clock_delay_thread(void *arg)
{
    pxfe_pthread_mutex  mutex;
    mutex.init();
    clock_source_cond.init();

    while (1)
    {
        clock_source_cond.wait(mutex());
#if JITTER > 0
        long r = random();
        usleep(r % JITTER); // introduce jitter
#endif
        mymsg * m = p.alloc(0, false, mymsg::REF);
        if (m)
            q.enqueue(m);
    }
}

double osc_interval = INTERVAL;

void *osc_thread(void *arg)
{
//    usleep(random() % 500000);

    while (1)
    {
        if (osc_interval > MAX_INTERVAL)
            osc_interval = MAX_INTERVAL;
        else if (osc_interval < MIN_INTERVAL)
            osc_interval = MIN_INTERVAL;

        pxfe_timeval s = osc_interval;
        (void) select(0, NULL, NULL, NULL, s());
        mymsg * m = p.alloc(0, false, mymsg::OSC);
        if (m)
            q.enqueue(m);
    }

    return 0;
}

void *dpll_thread(void *arg)
{
    enum { UP, DOWN, TRI } state = TRI;
    const char * state_names[3] = { "  UP", "DOWN", " TRI" };
    pxfe_timeval  start, last_ref, last_osc, now, d;
    FILE * f = fopen(LOGFILE, "w");

    // positive means osc is too slow, negative too fast.
    double accum_err = 0;
    double ki = 0.01;
    double kp = 0.1;
    double prop_adjust = 0;
    double phase_err;

    start.getNow();
    now = last_ref = last_osc = start;

    while (1)
    {
        mymsg * m = q.dequeue(-1);
        if (m)
        {
            now.getNow();

            switch (state)
            {
            case UP:
            {
                // make OSC faster
                d = now - last_osc;
                phase_err = d.to_double();
                accum_err += phase_err * ki;
                prop_adjust = phase_err * kp;
                osc_interval -= accum_err + prop_adjust;
                break;
            }

            case DOWN:
            {
                // make OSC slower
                d = now - last_ref;
                phase_err = d.to_double();
                accum_err -= phase_err * ki;
                prop_adjust = -1 * phase_err * kp;
                osc_interval += accum_err + prop_adjust;
                break;
            }

            case TRI:
                prop_adjust = 0;
                break;
            }

            pxfe_timeval relnow = now - start;

            if (f)
            {
                fprintf(f,
                        "%s "
                        "%6u.%06lu "
                        "%7f "
                        "%6f"
                        "\n",
                        state_names[state],
                        relnow.tv_sec, relnow.tv_usec,
                        phase_err,
                        osc_interval
                    );
                fflush(f);
            }

            printf("%s "
                   "pe %7f "
                   "int %6f "
                   "accum %f "
                   "adjust %f "
                   "\n",
                   state_names[state],
                   phase_err,
                   osc_interval,
                   accum_err,
                   prop_adjust
);

            switch (m->which)
            {
            case mymsg::REF:
                last_ref = now;
                if (state == DOWN)
                    state = TRI;
                else
                    state = UP;
                break;

            case mymsg::OSC:
                last_osc = now;
                if (state == UP)
                    state = TRI;
                else
                    state = DOWN;
                break;
            }

            p.release(m);
        }
    }
}

struct threadinfo
{
    typedef void * (*funcptr_t)(void *);

    pthread_t id;
    funcptr_t  funcptr;

    threadinfo(funcptr_t  f) { funcptr = f; }
};

#define DIM(a)  (sizeof(a) / sizeof(a[0]))

int main()
{
    pxfe_pthread_attr  attr;
    std::vector<threadinfo>  threads;

    threads.emplace_back(&clock_delay_thread);
    threads.emplace_back(&clock_source_thread);
    threads.emplace_back(&osc_thread);
    threads.emplace_back(&dpll_thread);

    p.add(100);

    attr.setinheritsched(false);
    attr.setfifoprio(1);
//    attr.setrrprio(1);

    for (auto &ti : threads)
    {
        int r = pthread_create(&ti.id, attr(), ti.funcptr, NULL);
        if (r != 0)
        {
            printf("pthread_create: %d: %s\n", r, strerror(r));
            return 1;
        }
        // a very short pause to ensure the thread has a chance to
        // start and initialize itself.
        usleep(100);
    }

    pthread_join(threads[1].id, NULL);
    return 0;
}
