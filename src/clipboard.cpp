//-----------------------------------------------------------------------------
// The clipboard that gets manipulated when the user selects Edit -> Cut,
// Copy, Paste, etc.; may contain entities only, not constraints.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpaceUI::Clipboard::Clear(void) {
    c.Clear();
    r.Clear();
}

hEntity SolveSpaceUI::Clipboard::NewEntityFor(hEntity he) {
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

bool SolveSpaceUI::Clipboard::ContainsEntity(hEntity he) {
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

        ClipboardRequest cr = {};
        cr.type         = req;
        cr.extraPoints  = e->extraPoints;
        cr.style        = e->style;
        cr.str.strcpy(    e->str.str);
        cr.font.strcpy(   e->font.str);
        cr.construction = e->construction;
        {for(int i = 0; i < pts; i++) {
            Vector pt = SK.GetEntity(e->point[i])->PointGetNum();
            pt = pt.Minus(p);
            pt = pt.DotInToCsys(u, v, n);
            cr.point[i] = pt;
        }}
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

void GraphicsWindow::PasteClipboard(Vector trans, double theta, double scale) {
    Entity *wrkpl  = SK.GetEntity(ActiveWorkplane());
    Entity *wrkpln = SK.GetEntity(wrkpl->normal);
    Vector u = wrkpln->NormalU(),
           v = wrkpln->NormalV(),
           n = wrkpln->NormalN(),
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
        int i, pts;
        EntReqTable::GetRequestInfo(r->type, r->extraPoints,
            NULL, &pts, NULL, &hasDistance);
        for(i = 0; i < pts; i++) {
            Vector pt = cr->point[i];
            // We need the reflection to occur within the workplane; it may
            // otherwise correspond to just a rotation as projected.
            if(scale < 0) {
                pt.x *= -1;
            }
            // Likewise the scale, which could otherwise take us out of the
            // workplane.
            pt = pt.ScaledBy(fabs(scale));
            pt = pt.ScaleOutOfCsys(u, v, Vector::From(0, 0, 0));
            pt = pt.Plus(p);
            pt = pt.RotatedAbout(n, theta);
            pt = pt.Plus(trans);
            SK.GetEntity(hr.entity(i+1))->PointForceTo(pt);
        }
        if(hasDistance) {
            SK.GetEntity(hr.entity(64))->DistanceForceTo(
                                            cr->distance*fabs(scale));
        }

        cr->newReq = hr;
        MakeSelected(hr.entity(0));
        for(i = 0; i < pts; i++) {
            MakeSelected(hr.entity(i+1));
        }
    }

    Constraint *c;
    for(c = SS.clipboard.c.First(); c; c = SS.clipboard.c.NextAfter(c)) {
        if(c->type == Constraint::POINTS_COINCIDENT) {
            Constraint::ConstrainCoincident(SS.clipboard.NewEntityFor(c->ptA),
                                            SS.clipboard.NewEntityFor(c->ptB));
        }
    }

    SS.ScheduleGenerateAll();
}

void GraphicsWindow::MenuClipboard(int id) {
    if(id != MNU_DELETE && !SS.GW.LockedInWorkplane()) {
        Error("Cut, paste, and copy work only in a workplane.\n\n"
              "Select one with Sketch -> In Workplane.");
        return;
    }

    switch(id) {
        case MNU_PASTE: {
            SS.UndoRemember();
            Vector trans = SS.GW.projRight.ScaledBy(80/SS.GW.scale).Plus(
                           SS.GW.projUp   .ScaledBy(40/SS.GW.scale));
            SS.GW.ClearSelection();
            SS.GW.PasteClipboard(trans, 0, 1);
            break;
        }

        case MNU_PASTE_TRANSFORM: {
            if(SS.clipboard.r.n == 0) {
                Error("Clipboard is empty; nothing to paste.");
                break;
            }

            Entity *wrkpl  = SK.GetEntity(SS.GW.ActiveWorkplane());
            Vector p = SK.GetEntity(wrkpl->point[0])->PointGetNum();
            SS.TW.shown.paste.times  = 1;
            SS.TW.shown.paste.trans  = Vector::From(0, 0, 0);
            SS.TW.shown.paste.theta  = 0;
            SS.TW.shown.paste.origin = p;
            SS.TW.shown.paste.scale  = 1;
            SS.TW.GoToScreen(TextWindow::SCREEN_PASTE_TRANSFORMED);
            SS.GW.ForceTextWindowShown();
            SS.ScheduleShowTW();
            break;
        }

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

bool TextWindow::EditControlDoneForPaste(const char *s) {
    Expr *e;
    switch(edit.meaning) {
        case EDIT_PASTE_TIMES_REPEATED: {
            e = Expr::From(s, true);
            if(!e) break;
            int v = (int)e->Eval();
            if(v > 0) {
                shown.paste.times = v;
            } else {
                Error("Number of copies to paste must be at least one.");
            }
            break;
        }
        case EDIT_PASTE_ANGLE:
            e = Expr::From(s, true);
            if(!e) break;
            shown.paste.theta = WRAP_SYMMETRIC((e->Eval())*PI/180, 2*PI);
            break;

        case EDIT_PASTE_SCALE: {
            e = Expr::From(s, true);
            double v = e->Eval();
            if(fabs(v) > 1e-6) {
                shown.paste.scale = v;
            } else {
                Error("Scale cannot be zero.");
            }
            break;
        }

        default:
            return false;
    }
    return true;
}

void TextWindow::ScreenChangePasteTransformed(int link, uint32_t v) {
    char str[300];
    switch(link) {
        case 't':
            sprintf(str, "%d", SS.TW.shown.paste.times);
            SS.TW.ShowEditControl(10, 13, str);
            SS.TW.edit.meaning = EDIT_PASTE_TIMES_REPEATED;
            break;

        case 'r':
            sprintf(str, "%.3f", SS.TW.shown.paste.theta*180/PI);
            SS.TW.ShowEditControl(12, 13, str);
            SS.TW.edit.meaning = EDIT_PASTE_ANGLE;
            break;

        case 's':
            sprintf(str, "%.3f", SS.TW.shown.paste.scale);
            SS.TW.ShowEditControl(18, 13, str);
            SS.TW.edit.meaning = EDIT_PASTE_SCALE;
            break;
    }
}

void TextWindow::ScreenPasteTransformed(int link, uint32_t v) {
    SS.GW.GroupSelection();
    switch(link) {
        case 'o':
            if(SS.GW.gs.points == 1 && SS.GW.gs.n == 1) {
                Entity *e = SK.GetEntity(SS.GW.gs.point[0]);
                SS.TW.shown.paste.origin = e->PointGetNum();
            } else {
                Error("Select one point to define origin of rotation.");
            }
            SS.GW.ClearSelection();
            break;

        case 't':
            if(SS.GW.gs.points == 2 && SS.GW.gs.n == 2) {
                Entity *pa = SK.GetEntity(SS.GW.gs.point[0]),
                       *pb = SK.GetEntity(SS.GW.gs.point[1]);
                SS.TW.shown.paste.trans =
                    (pb->PointGetNum()).Minus(pa->PointGetNum());
            } else {
                Error("Select two points to define translation vector.");
            }
            SS.GW.ClearSelection();
            break;

        case 'g': {
            if(fabs(SS.TW.shown.paste.theta) < LENGTH_EPS &&
               SS.TW.shown.paste.trans.Magnitude() < LENGTH_EPS &&
               SS.TW.shown.paste.times != 1)
            {
                Message("Transformation is identity. So all copies will be "
                        "exactly on top of each other.");
            }
            if(SS.TW.shown.paste.times*SS.clipboard.r.n > 100) {
                Error("Too many items to paste; split this into smaller "
                      "pastes.");
                break;
            }
            if(!SS.GW.LockedInWorkplane()) {
                Error("No workplane active.");
                break;
            }
            Entity *wrkpl  = SK.GetEntity(SS.GW.ActiveWorkplane());
            Entity *wrkpln = SK.GetEntity(wrkpl->normal);
            Vector wn = wrkpln->NormalN();
            SS.UndoRemember();
            SS.GW.ClearSelection();
            for(int i = 0; i < SS.TW.shown.paste.times; i++) {
                Vector trans  = SS.TW.shown.paste.trans.ScaledBy(i+1),
                       origin = SS.TW.shown.paste.origin;
                double theta = SS.TW.shown.paste.theta*(i+1);
                // desired transformation is Q*(p - o) + o + t =
                // Q*p - Q*o + o + t = Q*p + (t + o - Q*o)
                Vector t = trans.Plus(
                           origin).Minus(
                           origin.RotatedAbout(wn, theta));

                SS.GW.PasteClipboard(t, theta, SS.TW.shown.paste.scale);
            }
            SS.TW.GoToScreen(SCREEN_LIST_OF_GROUPS);
            SS.ScheduleShowTW();
            break;
        }
    }
}

void TextWindow::ShowPasteTransformed(void) {
    Printf(true, "%FtPASTE TRANSFORMED%E");
    Printf(true,  "%Ba   %Ftrepeat%E    %d time%s %Fl%Lt%f[change]%E",
        shown.paste.times, (shown.paste.times == 1) ? "" : "s",
        &ScreenChangePasteTransformed);
    Printf(false, "%Bd   %Ftrotate%E    %@ degrees %Fl%Lr%f[change]%E",
        shown.paste.theta*180/PI,
        &ScreenChangePasteTransformed);
    Printf(false, "%Ba   %Ftabout pt%E  (%s, %s, %s) %Fl%Lo%f[use selected]%E",
            SS.MmToString(shown.paste.origin.x),
            SS.MmToString(shown.paste.origin.y),
            SS.MmToString(shown.paste.origin.z),
        &ScreenPasteTransformed);
    Printf(false, "%Bd   %Fttranslate%E (%s, %s, %s) %Fl%Lt%f[use selected]%E",
            SS.MmToString(shown.paste.trans.x),
            SS.MmToString(shown.paste.trans.y),
            SS.MmToString(shown.paste.trans.z),
        &ScreenPasteTransformed);
    Printf(false, "%Ba   %Ftscale%E     %@ %Fl%Ls%f[change]%E",
        shown.paste.scale,
        &ScreenChangePasteTransformed);

    Printf(true, " %Fl%Lg%fpaste transformed now%E", &ScreenPasteTransformed);

    Printf(true, "(or %Fl%Ll%fcancel operation%E)", &ScreenHome);
}

