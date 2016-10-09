//-----------------------------------------------------------------------------
// OpenGL 1 based rendering interface.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "config.h"
#include "solvespace.h"

#ifdef WIN32
// Include after solvespace.h to avoid identifier clashes.
#   include <windows.h> // required by GL headers
#endif
#ifdef __APPLE__
#   include <OpenGL/gl.h>
#   include <OpenGL/glu.h>
#else
#   include <GL/gl.h>
#   include <GL/glu.h>
#endif

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Thin wrappers around OpenGL functions to fix bugs, adapt them to our
// data structures, etc.
//-----------------------------------------------------------------------------

static inline void ssglNormal3v(Vector n) {
    glNormal3d(n.x, n.y, n.z);
}

static inline void ssglVertex3v(Vector v) {
    glVertex3d(v.x, v.y, v.z);
}

void ssglLineWidth(double width) {
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

    glLineWidth((GLfloat)width);
}

static inline void ssglColorRGBA(RgbaColor color) {
    glColor4d(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

static inline void ssglMaterialRGBA(GLenum side, RgbaColor color) {
    GLfloat mpb[] = { color.redF(), color.greenF(), color.blueF(), color.alphaF() };
    glMaterialfv(side, GL_AMBIENT_AND_DIFFUSE, mpb);
}

static void ssglDepthRange(Canvas::Layer layer, int zIndex) {
    switch(layer) {
        case Canvas::Layer::NORMAL:
        case Canvas::Layer::FRONT:
        case Canvas::Layer::BACK:
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            break;

        case Canvas::Layer::DEPTH_ONLY:
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            break;

        case Canvas::Layer::OCCLUDED:
            glDepthFunc(GL_GREATER);
            glDepthMask(GL_FALSE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            break;
    }

    switch(layer) {
        case Canvas::Layer::FRONT:
            glDepthRange(0.0, 0.0);
            break;

        case Canvas::Layer::BACK:
            glDepthRange(1.0, 1.0);
            break;

        case Canvas::Layer::NORMAL:
        case Canvas::Layer::DEPTH_ONLY:
        case Canvas::Layer::OCCLUDED:
            // The size of this step depends on the resolution of the Z buffer; for
            // a 16-bit buffer, this should be fine.
            double offset = 1.0 / (65535 * 0.8) * zIndex;
            glDepthRange(0.1 - offset, 1.0 - offset);
            break;
    }
}

static void ssglFillPattern(Canvas::FillPattern pattern) {
    static bool Init;
    static GLubyte MaskA[(32*32)/8];
    static GLubyte MaskB[(32*32)/8];
    if(!Init) {
        int x, y;
        for(x = 0; x < 32; x++) {
            for(y = 0; y < 32; y++) {
                int i = y*4 + x/8, b = x % 8;
                int ym = y % 4, xm = x % 4;
                for(int k = 0; k < 2; k++) {
                    if(xm >= 1 && xm <= 2 && ym >= 1 && ym <= 2) {
                        (k == 0 ? MaskB : MaskA)[i] |= (0x80 >> b);
                    }
                    ym = (ym + 2) % 4; xm = (xm + 2) % 4;
                }
            }
        }
        Init = true;
    }

    switch(pattern) {
        case Canvas::FillPattern::SOLID:
            glDisable(GL_POLYGON_STIPPLE);
            break;

        case Canvas::FillPattern::CHECKERED_A:
            glEnable(GL_POLYGON_STIPPLE);
            glPolygonStipple(MaskA);
            break;

        case Canvas::FillPattern::CHECKERED_B:
            glEnable(GL_POLYGON_STIPPLE);
            glPolygonStipple(MaskB);
            break;
    }
}

//-----------------------------------------------------------------------------
// A simple OpenGL state tracker to group consecutive draw calls.
//-----------------------------------------------------------------------------

void OpenGl1Renderer::SelectPrimitive(GLenum mode) {
    if(current.drawing && current.mode == mode) {
        return;
    } else if(current.drawing) {
        glEnd();
    }
    glBegin(mode);
    current.drawing = true;
    current.mode    = mode;
}

void OpenGl1Renderer::UnSelectPrimitive() {
    if(!current.drawing) return;
    glEnd();
    current.drawing = false;
}

Canvas::Stroke *OpenGl1Renderer::SelectStroke(hStroke hcs) {
    if(current.hcs.v == hcs.v) return current.stroke;

    Stroke *stroke = strokes.FindById(hcs);
    UnSelectPrimitive();
    ssglColorRGBA(stroke->color);
    ssglDepthRange(stroke->layer, stroke->zIndex);
    ssglLineWidth(stroke->width);
    // Fat lines and points are quads affected by glPolygonStipple, so make sure
    // they are displayed correctly.
    ssglFillPattern(FillPattern::SOLID);
    glDisable(GL_TEXTURE_2D);

    current.hcs    = hcs;
    current.stroke = stroke;
    current.hcf    = {};
    current.fill   = NULL;
    current.texture.reset();
    return stroke;
}

Canvas::Fill *OpenGl1Renderer::SelectFill(hFill hcf) {
    if(current.hcf.v == hcf.v) return current.fill;

    Fill *fill = fills.FindById(hcf);
    UnSelectPrimitive();
    ssglColorRGBA(fill->color);
    ssglDepthRange(fill->layer, fill->zIndex);
    ssglFillPattern(fill->pattern);
    glDisable(GL_TEXTURE_2D);

    current.hcs    = {};
    current.stroke = NULL;
    current.hcf    = hcf;
    current.fill   = fill;
    current.texture.reset();
    return fill;
}

static int RoundUpToPowerOfTwo(int v)
{
    for(int i = 0; i < 31; i++) {
        int vt = (1 << i);
        if(vt >= v) {
            return vt;
        }
    }
    return 0;
}

static bool NeedsGl1V1Workaround()
{
    static bool didGlVersionCheck, needsGl1V1Workaround;
    if(!didGlVersionCheck) {
        didGlVersionCheck = true;
        if(!strcmp((const char*) glGetString(GL_VERSION), "1.1.0")) {
            // The default Windows GL renderer really does implement GL 1.1,
            // and cannot handle non-power-of-2 textures, which is legal.
            needsGl1V1Workaround = true;
        }
    }
    return needsGl1V1Workaround;
}

void OpenGl1Renderer::SelectTexture(std::shared_ptr<const Pixmap> pm) {
    if(current.texture.lock() == pm) return;

    glBindTexture(GL_TEXTURE_2D, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    GLenum format;
    switch(pm->format) {
        case Pixmap::Format::RGBA: format = GL_RGBA;  break;
        case Pixmap::Format::RGB:  format = GL_RGB;   break;
        case Pixmap::Format::A:    format = GL_ALPHA; break;
        case Pixmap::Format::BGRA:
        case Pixmap::Format::BGR:
            ssassert(false, "Unexpected pixmap format");
    }

    if(!NeedsGl1V1Workaround()) {
        glTexImage2D(GL_TEXTURE_2D, 0, format, pm->width, pm->height, 0,
                     format, GL_UNSIGNED_BYTE, &pm->data[0]);
    } else {
        GLsizei width = RoundUpToPowerOfTwo(pm->width);
        GLsizei height = RoundUpToPowerOfTwo(pm->height);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pm->width, pm->height,
                        format, GL_UNSIGNED_BYTE, &pm->data[0]);
    }

    glEnable(GL_TEXTURE_2D);

    current.texture = pm;
}

//-----------------------------------------------------------------------------
// OpenGL's GL_LINES mode does not work on lines thicker than about 3 px,
// so we have to draw thicker lines using triangles.
//-----------------------------------------------------------------------------

void OpenGl1Renderer::DoFatLineEndcap(const Vector &p, const Vector &u, const Vector &v) {
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

    SelectPrimitive(GL_TRIANGLE_FAN);
    for(auto pc : Circle) {
        double c = pc[0], s = pc[1];
        ssglVertex3v(p.Plus(u.ScaledBy(c)).Plus(v.ScaledBy(s)));
    }
    UnSelectPrimitive();
}

void OpenGl1Renderer::DoFatLine(const Vector &a, const Vector &b, double width) {
    // The half-width of the line we're drawing.
    double hw = width / 2;
    Vector ab  = b.Minus(a);
    Vector gn = (camera.projRight).Cross(camera.projUp);
    Vector abn = (ab.Cross(gn)).WithMagnitude(1);
    abn = abn.Minus(gn.ScaledBy(gn.Dot(abn)));
    // So now abn is normal to the projection of ab into the screen, so the
    // line will always have constant thickness as the view is rotated.

    abn = abn.WithMagnitude(hw);
    ab  = gn.Cross(abn);
    ab  = ab. WithMagnitude(hw);

    // The body of a line is a quad
    SelectPrimitive(GL_QUADS);
    ssglVertex3v(a.Plus (abn));
    ssglVertex3v(b.Plus (abn));
    ssglVertex3v(b.Minus(abn));
    ssglVertex3v(a.Minus(abn));
    // And the line has two semi-circular end caps.
    DoFatLineEndcap(a, ab,              abn);
    DoFatLineEndcap(b, ab.ScaledBy(-1), abn);
}

void OpenGl1Renderer::DoLine(const Vector &a, const Vector &b, hStroke hcs) {
    if(a.Equals(b)) return;

    Stroke *stroke = SelectStroke(hcs);
    if(stroke->width <= 3.0) {
        SelectPrimitive(GL_LINES);
        ssglVertex3v(a);
        ssglVertex3v(b);
    } else {
        DoFatLine(a, b, stroke->width / camera.scale);
    }
}

void OpenGl1Renderer::DoPoint(Vector p, double d) {
    if(d <= 3.0) {
        Vector u = camera.projRight.WithMagnitude(d / 2.0 / camera.scale);

        SelectPrimitive(GL_LINES);
        ssglVertex3v(p.Minus(u));
        ssglVertex3v(p.Plus(u));
    } else {
        Vector u = camera.projRight.WithMagnitude(d / 2.0 / camera.scale);
        Vector v = camera.projUp.WithMagnitude(d / 2.0 / camera.scale);

        DoFatLineEndcap(p, u, v);
        DoFatLineEndcap(p, u.ScaledBy(-1.0), v);
    }
}

void OpenGl1Renderer::DoStippledLine(const Vector &a, const Vector &b, hStroke hcs) {
    Stroke *stroke = SelectStroke(hcs);

    const char *patternSeq;
    switch(stroke->stipplePattern) {
        case StipplePattern::CONTINUOUS:    DoLine(a, b, hcs);  return;
        case StipplePattern::SHORT_DASH:    patternSeq = "-  "; break;
        case StipplePattern::DASH:          patternSeq = "- ";  break;
        case StipplePattern::LONG_DASH:     patternSeq = "_ ";  break;
        case StipplePattern::DASH_DOT:      patternSeq = "-.";  break;
        case StipplePattern::DASH_DOT_DOT:  patternSeq = "-.."; break;
        case StipplePattern::DOT:           patternSeq = ".";   break;
        case StipplePattern::FREEHAND:      patternSeq = "~";   break;
        case StipplePattern::ZIGZAG:        patternSeq = "~__"; break;
    }

    Vector dir = b.Minus(a);
    double len = dir.Magnitude();
    dir = dir.WithMagnitude(1.0);

    const char *si = patternSeq;
    double end = len;
    double ss = stroke->stippleScale / 2.0;
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
                DoLine(a.Plus(dir.ScaledBy(start)), a.Plus(dir.ScaledBy(end)), hcs);
                end = max(end - 0.5 * ss, 0.0);
                break;

            case '_':
                end = max(end - 4.0 * ss, 0.0);
                DoLine(a.Plus(dir.ScaledBy(start)), a.Plus(dir.ScaledBy(end)), hcs);
                break;

            case '.':
                end = max(end - 0.5 * ss, 0.0);
                if(end == 0.0) break;
                DoPoint(a.Plus(dir.ScaledBy(end)), stroke->width);
                end = max(end - 0.5 * ss, 0.0);
                break;

            case '~': {
                Vector ab  = b.Minus(a);
                Vector gn = (camera.projRight).Cross(camera.projUp);
                Vector abn = (ab.Cross(gn)).WithMagnitude(1);
                abn = abn.Minus(gn.ScaledBy(gn.Dot(abn)));
                double pws = 2.0 * stroke->width / camera.scale;

                end = max(end - 0.5 * ss, 0.0);
                Vector aa = a.Plus(dir.ScaledBy(start));
                Vector bb = a.Plus(dir.ScaledBy(end))
                             .Plus(abn.ScaledBy(pws * (start - end) / (0.5 * ss)));
                DoLine(aa, bb, hcs);
                if(end == 0.0) break;

                start = end;
                end = max(end - 1.0 * ss, 0.0);
                aa = a.Plus(dir.ScaledBy(end))
                      .Plus(abn.ScaledBy(pws))
                      .Minus(abn.ScaledBy(2.0 * pws * (start - end) / ss));
                DoLine(bb, aa, hcs);
                if(end == 0.0) break;

                start = end;
                end = max(end - 0.5 * ss, 0.0);
                bb = a.Plus(dir.ScaledBy(end))
                      .Minus(abn.ScaledBy(pws))
                      .Plus(abn.ScaledBy(pws * (start - end) / (0.5 * ss)));
                DoLine(aa, bb, hcs);
                break;
            }

            default: ssassert(false, "Unexpected stipple pattern element");
        }
        if(*(++si) == 0) si = patternSeq;
    } while(end > 0.0);
}

//-----------------------------------------------------------------------------
// A canvas implemented using OpenGL 2 immediate mode.
//-----------------------------------------------------------------------------

void OpenGl1Renderer::DrawLine(const Vector &a, const Vector &b, hStroke hcs) {
    DoStippledLine(a, b, hcs);
}

void OpenGl1Renderer::DrawEdges(const SEdgeList &el, hStroke hcs) {
    for(const SEdge *e = el.l.First(); e; e = el.l.NextAfter(e)) {
        DoStippledLine(e->a, e->b, hcs);
    }
}

void OpenGl1Renderer::DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) {
    Vector projDir = camera.projRight.Cross(camera.projUp);
    switch(drawAs) {
        case DrawOutlinesAs::EMPHASIZED_AND_CONTOUR:
            for(const SOutline &o : ol.l) {
                if(o.IsVisible(projDir) || o.tag != 0) {
                    DoStippledLine(o.a, o.b, hcs);
                }
            }
            break;

        case DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR:
            for(const SOutline &o : ol.l) {
                if(!o.IsVisible(projDir) && o.tag != 0) {
                    DoStippledLine(o.a, o.b, hcs);
                }
            }
            break;

        case DrawOutlinesAs::CONTOUR_ONLY:
            for(const SOutline &o : ol.l) {
                if(o.IsVisible(projDir)) {
                    DoStippledLine(o.a, o.b, hcs);
                }
            }
            break;
    }
}

