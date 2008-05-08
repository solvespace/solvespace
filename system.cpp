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

        mat.eq[i] = e->h;
        Expr *f = e->e->DeepCopyWithParamsAsPointers(&param, &(SS.param));
        f = f->FoldConstants();

        // Hash table (31 bits) to accelerate generation of zero partials.
        DWORD scoreboard = f->ParamsUsed();
        for(j = 0; j < mat.n; j++) {
            Expr *pd;
            if(scoreboard & (1 << (mat.param[j].v % 31))) { 
                pd = f->PartialWrt(mat.param[j]);
                pd = pd->FoldConstants();
            } else {
                pd = Expr::FromConstant(0);
            }
            mat.A.sym[i][j] = pd;
        }
        mat.B.sym[i] = f;
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

bool System::IsDragged(hParam p) {
    if(SS.GW.pending.point.v) {
        // The pending point could be one in a group that has not yet
        // been processed, in which case the lookup will fail; but
        // that's not an error.
        Entity *pt = SS.entity.FindByIdNoOops(SS.GW.pending.point);
        if(pt) {
            switch(pt->type) {
                case Entity::POINT_IN_3D:
                    if(p.v == (pt->param[0]).v) return true;
                    if(p.v == (pt->param[1]).v) return true;
                    if(p.v == (pt->param[2]).v) return true;
                    break;

                case Entity::POINT_IN_2D:
                case Entity::POINT_XFRMD:
                    if(p.v == (pt->param[0]).v) return true;
                    if(p.v == (pt->param[1]).v) return true;
                    break;
            }
        }
    }
    if(SS.GW.pending.circle.v) {
        Entity *circ = SS.entity.FindByIdNoOops(SS.GW.pending.circle);
        if(circ) {
            Entity *dist = SS.GetEntity(circ->distance);
            switch(dist->type) {
                case Entity::DISTANCE:
                    if(p.v == (dist->param[0].v)) return true;
                    break;
            }
        }
    }
    return false;
}

void System::SolveBySubstitution(void) {
    int i;
    for(i = 0; i < eq.n; i++) {
        Equation *teq = &(eq.elem[i]);
        Expr *tex = teq->e;

        if(tex->op    == Expr::MINUS &&
           tex->a->op == Expr::PARAM &&
           tex->b->op == Expr::PARAM)
        {
            hParam a = (tex->a)->x.parh;
            hParam b = (tex->b)->x.parh;
            if(!(param.FindByIdNoOops(a) && param.FindByIdNoOops(b))) {
                // Don't substitute unless they're both solver params;
                // otherwise it's an equation that can be solved immediately,
                // or an error to flag later.
                continue;
            }

            if(IsDragged(a)) {
                // A is being dragged, so A should stay, and B should go
                hParam t = a;
                a = b;
                b = t;
            }

            int j;
            for(j = 0; j < eq.n; j++) {
                Equation *req = &(eq.elem[j]);
                (req->e)->Substitute(a, b); // A becomes B, B unchanged
            }
            for(j = 0; j < param.n; j++) {
                Param *rp = &(param.elem[j]);
                if(rp->substd.v == a.v) {
                    rp->substd = b;
                }
            }
            Param *ptr = param.FindById(a);
            ptr->tag = VAR_SUBSTITUTED;
            ptr->substd = b;

            teq->tag = EQ_SUBSTITUTED;
        }
    }
}

