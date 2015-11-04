//-----------------------------------------------------------------------------
// Helper functions that ultimately draw stuff with gl.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// A public-domain Hershey vector font ("Simplex").
#include "font.table.h"

// A bitmap font.
#include "generated/bitmapfont.table.h"

static bool ColorLocked;
static bool DepthOffsetLocked;

#define FONT_SCALE(h) ((h)/22.0)
double ssglStrWidth(const char *str, double h)
{
    int w = 0;
    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;
        c -= 32;

        w += Font[c].width;
    }
    return w*FONT_SCALE(h)/SS.GW.scale;
}
double ssglStrHeight(double h)
{
    // The characters have height ~22, as they appear in the table.
    return 22.0*FONT_SCALE(h)/SS.GW.scale;
}
void ssglWriteTextRefCenter(const char *str, double h, Vector t, Vector u, Vector v,
                            ssglLineFn *fn, void *fndata)
{
    u = u.WithMagnitude(1);
    v = v.WithMagnitude(1);

    double scale = FONT_SCALE(h)/SS.GW.scale;
    double fh = ssglStrHeight(h);
    double fw = ssglStrWidth(str, h);

    t = t.Plus(u.ScaledBy(-fw/2));
    t = t.Plus(v.ScaledBy(-fh/2));

    // Undo the (+5, +5) offset that ssglWriteText applies.
    t = t.Plus(u.ScaledBy(-5*scale));
    t = t.Plus(v.ScaledBy(-5*scale));

    ssglWriteText(str, h, t, u, v, fn, fndata);
}

void ssglLineWidth(GLfloat width) {
    // Intel GPUs with Mesa on *nix render thin lines poorly.
    static bool workaroundChecked, workaroundEnabled;
    if(!workaroundChecked) {
        // ssglLineWidth can be called before GL is initialized
        if(glGetString(GL_VENDOR)) {
            workaroundChecked = true;
            if(!strcmp((char*)glGetString(GL_VENDOR), "Intel Open Source Technology Center"))
                workaroundEnabled = true;
        }
    }

    if(workaroundEnabled && width < 1.6)
        width = 1.6;

    glLineWidth(width);
}

static void LineDrawCallback(void *fndata, Vector a, Vector b)
{
    ssglLineWidth(1);
    glBegin(GL_LINES);
        ssglVertex3v(a);
        ssglVertex3v(b);
    glEnd();
}

void ssglWriteText(const char *str, double h, Vector t, Vector u, Vector v,
                   ssglLineFn *fn, void *fndata)
{
    if(!fn) fn = LineDrawCallback;
    u = u.WithMagnitude(1);
    v = v.WithMagnitude(1);

    double scale = FONT_SCALE(h)/SS.GW.scale;
    int xo = 5;
    int yo = 5;

    for(; *str; str++) {
        int c = *str;
        if(c < 32 || c > 126) c = 32;

        c -= 32;

        int j;
        Vector prevp = Vector::From(VERY_POSITIVE, 0, 0);
        for(j = 0; j < Font[c].points; j++) {
            int x = Font[c].coord[j*2];
            int y = Font[c].coord[j*2+1];

            if(x == PEN_UP && y == PEN_UP) {
                prevp.x = VERY_POSITIVE;
            } else {
                Vector p = t;
                p = p.Plus(u.ScaledBy((xo + x)*scale));
                p = p.Plus(v.ScaledBy((yo + y)*scale));
                if(EXACT(prevp.x != VERY_POSITIVE)) {
                    fn(fndata, prevp, p);
                }
                prevp = p;
            }
        }

        xo += Font[c].width;
    }
}

void ssglVertex3v(Vector u)
{
    glVertex3f((GLfloat)u.x, (GLfloat)u.y, (GLfloat)u.z);
}

void ssglAxisAlignedQuad(double l, double r, double t, double b, bool lone)
{
    if(lone) glBegin(GL_QUADS);
        glVertex2d(l, t);
        glVertex2d(l, b);
        glVertex2d(r, b);
        glVertex2d(r, t);
    if(lone) glEnd();
}

void ssglAxisAlignedLineLoop(double l, double r, double t, double b)
{
    glBegin(GL_LINE_LOOP);
        glVertex2d(l, t);
        glVertex2d(l, b);
        glVertex2d(r, b);
        glVertex2d(r, t);
    glEnd();
}