void OpenGl1Renderer::DrawVectorText(const std::string &text, double height,
                                     const Vector &o, const Vector &u, const Vector &v,
                                     hStroke hcs) {
    auto traceEdge = [&](Vector a, Vector b) { DoStippledLine(a, b, hcs); };
    VectorFont::Builtin()->Trace(height, o, u, v, text, traceEdge, camera);
}

void OpenGl1Renderer::DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                               hFill hcf) {
    SelectFill(hcf);
    SelectPrimitive(GL_QUADS);
    ssglVertex3v(a);
    ssglVertex3v(b);
    ssglVertex3v(c);
    ssglVertex3v(d);
}

void OpenGl1Renderer::DrawPoint(const Vector &o, double s, hFill hcf) {
    Vector r = camera.projRight.ScaledBy(s/camera.scale);
    Vector u = camera.projUp.ScaledBy(s/camera.scale);
    Vector a = o.Plus (r).Plus (u),
           b = o.Plus (r).Minus(u),
           c = o.Minus(r).Minus(u),
           d = o.Minus(r).Plus (u);
    DrawQuad(a, b, c, d, hcf);
}

#ifdef WIN32
#define SSGL_CALLBACK CALLBACK
#else
#define SSGL_CALLBACK
#endif
typedef void(SSGL_CALLBACK *GLUCallback)();

