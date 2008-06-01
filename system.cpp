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
                pd = pd->DeepCopyWithParamsAsPointers(&param, &(SS.param));
            } else {
                pd = Expr::From(0.0);
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
                case Entity::POINT_N_TRANS:
                case Entity::POINT_IN_3D:
                    if(p.v == (pt->param[0]).v) return true;
                    if(p.v == (pt->param[1]).v) return true;
                    if(p.v == (pt->param[2]).v) return true;
                    break;

                case Entity::POINT_IN_2D:
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
    if(SS.GW.pending.normal.v) {
        Entity *norm = SS.entity.FindByIdNoOops(SS.GW.pending.normal);
        if(norm) {
            switch(norm->type) {
                case Entity::NORMAL_IN_3D:
                    if(p.v == (norm->param[0].v)) return true;
                    if(p.v == (norm->param[1].v)) return true;
                    if(p.v == (norm->param[2].v)) return true;
                    if(p.v == (norm->param[3].v)) return true;
                    break;
                // other types are locked, so not draggable
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

bool System::Tol(double v) {
    return (fabs(v) < 0.0001);
}

int System::GaussJordan(void) {
    int i, j;

    for(j = 0; j < mat.n; j++) {
        mat.bound[j] = false;
    }

    // Now eliminate.
    i = 0;
    int rank = 0;
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
            rank++;

            // Move on to the next row, since we just used this one to
            // eliminate from column j.
            i++;
            if(i >= mat.m) break;
        }
    }
    return rank;
}

bool System::SolveLinearSystem(double X[], double A[][MAX_UNKNOWNS],
                               double B[], int n)
{
    // Gaussian elimination, with partial pivoting. It's an error if the
    // matrix is singular, because that means two constraints are
    // equivalent.
    int i, j, ip, jp, imax;
    double max, temp;

    for(i = 0; i < n; i++) {
        // We are trying eliminate the term in column i, for rows i+1 and
        // greater. First, find a pivot (between rows i and N-1).
        max = 0;
        for(ip = i; ip < n; ip++) {
            if(fabs(A[ip][i]) > max) {
                imax = ip;
                max = fabs(A[ip][i]);
            }
        }
        // Don't give up on a singular matrix unless it's really bad; the
        // assumption code is responsible for identifying that condition,
        // so we're not responsible for reporting that error.
        if(fabs(max) < 1e-20) return false;

        // Swap row imax with row i
        for(jp = 0; jp < n; jp++) {
            SWAP(double, A[i][jp], A[imax][jp]);
        }
        SWAP(double, B[i], B[imax]);

        // For rows i+1 and greater, eliminate the term in column i.
        for(ip = i+1; ip < n; ip++) {
            temp = A[ip][i]/A[i][i];

            for(jp = 0; jp < n; jp++) {
                A[ip][jp] -= temp*(A[i][jp]);
            }
            B[ip] -= temp*B[i];
        }
    }

    // We've put the matrix in upper triangular form, so at this point we
    // can solve by back-substitution.
    for(i = n - 1; i >= 0; i--) {
        if(fabs(A[i][i]) < 1e-20) return false;

        temp = B[i];
        for(j = n - 1; j > i; j--) {
            temp -= X[j]*A[i][j];
        }
        X[i] = temp / A[i][i];
    }

    return true;
}

bool System::SolveLeastSquares(void) {
    int r, c, i;

    // Scale the columns; this scale weights the parameters for the least
    // squares solve, so that we can encourage the solver to make bigger
    // changes in some parameters, and smaller in others.
    for(c = 0; c < mat.n; c++) {
        if(IsDragged(mat.param[c])) {
            mat.scale[c] = 1/5.0;
        } else {
            mat.scale[c] = 1;
        }
        for(r = 0; r < mat.m; r++) {
            mat.A.num[r][c] *= mat.scale[c];
        }
    }

    // Write A*A'
    for(r = 0; r < mat.m; r++) {
        for(c = 0; c < mat.m; c++) {  // yes, AAt is square
            double sum = 0;
            for(i = 0; i < mat.n; i++) {
                sum += mat.A.num[r][i]*mat.A.num[c][i];
            }
            mat.AAt[r][c] = sum;
        }
    }

    if(!SolveLinearSystem(mat.Z, mat.AAt, mat.B.num, mat.m)) return false;

    // And multiply that by A' to get our solution.
    for(c = 0; c < mat.n; c++) {
        double sum = 0;
        for(i = 0; i < mat.m; i++) {
            sum += mat.A.num[i][c]*mat.Z[i];
        }
        mat.X[c] = sum * mat.scale[c];
    }
    return true;
}

bool System::NewtonSolve(int tag) {
    WriteJacobian(tag, tag);
    if(mat.m > mat.n) return false;

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

        if(!SolveLeastSquares()) break;

        // Take the Newton step; 
        //      J(x_n) (x_{n+1} - x_n) = 0 - F(x_n)
        for(i = 0; i < mat.n; i++) {
            (param.FindById(mat.param[i]))->val -= mat.X[i];
        }

        // Re-evalute the functions, since the params have just changed.
        for(i = 0; i < mat.m; i++) {
            mat.B.num[i] = (mat.B.sym[i])->Eval();
        }
        // Check for convergence
        converged = true;
        for(i = 0; i < mat.m; i++) {
            if(fabs(mat.B.num[i]) > 1e-10) {
                converged = false;
                break;
            }
        }
    } while(iter++ < 50 && !converged);

    return converged;
}

void System::WriteEquationsExceptFor(hConstraint hc, hGroup hg) {
    int i;
    // Generate all the equations from constraints in this group
    for(i = 0; i < SS.constraint.n; i++) {
        Constraint *c = &(SS.constraint.elem[i]);
        if(c->group.v != hg.v) continue;
        if(c->h.v == hc.v) continue;

        c->Generate(&eq);
    }
    // And the equations from entities
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->group.v != hg.v) continue;

        e->GenerateEquations(&eq);
    }
    // And from the groups themselves
    (SS.GetGroup(hg))->GenerateEquations(&eq);
}

