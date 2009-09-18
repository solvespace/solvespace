#include "solvespace.h"
#include <stdarg.h>

const TextWindow::Color TextWindow::fgColors[] = {
    { 'd', RGB(255, 255, 255) },
    { 'l', RGB(100, 100, 255) },
    { 't', RGB(255, 200,   0) },
    { 'h', RGB( 90,  90,  90) },
    { 's', RGB( 40, 255,  40) },
    { 'm', RGB(200, 200,   0) },
    { 'r', RGB(  0,   0,   0) },
    { 'x', RGB(255,  20,  20) },
    { 'i', RGB(  0, 255, 255) },
    { 'g', RGB(160, 160, 160) },
    { 0, 0 },
};
const TextWindow::Color TextWindow::bgColors[] = {
    { 'd', RGB(  0,   0,   0) },
    { 't', RGB( 34,  15,  15) },
    { 'a', RGB( 20,  20,  20) },
    { 'r', RGB(255, 255, 255) },
    { 0, 0 },
};

void TextWindow::Init(void) {
    ClearSuper();
}

void TextWindow::ClearSuper(void) {
    HideTextEditControl();
    memset(this, 0, sizeof(*this));
    ClearScreen();
    Show();
}

void TextWindow::ClearScreen(void) {
    int i, j;
    for(i = 0; i < MAX_ROWS; i++) {
        for(j = 0; j < MAX_COLS; j++) {
            text[i][j] = ' ';
            meta[i][j].fg = 'd';
            meta[i][j].bg = 'd';
            meta[i][j].link = NOT_A_LINK;
        }
        top[i] = i*2;
    }
    rows = 0;
}

void TextWindow::Printf(bool halfLine, char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    if(rows >= MAX_ROWS) return;

    int r, c;
    r = rows;
    top[r] = (r == 0) ? 0 : (top[r-1] + (halfLine ? 3 : 2));
    rows++;

    for(c = 0; c < MAX_COLS; c++) {
        text[r][c] = ' ';
        meta[r][c].link = NOT_A_LINK;
    }

    int fg = 'd', bg = 'd';
    int link = NOT_A_LINK;
    DWORD data = 0;
    LinkFunction *f = NULL, *h = NULL;

    c = 0;
    while(*fmt) {
        char buf[1024];

        if(*fmt == '%') {
            fmt++;
            if(*fmt == '\0') goto done;
            strcpy(buf, "");
            switch(*fmt) {
                case 'd': {
                    int v = va_arg(vl, int);
                    sprintf(buf, "%d", v);
                    break;
                }
                case 'x': {
                    DWORD v = va_arg(vl, DWORD);
                    sprintf(buf, "%08x", v);
                    break;
                }
                case '@': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.2f", v);
                    break;
                }
                case '2': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.2f", v < 0 ? "" : " ", v);
                    break;
                }
                case '3': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.3f", v < 0 ? "" : " ", v);
                    break;
                }
                case 's': {
                    char *s = va_arg(vl, char *);
                    memcpy(buf, s, min(sizeof(buf), strlen(s)+1));
                    break;
                }
                case 'c': {
                    char v = va_arg(vl, char);
                    sprintf(buf, "%c", v);
                    break;
                }
                case 'E':
                    fg = 'd';
                    // leave the background, though
                    link = NOT_A_LINK;
                    data = 0;
                    f = NULL;
                    h = NULL;
                    break;

                case 'F':
                case 'B': {
                    int color;
                    if(fmt[1] == '\0') goto done;
                    if(fmt[1] == 'p') {
                        color = va_arg(vl, int);
                    } else {
                        color = fmt[1];
                    }
                    if((color < 0 || color > 255) && !(color & 0x80000000)) {
                        color = 0;
                    }
                    if(*fmt == 'F') {
                        fg = color;
                    } else {
                        bg = color;
                    }
                    fmt++;
                    break;
                }
                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    link = *fmt;
                    break;

                case 'f':
                    f = va_arg(vl, LinkFunction *);
                    break;

                case 'h':
                    h = va_arg(vl, LinkFunction *);
                    break;

                case 'D':
                    data = va_arg(vl, DWORD);
                    break;
                    
                case '%':
                    strcpy(buf, "%");
                    break;
            }
        } else {
            buf[0] = *fmt;
            buf[1]= '\0';
        }

        for(unsigned i = 0; i < strlen(buf); i++) {
            if(c >= MAX_COLS) goto done;
            text[r][c] = buf[i];
            meta[r][c].fg = fg;
            meta[r][c].bg = bg;
            meta[r][c].link = link;
            meta[r][c].data = data;
            meta[r][c].f = f;
            meta[r][c].h = h;
            c++;
        }

        fmt++;
    }
    while(c < MAX_COLS) {
        meta[r][c].fg = fg;
        meta[r][c].bg = bg;
        c++;
    }

