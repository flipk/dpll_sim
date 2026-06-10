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

#ifndef __posix_fe_h__
#define __posix_fe_h__

// use inttypes.h instead of stdint.h.  inttypes.h includes
// stdint.h for you, and then gives you more!

// on many OS's, inttypes.h does not define PRIu64 or
// any of the other PRI* macros unless you define this:
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif

// on many OS's, stdint.h won't give you INT32_MAX or
// any of the other *INT*MAX macros unless you define this:
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

// on many OS's, stdint.h won't give you UINT8_C(x) or
// any of the other *INT*C(x) macros unless you define this:
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS 1
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdlib.h>
#include <poll.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <math.h>

/** wrapper for struct timeval */
struct pxfe_timeval : public timeval
{
    /** default constructor initializes sec=0, usec=0 */
    pxfe_timeval(void) { tv_sec = 0; tv_usec = 0; }
    /** constructor which accepts sec and usec args */
    pxfe_timeval(time_t s, long u) { set(s,u); }
    pxfe_timeval(double s) { set(s); }
    /** copy constructor  */
    pxfe_timeval(const pxfe_timeval &other) {
        tv_sec = other.tv_sec;  tv_usec = other.tv_usec;
    }
    /** set method which accepts sec and usec args */
    void set(time_t s, long u) { tv_sec = s; tv_usec = u; }
    void set(double s) {
        double integ;
        double frac = modf(s, &integ);
        tv_sec = (time_t) integ;
        tv_usec = (suseconds_t) (frac * 1e6 + 0.5);
    }
    /** assignment operator from another timeval */
    const pxfe_timeval& operator=(const timeval &rhs) {
        tv_sec = rhs.tv_sec;
        tv_usec = rhs.tv_usec;
        return *this;
    }
    /** assignment operator from struct timespec, converts nsec to usec */
    const pxfe_timeval& operator=(const timespec &rhs) {
        tv_sec = rhs.tv_sec;
        tv_usec = rhs.tv_nsec / 1000;
        return *this;
    }
    /** subtract in place operator */
    const pxfe_timeval& operator-=(const timeval &rhs) {
        bool borrow = false;
        if (rhs.tv_usec > tv_usec)
            borrow = true;
        tv_sec -= rhs.tv_sec;
        tv_usec -= rhs.tv_usec;
        if (borrow)
        {
            tv_sec -= 1;
            tv_usec += 1000000;
        }
        return *this;
    }
    /** add in place operator */
    const pxfe_timeval& operator+=(const timeval &rhs) {
        tv_sec += rhs.tv_sec;
        tv_usec += rhs.tv_usec;
        if (tv_usec > 1000000)
        {
            tv_usec -= 1000000;
            tv_sec += 1;
        }
        return *this;
    }
    /** comparison operator checks both sec and usec */
    const bool operator==(const timeval &other) {
        if (tv_sec != other.tv_sec)
            return false;
        if (tv_usec != other.tv_usec)
            return false;
        return true;
    }
    /** comparison operator */
    const bool operator!=(const timeval &other) {
        return !operator==(other);
    }
    /** gettimeofday initializes this object to "now" and returns it */
    const pxfe_timeval &getNow(void) {
        gettimeofday(this, NULL);
        return *this;
    }
    /** turns this time into a string, plus optional format, usecs included,
     * see strftime for format definition */
    const std::string Format(const char *format = NULL) {
        if (format == NULL)
            format = "%Y-%m-%d %H:%M:%S";
        time_t seconds = tv_sec;
        tm t;
        localtime_r(&seconds, &t);
        char ymdhms[128], ms[12];
        strftime(ymdhms,sizeof(ymdhms),format,&t);
        ymdhms[sizeof(ymdhms)-1] = 0;
        snprintf(ms,sizeof(ms),"%06ld", tv_usec);
        ms[sizeof(ms)-1] = 0;
        return std::string(ymdhms) + "." + ms;
    }
    /** if this timeval is a duration, turn it into h,m,s,usec
     * which you can easily format using %02u:%02u:%02u.%06u */
    void breakdown(uint32_t &hours, uint32_t &minutes,
                   uint32_t &seconds, uint32_t &usecs) {
        seconds = tv_sec;
        hours  = seconds / 3600;
        seconds -= hours * 3600;
        minutes  = seconds / 60;
        seconds -= minutes * 60;
        usecs = tv_usec;
    }
    double to_double(void) {
        return tv_sec + (tv_usec / 1000000.0);
    }
    /** convert sec/usec into single milliseconds value */
    uint32_t msecs(void) {
        return (tv_sec * 1000) + (tv_usec / 1000);
    }
    /** convert sec/usec into single microseconds value */
    uint64_t usecs(void) {
        return ((uint64_t)tv_sec * 1000000) + tv_usec;
    }
    /** convert sec/usec into single nanoseconds value */
    uint64_t nsecs(void) {
        return ((uint64_t)tv_sec * 1000000000) + (tv_usec * 1000);
    }
    /** return this object as a struct timeval pointer */
    timeval *operator()(void) { return this; }
};
/** output a pxfe_timeval as "pxfe_timeval(sec,usec)" to an ostream */
static inline std::ostream& operator<<(std::ostream& ostr,
                                       const pxfe_timeval &rhs)
{
    ostr << "pxfe_timeval(" << rhs.tv_sec << "," << rhs.tv_usec << ")";
    return ostr;
}
/** subtract two pxfe_timeval */
static inline pxfe_timeval operator-(const pxfe_timeval &lhs,
                                    const pxfe_timeval &rhs) {
    return pxfe_timeval(lhs).operator-=(rhs);
}
/** add two pxfe_timeval */
static inline pxfe_timeval operator+(const pxfe_timeval &lhs,
                                    const pxfe_timeval &rhs) {
    return pxfe_timeval(lhs).operator+=(rhs);
}
/** compare two pxfe_timeval */
static inline bool operator>(const pxfe_timeval &lhs, const pxfe_timeval &other) {
   if (lhs.tv_sec > other.tv_sec) 
      return true;
   if (lhs.tv_sec < other.tv_sec) 
      return false;
   return lhs.tv_usec > other.tv_usec;
}
/** compare two pxfe_timeval */
static inline bool operator<(const pxfe_timeval &lhs, const pxfe_timeval &other) {
   if (lhs.tv_sec < other.tv_sec) 
      return true;
   if (lhs.tv_sec > other.tv_sec) 
      return false;
   return lhs.tv_usec < other.tv_usec;
}

/** wrapper for struct timespec */
struct pxfe_timespec : public timespec
{
    /** default constructor initializes sec=0, nsec=0 */
    pxfe_timespec(void) { tv_sec = 0; tv_nsec = 0; }
    /** constructor which accepts sec and nsec args */
    pxfe_timespec(time_t s, long n) { set(s,n); }
    /** copy constructor  */
    pxfe_timespec(const pxfe_timespec &other) {
        tv_sec = other.tv_sec; tv_nsec = other.tv_nsec;
    }
    /** set method which accepts sec and nsec args */
    void set(time_t s, long n) { tv_sec = s; tv_nsec = n; }
    /** assignment operator from another timespec */
    const pxfe_timespec &operator=(const timespec &rhs) {
        tv_sec = rhs.tv_sec;
        tv_nsec = rhs.tv_nsec;
        return *this;
    }
    /** assignment operator from struct timeval, converts usec to nsec */
    const pxfe_timespec &operator=(const timeval &rhs) {
        tv_sec = rhs.tv_sec;
        tv_nsec = rhs.tv_usec * 1000;
        return *this;
    }
    /** subtract in place operator */
    const pxfe_timespec &operator-=(const timespec &rhs) {
        bool borrow = false;
        if (rhs.tv_nsec > tv_nsec)
            borrow = true;
        tv_sec -= rhs.tv_sec;
        tv_nsec -= rhs.tv_nsec;
        if (borrow)
        {
            tv_sec -= 1;
            tv_nsec += 1000000000;
        }
        return *this;
    }
    /** add in place operator */
    const pxfe_timespec &operator+=(const timespec &rhs) {
        tv_sec += rhs.tv_sec;
        tv_nsec += rhs.tv_nsec;
        if (tv_nsec > 1000000000)
        {
            tv_nsec -= 1000000000;
            tv_sec += 1;
        }
        return *this;
    }
    /** comparison operator checks both sec and nsec */
    const bool operator==(const timespec &rhs) {
        return (tv_sec == rhs.tv_sec) && (tv_nsec == rhs.tv_nsec);
    }
    /** comparison operator */
    const bool operator!=(const timespec &rhs) {
        return !operator==(rhs);
    }
    /** clock_gettime initializes this object to "now" and returns it */
    const pxfe_timespec &getNow(clockid_t clk_id = CLOCK_REALTIME) {
        clock_gettime(clk_id, this);
        return *this;
    }
    /** shortcut for clock_gettime(CLOCK_MONOTONIC) */
    const pxfe_timespec &getMonotonic(void) {
        return getNow(CLOCK_MONOTONIC);
    }
    /** turns this time into a string, plus optional format, nsecs included,
     * see strftime for format definition */
    const std::string Format(const char *format = NULL) {
        if (format == NULL)
            format = "%Y-%m-%d %H:%M:%S";
        time_t seconds = tv_sec;
        tm t;
        localtime_r(&seconds, &t);
        char ymdhms[128], ns[12];
        strftime(ymdhms,sizeof(ymdhms),format,&t);
        ymdhms[sizeof(ymdhms)-1] = 0;
        snprintf(ns,sizeof(ns),"%09ld", tv_nsec);
        ns[sizeof(ns)-1] = 0;
        return std::string(ymdhms) + "." + ns;
    }
    /** if this timeval is a duration, turn it into h,m,s,nsec
     * which you can easily format using %02u:%02u:%02u.%09u */
    void breakdown(uint32_t &hours, uint32_t &minutes,
                   uint32_t &seconds, uint32_t &nsecs) {
        seconds = tv_sec;
        hours  = seconds / 3600;
        seconds -= hours * 3600;
        minutes  = seconds / 60;
        seconds -= minutes * 60;
        nsecs = tv_nsec;
    }
    /** convert sec/usec into single milliseconds value */
    uint32_t msecs(void) {
        return (tv_sec * 1000) + (tv_nsec / 1000000);
    }
    /** convert sec/usec into single microseconds value */
    uint64_t usecs(void) {
        return ((uint64_t)tv_sec * 1000000) + (tv_nsec / 1000);
    }
    /** convert sec/usec into single nanoseconds value */
    uint64_t nsecs(void) {
        return ((uint64_t)tv_sec * 1000000000) + tv_nsec;
    }
    /** return this object as a struct timespec pointer */
    timespec *operator()(void) { return this; }
};
static inline std::ostream& operator<<(std::ostream& ostr,
                                       const pxfe_timespec &rhs)
{
    ostr << "pxfe_timespec(" << rhs.tv_sec << "," << rhs.tv_nsec << ")";
    return ostr;
}
static inline pxfe_timespec operator-(const pxfe_timespec &lhs,
                                     const pxfe_timespec &rhs) {
   return pxfe_timespec(lhs).operator-=(rhs);
}
static inline pxfe_timespec operator+(const pxfe_timespec &lhs,
                                     const pxfe_timespec &rhs) {
   return pxfe_timespec(lhs).operator+=(rhs);
}
static inline bool operator>(const pxfe_timespec &lhs,
                             const pxfe_timespec &other) {
   if (lhs.tv_sec > other.tv_sec) 
      return true;
   if (lhs.tv_sec < other.tv_sec) 
      return false;
   return lhs.tv_nsec > other.tv_nsec;
}
static inline bool operator<(const pxfe_timespec &lhs,
                             const pxfe_timespec &other) {
   if (lhs.tv_sec < other.tv_sec) 
      return true;
   if (lhs.tv_sec > other.tv_sec) 
      return false;
   return lhs.tv_nsec < other.tv_nsec;
}


