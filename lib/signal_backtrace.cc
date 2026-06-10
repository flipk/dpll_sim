
#define __STDC_FORMAT_MACROS 1

#include "signal_backtrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __CYGWIN__
#include <execinfo.h>
#endif
#include <stdarg.h>
#include <pthread.h>
#include <inttypes.h>
#include <cxxabi.h>

extern char _start;
extern char _etext;
extern char _fini;

namespace BackTraceUtil {

SignalBacktrace* SignalBacktrace::instance = NULL;

SignalBacktrace :: SignalBacktrace(void)
{
    struct sigaction sa;

    sa.sa_handler = SIG_IGN;
    sigfillset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, NULL);

    sa.sa_sigaction = &signal_handler;
    sa.sa_flags = SA_SIGINFO;
    if (getenv("ALLOW_CORE_DUMPS") == NULL)
    {
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGILL,  &sa, NULL);
        sigaction(SIGBUS,  &sa, NULL);
        sigaction(SIGFPE,  &sa, NULL);
        sigaction(SIGTRAP, &sa, NULL);
        sigaction(SIGABRT, &sa, NULL);
    }

    // restartable and not fatal.
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    handler = &default_user_handler;
    handler_arg = NULL;
    instance = this;
}

SignalBacktrace :: ~SignalBacktrace(void)
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask); // makes valgrind happy
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    instance = NULL;
}

SignalBacktrace *
SignalBacktrace :: get_instance(void)
{
    if (instance == NULL)
        instance = new SignalBacktrace;
    return instance;
}

void
SignalBacktrace :: cleanup(void)
{
    if (instance != NULL)
        delete instance;
    instance = NULL;
}

SignalBacktraceInfo :: SignalBacktraceInfo(void)
{
    // clear will try to free these if they
    // are not null.
    messages = NULL;
#if STACK_SYMBOL_SEARCH
    messages2 = NULL;
#endif
    clear();
}

SignalBacktraceInfo :: ~SignalBacktraceInfo(void)
{
    if (messages)
        free(messages);
#if STACK_SYMBOL_SEARCH
    if (messages2)
        free(messages2);
#endif
}

void
SignalBacktraceInfo :: clear(void)
{
    fatal = true;
    description[0] = 0;
    desc_len = 0;
    sig = 0;
    memset(&siginfo_data, 0, sizeof(siginfo_data));
    uc = NULL;
    trace_size = 0;
    memset(trace,0,sizeof(trace));
    if (messages)
        free(messages);
    messages = NULL;
    memset(&tv, 0, sizeof(tv));
    memset(&tm, 0, sizeof(tm));
    memset(timebuf, 0, sizeof(timebuf));
#if STACK_SYMBOL_SEARCH
    trace_size2 = 0;
    memset(trace2,0,sizeof(trace2));
    if (messages2)
        free(messages2);
    messages2 = NULL;
#endif
}

void
SignalBacktraceInfo :: init(bool new_fatal, int new_sig /*= 0*/,
                            const siginfo_t *new_si /*= NULL*/,
                            const ucontext_t *new_uc /*= NULL*/)
{
    fatal = new_fatal;
    sig = new_sig;
    if (new_si)
        siginfo_data = *new_si;
    else
        memset(&siginfo_data,0,sizeof(siginfo_data));
    uc = new_uc;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    memset(timebuf,0,sizeof(timebuf));
    strftime(timebuf,sizeof(timebuf)-1, "%Y/%m/%d-%H:%M:%S", &tm);
}

void
SignalBacktraceInfo :: desc_print(const char *format...)
{
    va_list ap;
    va_start(ap, format);
    if (desc_len < (SignalBacktrace_MAX_DESCRIPTION - 1)) {
        int cc = vsnprintf(description + desc_len,
                           SignalBacktrace_MAX_DESCRIPTION - desc_len - 1,
                           format, ap);
        desc_len += cc;
        if (desc_len > (SignalBacktrace_MAX_DESCRIPTION - 1))
            desc_len = (SignalBacktrace_MAX_DESCRIPTION - 1);
        va_end(ap);
        description[desc_len] = 0; // always null terminated
    }
}

void
SignalBacktrace :: register_handler( const char *new_process_name,
                                     void * arg,
                                     SignalBacktraceHandler new_handler )
{
    if (new_process_name != NULL)
    {
        strncpy(process_name, new_process_name, sizeof(process_name)-1);
        process_name[sizeof(process_name)-1] = 0;
    }
    else
        process_name[0] = 0;
    if (new_handler != NULL)
    {
        handler = new_handler;
        handler_arg = arg;
    }
    else
    {
        handler = &default_user_handler;
        handler_arg = NULL;
    }
}

