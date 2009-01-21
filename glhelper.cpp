#include "solvespace.h"

// A public-domain Hershey vector font ("Simplex").
#include "font.table"

static bool ColorLocked;
static bool DepthOffsetLocked;

#define FONT_SCALE (0.55)
double glxStrWidth(char *str) {
    int w = 0;
    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;
        c -= 32;

        w += Font[c].width;
    }
    return w*FONT_SCALE/SS.GW.scale;
}
double glxStrHeight(void) {
    // The characters have height ~21, as they appear in the table.
    return 21.0*FONT_SCALE/SS.GW.scale;
}
void glxWriteTextRefCenter(char *str)
{
    double scale = FONT_SCALE/SS.GW.scale;
    double fh = glxStrHeight();
    double fw = glxStrWidth(str);
    glPushMatrix();
        glTranslated(-fw/2, -fh/2, 0);
        // Undo the (+5, +5) offset that glxWriteText applies.
        glTranslated(-5*scale, -5*scale, 0);
        glxWriteText(str);
    glPopMatrix();
}

void glxWriteText(char *str)
{
    double scale = FONT_SCALE/SS.GW.scale;
    int xo = 5;
    int yo = 5;
    
    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;

        c -= 32;

        glBegin(GL_LINE_STRIP);
        int j;
        for(j = 0; j < Font[c].points; j++) {
            int x = Font[c].coord[j*2];
            int y = Font[c].coord[j*2+1];

            if(x == PEN_UP && y == PEN_UP) {
                glEnd();
                glBegin(GL_LINE_STRIP);
            } else {
                glVertex3d((xo + x)*scale, (yo + y)*scale, 0);
            }
        }
        glEnd();

        xo += Font[c].width;
    }
}

void glxVertex3v(Vector u)
{
    glVertex3f((GLfloat)u.x, (GLfloat)u.y, (GLfloat)u.z);
}

void glxTranslatev(Vector u)
{
    glTranslated((GLdouble)u.x, (GLdouble)u.y, (GLdouble)u.z);
}

void glxOntoWorkplane(Vector u, Vector v)
{
    u = u.WithMagnitude(1);
    v = v.WithMagnitude(1);

    double mat[16];
    Vector n = u.Cross(v);
    MakeMatrix(mat,     u.x, v.x, n.x, 0,
                        u.y, v.y, n.y, 0,
                        u.z, v.z, n.z, 0,
                        0,   0,   0,   1);
    glMultMatrixd(mat);
}

void glxLockColorTo(double r, double g, double b)
{
    ColorLocked = false;    
    glxColor3d(r, g, b);
    ColorLocked = true;
}

void glxUnlockColor(void)
{
    ColorLocked = false;    
}

void glxColor3d(double r, double g, double b)
{
    if(!ColorLocked) glColor3d(r, g, b);
}

void glxColor4d(double r, double g, double b, double a)
{
    if(!ColorLocked) glColor4d(r, g, b, a);
}

static void Stipple(BOOL forSel)
{
    static BOOL Init;
    const int BYTES = (32*32)/8;
    static GLubyte HoverMask[BYTES];
    static GLubyte SelMask[BYTES];
    if(!Init) {
        int x, y;
        for(x = 0; x < 32; x++) {
            for(y = 0; y < 32; y++) {
                int i = y*4 + x/8, b = x % 8;
                int ym = y % 4, xm = x % 4;
                for(int k = 0; k < 2; k++) {
                    if(xm >= 1 && xm <= 2 && ym >= 1 && ym <= 2) {
                        (k == 0 ? SelMask : HoverMask)[i] |= (0x80 >> b);
                    }
                    ym = (ym + 2) % 4; xm = (xm + 2) % 4;
                }
            }
        }
        Init = TRUE;
    }

    glEnable(GL_POLYGON_STIPPLE);
    if(forSel) {
        glPolygonStipple(SelMask);
    } else {
        glPolygonStipple(HoverMask);
    }
}

