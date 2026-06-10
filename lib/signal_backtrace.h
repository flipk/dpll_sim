/* -*- Mode:c++; eval:(c-set-style "BSD"); c-basic-offset:4; indent-tabs-mode:nil; tab-width:8 -*- */

#ifndef __SIGNAL_BACKTRACE_H__
#define __SIGNAL_BACKTRACE_H__

#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>
#include <string>

// NOTE NOTE NOTE
//
// proper use of the backtrace functionality requires use
// of the "-rdynamic" flag to gcc.
//
// NOTE NOTE NOTE

#ifdef __arm__
// work around arm's backy craptraces
#define STACK_SYMBOL_SEARCH 1
#endif

/** handy utilities for throwing exceptions and getting
 * function backtraces of them */
namespace BackTraceUtil {

struct SignalBacktraceInfo
{
public:
    SignalBacktraceInfo(void);
    ~SignalBacktraceInfo(void);
    void clear(void);
    void init(bool fatal, int sig = 0,
              const siginfo_t *new_si = NULL,
              const ucontext_t *new_uc = NULL);
    void desc_print(const char *format...);
    void do_backtrace( const char *process_name,
                       const char *reason = NULL );
    bool fatal;

    struct timeval tv;
    struct tm tm;
    char timebuf[64];

#define SignalBacktrace_MAX_DESCRIPTION 8192
    char description[SignalBacktrace_MAX_DESCRIPTION];
    int desc_len;

    int sig; // 0 means no sig (backtrace_now was called)
    siginfo_t siginfo_data;
    const ucontext_t *uc;

#define SignalBacktrace_MAX_TRACE_ENTRIES 40
    int trace_size;
    void *trace[SignalBacktrace_MAX_TRACE_ENTRIES];
    char **messages;

#if STACK_SYMBOL_SEARCH
    int trace_size2;
    void *trace2[SignalBacktrace_MAX_TRACE_ENTRIES];
    char **messages2;
#endif
};

// recommend checking info->sig for
// SIGTERM, SIGHUP, SIGINT, or SIGQUIT (info->fatal == false)
// and handling them specially, for instance SIGTERM doesn't
// have to exit immediately, it could trigger a safe shutdown sequence
// in the application.
typedef void (*SignalBacktraceHandler)(
    void * arg,
    const struct SignalBacktraceInfo *info);

class SignalBacktrace
{
    char process_name[64];
    SignalBacktraceHandler handler;
    void * handler_arg;
    SignalBacktraceInfo  info;
    static void default_user_handler(void * arg,
                                     const struct SignalBacktraceInfo *info);
    static void signal_handler(int sig, siginfo_t *info, void *uc);
    static SignalBacktrace * instance;
    SignalBacktrace(void);
    ~SignalBacktrace(void);
public:
    static SignalBacktrace * get_instance(void);
    static void cleanup(void);
    void register_handler(const char *process_name,
                          void * arg,
                          SignalBacktraceHandler new_handler);
    static void backtrace_now(const char *reason);
#if STACK_SYMBOL_SEARCH
    // if using STACK_SYMBOL_SEARCH, call this periodically
    // to zero out a reasonable amount of memory below the current
    // stack frame to make the eventual backtrace cleaner and have
    // less "strays".
    static void clean_stack(void);
#else
    static void clean_stack(void) { }
#endif
};

/** base class for errors you can throw. constructor
 * takes a snapshot of the stack and the Format method
 * prints out the symbol names. derive all your throwable
 * errors from this base class. */
struct BackTrace {
    SignalBacktraceInfo  info;
    /** constructor takes a stack snapshot */
    BackTrace(void)
    {
        info.init(false);
        info.do_backtrace("BT");
    }
    /** look up the symbols associated with the stack trace
     * and return a multi-line string listing them.
     * \return multi-line string containing the stack backtrace. */
    const std::string Format(void) const
    {
        std::string ret = _Format();
        ret += info.description;
        return ret;
    }
protected:
    virtual const std::string _Format(void) const
    {
        // a derived class may override this if it wishes
        // to provide more information appended to the backtrace.
        return "";
    }
};

}; // namespace BackTraceUtil

#endif /* __SIGNAL_BACKTRACE_H__ */
