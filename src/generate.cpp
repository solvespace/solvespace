//-----------------------------------------------------------------------------
// Generate our model based on its parametric description, by solving each
// sketch, generating surfaces from the resulting entities, performing any
// requested surface operations (e.g. Booleans) with our model so far, and
// then repeating this process for each subsequent group.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpaceUI::MarkGroupDirtyByEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    MarkGroupDirty(e->group);
}

void SolveSpaceUI::MarkGroupDirty(hGroup hg) {
    int i;
    bool go = false;
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        if(g->h.v == hg.v) {
            go = true;
        }
        if(go) {
            g->clean = false;
        }
    }
    unsaved = true;
}

bool SolveSpaceUI::PruneOrphans(void) {
    int i;
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(GroupExists(r->group)) continue;

        (deleted.requests)++;
        SK.request.RemoveById(r->h);
        return true;
    }

    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *c = &(SK.constraint.elem[i]);
        if(GroupExists(c->group)) continue;

        (deleted.constraints)++;
        (deleted.nonTrivialConstraints)++;

        SK.constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

bool SolveSpaceUI::GroupsInOrder(hGroup before, hGroup after) {
    if(before.v == 0) return true;
    if(after.v  == 0) return true;

    int beforep = -1, afterp = -1;
    int i;
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        if(g->h.v == before.v) beforep = i;
        if(g->h.v == after.v)  afterp  = i;
    }
    if(beforep < 0 || afterp < 0) return false;
    if(beforep >= afterp) return false;
    return true;
}

bool SolveSpaceUI::GroupExists(hGroup hg) {
    // A nonexistent group is not acceptable
    return SK.group.FindByIdNoOops(hg) ? true : false;
}
bool SolveSpaceUI::EntityExists(hEntity he) {
    // A nonexstient entity is acceptable, though, usually just means it
    // doesn't apply.
    if(he.v == Entity::NO_ENTITY.v) return true;
    return SK.entity.FindByIdNoOops(he) ? true : false;
}

bool SolveSpaceUI::PruneGroups(hGroup hg) {
    Group *g = SK.GetGroup(hg);
    if(GroupsInOrder(g->opA, hg) &&
       EntityExists(g->predef.origin) &&
       EntityExists(g->predef.entityB) &&
       EntityExists(g->predef.entityC))
    {
        return false;
    }
    (deleted.groups)++;
    SK.group.RemoveById(g->h);
    return true;
}

bool SolveSpaceUI::PruneRequests(hGroup hg) {
    int i;
    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(e->group.v != hg.v) continue;

        if(EntityExists(e->workplane)) continue;

        if(!e->h.isFromRequest()) oops();

        (deleted.requests)++;
        SK.request.RemoveById(e->h.request());
        return true;
    }
    return false;
}

bool SolveSpaceUI::PruneConstraints(hGroup hg) {
    int i;
    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *c = &(SK.constraint.elem[i]);
        if(c->group.v != hg.v) continue;

        if(EntityExists(c->workplane) &&
           EntityExists(c->ptA) &&
           EntityExists(c->ptB) &&
           EntityExists(c->entityA) &&
           EntityExists(c->entityB) &&
           EntityExists(c->entityC) &&
           EntityExists(c->entityD))
        {
            continue;
        }

        (deleted.constraints)++;
        if(c->type != Constraint::POINTS_COINCIDENT &&
           c->type != Constraint::HORIZONTAL &&
           c->type != Constraint::VERTICAL)
        {
            (deleted.nonTrivialConstraints)++;
        }

        SK.constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

void SolveSpaceUI::GenerateAll(void) {
    int i;
    int firstDirty = INT_MAX, lastVisible = 0;
    // Start from the first dirty group, and solve until the active group,
    // since all groups after the active group are hidden.
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        g->order = i;
        if((!g->clean) || (g->solved.how != System::SOLVED_OKAY)) {
            firstDirty = min(firstDirty, i);
        }
        if(g->h.v == SS.GW.activeGroup.v) {
            lastVisible = i;
        }
    }
    if(firstDirty == INT_MAX || lastVisible == 0) {
        // All clean; so just regenerate the entities, and don't solve anything.
        GenerateAll(-1, -1);
    } else {
        SS.nakedEdges.Clear();
        GenerateAll(firstDirty, lastVisible);
    }
}

