#include "solversystem.h"

System sys;

SolverResult SolverSystem::Solve(GroupBase *g) {
    sys.Clear();

    for(EntityBase &ent : SK.entity) {
        EntityBase *e = &ent;
        if (e->group != g->h) {
            continue;
        }
        for (hParam &parh : e->GetParams()) {
            Param *p = SK.GetParam(parh);
            p->known = false;
            sys.param.Add(p);
        }
    }

    IdList<Param, hParam> constraintParams = {};
    for(ConstraintBase &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->group != g->h)
            continue;
        c->Generate(&constraintParams);
        if(!constraintParams.IsEmpty()) {
            for(Param &p : constraintParams) {
                p.h    = SK.param.AddAndAssignId(&p);
                c->valP = p.h;
                sys.param.Add(&p);
            }
            constraintParams.Clear();
            c->ModifyToSatisfy();
        }
    }

    for(ConstraintBase &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->type == ConstraintBase::Type::WHERE_DRAGGED) {
            EntityBase *e = SK.GetEntity(c->ptA);
            sys.dragged.Add(&(e->param[0]));
            sys.dragged.Add(&(e->param[1]));
            if (e->type == EntityBase::Type::POINT_IN_3D) {
                sys.dragged.Add(&(e->param[2]));
            }
        }
    }

    for(hParam &par : sys.dragged) {
        std::cout << "DraggedParam( h:" << par.v << " )\n";
    }

    for(Param &par : sys.param) {
        std::cout << "Param( " << par.ToString() << " )\n";
    }

    for(EntityBase &ent : SK.entity) {
        std::cout << "EntityBase( " << ent.ToString() << " )\n";
    }

    for(ConstraintBase &con : SK.constraint) {
        std::cout << "ConstraintBase( " << con.ToString() << " )\n";
    }

    int rank = 0;
    int dof = 0;
    List<hConstraint> badList;
    SolveResult status = sys.Solve(g, &rank, &dof, &badList, false, false, false);

    std::vector<hConstraint> bad;
    for (hConstraint &b : badList) {
        bad.push_back(b);
    }

    SolverResult result;
    result.status = status;
    result.bad = bad;
    result.rank = rank;
    result.dof = dof;

    return result;
}