static void StippleTriangle(STriangle *tr, BOOL s, double r, double g, double b)
{
    glEnd();
    glDisable(GL_LIGHTING);
    glColor3d(r, g, b);
    Stipple(s);
    glBegin(GL_TRIANGLES);
        glxVertex3v(tr->a);
        glxVertex3v(tr->b);
        glxVertex3v(tr->c);
    glEnd();
    glEnable(GL_LIGHTING);
    glDisable(GL_POLYGON_STIPPLE);
    glBegin(GL_TRIANGLES);
}

void glxFillMesh(int specColor, SMesh *m, DWORD h, DWORD s1, DWORD s2)
{
    glEnable(GL_NORMALIZE);
    int prevColor = -1;
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);

        int color;
        if(specColor < 0) {
            color = tr->meta.color;
        } else {
            color = specColor;
        }
        if(color != prevColor) {
            GLfloat mpf[] = { REDf(color), GREENf(color), BLUEf(color), 1.0 };
            glEnd();
            glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mpf);
            prevColor = color;
            glBegin(GL_TRIANGLES);
        }

        if(1 || tr->an.EqualsExactly(Vector::From(0, 0, 0))) {
            // Compute the normal from the vertices
            Vector n = tr->Normal();
            glNormal3d(n.x, n.y, n.z);
            glxVertex3v(tr->a);
            glxVertex3v(tr->b);
            glxVertex3v(tr->c);
        } else {
            // Use the exact normals that are specified
            glNormal3d((tr->an).x, (tr->an).y, (tr->an).z);
            glxVertex3v(tr->a);

            glNormal3d((tr->bn).x, (tr->bn).y, (tr->bn).z);
            glxVertex3v(tr->b);

            glNormal3d((tr->cn).x, (tr->cn).y, (tr->cn).z);
            glxVertex3v(tr->c);
        }

        if((s1 != 0 && tr->meta.face == s1) || 
           (s2 != 0 && tr->meta.face == s2))
        {
            StippleTriangle(tr, TRUE, 1, 0, 0);
        }
        if(h != 0 && tr->meta.face == h) {
            StippleTriangle(tr, FALSE, 1, 1, 0);
        }
    }
    glEnd();
}

static void GLX_CALLBACK Vertex(Vector *p) {
    glxVertex3v(*p);
}

void glxFillPolygon(SPolygon *p)
{
    GLUtesselator *gt = gluNewTess();
    gluTessCallback(gt, GLU_TESS_BEGIN, (glxCallbackFptr *)glBegin);
    gluTessCallback(gt, GLU_TESS_END, (glxCallbackFptr *)glEnd);
    gluTessCallback(gt, GLU_TESS_VERTEX, (glxCallbackFptr *)Vertex);

    glxTesselatePolygon(gt, p);

    gluDeleteTess(gt);
}

static void GLX_CALLBACK Combine(double coords[3], void *vertexData[4],
                                 float weight[4], void **outData)
{
    Vector *n = (Vector *)AllocTemporary(sizeof(Vector));
    n->x = coords[0];
    n->y = coords[1];
    n->z = coords[2];

    *outData = n;
}
void glxTesselatePolygon(GLUtesselator *gt, SPolygon *p)
{
    int i, j;

    gluTessCallback(gt, GLU_TESS_COMBINE, (glxCallbackFptr *)Combine);
    gluTessProperty(gt, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);

    Vector normal = p->normal;
    glNormal3d(normal.x, normal.y, normal.z);
    gluTessNormal(gt, normal.x, normal.y, normal.z);

    gluTessBeginPolygon(gt, NULL);
    for(i = 0; i < p->l.n; i++) {
        SContour *sc = &(p->l.elem[i]);
        gluTessBeginContour(gt);
        for(j = 0; j < (sc->l.n-1); j++) {
            SPoint *sp = &(sc->l.elem[j]);
            double ap[3];
            ap[0] = sp->p.x;
            ap[1] = sp->p.y;
            ap[2] = sp->p.z;
            gluTessVertex(gt, ap, &(sp->p));
        }
        gluTessEndContour(gt);
    }
    gluTessEndPolygon(gt);
}

