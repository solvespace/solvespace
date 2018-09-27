//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_SYSTEM_H
#define SOLVESPACE_SYSTEM_H

#include "handle.h"


namespace SolveSpace {

class System {
public:
    enum { MAX_UNKNOWNS = 1024 };

    EntityList                      entity;
    ParamList                       param;
    IdList<Equation,hEquation>      eq;

    // A list of parameters that are being dragged; these are the ones that
    // we should put as close as possible to their initial positions.
    List<hParam>                    dragged;

    enum {
        // In general, the tag indicates the subsys that a variable/equation
        // has been assigned to; these are exceptions for variables:
                VAR_SUBSTITUTED      = 10000,
        VAR_DOF_TEST         = 10001,
        // and for equations:
                EQ_SUBSTITUTED       = 20000
    };

    // The system Jacobian matrix
    struct {
        // The corresponding equation for each row
        hEquation   eq[MAX_UNKNOWNS];

        // The corresponding parameter for each column
        hParam      param[MAX_UNKNOWNS];

        // We're solving AX = B
        int m, n;
        struct {
            Expr        *sym[MAX_UNKNOWNS][MAX_UNKNOWNS];
            double       num[MAX_UNKNOWNS][MAX_UNKNOWNS];
        }           A;

        double      scale[MAX_UNKNOWNS];

        // Some helpers for the least squares solve
        double AAt[MAX_UNKNOWNS][MAX_UNKNOWNS];
        double Z[MAX_UNKNOWNS];

        double      X[MAX_UNKNOWNS];

        struct {
            Expr        *sym[MAX_UNKNOWNS];
            double       num[MAX_UNKNOWNS];
        }           B;
    } mat;

    static const double RANK_MAG_TOLERANCE, CONVERGE_TOLERANCE;
    int CalculateRank();
    bool TestRank();
    static bool SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS],
                                  double B[], int N);
    bool SolveLeastSquares();

    bool WriteJacobian(int tag);
    void EvalJacobian();

    void WriteEquationsExceptFor(hConstraint hc, Group *g);
    void FindWhichToRemoveToFixJacobian(Group *g, List<hConstraint> *bad, bool forceDofCheck);
    void SolveBySubstitution();

    bool IsDragged(hParam p);

    bool NewtonSolve(int tag);

    void MarkParamsFree(bool findFree);
    int CalculateDof();

    SolveResult Solve(Group *g, int *dof, List<hConstraint> *bad,
                      bool andFindBad, bool andFindFree, bool forceDofCheck = false);

    SolveResult SolveRank(Group *g, int *dof, List<hConstraint> *bad,
                          bool andFindBad, bool andFindFree, bool forceDofCheck = false);

    void Clear();
};

}

#endif //SOLVESPACE_SYSTEM_H
