//-----------------------------------------------------------------------------
// Backend-agnostic rendering interface, and various backends we use.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Camera transformations.
//-----------------------------------------------------------------------------

Point2d Camera::ProjectPoint(Vector p) const {
    Vector p3 = ProjectPoint3(p);
    Point2d p2 = { p3.x, p3.y };
    return p2;
}

Vector Camera::ProjectPoint3(Vector p) const {
    double w;
    Vector r = ProjectPoint4(p, &w);
    return r.ScaledBy(scale/w);
}

Vector Camera::ProjectPoint4(Vector p, double *w) const {
    p = p.Plus(offset);

    Vector r;
    r.x = p.Dot(projRight);
    r.y = p.Dot(projUp);
    r.z = p.Dot(projUp.Cross(projRight));

    *w = 1 + r.z*tangent*scale;
    return r;
}

Vector Camera::UnProjectPoint(Point2d p) const {
    Vector orig = offset.ScaledBy(-1);

    // Note that we're ignoring the effects of perspective. Since our returned
    // point has the same component normal to the screen as the offset, it
    // will have z = 0 after the rotation is applied, thus w = 1. So this is
    // correct.
    orig = orig.Plus(projRight.ScaledBy(p.x / scale)).Plus(
                     projUp.   ScaledBy(p.y / scale));
    return orig;
}

Vector Camera::UnProjectPoint3(Vector p) const {
    p.z = p.z / (scale - p.z * tangent * scale);
    double w = 1 + p.z * tangent * scale;
    p.x *= w / scale;
    p.y *= w / scale;

    Vector orig = offset.ScaledBy(-1);
    orig = orig.Plus(projRight.ScaledBy(p.x)).Plus(
                     projUp.   ScaledBy(p.y).Plus(
                     projUp.Cross(projRight). ScaledBy(p.z)));
    return orig;
}

Vector Camera::VectorFromProjs(Vector rightUpForward) const {
    Vector n = projRight.Cross(projUp);

    Vector r = (projRight.ScaledBy(rightUpForward.x));
    r =  r.Plus(projUp.ScaledBy(rightUpForward.y));
    r =  r.Plus(n.ScaledBy(rightUpForward.z));
    return r;
}

Vector Camera::AlignToPixelGrid(Vector v) const {
    if(!gridFit) return v;

    v = ProjectPoint3(v);
    v.x = floor(v.x) + 0.5;
    v.y = floor(v.y) + 0.5;
    return UnProjectPoint3(v);
}

SBezier Camera::ProjectBezier(SBezier b) const {
    Quaternion q = Quaternion::From(projRight, projUp);
    q = q.Inverse();
    // we want Q*(p - o) = Q*p - Q*o
    b = b.TransformedBy(q.Rotate(offset).ScaledBy(scale), q, scale);
    for(int i = 0; i <= b.deg; i++) {
        Vector4 ct = Vector4::From(b.weight[i], b.ctrl[i]);
        // so the desired curve, before perspective, is
        //    (x/w, y/w, z/w)
        // and after perspective is
        //    ((x/w)/(1 - (z/w)*tangent, ...
        //  = (x/(w - z*tangent), ...
        // so we want to let w' = w - z*tangent
        ct.w = ct.w - ct.z*tangent;

        b.ctrl[i] = ct.PerspectiveProject();
        b.weight[i] = ct.w;
    }
    return b;
}

void Camera::LoadIdentity() {
    offset    = { 0.0, 0.0, 0.0 };
    projRight = { 1.0, 0.0, 0.0 };
    projUp    = { 0.0, 1.0, 0.0 };
    scale     = 1.0;
    tangent   = 0.0;
}

void Camera::NormalizeProjectionVectors() {
    if(projRight.Magnitude() < LENGTH_EPS) {
        projRight = Vector::From(1, 0, 0);
    }

    Vector norm = projRight.Cross(projUp);
    // If projRight and projUp somehow ended up parallel, then pick an
    // arbitrary projUp normal to projRight.
    if(norm.Magnitude() < LENGTH_EPS) {
        norm = projRight.Normal(0);
    }
    projUp = norm.Cross(projRight);

    projUp = projUp.WithMagnitude(1);
    projRight = projRight.WithMagnitude(1);
}

