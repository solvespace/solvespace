//-----------------------------------------------------------------------------
// Backend-agnostic rendering interface, and various backends we use.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_RENDER_H
#define SOLVESPACE_RENDER_H

enum class StipplePattern : uint32_t;

// A mapping from 3d sketch coordinates to 2d screen coordinates, using
// an axonometric projection.
class Camera {
public:
    size_t      width, height;
    Vector      offset;
    Vector      projRight;
    Vector      projUp;
    double      scale;
    double      tangent;
    bool        hasPixels;

    bool IsPerspective() const { return tangent != 0.0; }

    Point2d ProjectPoint(Vector p) const;
    Vector ProjectPoint3(Vector p) const;
    Vector ProjectPoint4(Vector p, double *w) const;
    Vector UnProjectPoint(Point2d p) const;
    Vector UnProjectPoint3(Vector p) const;
    Vector VectorFromProjs(Vector rightUpForward) const;
    Vector AlignToPixelGrid(Vector v) const;

    void LoadIdentity();
    void NormalizeProjectionVectors();
};

// A description of scene lighting.
class Lighting {
public:
    RgbaColor   backgroundColor;
    double      ambientIntensity;
    double      lightIntensity[2];
    Vector      lightDirection[2];
};

// An interface for populating a drawing area with geometry.
class Canvas {
public:
    // Stroke and fill styles are addressed with handles to be able to quickly
    // group geometry into indexed draw calls.
    class hStroke {
    public:
        uint32_t v;
    };

    class hFill {
    public:
        uint32_t v;
    };

    // The layer of a geometry describes how it occludes other geometry.
    // Within a layer, geometry with higher z-index occludes geometry with lower z-index,
    // or geometry drawn earlier if z-indexes match.
    enum class Layer {
        NORMAL,     // Occluded by geometry with lower Z coordinate
        OCCLUDED,   // Only drawn over geometry with lower Z coordinate
        DEPTH_ONLY, // Like NORMAL, but only affects future occlusion, not color
        BACK,       // Always drawn below all other geometry
        FRONT,      // Always drawn above all other geometry
    };

    class Stroke {
    public:
        hStroke         h;

        Layer           layer;
        int             zIndex;
        RgbaColor       color;
        double          width;
        StipplePattern  stipplePattern;
        double          stippleScale;

        bool Equals(const Stroke &other) const;
    };

    enum class FillPattern {
        SOLID, CHECKERED_A, CHECKERED_B
    };

    class Fill {
    public:
        hFill           h;

        Layer           layer;
        int             zIndex;
        RgbaColor       color;
        FillPattern     pattern;

        bool Equals(const Fill &other) const;
    };

    IdList<Stroke, hStroke> strokes;
    IdList<Fill,   hFill>   fills;

    Canvas() : strokes(), fills() {};

    hStroke GetStroke(const Stroke &stroke);
    hFill GetFill(const Fill &fill);

    virtual const Camera &GetCamera() const = 0;

    virtual void DrawLine(const Vector &a, const Vector &b, hStroke hcs) = 0;
    virtual void DrawEdges(const SEdgeList &el, hStroke hcs) = 0;
    virtual bool DrawBeziers(const SBezierList &bl, hStroke hcs) = 0;
    virtual void DrawOutlines(const SOutlineList &ol, hStroke hcs) = 0;
    virtual void DrawVectorText(const std::string &text, double height,
                                const Vector &o, const Vector &u, const Vector &v,
                                hStroke hcs) = 0;

    virtual void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                          hFill hcf) = 0;
    virtual void DrawPoint(const Vector &o, double d, hFill hcf) = 0;
    virtual void DrawPolygon(const SPolygon &p, hFill hcf) = 0;
    virtual void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack = {},
                          hStroke hcsTriangles = {}) = 0;
    virtual void DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) = 0;

    virtual void DrawPixmap(std::shared_ptr<const Pixmap> pm,
                            const Vector &o, const Vector &u, const Vector &v,
                            const Point2d &ta, const Point2d &tb, hFill hcf) = 0;
    virtual void InvalidatePixmap(std::shared_ptr<const Pixmap> pm) = 0;
};

// A wrapper around Canvas that simplifies drawing UI in screen coordinates.
class UiCanvas {
public:
    Canvas     *canvas;
    bool        flip;

    UiCanvas() : canvas(), flip() {};