//static
void
SignalBacktrace :: default_user_handler(
    void *arg,
    const struct SignalBacktraceInfo *info )
{
    ssize_t s = write(2, info->description, info->desc_len);
    if (s) { } // remove unused warning
    if (info->fatal)
        exit(1);
}

//static
void
SignalBacktrace :: signal_handler(int sig, siginfo_t *info, void *uc)
{
    if (instance == NULL)
    {
        fprintf(stderr,
                "[bt] %u unhandled signal %d with no SignalBacktrace!\n",
                getpid(), sig);
        exit(1);
    }
    instance->info.clear();
    bool fatal = true;
    switch (sig)
    {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        fatal = false;
        break;
    default:
        ;
    }
    instance->info.init(fatal, sig, info, (ucontext_t *) uc);
    instance->info.do_backtrace(instance->process_name);
    instance->handler(instance->handler_arg, &instance->info);
}

//static
void
SignalBacktrace :: backtrace_now( const char *reason )
{
    SignalBacktraceInfo  info2;
    info2.init(/*fatal*/false);
    const char * pn = "";
    if (instance != NULL)
        pn = instance->process_name;
    info2.do_backtrace(pn, reason);
    if (instance != NULL)
        instance->handler(instance->handler_arg, &info2);
}

#if STACK_SYMBOL_SEARCH
// static
void
SignalBacktrace :: clean_stack(void)
{
    char buf[30000];
    memset(buf, 0, sizeof(buf));
}
#endif

static void demangle(char *input,
                     char *output, int out_len)
{
    int left_paren_pos = -1;
    int right_paren_pos = -1;
    int plus_pos = -1;
    int status = 0;
    size_t rn_size = 0;
    char * pos;
    const char * hex_part;

    // format of the output of "backtrace_symbols":
    // /path/to/file(function_name+hex_offset) [0xADDRESS]
    // tho sometimes the function name part is zero-length.

    pos = strchr(input, '(');
    if (pos == NULL)
        goto bad;
    left_paren_pos = pos - input;

    pos = strrchr(input, ')');
    if (pos == NULL)
        goto bad;
    right_paren_pos = pos - input;

    pos = strrchr(input + left_paren_pos, '+');
    if (pos == NULL)
    {
        plus_pos = -1;
        hex_part = "0";
    }
    else
    {
        plus_pos = pos - input;
        hex_part = input + plus_pos + 1;
        // if function_name is missing, just copy the
        // whole string as-is.
        if ((left_paren_pos + 1) == plus_pos)
            goto bad;
    }

    char realname[500];
    rn_size = sizeof(realname);
    if (plus_pos > 0)
        input[plus_pos] = 0;
    input[right_paren_pos] = 0;

    abi::__cxa_demangle(input + left_paren_pos + 1,
                        realname, &rn_size, &status);
    if (status < 0)
        snprintf(output, out_len, "%s + %s",
                 input + left_paren_pos + 1,
                 hex_part);
    else
        snprintf(output, out_len, "%s + %s",
                 realname,
                 hex_part);
    output[out_len-1] = 0;

    if (plus_pos > 0)
        input[plus_pos] = '+';
    input[right_paren_pos] = ')';

    return;
bad:
    strncpy(output, input, out_len-1);
    output[out_len-1] = 0;
}

