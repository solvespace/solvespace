//-----------------------------------------------------------------------------
// Backend-agnostic rendering interface, and various backends we use.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_RENDER_H
#define SOLVESPACE_RENDER_H

#include "dsc.h"
#include "srf/surface.h"
#include "resource.h"
#include "soutline.h"
#include "camera.h"
#include "canvas.h"
#include "stipplepattern.h"

//-----------------------------------------------------------------------------
// Interfaces common for all renderers
//-----------------------------------------------------------------------------
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

namespace SolveSpace {

class BatchCanvas;


// A canvas that performs picking against drawn geometry.
class ObjectPicker : public Canvas {
public:
    Camera      camera      = {};
    // Configuration.
    Point2d     point       = {};
    double      selRadius   = 0.0;
    // Picking state.
    double      minDistance = 0.0;
    int         maxZIndex   = 0;
    uint32_t    position    = 0;

    const Camera &GetCamera() const override { return camera; }

    void DrawLine(const Vector &a, const Vector &b, hStroke hcs) override;
    void DrawEdges(const SEdgeList &el, hStroke hcs) override;
    bool DrawBeziers(const SBezierList &bl, hStroke hcs) override { return false; }
    void DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) override;
    void DrawVectorText(const std::string &text, double height,
                        const Vector &o, const Vector &u, const Vector &v,
                        hStroke hcs) override;

    void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                  hFill hcf) override;
    void DrawPoint(const Vector &o, hStroke hcs) override;
    void DrawPolygon(const SPolygon &p, hFill hcf) override;
    void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack) override;
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

// A canvas that renders onto a 2d surface, performing z-index sorting, occlusion testing, etc,
// on the CPU.
class SurfaceRenderer : public ViewportCanvas {
public:
    Camera      camera   = {};
    Lighting    lighting = {};
    // Chord tolerance, for converting beziers to pwl.
    double      chordTolerance = 0.0;
    // Render lists.
    handle_map<hStroke, SEdgeList>   edges;
    handle_map<hStroke, SBezierList> beziers;
    SMesh       mesh = {};
    // State.
    BBox        bbox = {};

    void Clear() override;

    // Canvas interface.
    const Camera &GetCamera() const override { return camera; }

    // ViewportCanvas interface.
    void SetCamera(const Camera &camera) override { this->camera = camera; };
    void SetLighting(const Lighting &lighting) override { this->lighting = lighting; }

    void DrawLine(const Vector &a, const Vector &b, hStroke hcs) override;
    void DrawEdges(const SEdgeList &el, hStroke hcs) override;
    bool DrawBeziers(const SBezierList &bl, hStroke hcs) override;
    void DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) override;
    void DrawVectorText(const std::string &text, double height,
                        const Vector &o, const Vector &u, const Vector &v,
                        hStroke hcs) override;

    void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                  hFill hcf) override;
    void DrawPoint(const Vector &o, hStroke hcs) override;
    void DrawPolygon(const SPolygon &p, hFill hcf) override;
    void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack) override;
    void DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) override;

    void DrawPixmap(std::shared_ptr<const Pixmap> pm,
                    const Vector &o, const Vector &u, const Vector &v,
                    const Point2d &ta, const Point2d &tb, hFill hcf) override;
    void InvalidatePixmap(std::shared_ptr<const Pixmap> pm) override;

    // Geometry manipulation.
    void CalculateBBox();
    void ConvertBeziersToEdges();
    void CullOccludedStrokes();

    // Renderer operations.
    void OutputInPaintOrder();

    virtual bool CanOutputCurves() const = 0;
    virtual bool CanOutputTriangles() const = 0;

    virtual void OutputStart() = 0;
    virtual void OutputBezier(const SBezier &b, hStroke hcs) = 0;
    virtual void OutputTriangle(const STriangle &tr) = 0;
    virtual void OutputEnd() = 0;

    void OutputBezierAsNonrationalCubic(const SBezier &b, hStroke hcs);
};

//-----------------------------------------------------------------------------
// 2d renderers
//-----------------------------------------------------------------------------

class CairoRenderer : public SurfaceRenderer {
public:
    cairo_t     *context  = NULL;
    // Renderer configuration.
    bool        antialias = false;
    // Renderer state.
    struct {
        hStroke     hcs;
    } current = {};

    void Clear() override;

    void NewFrame() override {}
    void FlushFrame() override;
    std::shared_ptr<Pixmap> ReadFrame() override;

    void GetIdent(const char **vendor, const char **renderer, const char **version) override;

    void SelectStroke(hStroke hcs);
    void MoveTo(Vector p);
    void FinishPath();

    bool CanOutputCurves() const override { return true; }
    bool CanOutputTriangles() const override { return true; }

    void OutputStart() override;
    void OutputBezier(const SBezier &b, hStroke hcs) override;
    void OutputTriangle(const STriangle &tr) override;
    void OutputEnd() override;
};

class CairoPixmapRenderer : public CairoRenderer {
public:
    std::shared_ptr<Pixmap>  pixmap;

    cairo_surface_t         *surface = NULL;

    void Init();
    void Clear() override;

    std::shared_ptr<Pixmap> ReadFrame() override;
};

//-----------------------------------------------------------------------------
// 3d renderers
//-----------------------------------------------------------------------------

// An offscreen renderer based on OpenGL framebuffers.
class GlOffscreen {
public:
    unsigned int          framebuffer = 0;
    unsigned int          colorRenderbuffer = 0;
    unsigned int          depthRenderbuffer = 0;
    std::vector<uint8_t>  data;

    bool Render(int width, int height, std::function<void()> renderFn);
    void Clear();
};

//-----------------------------------------------------------------------------
// Factories
//-----------------------------------------------------------------------------

std::shared_ptr<ViewportCanvas> CreateRenderer();

}

#endif