/** base type for use with pxfe_shared_ptr */
class pxfe_shared_ptr_base {
    template <typename T> friend class pxfe_shared_ptr;
    std::atomic_int __pxfe_sp_refcount;
public:
    pxfe_shared_ptr_base(void)
        : __pxfe_sp_refcount(0) { /*nothing*/ }
    virtual ~pxfe_shared_ptr_base(void) { /* nothing*/ }
    /** returns the current usage counter;
     * note this is only advisory because if there's more
     * than one thread holding this object, this value can
     * change. you can only really trust it if it's 1. */
    int use_count(void) const {
        return std::atomic_load(&__pxfe_sp_refcount);
    }
};

/** a shared pointer object, like std::shared_ptr but different.
 * 1. the counter is in the object itself (must be derived from
 *    pxfe_shared_ptr_base class) and so does not require a separate
 *    administrative data structure like std::shared_ptr does.
 * 2. has "give" and "take" methods so you can put an object into a
 *    shared_ptr and take an object out without destroying it.
 * 3. has casting methods so it's easy to manage heirarchicaly-derived
 *    classes, just copy-construct or assign, and if it comes out
 *    NULL, then it wasn't a polymorphic base type (dynamic_cast
 *    failed). */
template <class T>
class pxfe_shared_ptr {
    static_assert(std::is_base_of<pxfe_shared_ptr_base, T>::value == true,
                  "T must be derived from pxfe_shared_ptr_base");
    T * ptr;
    void ref(void)
    {
        if (ptr)
            ptr->__pxfe_sp_refcount ++;
    }
    void deref(void)
    {
        if (ptr && ptr->__pxfe_sp_refcount-- <= 1)
        {
            delete ptr;
            ptr = NULL;
        }
    }
public:
    /** normal constructor, adds a ref to the object */
    pxfe_shared_ptr(T * _ptr = NULL)
    {
        ptr = _ptr;
        ref();
    }
    /** copy constructor */
    /* constexpr <-- gcc error?
       "constexpr constructor does not have empty body" */
    pxfe_shared_ptr(const pxfe_shared_ptr<T>& other)
    {
        ptr = other.ptr;
        ref();
    }
    /** casting constructor, if dynamic_cast to the new type
     * succeeds, takes a ref, otherwise sets to empty/NULL */
    template <class BaseT>
    pxfe_shared_ptr(const pxfe_shared_ptr<BaseT> &other)
    {
        ptr = dynamic_cast<T*>(*other);
        ref();
    }
    /** move constructor, transfers ownership */
    pxfe_shared_ptr(pxfe_shared_ptr<T> &&other)
    {
        ptr = other.ptr;
        other.ptr = NULL;
    }
    /** destructor which derefs the object (deleting it if ref==0) */
    ~pxfe_shared_ptr(void)
    {
        deref();
    }
    /** point this object to something else,
     *  deref the old and ref the new */
    void reset(T * _ptr = NULL)
    {
        deref();
        ptr = _ptr;
        ref();
    }
    /** give an object to this class for safe keeping; assumes
     * the reference count is already set properly and does not
     * change it (but does deref anything this class previously held) */
    void _give(T * _ptr)
    {
        deref();
        ptr = _ptr;
        // the caller is passing ownership to us,
        // presumably they had a refcount,
        // which they are giving to us,
        // so don't modify the refcount here.
    }
    /** takes an object away from this class (does not deref it) */
    T * _take(void)
    {
        T * ret = ptr;
        // we are letting the caller take ownership from us,
        // so the refcount we have is being given to them,
        // so don't modify the refcount here.
        ptr = NULL;
        return ret;
    }

    /** explicit assignment operator */
    constexpr pxfe_shared_ptr<T>& operator=(const pxfe_shared_ptr<T>& other)
    {
        deref();
        ptr = other.ptr;
        ref();
        return *this;
    }
    /** casting assignment operator, attempts dynamic_cast. if
     * casting fails, this object is now empty (NULL) */
    template <class BaseT>
    pxfe_shared_ptr<T> &operator=(const pxfe_shared_ptr<BaseT> &other)
    {
        deref();
        ptr = dynamic_cast<T*>(*other);
        ref();
        return *this;
    }
    /** move assignment operator, takes over ownership */
    pxfe_shared_ptr<T> &operator=(pxfe_shared_ptr<T> &&other)
    {
        deref();
        ptr = other.ptr;
        other.ptr = NULL;
        return *this;
    }
    /** if this is the only shared ptr referencing this object */
    bool unique(void) const {
        if (ptr)
            return (ptr->use_count() == 1);
        return false;
    }
    /** return if this is managing something (true) or empty (false).
     * useful for null checking just like a regular ptr, using the
     * syntax "if (sp)" */
    operator bool() const { return (ptr != NULL); }
    /** accessor that returns the pointer within */
    T * operator->(void) const { return ptr; }
    /** accessor that returns the pointer within */
    T * operator*(void) const { return ptr; }
    /** accessor that returns the pointer within */
    T * get(void) const { return ptr; }
};

/** container for an 'errno' */
class pxfe_errno {
    char error_string_storage[ 120 ];
public:
    /** the actual errno */
    int e;
    /** when init is used, this will point to a strerror */
    char * err;
    /** a name that will be printed "what: strerror" */
    const char * what;
    /** default constructor will init errno=0 and what="" */
    pxfe_errno(int _e = 0, const char *_what = NULL) { init(_e, _what); }
    /** set errno and "what", will also set err = strerror */
    void init(int _e, const char *_what = NULL) {
        e = _e;
        what = _what;
        if (e <= 0)
            err = NULL;
        else
        {

// NOTE : the strerror_r pooch got boned in all its holes
//        by the vastly different XSI vs GNU definitions.

#if (_POSIX_C_SOURCE >= 200112L) && !  _GNU_SOURCE

// we have to use the XSI-compliant version of strerror_r
// from POSIX.1-2001 (available since glibc 2.3.4,
// but not POSIX-compliant until glibc 2.13):
//    int strerror_r(int errnum, char *buf, size_t buflen)

// sigh, prior to glibc 2.13, strerror_r returned a (positive)
// error number, or 0 if success, but 2.13 and later, returns -1
// for failure and sets errno. poor, poor pooch.

            error_string_storage[0] = 0;
            if (strerror_r(e, error_string_storage,
                           sizeof(error_string_storage)) == 0)
            {
                err = error_string_storage;
            }
            else
            {
                // i don't care about the errno describing why we
                // can't describe the errno, so i'm not going to
                // bother with the glibc version# nonsense.  we'll
                // just print the (original) errno and skip the
                // textual description.
                err = NULL;
            }
#else

// we have to use the GNU-compliant version of strerror_r
//    char *strerror_r(int errnum, char *buf, size_t buflen)

            err = strerror_r(e, error_string_storage,
                             sizeof(error_string_storage));
            error_string_storage[sizeof(error_string_storage)-1] = 0;

// well, that seemed easier.

#endif
        }
    }
    /** format this errno into "what: error <number> (strerror)" */
    std::string Format(void) {
        std::ostringstream  ostr;
        if (what)
            ostr << what << ": ";
        ostr << "error " << e;
        if (err)
            ostr << " (" << err << ")";
        return ostr.str();
    }
};

/** a wrapper around std::string to make useful as data buffer */
class pxfe_string : public std::string {
public:
    /** default construct inits string to empty */
    pxfe_string(void) { }
    /** construct with a null terminated c-string */
    pxfe_string(const char *s) : std::string(s) { }
    /** construct with a pointer/length (can contain nuls) */
    pxfe_string(const char *s, size_t len) : std::string(s,len) { }
    /** construct with a string */
    pxfe_string(const std::string &other) : std::string(other) { }
    /** cast c_str to a void* ptr */
    void * vptr(void) {
        return (void*) c_str();
    }
    /** cast c_str to a const void* ptr */
    const void * vptr(void) const {
        return (const void*) c_str();
    }
    /** cast c_str to unsigned char* ptr */
    unsigned char * ucptr(void) {
        return (unsigned char *) c_str();
    }
    /** cast c_str to const unsigned char* ptr */
    const unsigned char * ucptr(void) const {
        return (const unsigned char *) c_str();
    }
    /** cast c_str to uint8_t* ptr */
    uint8_t * u8ptr(void) {
        return (uint8_t *) c_str();
    }
    /** cast c_str to const uint8_t* ptr */
    const uint8_t * u8ptr(void) const {
        return (const uint8_t *) c_str();
    }
    bool startswith(const char *prefix, size_t prefixlen = 0) {
        if (prefixlen == 0)
            prefixlen = strlen(prefix);
        if (size() < prefixlen)
            return false;
        if (compare(0, prefixlen, prefix) == 0)
            return true;
        return false;
    }
    bool startswith(const std::string &prefix) {
        return startswith(prefix.c_str(), prefix.size());
    }
    bool endswith(const char *suffix, size_t suffixlen = 0) {
        if (suffixlen == 0)
            suffixlen = strlen(suffix);
        if (size() < suffixlen)
            return false;
        if (compare(size()-suffixlen,
                    suffixlen,
                    suffix) == 0)
            return true;
        return false;
    }
    bool endswith(const std::string &suffix) {
        return endswith(suffix.c_str(), suffix.size());
    }
    /** make a human readable string containing a "%02x" format of 
     * this buffer */
    std::string format_hex(void) {
        std::ostringstream out;
        // at() returns a signed char, but the char and unsigned char
        // overloads for ostream operator<< try to output the char.
        // we want a binary to hex conversion that you get with the
        // "int" operator<< overload.  but we can't cast directly from
        // "char" to "int" because that will sign-extend, so cast to
        // unsigned char first.
        for (size_t i = 0; i < length(); i++)
            out << std::hex << std::setw(2) << std::setfill('0') <<
                (int)((unsigned char)at(i));
        return out.str();
    }
    /** attempt to read into this buffer from a descriptor, up to max
     * size, resize string to size actually read, return bytes read,
     * and fill errno if returns -1 */
    ssize_t read(int fd, size_t max, pxfe_errno *e = NULL) {
        resize(max);
        ssize_t cc = ::read(fd, vptr(), max);
        if (cc < 0)
            if (e) e->init(errno, "read");
        resize(cc > 0 ? cc : 0);
        return cc;
    }
    /** attempt to write this buffer to a file descriptor, and return
     * number of bytes written, or fill errno if returns -1 */
    ssize_t write(int fd, pxfe_errno *e = NULL) const {
        ssize_t cc = ::write(fd, vptr(), length());
        if (cc < 0)
            if (e) e->init(errno, "write");
        return cc;
    }
    /** assignment operator calls std::string.assign() */
    void operator=(const std::string &other) {
        assign(other.c_str(), other.length());
    }
};

