#include "solvespace.h"
#include "slvs.h"
#include <emscripten/bind.h>

struct JsSolveResult {
  int             result;
  int             dof;
  size_t          nbad;
  emscripten::val bad;
};

static JsSolveResult solveSketch(Slvs_hGroup g, bool calculateFaileds) {
  JsSolveResult jsResult = {};
  Slvs_hConstraint *c = nullptr;
  Slvs_SolveResult ret = Slvs_SolveSketch(g, calculateFaileds ? &c : nullptr);
  if(c) {
    jsResult.bad = emscripten::val::global("Uint32Array").new_(ret.nbad);
    jsResult.bad.call<void>("set", emscripten::typed_memory_view(ret.nbad, c));
    free(c);
  }
  jsResult.result = ret.result;
  jsResult.dof = ret.dof;
  jsResult.nbad = ret.nbad;
  return jsResult;
}

EMSCRIPTEN_BINDINGS(slvs) {
  emscripten::constant("C_POINTS_COINCIDENT",   SLVS_C_POINTS_COINCIDENT);
  emscripten::constant("C_PT_PT_DISTANCE",      SLVS_C_PT_PT_DISTANCE);
  emscripten::constant("C_PT_PLANE_DISTANCE",   SLVS_C_PT_PLANE_DISTANCE);
  emscripten::constant("C_PT_LINE_DISTANCE",    SLVS_C_PT_LINE_DISTANCE);
  emscripten::constant("C_PT_FACE_DISTANCE",    SLVS_C_PT_FACE_DISTANCE);
  emscripten::constant("C_PT_IN_PLANE",         SLVS_C_PT_IN_PLANE);
  emscripten::constant("C_PT_ON_LINE",          SLVS_C_PT_ON_LINE);
  emscripten::constant("C_PT_ON_FACE",          SLVS_C_PT_ON_FACE);
  emscripten::constant("C_EQUAL_LENGTH_LINES",  SLVS_C_EQUAL_LENGTH_LINES);
  emscripten::constant("C_LENGTH_RATIO",        SLVS_C_LENGTH_RATIO);
  emscripten::constant("C_EQ_LEN_PT_LINE_D",    SLVS_C_EQ_LEN_PT_LINE_D);
  emscripten::constant("C_EQ_PT_LN_DISTANCES",  SLVS_C_EQ_PT_LN_DISTANCES);
  emscripten::constant("C_EQUAL_ANGLE",         SLVS_C_EQUAL_ANGLE);
  emscripten::constant("C_EQUAL_LINE_ARC_LEN",  SLVS_C_EQUAL_LINE_ARC_LEN);
  emscripten::constant("C_SYMMETRIC",           SLVS_C_SYMMETRIC);
  emscripten::constant("C_SYMMETRIC_HORIZ",     SLVS_C_SYMMETRIC_HORIZ);
  emscripten::constant("C_SYMMETRIC_VERT",      SLVS_C_SYMMETRIC_VERT);
  emscripten::constant("C_SYMMETRIC_LINE",      SLVS_C_SYMMETRIC_LINE);
  emscripten::constant("C_AT_MIDPOINT",         SLVS_C_AT_MIDPOINT);
  emscripten::constant("C_HORIZONTAL",          SLVS_C_HORIZONTAL);
  emscripten::constant("C_VERTICAL",            SLVS_C_VERTICAL);
  emscripten::constant("C_DIAMETER",            SLVS_C_DIAMETER);
  emscripten::constant("C_PT_ON_CIRCLE",        SLVS_C_PT_ON_CIRCLE);
  emscripten::constant("C_SAME_ORIENTATION",    SLVS_C_SAME_ORIENTATION);
  emscripten::constant("C_ANGLE",               SLVS_C_ANGLE);
  emscripten::constant("C_PARALLEL",            SLVS_C_PARALLEL);
  emscripten::constant("C_PERPENDICULAR",       SLVS_C_PERPENDICULAR);
  emscripten::constant("C_ARC_LINE_TANGENT",    SLVS_C_ARC_LINE_TANGENT);
  emscripten::constant("C_CUBIC_LINE_TANGENT",  SLVS_C_CUBIC_LINE_TANGENT);
  emscripten::constant("C_EQUAL_RADIUS",        SLVS_C_EQUAL_RADIUS);
  emscripten::constant("C_PROJ_PT_DISTANCE",    SLVS_C_PROJ_PT_DISTANCE);
  emscripten::constant("C_WHERE_DRAGGED",       SLVS_C_WHERE_DRAGGED);
  emscripten::constant("C_CURVE_CURVE_TANGENT", SLVS_C_CURVE_CURVE_TANGENT);
  emscripten::constant("C_LENGTH_DIFFERENCE",   SLVS_C_LENGTH_DIFFERENCE);
  emscripten::constant("C_ARC_ARC_LEN_RATIO",   SLVS_C_ARC_ARC_LEN_RATIO);
  emscripten::constant("C_ARC_LINE_LEN_RATIO",  SLVS_C_ARC_LINE_LEN_RATIO);
  emscripten::constant("C_ARC_ARC_DIFFERENCE",  SLVS_C_ARC_ARC_DIFFERENCE);
  emscripten::constant("C_ARC_LINE_DIFFERENCE", SLVS_C_ARC_LINE_DIFFERENCE);

  emscripten::constant("E_POINT_IN_3D",         SLVS_E_POINT_IN_3D);
  emscripten::constant("E_POINT_IN_2D",         SLVS_E_POINT_IN_2D);
  emscripten::constant("E_NORMAL_IN_3D",        SLVS_E_NORMAL_IN_3D);
  emscripten::constant("E_NORMAL_IN_2D",        SLVS_E_NORMAL_IN_2D);
  emscripten::constant("E_DISTANCE",            SLVS_E_DISTANCE);
  emscripten::constant("E_WORKPLANE",           SLVS_E_WORKPLANE);
  emscripten::constant("E_LINE_SEGMENT",        SLVS_E_LINE_SEGMENT);
  emscripten::constant("E_CUBIC",               SLVS_E_CUBIC);
  emscripten::constant("E_CIRCLE",              SLVS_E_CIRCLE);
  emscripten::constant("E_ARC_OF_CIRCLE",       SLVS_E_ARC_OF_CIRCLE);

  emscripten::constant("E_NONE", SLVS_E_NONE);
  emscripten::constant("E_FREE_IN_3D", SLVS_E_FREE_IN_3D);

  emscripten::constant("RESULT_OKAY", SLVS_RESULT_OKAY);
  emscripten::constant("RESULT_INCONSISTENT", SLVS_RESULT_INCONSISTENT);
  emscripten::constant("RESULT_DIDNT_CONVERGE", SLVS_RESULT_DIDNT_CONVERGE);
  emscripten::constant("RESULT_TOO_MANY_UNKNOWNS", SLVS_RESULT_TOO_MANY_UNKNOWNS);
  emscripten::constant("RESULT_REDUNDANT_OKAY", SLVS_RESULT_REDUNDANT_OKAY);

  emscripten::value_array<std::array<uint32_t, 4>>("array_uint32_4")
    .element(emscripten::index<0>())
    .element(emscripten::index<1>())
    .element(emscripten::index<3>())
    .element(emscripten::index<4>());

  emscripten::value_object<Slvs_Entity>("Slvs_Entity")
    .field("h", &Slvs_Entity::h)
    .field("group", &Slvs_Entity::group)
    .field("type", &Slvs_Entity::type)
    .field("wrkpl", &Slvs_Entity::wrkpl)
    .field("point", &Slvs_Entity::point)
    .field("normal", &Slvs_Entity::normal)
    .field("distance", &Slvs_Entity::distance)
    .field("param", &Slvs_Entity::param);

  emscripten::value_object<Slvs_Constraint>("Slvs_Constraint")
    .field("h", &Slvs_Constraint::h)
    .field("group", &Slvs_Constraint::group)
    .field("type", &Slvs_Constraint::type)
    .field("wrkpl", &Slvs_Constraint::wrkpl)
    .field("valA", &Slvs_Constraint::valA)
    .field("ptA", &Slvs_Constraint::ptA)
    .field("ptB", &Slvs_Constraint::ptB)
    .field("entityA", &Slvs_Constraint::entityA)
    .field("entityB", &Slvs_Constraint::entityB)
    .field("entityC", &Slvs_Constraint::entityC)
    .field("entityD", &Slvs_Constraint::entityD)
    .field("other", &Slvs_Constraint::other)
    .field("other2", &Slvs_Constraint::other2);

  emscripten::value_object<JsSolveResult>("Slvs_SolveResult")
    .field("result", &JsSolveResult::result)
    .field("dof", &JsSolveResult::dof)
    .field("nbad", &JsSolveResult::nbad)
    .field("bad", &JsSolveResult::bad);

  emscripten::class_<Quaternion>("Quaternion")
    .constructor<>()
    .function("plus", &Quaternion::Plus)
    .function("minus", &Quaternion::Minus)
    .function("scaledBy", &Quaternion::ScaledBy)
    .function("magnitude", &Quaternion::Magnitude)
    .function("withMagnitude", &Quaternion::WithMagnitude)
    .function("toThe", &Quaternion::ToThe)
    .function("inverse", &Quaternion::Inverse)
    .function("times", &Quaternion::Times)
    .function("mirror", &Quaternion::Mirror)
    .function("rotationU", &Quaternion::RotationU)
    .function("rotationV", &Quaternion::RotationV)
    .function("rotationN", &Quaternion::RotationN)
    .property("w", &Quaternion::w)
    .property("vx", &Quaternion::vx)
    .property("vy", &Quaternion::vy)
    .property("vz", &Quaternion::vz)
    .class_function("from", emscripten::select_overload<Quaternion(double, double, double, double)>(&Quaternion::From));

  emscripten::class_<Vector>("Vector")
    .constructor<>()
    .property("x", &Vector::x)
    .property("y", &Vector::y)
    .property("z", &Vector::z);

  emscripten::function("isFreeIn3D", &Slvs_IsFreeIn3D);
  emscripten::function("is3D", &Slvs_Is3D);
  emscripten::function("isNone", &Slvs_IsNone);
  emscripten::function("isPoint2D", &Slvs_IsPoint2D);
  emscripten::function("isPoint3D", &Slvs_IsPoint3D);
  emscripten::function("isNormal2D", &Slvs_IsNormal2D);
  emscripten::function("isNormal3D", &Slvs_IsNormal3D);
  emscripten::function("isLine", &Slvs_IsLine);
  emscripten::function("isLine2D", &Slvs_IsLine2D);
  emscripten::function("isLine3D", &Slvs_IsLine3D);
  emscripten::function("isCubic", &Slvs_IsCubic);
  emscripten::function("isArc", &Slvs_IsArc);
  emscripten::function("isWorkplane", &Slvs_IsWorkplane);
  emscripten::function("isDistance", &Slvs_IsDistance);
  emscripten::function("isPoint", &Slvs_IsPoint);

  emscripten::function("addPoint2D", &Slvs_AddPoint2D);
  emscripten::function("addPoint3D", &Slvs_AddPoint3D);
  emscripten::function("addNormal2D", &Slvs_AddNormal2D);
  emscripten::function("addNormal3D", &Slvs_AddNormal3D);
  emscripten::function("addDistance", &Slvs_AddDistance);
  emscripten::function("addLine2D", &Slvs_AddLine2D);
  emscripten::function("addLine3D", &Slvs_AddLine3D);
  emscripten::function("addCubic", &Slvs_AddCubic);
  emscripten::function("addArc", &Slvs_AddArc);
  emscripten::function("addCircle", &Slvs_AddCircle);
  emscripten::function("addWorkplane", &Slvs_AddWorkplane);
  emscripten::function("addBase2D", &Slvs_AddBase2D);

  emscripten::function("addConstraint", &Slvs_AddConstraint);
  emscripten::function("coincident", &Slvs_Coincident);
  emscripten::function("distance", &Slvs_Distance);
  emscripten::function("equal", &Slvs_Equal);
  emscripten::function("equalAngle", &Slvs_EqualAngle);
  emscripten::function("equalPointToLine", &Slvs_EqualPointToLine);
  emscripten::function("ratio", &Slvs_Ratio);
  emscripten::function("symmetric", &Slvs_Symmetric);
  emscripten::function("symmetricH", &Slvs_SymmetricH);
  emscripten::function("symmetricV", &Slvs_SymmetricV);
  emscripten::function("midpoint", &Slvs_Midpoint);
  emscripten::function("horizontal", &Slvs_Horizontal);
  emscripten::function("vertical", &Slvs_Vertical);
  emscripten::function("diameter", &Slvs_Diameter);
  emscripten::function("sameOrientation", &Slvs_SameOrientation);
  emscripten::function("angle", &Slvs_Angle);
  emscripten::function("perpendicular", &Slvs_Perpendicular);
  emscripten::function("parallel", &Slvs_Parallel);
  emscripten::function("tangent", &Slvs_Tangent);
  emscripten::function("distanceProj", &Slvs_DistanceProj);
  emscripten::function("lengthDiff", &Slvs_LengthDiff);
  emscripten::function("dragged", &Slvs_Dragged);

  emscripten::function("getParamValue", &Slvs_GetParamValue);
  emscripten::function("setParamValue", &Slvs_SetParamValue);
  emscripten::function("markDragged", &Slvs_MarkDragged);
  emscripten::function("solveSketch", &solveSketch);
  emscripten::function("clearSketch", &Slvs_ClearSketch);
}
