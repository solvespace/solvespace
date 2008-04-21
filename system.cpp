#include "solvespace.h"

void System::WriteJacobian(int eqTag, int paramTag) {
    int a, i, j;

    j = 0;
    for(a = 0; a < param.n; a++) {
        Param *p = &(param.elem[a]);
        if(p->tag != paramTag) continue;
        mat.param[j] = p->h;
        j++;
    }
    mat.n = j;

    i = 0;
    for(a = 0; a < eq.n; a++) {
        Equation *e = &(eq.elem[a]);
        if(e->tag != eqTag) continue;

        mat.eq[i] = eq.elem[i].h;
        mat.B.sym[i] = eq.elem[i].e;
        for(j = 0; j < mat.n; j++) {
            mat.A.sym[i][j] = e->e->PartialWrt(mat.param[j]);
        }
        i++;
    }
    mat.m = i;
}

void System::EvalJacobian(void) {
    int i, j;
    for(i = 0; i < mat.m; i++) {
        for(j = 0; j < mat.n; j++) {
            mat.A.num[i][j] = (mat.A.sym[i][j])->Eval();
        }
    }
}

bool System::Tol(double v) {
    return (fabs(v) < 0.01);
}

void System::GaussJordan(void) {
    int i, j;

    for(j = 0; j < mat.n; j++) {
        mat.bound[j] = false;
    }

    // Now eliminate.
    i = 0;
    for(j = 0; j < mat.n; j++) {
        // First, seek a pivot in our column.
        int ip, imax;
        double max = 0;
        for(ip = i; ip < mat.m; ip++) {
            double v = fabs(mat.A.num[ip][j]);
            if(v > max) {
                imax = ip;
                max = v;
            }
        }
        if(!Tol(max)) {
            // There's a usable pivot in this column. Swap it in:
            int js;
            for(js = j; js < mat.n; js++) {
                double temp;
                temp = mat.A.num[imax][js];
                mat.A.num[imax][js] = mat.A.num[i][js];
                mat.A.num[i][js] = temp;
            }

            // Get a 1 as the leading entry in the row we're working on.
            double v = mat.A.num[i][j];
            for(js = 0; js < mat.n; js++) {
                mat.A.num[i][js] /= v;
            }

            // Eliminate this column from rows except this one.
            int is;
            for(is = 0; is < mat.m; is++) {
                if(is == i) continue;

                // We're trying to drive A[is][j] to zero. We know
                // that A[i][j] is 1, so we want to subtract off
                // A[is][j] times our present row.
                double v = mat.A.num[is][j];
                for(js = 0; js < mat.n; js++) {
                    mat.A.num[is][js] -= v*mat.A.num[i][js];
                }
                mat.A.num[is][j] = 0;
            }

            // And mark this as a bound variable.
            mat.bound[j] = true;

            // Move on to the next row, since we just used this one to
            // eliminate from column j.
            i++;
            if(i >= mat.m) break;
        }
    }
}

bool System::SolveLinearSystem(void) {
    if(mat.m != mat.n) oops();
    // Gaussian elimination, with partial pivoting. It's an error if the
    // matrix is singular, because that means two constraints are
    // equivalent.
    int i, j, ip, jp, imax;
    double max, temp;

    for(i = 0; i < mat.m; i++) {
        // We are trying eliminate the term in column i, for rows i+1 and
        // greater. First, find a pivot (between rows i and N-1).
        max = 0;
        for(ip = i; ip < mat.m; ip++) {
            if(fabs(mat.A.num[ip][i]) > max) {
                imax = ip;
                max = fabs(mat.A.num[ip][i]);
            }
        }
        if(fabs(max) < 1e-12) return false;

        // Swap row imax with row i
        for(jp = 0; jp < mat.n; jp++) {
            temp = mat.A.num[i][jp];
            mat.A.num[i][jp] = mat.A.num[imax][jp];
            mat.A.num[imax][jp] = temp;
        }
        temp = mat.B.num[i];
        mat.B.num[i] = mat.B.num[imax];
        mat.B.num[imax] = temp;

        // For rows i+1 and greater, eliminate the term in column i.
        for(ip = i+1; ip < mat.m; ip++) {
            temp = mat.A.num[ip][i]/mat.A.num[i][i];

            for(jp = 0; jp < mat.n; jp++) {
                mat.A.num[ip][jp] -= temp*(mat.A.num[i][jp]);
            }
            mat.B.num[ip] -= temp*mat.B.num[i];
        }
    }

    // We've put the matrix in upper triangular form, so at this point we
    // can solve by back-substitution.
    for(i = mat.m - 1; i >= 0; i--) {
        if(fabs(mat.A.num[i][i]) < 1e-10) return false;

        temp = mat.B.num[i];
        for(j = mat.n - 1; j > i; j--) {
            temp -= mat.X[j]*mat.A.num[i][j];
        }
        mat.X[i] = temp / mat.A.num[i][i];
    }

    return true;
}

bool System::NewtonSolve(int tag) {
    WriteJacobian(tag, tag);
    if(mat.m != mat.n) oops();

    int iter = 0;
    bool converged = false;
    int i;
    do {
        // Evaluate the functions numerically
        for(i = 0; i < mat.m; i++) {
            mat.B.num[i] = (mat.B.sym[i])->Eval();
            dbp("mat.B.num[%d] = %.3f", i, mat.B.num[i]);
            dbp("mat.B.sym[%d] = %s", i, (mat.B.sym[i])->Print());
        }
        // And likewise for the Jacobian
        EvalJacobian();

        if(!SolveLinearSystem()) break;

        // Take the Newton step; 
        //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
        for(i = 0; i < mat.m; i++) {
            dbp("mat.X[%d] = %.3f", i, mat.X[i]);
            dbp("modifying param %08x, now %.3f", mat.param[i],
                param.FindById(mat.param[i])->val);
            (param.FindById(mat.param[i]))->val -= mat.X[i];
            // XXX do this properly
            SS.GetParam(mat.param[i])->val =
                (param.FindById(mat.param[i]))->val;
        }

        // XXX re-evaluate functions before checking convergence
        converged = true;
        for(i = 0; i < mat.m; i++) {
            if(!Tol(mat.B.num[i])) {
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

    for(i = 0; i < mat.m; i++) {
        for(j = 0; j < mat.n; j++) {
            dbp("A[%d][%d] = %.3f", i, j, mat.A.num[i][j]);
        }
    }

    GaussJordan();

    dbp("bound states:");
    for(j = 0; j < mat.n; j++) {
        dbp("  param %08x: %d", mat.param[j], mat.bound[j]);
    }

    // Fix any still-free variables wherever they are now.
    for(j = 0; j < mat.n; j++) {
        if(mat.bound[j]) continue;
        param.FindById(mat.param[j])->tag = ASSUMED;
    }

    NewtonSolve(0);

    return true;
}