void
SignalBacktraceInfo :: do_backtrace( const char *process_name,
                                     const char *reason /* = NULL */ )
{
    pid_t pid = getpid();

#ifdef __CYGWIN__

    trace_size = 0;
    desc_print("[bt] backtrace not present because CYGWIN sucks\n");

#else

    int i;

    // first we'll try a good old honest backtrace try
    // and see what we get.
    trace_size = backtrace( trace,
                            SignalBacktrace_MAX_TRACE_ENTRIES );
    messages = backtrace_symbols( trace, trace_size );

    desc_print("[bt] %u %s ---BACKTRACE AT %s.%06d",
               pid, process_name, timebuf, tv.tv_usec);

    if (reason != NULL)
        desc_print(": %s\n", reason);
    else
        desc_print("\n");

#endif /* CYGWIN */

    // indicates whether to print si_addr but ALSO
    // indicates that a register dump is useful.
    bool si_addr_valid = false;

    if (sig != 0)
    {
        desc_print("[bt] %u %s ---got signal %d ",
                        pid, process_name, sig);

        switch (sig)
        {
        case SIGHUP:
            desc_print("SIGHUP");
            break;
        case SIGINT:
            desc_print("SIGINT -- did you hit ^C?");
            break;
        case SIGQUIT:
            desc_print("SIGQUIT -- did you hit ^\\?");
            break;
        case SIGTERM:
            desc_print("SIGTERM");
            break;

        case SIGABRT:
            desc_print("SIGABRT");
            break;

        case SIGILL:
            desc_print("SIGILL");
            si_addr_valid = true;
            break;
        case SIGFPE:
            desc_print("SIGFPE");
            si_addr_valid = true;
            break;
        case SIGTRAP:
            desc_print("SIGTRAP");
            si_addr_valid = true;
            break;
        case SIGSEGV:
            desc_print("SIGSEGV");
            si_addr_valid = true;
            break;
        case SIGBUS:
            desc_print("SIGBUS");
            si_addr_valid = true;
            break;
        }

        if (si_addr_valid)
            desc_print(" at si_addr = 0x%" PRIxPTR,
                   (uintptr_t) siginfo_data.si_addr);

        desc_print("\n");
    }

#ifndef __CYGWIN__

# if __WORDSIZE == 64
#define __PRIPTR_LEADING_ZEROS "016"
#else
#define __PRIPTR_LEADING_ZEROS "08"
#endif
    char func_and_offset[1024];

    desc_print("[bt] %u %s ---Execution path according to backtrace:\n",
               pid, process_name);
    for ( i = 0; i < trace_size; i++ )
    {
        demangle(messages[i], func_and_offset, sizeof(func_and_offset));
        desc_print( "[bt] %u [0x%" __PRIPTR_LEADING_ZEROS
                    PRIxPTR "] %s\n", pid, trace[i], func_and_offset );
    }
    desc_print( "[bt] %u ---end of backtrace\n", pid);

#if STACK_SYMBOL_SEARCH
    do { // extra sauce to deal with arm's backy craptraces
        trace_size2 = 0; // start over
        pthread_attr_t   attr;
        if (pthread_getattr_np(pthread_self(), &attr) != 0)
            // give up
            break;
        void *stackaddr;
        size_t stacksize;
        if (pthread_attr_getstack(&attr, &stackaddr, &stacksize) != 0)
            // give up
            break;
        uintptr_t  text_lo = (uintptr_t) &_start;
        uintptr_t  text_hi = (uintptr_t) &_fini;
        uintptr_t  stack_lo = (uintptr_t) stackaddr;
        uintptr_t  stack_hi = stack_lo + stacksize;
        uint32_t * ptr = (uint32_t *) &i;

        for (int ind = 0;
             trace_size2 < SignalBacktrace_MAX_TRACE_ENTRIES;
             ind++)
        {
            uintptr_t ptr_to_v = (uintptr_t) &ptr[ind];
            if (ptr_to_v >= stack_hi)
                break;
            uintptr_t v = (uintptr_t) ptr[ind];
            if (v > text_lo && v < text_hi)
                trace2[trace_size2++] = (void*) v;
        }
        // skip cuz we're about to die and i don't care
        // and i ain't taking any risks: pthread_attr_destroy(&attr);
        messages2 = backtrace_symbols( trace2, trace_size2 );
        desc_print( "[bt] %s %u ---all text values on stack "
                         "(NOTE SOME MAY BE STRAYS):\n", process_name, pid);
        for ( i = 0; i < trace_size2; i++ )
        {
            demangle(messages2[i],
                     func_and_offset, sizeof(func_and_offset));
            desc_print( "[bt] %u [0x%" __PRIPTR_LEADING_ZEROS
                        PRIxPTR "] %s\n", pid, trace2[i], func_and_offset);
        }
        desc_print( "[bt] %u ---end of all text values\n", pid);
    } while(0);
#endif

    if (si_addr_valid && uc != NULL)
    {
#if __powerpc__
        // register dump
        const mcontext_t *regs = uc->uc_mcontext.uc_regs;
        desc_print("[bt] %u %s REGISTER DUMP:\n", pid, process_name);
        for (i=0; i < 32; i++)
        {
            desc_print("  r%-2d %08x  ", i, regs->gregs[i]);
            if ((i & 3) == 3)
                desc_print("\n");
        }
        desc_print("  nip %08x  ", regs->gregs[32]);
        desc_print("  msr %08x  ", regs->gregs[33]);
        desc_print("   r3 %08x  ", regs->gregs[34]);
        desc_print("  ctr %08x\n", regs->gregs[35]);
        desc_print("  lnk %08x  ", regs->gregs[36]);
        desc_print("  xer %08x  ", regs->gregs[37]);
        desc_print("  ccr %08x  ", regs->gregs[38]);
        desc_print("   mq %08x\n", regs->gregs[39]);
        desc_print("  trp %08x  ", regs->gregs[40]);
        desc_print("  dar %08x  ", regs->gregs[41]);
        desc_print("  dir %08x  ", regs->gregs[42]);
        desc_print("  res %08x\n", regs->gregs[43]);
        desc_print( "[bt] %u ---end of register dump\n", pid);
#endif
#if __arm__
        const mcontext_t *regs = &uc->uc_mcontext;
        desc_print("[bt] %u %s REGISTER DUMP:\n", pid, process_name);
        desc_print("  r0  %08x  ", regs->arm_r0);
        desc_print("  r1  %08x  ", regs->arm_r1);
        desc_print("  r2  %08x  ", regs->arm_r2);
        desc_print("  r3  %08x\n", regs->arm_r3);
        desc_print("  r4  %08x  ", regs->arm_r4);
        desc_print("  r5  %08x  ", regs->arm_r5);
        desc_print("  r6  %08x  ", regs->arm_r6);
        desc_print("  r7  %08x\n", regs->arm_r7);
        desc_print("  r8  %08x  ", regs->arm_r8);
        desc_print("  r9  %08x  ", regs->arm_r9);
        desc_print("  r10 %08x  ", regs->arm_r10);
        desc_print("  fp  %08x\n", regs->arm_fp);
        desc_print("  ip  %08x  ", regs->arm_ip);
        desc_print("  sp  %08x  ", regs->arm_sp);
        desc_print("  lr  %08x  ", regs->arm_lr);
        desc_print(" cpsr %08x\n", regs->arm_cpsr);
// in si_addr already // desc_print("  FA  %08x\n", regs->fault_address);
        desc_print( "[bt] %u ---end of register dump\n", pid);
#endif
#ifdef __x86_64__
        const mcontext_t *regs = &uc->uc_mcontext;
        desc_print("[bt] %u %s REGISTER DUMP:\n", pid, process_name);
        desc_print("  r8 %016" PRIx64 "  ", regs->gregs[REG_R8     ]);
        desc_print("  r9 %016" PRIx64 "  ", regs->gregs[REG_R9     ]);
        desc_print(" r10 %016" PRIx64 "\n", regs->gregs[REG_R10    ]);
        desc_print(" r11 %016" PRIx64 "  ", regs->gregs[REG_R11    ]);
        desc_print(" r12 %016" PRIx64 "  ", regs->gregs[REG_R12    ]);
        desc_print(" r13 %016" PRIx64 "\n", regs->gregs[REG_R13    ]);
        desc_print(" r14 %016" PRIx64 "  ", regs->gregs[REG_R14    ]);
        desc_print(" r15 %016" PRIx64 "  ", regs->gregs[REG_R15    ]);
        desc_print(" rdi %016" PRIx64 "\n", regs->gregs[REG_RDI    ]);
        desc_print(" rsi %016" PRIx64 "  ", regs->gregs[REG_RSI    ]);
        desc_print(" rbp %016" PRIx64 "  ", regs->gregs[REG_RBP    ]);
        desc_print(" rbx %016" PRIx64 "\n", regs->gregs[REG_RBX    ]);
        desc_print(" rdx %016" PRIx64 "  ", regs->gregs[REG_RDX    ]);
        desc_print(" rax %016" PRIx64 "  ", regs->gregs[REG_RAX    ]);
        desc_print(" rcx %016" PRIx64 "\n", regs->gregs[REG_RCX    ]);
        desc_print(" rsp %016" PRIx64 "  ", regs->gregs[REG_RSP    ]);
        desc_print(" rip %016" PRIx64 "  ", regs->gregs[REG_RIP    ]);
        desc_print(" efl %016" PRIx64 "\n", regs->gregs[REG_EFL    ]);
        desc_print("cgfs %016" PRIx64 "  ", regs->gregs[REG_CSGSFS ]);
        desc_print(" err %016" PRIx64 "  ", regs->gregs[REG_ERR    ]);
        desc_print("trap %016" PRIx64 "\n", regs->gregs[REG_TRAPNO ]);
        desc_print("oldm %016" PRIx64 "  ", regs->gregs[REG_OLDMASK]);
        desc_print(" cr2 %016" PRIx64 "\n", regs->gregs[REG_CR2    ]);
        desc_print( "[bt] %u ---end of register dump\n", pid);
#endif
#ifdef __i386__
        desc_print(" NOTE __i386__ linux mcontext_t register dump "
                        "not implemented\n");
#endif
    }
#endif /* CYGWIN */
}

}; // namespace BackTraceUtil