void SolveSpaceUI::GenerateAll(int first, int last, bool andFindFree) {
    int i, j;

    // Remove any requests or constraints that refer to a nonexistent
    // group; can check those immediately, since we know what the list
    // of groups should be.
    while(PruneOrphans())
        ;

    // Don't lose our numerical guesses when we regenerate.
    IdList<Param,hParam> prev = {};
    SK.param.MoveSelfInto(&prev);
    SK.entity.Clear();

    int64_t inTime = GetMilliseconds();

    bool displayedStatusMessage = false;
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);

        int64_t now = GetMilliseconds();
        // Display the status message if we've taken more than 400 ms, or
        // if we've taken 200 ms but we're not even halfway done, or if
        // we've already started displaying the status message.
        if( (now - inTime > 400) ||
           ((now - inTime > 200) && i < (SK.group.n / 2)) ||
           displayedStatusMessage)
        {
            displayedStatusMessage = true;
            char msg[1024];
            sprintf(msg, "generating group %d/%d", i, SK.group.n);

            int w, h;
            GetGraphicsWindowSize(&w, &h);
            glDrawBuffer(GL_FRONT);
            glViewport(0, 0, w, h);
            glLoadIdentity();
            glTranslated(-1, 1, 0);
            glScaled(2.0/w, 2.0/h, 1.0);
            glDisable(GL_DEPTH_TEST);

            double left = 80, top = -20, width = 240, height = 24;
            glColor3d(0.9, 0.8, 0.8);
            ssglAxisAlignedQuad(left, left+width, top, top-height);
            ssglLineWidth(1);
            glColor3d(0.0, 0.0, 0.0);
            ssglAxisAlignedLineLoop(left, left+width, top, top-height);

            ssglInitializeBitmapFont();
            glColor3d(0, 0, 0);
            glPushMatrix();
                glTranslated(left+8, top-20, 0);
                glScaled(1, -1, 1);
                ssglBitmapText(msg, Vector::From(0, 0, 0));
            glPopMatrix();
            glFlush();
            glDrawBuffer(GL_BACK);
        }

        // The group may depend on entities or other groups, to define its
        // workplane geometry or for its operands. Those must already exist
        // in a previous group, so check them before generating.
        if(PruneGroups(g->h))
            goto pruned;

        for(j = 0; j < SK.request.n; j++) {
            Request *r = &(SK.request.elem[j]);
            if(r->group.v != g->h.v) continue;

            r->Generate(&(SK.entity), &(SK.param));
        }
        g->Generate(&(SK.entity), &(SK.param));

        // The requests and constraints depend on stuff in this or the
        // previous group, so check them after generating.
        if(PruneRequests(g->h) || PruneConstraints(g->h))
            goto pruned;

        // Use the previous values for params that we've seen before, as
        // initial guesses for the solver.
        for(j = 0; j < SK.param.n; j++) {
            Param *newp = &(SK.param.elem[j]);
            if(newp->known) continue;

            Param *prevp = prev.FindByIdNoOops(newp->h);
            if(prevp) newp->val = prevp->val;
        }

        if(g->h.v == Group::HGROUP_REFERENCES.v) {
            ForceReferences();
            g->solved.how = System::SOLVED_OKAY;
            g->clean = true;
        } else {
            if(i >= first && i <= last) {
                // The group falls inside the range, so really solve it,
                // and then regenerate the mesh based on the solved stuff.
                SolveGroup(g->h, andFindFree);
                g->GenerateLoops();
                g->GenerateShellAndMesh();
                g->clean = true;
            } else {
                // The group falls outside the range, so just assume that
                // it's good wherever we left it. The mesh is unchanged,
                // and the parameters must be marked as known.
                for(j = 0; j < SK.param.n; j++) {
                    Param *newp = &(SK.param.elem[j]);

                    Param *prevp = prev.FindByIdNoOops(newp->h);
                    if(prevp) newp->known = true;
                }
            }
        }
    }

    // And update any reference dimensions with their new values
    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *c = &(SK.constraint.elem[i]);
        if(c->reference) {
            c->ModifyToSatisfy();
        }
    }

    // Make sure the point that we're tracing exists.
    if(traced.point.v && !SK.entity.FindByIdNoOops(traced.point)) {
        traced.point = Entity::NO_ENTITY;
    }
    // And if we're tracing a point, add its new value to the path
    if(traced.point.v) {
        Entity *pt = SK.GetEntity(traced.point);
        traced.path.AddPoint(pt->PointGetNum());
    }

    prev.Clear();
    InvalidateGraphics();

    // Remove nonexistent selection items, for same reason we waited till
    // the end to put up a dialog box.
    GW.ClearNonexistentSelectionItems();

    if(deleted.requests > 0 || deleted.constraints > 0 || deleted.groups > 0) {
        // All sorts of interesting things could have happened; for example,
        // the active group or active workplane could have been deleted. So
        // clear all that out.
        if(deleted.groups > 0) {
            SS.TW.ClearSuper();
        }
        ScheduleShowTW();
        GW.ClearSuper();

        // People get annoyed if I complain whenever they delete any request,
        // and I otherwise will, since those always come with pt-coincident
        // constraints.
        if(deleted.requests > 0 || deleted.nonTrivialConstraints > 0 ||
           deleted.groups > 0)
        {
            // Don't display any errors until we've regenerated fully. The
            // sketch is not necessarily in a consistent state until we've
            // pruned any orphaned etc. objects, and the message loop for the
            // messagebox could allow us to repaint and crash. But now we must
            // be fine.
            Message("Additional sketch elements were deleted, because they "
                    "depend on the element that was just deleted explicitly. "
                    "These include: \n"
                    "     %d request%s\n"
                    "     %d constraint%s\n"
                    "     %d group%s\n\n"
                    "Choose Edit -> Undo to undelete all elements.",
                       deleted.requests, deleted.requests == 1 ? "" : "s",
                       deleted.constraints, deleted.constraints == 1 ? "" : "s",
                       deleted.groups, deleted.groups == 1 ? "" : "s");
        }

        deleted = {};
    }

    FreeAllTemporary();
    allConsistent = true;
    return;

