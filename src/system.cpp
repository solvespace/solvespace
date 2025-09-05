//-----------------------------------------------------------------------------
// Once we've written our constraint equations in the symbolic algebra system,
// these routines linearize them, and solve by a modified Newton's method.
// This also contains the routines to detect non-convergence or inconsistency,
// and report diagnostics to the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#include <Eigen/Core>
#include <Eigen/SparseQR>

// The solver will converge all unknowns to within this tolerance. This must
// always be much less than LENGTH_EPS, and in practice should be much less.
const double System::CONVERGE_TOLERANCE = (LENGTH_EPS/(1e2));

constexpr size_t LikelyPartialCountPerEq = 10;

bool System::WriteJacobian(int tag) {
    // Clear all
    mat.param.clear();
    mat.eq.clear();
    mat.A.sym.setZero();
    mat.B.sym.clear();

    for(Equation &e : eq) {
        if(e.tag != tag) continue;
        mat.eq.push_back(&e);
    }
    if(mat.eq.size() >= MAX_UNKNOWNS) {
        return false;
    }
    mat.m = mat.eq.size();

    std::unordered_map<uint32_t, int> paramToIndex;
    for(Param &p : param) {
        if(p.tag != tag) continue;
        // Fill the param id to index map
        paramToIndex[p.h.v] = mat.param.size();
        mat.param.push_back(p.h);
    }
    mat.n = mat.param.size();

    // In some experimenting, this is almost always the right size.
    // Value is usually between 0 and 20, comes from number of constraints?
    mat.A.sym.resize(mat.m, mat.n);
    mat.A.sym.reserve(Eigen::VectorXi::Constant(mat.n, LikelyPartialCountPerEq));

    mat.B.sym.reserve(mat.eq.size());
    for(size_t i = 0; i < mat.eq.size(); i++) {
        Equation *e = mat.eq[i];
        // Deep-copy and simplify (fold) the current equation.
        Expr *f = e->e->DeepCopyWithParamsAsPointers(&param, &(SK.param), /*foldConstants=*/true);

        ParamSet paramsUsed;
        f->ParamsUsedList(&paramsUsed);

        for(hParam p : paramsUsed) {
            // Find the index of this parameter
            auto it = paramToIndex.find(p.v);
            if(it == paramToIndex.end()) continue;
            // this is the parameter index
            const int j = it->second;
            // compute partial derivative of f
            Expr *pd = f->PartialWrt(p);
            pd = pd->FoldConstants(/*allocCopy=*/false);
            if(pd->IsZeroConst())
                continue;
            mat.A.sym.insert(i, j) = pd;
        }
        mat.B.sym.push_back(f);
    }
    return true;
}

void System::EvalJacobian() {
    using namespace Eigen;
    mat.A.num.setZero();
    mat.A.num.resize(mat.m, mat.n);
    const int size = mat.A.sym.outerSize();

    for(int k = 0; k < size; k++) {
        for(SparseMatrix <Expr *>::InnerIterator it(mat.A.sym, k); it; ++it) {
            double value = it.value()->Eval();
            if(EXACT(value == 0.0)) continue;
            mat.A.num.insert(it.row(), it.col()) = value;
        }
    }
    mat.A.num.makeCompressed();
}

bool System::IsDragged(hParam p) {
    return dragged.find(p) != dragged.end();
}