/** wrapper around pthread_mutexattr_t */
class pxfe_pthread_mutexattr {
    pthread_mutexattr_t  attr;
public:
    /** constructor does mutexattr_init */
    pxfe_pthread_mutexattr(void) {
        pthread_mutexattr_init(&attr);
    }
    /** destructor does mutexattr_destroy */
    ~pxfe_pthread_mutexattr(void) {
        pthread_mutexattr_destroy(&attr);
    }
    /** return a "const pthread_mutexattr_t *" */
    const pthread_mutexattr_t *operator()(void) { return &attr; }
    /** set to PTHREAD_PROCESS_SHARED or PRIVATE
     * (init defaults to private) */
    void setpshared(bool shared = true) {
        pthread_mutexattr_setpshared(
            &attr,
            shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE);
    }
    /** set protocol to PTHREAD_PRIO_INHERIT, NONE, PROTECT
     * (default is NONE) */
    void setproto(int proto) {
        pthread_mutexattr_setprotocol(&attr, proto);
    }
    /** set type to PTHREAD_MUTEX_NORMAL, ERRORCHECK, RECURSIVE, DEFAULT */
    void settype(int type) {
        pthread_mutexattr_settype(&attr, type);
    }
#if HAVE_PTHREAD_MUTEXATTR_SETROBUST || HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP
    /** set to PTHREAD_MUTEX_ROBUST or PTHREAD_MUTEX_STALLED
     * (default is STALLED) */
    void setrobust(bool robust = true) {
#if HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP
        pthread_mutexattr_setrobust_np(
            &attr, robust ? PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED);
#elif HAVE_PTHREAD_MUTEXATTR_SETROBUST
        pthread_mutexattr_setrobust(
            &attr, robust ? PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED);
#endif
    }
#else
    /** set to PTHREAD_MUTEX_ROBUST or PTHREAD_MUTEX_STALLED
     * (default is STALLED) */
    void setrobust(bool robust = true) {
        std::cerr << " WARNING : ROBUST MUTEX NOT SUPPORTED" << std::endl;
    }
#endif
    // pthread_mutexattr_setprioceiling
};

/** wrapper for pthread_mutex_t */
class pxfe_pthread_mutex {
    pthread_mutex_t  mutex;
    bool initialized;
public:
    /** attributes; you will want to fill this out before calling init */
    pxfe_pthread_mutexattr attr;
    /** constructor does nothing; you need to call init when you're done
     * filling out attr member */
    pxfe_pthread_mutex(void) {
        initialized = false;
    }
    /** destructor calls pthread_mutex_destroy if it was initialized */
    ~pxfe_pthread_mutex(void) {
        if (initialized)
            pthread_mutex_destroy(&mutex);
    }
    /* call pthread_mutex_init with the included "attr" object */
    void init(void) {
        if (initialized)
            pthread_mutex_destroy(&mutex);
        pthread_mutex_init(&mutex, attr());
        initialized = true;
    }
    /** return a pthread_mutex_t * pointer */
    pthread_mutex_t *operator()(void) { return initialized ? &mutex : NULL; }
    /** lock the mutex or block waiting */
    void lock(void) {
        if (!initialized)
            std::cerr << "pxfe_pthread_mutex::lock: not initialized\n";
        else
            pthread_mutex_lock(&mutex);
    }
    /** try locking the mutex, return <0 if currently locked by someone else */
    int trylock(void) {
        if (!initialized) {
            std::cerr << "pxfe_pthread_mutex::trylock: not initialized\n";
            return EINVAL;
        }
        // else
        return pthread_mutex_trylock(&mutex);
    }
    /** unlock the mutex and unblock any potential lock waiters */
    void unlock(void) {
        if (!initialized)
            std::cerr << "pxfe_pthread_mutex::unlock: not initialized\n";
        else
            pthread_mutex_unlock(&mutex);
    }
};

/** handy wrapper for locking a mutex, most useful for automatically
 * locking and unlocking on construction and destruction */
class pxfe_pthread_mutex_lock {
    pxfe_pthread_mutex &mut;
    bool ilocked;
public:
    /** constructor will automatically lock the mutex on construction */
     pxfe_pthread_mutex_lock(pxfe_pthread_mutex &_mut, bool dolock=true)
        : mut(_mut), ilocked(dolock) {
        if (dolock)
            mut.lock();
    }
    /** destructor automatically unlocks if locked upon destruction */
    ~pxfe_pthread_mutex_lock(void) {
        if (ilocked) {
            mut.unlock();
        }
    }
    /** manually locked if not locked */
    bool lock(void) {
        if (ilocked)
            return false;
        mut.lock();
        ilocked = true;
        return true;
    }
    /** manually unlock if currently locked */
    bool unlock(void) {
        if (!ilocked)
            return false;
        mut.unlock();
        ilocked = false;
        return true;
    }
};

/** wrapper around pthread_condattr_t */
class pxfe_pthread_condattr {
    pthread_condattr_t attr;
public:
    /** constructor does condattr_init */
    pxfe_pthread_condattr(void) {
        pthread_condattr_init(&attr);
    }
    /** destructor does condattr_destroy */
    ~pxfe_pthread_condattr(void) {
        pthread_condattr_destroy(&attr);
    }
    /** return a const pthread_condattr_t *pointer */
    const pthread_condattr_t *operator()(void) { return &attr; }
    /** set clock type to CLOCK_REALTIME or MONOTONIC or BOOTTIME */
    void setclock(clockid_t id) {
    // but not: CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID
        pthread_condattr_setclock(&attr,id);
    }
    /** set clock to REALTIME */
    void setclock_rt(void) { setclock(CLOCK_REALTIME); }
    /** set clock to MONOTONIC */
    void setclock_monotonic(void) { setclock(CLOCK_MONOTONIC); }
    /** set clock to BOOTTIME */
    void setclock_boottime(void) { setclock(CLOCK_BOOTTIME); }
    /** set PTHREAD_PROCESS_SHARED or PRIVATE */
    void setpshared(bool shared = true) {
        pthread_condattr_setpshared(
            &attr, shared ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE);
    }
};

/** wrapper for pthread_cond_t */
class pxfe_pthread_cond {
    pthread_cond_t  cond;
    bool initialized;
public:
    /** attributes; you will want to fill this out before calling init */
    pxfe_pthread_condattr attr;
    /** constructor does nothing; you need to call init when you're done
     * filling out attr member */
    pxfe_pthread_cond(void) {
        initialized = false;
    }
    /** destructor calls cond_destroy if it was initialized */
    ~pxfe_pthread_cond(void) {
        if (initialized)
            pthread_cond_destroy(&cond);
    }
    /** call pthread_cond_init with the included "attr" object */
    void init(void) {
        if (initialized)
            pthread_cond_destroy(&cond);
        pthread_cond_init(&cond, attr());
        initialized = true;
    }
    /** return const pthread_cond_t * pointer */
    const pthread_cond_t *operator()(void) {
        return initialized ? &cond : NULL; }
    /** block waiting on this condition; if time is NULL, block forever,
     * otherwise block until timeout expires */
    int wait(pthread_mutex_t *mut, timespec *abstime = NULL) {
        if (!initialized) {
            std::cerr << "pxfe_pthread_cond::wait: not initialized\n";
            return EINVAL;
        } else if (abstime == NULL)
            return pthread_cond_wait(&cond, mut);
        return pthread_cond_timedwait(&cond, mut, abstime);
    }
    /** signal a single waiter to wakeup */
    void signal(void) {
        if (!initialized)
            std::cerr << "pxfe_pthread_cond::signal: not initialized\n";
        else
            pthread_cond_signal(&cond);
    }
    /** signal all waiters to wakeup */
    void bcast(void) {
        if (!initialized)
            std::cerr << "pxfe_pthread_cond::bcast: not initialized\n";
        else
            pthread_cond_broadcast(&cond);
    }
};

/** classical counting semaphore like P and V from school */

/*  FYI: V stands for 'Verhoog' and P stands for 'Prolaag', not 'Probeer'.
    Verhoog can be translated as 'increasing'.
    Decreasing would be 'Verlaag', but for better distinction between
    the letters, Edsger Wybe Dijkstra invented the word 'Prolaag'.
    "Probeer te verlagen" is "try to decrease".
    "Verhogen" is increase. Dijkstra's earliest paper
    on the subject gives passering ("passing") as the meaning for P,
    and vrijgave ("release") as the meaning for V.
    It also mentions that the terminology is taken from that used in
    railroad signals. Dijkstra subsequently wrote that
    he intended P to stand for prolaag, short for probeer te verlagen,
    literally "try to reduce", or to parallel the terms used in the other
    case, "try to decrease". Some texts call them vacate and procure to
    match the original Dutch initials.
*/

class pxfe_semaphore {
    pxfe_pthread_cond cond;
    pxfe_pthread_mutex mut;
    int value;
public:
    /** initialize semaphore with an initial value */
    pxfe_semaphore(int initial = 1) {
        value = initial;
        mut.init();
        cond.init();
    }
    /** cleanup */
    ~pxfe_semaphore(void) { /* what */ }
    /** "give" the semaphore (P) */
    void give(void) {
        pxfe_pthread_mutex_lock lock(mut);
        value++;
        lock.unlock();
        cond.signal();
    }
    /** take the semaphore (V) and wait forever if time is NULL */
    bool take(timespec *expire = NULL) {
        pxfe_pthread_mutex_lock lock(mut);
        while (value <= 0) {
            if (cond.wait(mut(), expire) < 0)
                return false;
        }
        value--;
        return true;
    }
    void P(void) { give(); }
    bool V(timespec *expire = NULL) { return take(expire); }
};

