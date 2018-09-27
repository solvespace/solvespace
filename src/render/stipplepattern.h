//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_STIPPLEPATTERN_H
#define SOLVESPACE_STIPPLEPATTERN_H

#include <stdint.h>
#include <vector>

namespace SolveSpace{

enum class StipplePattern : uint32_t {
    CONTINUOUS     = 0,
    SHORT_DASH     = 1,
    DASH           = 2,
    LONG_DASH      = 3,
    DASH_DOT       = 4,
    DASH_DOT_DOT   = 5,
    DOT            = 6,
    FREEHAND       = 7,
    ZIGZAG         = 8,

    LAST           = ZIGZAG
};

const std::vector<double> &StipplePatternDashes(StipplePattern pattern);
double StipplePatternLength(StipplePattern pattern);

}

#endif //SOLVESPACE_STIPPLEPATTERN_H
