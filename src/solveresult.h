//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SOLVERESULT_H
#define SOLVESPACE_SOLVERESULT_H

#include <stdint.h>

namespace SolveSpace {

enum class SolveResult : uint32_t {
    OKAY                     = 0,
    DIDNT_CONVERGE           = 10,
    REDUNDANT_OKAY           = 11,
    REDUNDANT_DIDNT_CONVERGE = 12,
    TOO_MANY_UNKNOWNS        = 20
};

}

#endif //SOLVESPACE_SOLVERESULT_H