//-----------------------------------------------------------------------------
// Stroke and fill caching.
//-----------------------------------------------------------------------------

bool Canvas::Stroke::Equals(const Stroke &other) const {
    return (layer  == other.layer &&
            zIndex == other.zIndex &&
            color.Equals(other.color) &&
            width == other.width &&
            unit == other.unit &&
            stipplePattern == other.stipplePattern &&
            stippleScale == other.stippleScale);
}

double Canvas::Stroke::WidthMm(const Camera &camera) const {
    switch(unit) {
        case Canvas::Unit::MM:
            return width;
        case Canvas::Unit::PX:
            return width / camera.scale;
        default:
            ssassert(false, "Unexpected unit");
    }
}

double Canvas::Stroke::WidthPx(const Camera &camera) const {
    switch(unit) {
        case Canvas::Unit::MM:
            return width * camera.scale;
        case Canvas::Unit::PX:
            return width;
        default:
            ssassert(false, "Unexpected unit");
    }
}

double Canvas::Stroke::StippleScaleMm(const Camera &camera) const {
    switch(unit) {
        case Canvas::Unit::MM:
            return stippleScale;
        case Canvas::Unit::PX:
            return stippleScale / camera.scale;
        default:
            ssassert(false, "Unexpected unit");
    }
}

double Canvas::Stroke::StippleScalePx(const Camera &camera) const {
    switch(unit) {
        case Canvas::Unit::MM:
            return stippleScale * camera.scale;
        case Canvas::Unit::PX:
            return stippleScale;
        default:
            ssassert(false, "Unexpected unit");
    }
}

bool Canvas::Fill::Equals(const Fill &other) const {
    return (layer  == other.layer &&
            zIndex == other.zIndex &&
            color.Equals(other.color) &&
            pattern == other.pattern &&
            texture == other.texture);
}

void Canvas::Clear() {
    strokes.Clear();
    fills.Clear();
}

Canvas::hStroke Canvas::GetStroke(const Stroke &stroke) {
    for(const Stroke &s : strokes) {
        if(s.Equals(stroke)) return s.h;
    }
    Stroke strokeCopy = stroke;
    return strokes.AddAndAssignId(&strokeCopy);
}

Canvas::hFill Canvas::GetFill(const Fill &fill) {
    for(const Fill &f : fills) {
        if(f.Equals(fill)) return f.h;
    }
    Fill fillCopy = fill;
    return fills.AddAndAssignId(&fillCopy);
}

BitmapFont *Canvas::GetBitmapFont() {
    if(bitmapFont.IsEmpty()) {
        bitmapFont = BitmapFont::Create();
    }
    return &bitmapFont;
}

std::shared_ptr<BatchCanvas> Canvas::CreateBatch() {
    return std::shared_ptr<BatchCanvas>();
}

//-----------------------------------------------------------------------------
// An interface for view-independent visualization
//-----------------------------------------------------------------------------

const Camera &BatchCanvas::GetCamera() const {
    ssassert(false, "Geometry drawn on BatchCanvas must be independent from camera");
}

//-----------------------------------------------------------------------------
// A wrapper around Canvas that simplifies drawing UI in screen coordinates
//-----------------------------------------------------------------------------

void UiCanvas::DrawLine(int x1, int y1, int x2, int y2, RgbaColor color, int width, int zIndex) {
    Vector va = { (double)x1 + 0.5, (double)Flip(y1) + 0.5, 0.0 },
           vb = { (double)x2 + 0.5, (double)Flip(y2) + 0.5, 0.0 };

    Canvas::Stroke stroke = {};
    stroke.layer  = Canvas::Layer::NORMAL;
    stroke.zIndex = zIndex;
    stroke.width  = (double)width;
    stroke.color  = color;
    stroke.unit   = Canvas::Unit::PX;
    Canvas::hStroke hcs = canvas->GetStroke(stroke);

    canvas->DrawLine(va, vb, hcs);
}

