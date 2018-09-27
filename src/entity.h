//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_ENTITY_H
#define SOLVESPACE_ENTITY_H

#include "handle.h"
#include "list.h"

namespace SolveSpace {

class Entity : public EntityBase {
public:
    // Necessary for Entity e = {} to zero-initialize, since
    // classes with base classes are not aggregates and
    // the default constructor does not initialize members.
    //
    // Note EntityBase({}); without explicitly value-initializing
    // the base class, MSVC2013 will default-initialize it, leaving
    // POD members with indeterminate value.
    Entity() : EntityBase({}), forceHidden(), actPoint(), actNormal(),
               actDistance(), actVisible(), style(), construction(),
               beziers(), edges(), edgesChordTol(), screenBBox(), screenBBoxValid() {};

    // A linked entity that was hidden in the source file ends up hidden
    // here too.
    bool        forceHidden;

    // All points/normals/distances have their numerical value; this is
    // a convenience, to simplify the link/assembly code, so that the
    // part is entirely described by the entities.
    Vector      actPoint;
    Quaternion  actNormal;
    double      actDistance;
    // and the shown state also gets saved here, for later import
    bool        actVisible;

    hStyle      style;
    bool        construction;

    SBezierList beziers;
    SEdgeList   edges;
    double      edgesChordTol;
    BBox        screenBBox;
    bool        screenBBoxValid;

    bool IsStylable() const;
    bool IsVisible() const;

    enum class DrawAs { DEFAULT, OVERLAY, HIDDEN, HOVERED, SELECTED };
    void Draw(DrawAs how, Canvas *canvas);
    void GetReferencePoints(std::vector<Vector> *refs);
    int GetPositionOfPoint(const Camera &camera, Point2d p);

    void ComputeInterpolatingSpline(SBezierList *sbl, bool periodic) const;
    void GenerateBezierCurves(SBezierList *sbl) const;
    void GenerateEdges(SEdgeList *el);

    SBezierList *GetOrGenerateBezierCurves();
    SEdgeList *GetOrGenerateEdges();
    BBox GetOrGenerateScreenBBox(bool *hasBBox);

    void CalculateNumerical(bool forExport);

    std::string DescriptionString() const;

    void Clear() {
        beziers.l.Clear();
        edges.l.Clear();
    }
};

typedef IdList<Entity,hEntity> EntityList;
}

#endif //SOLVESPACE_ENTITY_H