class pxfe_pthread_attr {
    pthread_attr_t _attr;
public:
    /** initializes attr with pthread defaults */
    pxfe_pthread_attr(void) {
        pthread_attr_init(&_attr);
    }
    /** cleanup */
    ~pxfe_pthread_attr(void) {
        pthread_attr_destroy(&_attr);
    }
    /** handy accessor returns the attr for passing to pthread_create */
    const pthread_attr_t *operator()(void) { return &_attr; }
    /** select DETACHED vs JOINABLE */
    void set_detach(bool set=true) {
        int state = set ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
        pthread_attr_setdetachstate(&_attr, state);
    }
#ifdef _GNU_SOURCE
    /** set CPU affinity */
    void setaffinity(size_t cpusetsize, const cpu_set_t *cpuset) {
        pthread_attr_setaffinity_np(&_attr, cpusetsize, cpuset);
    }
#endif
    /** set size of virtual space (rounded up to page size)
     * left unmapped between stacks as a guard between stacks */
    void setguardsize(size_t guardsize) {
        pthread_attr_setguardsize(&_attr, guardsize);
    }
    /** set whether new thread inherits scheduling info from parent
     * or takes it from this attr object */
    void setinheritsched(bool inherit=true) {
        int i = inherit ? PTHREAD_INHERIT_SCHED : PTHREAD_EXPLICIT_SCHED;
        pthread_attr_setinheritsched(&_attr, i);
    }
    /** set scheduler to FIFO with a given rt priority */
    void setfifoprio(int prio) {
        struct sched_param par;
        par.sched_priority = prio;
        pthread_attr_setschedpolicy(&_attr, SCHED_FIFO);
        pthread_attr_setschedparam(&_attr, &par);
    }
    /** set scheduler to RR with a given rt priority */
    void setrrprio(int prio) {
        struct sched_param par;
        par.sched_priority = prio;
        pthread_attr_setschedpolicy(&_attr, SCHED_RR);
        pthread_attr_setschedparam(&_attr, &par);
    }
    /** set scheduler to OTHER with given non-rt priority */
    void setotherprio(int prio) {
        struct sched_param par;
        par.sched_priority = prio;
        pthread_attr_setschedpolicy(&_attr, SCHED_OTHER);
        pthread_attr_setschedparam(&_attr, &par);
    }
    // chose not to implement
    //   pthread_attr_setstack
    //   pthread_attr_setstackaddr
    //   pthread_attr_setstacksize
    // pthread_attr_setscope -- SCOPE_PROCESS not supported on linux
};

/** front end to pipe(2) */
class pxfe_pipe {
public:
    /** the read-end of this pipe */
    int readEnd;
    /** the write-end of this pipe */
    int writeEnd;
    /** constructor calls pipe(2) and inits */
    pxfe_pipe(void)
    {
        int fds[2];
        if (pipe(fds) < 0)
            fprintf(stderr, "pxfe_pipe: pipe failed\n");
        readEnd = fds[0];
        writeEnd = fds[1];
    }
    /** destructor automatically closes both descriptors */
    ~pxfe_pipe(void)
    {
        close(writeEnd);
        close(readEnd);
    }
    /** read from the readEnd into a string, of a max size, resize
     * string to actual size read, and set errno and return false
     * if the read fails */
    bool read(std::string &buf, int max, pxfe_errno *e = NULL)
    {
        pxfe_string &_buf = (pxfe_string &)buf;
        buf.resize(max);
        int cc = ::read(readEnd, _buf.vptr(), _buf.length());
        if (cc < 0) {
            if (e) e->init(errno, "read");
            buf.resize(0);
            return false;
        }
        buf.resize(cc);
        return true;
    }
    /** write a string to the writeEnd, setting errno and return false
     *  if it fails */
    bool write(const std::string &buf, pxfe_errno *e = NULL)
    {
        pxfe_string &_buf = (pxfe_string &)buf;
        int cc = ::write(writeEnd, _buf.vptr(), _buf.length());
        if (cc < 0) {
            if (e) e->init(errno, "write");
            return false;
        }
        if (cc != (int)_buf.length()) {
            // i don't yet know if this is a case i have to handle.
            fprintf(stderr, "pxfe_pipe: short write %d != %d!\n",
                    cc, (int) _buf.length());
        }
        return true;
    }
    /** read from readEnd into a generic buffer, returning size read */
    int read(void *buf, size_t count, pxfe_errno *e = NULL)
    {
        int cc = ::read(readEnd, buf, count);
        if (cc < 0)
            if (e) e->init(errno, "read");
        return cc;
    }
    /** write to writeEnd into a generic buffer, returning size written */
    int write(void *buf, size_t count, pxfe_errno *e = NULL)
    {
        int cc = ::write(writeEnd, buf, count);
        if (cc < 0)
            if (e) e->init(errno, "write");
        return cc;
    }
};

/** base class for file descriptor, inherited by several other
 * descriptor wrapper classes */
class pxfe_fd {
protected:
    int fd;
public:
    /** max size of a message this API supports */
    static const std::string::size_type MAX_MSG_LEN = 16384;
    /** constructor initializes fd to -1 */
    pxfe_fd(void) {
        fd = -1;
    }
    /** destructor automatically closes fd if it is not -1 */
    ~pxfe_fd(void) {
        close();
    }
    /** return the descriptor */
    int  getFd(void) const { return fd; }
    /** set the descriptor, closing old fd it was set before */
    void setFd(int nfd) { close(); fd = nfd; }
    /** use fcntl to toggle O_NONBLOCK off */
    bool set_blocking(pxfe_errno *e = NULL) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
        {
            if (e) e->init(errno, "fcntl:F_GETFL");
            return false;
        }
        flags &= ~O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) == -1)
        {
            if (e) e->init(errno, "fcntl:F_SETFL");
            return false;
        }
        return true;
    }
    /** use fcntl to toggle O_NONBLOCK on */
    bool set_nonblocking(pxfe_errno *e = NULL) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
        {
            if (e) e->init(errno, "fcntl:F_GETFL");
            return false;
        }
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) == -1)
        {
            if (e) e->init(errno, "fcntl:F_SETFL");
            return false;
        }
        return true;
    }
    /** attempt to open a file path (using std::string) */
    bool open(const std::string &path, int flags,
              pxfe_errno *e = NULL,
              mode_t mode = 0600) {
        return open(path.c_str(), flags, e, mode);
    }
    /** attempt to open a file path (using char*) */
    bool open(const char *path, int flags,
              pxfe_errno *e = NULL,
              mode_t mode = 0600) {
        fd = ::open(path, flags | O_CLOEXEC, mode);
        if (fd < 0)
            if (e) e->init(errno, "open");
        return (fd >= 0);
    }
    /** close the descriptor if it is open, then set fd to -1 */
    void close(void) {
        if (fd >= 0)
            ::close(fd);
        fd = -1;
    }
    /** attempt read into generic buffer of size count, return size read */
    int read(void *buf, size_t count, pxfe_errno *e = NULL) {
        int cc = ::read (fd, buf, count);
        if (cc < 0 && e) e->init(errno, "read");
        return cc;
    }
    /** attempt write of generic buffer, return size written */
    int write(void *buf, size_t count, pxfe_errno *e = NULL) {
        int cc = ::write(fd, buf, count);
        if (cc < 0 && e) e->init(errno, "write");
        return cc;
    }
    /** attempt to read into string, of up to max size, resize string to
     * actual size read, set errno and return false if fail */
    bool read(std::string &buf, int max = 16384, pxfe_errno *e = NULL) {
        pxfe_string &_buf = (pxfe_string &)buf;
        _buf.resize(max);
        int cc = ::read(fd, _buf.vptr(), max);
        if (cc < 0) {
            if (e) e->init(errno, "read");
            _buf.resize(0);
            return false;
        }
        _buf.resize(cc);
        return true;
    }
    /** attempt write of string, set errno and return false if fail */
    bool write(const std::string &buf, pxfe_errno *e = NULL) {
        pxfe_string &_buf = (pxfe_string &)buf;
        int cc = ::write(fd, _buf.vptr(), _buf.length());
        if (cc < 0) {
            if (e) e->init(errno, "write");
            return false;
        }
        if (cc != (int)_buf.length())
        {
            // haven't figured out if i need to handle this yet.
            fprintf(stderr, " **** SHORT WRITE %d != %d\n",
                    cc, (int) _buf.length());
        }
        return true;
    }
};

// needs doxygen
class pxfe_pthread {
    pxfe_pthread_mutex mut;
    pxfe_pthread_cond cond;
    enum {
        INIT, NEWBORN, RUNNING, STOPPING, ZOMBIE
    } state;
    pthread_t  id;
    void *user_arg;
    static void * _entry(void *thread_obj) {
        pxfe_pthread * th = (pxfe_pthread *)thread_obj;
        pxfe_pthread_mutex_lock lock(th->mut);
        th->state = RUNNING;
        lock.unlock();
        th->cond.signal();
        void * ret = th->entry(th->user_arg);
        lock.lock();
        th->state = ZOMBIE;
        return ret;
    }
protected:
    virtual void * entry(void *arg) = 0;
    virtual void send_stop(void) = 0;
public:
    pxfe_pthread_attr attr;
    pxfe_pthread(void) {
        state = INIT;
        mut.init();
        cond.init();
    }
    ~pxfe_pthread(void) {
        // derived class destructor really should do stop/join
        // before destroying anything else in derived, but it doesn't
        // hurt to repeat it here since stop and join both check the state.
        stopjoin();
    }
    int create(void *_user_arg=NULL) {
        pxfe_pthread_mutex_lock lock(mut);
        if (state != INIT)
            return -1;
        state = NEWBORN;
        lock.unlock();
        attr.set_detach(false); // this class depends on joinable.
        user_arg = _user_arg;
        int ret = pthread_create(&id, attr(), &_entry, this);
        lock.lock();
        if (ret == 0) {
            while (state == NEWBORN) {
                pxfe_timespec ts(5,0);
                ts += pxfe_timespec().getNow();
                cond.wait(mut(), ts());
            }
        } else {
            state = INIT;
        }
        return ret;
    }
    void stop(void) {
        pxfe_pthread_mutex_lock lock(mut);
        if (state != RUNNING)
            return;
        state = STOPPING;
        send_stop();
    }
    void * join(void) {
        pxfe_pthread_mutex_lock lock(mut);
        if (state != STOPPING && state != ZOMBIE)
            return NULL;
        lock.unlock();
        void * ret = NULL;
        pthread_join(id, &ret);
        lock.lock();
        state = INIT;
        return ret;
    }
    void * stopjoin(void) { stop(); return join(); }
    const bool running(void) const { return (state != INIT); }
};