void UiCanvas::DrawRect(int l, int r, int t, int b, RgbaColor fillColor, RgbaColor outlineColor,
                        int zIndex) {
    Vector va = { (double)l + 0.5, (double)Flip(b) + 0.5, 0.0 },
           vb = { (double)l + 0.5, (double)Flip(t) + 0.5, 0.0 },
           vc = { (double)r + 0.5, (double)Flip(t) + 0.5, 0.0 },
           vd = { (double)r + 0.5, (double)Flip(b) + 0.5, 0.0 };

    if(!fillColor.IsEmpty()) {
        Canvas::Fill fill = {};
        fill.layer  = Canvas::Layer::NORMAL;
        fill.zIndex = zIndex;
        fill.color  = fillColor;
        Canvas::hFill hcf = canvas->GetFill(fill);

        canvas->DrawQuad(va, vb, vc, vd, hcf);
    }

    if(!outlineColor.IsEmpty()) {
        Canvas::Stroke stroke = {};
        stroke.layer  = Canvas::Layer::NORMAL;
        stroke.zIndex = zIndex;
        stroke.width  = 1.0;
        stroke.color  = outlineColor;
        stroke.unit   = Canvas::Unit::PX;
        Canvas::hStroke hcs = canvas->GetStroke(stroke);

        canvas->DrawLine(va, vb, hcs);
        canvas->DrawLine(vb, vc, hcs);
        canvas->DrawLine(vc, vd, hcs);
        canvas->DrawLine(vd, va, hcs);
    }
}

void UiCanvas::DrawPixmap(std::shared_ptr<const Pixmap> pm, int x, int y, int zIndex) {
    Canvas::Fill fill = {};
    fill.layer  = Canvas::Layer::NORMAL;
    fill.zIndex = zIndex;
    fill.color  = { 255, 255, 255, 255 };
    Canvas::hFill hcf = canvas->GetFill(fill);

    canvas->DrawPixmap(pm,
                       { (double)x, (double)(flip ? Flip(y) - pm->height : y), 0.0 },
                       { (double)pm->width,  0.0, 0.0 },
                       { 0.0, (double)pm->height, 0.0 },
                       { 0.0, 1.0 },
                       { 1.0, 0.0 },
                       hcf);
}

void UiCanvas::DrawBitmapChar(char32_t codepoint, int x, int y, RgbaColor color, int zIndex) {
    BitmapFont *font = canvas->GetBitmapFont();

    Canvas::Fill fill = {};
    fill.layer  = Canvas::Layer::NORMAL;
    fill.zIndex = zIndex;
    fill.color  = color;
    Canvas::hFill hcf = canvas->GetFill(fill);

    if(codepoint >= 0xe000 && codepoint <= 0xefff) {
        // Special character, like a checkbox or a radio button
        x -= 3;
    }

    double s0, t0, s1, t1;
    size_t w, h;
    font->LocateGlyph(codepoint, &s0, &t0, &s1, &t1, &w, &h);
    if(font->textureUpdated) {
        // LocateGlyph modified the texture, reload it.
        canvas->InvalidatePixmap(font->texture);
        font->textureUpdated = false;
    }

    canvas->DrawPixmap(font->texture,
                       { (double)x, (double)Flip(y), 0.0 },
                       { (double)w,  0.0, 0.0 },
                       { 0.0, (double) h, 0.0 },
                       { s0, t1 },
                       { s1, t0 },
                       hcf);
}

void UiCanvas::DrawBitmapText(const std::string &str, int x, int y, RgbaColor color, int zIndex) {
    BitmapFont *font = canvas->GetBitmapFont();

    for(char32_t codepoint : ReadUTF8(str)) {
        DrawBitmapChar(codepoint, x, y, color, zIndex);
        x += (int)font->GetWidth(codepoint) * 8;
    }
}

//-----------------------------------------------------------------------------
// A canvas that performs picking against drawn geometry.
//-----------------------------------------------------------------------------

void ObjectPicker::DoCompare(double distance, int zIndex, int comparePosition) {
    if(distance > selRadius) return;
    if((zIndex == maxZIndex && distance < minDistance) || (zIndex > maxZIndex)) {
        minDistance = distance;
        maxZIndex   = zIndex;
        position    = comparePosition;
    }
}

