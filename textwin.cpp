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
    shown = &(showns[shownIndex]);
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
    } else if(gs.n > 0) {
        HideTextEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        ShowHeader(true);
        switch(shown->screen) {
            default:
                shown->screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case SCREEN_CONFIGURATION:      ShowConfiguration();    break;
        }
    }
    Printf(false, "");
    InvalidateText();
}

void TextWindow::ScreenUnselectAll(int link, DWORD v) {
    GraphicsWindow::MenuEdit(GraphicsWindow::MNU_UNSELECT_ALL);
}

void TextWindow::DescribeSelection(void) {
    Entity *e;
    Vector p;
    int i;
    Printf(false, "");

    if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        e = SS.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]);

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
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(false, "%FtWORKPLANE%E");
                Printf(true, "   origin = " PT_AS_STR, COSTR(p));
                Quaternion q = e->Normal()->NormalGetNum();
                p = q.RotationN();
                Printf(true, "   normal = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::LINE_SEGMENT: {
                Vector p0 = SS.GetEntity(e->point[0])->PointGetNum();
                p = p0;
                Printf(false, "%FtLINE SEGMENT%E");
                Printf(true,  "   thru " PT_AS_STR, COSTR(p));
                Vector p1 = SS.GetEntity(e->point[1])->PointGetNum();
                p = p1;
                Printf(false, "        " PT_AS_STR, COSTR(p));
                Printf(true,  "   len = %Fi%s%E",
                    SS.MmToString((p1.Minus(p0).Magnitude())));
                break;
            }
            case Entity::CUBIC:
                Printf(false, "%FtCUBIC BEZIER CURVE%E");
                for(i = 0; i <= 3; i++) {
                    p = SS.GetEntity(e->point[i])->PointGetNum();
                    Printf((i==0), "   p%c = " PT_AS_STR, '0'+i, COSTR(p));
                }
                break;

            case Entity::ARC_OF_CIRCLE: {
                Printf(false, "%FtARC OF A CIRCLE%E");
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(true,  "     center = " PT_AS_STR, COSTR(p));
                p = SS.GetEntity(e->point[1])->PointGetNum();
                Printf(true,  "  endpoints = " PT_AS_STR, COSTR(p));
                p = SS.GetEntity(e->point[2])->PointGetNum();
                Printf(false, "              " PT_AS_STR, COSTR(p));
                double r = e->CircleGetRadiusNum();
                Printf(true, "   diameter =  %Fi%s", SS.MmToString(r*2));
                Printf(false, "     radius =  %Fi%s", SS.MmToString(r));
                break;
            }
            case Entity::CIRCLE: {
                Printf(false, "%FtCIRCLE%E");
                p = SS.GetEntity(e->point[0])->PointGetNum();
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

            default:
                Printf(true, "%Ft?? ENTITY%E");
                break;
        }

        Group *g = SS.GetGroup(e->group);
        Printf(false, "");
        Printf(false, "%FtIN GROUP%E      %s", g->DescriptionString());
        if(e->workplane.v == Entity::FREE_IN_3D.v) {
            Printf(false, "%FtNO WORKPLANE (FREE IN 3D)%E"); 
        } else {
            Entity *w = SS.GetEntity(e->workplane);
            Printf(false, "%FtIN WORKPLANE%E  %s", w->DescriptionString());
        }
    } else if(gs.n == 2 && gs.points == 2) {
        Printf(false, "%FtTWO POINTS");
        Vector p0 = SS.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "   at " PT_AS_STR, COSTR(p0));
        Vector p1 = SS.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "      " PT_AS_STR, COSTR(p1));
        double d = (p1.Minus(p0)).Magnitude();
        Printf(true, "  d = %Fi%s", SS.MmToString(d));
    } else if(gs.n == 2 && gs.faces == 1 && gs.points == 1) {
        Printf(false, "%FtA POINT AND A PLANE FACE");
        Vector pt = SS.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "        point = " PT_AS_STR, COSTR(pt));
        Vector n = SS.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " plane normal = " PT_AS_NUM, CO(n));
        Vector pl = SS.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   plane thru = " PT_AS_STR, COSTR(pl));
        double dd = n.Dot(pl) - n.Dot(pt);
        Printf(true,  "     distance = %Fi%s", SS.MmToString(dd));
    } else if(gs.n == 3 && gs.points == 2 && gs.vectors == 1) {
        Printf(false, "%FtTWO POINTS AND A VECTOR");
        Vector p0 = SS.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "  pointA = " PT_AS_STR, COSTR(p0));
        Vector p1 = SS.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "  pointB = " PT_AS_STR, COSTR(p1));
        Vector v  = SS.GetEntity(gs.vector[0])->VectorGetNum();
        v = v.WithMagnitude(1);
        Printf(true,  "  vector = " PT_AS_NUM, CO(v));
        double d = (p1.Minus(p0)).Dot(v);
        Printf(true,  "  proj_d = %Fi%s", SS.MmToString(d));
    } else if(gs.n == 2 && gs.faces == 2) {
        Printf(false, "%FtTWO PLANE FACES");

        Vector n0 = SS.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " planeA normal = " PT_AS_NUM, CO(n0));
        Vector p0 = SS.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   planeA thru = " PT_AS_STR, COSTR(p0));

        Vector n1 = SS.GetEntity(gs.face[1])->FaceGetNormalNum();
        Printf(true,  " planeB normal = " PT_AS_NUM, CO(n1));
        Vector p1 = SS.GetEntity(gs.face[1])->FaceGetPointNum();
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

void TextWindow::OneScreenForwardTo(int screen) {
    SS.TW.shownIndex++;
    if(SS.TW.shownIndex >= HISTORY_LEN) SS.TW.shownIndex = 0;
    SS.TW.shown = &(SS.TW.showns[SS.TW.shownIndex]);
    history++;

    if(screen >= 0) shown->screen = screen;
}

