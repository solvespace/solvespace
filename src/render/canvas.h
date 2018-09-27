//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_CANVAS_H
#define SOLVESPACE_CANVAS_H

#include <soutline.h>
#include "dsc.h"
#include "camera.h"
#include "stipplepattern.h"
#include "resource.h"

namespace SolveSpace {

class BatchCanvas;
class SMesh;

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
        LAST = FRONT
    };

    // The outlines are the collection of all edges that may be drawn.
    // Outlines can be classified as emphasized or not; emphasized outlines indicate an abrupt
    // change in the surface curvature. These are indicated by the SOutline tag.
    // Outlines can also be classified as contour or not; contour outlines indicate the boundary
    // of the filled mesh. Whether an outline is a part of contour or not depends on point of view.
    enum class DrawOutlinesAs {
        EMPHASIZED_AND_CONTOUR      = 0, // Both emphasized and contour outlines
        EMPHASIZED_WITHOUT_CONTOUR  = 1, // Emphasized outlines except those also belonging to contour
        CONTOUR_ONLY                = 2  // Contour outlines only
    };

    // Stroke widths, etc, can be scale-invariant (in pixels) or scale-dependent (in millimeters).
    enum class Unit {
        MM,
        PX
    };

    class Stroke {
    public:
        hStroke         h;

        Layer           layer;
        int             zIndex;
        RgbaColor       color;
        double          width;
        Unit            unit;
        StipplePattern  stipplePattern;
        double          stippleScale;

        void Clear() { *this = {}; }
        bool Equals(const Stroke &other) const;

        double WidthMm(const Camera &camera) const;
        double WidthPx(const Camera &camera) const;
        double StippleScaleMm(const Camera &camera) const;
        double StippleScalePx(const Camera &camera) const;
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
        std::shared_ptr<const Pixmap> texture;

        void Clear() { *this = {}; }
        bool Equals(const Fill &other) const;
    };

    IdList<Stroke, hStroke> strokes = {};
    IdList<Fill,   hFill>   fills   = {};
    BitmapFont bitmapFont = {};

    virtual void Clear();

    hStroke GetStroke(const Stroke &stroke);
    hFill GetFill(const Fill &fill);
    BitmapFont *GetBitmapFont();

    virtual const Camera &GetCamera() const = 0;

    virtual void DrawLine(const Vector &a, const Vector &b, hStroke hcs) = 0;
    virtual void DrawEdges(const SEdgeList &el, hStroke hcs) = 0;
    virtual bool DrawBeziers(const SBezierList &bl, hStroke hcs) = 0;
    virtual void DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) = 0;
    virtual void DrawVectorText(const std::string &text, double height,
                                const Vector &o, const Vector &u, const Vector &v,
                                hStroke hcs) = 0;

    virtual void DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                          hFill hcf) = 0;
    virtual void DrawPoint(const Vector &o, hStroke hcs) = 0;
    virtual void DrawPolygon(const SPolygon &p, hFill hcf) = 0;
    virtual void DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack = {}) = 0;
    virtual void DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) = 0;

    virtual void DrawPixmap(std::shared_ptr<const Pixmap> pm,
                            const Vector &o, const Vector &u, const Vector &v,
                            const Point2d &ta, const Point2d &tb, hFill hcf) = 0;
    virtual void InvalidatePixmap(std::shared_ptr<const Pixmap> pm) = 0;

    virtual std::shared_ptr<BatchCanvas> CreateBatch();
};


}

#endif //SOLVESPACE_CANVAS_H