done:
    va_end(vl);
}

#define gs (SS.GW.gs)
void TextWindow::Show(void) {
    if(!(SS.GW.pending.operation)) SS.GW.ClearPending();

    SS.GW.GroupSelection();

    if(SS.GW.pending.description) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        HideTextEditControl();
        ShowHeader(false);
        Printf(false, "");
        Printf(false, "%s", SS.GW.pending.description);
        Printf(true, "%Fl%f%Ll(cancel operation)%E",
            &TextWindow::ScreenUnselectAll);
    } else if(gs.n > 0) {
        if(edit.meaning != EDIT_TTF_TEXT) HideTextEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        if(edit.meaning == EDIT_TTF_TEXT) HideTextEditControl();
        ShowHeader(true);
        switch(shown.screen) {
            default:
                shown.screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case SCREEN_CONFIGURATION:      ShowConfiguration();    break;
            case SCREEN_STEP_DIMENSION:     ShowStepDimension();    break;
            case SCREEN_MESH_VOLUME:        ShowMeshVolume();       break;
            case SCREEN_LIST_OF_STYLES:     ShowListOfStyles();     break;
            case SCREEN_STYLE_INFO:         ShowStyleInfo();        break;
        }
    }
    Printf(false, "");
    InvalidateText();
}

void TextWindow::ScreenUnselectAll(int link, DWORD v) {
    GraphicsWindow::MenuEdit(GraphicsWindow::MNU_UNSELECT_ALL);
}

void TextWindow::ScreenEditTtfText(int link, DWORD v) {
    hRequest hr = { v };
    Request *r = SK.GetRequest(hr);

    ShowTextEditControl(13, 10, r->str.str);
    SS.TW.edit.meaning = EDIT_TTF_TEXT;
    SS.TW.edit.request = hr;
}

void TextWindow::ScreenSetTtfFont(int link, DWORD v) {
    int i = (int)v;
    if(i < 0) return;
    if(i >= SS.fonts.l.n) return;

    SS.GW.GroupSelection();
    if(gs.entities != 1 || gs.n != 1) return;

    Entity *e = SK.entity.FindByIdNoOops(gs.entity[0]);
    if(!e || e->type != Entity::TTF_TEXT || !e->h.isFromRequest()) return;

    Request *r = SK.request.FindByIdNoOops(e->h.request());
    if(!r) return;
   
    SS.UndoRemember();
    r->font.strcpy(SS.fonts.l.elem[i].FontFileBaseName());
    SS.MarkGroupDirty(r->group);
    SS.later.generateAll = true;
    SS.later.showTW = true;
}

void TextWindow::DescribeSelection(void) {
    Entity *e;
    Vector p;
    int i;
    Printf(false, "");

    if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        e = SK.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]);

#define COSTR(p) \
    SS.MmToString((p).x), SS.MmToString((p).y), SS.MmToString((p).z)
