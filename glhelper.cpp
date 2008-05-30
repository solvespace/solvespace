#include "solvespace.h"

// A public-domain Hershey vector font ("Simplex").
#include "font.table"

static bool ColorLocked;

#define FONT_SCALE (0.5)
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

void glxFillMesh(bool useModelColor, SMesh *m)
{
    glEnable(GL_NORMALIZE);
    int prevColor = -1;
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);
        Vector n = tr->Normal();
        glNormal3d(n.x, n.y, n.z);

        int color = tr->meta.color;
        if(useModelColor && color != prevColor) {
            GLfloat mpf[] = { ((color >>  0) & 0xff) / 255.0f,
                              ((color >>  8) & 0xff) / 255.0f,
                              ((color >> 16) & 0xff) / 255.0f, 1.0 };
            glEnd();
            glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mpf);
            prevColor = color;
            glBegin(GL_TRIANGLES);
        }

        glxVertex3v(tr->a);
        glxVertex3v(tr->b);
        glxVertex3v(tr->c);
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

void glxDebugEdgeList(SEdgeList *el)
{
    int i;
    glLineWidth(2);
    glPointSize(7);
    glDisable(GL_DEPTH_TEST);
    for(i = 0; i < el->l.n; i++) {
        SEdge *se = &(el->l.elem[i]);
        if(se->tag) continue;
        Vector a = se->a, b = se->b;

        glxLockColorTo(0, 1, 1);
        Vector d = (a.Minus(b)).WithMagnitude(-0);
        glBegin(GL_LINES);
            glxVertex3v(a.Plus(d));
            glxVertex3v(b.Minus(d));
        glEnd();
        glxLockColorTo(0, 0, 1);
        glBegin(GL_POINTS);
            glxVertex3v(a.Plus(d));
            glxVertex3v(b.Minus(d));
        glEnd();
    }
}

void glxDebugMesh(SMesh *m)
{
    int i;
    glLineWidth(1);
    glPointSize(7);
    glxUnlockColor();
    for(i = 0; i < m->l.n; i++) {
        STriangle *t = &(m->l.elem[i]);
        if(t->tag) continue;

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glxColor4d(0, 1, 0, 1.0);
        glBegin(GL_TRIANGLES);
            glxVertex3v(t->a);
            glxVertex3v(t->b);
            glxVertex3v(t->c);
        glEnd();
        glPolygonOffset(0, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void glxMarkPolygonNormal(SPolygon *p)
{
    Vector tail = Vector::MakeFrom(0, 0, 0);
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

