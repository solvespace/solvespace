//-----------------------------------------------------------------------------
// A rendering backend that draws on a Cairo surface.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include <cairo.h>
#include "solvespace.h"

namespace SolveSpace {

void CairoRenderer::OutputStart() {
    cairo_save(context);

    RgbaColor bgColor = lighting.backgroundColor;
    cairo_rectangle(context, 0.0, 0.0, (double)camera.width, (double)camera.height);
    cairo_set_source_rgba(context, bgColor.redF(), bgColor.greenF(), bgColor.blueF(),
                          bgColor.alphaF());
    cairo_fill(context);

    cairo_translate(context, camera.width / 2.0, camera.height / 2.0);

    // Avoid pixel boundaries; when not using antialiasing, we would otherwise
    // get numerically unstable output.
    cairo_translate(context, 0.1, 0.1);

    cairo_set_line_join(context, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(context, CAIRO_LINE_CAP_ROUND);
}

void CairoRenderer::OutputEnd() {
    FinishPath();

    cairo_restore(context);
    cairo_surface_flush(cairo_get_target(context));
}

void CairoRenderer::SelectStroke(hStroke hcs) {
    if(current.hcs.v == hcs.v) return;
    FinishPath();

    Stroke *stroke = strokes.FindById(hcs);
    current.hcs = hcs;

    RgbaColor color = stroke->color;
    std::vector<double> dashes = StipplePatternDashes(stroke->stipplePattern);
    for(double &dash : dashes) {
        dash *= stroke->StippleScalePx(camera);
    }
    cairo_set_line_width(context, stroke->WidthPx(camera));
    cairo_set_dash(context, dashes.data(), (int)dashes.size(), 0);
    cairo_set_source_rgba(context, color.redF(), color.greenF(), color.blueF(),
                          color.alphaF());
    if(antialias) {
        cairo_set_antialias(context, CAIRO_ANTIALIAS_GRAY);
    } else {
        cairo_set_antialias(context, CAIRO_ANTIALIAS_NONE);
    }
}

void CairoRenderer::MoveTo(Vector p) {
    Point2d pos;
    cairo_get_current_point(context, &pos.x, &pos.y);
    if(cairo_has_current_point(context) && pos.Equals(p.ProjectXy())) return;
    FinishPath();

    cairo_move_to(context, p.x, p.y);
}

void CairoRenderer::FinishPath() {
    if(!cairo_has_current_point(context)) return;

    cairo_stroke(context);
}

void CairoRenderer::OutputBezier(const SBezier &b, hStroke hcs) {
    SelectStroke(hcs);

    Vector c, n = Vector::From(0, 0, 1);
    double r;
    if(b.deg == 1) {
        MoveTo(b.ctrl[0]);
        cairo_line_to(context,
            b.ctrl[1].x, b.ctrl[1].y);
    } else if(b.IsCircle(n, &c, &r)) {
        MoveTo(b.ctrl[0]);
        double theta0 = atan2(b.ctrl[0].y - c.y, b.ctrl[0].x - c.x),
               theta1 = atan2(b.ctrl[2].y - c.y, b.ctrl[2].x - c.x),
               dtheta = WRAP_SYMMETRIC(theta1 - theta0, 2*PI);
        if(dtheta > 0) {
            cairo_arc(context,
                c.x, c.y, r, theta0, theta1);
        } else {
            cairo_arc_negative(context,
                c.x, c.y, r, theta0, theta1);
        }
    } else if(b.deg == 3 && !b.IsRational()) {
        MoveTo(b.ctrl[0]);
        cairo_curve_to(context,
            b.ctrl[1].x, b.ctrl[1].y,
            b.ctrl[2].x, b.ctrl[2].y,
            b.ctrl[3].x, b.ctrl[3].y);
    } else {
        OutputBezierAsNonrationalCubic(b, hcs);
    }
}

void CairoRenderer::OutputTriangle(const STriangle &tr) {
    FinishPath();
    current.hcs = {};

    RgbaColor color = tr.meta.color;
    cairo_set_source_rgba(context, color.redF(), color.greenF(), color.blueF(),
                          color.alphaF());
    cairo_set_antialias(context, CAIRO_ANTIALIAS_NONE);
    cairo_move_to(context, tr.a.x, tr.a.y);
    cairo_line_to(context, tr.b.x, tr.b.y);
    cairo_line_to(context, tr.c.x, tr.c.y);
    cairo_fill(context);
}

}
