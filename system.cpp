#include "solvespace.h"

void System::WriteJacobian(int eqTag, int paramTag) {
    int a, i, j;

    j = 0;
    for(a = 0; a < param.n; a++) {
        Param *p = &(param.elem[a]);
        if(p->tag != paramTag) continue;
        J.param[j] = p->h;
        j++;
    }
    J.n = j;

    i = 0;
    for(a = 0; a < eq.n; a++) {
        Equation *e = &(eq.elem[a]);
        if(e->tag != eqTag) continue;

        J.eq[i] = eq.elem[i].h;
        J.B.sym[i] = eq.elem[i].e;
        for(j = 0; j < J.n; j++) {
            J.sym[i][j] = e->e->PartialWrt(J.param[j]);
        }
        i++;
    }
    J.m = i;
}

void System::EvalJacobian(void) {
    int i, j;
    for(i = 0; i < J.m; i++) {
        for(j = 0; j < J.n; j++) {
            J.num[i][j] = (J.sym[i][j])->Eval();
        }
    }
}

bool System::Tol(double v) {
    return (fabs(v) < 0.01);
}

void System::GaussJordan(void) {
    int i, j;

    for(j = 0; j < J.n; j++) {
        J.bound[j] = false;
    }

    // Now eliminate.
    i = 0;
    for(j = 0; j < J.n; j++) {
        // First, seek a pivot in our column.
        int ip, imax;
        double max = 0;
        for(ip = i; ip < J.m; ip++) {
            double v = fabs(J.num[ip][j]);
            if(v > max) {
                imax = ip;
                max = v;
            }
        }
        if(!Tol(max)) {
            // There's a usable pivot in this column. Swap it in:
            int js;
            for(js = j; js < J.n; js++) {
                double temp;
                temp = J.num[imax][js];
                J.num[imax][js] = J.num[i][js];
                J.num[i][js] = temp;
            }

            // Get a 1 as the leading entry in the row we're working on.
            double v = J.num[i][j];
            for(js = 0; js < J.n; js++) {
                J.num[i][js] /= v;
            }

            // Eliminate this column from rows except this one.
            int is;
            for(is = 0; is < J.m; is++) {
                if(is == i) continue;

                // We're trying to drive J.num[is][j] to zero. We know
                // that J.num[i][j] is 1, so we want to subtract off
                // J.num[is][j] times our present row.
                double v = J.num[is][j];
                for(js = 0; js < J.n; js++) {
                    J.num[is][js] -= v*J.num[i][js];
                }
                J.num[is][j] = 0;
            }

            // And mark this as a bound variable.
            J.bound[j] = true;

            // Move on to the next row, since we just used this one to
            // eliminate from column j.
            i++;
            if(i >= J.m) break;
        }
    }
}

bool System::SolveLinearSystem(void) {
    if(J.m != J.n) oops();
    // Gaussian elimination, with partial pivoting. It's an error if the
    // matrix is singular, because that means two constraints are
    // equivalent.
    int i, j, ip, jp, imax;
    double max, temp;

    for(i = 0; i < J.m; i++) {
        // We are trying eliminate the term in column i, for rows i+1 and
        // greater. First, find a pivot (between rows i and N-1).
        max = 0;
        for(ip = i; ip < J.m; ip++) {
            if(fabs(J.num[ip][i]) > max) {
                imax = ip;
                max = fabs(J.num[ip][i]);
            }
        }
        if(fabs(max) < 1e-12) return false;

        // Swap row imax with row i
        for(jp = 0; jp < J.n; jp++) {
            temp = J.num[i][jp];
            J.num[i][jp] = J.num[imax][jp];
            J.num[imax][jp] = temp;
        }
        temp = J.B.num[i];
        J.B.num[i] = J.B.num[imax];
        J.B.num[imax] = temp;

        // For rows i+1 and greater, eliminate the term in column i.
        for(ip = i+1; ip < J.m; ip++) {
            temp = J.num[ip][i]/J.num[i][i];

            for(jp = 0; jp < J.n; jp++) {
                J.num[ip][jp] -= temp*(J.num[i][jp]);
            }
            J.B.num[ip] -= temp*J.B.num[i];
        }
    }

    // We've put the matrix in upper triangular form, so at this point we
    // can solve by back-substitution.
    for(i = J.m - 1; i >= 0; i--) {
        if(fabs(J.num[i][i]) < 1e-10) return false;

        temp = J.B.num[i];
        for(j = J.n - 1; j > i; j--) {
            temp -= J.X[j]*J.num[i][j];
        }
        J.X[i] = temp / J.num[i][i];
    }

    return true;
}

bool System::NewtonSolve(int tag) {
    WriteJacobian(tag, tag);
    if(J.m != J.n) oops();

    int iter = 0;
    bool converged = false;
    int i;
    do {
        // Evaluate the functions numerically
        for(i = 0; i < J.m; i++) {
            J.B.num[i] = (J.B.sym[i])->Eval();
            dbp("J.B.num[%d] = %.3f", i, J.B.num[i]);
            dbp("J.B.sym[%d] = %s", i, (J.B.sym[i])->Print());
        }
        // And likewise for the Jacobian
        EvalJacobian();

        if(!SolveLinearSystem()) break;

        // Take the Newton step; 
        //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
        for(i = 0; i < J.m; i++) {
            dbp("J.X[%d] = %.3f", i, J.X[i]);
            dbp("modifying param %08x, now %.3f", J.param[i],
                param.FindById(J.param[i])->val);
            (param.FindById(J.param[i]))->val -= J.X[i];
            // XXX do this properly
            SS.GetParam(J.param[i])->val = (param.FindById(J.param[i]))->val;
        }

        // XXX re-evaluate functions before checking convergence
        converged = true;
        for(i = 0; i < J.m; i++) {
            if(!Tol(J.B.num[i])) {
                converged = false;
                break;
            }
        }
    } while(iter++ < 50 && !converged);

    if(converged) {
        return true;
    } else {
        return false;
    }
}

bool System::Solve(void) {
    int i, j;
    dbp("%d equations", eq.n);
    for(i = 0; i < eq.n; i++) {
        dbp("  %s = 0", eq.elem[i].e->Print());
    }

    param.ClearTags();
    eq.ClearTags();
    
    WriteJacobian(0, 0);
    EvalJacobian();

    for(i = 0; i < J.m; i++) {
        for(j = 0; j < J.n; j++) {
            dbp("J[%d][%d] = %.3f", i, j, J.num[i][j]);
        }
    }

    GaussJordan();

    dbp("bound states:");
    for(j = 0; j < J.n; j++) {
        dbp("  param %08x: %d", J.param[j], J.bound[j]);
    }

    // Fix any still-free variables wherever they are now.
    for(j = 0; j < J.n; j++) {
        if(J.bound[j]) continue;
        param.FindById(J.param[j])->tag = ASSUMED;
    }

    NewtonSolve(0);

    return true;
}