void glxDebugPolygon(SPolygon *p)
{
    int i, j;
    glLineWidth(2);
    glPointSize(7);
    glDisable(GL_DEPTH_TEST);
    for(i = 0; i < p->l.n; i++) {
        SContour *sc = &(p->l.elem[i]);
        for(j = 0; j < (sc->l.n-1); j++) {
            Vector a = (sc->l.elem[j]).p;
            Vector b = (sc->l.elem[j+1]).p;

            glxLockColorTo(0, 0, 1);
            Vector d = (a.Minus(b)).WithMagnitude(-0);
            glBegin(GL_LINES);
                glxVertex3v(a.Plus(d));
                glxVertex3v(b.Minus(d));
            glEnd();
            glxLockColorTo(1, 0, 0);
            glBegin(GL_POINTS);
                glxVertex3v(a.Plus(d));
                glxVertex3v(b.Minus(d));
            glEnd();
        }
    }
}

void glxDrawEdges(SEdgeList *el)
{
    int i;
    glLineWidth(1);
    glxDepthRangeOffset(2);
    glxColor3d(REDf(SS.edgeColor), GREENf(SS.edgeColor), BLUEf(SS.edgeColor));

    glBegin(GL_LINES);
    for(i = 0; i < el->l.n; i++) {
        SEdge *se = &(el->l.elem[i]);
        glxVertex3v(se->a);
        glxVertex3v(se->b);
    }
    glEnd();
}

void glxDebugMesh(SMesh *m)
{
    int i;
    glLineWidth(1);
    glPointSize(7);
    glxDepthRangeOffset(1);
    glxUnlockColor();
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glxColor4d(0, 1, 0, 1.0);
    glBegin(GL_TRIANGLES);
    for(i = 0; i < m->l.n; i++) {
        STriangle *t = &(m->l.elem[i]);
        if(t->tag) continue;

        glxVertex3v(t->a);
        glxVertex3v(t->b);
        glxVertex3v(t->c);
    }
    glEnd();
    glxDepthRangeOffset(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void glxMarkPolygonNormal(SPolygon *p)
{
    Vector tail = Vector::From(0, 0, 0);
    int i, j, cnt = 0;
    // Choose some reasonable center point.
    for(i = 0; i < p->l.n; i++) {
        SContour *sc = &(p->l.elem[i]);
        for(j = 0; j < (sc->l.n-1); j++) {
            SPoint *sp = &(sc->l.elem[j]);
            tail = tail.Plus(sp->p);
            cnt++;
        }
    }
    if(cnt == 0) return;
    tail = tail.ScaledBy(1.0/cnt);

    Vector gn = SS.GW.projRight.Cross(SS.GW.projUp);
    Vector tip = tail.Plus((p->normal).WithMagnitude(40/SS.GW.scale));
    Vector arrow = (p->normal).WithMagnitude(15/SS.GW.scale);

    glColor3d(1, 1, 0);
    glBegin(GL_LINES);
        glxVertex3v(tail);
        glxVertex3v(tip);
        glxVertex3v(tip);
        glxVertex3v(tip.Minus(arrow.RotatedAbout(gn, 0.6)));
        glxVertex3v(tip);
        glxVertex3v(tip.Minus(arrow.RotatedAbout(gn, -0.6)));
    glEnd();
    glEnable(GL_LIGHTING);
}

void glxDepthRangeOffset(int units) {
    if(!DepthOffsetLocked) {
        // The size of this step depends on the resolution of the Z buffer; for
        // a 16-bit buffer, this should be fine.
        double d = units/60000.0;
        glDepthRange(0.1-d, 1-d);
    }
}

void glxDepthRangeLockToFront(bool yes) {
    if(yes) {
        DepthOffsetLocked = true;
        glDepthRange(0, 0);
    } else {
        DepthOffsetLocked = false;
        glxDepthRangeOffset(0);
    }
}

