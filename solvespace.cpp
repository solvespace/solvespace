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
        GetEntity(hr.entity(1))->ForcePointTo(v);
        // The quaternion that defines the rotation, from the table.
        GetParam(hr.param(0))->ForceTo(Quat[i].a);
        GetParam(hr.param(1))->ForceTo(Quat[i].b);
        GetParam(hr.param(2))->ForceTo(Quat[i].c);
        GetParam(hr.param(3))->ForceTo(Quat[i].d);
    }
}

void SolveSpace::Solve(void) {
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
