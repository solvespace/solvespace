#include "solvespace.h"

bool SolveSpace::SaveToFile(char *filename) {
    fh = fopen(filename, "w");
    if(!fh) {   
        Error("Couldn't write to file '%s'", fh);
        return false;
    }

    fprintf(fh, "!!SolveSpaceREVa\n\n\n");

    int i;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        fprintf(fh, "Group.h.v=%08x\n", g->h.v);
        fprintf(fh, "Group.name=%s\n", g->name.str);
        fprintf(fh, "AddGroup\n\n");
    }

    for(i = 0; i < param.n; i++) {
        Param *p = &(param.elem[i]);
        fprintf(fh, "Param.h.v=%08x\n", p->h.v);
        fprintf(fh, "Param.val=%.20f\n", p->val);
        fprintf(fh, "AddParam\n\n");
    }

    for(i = 0; i < request.n; i++) {
        Request *r = &(request.elem[i]);
        fprintf(fh, "Request.h.v=%08x\n", r->h.v);
        fprintf(fh, "Request.type=%d\n", r->type);
        fprintf(fh, "Request.csys.v=%08x\n", r->csys.v);
        fprintf(fh, "Request.group.v=%08x\n", r->group.v);
        fprintf(fh, "Request.name=%s\n", r->name.str);
        fprintf(fh, "AddRequest\n\n");
    }

    for(i = 0; i < entity.n; i++) {
        Entity *e = &(entity.elem[i]);
        fprintf(fh, "Entity.h.v=%08x\n", e->h.v);
        fprintf(fh, "Entity.type=%d\n", e->type);
        fprintf(fh, "AddEntity\n\n");
    }

    for(i = 0; i < point.n; i++) {
        Point *p = &(point.elem[i]);
        fprintf(fh, "Point.h.v=%08x\n", p->h.v);
        fprintf(fh, "Point.type=%d\n", p->type);
        fprintf(fh, "Point.csys.v=%08x\n", p->csys.v);
        fprintf(fh, "AddPoint\n\n");
    }

    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        fprintf(fh, "Constraint.h.v=%08x\n", c->h.v);
        fprintf(fh, "Constraint.type=%d\n", c->type);
        fprintf(fh, "Constraint.group.v=%08x\n", c->group);
        fprintf(fh, "Constraint.exprA=%s\n", c->exprA->Print());
        fprintf(fh, "Constraint.exprB=%s\n", c->exprB->Print());
        fprintf(fh, "Constraint.ptA.v=%08x\n", c->ptA.v);
        fprintf(fh, "Constraint.ptB.v=%08x\n", c->ptB.v);
        fprintf(fh, "Constraint.ptC.v=%08x\n", c->ptC.v);
        fprintf(fh, "Constraint.entityA.v=%08x\n", c->entityA.v);
        fprintf(fh, "Constraint.entityB.v=%08x\n", c->entityB.v);
        fprintf(fh, "Constraint.disp.offset.x=%.20f\n", c->disp.offset.x);
        fprintf(fh, "Constraint.disp.offset.y=%.20f\n", c->disp.offset.y);
        fprintf(fh, "Constraint.disp.offset.z=%.20f\n", c->disp.offset.z);
        fprintf(fh, "AddConstraint\n\n");
    }

    fclose(fh);

    return true;
}

bool SolveSpace::LoadFromFile(char *filename) {
    fh = fopen(filename, "r");
    if(!fh) {   
        Error("Couldn't read from file '%s'", fh);
        return false;
    }

    return true;
}

