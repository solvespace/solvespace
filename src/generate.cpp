//-----------------------------------------------------------------------------
// Generate our model based on its parametric description, by solving each
// sketch, generating surfaces from the resulting entities, performing any
// requested surface operations (e.g. Booleans) with our model so far, and
// then repeating this process for each subsequent group.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void SolveSpaceUI::MarkGroupDirtyByEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    MarkGroupDirty(e->group);
}

void Sketch::MarkGroupDirty(hGroup hg, bool onlyThis) {
    bool go = false;
    for(auto const &gh : groupOrder) {
        Group *g = GetGroup(gh);
        if(g->h == hg) {
            go = true;
        }
        if(go) {
            g->clean = false;
            if(onlyThis) break;
        }
    }
}

void SolveSpaceUI::MarkGroupDirty(hGroup hg, bool onlyThis) {
    SK.MarkGroupDirty(hg, onlyThis);
    unsaved = true;
    ScheduleGenerateAll();
}

bool Sketch::GroupsInOrder(hGroup hbefore, hGroup hafter) {
    if(hbefore.v != 0 && hafter.v != 0) {
        const Group *before = group.FindByIdNoOops(hbefore);
        if(before == nullptr) {
            return false;
        }
        const Group *after = group.FindByIdNoOops(hafter);
        if(after == nullptr) {
            return false;
        }
        if(before->order >= after->order) {
            return false;
        }
    }
    return true;
}

bool Sketch::GroupExists(hGroup hg) {
    // A nonexistent group is not acceptable
    return group.FindByIdNoOops(hg) ? true : false;
}
bool Sketch::EntityExists(hEntity he) {
    // A nonexstient entity is acceptable, though, usually just means it
    // doesn't apply.
    if(he == Entity::NO_ENTITY) return true;
    return entity.FindByIdNoOops(he) ? true : false;
}

// Remove the group if anything it depends on doesn't exist. Returns true
// if it was removed.
bool Sketch::PruneGroup(Group *g) {
    if(GroupsInOrder(g->opA, g->h) &&
       EntityExists(g->predef.origin) &&
       EntityExists(g->predef.entityB) &&
       EntityExists(g->predef.entityC))
    {
        return false;
    }
    group.RemoveById(g->h);
    RegenerateGroupOrder();
    return true;
}

// Remove the constraints tagged for pruning, if any were, and clear the flag.
// Returns the number of constraints removed.
int Sketch::RemoveTaggedConstraints(bool *tagged) {
    if(!*tagged) {
        return 0;
    }
    const int constraints = constraint.n;
    constraint.RemoveTagged();
    *tagged = false;
    return constraints - constraint.n;
}

void Sketch::RegenerateGroupOrder() {
    using GroupOrderPair = std::pair<int, hGroup>;
    std::vector<GroupOrderPair> order;
    order.reserve(group.n);
    for(auto &g : group) {
        order.push_back({g.order, g.h});
    }
    std::sort(order.begin(), order.end(), [](const GroupOrderPair &a, const GroupOrderPair &b) {
        return a.first < b.first;
    });
    groupOrder.Clear();
    groupOrder.ReserveMore(group.n);
    for(auto &p : order) {
        groupOrder.Add(&p.second);
    }
}