static void SSGL_CALLBACK Vertex(Vector *p) {
    ssglVertex3v(*p);
}
static void SSGL_CALLBACK Combine(double coords[3], void *vertexData[4],
                                  float weight[4], void **outData) {
    Vector *n = (Vector *)AllocTemporary(sizeof(Vector));
    n->x = coords[0];
    n->y = coords[1];
    n->z = coords[2];

    *outData = n;
}
void OpenGl1Renderer::DrawPolygon(const SPolygon &p, hFill hcf) {
    UnSelectPrimitive();
    SelectFill(hcf);

    GLUtesselator *gt = gluNewTess();
    gluTessCallback(gt, GLU_TESS_BEGIN,   (GLUCallback) glBegin);
    gluTessCallback(gt, GLU_TESS_VERTEX,  (GLUCallback) Vertex);
    gluTessCallback(gt, GLU_TESS_END,     (GLUCallback) glEnd);
    gluTessCallback(gt, GLU_TESS_COMBINE, (GLUCallback) Combine);

    gluTessProperty(gt, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);

    ssglNormal3v(p.normal);
    gluTessNormal(gt, p.normal.x, p.normal.y, p.normal.z);

    gluTessBeginPolygon(gt, NULL);
    for(const SContour &sc : p.l) {
        gluTessBeginContour(gt);
        for(const SPoint &sp : sc.l) {
            double ap[3] = { sp.p.x, sp.p.y, sp.p.z };
            gluTessVertex(gt, ap, (GLvoid *) &sp.p);
        }
        gluTessEndContour(gt);
    }
    gluTessEndPolygon(gt);

    gluDeleteTess(gt);
}

