#ifndef SOLVESPACE_SOLVERSYSTEM_H
#define SOLVESPACE_SOLVERSYSTEM_H

#include <string>
#include <iostream>
#include "solvespace.h"

struct SolverResult {
    SolveResult status;
    int dof;
    int rank;
    std::vector<hConstraint> bad;
};

class SolverSystem {
public:
  SolverResult Solve(GroupBase *g);
};

#endif