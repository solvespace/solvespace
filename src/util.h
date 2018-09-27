//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_UTIL_H
#define SOLVESPACE_UTIL_H

#include <ctype.h>
#include <stddef.h>
#include <iterator>
#include <functional>
#include <platform.h>

// The few floating-point equality comparisons in SolveSpace have been
// carefully considered, so we disable the -Wfloat-equal warning for them
#ifdef __clang__
#   define EXACT(expr) \
        (_Pragma("clang diagnostic push") \
         _Pragma("clang diagnostic ignored \"-Wfloat-equal\"") \
         (expr) \
         _Pragma("clang diagnostic pop"))
#else
#   define EXACT(expr) (expr)
#endif

#define AUTOSAVE_EXT "slvs~"

namespace SolveSpace{

using std::min;
using std::max;
using std::swap;

// Utility functions that are provided in the platform-independent code.
class utf8_iterator : std::iterator<std::forward_iterator_tag, char32_t> {
const char *p, *n;
public:
utf8_iterator(const char *p) : p(p), n(NULL) {}
bool           operator==(const utf8_iterator &i) const { return p==i.p; }
bool           operator!=(const utf8_iterator &i) const { return p!=i.p; }
ptrdiff_t      operator- (const utf8_iterator &i) const { return p -i.p; }
utf8_iterator& operator++()    { **this; p=n; n=NULL; return *this; }
utf8_iterator  operator++(int) { utf8_iterator t(*this); operator++(); return t; }
char32_t       operator*();
const char*    ptr() const { return p; }
};
class ReadUTF8 {
        const std::string &str;
        public:
        ReadUTF8(const std::string &str) : str(str) {}
        utf8_iterator begin() const { return utf8_iterator(&str[0]); }
        utf8_iterator end()   const { return utf8_iterator(&str[str.length()]); }
};

#define arraylen(x) (sizeof((x))/sizeof((x)[0]))
#define PI (3.1415926535897931)
void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                double a21, double a22, double a23, double a24,
                double a31, double a32, double a33, double a34,
                double a41, double a42, double a43, double a44);
void MultMatrix(double *mata, double *matb, double *matr);

int64_t GetMilliseconds();
void Message(const char *fmt, ...);
void MessageAndRun(std::function<void()> onDismiss, const char *fmt, ...);
void Error(const char *fmt, ...);

void ImportDxf(const Platform::Path &file);
void ImportDwg(const Platform::Path &file);

}

#ifndef __OBJC__
using namespace SolveSpace;
#endif

#endif //SOLVESPACE_UTIL_H