pruned:
    // Restore the numerical guesses
    SK.param.Clear();
    prev.MoveSelfInto(&(SK.param));
    // Try again
    GenerateAll(first, last);
}

void SolveSpaceUI::ForceReferences(void) {
    // Force the values of the parameters that define the three reference
    // coordinate systems.
    static const struct {
        hRequest    hr;
        Quaternion  q;
    } Quat[] = {
        { Request::HREQUEST_REFERENCE_XY, { 1,    0,    0,    0,   } },
        { Request::HREQUEST_REFERENCE_YZ, { 0.5,  0.5,  0.5,  0.5, } },
        { Request::HREQUEST_REFERENCE_ZX, { 0.5, -0.5, -0.5, -0.5, } },
    };
    for(int i = 0; i < 3; i++) {
        hRequest hr = Quat[i].hr;
        Entity *wrkpl = SK.GetEntity(hr.entity(0));
        // The origin for our coordinate system, always zero
        Entity *origin = SK.GetEntity(wrkpl->point[0]);
        origin->PointForceTo(Vector::From(0, 0, 0));
        SK.GetParam(origin->param[0])->known = true;
        SK.GetParam(origin->param[1])->known = true;
        SK.GetParam(origin->param[2])->known = true;
        // The quaternion that defines the rotation, from the table.
        Entity *normal = SK.GetEntity(wrkpl->normal);
        normal->NormalForceTo(Quat[i].q);
        SK.GetParam(normal->param[0])->known = true;
        SK.GetParam(normal->param[1])->known = true;
        SK.GetParam(normal->param[2])->known = true;
        SK.GetParam(normal->param[3])->known = true;
    }
}

