#include "solvespace.h"

// A public-domain Hershey vector font ("Simplex").
#include "font.table"

static bool ColorLocked;

#define FONT_SCALE (0.5)
static int StrWidth(char *str) {
    int w = 0;
    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;
        c -= 32;

        w += Font[c].width;
    }
    return w;
}
void glxWriteTextRefCenter(char *str)
{
    double scale = FONT_SCALE/SS.GW.scale;
    // The characters have height ~21, as they appear in the table.
    double fh = (21.0)*scale;
    double fw = StrWidth(str)*scale;
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

static void __stdcall Vertex(Vector *p) {
    glxVertex3v(*p);
}
static void __stdcall Combine(double coords[3], void *vertexData[4],
                                float weight[4], void **outData)
{
    Vector *n = (Vector *)AllocTemporary(sizeof(Vector));
    n->x = coords[0];
    n->y = coords[1];
    n->z = coords[2];

    *outData = n;
}
void glxFillPolygon(SPolygon *p)
{
    int i, j;

    GLUtesselator *gt = gluNewTess();
    typedef void __stdcall cf(void);
    gluTessCallback(gt, GLU_TESS_BEGIN, (cf *)glBegin);
    gluTessCallback(gt, GLU_TESS_END, (cf *)glEnd);
    gluTessCallback(gt, GLU_TESS_VERTEX, (cf *)Vertex);
    gluTessCallback(gt, GLU_TESS_COMBINE, (cf *)Combine);
    gluTessProperty(gt, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);

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
    gluDeleteTess(gt);
}

