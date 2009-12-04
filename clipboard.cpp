#include "solvespace.h"

void SolveSpace::Clipboard::Clear(void) {
    c.Clear();
    r.Clear();
}

hEntity SolveSpace::Clipboard::NewEntityFor(hEntity he) {
    ClipboardRequest *cr;
    for(cr = r.First(); cr; cr = r.NextAfter(cr)) {
        if(cr->oldEnt.v == he.v) {
            return cr->newReq.entity(0);
        }
        for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
            if(cr->oldPointEnt[i].v == he.v) {
                return cr->newReq.entity(1+i);
            }
        }
    }
    return Entity::NO_ENTITY;
}

bool SolveSpace::Clipboard::ContainsEntity(hEntity he) {
    hEntity hen = NewEntityFor(he);
    if(hen.v) {
        return true;
    } else {
        return false;
    }
}

void GraphicsWindow::DeleteSelection(void) {
    SK.request.ClearTags();
    SK.constraint.ClearTags();
    List<Selection> *ls = &(selection);
    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        hRequest r = { 0 };
        if(s->entity.v && s->entity.isFromRequest()) {
            r = s->entity.request();
        }
        if(r.v && !r.IsFromReferences()) {
            SK.request.Tag(r, 1);
        }
        if(s->constraint.v) {
            SK.constraint.Tag(s->constraint, 1);
        }
    }

    SK.constraint.RemoveTagged();
    // Note that this regenerates and clears the selection, to avoid
    // lingering references to the just-deleted items.
    DeleteTaggedRequests();
}

void GraphicsWindow::CopySelection(void) {
    SS.clipboard.Clear();

    Entity *wrkpl  = SK.GetEntity(ActiveWorkplane());
    Entity *wrkpln = SK.GetEntity(wrkpl->normal);
    Vector u = wrkpln->NormalU(),
           v = wrkpln->NormalV(),
           n = wrkpln->NormalN(),
           p = SK.GetEntity(wrkpl->point[0])->PointGetNum();

    List<Selection> *ls = &(selection);
    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        if(!s->entity.v) continue;
        // Work only on entities that have requests that will generate them.
        Entity *e = SK.GetEntity(s->entity);
        bool hasDistance;
        int req, pts;
        if(!EntReqTable::GetEntityInfo(e->type, e->extraPoints,
                &req, &pts, NULL, &hasDistance))
        {
            continue;
        }
        if(req == Request::WORKPLANE) continue;

        ClipboardRequest cr;
        ZERO(&cr);
        cr.type         = req;
        cr.extraPoints  = e->extraPoints;
        cr.style        = e->style;
        cr.str.strcpy(    e->str.str);
        cr.font.strcpy(   e->font.str);
        cr.construction = e->construction;
        for(int i = 0; i < pts; i++) {
            Vector pt = SK.GetEntity(e->point[i])->PointGetNum();
            pt = pt.Minus(p);
            pt = pt.DotInToCsys(u, v, n);
            cr.point[i] = pt;
        }
        if(hasDistance) {
            cr.distance = SK.GetEntity(e->distance)->DistanceGetNum();
        }

        cr.oldEnt = e->h;
        for(int i = 0; i < pts; i++) {
            cr.oldPointEnt[i] = e->point[i];
        }

        SS.clipboard.r.Add(&cr);
    }

    Constraint *c;
    for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
        if(c->type == Constraint::POINTS_COINCIDENT) {
            if(!SS.clipboard.ContainsEntity(c->ptA)) continue;
            if(!SS.clipboard.ContainsEntity(c->ptB)) continue;
        } else {
            continue;
        }
        SS.clipboard.c.Add(c);
    }
}

void GraphicsWindow::PasteClipboard(Vector trans, double theta, bool mirror) {
    SS.UndoRemember();
    ClearSelection();

    Entity *wrkpl  = SK.GetEntity(ActiveWorkplane());
    Entity *wrkpln = SK.GetEntity(wrkpl->normal);
    Vector u = wrkpln->NormalU(),
           v = wrkpln->NormalV(),
           p = SK.GetEntity(wrkpl->point[0])->PointGetNum();


    ClipboardRequest *cr;
    for(cr = SS.clipboard.r.First(); cr; cr = SS.clipboard.r.NextAfter(cr)) {
        hRequest hr = AddRequest(cr->type, false);
        Request *r = SK.GetRequest(hr);
        r->extraPoints  = cr->extraPoints;
        r->style        = cr->style;
        r->str.strcpy(    cr->str.str);
        r->font.strcpy(   cr->font.str);
        r->construction = cr->construction;
        // Need to regen to get the right number of points, if extraPoints
        // changed.
        SS.GenerateAll(-1, -1);
        SS.MarkGroupDirty(r->group);
        bool hasDistance;
        int pts;
        EntReqTable::GetRequestInfo(r->type, r->extraPoints,
            NULL, &pts, NULL, &hasDistance);
        for(int i = 0; i < pts; i++) {
            Vector pt = cr->point[i];
            if(mirror) pt.x *= -1;
            pt = Vector::From( cos(theta)*pt.x + sin(theta)*pt.y,
                              -sin(theta)*pt.x + cos(theta)*pt.y,
                              0);
            pt = pt.ScaleOutOfCsys(u, v, Vector::From(0, 0, 0));
            pt = pt.Plus(p);
            pt = pt.Plus(trans);
            SK.GetEntity(hr.entity(i+1))->PointForceTo(pt);
        }
        if(hasDistance) {
            SK.GetEntity(hr.entity(64))->DistanceForceTo(cr->distance);
        }

        cr->newReq = hr;
        ToggleSelectionStateOf(hr.entity(0));
    }
    
    Constraint *c;
    for(c = SS.clipboard.c.First(); c; c = SS.clipboard.c.NextAfter(c)) {
        if(c->type == Constraint::POINTS_COINCIDENT) {
            Constraint::ConstrainCoincident(SS.clipboard.NewEntityFor(c->ptA),
                                            SS.clipboard.NewEntityFor(c->ptB));
        }
    }

    SS.later.generateAll = true;
}

void GraphicsWindow::MenuClipboard(int id) {
    if(id != MNU_DELETE && !SS.GW.LockedInWorkplane()) {
        Error("Cut, paste, and copy work only in a workplane.\r\n\r\n"
              "Select one with Sketch -> In Workplane.");
        return;
    }

    switch(id) {
        case MNU_PASTE: {   
            Vector trans = SS.GW.projRight.ScaledBy(80/SS.GW.scale).Plus(
                           SS.GW.projUp   .ScaledBy(40/SS.GW.scale));
            SS.GW.PasteClipboard(trans, 0, false);
            break;
        }

        case MNU_PASTE_TRANSFORM:
            break;

        case MNU_COPY:
            SS.GW.CopySelection();
            SS.GW.ClearSelection();
            break;

        case MNU_CUT:
            SS.UndoRemember();
            SS.GW.CopySelection();
            SS.GW.DeleteSelection();
            break;

        case MNU_DELETE:
            SS.UndoRemember();
            SS.GW.DeleteSelection();
            break;

        default: oops();
    }
}

