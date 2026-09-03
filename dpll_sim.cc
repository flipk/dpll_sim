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
#include <cmath>
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
    pxfe_timespec  stamp;
    void init(which_t _w) { which = _w; }
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
    syscall(SYS_setgid, UNPRIV_GID);
    syscall(SYS_setuid, UNPRIV_UID);
}

bool haveOppositeSigns(double a, double b) {
    return std::signbit(a) != std::signbit(b);
}

void *ref_thread(void * arg)
{
    pxfe_timespec  ref_desired;
    pxfe_timespec  interval((double)INTERVAL);
    pxfe_timespec  now;
    pxfe_timespec  early_alarm;
    pxfe_timespec  jitter;

    give_up_privs();

    early_alarm.set(0, 10000);
    ref_desired.getNow(CLOCK_MONOTONIC);
    // ref intervals are aligned to 1s boundaries.
    ref_desired.tv_nsec = 0;
    
    while (!done)
    {
        // alloc first, so the variability of the malloc
        // is already accounted for.
        mymsg * m = p.alloc(0, false, mymsg::REF);
        if (!m)
            // shouldn't happen, but bad
            break;

        jitter.set(0, random() % JITTER);

        ref_desired += interval;

        pxfe_timespec early_target = ref_desired - early_alarm;

        now.getNow(CLOCK_MONOTONIC);
        pxfe_timespec s = early_target - now;

        // go to sleep, but wake up early, in case traffic is bad.
        clock_nanosleep(CLOCK_MONOTONIC, 0, s(), NULL);

        pxfe_timespec ref_desired_plus_jitter = ref_desired + jitter;

        // now that we're in the lobby way early, surf the web and
        // kill time until the exact time of the appointment.
        do {
            now.getNow(CLOCK_MONOTONIC);
        } while (now < ref_desired_plus_jitter);

        m->stamp = now; // note this is the time before the jitter.
        q.enqueue(m);
    }

    return NULL;
}

double osc_interval = INTERVAL;

void *osc_thread(void *arg)
{
    pxfe_timespec  osc_desired;
    pxfe_timespec  now;
    pxfe_timespec  early_alarm;

    give_up_privs();

    early_alarm.set(0, 10000);
    osc_desired.getNow(CLOCK_MONOTONIC);

    while (!done)
    {
        // alloc first, so the variability of the malloc
        // is already accounted for.
        mymsg * m = p.alloc(0, false, mymsg::OSC);
        if (!m)
            // shouldn't happen, but bad
            break;

        if (osc_interval > MAX_INTERVAL)
            osc_interval = MAX_INTERVAL;
        else if (osc_interval < MIN_INTERVAL)
            osc_interval = MIN_INTERVAL;

        pxfe_timespec interval = osc_interval;
        osc_desired += interval;

        pxfe_timespec early_target = osc_desired - early_alarm;

        now.getNow(CLOCK_MONOTONIC);
        pxfe_timespec s = early_target - now;

        // go to sleep, but wake up early, in case traffic is bad.
        clock_nanosleep(CLOCK_MONOTONIC, 0, s(), NULL);

        // now that we're in the lobby way early, surf the web and
        // kill time until the exact time of the appointment.
        do {
            now.getNow(CLOCK_MONOTONIC);
        } while (now < osc_desired);

        m->stamp = now;
        q.enqueue(m);
    }

    return NULL;
}