void Sketch::GenerateEntities(const GenerateOptions &opts, GenerateResult *result) {
    *result = {};
    result->first = opts.first;
    result->last  = opts.last;
    int i;

    // Don't lose our numerical guesses when we regenerate.
    ParamList prev = {};
    param.MoveSelfInto(&prev);
    param.ReserveMore(prev.n);
    const int oldEntityCount = entity.n;
    entity.Clear();
    entity.ReserveMore(oldEntityCount);

    request.ClearTags();
    constraint.ClearTags();

    // The solver uses the constraint tags too: it clears them and marks the
    // constraints that it couldn't satisfy. So any constraints tagged for
    // pruning must be removed before a group is solved, and the solver's marks
    // cleared afterwards, or they would be mistaken for each other.
    bool constraintsTagged = false;

    // Prune any requests and constraints that belong to non-existent groups
    for(auto &r : request) {
        if(!GroupExists(r.group)) {
            r.tag = 1;
        }
    }
    for(auto &c : constraint) {
        if(!GroupExists(c.group)) {
            c.tag = 1;
            constraintsTagged = true;
        }
    }

    // The group before the current one, whose named parameters it inherits
    hGroup previous = {};

    // Not using range-for because we're using the index inside the loop.
    for(i = 0; i < groupOrder.n; ) {
        const hGroup hg = groupOrder[i];
        Group *g = GetGroup(hg);

        // The group may depend on entities or other groups, to define its
        // workplane geometry or for its operands. Those must already exist
        // in a previous group, so check them before generating.
        if(PruneGroup(g)) {
            (result->groups)++;
            // A group was just removed and the group order was regenerated,
            // so adjust the first and last indices accordingly.
            if(i <= result->last && result->last < INT_MAX) {
                --(result->last);
            }
            if(i < result->first && result->first < INT_MAX) {
                --(result->first);
            }
            // Tag this group's requests for pruning
            for(auto &r : request) {
                if(r.group == hg) {
                    r.tag = 1;
                }
            }
            // Tag this group's constraints for pruning
            for(auto &c : constraint) {
                if(c.group == hg) {
                    c.tag = 1;
                    constraintsTagged = true;
                }
            }
            continue;
        }

        // Named parameters are in scope from where they are defined to the
        // end of the sketch. A name defined again replaces the earlier one.
        if(previous.v != 0) {
            g->inheritedParamNames = GetGroup(previous)->paramNames;
        } else {
            g->inheritedParamNames.clear();
        }
        g->paramNames = g->inheritedParamNames;

        // Regenerate requests, tagging any dead ones for pruning
        int groupRequestIndex = 0;
        for(auto &r : request) {
            if(r.group != hg) {
                continue;
            }

            if(r.IsNamedParam()) {
                r.Generate(&entity, &param);
                g->paramNames[r.str] = r.NamedParam();
                continue;
            }

            // Prune the request if it depends on a pruned workplane
            if(r.workplane != Entity::NO_ENTITY && r.workplane.isFromRequest()) {
                const Request *wr = request.FindByIdNoOops(r.workplane.request());
                if(wr == nullptr || wr->tag) {
                    r.tag = 1;
                    continue;
                }
            }

            r.groupRequestIndex = groupRequestIndex++;
            r.Generate(&entity, &param);
        }

        // Regenerate the group
        g->Generate(&entity, &param);

        // Regenerate constraints, tagging any dead ones for pruning
        for(auto &c : constraint) {
            if(c.group != hg) {
                continue;
            }

            if(!EntityExists(c.workplane) ||
               !EntityExists(c.ptA) ||
               !EntityExists(c.ptB) ||
               !EntityExists(c.entityA) ||
               !EntityExists(c.entityB) ||
               !EntityExists(c.entityC) ||
               !EntityExists(c.entityD)) {
                if(c.type != Constraint::Type::POINTS_COINCIDENT &&
                   c.type != Constraint::Type::HORIZONTAL &&
                   c.type != Constraint::Type::VERTICAL) {
                    ++result->nonTrivialConstraints;
                }

                c.tag = 1;
                constraintsTagged = true;
                continue;
            }

            c.Generate(&param);
        }

        // Use the previous values for params that we've seen before, as
        // initial guesses for the solver.
        for(auto &p : param) {
            Param *newp = &p;
            if(newp->known) {
                continue;
            }

            Param *prevp = prev.FindByIdNoOops(newp->h);
            if(prevp) {
                newp->val = prevp->val;
                newp->free = prevp->free;
            }
        }

        // A named parameter given by an expression gets its value from the
        // (already solved) parameters of earlier groups. If a name isn't
        // defined, it keeps its last value, and the solve reports it.
        for(auto &r : request) {
            if(r.group != hg || !r.IsNamedParam() || !r.HasExpression()) continue;

            if(Expr *e = r.ParseExpression()) {
                GetParam(r.NamedParam())->val = e->Eval();
            }
        }

        if(hg == Group::HGROUP_REFERENCES) {
            ForceReferences();
            Group *g = GetGroup(hg);
            g->solved.how = SolveResult::OKAY;
            g->clean = true;
        } else {
            // this i is an index in groupOrder
            if(i >= result->first && i <= result->last) {
                // The group falls inside the range, so really solve it,
                // and then regenerate the mesh based on the solved stuff.
                if(!opts.noSolve) {
                    result->constraints += RemoveTaggedConstraints(&constraintsTagged);
                    result->solved.push_back({ hg, g->solved.how });
                    SolveGroup(hg, opts.sys, opts.dragged, opts.andFindFree,
                               opts.timeoutRedundantConstr);
                    constraint.ClearTags();
                    g->GenerateLoops();
                }
            } else {
                // The group falls outside the range, so just assume that
                // it's good wherever we left it. The mesh is unchanged,
                // and the parameters must be marked as known.
                for(auto &p : param) {
                    Param *newp = &p;

                    Param *prevp = prev.FindByIdNoOops(newp->h);
                    if(prevp) {
                        newp->known = true;
                    }
                }
            }
        }

        // Keep the value of each dimension given by an expression up to date,
        // so that anything that reads valA sees the actual dimension. When all
        // dimensions are reference, they hold their measured values instead.
        if(!g->allDimsReference) {
            for(auto &c : constraint) {
                if(c.group == hg && !c.tag && !c.reference) {
                    c.UpdateValueFromExpression();
                }
            }
        }

        previous = hg;
        ++i;
    }

    // Remove any requests that we tagged for pruning
    const int requests = request.n;
    request.RemoveTagged();
    result->requests += requests - request.n;

    // Remove any constraints that we tagged for pruning
    result->constraints += RemoveTaggedConstraints(&constraintsTagged);

    prev.Clear();
}

