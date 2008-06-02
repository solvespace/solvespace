#include "solvespace.h"

SolveSpace SS;

void SolveSpace::Init(char *cmdLine) {
    NewFile();
    AfterNewFile();

    if(strlen(cmdLine) != 0) {
        if(LoadFromFile(cmdLine)) {
            strcpy(saveFile, cmdLine);
        } else {
            NewFile();
        }
    }

    AfterNewFile();
}

void SolveSpace::AfterNewFile(void) {
    ReloadAllImported();
    GenerateAll(-1, -1);

    TW.Init();
    GW.Init();

    GenerateAll(0, INT_MAX);
    TW.Show();
}

bool SolveSpace::PruneOrphans(void) {
    int i;
    for(i = 0; i < request.n; i++) {
        Request *r = &(request.elem[i]);
        if(GroupExists(r->group)) continue;

        (deleted.requests)++;
        request.RemoveById(r->h);
        return true;
    }

    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(GroupExists(c->group)) continue;

        (deleted.constraints)++;
        constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

bool SolveSpace::GroupsInOrder(hGroup before, hGroup after) {
    if(before.v == 0) return true;
    if(after.v  == 0) return true;

    int beforep = -1, afterp = -1;
    int i;
    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->h.v == before.v) beforep = i;
        if(g->h.v == after.v)  afterp  = i;
    }
    if(beforep < 0 || afterp < 0) return false;
    if(beforep >= afterp) return false;
    return true;
}

bool SolveSpace::GroupExists(hGroup hg) {
    // A nonexistent group is not acceptable
    return group.FindByIdNoOops(hg) ? true : false;
}
bool SolveSpace::EntityExists(hEntity he) {
    // A nonexstient entity is acceptable, though, usually just means it
    // doesn't apply.
    if(he.v == Entity::NO_ENTITY.v) return true;
    return entity.FindByIdNoOops(he) ? true : false;
}

bool SolveSpace::PruneGroups(hGroup hg) {
    Group *g = GetGroup(hg);
    if(GroupsInOrder(g->opA, hg) &&
       EntityExists(g->predef.origin) &&
       EntityExists(g->predef.entityB) &&
       EntityExists(g->predef.entityC))
    {
        return false;
    }
    (deleted.groups)++;
    group.RemoveById(g->h);
    return true;
}

bool SolveSpace::PruneRequests(hGroup hg) {
    int i;
    for(i = 0; i < entity.n; i++) {
        Entity *e = &(entity.elem[i]);
        if(e->group.v != hg.v) continue;

        if(EntityExists(e->workplane)) continue;

        if(!e->h.isFromRequest()) oops();

        (deleted.requests)++;
        request.RemoveById(e->h.request());
        return true;
    }
    return false;
}

bool SolveSpace::PruneConstraints(hGroup hg) {
    int i;
    for(i = 0; i < constraint.n; i++) {
        Constraint *c = &(constraint.elem[i]);
        if(c->group.v != hg.v) continue;

        if(EntityExists(c->workplane) &&
           EntityExists(c->ptA) &&
           EntityExists(c->ptB) &&
           EntityExists(c->ptC) &&
           EntityExists(c->entityA) &&
           EntityExists(c->entityB))
        {
            continue;
        }

        (deleted.constraints)++;
        constraint.RemoveById(c->h);
        return true;
    }
    return false;
}

void SolveSpace::GenerateAll(void) {
    int i;
    int firstShown = INT_MAX, lastShown = 0;
    // The references don't count, so start from group 1
    for(i = 1; i < group.n; i++) {
        Group *g = &(group.elem[i]);
        if(g->visible || (g->solved.how != Group::SOLVED_OKAY)) {
            firstShown = min(firstShown, i);
            lastShown  = max(lastShown,  i);
        }
    }
    // Even if nothing is shown, we have to keep going; the entities get
    // generated for hidden groups, even though they're not solved.
    GenerateAll(firstShown, lastShown);
}