void *dpll_thread(void *arg)
{
    enum { IDLE, UP, DOWN } state = IDLE;
    pxfe_timespec  start, last_ref, last_osc, d;
    FILE * f = NULL;

    // give up priviledges before opening data file.
    give_up_privs();

    f = fopen(LOGFILE, "w");

    // positive means osc is too slow, negative too fast.
    double accum_err = 0, prev_accum_err = 0;
    double prop_adjust = 0;
    double phase_err;

    int lock_count = 0;
    int unlock_count = 0;
    int stage = 0;

    // linux simulation (particularly on WSL) introduces random
    // wacky out of bound readings in phase error. reject those
    // that are 3-sigma out of bounds.
    stats_history<double, 100>  phase_err_history;
    stats_history<double, 100>  adjust_history;

    start.getNow(CLOCK_MONOTONIC);
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
                phase_err = (double) (int64_t) d.nsecs();
                phase_err /= 1e9;

                if (phase_err_history.count() >= 30)
                {
                    double pe_sd = phase_err_history.stddev();
                    double pe_av = phase_err_history.average();

                    // Reject deviations > 3 standard deviations
                    if (fabs(phase_err - pe_av) > (3.0 * pe_sd))
                    {
                        //printf("OUTLIER DETECTED, SKIPPED\n");
                        goto next_iter;
                    }
                }

                phase_err_history.add(phase_err);

                StageParams * sp = &stage_params[stage];

                accum_err  += phase_err * sp->k_i;
                prop_adjust = phase_err * sp->k_p;

                bool ae_sign_changed =
                    haveOppositeSigns(accum_err,
                                      prev_accum_err);

                prev_accum_err = accum_err;

                double adjust = prop_adjust + accum_err;
                osc_interval = INTERVAL + adjust;

                adjust_history.add(adjust);
                double ad_sd = adjust_history.stddev();
                double ad_av = adjust_history.average();

                const char * sd_color = "";
                const char * ae_color = "";
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

                if (ad_sd < sp->lock_thresh)
                {
                    sd_color = color_green;
                    good_count ++;
                }
                else
                    sd_color = color_red;

                if (good_count == 2)
                {
                    unlock_count = 0;
                    if (ae_sign_changed  &&
                        lock_count >= sp->lock_thresh_count)
                    {
                        if (stage < (NUM_STAGES-1))
                        {
                            stage ++;
                        }
                        lock_count = 0;
                    }
                    else
                    {
                        if (stage < (NUM_STAGES-1))
                        {
                            lock_count ++;
                            if (lock_count >= sp->lock_fail_count)
                            {
                                if (stage > 0)
                                {
                                    stage --;
                                    lock_count = 0;
                                }
                            }
                        }
                    }
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

#define PRINTARGS_CONSOLE                                       \
                "%s "                                           \
                    "ie:%9.3f "                                 \
                    "pe:%8.1f "                                 \
                    "ae:%s%15.9f%s "                            \
                    "pa:%12.6f "                                \
                    "ad:%11.6f "                                \
                    "sd:%s%10.6f%s "                            \
                    "av:%12.6f "                                \
                    "lc:%04d uc:%03d S%d (us.ns)"               \
                    "\n",                                       \
                    last_s,                                     \
                    (osc_interval - INTERVAL) * 1e6,            \
                    phase_err * 1e6,                            \
                    ae_color, accum_err * 1e6, color_norm,      \
                    prop_adjust * 1e6,                          \
                    adjust * 1e6,                               \
                    sd_color, ad_sd * 1e6, color_norm,          \
                    ad_av * 1e6,                                \
                    lock_count, unlock_count, stage

#define PRINTARGS_LOGFILE                               \
                "%s "             /*0*/                 \
                    " %13e "      /*1*/                 \
                    " %13e "      /*2*/                 \
                    " %13e "      /*3*/                 \
                    " %13e "      /*4*/                 \
                    " %13e "      /*5*/                 \
                    " %13e "      /*6*/                 \
                    " %13e "      /*7*/                 \
                    " %d %d S%d " /*8 9 10*/            \
                    "\n",                               \
                    last_s,         /*0*/               \
                    phase_err,      /*1*/               \
                    accum_err,      /*2*/               \
                    prop_adjust,    /*3*/               \
                    adjust,         /*4*/               \
                    ad_sd,          /*5*/               \
                    ad_av,          /*6*/               \
                    (osc_interval - INTERVAL),  /*7*/   \
                    lock_count,     /*8*/               \
                    unlock_count,   /*9*/               \
                    stage           /*10*/

                printf(PRINTARGS_CONSOLE);
                fprintf(f, PRINTARGS_LOGFILE);
                fflush(f);
            }

        next_iter:
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
