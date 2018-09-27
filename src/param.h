//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_PARAM_H
#define SOLVESPACE_PARAM_H

#include "handle.h"
#include "list.h"

namespace SolveSpace {

class Param {
public:
    int         tag;
    hParam      h;

    double      val;
    bool        known;
    bool        free;

    // Used only in the solver
    hParam      substd;

    static const hParam NO_PARAM;

    void Clear() {}
};

typedef IdList<Param,hParam> ParamList;
}

#endif //SOLVESPACE_PARAM_H