void Sketch::GenerateShellsAndMeshes(int first, int last) {
    for(int i = 0; i < groupOrder.n; ++i) {
        const hGroup hg = groupOrder[i];
        if(hg == Group::HGROUP_REFERENCES) {
            continue;
        }
        if(i >= first && i <= last) {
            Group *g = GetGroup(hg);
            g->GenerateShellAndMesh();
            g->clean = true;
        }
    }

    // And update any reference dimensions with their new values
    for(auto &c : constraint) {
        if(c.reference) {
            c.ModifyToSatisfy();
        }
    }
}

void SolveSpaceUI::GenerateAll(Generate type, bool andFindFree) {
    int first = 0, last = 0, i;

    uint64_t startMillis = GetMilliseconds(),
             endMillis;

    SK.RegenerateGroupOrder();

    switch(type) {
        case Generate::DIRTY: {
            first = INT_MAX;
            last  = 0;

            // Start from the first dirty group, and solve until the active group,
            // since all groups after the active group are hidden.
            // Not using range-for because we're tracking the indices.
            for(i = 0; i < SK.groupOrder.n; i++) {
                Group *g = SK.GetGroup(SK.groupOrder[i]);
                if((!g->clean) || !g->IsSolvedOkay()) {
                    first = min(first, i);
                }
                if(g->h == SS.GW.activeGroup) {
                    last = i;
                }
            }
            if(first == INT_MAX || last == 0) {
                // All clean; so just regenerate the entities, and don't solve anything.
                first = -1;
                last  = -1;
            } else {
                SS.nakedEdges.Clear();
            }
            break;
        }

        case Generate::ALL:
            first = 0;
            last  = INT_MAX;
            break;

        case Generate::REGEN:
            first = -1;
            last  = -1;
            break;

        case Generate::UNTIL_ACTIVE: {
            for(i = 0; i < SK.groupOrder.n; i++) {
                if(SK.groupOrder[i] == SS.GW.activeGroup)
                    break;
            }

            first = 0;
            last  = i;
            break;
        }
    }

    Sketch::GenerateOptions opts = {};
    opts.first = first;
    opts.last = last;
    opts.noSolve = exportMode;
    opts.andFindFree = andFindFree;
    opts.timeoutRedundantConstr = timeoutRedundantConstr;
    opts.sys = &sys;
    opts.dragged = DraggedEntities();
    Sketch::GenerateResult result;
    SK.GenerateEntities(opts, &result);

    deleted.requests += result.requests;
    deleted.groups += result.groups;
    deleted.constraints += result.constraints;
    deleted.nonTrivialConstraints += result.nonTrivialConstraints;

    // Show why any group failed to solve
    for(const Sketch::SolvedGroup &sg : result.solved) {
        ReportHowGroupSolved(sg.h, sg.before);
    }

    // If we're generating entities for display, first we need to find
    // the bounding box to turn relative chord tolerance to absolute.
    if(!exportMode) {
        BBox box = SK.CalculateEntityBBox(/*includeInvisibles=*/true);
        Vector size = box.maxp.Minus(box.minp);
        double maxSize = std::max({ size.x, size.y, size.z });
        chordTolCalculated = maxSize * chordTol / 100.0;
    }

    // Then generate the shell and mesh
    SK.GenerateShellsAndMeshes(result.first, result.last);

    // Make sure the point that we're tracing exists.
    if(traced.point.v && !SK.entity.FindByIdNoOops(traced.point)) {
        traced.point = Entity::NO_ENTITY;
    }
    // And if we're tracing a point, add its new value to the path
    if(traced.point.v) {
        Entity *pt = SK.GetEntity(traced.point);
        traced.path.AddPoint(pt->PointGetNum());
    }

    GW.Invalidate();

    // Remove nonexistent selection items, for same reason we waited till
    // the end to put up a dialog box.
    GW.ClearNonexistentSelectionItems();

    if(deleted.requests > 0 || deleted.constraints > 0 || deleted.groups > 0) {
        // All sorts of interesting things could have happened; for example,
        // the active group or active workplane could have been deleted. So
        // clear all that out.
        if(deleted.groups > 0) {
            SS.TW.ClearSuper();
            GW.EnsureValidActives();
        }
        ScheduleShowTW();
        GW.ClearSuper();

        auto deletedStat = deleted;
        deleted = {};

        // People get annoyed if I complain whenever they delete any request,
        // and I otherwise will, since those always come with pt-coincident
        // constraints.
        if(deletedStat.requests > 0 || deletedStat.nonTrivialConstraints > 0 ||
           deletedStat.groups > 0)
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
                    "     %d group%s"
                    "%s",
                       deletedStat.requests, deletedStat.requests == 1 ? "" : "s",
                       deletedStat.constraints, deletedStat.constraints == 1 ? "" : "s",
                       deletedStat.groups, deletedStat.groups == 1 ? "" : "s",
                       undo.cnt > 0 ? "\n\nChoose Edit -> Undo to undelete all elements." : "");
        }
    }

    Platform::FreeAllTemporary();
    allConsistent = true;
    SS.GW.persistentDirty = true;
    SS.centerOfMass.dirty = true;

    endMillis = GetMilliseconds();

    if(endMillis - startMillis > 30) {
        const char *typeStr = "";
        switch(type) {
            case Generate::DIRTY:           typeStr = "DIRTY";        break;
            case Generate::ALL:             typeStr = "ALL";          break;
            case Generate::REGEN:           typeStr = "REGEN";        break;
            case Generate::UNTIL_ACTIVE:    typeStr = "UNTIL_ACTIVE"; break;
        }
        if(endMillis)
        dbp("Generate::%s took %lld ms",
            typeStr,
            GetMilliseconds() - startMillis);
    }
}