/** wrapper for opendir/readdir/closedir */
class pxfe_readdir {
    DIR * d;
public:
    /** constructor, does nothing */
    pxfe_readdir(void) {
        d = NULL;
    }
    /** destructor, closes DIR if currently open */
    ~pxfe_readdir(void) {
        close();
    }
    /** close DIR if currently open */
    void close(void) {
        if (d)
            ::closedir(d);
        d = NULL;
    }
    /** open a dir path, set errno and return false if failure */
    bool open(const std::string &dirstr, pxfe_errno *e = NULL) {
        return open(dirstr.c_str(), e);
    }
    /** open a dir path, set errno and return false if failure */
    bool open(const char *dirname, pxfe_errno *e = NULL) {
        close();
        d = ::opendir(dirname);
        if (d == NULL) {
            if (e) e->init(errno, "opendir");
            return false;
        }
        return true;
    }

/*
   I DO NOT USE "readdir_r" AND WILL NOT ENTERTAIN
   REQUESTS TO CHANGE IT.  Here's why.

   The reason you want _r implementations is to be thread-safe. But
   readdir() references a data structure stored in the DIR* object, so
   as long as you don't call readdir() on the same DIR* from two
   threads, you will always be fine, no need for locks.

   If one thread opens a DIR* using opendir, and another thread does
   another opendir to get another DIR*, those two threads may do
   readdir() on their respective DIR* objects just fine, too, no need
   for locks.

   And, in my opinion, if one thread does opendir(), and then many
   threads proceed to do readdir() from that DIR*, that is a stupid
   design and you should never have done that in the first place, even
   if it was threadsafe, and even if you did intend to use readdir_r!

   But one more thing!  GLIBC 2.24 marked readdir_r as DEPRECATED!
   Apparently there's serious issues with it, see:
   https://lwn.net/Articles/696474/
*/

    /** read one directory entry, or return false if end of dir */
    bool read(dirent &de) {
        if (d == NULL)
            return false;
        dirent * result = readdir(d);
        if (result == NULL)
            return false;
        de = *result;
        return true;
    }
    /** rewind DIR to first entry of directory */
    void rewind(void) {
        if (d)
            ::rewinddir(d);
    }
};

template <typename... Ts>
struct pxfe_largest_type;

template <typename T>
struct pxfe_largest_type<T>
{
    using type = T;
    static const int size = sizeof(type);
};

template <typename T, typename U, typename... Ts>
struct pxfe_largest_type<T, U, Ts...>
{
    using type =
        typename pxfe_largest_type<
            typename std::conditional<
                (sizeof(U) <= sizeof(T)), T, U
            >::type, Ts...
        >::type;
    static const int size = sizeof(type);
};

/** container for sockaddrs, supporting IPv4, IPv6, and UNIX domain */
struct pxfe_sockaddr {
    static const int addr_data_size =
        pxfe_largest_type<
            sockaddr,
            sockaddr_in,
            sockaddr_in6,
            sockaddr_un   >::size;
    uint8_t  addr_data[addr_data_size];

    sockaddr * sa(void) {
        return (sockaddr *) addr_data;
    }
    const sockaddr * sa(void) const {
        return (const sockaddr *) addr_data;
    }
    sockaddr *operator()() {
        return (sockaddr *) addr_data;
    }
    socklen_t sasize(void) const {
        switch (family()) {
        case AF_INET:  return sizeof(sockaddr_in);
        case AF_INET6: return sizeof(sockaddr_in6);
        case AF_UNIX:  return sizeof(sockaddr_un);
        }
        return 0;
    }
    sa_family_t family(void) const {
        return sa()->sa_family;
    }
    void set_family(sa_family_t f) {
        sa()->sa_family = f;
    }
    sockaddr_in *in4(void) {
        return (sockaddr_in *) addr_data;
    }
    const sockaddr_in *in4(void) const {
        return (const sockaddr_in *) addr_data;
    }
    sockaddr_in6 *in6(void) {
        return (sockaddr_in6 *) addr_data;
    }
    const sockaddr_in6 *in6(void) const {
        return (const sockaddr_in6 *) addr_data;
    }
    sockaddr_un *un(void) {
        return (sockaddr_un *) addr_data;
    }
    const sockaddr_un *un(void) const {
        return (sockaddr_un *) addr_data;
    }
    bool set4(const char * s) {
        if (inet_pton(AF_INET, s, &in4()->sin_addr) == 1)
        {
            set_family(AF_INET);
            return true;
        }
        return false;
    }
    bool set6(const char * s) {
        if (inet_pton(AF_INET6, s, &in6()->sin6_addr) == 1)
        {
            set_family(AF_INET6);
            return true;
        }
        return false;
    }
    bool set46(const char * s) {
        if (set4(s))
            return true;
        return set6(s);
    }
    void set_un(const std::string &str) { set_un(str.c_str()); }
    void set_un(const char *p) {
        int len = sizeof(un()->sun_path)-1;
        strncpy(un()->sun_path, p, len);
        un()->sun_path[len] = 0;
    }
    void get_un(std::string &str) {
        if (family() == AF_INET)
            str.assign(un()->sun_path);
        else
            str = "";
    }
    std::string Format(void) {
        std::string ret;
        switch (family()) {
        case AF_INET:
            ret.resize(INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &in4()->sin_addr,
                      (char*) ret.c_str(), ret.length());
            break;
        case AF_INET6:
            ret.resize(INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &in6()->sin6_addr,
                      (char*) ret.c_str(), ret.length());
            break;
        case AF_UNIX:
            ret = "unix:";
            ret.append(un()->sun_path);
            break;
        }
        ret.resize(strlen(ret.c_str()));
        return ret;
    }

};

/** wrapper for struct sockaddr_in (IP address) */
struct pxfe_sockaddr_in : public sockaddr_in {
    /** set family to AF_INET */
    void init(void) { sin_family = AF_INET; }
    /** set family, set addr to INADDR_ANY, set port number to p */
    void init_any(uint16_t p) { init(); set_addr(INADDR_ANY); set_port(p); }
    /** set family, set addr to a, set port number to p */
    void init(uint32_t a, uint16_t p) { init(); set_addr(a); set_port(p); }
    /** return address */
    uint32_t get_addr(void) const { return ntohl(sin_addr.s_addr); }
    /** set address only */
    void set_addr(uint32_t a) { sin_addr.s_addr = htonl(a); }
    /** return port number */
    uint16_t get_port(void) const { return ntohs(sin_port); }
    /** set port number only */
    void set_port(uint16_t p) { sin_port = htons(p); }
    /** return a struct sockaddr * pointer */
    sockaddr *operator()() {
        return (sockaddr *)static_cast<sockaddr_in*>(this);
    }
    /** return a const struct sockaddr * pointer */
    const sockaddr *operator()() const {
        return (const sockaddr *)static_cast<const sockaddr_in*>(this);
    }
    /** return sizeof(sockaddr_in) */
    socklen_t sasize(void) const { return sizeof(sockaddr_in); }
};

/** wrapper for struct sockaddr_un (unix domain address) */
struct pxfe_sockaddr_un : public sockaddr_un {
    /** init family to AF_UNIX */
    void init(void) { sun_family = AF_UNIX; }
    /** set family and path (using std::string) */
    void init(const std::string &str) { init(); set_path(str); }
    /** set family and path (using char *) */
    void init(const char *p) { init(); set_path(p); }
    /** set path only (using std::string) */
    void set_path(const std::string &str) { set_path(str.c_str()); }
    /** set path only (using char*) */
    void set_path(const char *p) {
        int len = sizeof(sun_path)-1;
        strncpy(sun_path, p, len);
        sun_path[len] = 0;
    }
    /** retrieve path into std::string */
    void get_path(std::string &str) {
        pxfe_string &_str = (pxfe_string &)str;
        int len = strnlen(sun_path, sizeof(sun_path)-1);
        _str.resize(len);
        memcpy(_str.vptr(), sun_path, len);
    }
    /** return a struct sockaddr * pointer */
    sockaddr *operator()() {
        return (sockaddr *)static_cast<sockaddr_un*>(this);
    }
    /** return a const struct sockaddr * pointer */
    const sockaddr *operator()() const {
        return (const sockaddr *)static_cast<const sockaddr_un*>(this);
    }
    /** return sizeof(sockaddr_un) */
    socklen_t sasize(void) const { return sizeof(sockaddr_un); }
};

/** container for several useful generic utility methods */
class pxfe_utils {
public:
    /** parse a std::string into a uint32_t number, return false for error */
    static bool parse_number( const std::string &s, uint32_t *val,
                              int base = 0 )
    {
        return parse_number(s.c_str(), val, base);
    }
    /** parse a char* into a uint32_t number, return false for error */
    static bool parse_number( const char *s, uint32_t *_val,
                              int base = 0 )
    {
        char * endptr = NULL;
        unsigned long val = strtoul(s, &endptr, base);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = (uint32_t) val;
            return true;
        }
        return false;
    }
    /** parse a std::string into an int32_t number, return false for error */
    static bool parse_number( const std::string &s, int32_t *val,
                              int base = 0 )
    {
        return parse_number(s.c_str(), val, base);
    }
    /** parse a char* into an int32_t number, return false for error */
    static bool parse_number( const char *s, int32_t *_val,
                              int base = 0 )
    {
        char * endptr = NULL;
        long val = strtol(s, &endptr, base);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = (int32_t) val;
            return true;
        }
        return false;
    }
    /** parse a std::string into a uint64_t number, return false for error */
    static bool parse_number( const std::string &s, uint64_t *val,
                              int base = 0 )
    {
        return parse_number(s.c_str(), val, base);
    }
    /** parse a char* into a uint64_t number, return false for error */
    static bool parse_number( const char *s, uint64_t *_val,
                              int base = 0 )
    {
        char * endptr = NULL;
        unsigned long long val = strtoull(s, &endptr, base);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = (uint64_t) val;
            return true;
        }
        return false;
    }
    /** parse a std::string into an int64_t number, return false for error */
    static bool parse_number( const std::string &s, int64_t *val,
                              int base = 0 )
    {
        return parse_number(s.c_str(), val, base);
    }
    /** parse a char* into an int64_t number, return false for error */
    static bool parse_number( const char *s, int64_t *_val,
                              int base = 0 )
    {
        char * endptr = NULL;
        long long val = strtoll(s, &endptr, base);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = (int64_t) val;
            return true;
        }
        return false;
    }
    /** parse a std::string into a double, return false for error */
    static bool parse_number( const std::string &s, double *val )
    {
        return parse_number(s.c_str(), val);
    }
    /** parse a char* into a double, return false for error */
    static bool parse_number( const char *s, double *_val )
    {
        char * endptr = NULL;
        double val = strtod(s, &endptr);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = val;
            return true;
        }
        return false;
    }
    /** parse a std::string into a float, return false for error */
    static bool parse_number( const std::string &s, float *val )
    {
        return parse_number(s.c_str(), val);
    }
    /** parse a char* into a float, return false for error */
    static bool parse_number( const char *s, float *_val )
    {
        char * endptr = NULL;
        float val = strtof(s, &endptr);
        if (endptr != NULL && *endptr == 0)
        {
            *_val = val;
            return true;
        }
        return false;
    }
    static void format_thousands(std::string &out, uint64_t  v)
    {
        if (v == 0)
        {
            out = "0";
            return;
        }
        out.resize(48);
        char * buf = (char*) out.c_str();
        memset(buf, '-', 48);
        char * triple = buf + 48;
        while (true)
        {
            uint32_t  limb = v % 1000;
            triple -= 3;
            char temp[4];
            sprintf(temp, "%03d", limb);
            triple[0] = temp[0];
            triple[1] = temp[1];
            triple[2] = temp[2];
            v /= 1000;
            if (v == 0)
                break;
            triple --;
            *triple = ',';
        }
        while (triple[0] == '0')
            triple++;
        size_t len = buf + 48 - triple;
        memmove(buf, triple, len);
        out.resize(len);
    }
};