void OpenGl1Renderer::DrawMesh(const SMesh &m,
                               hFill hcfFront, hFill hcfBack, hStroke hcsTriangles) {
    UnSelectPrimitive();

    Fill *frontFill = SelectFill(hcfFront);
    ssglMaterialRGBA(GL_FRONT, frontFill->color);

    if(hcfBack.v != 0) {
        Fill *backFill = fills.FindById(hcfBack);
        ssassert(frontFill->layer  == backFill->layer &&
                 frontFill->zIndex == backFill->zIndex,
                 "frontFill and backFill should belong to the same depth range");
        ssassert(frontFill->pattern == backFill->pattern,
                 "frontFill and backFill should have the same pattern");
        ssglMaterialRGBA(GL_BACK, backFill->color);
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    } else {
        glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
    }

    RgbaColor frontColor = {};
    glEnable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);
    for(const STriangle &tr : m.l) {
        if(frontFill->color.IsEmpty()) {
            if(frontColor.IsEmpty() || !frontColor.Equals(tr.meta.color)) {
                frontColor = tr.meta.color;
                ssglMaterialRGBA(GL_FRONT, frontColor);
            }
        }

        if(tr.an.EqualsExactly(Vector::From(0, 0, 0))) {
            // Compute the normal from the vertices
            ssglNormal3v(tr.Normal());
            ssglVertex3v(tr.a);
            ssglVertex3v(tr.b);
            ssglVertex3v(tr.c);
        } else {
            // Use the exact normals that are specified
            ssglNormal3v(tr.an);
            ssglVertex3v(tr.a);
            ssglNormal3v(tr.bn);
            ssglVertex3v(tr.b);
            ssglNormal3v(tr.cn);
            ssglVertex3v(tr.c);
        }
    }
    glEnd();
    glDisable(GL_LIGHTING);

    if(hcsTriangles.v != 0) {
        Stroke *triangleStroke = SelectStroke(hcsTriangles);
        ssassert(triangleStroke->width == 1 &&
                 triangleStroke->stipplePattern == StipplePattern::CONTINUOUS &&
                 triangleStroke->stippleScale == 0.0,
                 "Triangle stroke must match predefined OpenGL parameters");

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBegin(GL_TRIANGLES);
        for(const STriangle &tr : m.l) {
            ssglVertex3v(tr.a);
            ssglVertex3v(tr.b);
            ssglVertex3v(tr.c);
        }
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void OpenGl1Renderer::DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) {
    SelectFill(hcf);
    SelectPrimitive(GL_TRIANGLES);
    size_t facesSize = faces.size();
    for(const STriangle &tr : m.l) {
        uint32_t face = tr.meta.face;
        for(size_t j = 0; j < facesSize; j++) {
            if(faces[j] != face) continue;
            ssglVertex3v(tr.a);
            ssglVertex3v(tr.b);
            ssglVertex3v(tr.c);
            break;
        }
    }
}