void Sketch::ForceReferences() {
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
        Entity *wrkpl = GetEntity(hr.entity(0));
        // The origin for our coordinate system, always zero
        Entity *origin = GetEntity(wrkpl->point[0]);
        origin->PointForceTo({0, 0, 0});
        origin->construction = true;
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

void SolveSpaceUI::UpdateCenterOfMass() {
    SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);
    SS.centerOfMass.position = m->GetCenterOfMass();
    SS.centerOfMass.dirty = false;
}

// What is being dragged in the graphics window, if anything
Sketch::Dragged SolveSpaceUI::DraggedEntities() {
    Sketch::Dragged dragged = {};
    if(GW.pending.point.v) {
        dragged.points.push_back(GW.pending.point);
    }
    for(const hEntity &hp : GW.pending.points) {
        dragged.points.push_back(hp);
    }
    dragged.circle = GW.pending.circle;
    dragged.normal = GW.pending.normal;
    return dragged;
}

void Sketch::MarkDraggedParams(System *sys, const Dragged &dragged) {
    sys->dragged.clear();

    for(const hEntity &hp : dragged.points) {
        // The pending point could be one in a group that has not yet
        // been processed, in which case the lookup will fail; but
        // that's not an error.
        Entity *pt = entity.FindByIdNoOops(hp);
        if(pt) {
            switch(pt->type) {
                case Entity::Type::POINT_N_TRANS:
                case Entity::Type::POINT_IN_3D:
                case Entity::Type::POINT_N_ROT_AXIS_TRANS:
                    sys->dragged.insert(pt->param[0]);
                    sys->dragged.insert(pt->param[1]);
                    sys->dragged.insert(pt->param[2]);
                    break;

                case Entity::Type::POINT_IN_2D:
                    sys->dragged.insert(pt->param[0]);
                    sys->dragged.insert(pt->param[1]);
                    break;

                default: // Only the entities above can be dragged.
                    break;
            }
        }
    }
    if(dragged.circle.v) {
        Entity *circ = entity.FindByIdNoOops(dragged.circle);
        if(circ) {
            Entity *dist = GetEntity(circ->distance);
            switch(dist->type) {
                case Entity::Type::DISTANCE:
                    sys->dragged.insert(dist->param[0]);
                    break;

                default: // Only the entities above can be dragged.
                    break;
            }
        }
    }
    if(dragged.normal.v) {
        Entity *norm = entity.FindByIdNoOops(dragged.normal);
        if(norm) {
            switch(norm->type) {
                case Entity::Type::NORMAL_IN_3D:
                    sys->dragged.insert(norm->param[0]);
                    sys->dragged.insert(norm->param[1]);
                    sys->dragged.insert(norm->param[2]);
                    sys->dragged.insert(norm->param[3]);
                    break;

                default: // Only the entities above can be dragged.
                    break;
            }
        }
    }
}

