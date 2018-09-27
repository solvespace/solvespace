//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SCURVEPT_H
#define SOLVESPACE_SCURVEPT_H

#include "dsc.h"

namespace SolveSpace{

// Stuff for the surface trim curves: piecewise linear
class SCurvePt {
public:
    int         tag;
    Vector      p;
    bool        vertex;
};

}

#endif //SOLVESPACE_SCURVEPT_H