void ObjectPicker::DoQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                          int zIndex, int comparePosition) {
    Point2d corners[4] = {
        camera.ProjectPoint(a),
        camera.ProjectPoint(b),
        camera.ProjectPoint(c),
        camera.ProjectPoint(d)
    };
    double minNegative = VERY_NEGATIVE,
           maxPositive = VERY_POSITIVE;
    for(int i = 0; i < 4; i++) {
        Point2d ap = corners[i],
                bp = corners[(i + 1) % 4];
        double distance = point.DistanceToLineSigned(ap, bp.Minus(ap), /*asSegment=*/true);
        if(distance < 0) minNegative = std::max(minNegative, distance);
        if(distance > 0) maxPositive = std::min(maxPositive, distance);
    }

    bool insideQuad = (minNegative == VERY_NEGATIVE || maxPositive == VERY_POSITIVE);
    if(insideQuad) {
        DoCompare(0.0, zIndex, comparePosition);
    } else {
        double distance = std::min(fabs(minNegative), fabs(maxPositive));
        DoCompare(distance, zIndex, comparePosition);
    }
}

void ObjectPicker::DrawLine(const Vector &a, const Vector &b, hStroke hcs) {
    Stroke *stroke = strokes.FindById(hcs);
    Point2d ap = camera.ProjectPoint(a);
    Point2d bp = camera.ProjectPoint(b);
    double distance = point.DistanceToLine(ap, bp.Minus(ap), /*asSegment=*/true);
    DoCompare(distance - stroke->width / 2.0, stroke->zIndex);
}

void ObjectPicker::DrawEdges(const SEdgeList &el, hStroke hcs) {
    Stroke *stroke = strokes.FindById(hcs);
    for(const SEdge &e : el.l) {
        Point2d ap = camera.ProjectPoint(e.a);
        Point2d bp = camera.ProjectPoint(e.b);
        double distance = point.DistanceToLine(ap, bp.Minus(ap), /*asSegment=*/true);
        DoCompare(distance - stroke->width / 2.0, stroke->zIndex, e.auxB);
    }
}

void ObjectPicker::DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) {
    ssassert(false, "Not implemented");
}

void ObjectPicker::DrawVectorText(const std::string &text, double height,
                                  const Vector &o, const Vector &u, const Vector &v,
                                  hStroke hcs) {
    Stroke *stroke = strokes.FindById(hcs);
    double w = VectorFont::Builtin()-> GetWidth(height, text),
           h = VectorFont::Builtin()->GetHeight(height);
    DoQuad(o,
           o.Plus(v.ScaledBy(h)),
           o.Plus(u.ScaledBy(w)).Plus(v.ScaledBy(h)),
           o.Plus(u.ScaledBy(w)),
           stroke->zIndex);
}

void ObjectPicker::DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                             hFill hcf) {
    Fill *fill = fills.FindById(hcf);
    DoQuad(a, b, c, d, fill->zIndex);
}

void ObjectPicker::DrawPoint(const Vector &o, Canvas::hStroke hcs) {
    Stroke *stroke = strokes.FindById(hcs);
    double distance = point.DistanceTo(camera.ProjectPoint(o)) - stroke->width / 2;
    DoCompare(distance, stroke->zIndex);
}

void ObjectPicker::DrawPolygon(const SPolygon &p, hFill hcf) {
    ssassert(false, "Not implemented");
}

void ObjectPicker::DrawMesh(const SMesh &m, hFill hcfFront, hFill hcfBack) {
    ssassert(false, "Not implemented");
}

void ObjectPicker::DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) {
    ssassert(false, "Not implemented");
}

void ObjectPicker::DrawPixmap(std::shared_ptr<const Pixmap> pm,
                              const Vector &o, const Vector &u, const Vector &v,
                              const Point2d &ta, const Point2d &tb, Canvas::hFill hcf) {
    DrawQuad(o, o.Plus(u), o.Plus(u).Plus(v), o.Plus(v), hcf);
}

bool ObjectPicker::Pick(std::function<void()> drawFn) {
    minDistance = VERY_POSITIVE;
    maxZIndex = INT_MIN;

    drawFn();
    return minDistance < selRadius;
}

}
