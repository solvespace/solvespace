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

#define ANGLE_COS_EPS   (1e-6)
#define LENGTH_EPS      (1e-6)
#define VERY_POSITIVE   (1e10)
#define VERY_NEGATIVE   (-1e10)


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

inline int WRAP(int v, int n) {
    // Clamp it to the range [0, n)
    while(v >= n) v -= n;
    while(v < 0) v += n;
    return v;
}
inline double WRAP_NOT_0(double v, double n) {
    // Clamp it to the range (0, n]
    while(v > n) v -= n;
    while(v <= 0) v += n;
    return v;
}
inline double WRAP_SYMMETRIC(double v, double n) {
    // Clamp it to the range (-n/2, n/2]
    while(v >   n/2) v -= n;
    while(v <= -n/2) v += n;
    return v;
}

// Why is this faster than the library function?
inline double ffabs(double v) { return (v > 0) ? v : (-v); }

inline double Random(double vmax) {
    return (vmax*rand()) / RAND_MAX;
}

}

#endif //SOLVESPACE_UTIL_H