static void FatLineEndcap(Vector p, Vector u, Vector v)
{
    // A table of cos and sin of (pi*i/10 + pi/2), as i goes from 0 to 10
    static const double Circle[11][2] = {
        {  0.0000,   1.0000 },
        { -0.3090,   0.9511 },
        { -0.5878,   0.8090 },
        { -0.8090,   0.5878 },
        { -0.9511,   0.3090 },
        { -1.0000,   0.0000 },
        { -0.9511,  -0.3090 },
        { -0.8090,  -0.5878 },
        { -0.5878,  -0.8090 },
        { -0.3090,  -0.9511 },
        {  0.0000,  -1.0000 },
    };
    glBegin(GL_TRIANGLE_FAN);
    for(int i = 0; i <= 10; i++) {
        double c = Circle[i][0], s = Circle[i][1];
        ssglVertex3v(p.Plus(u.ScaledBy(c)).Plus(v.ScaledBy(s)));
    }
    glEnd();
}

void ssglFatLine(Vector a, Vector b, double width)
{
    // The half-width of the line we're drawing.
    double hw = width / 2;
    Vector ab  = b.Minus(a);
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    Vector abn = (ab.Cross(gn)).WithMagnitude(1);
    abn = abn.Minus(gn.ScaledBy(gn.Dot(abn)));
    // So now abn is normal to the projection of ab into the screen, so the
    // line will always have constant thickness as the view is rotated.

    abn = abn.WithMagnitude(hw);
    ab  = gn.Cross(abn);
    ab  = ab. WithMagnitude(hw);

    // The body of a line is a quad
    glBegin(GL_QUADS);
        ssglVertex3v(a.Minus(abn));
        ssglVertex3v(b.Minus(abn));
        ssglVertex3v(b.Plus (abn));
        ssglVertex3v(a.Plus (abn));
    glEnd();
    // And the line has two semi-circular end caps.
    FatLineEndcap(a, ab,              abn);
    FatLineEndcap(b, ab.ScaledBy(-1), abn);
}


void ssglLockColorTo(RgbaColor rgb)
{
    ColorLocked = false;
    glColor3d(rgb.redF(), rgb.greenF(), rgb.blueF());
    ColorLocked = true;
}

void ssglUnlockColor(void)
{
    ColorLocked = false;
}

void ssglColorRGB(RgbaColor rgb)
{
    // Is there a bug in some graphics drivers where this is not equivalent
    // to glColor3d? There seems to be...
    ssglColorRGBa(rgb, 1.0);
}

void ssglColorRGBa(RgbaColor rgb, double a)
{
    if(!ColorLocked) glColor4d(rgb.redF(), rgb.greenF(), rgb.blueF(), a);
}

static void Stipple(bool forSel)
{
    static bool Init;
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
        Init = true;
    }

    glEnable(GL_POLYGON_STIPPLE);
    if(forSel) {
        glPolygonStipple(SelMask);
    } else {
        glPolygonStipple(HoverMask);
    }
}

static void StippleTriangle(STriangle *tr, bool s, RgbaColor rgb)
{
    glEnd();
    glDisable(GL_LIGHTING);
    ssglColorRGB(rgb);
    Stipple(s);
    glBegin(GL_TRIANGLES);
        ssglVertex3v(tr->a);
        ssglVertex3v(tr->b);
        ssglVertex3v(tr->c);
    glEnd();
    glEnable(GL_LIGHTING);
    glDisable(GL_POLYGON_STIPPLE);
    glBegin(GL_TRIANGLES);
}

