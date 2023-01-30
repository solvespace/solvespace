#include "solvespace.h"
#include "solversystem.h"
#include <string>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

Sketch SolveSpace::SK = {};

void SolveSpace::Platform::FatalError(const std::string &message) {
    fprintf(stderr, "%s", message.c_str());
    abort();
}

namespace py = pybind11;

PYBIND11_MODULE(slvs, m) {
    py::class_<hGroup> hg(m, "hGroup");
    py::class_<hEntity> he(m, "hEntity");
    py::class_<hConstraint> hc(m, "hConstraint");
    py::class_<hEquation> heq(m, "hEquation");
    py::class_<hParam> hp(m, "hParam");

    hg
        .def_readwrite("v", &hGroup::v)
        .def("entity", &hGroup::entity)
        .def("param", &hGroup::param)
        .def("equation", &hGroup::equation);

    he
        .def_readwrite("v", &hEntity::v)
        .def("group", &hEntity::group)
        .def("equation", &hEntity::equation);

    hc
        .def_readwrite("v", &hConstraint::v)
        .def("equation", &hConstraint::equation)
        .def("param", &hConstraint::param);

    heq
        .def_readwrite("v", &hEquation::v)
        .def("is_from_constraint", &hEquation::isFromConstraint)
        .def("constraint", &hEquation::constraint);

    hp
        .def_readwrite("v", &hParam::v)
        .def("__repr__", [](const hParam &ph) {
            return "<solvespace.hParam v=" + std::to_string(ph.v) + ">";
        });

    py::class_<Equation>(m, "Equation")
        .def_readwrite("tag", &Equation::tag)
        .def_readwrite("h", &Equation::h)
        // .def_readwrite("e", &Equation::e)
        .def("clear", &Equation::Clear);

    py::class_<Param>(m, "Param")
        .def_readwrite("tag", &Param::tag)
        .def_readwrite("h", &Param::h)
        .def_readwrite("val", &Param::val)
        .def_readwrite("known", &Param::known)
        .def_readwrite("free", &Param::free)
        // .def_readwrite("substd", &Param::substd)
        // .def_readonly_static("NO_PARAM", &Param::NO_PARAM);
        .def("__repr__", [](const Param &p) {
            return "<solvespace.Param h.v=" + std::to_string(p.h.v) + " known=" + (p.known
                       ? "1"
                       : "0") + " val=" + std::to_string(p.val) + ">";
        });

    py::class_<GroupBase>(m, "Group")
        .def(py::init<>())
        .def_readwrite("all_dims_referenc", &GroupBase::allDimsReference)
        .def_readwrite("relax_constraints", &GroupBase::relaxConstraints)
        // .def_readwrite("solved", &GroupBase::solved)
        .def_readwrite("suppress_dof_calculation", &GroupBase::suppressDofCalculation)
        .def_readwrite("allow_redundant", &GroupBase::allowRedundant)
        .def_readwrite("h", &GroupBase::h)
        .def("__repr__", [](const GroupBase &g) {
            return "<solvespace.Group id=" + std::to_string(g.h.v) + ">";
        });

    // py::class_<Expr> expr(m, "Expr");
    // expr.def(py::init<>()).def(py::init<double>());
    // py::enum_<Expr::Op>(expr, "Op")
    //     .value("PARAM", Expr::Op::PARAM)
    //     .value("PARAM_PTR", Expr::Op::PARAM_PTR)
    //     .value("CONSTANT", Expr::Op::CONSTANT)
    //     .value("VARIABLE", Expr::Op::VARIABLE)
    //     .value("PLUS", Expr::Op::PLUS)
    //     .value("MINUS", Expr::Op::MINUS)
    //     .value("TIMES", Expr::Op::TIMES)
    //     .value("DIV", Expr::Op::DIV)
    //     .value("NEGATE", Expr::Op::NEGATE)
    //     .value("SQRT", Expr::Op::SQRT)
    //     .value("SQUARE", Expr::Op::SQUARE)
    //     .value("SIN", Expr::Op::SIN)
    //     .value("COS", Expr::Op::COS)
    //     .value("ASIN", Expr::Op::ASIN)
    //     .value("ACOS", Expr::Op::ACOS);

    py::class_<Vector>(m, "Vector")
        .def_readwrite("x", &Vector::x)
        .def_readwrite("y", &Vector::y)
        .def_readwrite("z", &Vector::z)
        .def_static("make", py::overload_cast<double, double, double>(&Vector::From))
        .def_static("at_intersection_of_planes", py::overload_cast<Vector, double, Vector, double>(
                                                     &Vector::AtIntersectionOfPlanes))
        .def_static("at_intersection_of_planes",
                    py::overload_cast<Vector, double, Vector, double, Vector, double, bool *>(
                        &Vector::AtIntersectionOfPlanes))
        .def_static("at_intersection_of_lines",
                    py::overload_cast<Vector, Vector, Vector, Vector, bool *, double *, double *>(
                        &Vector::AtIntersectionOfLines))
        .def_static("at_intersection_of_plane_and_line", &Vector::AtIntersectionOfPlaneAndLine)
        .def_static("closest_point_between_lines",
                    py::overload_cast<Vector, Vector, Vector, Vector, double *, double *>(
                        &Vector::ClosestPointBetweenLines))
        .def("element", &Vector::Element)
        .def("equals", &Vector::Equals)
        .def("equals_exactly", &Vector::EqualsExactly)
        .def("plus", &Vector::Plus)
        .def("minus", &Vector::Minus)
        .def("cross", &Vector::Cross)
        .def("direction_cosine_with", &Vector::DirectionCosineWith)
        .def("dot", &Vector::Dot)
        .def("normal", &Vector::Normal)
        // .def("rotated_about", py::overload_cast<Vector, Vector, double>(&Vector::RotatedAbout))
        // .def("rotated_about", py::overload_cast<Vector, double>(&Vector::RotatedAbout))
        .def("dot_in_to_csys", &Vector::DotInToCsys)
        .def("scale_out_of_csys", &Vector::ScaleOutOfCsys)
        .def("distance_to_line", &Vector::DistanceToLine)
        .def("distance_to_plane", &Vector::DistanceToPlane)
        .def("on_line_segment", &Vector::OnLineSegment)
        .def("closest_point_on_line", &Vector::ClosestPointOnLine)
        .def("magnitude", &Vector::Magnitude)
        .def("mag_squared", &Vector::MagSquared)
        .def("with_magnitude", &Vector::WithMagnitude)
        .def("scaled_by", &Vector::ScaledBy)
        .def("div_projected", &Vector::DivProjected)
        .def("closest_ortho", &Vector::ClosestOrtho)
        .def("make_max_min", &Vector::MakeMaxMin)
        .def("clamp_within", &Vector::ClampWithin)
        .def_static("bounding_boxes_disjoint", &Vector::BoundingBoxesDisjoint)
        .def_static("bounding_box_intersects_line", &Vector::BoundingBoxIntersectsLine)
        .def("outside_and_not_on", &Vector::OutsideAndNotOn)
        .def("in_perspective", &Vector::InPerspective);
        // .def("project_2d", &Vector::Project2d)
        // .def("project_xy", &Vector::ProjectXy);

    py::class_<Quaternion>(m, "Quaternion")
        .def_readwrite("w", &Quaternion::w)
        .def_readwrite("vx", &Quaternion::vx)
        .def_readwrite("vy", &Quaternion::vy)
        .def_readwrite("vz", &Quaternion::vz)
        .def_static("make", py::overload_cast<double, double, double, double>(&Quaternion::From))
        .def_static("make", py::overload_cast<Vector, Vector>(&Quaternion::From))
        .def_static("make", py::overload_cast<Vector, double>(&Quaternion::From))
        .def("plus", &Quaternion::Plus)
        .def("minus", &Quaternion::Minus)
        .def("scaled_by", &Quaternion::ScaledBy)
        .def("magnitude", &Quaternion::Magnitude)
        .def("with_magnitude", &Quaternion::WithMagnitude)
        .def("rotation_u", &Quaternion::RotationU)
        .def("rotation_v", &Quaternion::RotationV)
        .def("rotation_n", &Quaternion::RotationN)
        .def("rotate", &Quaternion::Rotate)
        .def("to_the", &Quaternion::ToThe)
        .def("inverse", &Quaternion::Inverse)
        .def("times", &Quaternion::Times)
        .def("mirror", &Quaternion::Mirror);

    py::class_<EntityBase> entity(m, "Entity");
    entity.def(py::init<>())
        .def_readonly_static("NO_ENTITY", &EntityBase::_NO_ENTITY)
        .def_readonly_static("FREE_IN_3D", &EntityBase::_FREE_IN_3D)
        .def_readwrite("tag", &EntityBase::tag)
        .def_readwrite("h", &EntityBase::h)
        .def_readwrite("type", &EntityBase::type)
        .def_readwrite("group", &EntityBase::group)
        .def_readwrite("workplane", &EntityBase::workplane)
        .def_readwrite("point", &EntityBase::point)
        .def_readwrite("param", &EntityBase::param)
        .def_readwrite("num_point", &EntityBase::numPoint)
        .def_readwrite("num_normal", &EntityBase::numNormal)
        .def_readwrite("num_distance", &EntityBase::numDistance)
        .def_readwrite("str", &EntityBase::str)
        // .def_readwrite("font", &EntityBase::font)
        .def_readwrite("aspect_ratio", &EntityBase::aspectRatio)
        .def_readwrite("times_applied", &EntityBase::timesApplied)
        // .def("quaternion_from_params", &EntityBase::QuaternionFromParams)
        .def("get_axis_angle_quaternion", &EntityBase::GetAxisAngleQuaternion)
        .def("is_circle", &EntityBase::IsCircle)
        // .def("circle_get_radius_expr", &EntityBase::CircleGetRadiusExpr)
        .def("circle_get_radius_num", &EntityBase::CircleGetRadiusNum)
        .def("arc_get_angles", &EntityBase::ArcGetAngles)
        .def("has_vector", &EntityBase::HasVector)
        // .def("vector_get_exprs", &EntityBase::VectorGetExprs)
        // .def("vector_get_exprs_in_workplane", &EntityBase::VectorGetExprsInWorkplane)
        .def("vector_get_num", &EntityBase::VectorGetNum)
        .def("vector_get_ref_point", &EntityBase::VectorGetRefPoint)
        .def("vector_get_start_point", &EntityBase::VectorGetStartPoint)
        .def("is_distance", &EntityBase::IsDistance)
        .def("distance_get_num", &EntityBase::DistanceGetNum)
        // .def("distance_get_expr", &EntityBase::DistanceGetExpr)
        .def("distance_force_to", &EntityBase::DistanceForceTo)
        .def("is_workplane", &EntityBase::IsWorkplane)
        // .def("workplane_get_plane_exprs", py::overload_cast<ExprVector*,
        // Expr*>(&EntityBase::WorkplaneGetPlaneExprs))
        // .def("workplane_get_offset_exprs", &EntityBase::WorkplaneGetOffsetExprs)
        .def("workplane_get_offset", &EntityBase::WorkplaneGetOffset)
        .def("normal", &EntityBase::Normal)
        .def("is_face", &EntityBase::IsFace)
        // .def("face_get_normal_exprs", &EntityBase::FaceGetNormalExprs)
        .def("face_get_normal_num", &EntityBase::FaceGetNormalNum)
        // .def("face_get_point_exprs", &EntityBase::FaceGetPointExprs)
        .def("face_get_point_num", &EntityBase::FaceGetPointNum)
        .def("is_point", &EntityBase::IsPoint)
        .def("point_get_num", &EntityBase::PointGetNum)
        // .def("point_get_exprs", &EntityBase::PointGetExprs)
        // .def("point_get_exprs_in_workplane", py::overload_cast<hEntity*, Expr**,
        // Expr**>(&EntityBase::PointGetExprsInWorkplane)) .def("point_get_exprs_in_workplane",
        // py::overload_cast<hEntity>(&EntityBase::PointGetExprsInWorkplane))
        .def("point_force_to", &EntityBase::PointForceTo)
        .def("point_force_param_to", &EntityBase::PointForceParamTo)
        .def("point_get_quaternion", &EntityBase::PointGetQuaternion)
        .def("point_force_quaternion_to", &EntityBase::PointForceQuaternionTo)
        .def("is_normal", &EntityBase::IsNormal)
        .def("normal_get_num", &EntityBase::NormalGetNum)
        // .def("normal_get_exprs", &EntityBase::NormalGetExprs)
        .def("normal_force_to", &EntityBase::NormalForceTo)
        .def("normal_u", &EntityBase::NormalU)
        .def("normal_v", &EntityBase::NormalV)
        .def("normal_n", &EntityBase::NormalN)
        // .def("normal_exprs_u", &EntityBase::NormalExprsU)
        // .def("normal_exprs_v", &EntityBase::NormalExprsV)
        // .def("normal_exprs_n", &EntityBase::NormalExprsN)
        .def("cubic_get_start_num", &EntityBase::CubicGetStartNum)
        .def("cubic_get_finish_num", &EntityBase::CubicGetFinishNum)
        // .def("cubic_get_start_tangent_exprs", &EntityBase::CubicGetStartTangentExprs)
        // .def("cubic_get_finish_tangent_exprs", &EntityBase::CubicGetFinishTangentExprs)
        .def("cubic_get_start_tangent_num", &EntityBase::CubicGetStartTangentNum)
        .def("cubic_get_finish_tangent_num", &EntityBase::CubicGetFinishTangentNum)
        .def("has_endpoints", &EntityBase::HasEndpoints)
        .def("endpoint_start", &EntityBase::EndpointStart)
        .def("endpoint_finish", &EntityBase::EndpointFinish)
        .def("is_in_plane", &EntityBase::IsInPlane)
        // .def("rect_get_points_exprs", &EntityBase::RectGetPointsExprs)
        // .def("add_eq", &EntityBase::AddEq)
        // .def("generate_equations", &EntityBase::GenerateEquations)
        .def("clear", &EntityBase::Clear)

        .def("params", &EntityBase::GetParams)
        .def("is_3d", &EntityBase::Is3D)
        .def("is_free_in_3d", &EntityBase::IsFreeIn3D)
        .def("is_none", &EntityBase::IsNone)
        .def("is_point_2d", &EntityBase::IsPoint2D)
        .def("is_point_3d", &EntityBase::IsPoint3D)
        .def("is_normal_2d", &EntityBase::IsNormal2D)
        .def("is_normal_3d", &EntityBase::IsNormal3D)
        .def("is_line", &EntityBase::IsLine)
        .def("is_line_2d", &EntityBase::IsLine2D)
        .def("is_line_3d", &EntityBase::IsLine3D)
        .def("is_cubic", &EntityBase::IsCubic)
        .def("is_arc", &EntityBase::IsArc)
        .def("__repr__", [](const EntityBase &e) {
            return "<solvespace.Entity:" + e.ToString() + " id=" + std::to_string(e.h.v) + ">";
        });

    py::enum_<EntityBase::Type>(entity, "Type")
        .value("POINT_IN_3D", EntityBase::Type::POINT_IN_3D)
        .value("POINT_IN_2D", EntityBase::Type::POINT_IN_2D)
        .value("POINT_N_TRANS", EntityBase::Type::POINT_N_TRANS)
        .value("POINT_N_ROT_TRANS", EntityBase::Type::POINT_N_ROT_TRANS)
        .value("POINT_N_COPY", EntityBase::Type::POINT_N_COPY)
        .value("POINT_N_ROT_AA", EntityBase::Type::POINT_N_ROT_AA)
        .value("POINT_N_ROT_AXIS_TRANS", EntityBase::Type::POINT_N_ROT_AXIS_TRANS)
        .value("NORMAL_IN_3D", EntityBase::Type::NORMAL_IN_3D)
        .value("NORMAL_IN_2D", EntityBase::Type::NORMAL_IN_2D)
        .value("NORMAL_N_COPY", EntityBase::Type::NORMAL_N_COPY)
        .value("NORMAL_N_ROT", EntityBase::Type::NORMAL_N_ROT)
        .value("NORMAL_N_ROT_AA", EntityBase::Type::NORMAL_N_ROT_AA)
        .value("DISTANCE", EntityBase::Type::DISTANCE)
        .value("DISTANCE_N_COPY", EntityBase::Type::DISTANCE_N_COPY)
        .value("FACE_NORMAL_PT", EntityBase::Type::FACE_NORMAL_PT)
        .value("FACE_XPROD", EntityBase::Type::FACE_XPROD)
        .value("FACE_N_ROT_TRANS", EntityBase::Type::FACE_N_ROT_TRANS)
        .value("FACE_N_TRANS", EntityBase::Type::FACE_N_TRANS)
        .value("FACE_N_ROT_AA", EntityBase::Type::FACE_N_ROT_AA)
        .value("FACE_ROT_NORMAL_PT", EntityBase::Type::FACE_ROT_NORMAL_PT)
        .value("FACE_N_ROT_AXIS_TRANS", EntityBase::Type::FACE_N_ROT_AXIS_TRANS)
        .value("WORKPLANE", EntityBase::Type::WORKPLANE)
        .value("LINE_SEGMENT", EntityBase::Type::LINE_SEGMENT)
        .value("CUBIC", EntityBase::Type::CUBIC)
        .value("CUBIC_PERIODIC", EntityBase::Type::CUBIC_PERIODIC)
        .value("CIRCLE", EntityBase::Type::CIRCLE)
        .value("ARC_OF_CIRCLE", EntityBase::Type::ARC_OF_CIRCLE)
        .value("TTF_TEXT", EntityBase::Type::TTF_TEXT)
        .value("IMAGE", EntityBase::Type::IMAGE);

    py::class_<ConstraintBase> constraint(m, "Constraint");
    constraint.def(py::init<>())
        .def_readwrite("tag", &ConstraintBase::tag)
        .def_readwrite("h", &ConstraintBase::h)
        .def_readwrite("type", &ConstraintBase::type)
        .def_readwrite("group", &ConstraintBase::group)
        .def_readwrite("workplane", &ConstraintBase::workplane)
        .def_readwrite("val_a", &ConstraintBase::valA)
        .def_readwrite("val_p", &ConstraintBase::valP)
        .def_readwrite("pt_a", &ConstraintBase::ptA)
        .def_readwrite("pt_b", &ConstraintBase::ptB)
        .def_readwrite("entity_a", &ConstraintBase::entityA)
        .def_readwrite("entity_b", &ConstraintBase::entityB)
        .def_readwrite("entity_c", &ConstraintBase::entityC)
        .def_readwrite("entity_d", &ConstraintBase::entityD)
        .def_readwrite("other", &ConstraintBase::other)
        .def_readwrite("other2", &ConstraintBase::other2)
        .def_readwrite("reference", &ConstraintBase::reference)
        .def_readwrite("comment", &ConstraintBase::comment)
        .def("equals", &ConstraintBase::Equals)
        .def("has_label", &ConstraintBase::HasLabel)
        .def("has_label", &ConstraintBase::HasLabel)
        .def("is_projectible", &ConstraintBase::IsProjectible)
        // .def("generate", &ConstraintBase::Generate)
        // .def("generate_equations", &ConstraintBase::GenerateEquations)
        // .def("project_vector_into", &ConstraintBase::ProjectVectorInto)
        // .def("project_into", &ConstraintBase::ProjectInto)
        .def("modify_to_satisfy", &ConstraintBase::ModifyToSatisfy)
        // .def("add_eq", &ConstraintBase::AddEq)
        // .def("add_eq", &ConstraintBase::AddEq)
        // .def("add_eq", py::overload_cast<IdList<Equation,hEquation>*, Expr*,
        // int>(&ConstraintBase::AddEq)) .def("add_eq",
        // py::overload_cast<IdList<Equation,hEquation>*, const ExprVector&,
        // int>(&ConstraintBase::AddEq))
        // .def("direction_cosine", &ConstraintBase::DirectionCosine)
        // .def("distance", &ConstraintBase::Distance)
        // .def("point_line_distance", &ConstraintBase::PointLineDistance)
        // .def("point_plane_distance", &ConstraintBase::PointPlaneDistance)
        // .def("vectors_parallel_3d", &ConstraintBase::VectorsParallel3d)
        // .def("point_in_three_space", &ConstraintBase::PointInThreeSpace)
        .def("clear", &ConstraintBase::Clear)
        .def("__repr__", [](const ConstraintBase &c) {
            return "<solvespace.Constraint id=" + std::to_string(c.h.v) + " type=" +
                   std::to_string(static_cast<std::underlying_type<ConstraintBase::Type>::type>(c.type)) + ">";
        });
    py::enum_<ConstraintBase::Type>(constraint, "Type")
        .value("POINTS_COINCIDENT", ConstraintBase::Type::POINTS_COINCIDENT)
        .value("PT_PT_DISTANCE", ConstraintBase::Type::PT_PT_DISTANCE)
        .value("PT_PLANE_DISTANCE", ConstraintBase::Type::PT_PLANE_DISTANCE)
        .value("PT_LINE_DISTANCE", ConstraintBase::Type::PT_LINE_DISTANCE)
        .value("PT_FACE_DISTANCE", ConstraintBase::Type::PT_FACE_DISTANCE)
        .value("PROJ_PT_DISTANCE", ConstraintBase::Type::PROJ_PT_DISTANCE)
        .value("PT_IN_PLANE", ConstraintBase::Type::PT_IN_PLANE)
        .value("PT_ON_LINE", ConstraintBase::Type::PT_ON_LINE)
        .value("PT_ON_FACE", ConstraintBase::Type::PT_ON_FACE)
        .value("EQUAL_LENGTH_LINES", ConstraintBase::Type::EQUAL_LENGTH_LINES)
        .value("LENGTH_RATIO", ConstraintBase::Type::LENGTH_RATIO)
        .value("EQ_LEN_PT_LINE_D", ConstraintBase::Type::EQ_LEN_PT_LINE_D)
        .value("EQ_PT_LN_DISTANCES", ConstraintBase::Type::EQ_PT_LN_DISTANCES)
        .value("EQUAL_ANGLE", ConstraintBase::Type::EQUAL_ANGLE)
        .value("EQUAL_LINE_ARC_LEN", ConstraintBase::Type::EQUAL_LINE_ARC_LEN)
        .value("LENGTH_DIFFERENCE", ConstraintBase::Type::LENGTH_DIFFERENCE)
        .value("SYMMETRIC", ConstraintBase::Type::SYMMETRIC)
        .value("SYMMETRIC_HORIZ", ConstraintBase::Type::SYMMETRIC_HORIZ)
        .value("SYMMETRIC_VERT", ConstraintBase::Type::SYMMETRIC_VERT)
        .value("SYMMETRIC_LINE", ConstraintBase::Type::SYMMETRIC_LINE)
        .value("AT_MIDPOINT", ConstraintBase::Type::AT_MIDPOINT)
        .value("HORIZONTAL", ConstraintBase::Type::HORIZONTAL)
        .value("VERTICAL", ConstraintBase::Type::VERTICAL)
        .value("DIAMETER", ConstraintBase::Type::DIAMETER)
        .value("PT_ON_CIRCLE", ConstraintBase::Type::PT_ON_CIRCLE)
        .value("SAME_ORIENTATION", ConstraintBase::Type::SAME_ORIENTATION)
        .value("ANGLE", ConstraintBase::Type::ANGLE)
        .value("PARALLEL", ConstraintBase::Type::PARALLEL)
        .value("PERPENDICULAR", ConstraintBase::Type::PERPENDICULAR)
        .value("ARC_LINE_TANGENT", ConstraintBase::Type::ARC_LINE_TANGENT)
        .value("CUBIC_LINE_TANGENT", ConstraintBase::Type::CUBIC_LINE_TANGENT)
        .value("CURVE_CURVE_TANGENT", ConstraintBase::Type::CURVE_CURVE_TANGENT)
        .value("EQUAL_RADIUS", ConstraintBase::Type::EQUAL_RADIUS)
        .value("WHERE_DRAGGED", ConstraintBase::Type::WHERE_DRAGGED)
        .value("ARC_ARC_LEN_RATIO", ConstraintBase::Type::ARC_ARC_LEN_RATIO)
        .value("ARC_LINE_LEN_RATIO", ConstraintBase::Type::ARC_LINE_LEN_RATIO)
        .value("ARC_ARC_DIFFERENCE", ConstraintBase::Type::ARC_ARC_DIFFERENCE)
        .value("ARC_LINE_DIFFERENCE", ConstraintBase::Type::ARC_LINE_DIFFERENCE)
        .value("COMMENT", ConstraintBase::Type::COMMENT);

    py::class_<Sketch>(m, "Sketch")
        .def(py::init<>())
        .def("get_entity", &Sketch::GetEntity,
             "Get an entity by it's handle", py::return_value_policy::reference)
        .def("get_constraint", &Sketch::GetConstraint,
             "Get a constraint by it's handle", py::return_value_policy::reference)
        .def("get_param", &Sketch::GetParam,
             "Get a param by it's handle", py::return_value_policy::reference)
        .def("params", py::overload_cast<>(&Sketch::GetParams))
        .def("params",
             py::overload_cast<std::vector<hParam>>(&Sketch::GetParams))
        .def("constraints", &Sketch::GetConstraints)
        .def("entities", &Sketch::GetEntities)
        .def_readwrite("group", &Sketch::group)
        .def_readwrite("param", &Sketch::param)
        .def_readwrite("entity", &Sketch::entity)
        .def_readwrite("constraint", &Sketch::constraint)
        .def("add_group", &Sketch::AddGroup)
        .def("set_active_group", &Sketch::SetActiveGroup)
        .def("get_active_group", &Sketch::GetActiveGroup)
        .def("add_param", &Sketch::AddParam)
        .def("add_point_2d", &Sketch::AddPoint2D)
        .def("add_point_3d", &Sketch::AddPoint3D)
        .def("add_normal_2d", &Sketch::AddNormal2D)
        .def("add_normal_3d", &Sketch::AddNormal3D)
        .def("add_distance", &Sketch::AddDistance)
        .def("add_line_2d", &Sketch::AddLine2D)
        .def("add_line_3d", &Sketch::AddLine3D)
        .def("add_cubic", &Sketch::AddCubic)
        .def("add_arc", &Sketch::AddArc)
        .def("add_circle", &Sketch::AddCircle)
        .def("add_workplane", &Sketch::AddWorkplane)
        .def("create_2d_base", &Sketch::Create2DBase)
        .def("add_constraint", &Sketch::AddConstraint)
        .def("coincident", &Sketch::Coincident)
        .def("distance", &Sketch::Distance)
        .def("equal", &Sketch::Equal)
        .def("equal_angle", &Sketch::EqualAngle)
        .def("equal_point_to_line", &Sketch::EqualPointToLine)
        .def("ratio", &Sketch::Ratio)
        .def("symmetric", &Sketch::Symmetric)
        .def("symmetric_h", &Sketch::SymmetricH)
        .def("symmetric_v", &Sketch::SymmetricV)
        .def("midpoint", &Sketch::Midpoint)
        .def("horizontal", &Sketch::Horizontal)
        .def("vertical", &Sketch::Vertical)
        .def("diameter", &Sketch::Diameter)
        .def("same_orientation", &Sketch::SameOrientation)
        .def("angle", &Sketch::Angle)
        .def("perpendicular", &Sketch::Perpendicular)
        .def("parallel", &Sketch::Parallel)
        .def("tangent", &Sketch::Tangent)
        .def("distance_proj", &Sketch::DistanceProj)
        .def("dragged", &Sketch::Dragged)
        .def("length_diff", &Sketch::LengthDiff)
        .def("clear", &Sketch::Clear);

    py::enum_<SolveResult>(m, "SolveResult")
        .value("OKAY", SolveResult::OKAY)
        .value("DIDNT_CONVERGE", SolveResult::DIDNT_CONVERGE)
        .value("REDUNDANT_OKAY", SolveResult::REDUNDANT_OKAY)
        .value("REDUNDANT_DIDNT_CONVERGE", SolveResult::REDUNDANT_DIDNT_CONVERGE)
        .value("TOO_MANY_UNKNOWNS", SolveResult::TOO_MANY_UNKNOWNS);

    py::class_<SolverResult>(m, "SolverResult")
        .def_readonly("status", &SolverResult::status)
        .def_readonly("bad", &SolverResult::bad)
        .def_readonly("rank", &SolverResult::rank)
        .def_readonly("dof", &SolverResult::dof);

    py::class_<SolverSystem>(m, "SolverSystem")
        .def(py::init<>())
        .def("solve", &SolverSystem::Solve);
    //     .def("solve_rank", &System::SolveRank);

    m.attr("SK") = &SK;
}