void OpenGl1Renderer::DrawPixmap(std::shared_ptr<const Pixmap> pm,
                                 const Vector &o, const Vector &u, const Vector &v,
                                 const Point2d &ta, const Point2d &tb, hFill hcf) {
    double xfactor = 1.0,
           yfactor = 1.0;
    if(NeedsGl1V1Workaround()) {
        xfactor = (double)pm->width / RoundUpToPowerOfTwo(pm->width);
        yfactor = (double)pm->height / RoundUpToPowerOfTwo(pm->height);
    }

    UnSelectPrimitive();
    SelectFill(hcf);
    SelectTexture(pm);
    SelectPrimitive(GL_QUADS);
    glTexCoord2d(ta.x * xfactor, ta.y * yfactor);
    ssglVertex3v(o);
    glTexCoord2d(ta.x * xfactor, tb.y * yfactor);
    ssglVertex3v(o.Plus(v));
    glTexCoord2d(tb.x * xfactor, tb.y * yfactor);
    ssglVertex3v(o.Plus(u).Plus(v));
    glTexCoord2d(tb.x * xfactor, ta.y * yfactor);
    ssglVertex3v(o.Plus(u));
}

void OpenGl1Renderer::InvalidatePixmap(std::shared_ptr<const Pixmap> pm) {
    if(current.texture.lock() == pm) {
        current.texture.reset();
    }
}