    void DrawLine(int x1, int y1, int x2, int y2, RgbaColor color, int width = 1);
    void DrawRect(int l, int r, int t, int b, RgbaColor fillColor, RgbaColor outlineColor);
    void DrawPixmap(std::shared_ptr<const Pixmap> pm, int x, int y);
    void DrawBitmapChar(char32_t codepoint, int x, int y, RgbaColor color);
    void DrawBitmapText(const std::string &str, int x, int y, RgbaColor color);

    int Flip(int y) const { return flip ? (int)canvas->GetCamera().height - y : y; }
};

// A canvas that performs picking against drawn geometry.
class ObjectPicker : public Canvas {
public:
    Camera      camera;
    // Configuration.
    Point2d     point;
    double      selRadius;
    // Picking state.
    double      minDistance;
    int         maxZIndex;
    uint32_t    position;

    ObjectPicker() : camera(), point(), selRadius(),
                     minDistance(), maxZIndex(), position() {}

    const Camera &GetCamera() const override { return camera; }

    void DrawLine(const Vector &a, const Vector &b, hStroke hcs) override;
    void DrawEdges(const SEdgeList &el, hStroke hcs) override;
    bool DrawBeziers(const SBezierList &bl, hStroke hcs) override { return false; }
    void DrawOutlines(const SOutlineList &ol, hStroke hcs) override;
    void DrawVectorText(const std::string &text, double height,
                        const Vector &o, const Vector &u, const Vector &v,
                        hStroke hcs) override;

    void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                  hFill hcf) override;
    void DrawPoint(const Vector &o, double s, hFill hcf) override;
    void DrawPolygon(const SPolygon &p, hFill hcf) override;
    void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack, hStroke hcsTriangles) override;
    void DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) override;

    void DrawPixmap(std::shared_ptr<const Pixmap> pm,
                    const Vector &o, const Vector &u, const Vector &v,
                    const Point2d &ta, const Point2d &tb, hFill hcf) override;
    void InvalidatePixmap(std::shared_ptr<const Pixmap> pm) override {}

    void DoCompare(double distance, int zIndex, int comparePosition = 0);
    void DoQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                int zIndex, int comparePosition = 0);

    bool Pick(std::function<void()> drawFn);
};

// A canvas that uses the core OpenGL profile, for desktop systems.
class OpenGl1Renderer : public Canvas {
public:
    Camera      camera;
    Lighting    lighting;
    // Cached OpenGL state.
    struct {
        bool        drawing;
        unsigned    mode; // GLenum, but we don't include GL.h globally
        hStroke     hcs;
        Stroke     *stroke;
        hFill       hcf;
        Fill       *fill;
        std::weak_ptr<const Pixmap> texture;
    } current;

    OpenGl1Renderer() : camera(), lighting(), current() {}

    const Camera &GetCamera() const override { return camera; }

    void DrawLine(const Vector &a, const Vector &b, hStroke hcs) override;
    void DrawEdges(const SEdgeList &el, hStroke hcs) override;
    bool DrawBeziers(const SBezierList &bl, hStroke hcs) override { return false; }
    void DrawOutlines(const SOutlineList &ol, hStroke hcs) override;
    void DrawVectorText(const std::string &text, double height,
                        const Vector &o, const Vector &u, const Vector &v,
                        hStroke hcs) override;

    void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                  hFill hcf) override;
    void DrawPoint(const Vector &o, double s, hFill hcf) override;
    void DrawPolygon(const SPolygon &p, hFill hcf) override;
    void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack, hStroke hcsTriangles) override;
    void DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) override;
    void DrawPixmap(std::shared_ptr<const Pixmap> pm,
                    const Vector &o, const Vector &u, const Vector &v,
                    const Point2d &ta, const Point2d &tb, hFill hcf) override;
    void InvalidatePixmap(std::shared_ptr<const Pixmap> pm) override;

    void SelectPrimitive(unsigned mode);
    void UnSelectPrimitive();
    Stroke *SelectStroke(hStroke hcs);
    Fill *SelectFill(hFill hcf);
    void SelectTexture(std::shared_ptr<const Pixmap> pm);
    void DoFatLineEndcap(const Vector &p, const Vector &u, const Vector &v);
    void DoFatLine(const Vector &a, const Vector &b, double width);
    void DoLine(const Vector &a, const Vector &b, hStroke hcs);
    void DoPoint(Vector p, double radius);
    void DoStippledLine(const Vector &a, const Vector &b, hStroke hcs);

    void UpdateProjection(bool flip = FLIP_FRAMEBUFFER);
    void BeginFrame();
    void EndFrame();
    std::shared_ptr<Pixmap> ReadFrame();

    static void GetIdent(const char **vendor, const char **renderer, const char **version);
};

#endif