void SolveSpaceUI::MarkDraggedParams(void) {
    sys.dragged.Clear();

    for(int i = -1; i < SS.GW.pending.points.n; i++) {
        hEntity hp;
        if(i == -1) {
            hp = SS.GW.pending.point;
        } else {
            hp = SS.GW.pending.points.elem[i];
        }
        if(!hp.v) continue;

        // The pending point could be one in a group that has not yet
        // been processed, in which case the lookup will fail; but
        // that's not an error.
        Entity *pt = SK.entity.FindByIdNoOops(hp);
        if(pt) {
            switch(pt->type) {
                case Entity::POINT_N_TRANS:
                case Entity::POINT_IN_3D:
                    sys.dragged.Add(&(pt->param[0]));
                    sys.dragged.Add(&(pt->param[1]));
                    sys.dragged.Add(&(pt->param[2]));
                    break;

                case Entity::POINT_IN_2D:
                    sys.dragged.Add(&(pt->param[0]));
                    sys.dragged.Add(&(pt->param[1]));
                    break;
            }
        }
    }
    if(SS.GW.pending.circle.v) {
        Entity *circ = SK.entity.FindByIdNoOops(SS.GW.pending.circle);
        if(circ) {
            Entity *dist = SK.GetEntity(circ->distance);
            switch(dist->type) {
                case Entity::DISTANCE:
                    sys.dragged.Add(&(dist->param[0]));
                    break;
            }
        }
    }
    if(SS.GW.pending.normal.v) {
        Entity *norm = SK.entity.FindByIdNoOops(SS.GW.pending.normal);
        if(norm) {
            switch(norm->type) {
                case Entity::NORMAL_IN_3D:
                    sys.dragged.Add(&(norm->param[0]));
                    sys.dragged.Add(&(norm->param[1]));
                    sys.dragged.Add(&(norm->param[2]));
                    sys.dragged.Add(&(norm->param[3]));
                    break;
                // other types are locked, so not draggable
            }
        }
    }
}

void SolveSpaceUI::SolveGroup(hGroup hg, bool andFindFree) {
    int i;
    // Clear out the system to be solved.
    sys.entity.Clear();
    sys.param.Clear();
    sys.eq.Clear();
    // And generate all the params for requests in this group
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != hg.v) continue;

        r->Generate(&(sys.entity), &(sys.param));
    }
    // And for the group itself
    Group *g = SK.GetGroup(hg);
    g->Generate(&(sys.entity), &(sys.param));
    // Set the initial guesses for all the params
    for(i = 0; i < sys.param.n; i++) {
        Param *p = &(sys.param.elem[i]);
        p->known = false;
        p->val = SK.GetParam(p->h)->val;
    }

    MarkDraggedParams();
    g->solved.remove.Clear();
    int how = sys.Solve(g, &(g->solved.dof),
                           &(g->solved.remove), true, andFindFree);
    if((how != System::SOLVED_OKAY) ||
       (how == System::SOLVED_OKAY && g->solved.how != System::SOLVED_OKAY))
    {
        TextWindow::ReportHowGroupSolved(g->h);
    }
    g->solved.how = how;
    FreeAllTemporary();
}

bool SolveSpaceUI::AllGroupsOkay(void) {
    int i;
    bool allOk = true;
    for(i = 0; i < SK.group.n; i++) {
        if(SK.group.elem[i].solved.how != System::SOLVED_OKAY) {
            allOk = false;
        }
    }
    return allOk;
}

