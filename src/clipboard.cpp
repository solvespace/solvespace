//-----------------------------------------------------------------------------
// The clipboard that gets manipulated when the user selects Edit -> Cut,
// Copy, Paste, etc.; may contain entities only, not constraints.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void SolveSpaceUI::Clipboard::Clear() {
    c.Clear();
    r.Clear();
}

bool SolveSpaceUI::Clipboard::ContainsEntity(hEntity he) {
    if(he == Entity::NO_ENTITY)
        return true;

    ClipboardRequest *cr;
    for(cr = r.First(); cr; cr = r.NextAfter(cr)) {
        if(cr->oldEnt == he)
            return true;

        for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
            if(cr->oldPointEnt[i] == he)
                return true;
        }
    }
    return false;
}

hEntity SolveSpaceUI::Clipboard::NewEntityFor(hEntity he) {
    if(he == Entity::NO_ENTITY)
        return Entity::NO_ENTITY;

    ClipboardRequest *cr;
    for(cr = r.First(); cr; cr = r.NextAfter(cr)) {
        if(cr->oldEnt == he)
            return cr->newReq.entity(0);

        for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
            if(cr->oldPointEnt[i] == he)
                return cr->newReq.entity(1+i);
        }
    }

    ssassert(false, "Expected to find entity in some clipboard request");
}