/** container for several useful IP-related utility methods */
class pxfe_iputils {
public:
    /** gethostbyname or inet_aton to get IPv4 address (using std::string) */
    static bool hostname_to_ipaddr( const std::string &host,
                                    uint32_t * _addr )
    {
        return hostname_to_ipaddr(host.c_str(), _addr);
    }
    /** gethostbyname or inet_aton to get IPv4 address (using char*) */
    static bool hostname_to_ipaddr( const char * host, uint32_t * _addr )
    {
        uint32_t addr;
        if ( ! (inet_aton( host, (in_addr*) &addr )))
        {
            struct hostent * he;
            if (( he = gethostbyname( host )) == NULL )
            {
                const char * reason = "UNKNOWN";
                switch (h_errno)
                {
                case HOST_NOT_FOUND: reason = "host not found";    break;
                case NO_DATA:        reason = "no data returned";  break;
                case NO_RECOVERY:    reason = "name server error"; break;
                case TRY_AGAIN:      reason = "try again";         break;
                }
                fprintf( stderr, "host lookup of %s: %s\n", host, reason );
                return false;
            }
            memcpy( &addr, he->h_addr, he->h_length );
        }
        addr = ntohl(addr);
        *_addr = addr;
        return true;
    }
    /** parse a std::string into a uint16_t, return false for error */
    static bool parse_port_number( const std::string &s, uint16_t *_port )
    {
        return parse_port_number(s.c_str(), _port);
    }
    /** parse a char* into a uint16_t, return false for error */
    static bool parse_port_number( const char * portstr, uint16_t *_port )
    {
        char * endptr = NULL;
        unsigned long val = strtoul(portstr, &endptr, 0);
        if (endptr != NULL && *endptr == 0)
        {
            // consumed the whole input string, thus good integer.
            if (val > 65535)
            {
                fprintf(stderr, "value %lu is outside valid range for "
                        "a port number\n", val);
                return false;
            }
            *_port = (uint16_t) val;
            return true;
        }
        fprintf(stderr, "parse error: expecting a port number, not '%s'\n",
                portstr);
        // string was not an integer
        return false;
    }
    /** format an IPv4 address into a dotted quad string (A.B.C.D) */
    static std::string format_ip(struct in_addr &ia) {
        std::string ret;
        ret.resize(INET_ADDRSTRLEN);
        // string.resize() on a brand new string
        // guarantees the bytes are already zero'd.
        inet_ntop(AF_INET, &ia, (char*) ret.c_str(), ret.length());
        ret.resize(strlen(ret.c_str()));
        return ret;
    }
    /** format an IPv4 address into a dotted quad string (A.B.C.D) */
    static std::string format_ip(uint32_t ip) {
        struct in_addr ia;
        ia.s_addr = htonl(ip);
        return format_ip(ia);
    }
    /** format an IPv6 address into a string */
    static std::string format_ip6(struct in6_addr &ia) {
        std::string ret;
        ret.resize(INET6_ADDRSTRLEN);
        // string.resize() on a brand new string
        // guarantees the bytes are already zero'd.
        inet_ntop(AF_INET6, &ia, (char*) ret.c_str(), ret.length());
        ret.resize(strlen(ret.c_str()));
        return ret;
    }
};

/** a pxfe_fd derived class for unix datagram socket abstraction */
class pxfe_unix_dgram_socket : public pxfe_fd {
    static int counter;
    std::string path;
    bool init_common(bool new_path, pxfe_errno *e) {
        static int counter = 1;
        if (new_path)
        {
            const char * temp_path = getenv("TMP");
            if (temp_path == NULL)
                temp_path = getenv("TEMP");
            if (temp_path == NULL)
                temp_path = getenv("TMPDIR");
            if (temp_path == NULL)
                temp_path = "/tmp";
            std::ostringstream  str;
            str << temp_path << "/udslibtmp." << getpid() << "." << counter;
            counter++;
            path = str.str();
        }
        fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
        {
            if (e) e->init(errno, "socket");
            return false;
        }
        pxfe_sockaddr_un sa;
        sa.init(path);
        if (::bind(fd, sa(), sa.sasize()) < 0)
        {
            if (e) e->init(errno, "bind");
            ::close(fd);
            fd = -1;
            return false;
        }
        return true;
    }
public:
    /** constructor does nothing, you must init first */
    pxfe_unix_dgram_socket(void) {
        path.clear();
    }
    /** destructor closes the socket if it is open */
    ~pxfe_unix_dgram_socket(void) {
        if (fd > 0)
            (void) unlink( path.c_str() );
    }
    /** initialize with a random path for outbound connections */
    bool init(pxfe_errno *e = NULL) { return init_common(true,e); }
    /** initialize with a known path for inbound connections */
    bool init(const std::string &_path, pxfe_errno *e = NULL) {
        path = _path;
        return init_common(false,e);
    }
    /** retrieve the path */
    const std::string &getPath(void) const { return path; }
    /** connect to a remote path (makes a sockaddr_un) */
    bool connect(const std::string &remote_path, pxfe_errno *e = NULL) {
        pxfe_sockaddr_un  sa;
        sa.init(remote_path);
        return connect(sa, e);
    }
    /** connect to a remote path (using a sockaddr_un) */
    bool connect(const sockaddr_un &sa, pxfe_errno *e = NULL) {
        pxfe_sockaddr_un &_sa = (pxfe_sockaddr_un &)sa;
        if (::connect(fd, _sa(), sizeof(sa)) < 0) {
            if (e) e->init(errno, "connect");
            return false;
        }
        return true;
    }
    /** send a message on connected socket */
    bool send(const std::string &msg, pxfe_errno *e = NULL) {
        if (msg.size() > MAX_MSG_LEN)
        {
            if (e) e->init(EMSGSIZE, "msg.size");
            return false;
        }
        if (::send(fd, msg.c_str(), msg.size(), /*flags*/0) < 0)
        {
            if (e) e->init(errno, "send");
            return false;
        }
        return true;
    }
    /** receive a message on connected socket */
    bool recv(std::string &msg, pxfe_errno *e = NULL) {
        msg.resize(MAX_MSG_LEN);
        ssize_t msglen = ::recv(fd, (void*) msg.c_str(),
                                MAX_MSG_LEN, /*flags*/0);
        if (msglen < 0)
        {
            if (e) e->init(errno, "recv");
            msg.resize(0);
            return false;
        }
        msg.resize(msglen);
        return true;
    }
    /** send a message to a remote path (using path) */
    bool send(const std::string &msg, const std::string &remote_path,
              pxfe_errno *e = NULL) {
        pxfe_sockaddr_un  sa;
        sa.init(remote_path);
        return send(msg, sa, e);
    }
    /** send a message to a remote path (using sockaddr_un) */
    bool send(const std::string &msg, const sockaddr_un &sa,
              pxfe_errno *e = NULL) {
        pxfe_string &_msg = (pxfe_string &)msg;
        pxfe_sockaddr_un &_sa = (pxfe_sockaddr_un &)sa;
        if (msg.size() > MAX_MSG_LEN)
        {
            if (e) e->init(EMSGSIZE, "msg.size");
            return false;
        }
        if (::sendto(fd, _msg.vptr(), _msg.size(), /*flags*/0,
                     _sa(), sizeof(sa)) < 0) {
            if (e) e->init(errno, "sendto");
            return false;
        }
        return true;
    }
    /** receive a message from any source, return source path in string */
    bool recv(std::string &msg, std::string &remote_path,
              pxfe_errno *e = NULL) {
        pxfe_sockaddr_un  sa;
        bool ret = recv(msg, sa, e);
        if (ret == false)
            return false;
        sa.get_path(remote_path);
        return true;
    }
    /** receive a message from any source, return source path in sockaddr_un */
    bool recv(std::string &msg, sockaddr_un &sa,
              pxfe_errno *e = NULL) {
        pxfe_string &_msg = (pxfe_string &)msg;
        pxfe_sockaddr_un &_sa = (pxfe_sockaddr_un &)sa;
        _msg.resize(MAX_MSG_LEN);
        socklen_t salen = sizeof(sa);
        ssize_t msglen = ::recvfrom(fd, _msg.vptr(), _msg.size(),
                                    /*flags*/0, _sa(), &salen);
        if (msglen < 0)
        {
            if (e) e->init(errno, "recvfrom");
            msg.resize(0);
            return false;
        }
        msg.resize(msglen);
        return true;
    }
};