void OpenGl1Renderer::UpdateProjection(bool flip) {
    UnSelectPrimitive();

    glViewport(0, 0, camera.width, camera.height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glScaled(camera.scale * 2.0 / camera.width,
             camera.scale * 2.0 / camera.height,
             camera.scale * 1.0 / 30000);

    double mat[16];
    // Last thing before display is to apply the perspective
    double clp = camera.tangent * camera.scale;
    double sy = flip ? -1.0 : 1.0;
    MakeMatrix(mat, 1,              0,              0,              0,
                    0,             sy,              0,              0,
                    0,              0,              1,              0,
                    0,              0,              clp,            1);
    glMultMatrixd(mat);
    // Before that, we apply the rotation
    Vector projRight = camera.projRight,
           projUp    = camera.projUp,
           n         = camera.projUp.Cross(camera.projRight);
    MakeMatrix(mat, projRight.x,    projRight.y,    projRight.z,    0,
                    projUp.x,       projUp.y,       projUp.z,       0,
                    n.x,            n.y,            n.z,            0,
                    0,              0,              0,              1);
    glMultMatrixd(mat);
    // And before that, the translation
    Vector offset = camera.offset;
    MakeMatrix(mat, 1,              0,              0,              offset.x,
                    0,              1,              0,              offset.y,
                    0,              0,              1,              offset.z,
                    0,              0,              0,              1);
    glMultMatrixd(mat);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void OpenGl1Renderer::BeginFrame() {
    glEnable(GL_NORMALIZE);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    // don't enable GL_POLYGON_SMOOTH; that looks ugly on some graphics cards,
    // drawn with leaks in the mesh

    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    if(EXACT(lighting.lightIntensity[0] != 0.0)) {
        glEnable(GL_LIGHT0);
        GLfloat f = (GLfloat)lighting.lightIntensity[0];
        GLfloat li0[] = { f, f, f, 1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, li0);
        glLightfv(GL_LIGHT0, GL_SPECULAR, li0);

        Vector ld = camera.VectorFromProjs(lighting.lightDirection[0]);
        GLfloat ld0[4] = { (GLfloat)ld.x, (GLfloat)ld.y, (GLfloat)ld.z, 0 };
        glLightfv(GL_LIGHT0, GL_POSITION, ld0);
    }

    if(EXACT(lighting.lightIntensity[1] != 0.0)) {
        glEnable(GL_LIGHT1);
        GLfloat f = (GLfloat)lighting.lightIntensity[1];
        GLfloat li0[] = { f, f, f, 1.0f };
        glLightfv(GL_LIGHT1, GL_DIFFUSE, li0);
        glLightfv(GL_LIGHT1, GL_SPECULAR, li0);

        Vector ld = camera.VectorFromProjs(lighting.lightDirection[1]);
        GLfloat ld0[4] = { (GLfloat)ld.x, (GLfloat)ld.y, (GLfloat)ld.z, 0 };
        glLightfv(GL_LIGHT1, GL_POSITION, ld0);
    }

    if(EXACT(lighting.ambientIntensity != 0.0)) {
        GLfloat ambient[4] = { (float)lighting.ambientIntensity,
                               (float)lighting.ambientIntensity,
                               (float)lighting.ambientIntensity, 1 };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    }

    glClearColor(lighting.backgroundColor.redF(),  lighting.backgroundColor.greenF(),
                 lighting.backgroundColor.blueF(), lighting.backgroundColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT);

    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void OpenGl1Renderer::EndFrame() {
    UnSelectPrimitive();
    glFlush();

    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        dbp("glGetError() == 0x%X %s", error, gluErrorString(error));
    }
}

std::shared_ptr<Pixmap> OpenGl1Renderer::ReadFrame() {
    std::shared_ptr<Pixmap> pixmap =
        Pixmap::Create(Pixmap::Format::RGB, (size_t)camera.width, (size_t)camera.height);
    glReadPixels(0, 0, camera.width, camera.height, GL_RGB, GL_UNSIGNED_BYTE, &pixmap->data[0]);
    return pixmap;
}

void OpenGl1Renderer::GetIdent(const char **vendor, const char **renderer, const char **version) {
    *vendor   = (const char *)glGetString(GL_VENDOR);
    *renderer = (const char *)glGetString(GL_RENDERER);
    *version  = (const char *)glGetString(GL_VERSION);
}

}
