#include "solvespace.h"

SolveSpace SS;

void SolveSpace::Init(void) {
    constraint.Clear();
    request.Clear();
    group.Clear();

    entity.Clear();
    param.Clear();

    // Our initial group, that contains the references.
    Group g;
    memset(&g, 0, sizeof(g));
    g.name.strcpy("#references");
    g.h = Group::HGROUP_REFERENCES;
    group.Add(&g);

    // And an empty group, for the first stuff the user draws.
    g.name.strcpy("");
    group.AddAndAssignId(&g);
    

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r;
    memset(&r, 0, sizeof(r));
    r.type = Request::CSYS_2D;
    r.group = Group::HGROUP_REFERENCES;
    r.csys = Entity::NO_CSYS;

    r.name.strcpy("#XY-csys");
    r.h = Request::HREQUEST_REFERENCE_XY;
    request.Add(&r);

    r.name.strcpy("#YZ-csys");
    r.h = Request::HREQUEST_REFERENCE_YZ;
    request.Add(&r);

    r.name.strcpy("#ZX-csys");
    r.h = Request::HREQUEST_REFERENCE_ZX;
    request.Add(&r);

    TW.Init();
    GW.Init();

    TW.Show();
    GenerateAll();
}

void SolveSpace::GenerateAll(void) {
    int i;

    IdList<Param,hParam> prev;
    param.MoveSelfInto(&prev);

    entity.Clear();
    for(i = 0; i < request.n; i++) {
        request.elem[i].Generate(&entity, &param);
    }

    for(i = 0; i < param.n; i++) {
        Param *p = prev.FindByIdNoOops(param.elem[i].h);
        if(p) {
            param.elem[i].val = p->val;
        }
    }

    prev.Clear();
    ForceReferences();
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
        GetEntity(hr.entity(1))->PointForceTo(v);
        // The quaternion that defines the rotation, from the table.
        GetParam(hr.param(0))->val = Quat[i].a;
        GetParam(hr.param(1))->val = Quat[i].b;
        GetParam(hr.param(2))->val = Quat[i].c;
        GetParam(hr.param(3))->val = Quat[i].d;
    }
}

bool SolveSpace::SolveGroup(hGroup hg) {
    int i;
    if(hg.v == Group::HGROUP_REFERENCES.v) {
        // Special case; mark everything in the references known.
        for(i = 0; i < param.n; i++) {
            Param *p = &(param.elem[i]);
            Request *r = GetRequest(p->h.request());
            if(r->group.v == hg.v) p->known = true;
        }
        return true;
    }

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
    FreeAllExprs();
    return r;
}

bool SolveSpace::SolveWorker(int order) {
    bool allSolved = true;

    int i;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->solved) continue;

        allSolved = false;
        dbp("try solve group %s", g->DescriptionString());

        // Save the parameter table; a failed solve attempt will mess that
        // up a little bit.
        IdList<Param,hParam> savedParam;
        param.DeepCopyInto(&savedParam);

        if(SolveGroup(g->h)) {
            g->solved = true;
            g->solveOrder = order;
            // So this one worked; let's see if we can go any further.
            if(SolveWorker(order+1)) {
                // So everything worked; we're done.
                return true;
            }
        }
        // Didn't work, so undo this choice and give up
        g->solved = false;
        param.Clear();
        savedParam.MoveSelfInto(&param);
    }

    // If we got here, then either everything failed, so we're stuck, or
    // everything was already solved, so we're done.
    return allSolved;
}

void SolveSpace::Solve(void) {
    int i;
    for(i = 0; i < group.n; i++) {
        group.elem[i].solved = false;
    }
    SolveWorker(0);

    InvalidateGraphics();
}

void SolveSpace::MenuFile(int id) {
    switch(id) {
        case GraphicsWindow::MNU_NEW:
        case GraphicsWindow::MNU_OPEN:

        case GraphicsWindow::MNU_SAVE:
            SS.SaveToFile("t.slv");
            break;

        case GraphicsWindow::MNU_SAVE_AS:
            break;

        case GraphicsWindow::MNU_EXIT:
            break;

        default: oops();
    }
}
