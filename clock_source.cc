#if 0
set -e -x
python3 pll_coeff.py > K_params.h
cat K_params.h
g++ -O3 -I lib -o cs \
    lib/signal_backtrace.cc \
    lib/thread_slinger.cc \
    clock_source.cc
sudo ./cs
exit 0
#endif

#if 0
# set terminal qt noraise

WAYLAND_DISPLAY= gnuplot
set terminal wxt noraise

bind "q" "stop=1"
plot 'plot.dat' using 7 title 'proportional adjustment', 'plot.dat' using 5 title 'accumulated error'
repeat_plot = "stop = 0; while (!stop) { pause 0.2 ; replot }"
eval repeat_plot


reset_and_plot = "stats 'plot.dat' using 9 nooutput ; start_line = int(STATS_records - 1000) ; plot 'plot.dat' every ::start_line using 7"
repeat_plot = "stop = 0; while (!stop) { pause 0.2 ; eval reset_and_plot }"
eval repeat_plot

#endif

// CONSIDER : should messages contain the timestamp, so dpll thread
// doesn't have to measure it? that might be even more accurate.


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

#include "K_params.h"

//#define JITTER 0
#define JITTER 5000

// standard deviation of jitter, given that JITTER is use
// as a "random() % JITTER",
// is = sqrt(J^2 / 12)
//   or = J/(2*sqrt(3))
//   or ~= J * 0.289.

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
    // ref intervals are aligned to 1s boundaries.
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
    pxfe_timeval  desired;
    pxfe_timeval  now;

    desired.getNow();

    while (1)
    {
        if (osc_interval > MAX_INTERVAL)
            osc_interval = MAX_INTERVAL;
        else if (osc_interval < MIN_INTERVAL)
            osc_interval = MIN_INTERVAL;

        pxfe_timeval interval = osc_interval;
        desired += interval;
        now.getNow();
        pxfe_timeval s = desired - now;
        (void) select(0, NULL, NULL, NULL, s());
        mymsg * m = p.alloc(0, false, mymsg::OSC);
        if (m)
            q.enqueue(m);
    }

    return 0;
}

void *dpll_thread(void *arg)
{
    enum { IDLE, UP, DOWN } state = IDLE;
    const char * state_names[3] = { "IDLE", "  UP", "DOWN" };
    pxfe_timeval  start, last_ref, last_osc, now, d;
    FILE * f = fopen(LOGFILE, "w");

    // positive means osc is too slow, negative too fast.
    double accum_err = 0;
    double prop_adjust = 0;
    double phase_err;

    start.getNow();
    now = last_ref = last_osc = start;

    while (1)
    {
        mymsg * m = q.dequeue(-1);
        if (m)
        {
            bool do_adj = false;
            const char * last_s = "";

            now.getNow();
            switch (m->which)
            {
            case mymsg::REF:
                last_ref = now;

                if (state == DOWN)
                {
                    do_adj = true;
                    last_s = "DOWN";
                    state = IDLE;
                }
                else
                    state = UP;

                break;

            case mymsg::OSC:
                last_osc = now;

                if (state == UP)
                {
                    do_adj = true;
                    last_s = "  UP";
                    state = IDLE;
                }
                else
                    state = DOWN;

                break;
            }

            if (do_adj)
            {
                d = last_ref - last_osc;
                phase_err = (double) (int64_t) d.usecs();
                phase_err /= 1e6;
                accum_err += phase_err * K_I;
                prop_adjust = phase_err * K_P;
                osc_interval = INTERVAL + prop_adjust + accum_err;

                printf("%s "
                       "pe %12.9f "
                       "ac %12.9f "
                       "pa %12.9f "
                       "int %9.6f "
                       "\n",
                       last_s,
                       phase_err,
                       accum_err,
                       prop_adjust,
                       osc_interval);

                fprintf(f,
                       "%s "
                       "pe %12.9f "
                       "ac %12.9f "
                       "pa %12.9f "
                       "int %9.6f "
                       "\n",
                       last_s,
                       phase_err,
                       accum_err,
                       prop_adjust,
                       osc_interval);
                fflush(f);
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
