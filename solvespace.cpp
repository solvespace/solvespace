#include "solvespace.h"

SolveSpace SS;

void SolveSpace::Init(char *cmdLine) {
    if(strlen(cmdLine) == 0) {
        NewFile();
    } else {
        LoadFromFile(cmdLine);
    }

    TW.Init();
    GW.Init();

    GenerateAll(false);

    TW.Show();
}

void SolveSpace::GenerateAll(bool andSolve) {
    int i, j;

    // Don't lose our numerical guesses when we regenerate.
    IdList<Param,hParam> prev;
    param.MoveSelfInto(&prev);
    entity.Clear();

    for(i = 0; i < group.n; i++) {
        group.elem[i].solved = false;
    }

    // For now, solve the groups in given order; should discover the
    // correct order later.
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);

        for(j = 0; j < request.n; j++) {
            Request *r = &(request.elem[j]);
            if(r->group.v != g->h.v) continue;

            r->Generate(&entity, &param);
        }

        g->Generate(&entity, &param);

        // Use the previous values for params that we've seen before, as
        // initial guesses for the solver.
        for(j = 0; j < param.n; j++) {
            Param *newp = &(param.elem[j]);
            if(newp->known) continue;

            Param *prevp = prev.FindByIdNoOops(newp->h);
            if(prevp) newp->val = prevp->val;
        }

        if(g->h.v == Group::HGROUP_REFERENCES.v) {
            ForceReferences();
            group.elem[0].solved = true;
        } else {
            // Solve this group.
            if(andSolve) SolveGroup(g->h);
        }
    }

    prev.Clear();
    InvalidateGraphics();
}

void SolveSpace::ForceReferences(void) {
    // Force the values of the paramters that define the three reference
    // coordinate systems.
    static const struct {
        hRequest hr;
        double a, b, c, d;
    } Quat[] = {
        { Request::HREQUEST_REFERENCE_XY, 1,    0,    0,    0, },
        { Request::HREQUEST_REFERENCE_YZ, 0.5,  0.5,  0.5,  0.5, },
        { Request::HREQUEST_REFERENCE_ZX, 0.5, -0.5, -0.5, -0.5, },
    };
    for(int i = 0; i < 3; i++) {
        hRequest hr = Quat[i].hr;
        // The origin for our coordinate system, always zero
        Vector v = Vector::MakeFrom(0, 0, 0);
        Entity *origin = GetEntity(hr.entity(1));
        origin->PointForceTo(v);
        GetParam(origin->param[0])->known = true;
        GetParam(origin->param[1])->known = true;
        GetParam(origin->param[2])->known = true;
        // The quaternion that defines the rotation, from the table.
        Param *p;
        p = GetParam(hr.param(0)); p->val = Quat[i].a; p->known = true;
        p = GetParam(hr.param(1)); p->val = Quat[i].b; p->known = true;
        p = GetParam(hr.param(2)); p->val = Quat[i].c; p->known = true;
        p = GetParam(hr.param(3)); p->val = Quat[i].d; p->known = true;
    }
}

bool SolveSpace::SolveGroup(hGroup hg) {
    int i;
    // Clear out the system to be solved.
    sys.entity.Clear();
    sys.param.Clear();
    sys.eq.Clear();
    // And generate all the params for requests in this group
    for(i = 0; i < request.n; i++) {
        Request *r = &(request.elem[i]);
        if(r->group.v != hg.v) continue;

        r->Generate(&(sys.entity), &(sys.param));
    }
    // And for the group itself
    Group *g = SS.GetGroup(hg);
    g->Generate(&(sys.entity), &(sys.param));
    // Set the initial guesses for all the params
    for(i = 0; i < sys.param.n; i++) {
        Param *p = &(sys.param.elem[i]);
        p->known = false;
        p->val = GetParam(p->h)->val;
    }
    // And generate all the equations from constraints in this group
    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(c->group.v != hg.v) continue;

        c->Generate(&(sys.eq));
    }

    bool r = sys.Solve();
    FreeAllTemporary();
    return r;
}

void SolveSpace::MenuFile(int id) {
    switch(id) {
        case GraphicsWindow::MNU_NEW:
            SS.NewFile();
            SS.GenerateAll(false);
            SS.GW.Init();
            SS.TW.Init();
            SS.TW.Show();
            break;

        case GraphicsWindow::MNU_OPEN:
            break;

        case GraphicsWindow::MNU_SAVE:
            SS.SaveToFile("t.slvs");
            break;

        case GraphicsWindow::MNU_SAVE_AS:
            break;

        case GraphicsWindow::MNU_EXIT:
            break;

        default: oops();
    }
}