SubstitutionMap System::SolveBySubstitution() {
    // Contains pointers to last substitutions in a substitution chain
    std::vector<Param *> subVec;
    // Maps a parameter to the index of its last substitution in  `subVec`
    std::unordered_map<hParam, size_t, HandleHasher<hParam>> leaves;
    // Tracks how many slots in `subVec` contain a specific last substitution
    // (this can happen as slots that once contained a specific substitution
    //  are updated to point to another over the run of the substitution algorithm)
    std::unordered_map<hParam, std::vector<size_t>, HandleHasher<hParam>> slotTrack;

    for(auto &teq : eq) {
        Expr *tex = teq.e;

        // If we have `(a - b) = 0` where both a and b are parameters, then `a = b` and we can substitute
        if(tex->op    == Expr::Op::MINUS &&
           tex->a->op == Expr::Op::PARAM &&
           tex->b->op == Expr::Op::PARAM)
        {
            Param *sub = param.FindByIdNoOops(tex->a->parh);
            Param *by = param.FindByIdNoOops(tex->b->parh);
            if(!sub || !by) {
                // Don't substitute unless they're both solver params;
                // otherwise it's an equation that can be solved immediately,
                // or an error to flag later.
                continue;
            }

            if(sub->h == by->h) {
                teq.tag = EQ_SUBSTITUTED;
                continue;
            }

            // Take the last substitution of parameter a
            size_t subIdx = 0;
            auto it = leaves.find(sub->h);
            if(it != leaves.end()) {
                subIdx = it->second;
                sub = subVec.at(it->second - 1);
            }

            // Take the last substitution of parameter b
            size_t byIdx = 0;
            it = leaves.find(by->h);
            if(it != leaves.end()) {
                byIdx = it->second;
                by = subVec.at(it->second - 1);
            }

            // If the last substituton of `a` is a dragged param, keep it
            // and substitute the other param
            if(IsDragged(sub->h)) {
                std::swap(sub, by);
                std::swap(subIdx, byIdx);
            }

            if(subIdx == 0) {
                if(byIdx == 0) {
                    // Neither `sub` nor `by` are in the map, so add them and
                    // set the target index
                    subVec.push_back(by);
                    leaves[by->h] = leaves[sub->h] = subVec.size();
                } else {
                    // `sub` isn't in the map, but `by` is, so just add `sub` to
                    // the map with `by` as the target
                    leaves[sub->h] = byIdx;
                }
            } else {
                // `sub` already exists in the map, so just update any slots
                // that point to it as the last substitution to point to `by`
                // instead
                auto it = slotTrack.find(sub->h);
                if(it == slotTrack.end()) {
                    // There's only this one slot, so just replace it with `by`
                    subVec[subIdx - 1] = by;

                    // If `by` was already in the map, that means we now have
                    // an additional slot where it resides, so add it to the
                    // slot tracker
                    if(byIdx != 0) {
                        // If `by` is already in the slot tracker, we'll get
                        // back a vector with at least two elements; otherwise
                        // this access will add a new item to the slot tracker
                        // with an empty vector
                        auto &bySlots = slotTrack[by->h];
                        if(bySlots.empty()) {
                            bySlots.push_back(byIdx);
                        }
                        bySlots.push_back(subIdx);
                    }
                } else {
                    // We have more than one slot pointing to `sub`, so update
                    // all of the slots to point to `by`
                    for(size_t i : it->second) {
                        subVec[i - 1] = by;
                    }

                    // No more slots are pointing to `sub`, so extract the slot list
                    // and erase `sub` from the tracker
                    auto subSlots = std::move(it->second);
                    slotTrack.erase(it);

                    // Same as above: this access either gives us an existing vector
                    // with at least two elements, or creates an empty vector
                    auto &bySlots = slotTrack[by->h];
                    if(bySlots.empty()) {
                        bySlots = std::move(subSlots);
                        if(byIdx != 0) {
                            bySlots.push_back(byIdx);
                        }
                    } else {
                        bySlots.insert(bySlots.end(), subSlots.begin(), subSlots.end());
                    }
                }

                leaves[by->h] = subIdx;
            }

            sub->tag = VAR_SUBSTITUTED;
            teq.tag = EQ_SUBSTITUTED;
        }
    }

    SubstitutionMap subMap;
    for(auto &sub : leaves) {
        Param *by = subVec[sub.second - 1];
        if(sub.first != by->h) {
            subMap[sub.first] = by;
        }
    }

    // Substitute all the equations
    for(auto &req : eq) {
        req.e->Substitute(subMap);
    }

    return subMap;
}

