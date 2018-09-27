//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_CONSTRAINT_H
#define SOLVESPACE_CONSTRAINT_H

#include "handle.h"
#include "equation.h"
#include "render/canvas.h"

namespace SolveSpace {

// todo fix circular
enum class Command : uint32_t;

class ConstraintBase {
public:
    int         tag;
    hConstraint h;

    static const hConstraint NO_CONSTRAINT;

    enum class Type : uint32_t {
        POINTS_COINCIDENT      =  20,
        PT_PT_DISTANCE         =  30,
        PT_PLANE_DISTANCE      =  31,
        PT_LINE_DISTANCE       =  32,
        PT_FACE_DISTANCE       =  33,
        PROJ_PT_DISTANCE       =  34,
        PT_IN_PLANE            =  41,
        PT_ON_LINE             =  42,
        PT_ON_FACE             =  43,
        EQUAL_LENGTH_LINES     =  50,
        LENGTH_RATIO           =  51,
        EQ_LEN_PT_LINE_D       =  52,
        EQ_PT_LN_DISTANCES     =  53,
        EQUAL_ANGLE            =  54,
        EQUAL_LINE_ARC_LEN     =  55,
        LENGTH_DIFFERENCE      =  56,
        SYMMETRIC              =  60,
        SYMMETRIC_HORIZ        =  61,
        SYMMETRIC_VERT         =  62,
        SYMMETRIC_LINE         =  63,
        AT_MIDPOINT            =  70,
        HORIZONTAL             =  80,
        VERTICAL               =  81,
        DIAMETER               =  90,
        PT_ON_CIRCLE           = 100,
        SAME_ORIENTATION       = 110,
        ANGLE                  = 120,
        PARALLEL               = 121,
        PERPENDICULAR          = 122,
        ARC_LINE_TANGENT       = 123,
        CUBIC_LINE_TANGENT     = 124,
        CURVE_CURVE_TANGENT    = 125,
        EQUAL_RADIUS           = 130,
        WHERE_DRAGGED          = 200,

        COMMENT                = 1000
    };

    Type        type;

    hGroup      group;
    hEntity     workplane;

    // These are the parameters for the constraint.
    double      valA;
    hParam      valP;
    hEntity     ptA;
    hEntity     ptB;
    hEntity     entityA;
    hEntity     entityB;
    hEntity     entityC;
    hEntity     entityD;
    bool        other;
    bool        other2;

    bool        reference;  // a ref dimension, that generates no eqs
    std::string comment;    // since comments are represented as constraints

    bool HasLabel() const;

    void Generate(IdList<Param, hParam> *param);

    void GenerateEquations(IdList<Equation,hEquation> *entity,
                           bool forReference = false) const;
    // Some helpers when generating symbolic constraint equations
    void ModifyToSatisfy();
    void AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) const;
    void AddEq(IdList<Equation,hEquation> *l, const ExprVector &v, int baseIndex = 0) const;
    static Expr *DirectionCosine(hEntity wrkpl, ExprVector ae, ExprVector be);
    static Expr *Distance(hEntity workplane, hEntity pa, hEntity pb);
    static Expr *PointLineDistance(hEntity workplane, hEntity pt, hEntity ln);
    static Expr *PointPlaneDistance(ExprVector p, hEntity plane);
    static ExprVector VectorsParallel3d(ExprVector a, ExprVector b, hParam p);
    static ExprVector PointInThreeSpace(hEntity workplane, Expr *u, Expr *v);

    void Clear() {}
};

class Constraint : public ConstraintBase {
public:
    // See Entity::Entity().
    Constraint() : ConstraintBase({}), disp() {}

    // These define how the constraint is drawn on-screen.
    struct {
        Vector      offset;
        hStyle      style;
    } disp;

    bool IsVisible() const;
    bool IsStylable() const;
    hStyle GetStyle() const;
    bool HasLabel() const;
    std::string Label() const;

    enum class DrawAs { DEFAULT, HOVERED, SELECTED };
    void Draw(DrawAs how, Canvas *canvas);
    Vector GetLabelPos(const Camera &camera);
    void GetReferencePoints(const Camera &camera, std::vector<Vector> *refs);

    void DoLayout(DrawAs how, Canvas *canvas,
                  Vector *labelPos, std::vector<Vector> *refs);
    void DoLine(Canvas *canvas, Canvas::hStroke hcs, Vector a, Vector b);
    void DoStippledLine(Canvas *canvas, Canvas::hStroke hcs, Vector a, Vector b);
    bool DoLineExtend(Canvas *canvas, Canvas::hStroke hcs,
                      Vector p0, Vector p1, Vector pt, double salient);
    void DoArcForAngle(Canvas *canvas, Canvas::hStroke hcs,
                       Vector a0, Vector da, Vector b0, Vector db,
                       Vector offset, Vector *ref, bool trim);
    void DoArrow(Canvas *canvas, Canvas::hStroke hcs,
                 Vector p, Vector dir, Vector n, double width, double angle, double da);
    void DoLineWithArrows(Canvas *canvas, Canvas::hStroke hcs,
                          Vector ref, Vector a, Vector b, bool onlyOneExt);
    int  DoLineTrimmedAgainstBox(Canvas *canvas, Canvas::hStroke hcs,
                                 Vector ref, Vector a, Vector b, bool extend,
                                 Vector gr, Vector gu, double swidth, double sheight);
    int  DoLineTrimmedAgainstBox(Canvas *canvas, Canvas::hStroke hcs,
                                 Vector ref, Vector a, Vector b, bool extend = true);
    void DoLabel(Canvas *canvas, Canvas::hStroke hcs,
                 Vector ref, Vector *labelPos, Vector gr, Vector gu);
    void DoProjectedPoint(Canvas *canvas, Canvas::hStroke hcs, Vector *p);
    void DoProjectedPoint(Canvas *canvas, Canvas::hStroke hcs, Vector *p, Vector n, Vector o);

    void DoEqualLenTicks(Canvas *canvas, Canvas::hStroke hcs,
                         Vector a, Vector b, Vector gn, Vector *refp);
    void DoEqualRadiusTicks(Canvas *canvas, Canvas::hStroke hcs,
                            hEntity he, Vector *refp);

    std::string DescriptionString() const;

    static hConstraint AddConstraint(Constraint *c, bool rememberForUndo);
    static hConstraint AddConstraint(Constraint *c);
    static void MenuConstrain(Command id);
    static void DeleteAllConstraintsFor(Constraint::Type type, hEntity entityA, hEntity ptA);

    static hConstraint ConstrainCoincident(hEntity ptA, hEntity ptB);
    static hConstraint Constrain(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA);
    static hConstraint Constrain(Constraint::Type type, hEntity ptA, hEntity ptB,
                                 hEntity entityA, hEntity entityB,
                                 bool other, bool other2);
};

}

#endif //SOLVESPACE_CONSTRAINT_H