/** a pxfe_fd subclass for UDP sockets */
class pxfe_udp_socket : public pxfe_fd {
public:
    /** constructor, note this does NOT init the descriptor */
    pxfe_udp_socket(void) { }
    /** destructor closes the descriptor if it is open */
    ~pxfe_udp_socket(void) { }
    /** init the socket, does not bind it, return false if failure */
    bool init(pxfe_errno *e = NULL) {
        return init(-1,e);
    }
    /** init the socket specifying a port number, return false if failure */
    bool init(int port, pxfe_errno *e = NULL) {
        fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
        {
            if (e) e->init(errno, "socket");
            return false;
        }
        if (port == -1)
            // no bind needed
            return true;
        pxfe_sockaddr_in sa;
        sa.init_any(port);
        if (::bind(fd, sa(), sa.sasize()) < 0)
        {
            if (e) e->init(errno, "bind");
            return false;
        }
        return true;
    }
    /** not sure what this is useful for */
    bool init_proto(int type, int protocol, pxfe_errno *e = NULL) {
        fd = ::socket(AF_INET, type | SOCK_CLOEXEC, protocol);
        if (fd < 0)
        {
            if (e) e->init(errno, "socket");
            return false;
        }
        return true;
    }
    /** put udp socket in connected mode to remote addr/port */
    bool connect(uint32_t addr, short port, pxfe_errno *e = NULL) {
        pxfe_sockaddr_in sa;
        sa.init(addr, port);
        return connect(sa,e);
    }
    /** put udp socket in connected mode to remote addr/port */
    bool connect(const sockaddr_in &sa, pxfe_errno *e = NULL) {
        pxfe_sockaddr_in &_sa = (pxfe_sockaddr_in &) sa;
        if (::connect(fd, _sa(), sizeof(sa)) < 0)
        {
            if (e) e->init(errno, "connect");
            return false;
        }
        return true;
    }
    /** send data on connected-mode udp port */
    bool send(const std::string &msg, pxfe_errno *e = NULL) {
        if (msg.size() > MAX_MSG_LEN)
        {
            if (e) e->init(EMSGSIZE, "msg.size");
            return false;
        }
        if (::send(fd, msg.c_str(), msg.size(), /*flags*/0) < 0)
        {
            if (e) e->init(errno, "send");
            return false;
        }
        return true;
    }
    /** recv data from connected-mode udp port */
    bool recv(std::string &msg, pxfe_errno *e = NULL) {
        msg.resize(MAX_MSG_LEN);
        ssize_t msglen = ::recv(fd, (void*) msg.c_str(),
                                MAX_MSG_LEN, /*flags*/0);
        if (msglen < 0)
        {
            if (e) e->init(errno, "recv");
            msg.resize(0);
            return false;
        }
        msg.resize(msglen);
        return true;
    }
    /** send data on promiscuous port to any destination */
    bool send(const std::string &msg, const sockaddr_in &sa,
              pxfe_errno *e = NULL) {
        pxfe_sockaddr_in &_sa = (pxfe_sockaddr_in &) sa;
        if (msg.size() > MAX_MSG_LEN)
        {
            if (e) e->init(EMSGSIZE, "msg.size");
            return false;
        }
        if (::sendto(fd, msg.c_str(), msg.size(), /*flags*/0,
                     _sa(), sizeof(sa)) < 0)
        {
            if (e) e->init(errno, "sendto");
            return false;
        }
        return true;
    }
    /** receive data on promiscuous port from any destination */
    bool recv(std::string &msg, sockaddr_in &sa,
              pxfe_errno *e = NULL) {
        pxfe_sockaddr_in &_sa = (pxfe_sockaddr_in &) sa;
        msg.resize(MAX_MSG_LEN);
        socklen_t salen = sizeof(sa);
        ssize_t msglen = ::recvfrom(fd, (void*) msg.c_str(), MAX_MSG_LEN,
                                    /*flags*/0, _sa(), &salen);
        if (msglen < 0)
        {
            if (e) e->init(errno, "recvfrom");
            msg.resize(0);
            return false;
        }
        msg.resize(msglen);
        return true;
    }
};

/** a pxfe_fd derived template class for TCP or SCTP sockets
 * (specialization types pxfe_sctp_stream_socket and
 * pxfe_tcp_stream_socket provided) */
template <int protocolNumber>
class _pxfe_stream_socket : public pxfe_fd {
    pxfe_sockaddr_in sa;
    _pxfe_stream_socket(int _fd, const sockaddr_in &_sa) {
        fd = _fd;
        sa = (pxfe_sockaddr_in&)_sa;
    }
public:
    /** constructor, note this does NOT init the descriptor */
    _pxfe_stream_socket(void) { }
    /** destructor closes the descriptor if it is open */
    ~_pxfe_stream_socket(void) { }
    /** init the socket, does not bind it, return false if failure */
    bool init(pxfe_errno *e = NULL) {
        fd = ::socket(PF_INET, SOCK_STREAM | SOCK_CLOEXEC, protocolNumber);
        if (fd < 0) {
            if (e) e->init(errno, "socket");
            return false;
        }
        return true;
    }
    /** set TCP_USER_TIMEOUT socket option to time in milliseconds */
    bool set_user_timeout(unsigned int timeout_ms, pxfe_errno *e = NULL) {
        if (setsockopt( fd, IPPROTO_TCP, TCP_USER_TIMEOUT,
                        (void*) &timeout_ms, sizeof(timeout_ms)) < 0)
        {
            if (e) e->init(errno, "setsockopt timeout");
            return false;
        }
        return true;
    }
    /** attempt to connect out to addr/port, returns false if failure */
    bool connect(uint32_t addr, short port, pxfe_errno *e = NULL) {
        sa.init(addr, port);
        if (::connect(fd, sa(), sizeof(sa)) < 0) {
            if (e) e->init(errno, "connect");
            return false;
        }
        return true;
    }
    /** init the socket for listening, binding to particular local
     * interface only, return false if failure, you must call listen
     * to start it listening */
    bool init(uint32_t addr, short port, bool reuse=false,
              pxfe_errno *e = NULL) {
        if (init(e) == false)
            return false;
        sa.init(addr, port);
        if (reuse) {
            int v = 1;
            setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void*) &v, sizeof( v ));
        }
        if (::bind(fd, sa(), sa.sasize()) < 0) {
            if (e) e->init(errno, "bind");
            return false;
        }
        return true;
    }
    /** init for listening, binding to INADDR_ANY, you must call
     * listen to start it listening */
    bool init(short port, bool reuse=false, pxfe_errno *e = NULL) {
        return init(INADDR_ANY,port,reuse,e);
    }
    /** start a bound socket listening for incoming connections, note
     * you can't do this without calling one of the init methods first */
    void listen(void) {
        (void) ::listen(fd, 1);
    }
    /** call accept on the socket, returning a new connected stream
     * object, or returns NULL if accept fails */
    _pxfe_stream_socket *accept(pxfe_errno *e = NULL) {
        socklen_t sz = sizeof(sa);
        int fdnew = ::accept4(fd, sa(), &sz, SOCK_CLOEXEC);
        if (fdnew < 0) {
            if (e) e->init(errno, "accept");
            return NULL;
        }
        _pxfe_stream_socket *s = new _pxfe_stream_socket(fdnew,sa);
        if (s)
            return s;
        e->init(0,"");
        ::close(fdnew);
        return NULL;
    }
    /** return address of the connected peer */
    uint32_t get_peer_addr(void) const { return sa.get_addr(); }
    /** send data on a connected socket, or return false if failure */
    bool send(const std::string &msg, pxfe_errno *e = NULL) {
        if (msg.size() > MAX_MSG_LEN) {
            if (e) e->init(EMSGSIZE, "msg.size");
            return false;
        }
        if (::send(fd, msg.c_str(), msg.size(), /*flags*/0) < 0) {
            if (e) e->init(errno, "send");
            return false;
        }
        return true;
    }
    /** receive data on a connected socket, or return false if failure */
    bool recv(std::string &msg, pxfe_errno *e = NULL) {
        msg.resize(MAX_MSG_LEN);
        ssize_t msglen = ::recv(fd, (void*) msg.c_str(),
                                MAX_MSG_LEN, /*flags*/0);
        if (msglen < 0) {
            if (e) e->init(errno, "recv");
            msg.resize(0);
            return false;
        }
        msg.resize(msglen);
        return true;
    }
};

#ifdef IPPROTO_SCTP
/** an SCTP specialization of a _pxfe_stream_socket template */
typedef _pxfe_stream_socket<IPPROTO_SCTP> pxfe_sctp_stream_socket;
#endif

/** a TCP specialization of a _pxfe_stream_socket template */
typedef _pxfe_stream_socket<0> pxfe_tcp_stream_socket;

/** class wrappering fd_set and all it's operations */
class pxfe_fd_set {
    fd_set  fds;
    int     max_fd;
public:
    /** constructor does FD_ZERO on the fd_set */
    pxfe_fd_set(void) { zero(); }
    ~pxfe_fd_set(void) { /*nothing for now*/ }
    /** FD_ZERO the fd_set */
    void zero(void) { FD_ZERO(&fds); max_fd=-1; }
    /** set a file descriptor in the set (any pxfe_fd derived class) */
    void set(const pxfe_fd &fd) { set(fd.getFd()); }
    /** remove a descriptor from the set (any pxfe_fd derived class) */
    void clr(const pxfe_fd &fd) { clr(fd.getFd()); }
    /** test whether descriptor in the set (any pxfe_fd derived class) */
    bool is_set(const pxfe_fd &fd) { return is_set(fd.getFd()); }
    /** set a descriptor (specifying fd) */
    void set(int fd) { FD_SET(fd, &fds); if (fd > max_fd) max_fd = fd; }
    /** remove descriptor from the set (specifying fd) */
    void clr(int fd) { FD_CLR(fd, &fds); }
    /** test whether descriptor is in the set (specifying fd) */
    bool is_set(int fd) { return FD_ISSET(fd, &fds) != 0; }
    /** return an fd_set * pointer (ie, for select(2)) */
    fd_set *operator()(void) { return max_fd==-1 ? NULL : &fds; }
    /** return a value suitable for the "nfds" arg of select(2) */
    int nfds(void) { return max_fd + 1; }
};

/** helper class to make select(2) very easy to use */
struct pxfe_select {
    /** set descriptors for-read in this set */
    pxfe_fd_set  rfds;
    /** set descriptors for-write in this set */
    pxfe_fd_set  wfds;
    /** set descriptors for-exception in this set */
    pxfe_fd_set  efds;
    /** set the timeout value */
    pxfe_timeval   tv;
    /** note you will need to re-fill rfds, wfds, efds, and tv every
     * time you call one of the select methods */
    pxfe_select(void) { }
    ~pxfe_select(void) { }
    /** call select with a timeout of forever (ignore "tv" member) */
    int select_forever(pxfe_errno *e = NULL) {
        return _select(NULL, e);
    }
    /** call select with a timeout (using "tv" member) */
    int select(pxfe_errno *e = NULL) {
        return _select(tv(), e);
    }
    /** call select if you have your own timeval* you want to pass in */
    int _select(struct timeval *tvp, pxfe_errno *e = NULL) {
        int n = rfds.nfds(), n2 = wfds.nfds(), n3 = efds.nfds();
        if (n < n2) n = n2;
        if (n < n3) n = n3;
        int cc = ::select(n, rfds(), wfds(), efds(), tvp);
        if (cc < 0)
            if (e) e->init(errno, "select");
        return cc;
    }
};