//-----------------------------------------------------------------------------
// Calculate the rank of the Jacobian matrix
//-----------------------------------------------------------------------------
int System::CalculateRank() {
    using namespace Eigen;
    if(mat.n == 0 || mat.m == 0) return 0;
    SparseQR <SparseMatrix<double>, COLAMDOrdering<int>> solver;
    solver.compute(mat.A.num);
    int result = solver.rank();
    return result;
}

bool System::TestRank(int *dof, int *rank) {
    EvalJacobian();
    int jacobianRank = CalculateRank();
    // We are calculating dof based on real rank, not mat.m.
    // Using this approach we can calculate real dof even when redundant is allowed.
    if(dof != NULL) *dof = mat.n - jacobianRank;
    if(rank) {
        *rank = jacobianRank;
    }
    return jacobianRank == mat.m;
}

bool System::SolveLinearSystem(const Eigen::SparseMatrix <double> &A,
                               const Eigen::VectorXd &B, Eigen::VectorXd *X)
{
    if(A.outerSize() == 0) return true;
    using namespace Eigen;
    SparseQR<SparseMatrix<double>, COLAMDOrdering<int>> solver;
    //SimplicialLDLT<SparseMatrix<double>> solver;
    solver.compute(A);
    *X = solver.solve(B);
    return (solver.info() == Success);
}

bool System::SolveLeastSquares() {
    using namespace Eigen;
    // Scale the columns; this scale weights the parameters for the least
    // squares solve, so that we can encourage the solver to make bigger
    // changes in some parameters, and smaller in others.
    VectorXd scale = VectorXd::Ones(mat.n);
    for(int c = 0; c < mat.n; c++) {
        if(IsDragged(mat.param[c])) {
            // It's least squares, so this parameter doesn't need to be all
            // that big to get a large effect.
            scale[c] = 1 / 20.0;
        }
    }

    const int size = mat.A.num.outerSize();
    for(int k = 0; k < size; k++) {
        for(SparseMatrix<double>::InnerIterator it(mat.A.num, k); it; ++it) {
            it.valueRef() *= scale[it.col()];
        }
    }

    SparseMatrix<double> AAt = mat.A.num * mat.A.num.transpose();
    AAt.makeCompressed();
    VectorXd z(mat.n);

    if(!SolveLinearSystem(AAt, mat.B.num, &z)) return false;

    mat.X = mat.A.num.transpose() * z;

    for(int c = 0; c < mat.n; c++) {
        mat.X[c] *= scale[c];
    }
    return true;
}

bool System::NewtonSolve() {

    int iter = 0;
    bool converged = false;
    int i;

    // Evaluate the functions at our operating point.
    mat.B.num = Eigen::VectorXd(mat.m);
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
            Param *p = param.FindById(mat.param[i]);
            p->val -= mat.X[i];
            if(IsReasonable(p->val)) {
                // Very bad, and clearly not convergent
                return false;
            }
        }

        // Re-evalute the functions, since the params have just changed.
        for(i = 0; i < mat.m; i++) {
            mat.B.num[i] = (mat.B.sym[i])->Eval();
            if(IsReasonable(mat.B.num[i])) {
                // Very bad, and clearly not convergent
                return false;
            }
        }
        
        // Check for convergence
        converged = true;
        for(i = 0; i < mat.m; i++) {
            if(fabs(mat.B.num[i]) > CONVERGE_TOLERANCE) {
                converged = false;
                break;
            }
        }
    } while(iter++ < 50 && !converged);

    return converged;
}

void System::WriteEquationsExceptFor(hConstraint hc, Group *g) {
    // Generate all the equations from constraints in this group
    for(auto &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->group != g->h) continue;
        if(c->h == hc) continue;

        if(c->HasLabel() && c->type != Constraint::Type::COMMENT &&
                g->allDimsReference)
        {
            // When all dimensions are reference, we adjust them to display
            // the correct value, and then don't generate any equations.
            c->ModifyToSatisfy();
            continue;
        }
        if(g->relaxConstraints && c->type != Constraint::Type::POINTS_COINCIDENT) {
            // When the constraints are relaxed, we keep only the point-
            // coincident constraints, and the constraints generated by
            // the entities and groups.
            continue;
        }

        c->GenerateEquations(&eq);
    }
    // And the equations from entities
    for(auto &ent : SK.entity) {
        EntityBase *e = &ent;
        if(e->group != g->h) continue;

        e->GenerateEquations(&eq);
    }
    // And from the groups themselves
    g->GenerateEquations(&eq);
}

