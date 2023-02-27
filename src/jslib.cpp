#include "solvespace.h"
#include "slvs.h"
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(slvs) {
  emscripten::constant("C_POINTS_COINCIDENT",   100000);
  emscripten::constant("C_PT_PT_DISTANCE",      100001);
  emscripten::constant("C_PT_PLANE_DISTANCE",   100002);
  emscripten::constant("C_PT_LINE_DISTANCE",    100003);
  emscripten::constant("C_PT_FACE_DISTANCE",    100004);
  emscripten::constant("C_PT_IN_PLANE",         100005);
  emscripten::constant("C_PT_ON_LINE",          100006);
  emscripten::constant("C_PT_ON_FACE",          100007);
  emscripten::constant("C_EQUAL_LENGTH_LINES",  100008);
  emscripten::constant("C_LENGTH_RATIO",        100009);
  emscripten::constant("C_EQ_LEN_PT_LINE_D",    100010);
  emscripten::constant("C_EQ_PT_LN_DISTANCES",  100011);
  emscripten::constant("C_EQUAL_ANGLE",         100012);
  emscripten::constant("C_EQUAL_LINE_ARC_LEN",  100013);
  emscripten::constant("C_SYMMETRIC",           100014);
  emscripten::constant("C_SYMMETRIC_HORIZ",     100015);
  emscripten::constant("C_SYMMETRIC_VERT",      100016);
  emscripten::constant("C_SYMMETRIC_LINE",      100017);
  emscripten::constant("C_AT_MIDPOINT",         100018);
  emscripten::constant("C_HORIZONTAL",          100019);
  emscripten::constant("C_VERTICAL",            100020);
  emscripten::constant("C_DIAMETER",            100021);
  emscripten::constant("C_PT_ON_CIRCLE",        100022);
  emscripten::constant("C_SAME_ORIENTATION",    100023);
  emscripten::constant("C_ANGLE",               100024);
  emscripten::constant("C_PARALLEL",            100025);
  emscripten::constant("C_PERPENDICULAR",       100026);
  emscripten::constant("C_ARC_LINE_TANGENT",    100027);
  emscripten::constant("C_CUBIC_LINE_TANGENT",  100028);
  emscripten::constant("C_EQUAL_RADIUS",        100029);
  emscripten::constant("C_PROJ_PT_DISTANCE",    100030);
  emscripten::constant("C_WHERE_DRAGGED",       100031);
  emscripten::constant("C_CURVE_CURVE_TANGENT", 100032);
  emscripten::constant("C_LENGTH_DIFFERENCE",   100033);
  emscripten::constant("C_ARC_ARC_LEN_RATIO",   100034);
  emscripten::constant("C_ARC_LINE_LEN_RATIO",  100035);
  emscripten::constant("C_ARC_ARC_DIFFERENCE",  100036);
  emscripten::constant("C_ARC_LINE_DIFFERENCE", 100037);

  emscripten::constant("E_POINT_IN_3D",         50000);
  emscripten::constant("E_POINT_IN_2D",         50001);
  emscripten::constant("E_NORMAL_IN_3D",        60000);
  emscripten::constant("E_NORMAL_IN_2D",        60001);
  emscripten::constant("E_DISTANCE",            70000);
  emscripten::constant("E_WORKPLANE",           80000);
  emscripten::constant("E_LINE_SEGMENT",        80001);
  emscripten::constant("E_CUBIC",               80002);
  emscripten::constant("E_CIRCLE",              80003);
  emscripten::constant("E_ARC_OF_CIRCLE",       80004);

  emscripten::constant("E_NONE", SLVS_E_NONE);
  emscripten::constant("E_FREE_IN_3D", SLVS_E_FREE_IN_3D);

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

  emscripten::value_object<Slvs_SolveResult>("Slvs_SolveResult")
    .field("result", &Slvs_SolveResult::result)
    .field("dof", &Slvs_SolveResult::dof)
    .field("rank", &Slvs_SolveResult::rank)
    .field("bad", &Slvs_SolveResult::bad);

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
  emscripten::function("solveSketch", &Slvs_SolveSketch);
  emscripten::function("clearSketch", &Slvs_ClearSketch);
}