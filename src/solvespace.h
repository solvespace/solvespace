//-----------------------------------------------------------------------------
// All declarations not grouped specially elsewhere.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_H
#define SOLVESPACE_H

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ss_util.h"
#include "platform.h"
#include "list.h"
#include "solvespaceui.h"
#include "globals.h"
#include "dsc.h"
#include "sketch.h"
#include "resource.h"
#include "polygon.h"
#include "srf/surface.h"
#include "filewriters.h"
#include "expr.h"
#include "render/render.h"
#include "platform/gui.h"
#include "ui.h"
#include "ttf.h"


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

namespace SolveSpace {

using std::min;
using std::max;
using std::swap;


//================
// From the platform-specific code.


const size_t MAX_RECENT = 8;
extern Platform::Path RecentFile[MAX_RECENT];

#define AUTOSAVE_EXT "slvs~"


// End of platform-specific functions
//================









const std::vector<double> &StipplePatternDashes(StipplePattern pattern);
double StipplePatternLength(StipplePattern pattern);


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

#endif
