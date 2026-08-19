#if 0
set -e -x
python3 pll_coeff.py > Params.h
cat Params.h
g++ -O3 -Wall -I lib -o dpll_sim \
    lib/signal_backtrace.cc \
    lib/thread_slinger.cc \
    dpll_sim.cc
sudo ./dpll_sim
rm -f dpll_sim
exit 0
#endif

#if 0
# set terminal qt noraise

WAYLAND_DISPLAY= gnuplot
set terminal x11 noraise
set terminal wxt noraise

bind "q" "stop=1"
plot 'plot.dat' using 7 title 'proportional adjustment', 'plot.dat' using 5 title 'accumulated error' with lines
repeat_plot = "stop = 0; while (!stop) { pause 0.2 ; replot }"
eval repeat_plot


reset_and_plot = "stats 'plot.dat' using 9 nooutput ; start_line = int(STATS_records - 1000) ; plot 'plot.dat' every ::start_line using 7"
repeat_plot = "stop = 0; while (!stop) { pause 0.2 ; eval reset_and_plot }"
eval repeat_plot

#endif

// silence compiler.
#define SILENCE(x)   if (x < 0) { /* nothing */ }

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <vector>
#include <math.h>
#include <syscall.h>

#include "posix_fe.h"
#include "thread_slinger.h"
#include "stddev.h"

using namespace ThreadSlinger;

#define LOGFILE "plot.dat"

#include "Params.h"

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
    pxfe_timeval  stamp;
    void init(which_t _w) { which = _w; stamp.getNow(); }
    void cleanup(void) { }
};

thread_slinger_pool<mymsg, mymsg::which_t>   p;
thread_slinger_queue<mymsg>  q;

// note "setuid" is global for the whole process and all its threads
// (setxid triage!). this makes it more difficult for main to hold
// onto it while creating threads (so we can set realtime prios) and
// then give up before creating log files, so we use
// syscall(SYS_getuid) instead.
// each thread gives up priviledges in its own time.
static void give_up_privs(void)
{
    syscall(SYS_setuid, UNPRIV_UID);
    syscall(SYS_setgid, UNPRIV_GID);
}

void *ref_thread(void * arg)
{
    pxfe_timeval  ref_desired;
    pxfe_timeval  interval((double)INTERVAL);
    pxfe_timeval  now;

    give_up_privs();

    ref_desired.getNow();
    // ref intervals are aligned to 1s boundaries.
    ref_desired.tv_usec = 0;
    
    while (!done)
    {
        ref_desired += interval;
        now.getNow();
        pxfe_timeval s = ref_desired - now;
        (void) select(0, NULL, NULL, NULL, s());

#if JITTER > 0
        long r = random();
        usleep(r % JITTER); // introduce jitter
#endif
        mymsg * m = p.alloc(0, false, mymsg::REF);
        if (m)
            q.enqueue(m);
    }

    return NULL;
}

double osc_interval = INTERVAL;

void *osc_thread(void *arg)
{
    pxfe_timeval  osc_desired;
    pxfe_timeval  now;

    give_up_privs();

    osc_desired.getNow();

    while (!done)
    {
        if (osc_interval > MAX_INTERVAL)
            osc_interval = MAX_INTERVAL;
        else if (osc_interval < MIN_INTERVAL)
            osc_interval = MIN_INTERVAL;

        pxfe_timeval interval = osc_interval;
        osc_desired += interval;
        now.getNow();
        pxfe_timeval s = osc_desired - now;
        (void) select(0, NULL, NULL, NULL, s());
        mymsg * m = p.alloc(0, false, mymsg::OSC);
        if (m)
            q.enqueue(m);
    }

    return NULL;
}

