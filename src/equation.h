//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_EQUATION_H
#define SOLVESPACE_EQUATION_H

#include "expr.h"

namespace SolveSpace {

class Equation {
public:
    int         tag;
    hEquation   h;

    Expr        *e;

    void Clear() {}
};

}

#endif //SOLVESPACE_EQUATION_H