void System::FindWhichToRemoveToFixJacobian(Group *g) {
    int i;
    (g->solved.remove).Clear();

    for(i = 0; i < SS.constraint.n; i++) {
        Constraint *c = &(SS.constraint.elem[i]);
        if(c->group.v != g->h.v) continue;

        param.ClearTags();
        eq.Clear();
        WriteEquationsExceptFor(c->h, g->h);
        eq.ClearTags();

        WriteJacobian(0, 0);
        EvalJacobian();

        int rank = GaussJordan();
        if(rank == mat.m) {
            // We fixed it by removing this constraint
            (g->solved.remove).Add(&(c->h));
        }
    }
}

void System::Solve(Group *g) {
    g->solved.remove.Clear();

    WriteEquationsExceptFor(Constraint::NO_CONSTRAINT, g->h);

    int i, j = 0;
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

/*
    for(i = 0; i < mat.m; i++) {
        dbp("function %d: %s", i, mat.B.sym[i]->Print());
    }
    dbp("m=%d", mat.m);
    for(i = 0; i < mat.m; i++) {
        for(j = 0; j < mat.n; j++) {
            dbp("A(%d,%d) = %.10f;", i+1, j+1, mat.A.num[i][j]);
        }
    } */

    int rank = GaussJordan();
    if(rank != mat.m) {
        FindWhichToRemoveToFixJacobian(g);
        g->solved.how = Group::SINGULAR_JACOBIAN;
        TextWindow::ReportHowGroupSolved(g->h);
        return;
    }

/*    dbp("bound states:");
    for(j = 0; j < mat.n; j++) {
        dbp("  param %08x: %d", mat.param[j], mat.bound[j]);
    } */

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
        if(g->solved.how != Group::SOLVED_OKAY) {
            g->solved.how = Group::SOLVED_OKAY;
            TextWindow::ReportHowGroupSolved(g->h);
        }
    } else {
        g->solved.how = Group::DIDNT_CONVERGE;
        TextWindow::ReportHowGroupSolved(g->h);
    }
}