void SolveSpace::GenerateAll(int first, int last) {
    int i, j;

    while(PruneOrphans())
        ;

    // Don't lose our numerical guesses when we regenerate.
    IdList<Param,hParam> prev;
    param.MoveSelfInto(&prev);
    entity.Clear();

    for(i = 0; i < group.n; i++) {
        Group *g = &(group.elem[i]);

        // The group may depend on entities or other groups, to define its
        // workplane geometry or for its operands. Those must already exist
        // in a previous group, so check them before generating.
        if(PruneGroups(g->h))
            goto pruned;

        for(j = 0; j < request.n; j++) {
            Request *r = &(request.elem[j]);
            if(r->group.v != g->h.v) continue;

            r->Generate(&entity, &param);
        }
        g->Generate(&entity, &param);

        // The requests and constraints depend on stuff in this or the
        // previous group, so check them after generating.
        if(PruneRequests(g->h) || PruneConstraints(g->h))
            goto pruned;

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
            g->solved.how = Group::SOLVED_OKAY;
        } else {
            if(i >= first && i <= last) {
                // The group falls inside the range, so really solve it,
                // and then regenerate the mesh based on the solved stuff.
                SolveGroup(g->h);
                g->GeneratePolygon();
                g->GenerateMesh();
            } else {
                // The group falls outside the range, so just assume that
                // it's good wherever we left it. The mesh is unchanged,
                // and the parameters must be marked as known.
                for(j = 0; j < param.n; j++) {
                    Param *newp = &(param.elem[j]);

                    Param *prevp = prev.FindByIdNoOops(newp->h);
                    if(prevp) newp->known = true;
                }
            }
        }
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
        TW.Show();
        GW.ClearSuper();
        // Don't display any errors until we've regenerated fully. The
        // sketch is not necessarily in a consistent state until we've
        // pruned any orphaned etc. objects, and the message loop for the
        // messagebox could allow us to repaint and crash. But now we must
        // be fine.
        Error("Additional sketch elements were deleted, because they depend "
              "on the element that was just deleted explicitly. These "
              "include: \r\n"
              "     %d request%s\r\n"
              "     %d constraint%s\r\n"
              "     %d group%s\r\n\r\n"
              "Choose Edit -> Undo to undelete all elements.",
                deleted.requests, deleted.requests == 1 ? "" : "s",
                deleted.constraints, deleted.constraints == 1 ? "" : "s",
                deleted.groups, deleted.groups == 1 ? "" : "s");
        memset(&deleted, 0, sizeof(deleted));
    }
    return;

pruned:
    // Restore the numerical guesses
    param.Clear();
    prev.MoveSelfInto(&param);
    // Try again
    GenerateAll(first, last);
}

void SolveSpace::ForceReferences(void) {
    // Force the values of the paramters that define the three reference
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
        Entity *wrkpl = GetEntity(hr.entity(0));
        // The origin for our coordinate system, always zero
        Entity *origin = GetEntity(wrkpl->point[0]);
        origin->PointForceTo(Vector::From(0, 0, 0));
        GetParam(origin->param[0])->known = true;
        GetParam(origin->param[1])->known = true;
        GetParam(origin->param[2])->known = true;
        // The quaternion that defines the rotation, from the table.
        Entity *normal = GetEntity(wrkpl->normal); 
        normal->NormalForceTo(Quat[i].q);
        GetParam(normal->param[0])->known = true;
        GetParam(normal->param[1])->known = true;
        GetParam(normal->param[2])->known = true;
        GetParam(normal->param[3])->known = true;
    }
}

void SolveSpace::SolveGroup(hGroup hg) {
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

    sys.Solve(g);
    FreeAllTemporary();
}

void SolveSpace::RemoveFromRecentList(char *file) {
    int src, dest;
    dest = 0;
    for(src = 0; src < MAX_RECENT; src++) {
        if(strcmp(file, RecentFile[src]) != 0) {
            if(src != dest) strcpy(RecentFile[dest], RecentFile[src]);
            dest++;
        }
    }
    while(dest < MAX_RECENT) strcpy(RecentFile[dest++], "");
    RefreshRecentMenus();
}
void SolveSpace::AddToRecentList(char *file) {
    RemoveFromRecentList(file);

    int src;
    for(src = MAX_RECENT - 2; src >= 0; src--) {
        strcpy(RecentFile[src+1], RecentFile[src]);
    }
    strcpy(RecentFile[0], file);
    RefreshRecentMenus();
}

void SolveSpace::MenuFile(int id) {

    if(id >= RECENT_OPEN && id < (RECENT_OPEN+MAX_RECENT)) {
        char newFile[MAX_PATH];
        strcpy(newFile, RecentFile[id-RECENT_OPEN]);
        RemoveFromRecentList(newFile);
        if(SS.LoadFromFile(newFile)) {
            strcpy(SS.saveFile, newFile);
            AddToRecentList(newFile);
        } else {
            strcpy(SS.saveFile, "");
            SS.NewFile();
        }
        SS.AfterNewFile();
        return;
    }

    switch(id) {
        case GraphicsWindow::MNU_NEW:
            strcpy(SS.saveFile, "");
            SS.NewFile();
            SS.AfterNewFile();
            break;

        case GraphicsWindow::MNU_OPEN: {
            char newFile[MAX_PATH] = "";
            if(GetOpenFile(newFile, SLVS_EXT, SLVS_PATTERN)) {
                if(SS.LoadFromFile(newFile)) {
                    strcpy(SS.saveFile, newFile);
                    AddToRecentList(newFile);
                } else {
                    strcpy(SS.saveFile, "");
                    SS.NewFile();
                }
                SS.AfterNewFile();
            }
            break;
        }

        case GraphicsWindow::MNU_SAVE:
        case GraphicsWindow::MNU_SAVE_AS: {
            char newFile[MAX_PATH];
            strcpy(newFile, SS.saveFile);
            if(id == GraphicsWindow::MNU_SAVE_AS || strlen(newFile)==0) {
                if(!GetSaveFile(newFile, SLVS_EXT, SLVS_PATTERN)) break;
            }

            if(SS.SaveToFile(newFile)) {
                AddToRecentList(newFile);
                strcpy(SS.saveFile, newFile);
            }
            break;
        }

        case GraphicsWindow::MNU_EXIT:
            break;

        default: oops();
    }
}