void System::FindWhichToRemoveToFixJacobian(Group *g, List<hConstraint> *bad, bool forceDofCheck) {
    auto time = GetMilliseconds();
    g->solved.timeout = false;
    int a;

    for(a = 0; a < 2; a++) {
        for(auto &con : SK.constraint) {
            if((GetMilliseconds() - time) > g->solved.findToFixTimeout) {
                g->solved.timeout = true;
                return;
            }

            ConstraintBase *c = &con;
            if(c->group != g->h) continue;
            if((c->type == Constraint::Type::POINTS_COINCIDENT && a == 0) ||
               (c->type != Constraint::Type::POINTS_COINCIDENT && a == 1))
            {
                // Do the constraints in two passes: first everything but
                // the point-coincident constraints, then only those
                // constraints (so they appear last in the list).
                continue;
            }

            param.ClearTags();
            eq.Clear();
            WriteEquationsExceptFor(c->h, g);
            eq.ClearTags();

            // It's a major speedup to solve the easy ones by substitution here,
            // and that doesn't break anything.
            if(!forceDofCheck) {
                SolveBySubstitution();
            }

            WriteJacobian(0);
            EvalJacobian();

            int rank = CalculateRank();
            if(rank == mat.m) {
                // We fixed it by removing this constraint
                bad->Add(&(c->h));
            }
        }
    }
}