void *dpll_thread(void *arg)
{
    enum { IDLE, UP, DOWN } state = IDLE;
    pxfe_timeval  start, last_ref, last_osc, d;
    FILE * f = NULL;

    // give up priviledges before opening data file.
    give_up_privs();

    f = fopen(LOGFILE, "w");

    // positive means osc is too slow, negative too fast.
    double accum_err = 0;
    double prop_adjust = 0;
    double phase_err;

    int lock_count = 0;
    int unlock_count = 0;
    int stage = 0;
#define SD_HISTORY_SIZE 100
    double sd_history[SD_HISTORY_SIZE];
    int sd_pos = 0, sd_got = 0;

    start.getNow();
    last_ref = last_osc = start;

    while (!done)
    {
        mymsg * m = q.dequeue(1000);
        if (m)
        {
            bool do_adj = false;
            const char * last_s = "";

            switch (m->which)
            {
            case mymsg::REF:
                last_ref = m->stamp;

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
                last_osc = m->stamp;

                if (state == UP)
                {
                    do_adj = true;
                    last_s = "  UP";
                    state = IDLE;
                }
                else
                    state = DOWN;

                break;

            case mymsg::NONE:
                // silence compiler warning.
                break;
            }

            if (do_adj)
            {
                d = last_ref - last_osc;
                phase_err = (double) (int64_t) d.usecs();
                phase_err /= 1e6;

                StageParams * sp = &stage_params[stage];

                accum_err  += phase_err * sp->k_i;
                prop_adjust = phase_err * sp->k_p;

                double adjust = prop_adjust + accum_err;
                osc_interval = INTERVAL + adjust;

                sd_history[sd_pos] = adjust;
                if (++sd_pos >= SD_HISTORY_SIZE)
                    sd_pos = 0;
                if (sd_got < SD_HISTORY_SIZE)
                    sd_got ++;

                double sd = calc_stddev(sd_history, sd_got);

                const char * sd_color = "";
                const char * ae_color = "";
                const char * norm = "";
                static const char * color_red   = "[31;1m";
                static const char * color_green = "[32;1m";
                static const char * color_norm  = "[m";


                int good_count = 0;

                if (fabs(accum_err) < sp->accum_error_thresh)
                {
                    ae_color = color_green;
                    good_count ++;
                }
                else
                    ae_color = color_red;

                if (sd < sp->lock_thresh)
                {
                    sd_color = color_green;
                    good_count ++;
                }
                else
                    sd_color = color_red;

                if (good_count == 2)
                {
                    unlock_count = 0;
                    if (lock_count >= sp->lock_thresh_count)
                    {
                        if (stage < (NUM_STAGES-1))
                        {
                            stage ++;
                            lock_count = 0;
                        }
                    }
                    else
                        lock_count ++;

                }
                else
                {
                    lock_count = 0;
                    unlock_count ++;
                    if (unlock_count >= sp->unlock_thresh_count)
                    {
                        if (stage > 0)
                        {
                            stage --;
                            unlock_count = 0;
                        }
                    }
                }



#define PRINTARGS                                       \
                    "%s "                               \
                    "pe %9.6f "                         \
                    "ae %s%13.10f%s "                   \
                    "ad %13.10f "                       \
                    "sd %s%13.10f%s "                   \
                    "lc %03d uc %03d S%d\n",            \
                    last_s,                             \
                    phase_err,                          \
                    ae_color, accum_err, norm,          \
                    adjust,                             \
                    sd_color, sd, norm,                 \
                    lock_count, unlock_count, stage

                norm = color_norm;
                printf(PRINTARGS);

                sd_color = ae_color = norm = "";
                fprintf(f, PRINTARGS);
                fflush(f);
            }

            p.release(m);
        }
    }
    return NULL;
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

    threads.emplace_back(&ref_thread);
    threads.emplace_back(&osc_thread);
    threads.emplace_back(&dpll_thread);

    p.add(100);

    if (getuid() == 0)
    {
        // as root, we're allowed to do these things.
        // as nonroot, we are not.
        attr.setinheritsched(false);
        attr.setfifoprio(1);
//      attr.setrrprio(1);
    }
    else
    {
        printf("INFO: not setting thread prios, no root access\n");
    }

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

    char c;
    SILENCE(read(0, &c, 1));
    done = true;

    for (auto &ti : threads)
        pthread_join(ti.id, NULL);

    return 0;
}