/** helper class to make poll(2) very easy to use */
class pxfe_poll {
    struct fdindex {
        fdindex(void) { ind = -1; }
        int ind;
    };
    std::vector<int> freestack; // value : index into fds
    std::vector<fdindex> by_fd; // index : fd#, value : index into fds
    std::vector<pollfd> fds;
public:
    /** note (unlike pxfe_select) you only need to set a descriptor once,
     * as the events registered for persist across calls to poll method.
     */
    pxfe_poll(void) { }
    /** add or remove a descriptor to/from the poll set, if setting,
     * use POLLIN | POLLOUT | POLLERR, or zero to remove a descriptor */
    void set(pxfe_fd &fd, short events) {
        set(fd.getFd(), events);
    }
    /** add or remove a descriptor to/from the poll set, if setting,
     * use POLLIN | POLLOUT | POLLERR, or zero to remove a descriptor */
    void set(int fd, short events) {
        if (fd >= (int)by_fd.size())
            by_fd.resize(fd+1);
        fdindex &ind = by_fd[fd];
        pollfd *pfd = NULL;
        if (events == 0)
        {
            if (ind.ind == -1)
                // nothing to do
                return;
            pfd = &fds[ind.ind];
            pfd->events = 0;
            pfd->fd = -1;
            freestack.push_back(ind.ind);
            ind.ind = -1;
        }
        else
        {
            if (ind.ind == -1)
            {
                if (freestack.size() == 0)
                {
                    ind.ind = fds.size();
                    fds.resize(fds.size() + 1);
                }
                else
                {
                    ind.ind = freestack.back();
                    freestack.pop_back();
                }
                pfd = &fds[ind.ind];
                pfd->fd = fd;
            }
            else
            {
                pfd = &fds[ind.ind];
            }
            pfd->events = events;
        }
    }
    /** wait for one of the descriptors in the set to become ready,
     * timeout is in milliseconds */
    int poll(int timeout) {
        return ::poll(fds.data(), fds.size(), timeout);
    }
    /** retrieve events set for a descriptor, returns 0 if none */
    short eget(int fd) {
        if (fd >= (int)by_fd.size())
            return 0;
        fdindex &ind = by_fd[fd];
        if (ind.ind == -1)
            return 0;
        return fds[ind.ind].events;
    }
    /** retrieve events that actually occurred on a descriptor,
     * or 0 if none */
    short rget(int fd) {
        if (fd >= (int)by_fd.size())
            return 0;
        fdindex &ind = by_fd[fd];
        if (ind.ind == -1)
            return 0;
        return fds[ind.ind].revents;
    }
    /** over time, internal data structure may get large and
     * fragmented, call this from time to time to clean up and
     * compact them */
    void compact(void) {
        int ind;
        // the compaction works only if the fds vector
        // is processed in reverse; so sort the freestack
        // in ascending order and process it backwards.
        std::sort(freestack.begin(), freestack.end());
        while (freestack.size() > 0)
        {
            ind = freestack.back();
            freestack.pop_back();
            // stupid ubuntu 10, i wanted to use "auto" here, grr.
            std::vector<pollfd>::iterator it = fds.begin() + ind;
            for (it = fds.erase(it); it != fds.end(); it++)
                // adjust indexes which have now moved down by 1
                if (it->fd != -1)
                    by_fd[it->fd].ind --;
        }
        // trim by_fd of trailing -1's
        for (ind = by_fd.size()-1;
             ind >= 0 && by_fd[ind].ind == -1;
             ind--)
            ;
        by_fd.resize(ind+1);
    }
    /** debugging, print out contents of this object */
    void print(void) {
        size_t ind;
        printf("by_fd.size = %d : ", (int) by_fd.size());
        for (ind = 0; ind < by_fd.size(); ind++)
            printf(" %d", by_fd[ind].ind);
        printf("\nfreestack.size = %d : ", (int) freestack.size());
        for (ind = 0; ind < freestack.size(); ind++)
            printf(" %d", freestack[ind]);
        printf("\nfds.size = %d : ", (int) fds.size());
        for (ind = 0; ind < fds.size(); ind++)
            printf(" [%d %04x]", fds[ind].fd, fds[ind].events);
        printf("\n");
    }
};

// needs doxygen
class pxfe_ticker : public pxfe_pthread {
    bool paused;
    pxfe_pipe closer_pipe;
    pxfe_pipe pipe;
    pxfe_timeval interval;
    /*virtual*/ void * entry(void *arg) {
        char c = 1;
        pxfe_select   sel;
        while (1) {
            sel.tv = interval;
            sel.rfds.zero();
            sel.rfds.set(closer_pipe.readEnd);
            if (sel.select() <= 0) {
                if (paused == false)
                    if (pipe.write(&c, 1) != 1)
                        fprintf(stderr, "pxfe_ticker: write failed\n");
                continue;
            }
            if (sel.rfds.is_set(closer_pipe.readEnd)) {
                if (closer_pipe.read(&c, 1) != 1)
                    fprintf(stderr, "pxfe_ticker: read failed\n");
                break;
            }
        }
        return NULL;
    }
    /*virtual*/ void send_stop(void) {
        char c = 1;
        if (closer_pipe.write(&c, 1) != 1)
            fprintf(stderr, "pxfe_ticker: write failed\n");
    }
public:
    pxfe_ticker(void) {
        interval.set(1,0);
        paused = false;
    }
    ~pxfe_ticker(void) {
        paused = true;
        stopjoin();
    }
    void start(time_t s, long us) {
        interval.set(s,us);
        if (!running())
            create();
        paused = false;
    }
    void pause(void) { paused = true; }
    void resume(void) { paused = false; }
    int fd(void) { return pipe.readEnd; }
    bool doread(void) {
        char c;
        int cc = ::read(pipe.readEnd, &c, 1);
        if (cc > 0)
            return true;
        return false;
    }
};

#endif /* __posix_fe_h__ */

/** \mainpage posix_fe

This is a template library for abstracting a number of common posix
APIs that normally require a lot of boring boilerplate code that
PFK got \em really tired of typing over and over in many applications.

Here's some sample code.

TO DOCUMENT:

\code
pxfe_string
pxfe_utils::parse_number
pxfe_pthread_cond
pxfe_semaphore
pxfe_pthread
pxfe_sockaddr_in
pxfe_sockaddr_un
pxfe_iputils
pxfe_unix_dgram_socket
_pxfe_stream_socket (pxfe_sctp_stream_socket / pxfe_tcp_stream_socket)
pxfe_udp_socket
\endcode

Demo of pxfe_pipe, pxfe_errno, pxfe_select, pxfe_ticker:

\code
 pxfe_fd      fd; // assume this was already initialized
 pxfe_pipe    p;
 pxfe_string  buf;
 pxfe_errno   e;
 pxfe_select  sel;
 pxfe_ticker  tick;
 bool done = false;
 tick.start(1, 0);
 while (!done)
 {
   sel.rfds.set(p.readEnd);
   sel.rfds.set(fd);
   sel.rfds.set(ticker.fd());
   sel.tv.set(10,0);
   sel.select();
   if (sel.rfds.isset(p.readEnd))
   {
     if (buf.read(fd, 4000, &e) == false)
     {
        std::cerr << "read: error " << e.e
                  << ": " << e.Format() << std::endl;
        done = true;
     }
   }
   if (sel.rfds.isset(fd))
      handle_fd(fd); // read and do something
   if (sel.rfds.isset(ticker.fd()))
   {
     ticker.doread();
     // handle tick here
   }
 }
 tick.pause();
\endcode

the same code, except using pxfe_poll:

\code
 pxfe_fd      fd; // assume this was already initialized
 pxfe_pipe    p;  // assume someone else is wring to p.writeEnd
 pxfe_string  buf;
 pxfe_errno   e;
 pxfe_poll    p;
 pxfe_ticker  tick;
 bool done = false;
 tick.start(1, 0);
 p.set(fd, POLLIN);
 p.set(ticker.fd, POLLIN);
 p.set(p.readEnd, POLLIN);
 while (!done)
 {
   p.poll(10000);
   if (p.rget(p.readEnd) & POLLIN)
   {
     if (buf.read(fd, 4000, &e) == false)
     {
        std::cerr << "read: error " << e.e
                  << ": " << e.Format() << std::endl;
        done = true;
     }
   }
   if (p.rget(fd) & POLLIN)
      handle_fd(fd); // read and do something
   if (p.rget(ticker.fd()) & POLLIN)
   {
     ticker.doread();
     // handle tick here
   }
 }
 tick.pause();
\endcode

Example of pxfe_readdir:

\code
  std::string some_path;
  pxfe_readdir d;
  if (d.open(some_path.c_str()))
  {
    dirent de;
    while (d.read(de))
    {
      string dename(de.d_name);
      if (dename == "." || dename == "..")
        continue;
      todo.push_back(fileInfo(some_path + "/" + dename));
    }
  }
  else
  {
    pxfe_errno e(errno);
    cout << "opendir: " << e.Format() << endl;
  }
\endcode

More about pxfe_timeval / pxfe_timespec :

\code
pxfe_timeval   begin, end, diff, tv;
// pxfe_timespec has all the same methods

begin.getNow();
// something that takes time we want to measure
end.getNow();
// operator- and operator+ handle tv_usec underflow/borrow
// and overflow/carry to tv_sec
diff = end - begin;
cout << "started at " << begin.Format() << endl;
cout << "ended at " << end.Format("%H:%M:%S") << endl;
cout << "it took " << diff.msecs() << " milliseconds" << endl;

tv.set(1,0);
// note operator() returns struct timeval*
int cc = select(fdcount, &rfds, &wfds, NULL, tv());
\endcode

pxfe_pthread_mutex / pxfe_pthread_mutex_lock:

\code
pxfe_pthread_mutex m;

void func1(void)
{
  pxfe_pthread_mutex_lock l(m);  // constructor locks mutex
  // critical region
} // lock released when destructor invoked

void func2(void)
{
  pxfe_pthread_mutex_lock l(m,false); // does not lock mutex
  l.lock();
  // critical region
  l.unlock();
  // noncritical region
  l.lock();
  // critical region
} // lock destructor unlocks during cleanup
\endcode

*/