// If a group just solved has failed, show why in the text window.
void SolveSpaceUI::ReportHowGroupSolved(hGroup hg, SolveResult before) {
    Group *g = SK.GetGroup(hg);
    // An unknown name stays until the user defines it again, perhaps in the
    // text window right now; so report it when it first happens, rather than
    // taking them away from the page they are on with every change.
    bool stillUnknownName = (before == SolveResult::UNKNOWN_NAME &&
                             g->solved.how == SolveResult::UNKNOWN_NAME);
    bool isOkay = g->solved.how == SolveResult::OKAY ||
                  (g->allowRedundant && g->solved.how == SolveResult::REDUNDANT_OKAY);
    if(!stillUnknownName && (!isOkay || (isOkay && !g->IsSolvedOkay()))) {
        TextWindow::ReportHowGroupSolved(g->h);
    }
}

void Sketch::WriteEqSystemForGroup(hGroup hg, System *sys, const Dragged &dragged) {
    // Clear out the system to be solved.
    sys->entity.Clear();
    sys->param.Clear();
    sys->eq.Clear();
    // And generate all the params for requests in this group
    for(auto &req : request) {
        Request *r = &req;
        if(r->group != hg || r->tag) continue;
        // A locked parameter stays out of the system, so the solver can't
        // change it; equations that use it see it as a constant.
        if(r->type == Request::Type::NAMED_CONST_PARAM) continue;

        r->Generate(&(sys->entity), &(sys->param));
    }
    for(auto &con : constraint) {
        Constraint *c = &con;
        if(c->group != hg || c->tag) continue;

        c->Generate(&(sys->param));
    }
    // And for the group itself
    Group *g = GetGroup(hg);
    g->Generate(&(sys->entity), &(sys->param));
    // Set the initial guesses for all the params
    for(auto &p : sys->param) {
        p.known = false;
        p.val = GetParam(p.h)->val;
    }

    MarkDraggedParams(sys, dragged);
}

