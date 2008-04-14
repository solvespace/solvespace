#include "solvespace.h"

hConstraint Constraint::AddConstraint(Constraint *c) {
    SS.constraint.AddAndAssignId(c);
    return c->h;
}

void Constraint::MenuConstrain(int id) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_DISTANCE_DIA:
            if(gs.points == 2 && gs.n == 2) {
                c.type = PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                c.type = PT_PT_DISTANCE;
                c.ptA = gs.entity[0].point(16);
                c.ptB = gs.entity[0].point(16+3);
            } else {
                Error("Bad selection for distance / diameter constraint.");
                return;
            }
            c.disp.offset = Vector::MakeFrom(50, 50, 50);
            AddConstraint(&c);
            break;

        default: oops();
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
}