static int BySensitivity(const void *va, const void *vb) {
    const int *a = (const int *)va, *b = (const int *)vb;

    hParam pa = SS.sys.mat.param[*a];
    hParam pb = SS.sys.mat.param[*b];
    
    if(System::IsDragged(pa) && !System::IsDragged(pb)) return  1;
    if(System::IsDragged(pb) && !System::IsDragged(pa)) return -1;

    double as = SS.sys.mat.sens[*a];
    double bs = SS.sys.mat.sens[*b];

    if(as < bs) return  1;
    if(as > bs) return -1;
    return 0;
}
void System::SortBySensitivity(void) {
    // For each unknown, sum up the sensitivities in that column of the
    // Jacobian
    int i, j;
    for(j = 0; j < mat.n; j++) {
        double s = 0;
        int i;
        for(i = 0; i < mat.m; i++) {
            s += fabs(mat.A.num[i][j]);
        }
        mat.sens[j] = s;
    }
    for(j = 0; j < mat.n; j++) {
        mat.permutation[j] = j;
    }

    qsort(mat.permutation, mat.n, sizeof(mat.permutation[0]), BySensitivity);

    int origPos[MAX_UNKNOWNS];
    int entryWithOrigPos[MAX_UNKNOWNS];
    for(j = 0; j < mat.n; j++) {
        origPos[j] = j;
        entryWithOrigPos[j] = j;
    }

#define SWAP(T, a, b) do { T temp = (a); (a) = (b); (b) = temp; } while(0)
    for(j = 0; j < mat.n; j++) {
        int dest = j; // we are writing to position j
        // And the source is whichever position ahead of us can be swapped
        // in to make the permutation vectors line up.
        int src = entryWithOrigPos[mat.permutation[j]];

        for(i = 0; i < mat.m; i++) {
            SWAP(double, mat.A.num[i][src], mat.A.num[i][dest]);
            SWAP(Expr *, mat.A.sym[i][src], mat.A.sym[i][dest]);
        }
        SWAP(hParam, mat.param[src], mat.param[dest]);

        SWAP(int, origPos[src], origPos[dest]);
        if(mat.permutation[dest] != origPos[dest]) oops();

        // Update the table; only necessary to do this for src, since dest
        // is already done.
        entryWithOrigPos[origPos[src]] = src;
    }
}

bool System::Tol(double v) {
    return (fabs(v) < 0.001);
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

    // Evaluate the functions at our operating point.
    for(i = 0; i < mat.m; i++) {
        mat.B.num[i] = (mat.B.sym[i])->Eval();
    }
    do {
        // And evaluate the Jacobian at our initial operating point.
        EvalJacobian();

        if(!SolveLinearSystem()) break;

        // Take the Newton step; 
        //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
        for(i = 0; i < mat.m; i++) {
            (param.FindById(mat.param[i]))->val -= mat.X[i];
        }

        // Re-evalute the functions, since the params have just changed.
        for(i = 0; i < mat.m; i++) {
            mat.B.num[i] = (mat.B.sym[i])->Eval();
        }
        // Check for convergence
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
        dbp("no convergence");
        return false;
    }
}

bool System::Solve(void) {
    int i, j;
    
/*
    dbp("%d equations", eq.n);
    for(i = 0; i < eq.n; i++) {
        dbp("  %.3f = %s = 0", eq.elem[i].e->Eval(), eq.elem[i].e->Print());
    }
    dbp("%d parameters", param.n);
    for(i = 0; i < param.n; i++) {
        dbp("   param %08x at %.3f", param.elem[i].h.v, param.elem[i].val);
    } */

    param.ClearTags();
    eq.ClearTags();
    
    SolveBySubstitution();

    WriteJacobian(0, 0);

    EvalJacobian();

    SortBySensitivity();

/*    dbp("write/eval jacboian=%d", GetMilliseconds() - in);
    for(i = 0; i < mat.m; i++) {
        dbp("function %d: %s", i, mat.B.sym[i]->Print());
    }
    dbp("m=%d", mat.m);
    for(i = 0; i < mat.m; i++) {
        for(j = 0; j < mat.n; j++) {
            dbp("A[%d][%d] = %.3f", i, j, mat.A.num[i][j]);
        }
    } */

    GaussJordan();

/*    dbp("bound states:");
    for(j = 0; j < mat.n; j++) {
        dbp("  param %08x: %d", mat.param[j], mat.bound[j]);
    } */

    // Fix any still-free variables wherever they are now.
    for(j = mat.n-1; j >= 0; --j) {
        if(mat.bound[j]) continue;
        Param *p = param.FindByIdNoOops(mat.param[j]);
        if(!p) {
            // This is parameter does not occur in this group, so it's
            // not available to assume.
            continue;
        }
        p->tag = VAR_ASSUMED;
    }
    
    bool ok = NewtonSolve(0);

    if(ok) {
        // System solved correctly, so write the new values back in to the
        // main parameter table.
        for(i = 0; i < param.n; i++) {
            Param *p = &(param.elem[i]);
            double val;
            if(p->tag == VAR_SUBSTITUTED) {
                val = param.FindById(p->substd)->val;
            } else {
                val = p->val;
            }

            Param *pp = SS.GetParam(p->h);
            pp->val = val;
            pp->known = true;
            // The main param table keeps track of what was assumed.
            pp->assumed = (p->tag == VAR_ASSUMED);
        }
    }

    return true;
}

