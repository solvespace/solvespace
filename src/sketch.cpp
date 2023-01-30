#include "solvespace.h"

void Sketch::Clear() {
    group.Clear();
    groupOrder.Clear();
    constraint.Clear();
#ifndef LIBRARY
    request.Clear();
    style.Clear();
#endif
    entity.Clear();
    param.Clear();
}

#ifndef LIBRARY
BBox Sketch::CalculateEntityBBox(bool includingInvisible) {
    BBox box = {};
    bool first = true;

    auto includePoint = [&](const Vector &point) {
        if(first) {
            box.minp = point;
            box.maxp = point;
            first = false;
        } else {
            box.Include(point);
        }
    };

    for(const Entity &e : entity) {
        if(e.construction) continue;
        if(!(includingInvisible || e.IsVisible())) continue;

        // arc center point shouldn't be included in bounding box calculation
        if(e.IsPoint() && e.h.isFromRequest()) {
            Request *r = SK.GetRequest(e.h.request());
            if(r->type == Request::Type::ARC_OF_CIRCLE && e.h == r->h.entity(1)) {
                continue;
            }
        }

        if(e.IsPoint()) {
            includePoint(e.PointGetNum());
            continue;
        }

        switch(e.type) {
            // Circles and arcs are special cases. We calculate their bounds
            // based on Bezier curve bounds. This is not exact for arcs,
            // but the implementation is rather simple.
            case Entity::Type::CIRCLE:
            case Entity::Type::ARC_OF_CIRCLE: {
                SBezierList sbl = {};
                e.GenerateBezierCurves(&sbl);

                for(const SBezier &sb : sbl.l) {
                    for(int j = 0; j <= sb.deg; j++) {
                        includePoint(sb.ctrl[j]);
                    }
                }
                sbl.Clear();
                continue;
            }

            default:
                continue;
        }
    }

    return box;
}

Group *Sketch::GetRunningMeshGroupFor(hGroup h) {
    Group *g = GetGroup(h);
    while(g != NULL) {
        if(g->IsMeshGroup()) {
            return g;
        }
        g = g->PreviousGroup();
    }
    return NULL;
}
#endif