SolveResult System::Solve(Group *g, int *dof, List<hConstraint> *bad,
                          bool andFindBad, bool andFindFree, bool forceDofCheck)
{
    WriteEquationsExceptFor(Constraint::NO_CONSTRAINT, g);

    bool rankOk;

    // int x;
    // printf("%d equations", eq.n);
    // for(x = 0; x < eq.n; x++) {
    //     printf("  %.3f = %s = 0", eq[x].e->Eval(), eq[x].e->Print().c_str());
    // }
    // printf("%d parameters", param.n);
    // for(x = 0; x < param.n; x++) {
    //     printf("   param %08x at %.3f", param[x].h.v, param[x].val);
    // }

    // All params and equations are assigned to group zero.
    param.ClearTags();
    eq.ClearTags();

    SubstitutionMap subMap;

    // Since we are suppressing dof calculation or allowing redundant, we
    // can't / don't want to catch result of dof checking without substitution
    if(g->suppressDofCalculation || g->allowRedundant || !forceDofCheck) {
        subMap = SolveBySubstitution();
    }

    // Before solving the big system, see if we can find any equations that
    // are soluble alone. This can be a huge speedup. We don't know whether
    // the system is consistent yet, but if it isn't then we'll catch that
    // later.
    int alone = 1;
    for(auto &e : eq) {
        if(e.tag != 0)
            continue;

        hParam hp = e.e->ReferencedParams(&param);
        if(hp == Expr::NO_PARAMS) continue;
        if(hp == Expr::MULTIPLE_PARAMS) continue;

        Param *p = param.FindById(hp);
        if(p->tag != 0) continue; // let rank test catch inconsistency

        e.tag  = alone;
        p->tag = alone;
        WriteJacobian(alone);
        if(!NewtonSolve()) {
            // We don't do the rank test, so let's arbitrarily return
            // the DIDNT_CONVERGE result here.
            rankOk = true;
            // Failed to converge, bail out early
            goto didnt_converge;
        }
        alone++;
    }

    // Now write the Jacobian for what's left, and do a rank test; that
    // tells us if the system is inconsistently constrained.
    if(!WriteJacobian(0)) {
        return SolveResult::TOO_MANY_UNKNOWNS;
    }
    // Clear dof value in order to have indication when dof is actually not calculated
    if(dof != NULL) *dof = -1;
    // We are suppressing or allowing redundant, so we no need to catch unsolveable + redundant
    rankOk = (!g->suppressDofCalculation && !g->allowRedundant) ? TestRank(dof) : true;

    // And do the leftovers as one big system
    if(!NewtonSolve()) {
        goto didnt_converge;
    }

    // Here we are want to calculate dof even when redundant is allowed, so just handle suppressing
    rankOk = (!g->suppressDofCalculation) ? TestRank(dof) : true;
    if(!rankOk) {
        if(andFindBad) FindWhichToRemoveToFixJacobian(g, bad, forceDofCheck);
    } else {
        MarkParamsFree(andFindFree);
    }
    // System solved correctly, so write the new values back in to the
    // main parameter table.
    for(auto &p : param) {
        auto it = subMap.find(p.h);
        double val = it == subMap.end() ? p.val : it->second->val;

        Param *pp = SK.GetParam(p.h);
        pp->val = val;
        pp->known = true;
        pp->free  = p.free;
    }
    return rankOk ? SolveResult::OKAY : SolveResult::REDUNDANT_OKAY;

didnt_converge:
    SK.constraint.ClearTags();
    // Not using range-for here because index is used in additional ways
    for(size_t i = 0; i < mat.eq.size(); i++) {
        if(fabs(mat.B.num[i]) > CONVERGE_TOLERANCE || IsReasonable(mat.B.num[i])) {
            // This constraint is unsatisfied.
            if(!mat.eq[i]->h.isFromConstraint()) continue;

            hConstraint hc = mat.eq[i]->h.constraint();
            ConstraintBase *c = SK.constraint.FindByIdNoOops(hc);
            if(!c) continue;
            // Don't double-show constraints that generated multiple
            // unsatisfied equations
            if(!c->tag) {
                bad->Add(&(c->h));
                c->tag = 1;
            }
        }
    }

    return rankOk ? SolveResult::DIDNT_CONVERGE : SolveResult::REDUNDANT_DIDNT_CONVERGE;
}

SolveResult System::SolveRank(Group *g, int *rank, int *dof, List<hConstraint> *bad,
                              bool andFindBad, bool andFindFree)
{
    WriteEquationsExceptFor(Constraint::NO_CONSTRAINT, g);

    // All params and equations are assigned to group zero.
    param.ClearTags();
    eq.ClearTags();

    // Now write the Jacobian, and do a rank test; that
    // tells us if the system is inconsistently constrained.
    if(!WriteJacobian(0)) {
        return SolveResult::TOO_MANY_UNKNOWNS;
    }

    bool rankOk = TestRank(dof, rank);
    if(!rankOk) {
        // When we are testing with redundant allowed, we don't want to have additional info
        // about redundants since this test is working only for single redundant constraint
        if(!g->suppressDofCalculation && !g->allowRedundant) {
            if(andFindBad) FindWhichToRemoveToFixJacobian(g, bad, true);
        }
    } else {
        MarkParamsFree(andFindFree);
    }
    return rankOk ? SolveResult::OKAY : SolveResult::REDUNDANT_OKAY;
}

void System::Clear() {
    entity.Clear();
    param.Clear();
    eq.Clear();
    dragged.clear();
    mat.A.num.setZero();
    mat.A.sym.setZero();
}

void System::MarkParamsFree(bool find) {
    // If requested, find all the free (unbound) variables. This might be
    // more than the number of degrees of freedom. Don't always do this,
    // because the display would get annoying and it's slow.
    for(auto &p : param) {
        p.free = false;

        if(find) {
            if(p.tag == 0) {
                p.tag = VAR_DOF_TEST;
                WriteJacobian(0);
                EvalJacobian();
                int rank = CalculateRank();
                if(rank == mat.m) {
                    p.free = true;
                }
                p.tag = 0;
            }
        }
    }
}