void ssglFillMesh(bool useSpecColor, RgbaColor specColor,
                  SMesh *m, uint32_t h, uint32_t s1, uint32_t s2)
{
    RgbaColor rgbHovered  = Style::Color(Style::HOVERED),
             rgbSelected = Style::Color(Style::SELECTED);

    glEnable(GL_NORMALIZE);
    bool hasMaterial = false;
    RgbaColor prevColor;
    glBegin(GL_TRIANGLES);
    for(int i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);

        RgbaColor color;
        if(useSpecColor) {
            color = specColor;
        } else {
            color = tr->meta.color;
        }
        if(!hasMaterial || !color.Equals(prevColor)) {
            GLfloat mpf[] = { color.redF(), color.greenF(), color.blueF(), color.alphaF() };
            glEnd();
            glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mpf);
            prevColor = color;
            hasMaterial = true;
            glBegin(GL_TRIANGLES);
        }

        if(tr->an.EqualsExactly(Vector::From(0, 0, 0))) {
            // Compute the normal from the vertices
            Vector n = tr->Normal();
            glNormal3d(n.x, n.y, n.z);
            ssglVertex3v(tr->a);
            ssglVertex3v(tr->b);
            ssglVertex3v(tr->c);
        } else {
            // Use the exact normals that are specified
            glNormal3d((tr->an).x, (tr->an).y, (tr->an).z);
            ssglVertex3v(tr->a);

            glNormal3d((tr->bn).x, (tr->bn).y, (tr->bn).z);
            ssglVertex3v(tr->b);

            glNormal3d((tr->cn).x, (tr->cn).y, (tr->cn).z);
            ssglVertex3v(tr->c);
        }

        if((s1 != 0 && tr->meta.face == s1) ||
           (s2 != 0 && tr->meta.face == s2))
        {
            StippleTriangle(tr, true, rgbSelected);
        }
        if(h != 0 && tr->meta.face == h) {
            StippleTriangle(tr, false, rgbHovered);
        }
    }
    glEnd();
}

static void SSGL_CALLBACK Vertex(Vector *p)
{
    ssglVertex3v(*p);
}
void ssglFillPolygon(SPolygon *p)
{
    GLUtesselator *gt = gluNewTess();
    gluTessCallback(gt, GLU_TESS_BEGIN,  (ssglCallbackFptr *)glBegin);
    gluTessCallback(gt, GLU_TESS_END,    (ssglCallbackFptr *)glEnd);
    gluTessCallback(gt, GLU_TESS_VERTEX, (ssglCallbackFptr *)Vertex);

    ssglTesselatePolygon(gt, p);

    gluDeleteTess(gt);
}

static void SSGL_CALLBACK Combine(double coords[3], void *vertexData[4],
                                  float weight[4], void **outData)
{
    Vector *n = (Vector *)AllocTemporary(sizeof(Vector));
    n->x = coords[0];
    n->y = coords[1];
    n->z = coords[2];

    *outData = n;
}
void ssglTesselatePolygon(GLUtesselator *gt, SPolygon *p)
{
    int i, j;

    gluTessCallback(gt, GLU_TESS_COMBINE, (ssglCallbackFptr *)Combine);
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

void ssglDebugPolygon(SPolygon *p)
{
    int i, j;
    ssglLineWidth(2);
    glPointSize(7);
    glDisable(GL_DEPTH_TEST);
    for(i = 0; i < p->l.n; i++) {
        SContour *sc = &(p->l.elem[i]);
        for(j = 0; j < (sc->l.n-1); j++) {
            Vector a = (sc->l.elem[j]).p;
            Vector b = (sc->l.elem[j+1]).p;

            ssglLockColorTo(RGBi(0, 0, 255));
            Vector d = (a.Minus(b)).WithMagnitude(-0);
            glBegin(GL_LINES);
                ssglVertex3v(a.Plus(d));
                ssglVertex3v(b.Minus(d));
            glEnd();
            ssglLockColorTo(RGBi(255, 0, 0));
            glBegin(GL_POINTS);
                ssglVertex3v(a.Plus(d));
                ssglVertex3v(b.Minus(d));
            glEnd();
        }
    }
}

void ssglDrawEdges(SEdgeList *el, bool endpointsToo)
{
    SEdge *se;
    glBegin(GL_LINES);
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        ssglVertex3v(se->a);
        ssglVertex3v(se->b);
    }
    glEnd();

    if(endpointsToo) {
        glPointSize(12);
        glBegin(GL_POINTS);
        for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
            ssglVertex3v(se->a);
            ssglVertex3v(se->b);
        }
        glEnd();
    }
}

