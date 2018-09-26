//
// Created by benjamin on 9/24/18.
//

#ifndef SOLVESPACE_UTIL_H
#define SOLVESPACE_UTIL_H

// C includes
#include <stdint.h>
// C++ includes
#include <string>

// Library includes

// defines

// Debugging functions
#if defined(__GNUC__)
#define ssassert(condition, message) \
    do { \
        if(__builtin_expect((condition), true) == false) { \
            SolveSpace::AssertFailure(__FILE__, __LINE__, __func__, #condition, message); \
            __builtin_unreachable(); \
        } \
    } while(0)
#else
#define ssassert(condition, message) \
    do { \
        if((condition) == false) { \
            SolveSpace::AssertFailure(__FILE__, __LINE__, __func__, #condition, message); \
            abort(); \
        } \
    } while(0)
#endif

#ifndef isnan
#   define isnan(x) (((x) != (x)) || (x > 1e11) || (x < -1e11))
#endif

#define PI (3.1415926535897931)

namespace SolveSpace {

// functions
#if defined(__GNUC__)
__attribute__((noreturn))
#endif
void AssertFailure(const char *file, unsigned line, const char *function,
                   const char *condition, const char *message);

#if defined(__GNUC__)
__attribute__((__format__ (__printf__, 1, 2)))
#endif
std::string ssprintf(const char *fmt, ...);

}

#endif //SOLVESPACE_UTIL_H
