//-----------------------------------------------------------------------------
// Helper functions that ultimately draw stuff with gl.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <zlib.h>
#include "solvespace.h"

namespace SolveSpace {

// A vector font.
#include "generated/vectorfont.table.h"

// A bitmap font.
#include "generated/bitmapfont.table.h"

static bool ColorLocked;
static bool DepthOffsetLocked;

static const VectorGlyph &GetVectorGlyph(char32_t chr) {
    int first = 0;
    int last  = sizeof(VectorFont) / sizeof(VectorGlyph);
    while(first <= last) {
        int mid = (first + last) / 2;
        char32_t midChr = VectorFont[mid].character;
        if(midChr > chr) {
            last = mid - 1; // and first stays the same
            continue;
        }
        if(midChr < chr) {
            first = mid + 1; // and last stays the same
            continue;
        }
        return VectorFont[mid];
    }
    return GetVectorGlyph(0xfffd); // replacement character
}

// Internally and in the UI, the vector font is sized using cap height.
#define FONT_SCALE(h) ((h)/(double)FONT_CAP_HEIGHT)
double ssglStrCapHeight(double h)
{
    return FONT_CAP_HEIGHT * FONT_SCALE(h) / SS.GW.scale;
}
double ssglStrFontSize(double h)
{
    return FONT_SIZE * FONT_SCALE(h) / SS.GW.scale;
}
double ssglStrWidth(const std::string &str, double h)
{
    int width = 0;
    for(char32_t chr : ReadUTF8(str)) {
        const VectorGlyph &glyph = GetVectorGlyph(chr);
        if(glyph.baseCharacter != 0) {
            const VectorGlyph &baseGlyph = GetVectorGlyph(glyph.baseCharacter);
            width += max(glyph.advanceWidth, baseGlyph.advanceWidth);
        } else {
            width += glyph.advanceWidth;
        }
    }
    return width * FONT_SCALE(h) / SS.GW.scale;
}
void ssglWriteTextRefCenter(const std::string &str, double h, Vector t, Vector u, Vector v,
                            ssglLineFn *fn, void *fndata)
{
    u = u.WithMagnitude(1);
    v = v.WithMagnitude(1);

    double scale = FONT_SCALE(h)/SS.GW.scale;
    double fh = ssglStrCapHeight(h);
    double fw = ssglStrWidth(str, h);

    t = t.Plus(u.ScaledBy(-fw/2));
    t = t.Plus(v.ScaledBy(-fh/2));

    // Apply additional offset to get an exact center alignment.
    t = t.Plus(v.ScaledBy(-4608*scale));

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

    if(workaroundEnabled && width < 1.6f)
        width = 1.6f;

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

Vector pixelAlign(Vector v) {
    v = SS.GW.ProjectPoint3(v);
    v.x = floor(v.x) + 0.5;
    v.y = floor(v.y) + 0.5;
    v = SS.GW.UnProjectPoint3(v);
    return v;
}

int ssglDrawCharacter(const VectorGlyph &glyph, Vector t, Vector o, Vector u, Vector v,
                      double scale, ssglLineFn *fn, void *fndata, bool gridFit) {
    int advanceWidth = glyph.advanceWidth;

    if(glyph.baseCharacter != 0) {
        const VectorGlyph &baseGlyph = GetVectorGlyph(glyph.baseCharacter);
        int baseWidth = ssglDrawCharacter(baseGlyph, t, o, u, v, scale, fn, fndata, gridFit);
        advanceWidth = max(glyph.advanceWidth, baseWidth);
    }

    int actualWidth, offsetX;
    if(gridFit) {
        o.x         += glyph.leftSideBearing;
        offsetX      = glyph.leftSideBearing;
        actualWidth  = glyph.boundingWidth;
        if(actualWidth == 0) {
            // Dot, "i", etc.
            actualWidth = 1;
        }
    } else {
        offsetX      = 0;
        actualWidth  = advanceWidth;
    }

    Vector tt = t;
    tt = tt.Plus(u.ScaledBy(o.x * scale));
    tt = tt.Plus(v.ScaledBy(o.y * scale));

    Vector tu = tt;
    tu = tu.Plus(u.ScaledBy(actualWidth * scale));

    Vector tv = tt;
    tv = tv.Plus(v.ScaledBy(FONT_CAP_HEIGHT * scale));

    if(gridFit) {
        tt = pixelAlign(tt);
        tu = pixelAlign(tu);
        tv = pixelAlign(tv);
    }

    tu = tu.Minus(tt).ScaledBy(1.0 / actualWidth);
    tv = tv.Minus(tt).ScaledBy(1.0 / FONT_CAP_HEIGHT);

    const int16_t *data = glyph.data;
    bool pen_up = true;
    Vector prevp;
    while(true) {
        int16_t x = *data++;
        int16_t y = *data++;

        if(x == PEN_UP && y == PEN_UP) {
            if(pen_up) break;
            pen_up = true;
        } else {
            Vector p = tt;
            p = p.Plus(tu.ScaledBy(x - offsetX));
            p = p.Plus(tv.ScaledBy(y));
            if(!pen_up) fn(fndata, prevp, p);
            prevp = p;
            pen_up = false;
        }
    }

    return advanceWidth;
}

void ssglWriteText(const std::string &str, double h, Vector t, Vector u, Vector v,
                   ssglLineFn *fn, void *fndata)
{
    if(!fn) fn = LineDrawCallback;
    u = u.WithMagnitude(1);
    v = v.WithMagnitude(1);

    // Perform grid-fitting only when the text is parallel to the view plane.
    bool gridFit = !SS.exportMode && u.Equals(SS.GW.projRight) && v.Equals(SS.GW.projUp);

    double scale = FONT_SCALE(h) / SS.GW.scale;
    Vector o = { 3840.0, 3840.0, 0.0 };
    for(char32_t chr : ReadUTF8(str)) {
        const VectorGlyph &glyph = GetVectorGlyph(chr);
        o.x += ssglDrawCharacter(glyph, t, o, u, v, scale, fn, fndata, gridFit);
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

void ssglLine(const Vector &a, const Vector &b, double pixelWidth, bool maybeFat) {
    if(!maybeFat || pixelWidth <= 3.0) {
        glBegin(GL_LINES);
            ssglVertex3v(a);
            ssglVertex3v(b);
        glEnd();
    } else {
        ssglFatLine(a, b, pixelWidth / SS.GW.scale);
    }
}

void ssglPoint(Vector p, double pixelSize)
{
    if(/*!maybeFat || */pixelSize <= 3.0) {
        glBegin(GL_LINES);
            Vector u = SS.GW.projRight.WithMagnitude(pixelSize / SS.GW.scale / 2.0);
            ssglVertex3v(p.Minus(u));
            ssglVertex3v(p.Plus(u));
        glEnd();
    } else {
        Vector u = SS.GW.projRight.WithMagnitude(pixelSize / SS.GW.scale / 2.0);
        Vector v = SS.GW.projUp.WithMagnitude(pixelSize / SS.GW.scale / 2.0);

        FatLineEndcap(p, u, v);
        FatLineEndcap(p, u.ScaledBy(-1.0), v);
    }
}

void ssglStippledLine(Vector a, Vector b, double width,
                      int stippleType, double stippleScale, bool maybeFat)
{
    const char *stipplePattern;
    switch(stippleType) {
        case Style::STIPPLE_CONTINUOUS:    ssglLine(a, b, width, maybeFat); return;
        case Style::STIPPLE_DASH:          stipplePattern = "- ";  break;
        case Style::STIPPLE_LONG_DASH:     stipplePattern = "_ ";  break;
        case Style::STIPPLE_DASH_DOT:      stipplePattern = "-.";  break;
        case Style::STIPPLE_DASH_DOT_DOT:  stipplePattern = "-.."; break;
        case Style::STIPPLE_DOT:           stipplePattern = ".";   break;
        case Style::STIPPLE_FREEHAND:      stipplePattern = "~";   break;
        case Style::STIPPLE_ZIGZAG:        stipplePattern = "~__"; break;
        default: oops();
    }
    ssglStippledLine(a, b, width, stipplePattern, stippleScale, maybeFat);
}

void ssglStippledLine(Vector a, Vector b, double width,
                      const char *stipplePattern, double stippleScale, bool maybeFat)
{
    if(stipplePattern == NULL || *stipplePattern == 0) oops();

    Vector dir = b.Minus(a);
    double len = dir.Magnitude();
    dir = dir.WithMagnitude(1.0);

    const char *si = stipplePattern;
    double end = len;
    double ss = stippleScale / 2.0;
    do {
        double start = end;
        switch(*si) {
            case ' ':
                end -= 1.0 * ss;
                break;

            case '-':
                start = max(start - 0.5 * ss, 0.0);
                end = max(start - 2.0 * ss, 0.0);
                if(start == end) break;
                ssglLine(a.Plus(dir.ScaledBy(start)), a.Plus(dir.ScaledBy(end)), width, maybeFat);
                end = max(end - 0.5 * ss, 0.0);
                break;

            case '_':
                end = max(end - 4.0 * ss, 0.0);
                ssglLine(a.Plus(dir.ScaledBy(start)), a.Plus(dir.ScaledBy(end)), width, maybeFat);
                break;

            case '.':
                end = max(end - 0.5 * ss, 0.0);
                if(end == 0.0) break;
                ssglPoint(a.Plus(dir.ScaledBy(end)), width);
                end = max(end - 0.5 * ss, 0.0);
                break;

            case '~': {
                Vector ab  = b.Minus(a);
                Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
                Vector abn = (ab.Cross(gn)).WithMagnitude(1);
                abn = abn.Minus(gn.ScaledBy(gn.Dot(abn)));
                double pws = 2.0 * width / SS.GW.scale;

                end = max(end - 0.5 * ss, 0.0);
                Vector aa = a.Plus(dir.ScaledBy(start));
                Vector bb = a.Plus(dir.ScaledBy(end))
                             .Plus(abn.ScaledBy(pws * (start - end) / (0.5 * ss)));
                ssglLine(aa, bb, width, maybeFat);
                if(end == 0.0) break;

                start = end;
                end = max(end - 1.0 * ss, 0.0);
                aa = a.Plus(dir.ScaledBy(end))
                      .Plus(abn.ScaledBy(pws))
                      .Minus(abn.ScaledBy(2.0 * pws * (start - end) / ss));
                ssglLine(bb, aa, width, maybeFat);
                if(end == 0.0) break;

                start = end;
                end = max(end - 0.5 * ss, 0.0);
                bb = a.Plus(dir.ScaledBy(end))
                      .Minus(abn.ScaledBy(pws))
                      .Plus(abn.ScaledBy(pws * (start - end) / (0.5 * ss)));
                ssglLine(aa, bb, width, maybeFat);
                break;
            }

            default: oops();
        }
        if(*(++si) == 0) si = stipplePattern;
    } while(end > 0.0);
}

void ssglFatLine(Vector a, Vector b, double width)
{
    if(a.EqualsExactly(b)) return;
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

void ssglDrawEdges(SEdgeList *el, bool endpointsToo, hStyle hs)
{
    double lineWidth = Style::Width(hs);
    int stippleType = Style::PatternType(hs);
    double stippleScale = Style::StippleScaleMm(hs);
    ssglLineWidth(float(lineWidth));
    ssglColorRGB(Style::Color(hs));

    SEdge *se;
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        ssglStippledLine(se->a, se->b, lineWidth, stippleType, stippleScale,
                         /*maybeFat=*/true);
    }

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

void ssglDrawOutlines(SOutlineList *sol, Vector projDir, hStyle hs)
{
    double lineWidth = Style::Width(hs);
    int stippleType = Style::PatternType(hs);
    double stippleScale = Style::StippleScaleMm(hs);
    ssglLineWidth((float)lineWidth);
    ssglColorRGB(Style::Color(hs));

    sol->FillOutlineTags(projDir);
    for(SOutline *so = sol->l.First(); so; so = sol->l.NextAfter(so)) {
        if(!so->tag) continue;
        ssglStippledLine(so->a, so->b, lineWidth, stippleType, stippleScale,
                         /*maybeFat=*/true);
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

const int BitmapFontChunkSize = 64 * 64;
static bool BitmapFontChunkInitialized[0x10000 / BitmapFontChunkSize];
static int BitmapFontCurrentChunk = -1;

static void CreateBitmapFontChunk(const uint8_t *source, size_t sourceLength,
                                  int textureIndex)
{
    // Place the font in our texture in a two-dimensional grid.
    // The maximum texture size that is reasonably supported is 1024x1024.
    const size_t fontTextureSize = BitmapFontChunkSize*16*16;
    uint8_t *fontTexture = (uint8_t *)malloc(fontTextureSize),
            *mappedTexture = (uint8_t *)malloc(fontTextureSize);

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    if(inflateInit(&stream) != Z_OK)
        oops();

    stream.next_in = (Bytef *)source;
    stream.avail_in = sourceLength;
    stream.next_out = fontTexture;
    stream.avail_out = fontTextureSize;
    if(inflate(&stream, Z_NO_FLUSH) != Z_STREAM_END)
        oops();
    if(stream.avail_out != 0)
        oops();

    inflateEnd(&stream);

    for(int a = 0; a < BitmapFontChunkSize; a++) {
        int row = a / 64, col = a % 64;

        for(int i = 0; i < 16; i++) {
            memcpy(mappedTexture + row*64*16*16 + col*16 + i*64*16,
                   fontTexture + a*16*16 + i*16,
                   16);
        }
    }

    free(fontTexture);

    glBindTexture(GL_TEXTURE_2D, textureIndex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
                 16*64, 64*16,
                 0,
                 GL_ALPHA, GL_UNSIGNED_BYTE,
                 mappedTexture);

    free(mappedTexture);
}

static void SwitchToBitmapFontChunkFor(char32_t chr)
{
    int plane = chr / BitmapFontChunkSize,
        textureIndex = TEXTURE_BITMAP_FONT + plane;

    if(BitmapFontCurrentChunk != textureIndex) {
        glEnd();

        if(!BitmapFontChunkInitialized[plane]) {
            CreateBitmapFontChunk(CompressedFontTexture[plane].data,
                                  CompressedFontTexture[plane].length,
                                  textureIndex);
            BitmapFontChunkInitialized[plane] = true;
        } else {
            glBindTexture(GL_TEXTURE_2D, textureIndex);
        }

        BitmapFontCurrentChunk = textureIndex;

        glBegin(GL_QUADS);
    }
}

void ssglInitializeBitmapFont()
{
    memset(BitmapFontChunkInitialized, 0, sizeof(BitmapFontChunkInitialized));
    BitmapFontCurrentChunk = -1;
}

int ssglBitmapCharWidth(char32_t chr) {
    if(!CodepointProperties[chr].exists)
        chr = 0xfffd; // replacement character

    return CodepointProperties[chr].isWide ? 2 : 1;
}

void ssglBitmapCharQuad(char32_t chr, double x, double y)
{
    int w, h;

    if(!CodepointProperties[chr].exists)
        chr = 0xfffd; // replacement character

    h = 16;
    if(chr >= 0xe000 && chr <= 0xefff) {
        // Special character, like a checkbox or a radio button
        w = 16;
        x -= 3;
    } else if(CodepointProperties[chr].isWide) {
        // Wide (usually CJK or reserved) character
        w = 16;
    } else {
        // Normal character
        w = 8;
    }

    if(chr != ' ' && chr != 0) {
        int n = chr % BitmapFontChunkSize;
        int row = n / 64, col = n % 64;
        double s0 = col/64.0,
               s1 = (col+1)/64.0,
               t0 = row/64.0,
               t1 = t0 + (w/16.0)/64;

        SwitchToBitmapFontChunkFor(chr);

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

void ssglBitmapText(const std::string &str, Vector p)
{
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for(char32_t chr : ReadUTF8(str)) {
        ssglBitmapCharQuad(chr, p.x, p.y);
        p.x += 8 * ssglBitmapCharWidth(chr);
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