void Sketch::SolveGroup(hGroup hg, System *sys, const Dragged &dragged, bool andFindFree,
                        int timeoutRedundantConstr) {
    WriteEqSystemForGroup(hg, sys, dragged);
    Group *g = GetGroup(hg);
    g->solved.remove.Clear();
    g->solved.findToFixTimeout = timeoutRedundantConstr;
    SolveResult how = sys->Solve(g, &(g->solved.dof),
                                    &(g->solved.remove),
                                    /*andFindBad=*/!g->allowRedundant,
                                    /*andFindFree=*/andFindFree,
                                    /*forceDofCheck=*/!g->dofCheckOk);
    if(how == SolveResult::OKAY) {
        g->dofCheckOk = true;
    }
    g->solved.how = how;

    // A dimension whose expression uses a name that isn't defined generated
    // no equation, and a parameter whose expression does kept its old value.
    // Report those instead of whatever else happened. The dimensions are
    // listed in solved.remove; the parameters are found again when shown.
    bool unknownName = false;
    if(!g->allDimsReference && !g->relaxConstraints) {
        for(auto &c : constraint) {
            if(c.group != hg || c.reference || !c.HasExpression()) continue;
            if(c.ParseExpression() != NULL) continue;

            if(!unknownName) {
                g->solved.remove.Clear();
                unknownName = true;
            }
            g->solved.remove.Add(&c.h);
        }
    }
    for(auto &r : request) {
        if(r.group != hg || !r.IsNamedParam() || !r.HasExpression()) continue;
        if(r.ParseExpression() != NULL) continue;

        if(!unknownName) {
            g->solved.remove.Clear();
            unknownName = true;
        }
    }
    if(unknownName) {
        g->solved.how = SolveResult::UNKNOWN_NAME;
    }
    Platform::FreeAllTemporary();
}

SolveResult Sketch::TestRankForGroup(hGroup hg, System *sys, int *rank) {
    Group *g = GetGroup(hg);
    // If we don't calculate dof or redundant is allowed, there is
    // no point to solve rank because this result is not meaningful
    if(g->suppressDofCalculation || g->allowRedundant) return SolveResult::OKAY;
    WriteEqSystemForGroup(hg, sys, Dragged {});
    SolveResult result = sys->SolveRank(g, rank);
    Platform::FreeAllTemporary();
    return result;
}

SolveResult SolveSpaceUI::TestRankForGroup(hGroup hg, int *rank) {
    return SK.TestRankForGroup(hg, &sys, rank);
}

bool SolveSpaceUI::ActiveGroupsOkay() {
    for(int i = 0; i < SK.groupOrder.n; i++) {
        Group *g = SK.GetGroup(SK.groupOrder[i]);
        if(!g->IsSolvedOkay())
            return false;
        if(g->h == SS.GW.activeGroup)
            break;
    }
    return true;
}

} // namespace SolveSpace