void GraphicsWindow::DeleteSelection() {
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

void GraphicsWindow::CopySelection() {
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
        Request::Type req;
        int pts;
        if(!EntReqTable::GetEntityInfo(e->type, e->extraPoints,
                &req, &pts, NULL, &hasDistance))
        {
            if(!e->h.isFromRequest()) continue;
            Request *r = SK.GetRequest(e->h.request());
            if(r->type != Request::Type::DATUM_POINT) continue;
            EntReqTable::GetEntityInfo((Entity::Type)0, e->extraPoints,
                &req, &pts, NULL, &hasDistance);
        }
        if(req == Request::Type::WORKPLANE) continue;

        ClipboardRequest cr = {};
        cr.type         = req;
        cr.extraPoints  = e->extraPoints;
        cr.style        = e->style;
        cr.str          = e->str;
        cr.font         = e->font;
        cr.file         = e->file;
        cr.construction = e->construction;
        {for(int i = 0; i < pts; i++) {
            Vector pt;
            if(req == Request::Type::DATUM_POINT) {
                pt = e->PointGetNum();
            } else {
                pt = SK.GetEntity(e->point[i])->PointGetNum();
            }
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

    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        if(!s->constraint.v) continue;

        Constraint *c = SK.GetConstraint(s->constraint);
        if(c->type == Constraint::Type::COMMENT) {
            SS.clipboard.c.Add(c);
        }
    }

    for(Constraint &c : SK.constraint) {
        if(!SS.clipboard.ContainsEntity(c.ptA) ||
           !SS.clipboard.ContainsEntity(c.ptB) ||
           !SS.clipboard.ContainsEntity(c.entityA) ||
           !SS.clipboard.ContainsEntity(c.entityB) ||
           !SS.clipboard.ContainsEntity(c.entityC) ||
           !SS.clipboard.ContainsEntity(c.entityD) ||
           c.type == Constraint::Type::COMMENT) {
            continue;
        }
        SS.clipboard.c.Add(&c);
    }
}

void GraphicsWindow::PasteClipboard(Vector trans, double theta, double scale) {
    Entity *wrkpl  = SK.GetEntity(ActiveWorkplane());
    Entity *wrkpln = SK.GetEntity(wrkpl->normal);
    Vector u = wrkpln->NormalU(),
           v = wrkpln->NormalV(),
           n = wrkpln->NormalN(),
           p = SK.GetEntity(wrkpl->point[0])->PointGetNum();

    // For arcs, reflection involves swapping the endpoints, or otherwise
    // the arc gets inverted.
    auto mapPoint = [scale](hEntity he) {
        if(he.v == 0) return he;

        if(scale < 0) {
            hRequest hr = he.request();
            Request *r = SK.GetRequest(hr);
            if(r->type == Request::Type::ARC_OF_CIRCLE) {
                if(he == hr.entity(2)) {
                    return hr.entity(3);
                } else if(he == hr.entity(3)) {
                    return hr.entity(2);
                }
            }
        }
        return he;
    };

    ClipboardRequest *cr;
    for(cr = SS.clipboard.r.First(); cr; cr = SS.clipboard.r.NextAfter(cr)) {
        hRequest hr = AddRequest(cr->type, /*rememberForUndo=*/false);
        Request *r = SK.GetRequest(hr);
        r->extraPoints  = cr->extraPoints;
        r->style        = cr->style;
        r->str          = cr->str;
        r->font         = cr->font;
        r->file         = cr->file;
        r->construction = cr->construction;
        // Need to regen to get the right number of points, if extraPoints
        // changed.
        SS.GenerateAll(SolveSpaceUI::Generate::REGEN);
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
            int j = (r->type == Request::Type::DATUM_POINT) ? i : i + 1;
            SK.GetEntity(mapPoint(hr.entity(j)))->PointForceTo(pt);
        }
        if(hasDistance) {
            SK.GetEntity(hr.entity(64))->DistanceForceTo(
                                            cr->distance*fabs(scale));
        }

        cr->newReq = hr;
        MakeSelected(hr.entity(0));
        for(i = 0; i < pts; i++) {
            int j = (r->type == Request::Type::DATUM_POINT) ? i : i + 1;
            MakeSelected(hr.entity(j));
        }
    }
    Constraint *cc;
    for(cc = SS.clipboard.c.First(); cc; cc = SS.clipboard.c.NextAfter(cc)) {
        Constraint c = {};
        c.group = SS.GW.activeGroup;
        c.workplane = SS.GW.ActiveWorkplane();
        c.type = cc->type;
        c.valA = cc->valA;
        c.ptA = SS.clipboard.NewEntityFor(mapPoint(cc->ptA));
        c.ptB = SS.clipboard.NewEntityFor(mapPoint(cc->ptB));
        c.entityA = SS.clipboard.NewEntityFor(cc->entityA);
        c.entityB = SS.clipboard.NewEntityFor(cc->entityB);
        c.entityC = SS.clipboard.NewEntityFor(cc->entityC);
        c.entityD = SS.clipboard.NewEntityFor(cc->entityD);
        c.other = cc->other;
        c.other2 = cc->other2;
        c.reference = cc->reference;
        c.disp = cc->disp;
        c.comment = cc->comment;
        bool dontAddConstraint = false;
        switch(c.type) {
            case Constraint::Type::COMMENT:
                c.disp.offset = c.disp.offset.Plus(trans);
                break;
            case Constraint::Type::PT_PT_DISTANCE:
            case Constraint::Type::PT_LINE_DISTANCE:
            case Constraint::Type::PROJ_PT_DISTANCE:
            case Constraint::Type::DIAMETER:
                c.valA *= fabs(scale);
                break;
            case Constraint::Type::ARC_LINE_TANGENT: {
                Entity *line = SK.GetEntity(c.entityB),
                       *arc  = SK.GetEntity(c.entityA);
                if(line->type == Entity::Type::ARC_OF_CIRCLE) {
                    swap(line, arc);
                }
                Constraint::ConstrainArcLineTangent(&c, line, arc);
                break;
            }
            case Constraint::Type::CUBIC_LINE_TANGENT: {
                Entity *line  = SK.GetEntity(c.entityB),
                       *cubic = SK.GetEntity(c.entityA);
                if(line->type == Entity::Type::CUBIC) {
                    swap(line, cubic);
                }
                Constraint::ConstrainCubicLineTangent(&c, line, cubic);
                break;
            }
            case Constraint::Type::CURVE_CURVE_TANGENT: {
                Entity *eA = SK.GetEntity(c.entityA),
                       *eB = SK.GetEntity(c.entityB);
                Constraint::ConstrainCurveCurveTangent(&c, eA, eB);
                break;
            }
            case Constraint::Type::HORIZONTAL:
            case Constraint::Type::VERTICAL:
                // When rotating 90 or 270 degrees, swap the vertical / horizontal constraints
                if (EXACT(fmod(theta + (PI/2), PI) == 0)) {
                    if(c.type == Constraint::Type::HORIZONTAL) {
                        c.type = Constraint::Type::VERTICAL;
                    } else {
                        c.type = Constraint::Type::HORIZONTAL;
                    }
                } else if (fmod(theta, PI/2) != 0) {
                    dontAddConstraint = true;
                }
                break;
            default:
                break;
        }
        if (!dontAddConstraint) {
            hConstraint hc = Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
            if(c.type == Constraint::Type::COMMENT) {
                MakeSelected(hc);
            }
        }
    }
}

void GraphicsWindow::MenuClipboard(Command id) {
    if(id != Command::DELETE && !SS.GW.LockedInWorkplane()) {
        Error(_("Cut, paste, and copy work only in a workplane.\n\n"
                "Activate one with Sketch -> In Workplane."));
        return;
    }

    switch(id) {
        case Command::PASTE: {
            SS.UndoRemember();
            Vector trans = SS.GW.projRight.ScaledBy(80/SS.GW.scale).Plus(
                           SS.GW.projUp   .ScaledBy(40/SS.GW.scale));
            SS.GW.ClearSelection();
            SS.GW.PasteClipboard(trans, 0, 1);
            break;
        }

        case Command::PASTE_TRANSFORM: {
            if(SS.clipboard.r.IsEmpty()) {
                Error(_("Clipboard is empty; nothing to paste."));
                break;
            }

            Entity *wrkpl  = SK.GetEntity(SS.GW.ActiveWorkplane());
            Vector p = SK.GetEntity(wrkpl->point[0])->PointGetNum();
            SS.TW.shown.paste.times  = 1;
            SS.TW.shown.paste.trans  = Vector::From(0, 0, 0);
            SS.TW.shown.paste.theta  = 0;
            SS.TW.shown.paste.origin = p;
            SS.TW.shown.paste.scale  = 1;
            SS.TW.GoToScreen(TextWindow::Screen::PASTE_TRANSFORMED);
            SS.GW.ForceTextWindowShown();
            SS.ScheduleShowTW();
            break;
        }

        case Command::COPY:
            SS.GW.CopySelection();
            SS.GW.ClearSelection();
            break;

        case Command::CUT:
            SS.UndoRemember();
            SS.GW.CopySelection();
            SS.GW.DeleteSelection();
            break;

        case Command::DELETE:
            SS.UndoRemember();
            SS.GW.DeleteSelection();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

bool TextWindow::EditControlDoneForPaste(const std::string &s) {
    Expr *e;
    switch(edit.meaning) {
        case Edit::PASTE_TIMES_REPEATED: {
            e = Expr::From(s, /*popUpError=*/true);
            if(!e) break;
            int v = (int)e->Eval();
            if(v > 0) {
                shown.paste.times = v;
            } else {
                Error(_("Number of copies to paste must be at least one."));
            }
            break;
        }
        case Edit::PASTE_ANGLE:
            e = Expr::From(s, /*popUpError=*/true);
            if(!e) break;
            shown.paste.theta = WRAP_SYMMETRIC((e->Eval())*PI/180, 2*PI);
            break;

        case Edit::PASTE_SCALE: {
            e = Expr::From(s, /*popUpError=*/true);
            double v = e->Eval();
            if(fabs(v) > 1e-6) {
                shown.paste.scale = shown.paste.scale < 0 ? -v : v;
            } else {
                Error(_("Scale cannot be zero."));
            }
            break;
        }

        default:
            return false;
    }
    return true;
}

void TextWindow::ScreenChangePasteTransformed(int link, uint32_t v) {
    switch(link) {
        case 't':
            SS.TW.ShowEditControl(13, ssprintf("%d", SS.TW.shown.paste.times));
            SS.TW.edit.meaning = Edit::PASTE_TIMES_REPEATED;
            break;

        case 'r':
            SS.TW.ShowEditControl(13, ssprintf("%.3f", SS.TW.shown.paste.theta*180/PI));
            SS.TW.edit.meaning = Edit::PASTE_ANGLE;
            break;

        case 's':
            SS.TW.ShowEditControl(13, ssprintf("%.3f", fabs(SS.TW.shown.paste.scale)));
            SS.TW.edit.meaning = Edit::PASTE_SCALE;
            break;

        case 'f':
            SS.TW.shown.paste.scale *= -1;
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
                Error(_("Select one point to define origin of rotation."));
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
                Error(_("Select two points to define translation vector."));
            }
            SS.GW.ClearSelection();
            break;

        case 'g': {
            if(fabs(SS.TW.shown.paste.theta) < LENGTH_EPS &&
               SS.TW.shown.paste.trans.Magnitude() < LENGTH_EPS &&
               SS.TW.shown.paste.times != 1)
            {
                Message(_("Transformation is identity. So all copies will be "
                          "exactly on top of each other."));
            }
            if(SS.TW.shown.paste.times*SS.clipboard.r.n > 100) {
                Error(_("Too many items to paste; split this into smaller "
                        "pastes."));
                break;
            }
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("No workplane active."));
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
            SS.TW.GoToScreen(Screen::LIST_OF_GROUPS);
            SS.ScheduleShowTW();
            break;
        }
    }
}

void TextWindow::ShowPasteTransformed() {
    Printf(true, "%FtPASTE TRANSFORMED%E");
    Printf(true,  "%Ba   %Ftrepeat%E    %d time%s %Fl%Lt%f[change]%E",
        shown.paste.times, (shown.paste.times == 1) ? "" : "s",
        &ScreenChangePasteTransformed);
    Printf(false, "%Bd   %Ftrotate%E    %@ degrees %Fl%Lr%f[change]%E",
        shown.paste.theta*180/PI,
        &ScreenChangePasteTransformed);
    Printf(false, "%Ba   %Ftabout pt%E  (%s, %s, %s) %Fl%Lo%f[use selected]%E",
            SS.MmToString(shown.paste.origin.x).c_str(),
            SS.MmToString(shown.paste.origin.y).c_str(),
            SS.MmToString(shown.paste.origin.z).c_str(),
        &ScreenPasteTransformed);
    Printf(false, "%Bd   %Fttranslate%E (%s, %s, %s) %Fl%Lt%f[use selected]%E",
            SS.MmToString(shown.paste.trans.x).c_str(),
            SS.MmToString(shown.paste.trans.y).c_str(),
            SS.MmToString(shown.paste.trans.z).c_str(),
        &ScreenPasteTransformed);
    Printf(false, "%Ba   %Ftscale%E     %@ %Fl%Ls%f[change]%E",
        fabs(shown.paste.scale),
        &ScreenChangePasteTransformed);
    Printf(false, "%Ba   %Ftmirror%E    %Fd%Lf%f%s  flip%E",
        &ScreenChangePasteTransformed,
        shown.paste.scale < 0 ? CHECK_TRUE : CHECK_FALSE);

    Printf(true, " %Fl%Lg%fpaste transformed now%E", &ScreenPasteTransformed);

    Printf(true, "(or %Fl%Ll%fcancel operation%E)", &ScreenHome);
}

