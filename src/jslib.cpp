#include <slvs.h>
#include <emscripten/bind.h>

EMSCRIPTEN_BINDINGS(slvs) {
  emscripten::value_object<Slvs_Entity>("Slvs_Entity")
    .field("h", &Slvs_Entity::h)
    .field("group", &Slvs_Entity::group)
    .field("type", &Slvs_Entity::type)
    .field("wrkpl", &Slvs_Entity::wrkpl)
    .field("normal", &Slvs_Entity::normal)
    .field("distance", &Slvs_Entity::distance);
    // .field("param", &Slvs_Entity::param);

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

  emscripten::function("quaternionU", &Slvs_QuaternionU, emscripten::allow_raw_pointers());
  emscripten::function("quaternionV", &Slvs_QuaternionV, emscripten::allow_raw_pointers());
  emscripten::function("quaternionN", &Slvs_QuaternionN, emscripten::allow_raw_pointers());
  emscripten::function("makeQuaternion", &Slvs_MakeQuaternion, emscripten::allow_raw_pointers());

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
  emscripten::function("add2DBase", &Slvs_Add2DBase);

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
  emscripten::function("solve", &Slvs_Solve, emscripten::allow_raw_pointers());
  emscripten::function("solveSketch", &Slvs_SolveSketch);
  emscripten::function("clearSketch", &Slvs_ClearSketch);
}