#define PT_AS_STR "(%Fi%s%E, %Fi%s%E, %Fi%s%E)"
#define PT_AS_NUM "(%Fi%3%E, %Fi%3%E, %Fi%3%E)"
        switch(e->type) {
            case Entity::POINT_IN_3D:
            case Entity::POINT_IN_2D:
            case Entity::POINT_N_TRANS:
            case Entity::POINT_N_ROT_TRANS:
            case Entity::POINT_N_COPY:
            case Entity::POINT_N_ROT_AA:
                p = e->PointGetNum();
                Printf(false, "%FtPOINT%E at " PT_AS_STR, COSTR(p));
                break;

            case Entity::NORMAL_IN_3D:
            case Entity::NORMAL_IN_2D:
            case Entity::NORMAL_N_COPY:
            case Entity::NORMAL_N_ROT:
            case Entity::NORMAL_N_ROT_AA: {
                Quaternion q = e->NormalGetNum();
                p = q.RotationN();
                Printf(false, "%FtNORMAL / COORDINATE SYSTEM%E");
                Printf(true,  "  basis n = " PT_AS_NUM, CO(p));
                p = q.RotationU();
                Printf(false, "        u = " PT_AS_NUM, CO(p));
                p = q.RotationV();
                Printf(false, "        v = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::WORKPLANE: {
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(false, "%FtWORKPLANE%E");
                Printf(true, "   origin = " PT_AS_STR, COSTR(p));
                Quaternion q = e->Normal()->NormalGetNum();
                p = q.RotationN();
                Printf(true, "   normal = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::LINE_SEGMENT: {
                Vector p0 = SK.GetEntity(e->point[0])->PointGetNum();
                p = p0;
                Printf(false, "%FtLINE SEGMENT%E");
                Printf(true,  "   thru " PT_AS_STR, COSTR(p));
                Vector p1 = SK.GetEntity(e->point[1])->PointGetNum();
                p = p1;
                Printf(false, "        " PT_AS_STR, COSTR(p));
                Printf(true,  "   len = %Fi%s%E",
                    SS.MmToString((p1.Minus(p0).Magnitude())));
                break;
            }
            case Entity::CUBIC:
                Printf(false, "%FtCUBIC BEZIER CURVE%E");
                for(i = 0; i <= 3; i++) {
                    p = SK.GetEntity(e->point[i])->PointGetNum();
                    Printf((i==0), "   p%c = " PT_AS_STR, '0'+i, COSTR(p));
                }
                break;

            case Entity::ARC_OF_CIRCLE: {
                Printf(false, "%FtARC OF A CIRCLE%E");
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(true,  "     center = " PT_AS_STR, COSTR(p));
                p = SK.GetEntity(e->point[1])->PointGetNum();
                Printf(true,  "  endpoints = " PT_AS_STR, COSTR(p));
                p = SK.GetEntity(e->point[2])->PointGetNum();
                Printf(false, "              " PT_AS_STR, COSTR(p));
                double r = e->CircleGetRadiusNum();
                Printf(true, "   diameter =  %Fi%s", SS.MmToString(r*2));
                Printf(false, "     radius =  %Fi%s", SS.MmToString(r));
                double thetas, thetaf, dtheta;
                e->ArcGetAngles(&thetas, &thetaf, &dtheta);
                Printf(false, "    arc len =  %Fi%s", SS.MmToString(dtheta*r));
                break;
            }
            case Entity::CIRCLE: {
                Printf(false, "%FtCIRCLE%E");
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(true,  "     center = " PT_AS_STR, COSTR(p));
                double r = e->CircleGetRadiusNum();
                Printf(true,  "   diameter =  %Fi%s", SS.MmToString(r*2));
                Printf(false, "     radius =  %Fi%s", SS.MmToString(r));
                break;
            }
            case Entity::FACE_NORMAL_PT:
            case Entity::FACE_XPROD:
            case Entity::FACE_N_ROT_TRANS:
            case Entity::FACE_N_ROT_AA:
            case Entity::FACE_N_TRANS:
                Printf(false, "%FtPLANE FACE%E");
                p = e->FaceGetNormalNum();
                Printf(true,  "   normal = " PT_AS_NUM, CO(p));
                p = e->FaceGetPointNum();
                Printf(false, "     thru = " PT_AS_STR, COSTR(p));
                break;

            case Entity::TTF_TEXT: {
                Printf(false, "%FtTRUETYPE FONT TEXT%E");
                Printf(true, "  font = '%Fi%s%E'", e->font.str);
                if(e->h.isFromRequest()) {
                    Printf(false, "  text = '%Fi%s%E' %Fl%Ll%f%D[change]%E",
                        e->str.str, &ScreenEditTtfText, e->h.request());
                    Printf(true, "  select new font");
                    SS.fonts.LoadAll();
                    int i;
                    for(i = 0; i < SS.fonts.l.n; i++) {
                        TtfFont *tf = &(SS.fonts.l.elem[i]);
                        if(strcmp(e->font.str, tf->FontFileBaseName())==0) {
                            Printf(false, "%Bp    %s",
                                (i & 1) ? 'd' : 'a',
                                tf->name.str);
                        } else {
                            Printf(false, "%Bp    %f%D%Fl%Ll%s%E%Bp",
                                (i & 1) ? 'd' : 'a',
                                &ScreenSetTtfFont, i,
                                tf->name.str,
                                (i & 1) ? 'd' : 'a');
                        }
                    }
                } else {
                    Printf(false, "  text = '%Fi%s%E'", e->str.str);
                }
                break;
            }

            default:
                Printf(true, "%Ft?? ENTITY%E");
                break;
        }

        Group *g = SK.GetGroup(e->group);
        Printf(false, "");
        Printf(false, "%FtIN GROUP%E      %s", g->DescriptionString());
        if(e->workplane.v == Entity::FREE_IN_3D.v) {
            Printf(false, "%FtNOT LOCKED IN WORKPLANE%E"); 
        } else {
            Entity *w = SK.GetEntity(e->workplane);
            Printf(false, "%FtIN WORKPLANE%E  %s", w->DescriptionString());
        }
    } else if(gs.n == 2 && gs.points == 2) {
        Printf(false, "%FtTWO POINTS");
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "   at " PT_AS_STR, COSTR(p0));
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "      " PT_AS_STR, COSTR(p1));
        double d = (p1.Minus(p0)).Magnitude();
        Printf(true, "  d = %Fi%s", SS.MmToString(d));
    } else if(gs.n == 2 && gs.faces == 1 && gs.points == 1) {
        Printf(false, "%FtA POINT AND A PLANE FACE");
        Vector pt = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "        point = " PT_AS_STR, COSTR(pt));
        Vector n = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " plane normal = " PT_AS_NUM, CO(n));
        Vector pl = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   plane thru = " PT_AS_STR, COSTR(pl));
        double dd = n.Dot(pl) - n.Dot(pt);
        Printf(true,  "     distance = %Fi%s", SS.MmToString(dd));
    } else if(gs.n == 3 && gs.points == 2 && gs.vectors == 1) {
        Printf(false, "%FtTWO POINTS AND A VECTOR");
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "  pointA = " PT_AS_STR, COSTR(p0));
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "  pointB = " PT_AS_STR, COSTR(p1));
        Vector v  = SK.GetEntity(gs.vector[0])->VectorGetNum();
        v = v.WithMagnitude(1);
        Printf(true,  "  vector = " PT_AS_NUM, CO(v));
        double d = (p1.Minus(p0)).Dot(v);
        Printf(true,  "  proj_d = %Fi%s", SS.MmToString(d));
    } else if(gs.n == 2 && gs.lineSegments == 1 && gs.points == 1) {
        Entity *ln = SK.GetEntity(gs.entity[0]);
        Vector lp0 = SK.GetEntity(ln->point[0])->PointGetNum(),
               lp1 = SK.GetEntity(ln->point[1])->PointGetNum();
        Printf(false, "%FtLINE SEGMENT AND POINT%E");
        Printf(true,  "   ln thru " PT_AS_STR, COSTR(lp0));
        Printf(false, "           " PT_AS_STR, COSTR(lp1));
        Vector pp = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "     point " PT_AS_STR, COSTR(pp));
        Printf(true,  " pt-ln distance = %Fi%s%E",
            SS.MmToString(pp.DistanceToLine(lp0, lp1.Minus(lp0))));
    } else if(gs.n == 2 && gs.faces == 2) {
        Printf(false, "%FtTWO PLANE FACES");

        Vector n0 = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " planeA normal = " PT_AS_NUM, CO(n0));
        Vector p0 = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   planeA thru = " PT_AS_STR, COSTR(p0));

        Vector n1 = SK.GetEntity(gs.face[1])->FaceGetNormalNum();
        Printf(true,  " planeB normal = " PT_AS_NUM, CO(n1));
        Vector p1 = SK.GetEntity(gs.face[1])->FaceGetPointNum();
        Printf(false, "   planeB thru = " PT_AS_STR, COSTR(p1));

        double theta = acos(n0.Dot(n1));
        Printf(true,  "         angle = %Fi%2%E degrees", theta*180/PI);
        while(theta < PI/2) theta += PI;
        while(theta > PI/2) theta -= PI; 
        Printf(false, "      or angle = %Fi%2%E (mod 180)", theta*180/PI);

        if(fabs(theta) < 0.01) {
            double d = (p1.Minus(p0)).Dot(n0);
            Printf(true,  "      distance = %Fi%s", SS.MmToString(d));
        }
    } else {
        Printf(true, "%FtSELECTED:%E %d item%s", gs.n, gs.n == 1 ? "" : "s");
    }

    Printf(true, "%Fl%f%Ll(unselect all)%E", &TextWindow::ScreenUnselectAll);
}

void TextWindow::GoToScreen(int screen) {
    shown.screen = screen;
}