void ssglDebugMesh(SMesh *m)
{
    int i;
    ssglLineWidth(1);
    glPointSize(7);
    ssglDepthRangeOffset(1);
    ssglUnlockColor();
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    ssglColorRGBa(RGBi(0, 255, 0), 1.0);
    glBegin(GL_TRIANGLES);
    for(i = 0; i < m->l.n; i++) {
        STriangle *t = &(m->l.elem[i]);
        if(t->tag) continue;

        ssglVertex3v(t->a);
        ssglVertex3v(t->b);
        ssglVertex3v(t->c);
    }
    glEnd();
    ssglDepthRangeOffset(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void ssglMarkPolygonNormal(SPolygon *p)
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
        ssglVertex3v(tail);
        ssglVertex3v(tip);
        ssglVertex3v(tip);
        ssglVertex3v(tip.Minus(arrow.RotatedAbout(gn, 0.6)));
        ssglVertex3v(tip);
        ssglVertex3v(tip.Minus(arrow.RotatedAbout(gn, -0.6)));
    glEnd();
    glEnable(GL_LIGHTING);
}

void ssglDepthRangeOffset(int units)
{
    if(!DepthOffsetLocked) {
        // The size of this step depends on the resolution of the Z buffer; for
        // a 16-bit buffer, this should be fine.
        double d = units/60000.0;
        glDepthRange(0.1-d, 1-d);
    }
}

void ssglDepthRangeLockToFront(bool yes)
{
    if(yes) {
        DepthOffsetLocked = true;
        glDepthRange(0, 0);
    } else {
        DepthOffsetLocked = false;
        ssglDepthRangeOffset(0);
    }
}

void ssglCreateBitmapFont(void)
{
    // Place the font in our texture in a two-dimensional grid; 1d would
    // be simpler, but long skinny textures (256*16 = 4096 pixels wide)
    // won't work.
    static uint8_t MappedTexture[4*16*64*16];
    int a, i;
    for(a = 0; a < 256; a++) {
        int row = a / 4, col = a % 4;

        for(i = 0; i < 16; i++) {
            memcpy(MappedTexture + row*4*16*16 + col*16 + i*4*16,
                   FontTexture + a*16*16 + i*16,
                   16);
        }
    }

    glBindTexture(GL_TEXTURE_2D, TEXTURE_BITMAP_FONT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                 16*4, 64*16,
                 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE,
                 MappedTexture);
}

void ssglBitmapCharQuad(char c, double x, double y)
{
    uint8_t b = (uint8_t)c;
    int w, h;

    if(b & 0x80) {
        // Special character, like a checkbox or a radio button
        w = h = 16;
        x -= 3;
    } else {
        // Normal character from our font
        w = SS.TW.CHAR_WIDTH,
        h = SS.TW.CHAR_HEIGHT;
    }

    if(b != ' ' && b != 0) {
        int row = b / 4, col = b % 4;
        double s0 = col/4.0,
               s1 = (col+1)/4.0,
               t0 = row/64.0,
               t1 = t0 + (w/16.0)/64;

        glTexCoord2d(s1, t0);
        glVertex2d(x, y);

        glTexCoord2d(s1, t1);
        glVertex2d(x + w, y);

        glTexCoord2d(s0, t1);
        glVertex2d(x + w, y - h);

        glTexCoord2d(s0, t0);
        glVertex2d(x, y - h);
    }
}

void ssglBitmapText(const char *str, Vector p)
{
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    while(*str) {
        ssglBitmapCharQuad(*str, p.x, p.y);

        str++;
        p.x += SS.TW.CHAR_WIDTH;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void ssglDrawPixelsWithTexture(uint8_t *data, int w, int h)
{
#define MAX_DIM 32
    static uint8_t Texture[MAX_DIM*MAX_DIM*3];
    int i, j;
    if(w > MAX_DIM || h > MAX_DIM) oops();

    for(i = 0; i < w; i++) {
        for(j = 0; j < h; j++) {
            Texture[(j*MAX_DIM + i)*3 + 0] = data[(j*w + i)*3 + 0];
            Texture[(j*MAX_DIM + i)*3 + 1] = data[(j*w + i)*3 + 1];
            Texture[(j*MAX_DIM + i)*3 + 2] = data[(j*w + i)*3 + 2];
        }
    }

    glBindTexture(GL_TEXTURE_2D, TEXTURE_DRAW_PIXELS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, MAX_DIM, MAX_DIM, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, Texture);

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
        glTexCoord2d(0, 0);
        glVertex2d(0, h);

        glTexCoord2d(((double)w)/MAX_DIM, 0);
        glVertex2d(w, h);

        glTexCoord2d(((double)w)/MAX_DIM, ((double)h)/MAX_DIM);
        glVertex2d(w, 0);

        glTexCoord2d(0, ((double)h)/MAX_DIM);
        glVertex2d(0, 0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

};
