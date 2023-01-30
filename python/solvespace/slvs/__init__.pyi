from __future__ import annotations
import solvespace.slvs
import typing

__all__ = [
    "Constraint",
    "Entity",
    "Equation",
    "Group",
    "Param",
    "Quaternion",
    "SK",
    "Sketch",
    "SolveResult",
    "SolverResult",
    "SolverSystem",
    "Vector",
    "hConstraint",
    "hEntity",
    "hEquation",
    "hGroup",
    "hParam"
]


class Constraint():
    class Type():
        """
        Members:

          POINTS_COINCIDENT

          PT_PT_DISTANCE

          PT_PLANE_DISTANCE

          PT_LINE_DISTANCE

          PT_FACE_DISTANCE

          PROJ_PT_DISTANCE

          PT_IN_PLANE

          PT_ON_LINE

          PT_ON_FACE

          EQUAL_LENGTH_LINES

          LENGTH_RATIO

          EQ_LEN_PT_LINE_D

          EQ_PT_LN_DISTANCES

          EQUAL_ANGLE

          EQUAL_LINE_ARC_LEN

          LENGTH_DIFFERENCE

          SYMMETRIC

          SYMMETRIC_HORIZ

          SYMMETRIC_VERT

          SYMMETRIC_LINE

          AT_MIDPOINT

          HORIZONTAL

          VERTICAL

          DIAMETER

          PT_ON_CIRCLE

          SAME_ORIENTATION

          ANGLE

          PARALLEL

          PERPENDICULAR

          ARC_LINE_TANGENT

          CUBIC_LINE_TANGENT

          CURVE_CURVE_TANGENT

          EQUAL_RADIUS

          WHERE_DRAGGED

          ARC_ARC_LEN_RATIO

          ARC_LINE_LEN_RATIO

          ARC_ARC_DIFFERENCE

          ARC_LINE_DIFFERENCE

          COMMENT
        """
        def __eq__(self, other: object) -> bool: ...
        def __getstate__(self) -> int: ...
        def __hash__(self) -> int: ...
        def __index__(self) -> int: ...
        def __init__(self, value: int) -> None: ...
        def __int__(self) -> int: ...
        def __ne__(self, other: object) -> bool: ...
        def __repr__(self) -> str: ...
        def __setstate__(self, state: int) -> None: ...
        @property
        def name(self) -> str:
            """
            :type: str
            """
        @property
        def value(self) -> int:
            """
            :type: int
            """
        ANGLE: solvespace.slvs.Constraint.Type # value = <Type.ANGLE: 120>
        ARC_ARC_DIFFERENCE: solvespace.slvs.Constraint.Type # value = <Type.ARC_ARC_DIFFERENCE: 212>
        ARC_ARC_LEN_RATIO: solvespace.slvs.Constraint.Type # value = <Type.ARC_ARC_LEN_RATIO: 210>
        ARC_LINE_DIFFERENCE: solvespace.slvs.Constraint.Type # value = <Type.ARC_LINE_DIFFERENCE: 213>
        ARC_LINE_LEN_RATIO: solvespace.slvs.Constraint.Type # value = <Type.ARC_LINE_LEN_RATIO: 211>
        ARC_LINE_TANGENT: solvespace.slvs.Constraint.Type # value = <Type.ARC_LINE_TANGENT: 123>
        AT_MIDPOINT: solvespace.slvs.Constraint.Type # value = <Type.AT_MIDPOINT: 70>
        COMMENT: solvespace.slvs.Constraint.Type # value = <Type.COMMENT: 1000>
        CUBIC_LINE_TANGENT: solvespace.slvs.Constraint.Type # value = <Type.CUBIC_LINE_TANGENT: 124>
        CURVE_CURVE_TANGENT: solvespace.slvs.Constraint.Type # value = <Type.CURVE_CURVE_TANGENT: 125>
        DIAMETER: solvespace.slvs.Constraint.Type # value = <Type.DIAMETER: 90>
        EQUAL_ANGLE: solvespace.slvs.Constraint.Type # value = <Type.EQUAL_ANGLE: 54>
        EQUAL_LENGTH_LINES: solvespace.slvs.Constraint.Type # value = <Type.EQUAL_LENGTH_LINES: 50>
        EQUAL_LINE_ARC_LEN: solvespace.slvs.Constraint.Type # value = <Type.EQUAL_LINE_ARC_LEN: 55>
        EQUAL_RADIUS: solvespace.slvs.Constraint.Type # value = <Type.EQUAL_RADIUS: 130>
        EQ_LEN_PT_LINE_D: solvespace.slvs.Constraint.Type # value = <Type.EQ_LEN_PT_LINE_D: 52>
        EQ_PT_LN_DISTANCES: solvespace.slvs.Constraint.Type # value = <Type.EQ_PT_LN_DISTANCES: 53>
        HORIZONTAL: solvespace.slvs.Constraint.Type # value = <Type.HORIZONTAL: 80>
        LENGTH_DIFFERENCE: solvespace.slvs.Constraint.Type # value = <Type.LENGTH_DIFFERENCE: 56>
        LENGTH_RATIO: solvespace.slvs.Constraint.Type # value = <Type.LENGTH_RATIO: 51>
        PARALLEL: solvespace.slvs.Constraint.Type # value = <Type.PARALLEL: 121>
        PERPENDICULAR: solvespace.slvs.Constraint.Type # value = <Type.PERPENDICULAR: 122>
        POINTS_COINCIDENT: solvespace.slvs.Constraint.Type # value = <Type.POINTS_COINCIDENT: 20>
        PROJ_PT_DISTANCE: solvespace.slvs.Constraint.Type # value = <Type.PROJ_PT_DISTANCE: 34>
        PT_FACE_DISTANCE: solvespace.slvs.Constraint.Type # value = <Type.PT_FACE_DISTANCE: 33>
        PT_IN_PLANE: solvespace.slvs.Constraint.Type # value = <Type.PT_IN_PLANE: 41>
        PT_LINE_DISTANCE: solvespace.slvs.Constraint.Type # value = <Type.PT_LINE_DISTANCE: 32>
        PT_ON_CIRCLE: solvespace.slvs.Constraint.Type # value = <Type.PT_ON_CIRCLE: 100>
        PT_ON_FACE: solvespace.slvs.Constraint.Type # value = <Type.PT_ON_FACE: 43>
        PT_ON_LINE: solvespace.slvs.Constraint.Type # value = <Type.PT_ON_LINE: 42>
        PT_PLANE_DISTANCE: solvespace.slvs.Constraint.Type # value = <Type.PT_PLANE_DISTANCE: 31>
        PT_PT_DISTANCE: solvespace.slvs.Constraint.Type # value = <Type.PT_PT_DISTANCE: 30>
        SAME_ORIENTATION: solvespace.slvs.Constraint.Type # value = <Type.SAME_ORIENTATION: 110>
        SYMMETRIC: solvespace.slvs.Constraint.Type # value = <Type.SYMMETRIC: 60>
        SYMMETRIC_HORIZ: solvespace.slvs.Constraint.Type # value = <Type.SYMMETRIC_HORIZ: 61>
        SYMMETRIC_LINE: solvespace.slvs.Constraint.Type # value = <Type.SYMMETRIC_LINE: 63>
        SYMMETRIC_VERT: solvespace.slvs.Constraint.Type # value = <Type.SYMMETRIC_VERT: 62>
        VERTICAL: solvespace.slvs.Constraint.Type # value = <Type.VERTICAL: 81>
        WHERE_DRAGGED: solvespace.slvs.Constraint.Type # value = <Type.WHERE_DRAGGED: 200>
        __members__: dict # value = {'POINTS_COINCIDENT': <Type.POINTS_COINCIDENT: 20>, 'PT_PT_DISTANCE': <Type.PT_PT_DISTANCE: 30>, 'PT_PLANE_DISTANCE': <Type.PT_PLANE_DISTANCE: 31>, 'PT_LINE_DISTANCE': <Type.PT_LINE_DISTANCE: 32>, 'PT_FACE_DISTANCE': <Type.PT_FACE_DISTANCE: 33>, 'PROJ_PT_DISTANCE': <Type.PROJ_PT_DISTANCE: 34>, 'PT_IN_PLANE': <Type.PT_IN_PLANE: 41>, 'PT_ON_LINE': <Type.PT_ON_LINE: 42>, 'PT_ON_FACE': <Type.PT_ON_FACE: 43>, 'EQUAL_LENGTH_LINES': <Type.EQUAL_LENGTH_LINES: 50>, 'LENGTH_RATIO': <Type.LENGTH_RATIO: 51>, 'EQ_LEN_PT_LINE_D': <Type.EQ_LEN_PT_LINE_D: 52>, 'EQ_PT_LN_DISTANCES': <Type.EQ_PT_LN_DISTANCES: 53>, 'EQUAL_ANGLE': <Type.EQUAL_ANGLE: 54>, 'EQUAL_LINE_ARC_LEN': <Type.EQUAL_LINE_ARC_LEN: 55>, 'LENGTH_DIFFERENCE': <Type.LENGTH_DIFFERENCE: 56>, 'SYMMETRIC': <Type.SYMMETRIC: 60>, 'SYMMETRIC_HORIZ': <Type.SYMMETRIC_HORIZ: 61>, 'SYMMETRIC_VERT': <Type.SYMMETRIC_VERT: 62>, 'SYMMETRIC_LINE': <Type.SYMMETRIC_LINE: 63>, 'AT_MIDPOINT': <Type.AT_MIDPOINT: 70>, 'HORIZONTAL': <Type.HORIZONTAL: 80>, 'VERTICAL': <Type.VERTICAL: 81>, 'DIAMETER': <Type.DIAMETER: 90>, 'PT_ON_CIRCLE': <Type.PT_ON_CIRCLE: 100>, 'SAME_ORIENTATION': <Type.SAME_ORIENTATION: 110>, 'ANGLE': <Type.ANGLE: 120>, 'PARALLEL': <Type.PARALLEL: 121>, 'PERPENDICULAR': <Type.PERPENDICULAR: 122>, 'ARC_LINE_TANGENT': <Type.ARC_LINE_TANGENT: 123>, 'CUBIC_LINE_TANGENT': <Type.CUBIC_LINE_TANGENT: 124>, 'CURVE_CURVE_TANGENT': <Type.CURVE_CURVE_TANGENT: 125>, 'EQUAL_RADIUS': <Type.EQUAL_RADIUS: 130>, 'WHERE_DRAGGED': <Type.WHERE_DRAGGED: 200>, 'ARC_ARC_LEN_RATIO': <Type.ARC_ARC_LEN_RATIO: 210>, 'ARC_LINE_LEN_RATIO': <Type.ARC_LINE_LEN_RATIO: 211>, 'ARC_ARC_DIFFERENCE': <Type.ARC_ARC_DIFFERENCE: 212>, 'ARC_LINE_DIFFERENCE': <Type.ARC_LINE_DIFFERENCE: 213>, 'COMMENT': <Type.COMMENT: 1000>}
        pass
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...
    def clear(self) -> None: ...
    def equals(self, arg0: Constraint) -> bool: ...
    def has_label(self) -> bool: ...
    def is_projectible(self) -> bool: ...
    def modify_to_satisfy(self) -> None: ...
    @property
    def comment(self) -> str:
        """
        :type: str
        """
    @comment.setter
    def comment(self, arg0: str) -> None:
        pass
    @property
    def entity_a(self) -> hEntity:
        """
        :type: hEntity
        """
    @entity_a.setter
    def entity_a(self, arg0: hEntity) -> None:
        pass
    @property
    def entity_b(self) -> hEntity:
        """
        :type: hEntity
        """
    @entity_b.setter
    def entity_b(self, arg0: hEntity) -> None:
        pass
    @property
    def entity_c(self) -> hEntity:
        """
        :type: hEntity
        """
    @entity_c.setter
    def entity_c(self, arg0: hEntity) -> None:
        pass
    @property
    def entity_d(self) -> hEntity:
        """
        :type: hEntity
        """
    @entity_d.setter
    def entity_d(self, arg0: hEntity) -> None:
        pass
    @property
    def group(self) -> hGroup:
        """
        :type: hGroup
        """
    @group.setter
    def group(self, arg0: hGroup) -> None:
        pass
    @property
    def h(self) -> hConstraint:
        """
        :type: hConstraint
        """
    @h.setter
    def h(self, arg0: hConstraint) -> None:
        pass
    @property
    def other(self) -> bool:
        """
        :type: bool
        """
    @other.setter
    def other(self, arg0: bool) -> None:
        pass
    @property
    def other2(self) -> bool:
        """
        :type: bool
        """
    @other2.setter
    def other2(self, arg0: bool) -> None:
        pass
    @property
    def pt_a(self) -> hEntity:
        """
        :type: hEntity
        """
    @pt_a.setter
    def pt_a(self, arg0: hEntity) -> None:
        pass
    @property
    def pt_b(self) -> hEntity:
        """
        :type: hEntity
        """
    @pt_b.setter
    def pt_b(self, arg0: hEntity) -> None:
        pass
    @property
    def reference(self) -> bool:
        """
        :type: bool
        """
    @reference.setter
    def reference(self, arg0: bool) -> None:
        pass
    @property
    def tag(self) -> int:
        """
        :type: int
        """
    @tag.setter
    def tag(self, arg0: int) -> None:
        pass
    @property
    def type(self) -> SolveSpace::ConstraintBase::Type:
        """
        :type: SolveSpace::ConstraintBase::Type
        """
    @type.setter
    def type(self, arg0: SolveSpace::ConstraintBase::Type) -> None:
        pass
    @property
    def val_a(self) -> float:
        """
        :type: float
        """
    @val_a.setter
    def val_a(self, arg0: float) -> None:
        pass
    @property
    def val_p(self) -> hParam:
        """
        :type: hParam
        """
    @val_p.setter
    def val_p(self, arg0: hParam) -> None:
        pass
    @property
    def workplane(self) -> hEntity:
        """
        :type: hEntity
        """
    @workplane.setter
    def workplane(self, arg0: hEntity) -> None:
        pass
    pass
class Entity():
    class Type():
        """
        Members:

          POINT_IN_3D

          POINT_IN_2D

          POINT_N_TRANS

          POINT_N_ROT_TRANS

          POINT_N_COPY

          POINT_N_ROT_AA

          POINT_N_ROT_AXIS_TRANS

          NORMAL_IN_3D

          NORMAL_IN_2D

          NORMAL_N_COPY

          NORMAL_N_ROT

          NORMAL_N_ROT_AA

          DISTANCE

          DISTANCE_N_COPY

          FACE_NORMAL_PT

          FACE_XPROD

          FACE_N_ROT_TRANS

          FACE_N_TRANS

          FACE_N_ROT_AA

          FACE_ROT_NORMAL_PT

          FACE_N_ROT_AXIS_TRANS

          WORKPLANE

          LINE_SEGMENT

          CUBIC

          CUBIC_PERIODIC

          CIRCLE

          ARC_OF_CIRCLE

          TTF_TEXT

          IMAGE
        """
        def __eq__(self, other: object) -> bool: ...
        def __getstate__(self) -> int: ...
        def __hash__(self) -> int: ...
        def __index__(self) -> int: ...
        def __init__(self, value: int) -> None: ...
        def __int__(self) -> int: ...
        def __ne__(self, other: object) -> bool: ...
        def __repr__(self) -> str: ...
        def __setstate__(self, state: int) -> None: ...
        @property
        def name(self) -> str:
            """
            :type: str
            """
        @property
        def value(self) -> int:
            """
            :type: int
            """
        ARC_OF_CIRCLE: solvespace.slvs.Entity.Type # value = <Type.ARC_OF_CIRCLE: 14000>
        CIRCLE: solvespace.slvs.Entity.Type # value = <Type.CIRCLE: 13000>
        CUBIC: solvespace.slvs.Entity.Type # value = <Type.CUBIC: 12000>
        CUBIC_PERIODIC: solvespace.slvs.Entity.Type # value = <Type.CUBIC_PERIODIC: 12001>
        DISTANCE: solvespace.slvs.Entity.Type # value = <Type.DISTANCE: 4000>
        DISTANCE_N_COPY: solvespace.slvs.Entity.Type # value = <Type.DISTANCE_N_COPY: 4001>
        FACE_NORMAL_PT: solvespace.slvs.Entity.Type # value = <Type.FACE_NORMAL_PT: 5000>
        FACE_N_ROT_AA: solvespace.slvs.Entity.Type # value = <Type.FACE_N_ROT_AA: 5004>
        FACE_N_ROT_AXIS_TRANS: solvespace.slvs.Entity.Type # value = <Type.FACE_N_ROT_AXIS_TRANS: 5006>
        FACE_N_ROT_TRANS: solvespace.slvs.Entity.Type # value = <Type.FACE_N_ROT_TRANS: 5002>
        FACE_N_TRANS: solvespace.slvs.Entity.Type # value = <Type.FACE_N_TRANS: 5003>
        FACE_ROT_NORMAL_PT: solvespace.slvs.Entity.Type # value = <Type.FACE_ROT_NORMAL_PT: 5005>
        FACE_XPROD: solvespace.slvs.Entity.Type # value = <Type.FACE_XPROD: 5001>
        IMAGE: solvespace.slvs.Entity.Type # value = <Type.IMAGE: 16000>
        LINE_SEGMENT: solvespace.slvs.Entity.Type # value = <Type.LINE_SEGMENT: 11000>
        NORMAL_IN_2D: solvespace.slvs.Entity.Type # value = <Type.NORMAL_IN_2D: 3001>
        NORMAL_IN_3D: solvespace.slvs.Entity.Type # value = <Type.NORMAL_IN_3D: 3000>
        NORMAL_N_COPY: solvespace.slvs.Entity.Type # value = <Type.NORMAL_N_COPY: 3010>
        NORMAL_N_ROT: solvespace.slvs.Entity.Type # value = <Type.NORMAL_N_ROT: 3011>
        NORMAL_N_ROT_AA: solvespace.slvs.Entity.Type # value = <Type.NORMAL_N_ROT_AA: 3012>
        POINT_IN_2D: solvespace.slvs.Entity.Type # value = <Type.POINT_IN_2D: 2001>
        POINT_IN_3D: solvespace.slvs.Entity.Type # value = <Type.POINT_IN_3D: 2000>
        POINT_N_COPY: solvespace.slvs.Entity.Type # value = <Type.POINT_N_COPY: 2012>
        POINT_N_ROT_AA: solvespace.slvs.Entity.Type # value = <Type.POINT_N_ROT_AA: 2013>
        POINT_N_ROT_AXIS_TRANS: solvespace.slvs.Entity.Type # value = <Type.POINT_N_ROT_AXIS_TRANS: 2014>
        POINT_N_ROT_TRANS: solvespace.slvs.Entity.Type # value = <Type.POINT_N_ROT_TRANS: 2011>
        POINT_N_TRANS: solvespace.slvs.Entity.Type # value = <Type.POINT_N_TRANS: 2010>
        TTF_TEXT: solvespace.slvs.Entity.Type # value = <Type.TTF_TEXT: 15000>
        WORKPLANE: solvespace.slvs.Entity.Type # value = <Type.WORKPLANE: 10000>
        __members__: dict # value = {'POINT_IN_3D': <Type.POINT_IN_3D: 2000>, 'POINT_IN_2D': <Type.POINT_IN_2D: 2001>, 'POINT_N_TRANS': <Type.POINT_N_TRANS: 2010>, 'POINT_N_ROT_TRANS': <Type.POINT_N_ROT_TRANS: 2011>, 'POINT_N_COPY': <Type.POINT_N_COPY: 2012>, 'POINT_N_ROT_AA': <Type.POINT_N_ROT_AA: 2013>, 'POINT_N_ROT_AXIS_TRANS': <Type.POINT_N_ROT_AXIS_TRANS: 2014>, 'NORMAL_IN_3D': <Type.NORMAL_IN_3D: 3000>, 'NORMAL_IN_2D': <Type.NORMAL_IN_2D: 3001>, 'NORMAL_N_COPY': <Type.NORMAL_N_COPY: 3010>, 'NORMAL_N_ROT': <Type.NORMAL_N_ROT: 3011>, 'NORMAL_N_ROT_AA': <Type.NORMAL_N_ROT_AA: 3012>, 'DISTANCE': <Type.DISTANCE: 4000>, 'DISTANCE_N_COPY': <Type.DISTANCE_N_COPY: 4001>, 'FACE_NORMAL_PT': <Type.FACE_NORMAL_PT: 5000>, 'FACE_XPROD': <Type.FACE_XPROD: 5001>, 'FACE_N_ROT_TRANS': <Type.FACE_N_ROT_TRANS: 5002>, 'FACE_N_TRANS': <Type.FACE_N_TRANS: 5003>, 'FACE_N_ROT_AA': <Type.FACE_N_ROT_AA: 5004>, 'FACE_ROT_NORMAL_PT': <Type.FACE_ROT_NORMAL_PT: 5005>, 'FACE_N_ROT_AXIS_TRANS': <Type.FACE_N_ROT_AXIS_TRANS: 5006>, 'WORKPLANE': <Type.WORKPLANE: 10000>, 'LINE_SEGMENT': <Type.LINE_SEGMENT: 11000>, 'CUBIC': <Type.CUBIC: 12000>, 'CUBIC_PERIODIC': <Type.CUBIC_PERIODIC: 12001>, 'CIRCLE': <Type.CIRCLE: 13000>, 'ARC_OF_CIRCLE': <Type.ARC_OF_CIRCLE: 14000>, 'TTF_TEXT': <Type.TTF_TEXT: 15000>, 'IMAGE': <Type.IMAGE: 16000>}
        pass
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...
    def arc_get_angles(self, arg0: float, arg1: float, arg2: float) -> None: ...
    def circle_get_radius_num(self) -> float: ...
    def clear(self) -> None: ...
    def cubic_get_finish_num(self) -> Vector: ...
    def cubic_get_finish_tangent_num(self) -> Vector: ...
    def cubic_get_start_num(self) -> Vector: ...
    def cubic_get_start_tangent_num(self) -> Vector: ...
    def distance_force_to(self, arg0: float) -> None: ...
    def distance_get_num(self) -> float: ...
    def endpoint_finish(self) -> Vector: ...
    def endpoint_start(self) -> Vector: ...
    def face_get_normal_num(self) -> Vector: ...
    def face_get_point_num(self) -> Vector: ...
    def get_axis_angle_quaternion(self, arg0: int) -> Quaternion: ...
    def has_endpoints(self) -> bool: ...
    def has_vector(self) -> bool: ...
    def is_3d(self) -> bool: ...
    def is_arc(self) -> bool: ...
    def is_circle(self) -> bool: ...
    def is_cubic(self) -> bool: ...
    def is_distance(self) -> bool: ...
    def is_face(self) -> bool: ...
    def is_free_in_3d(self) -> bool: ...
    def is_in_plane(self, arg0: Vector, arg1: float) -> bool: ...
    def is_line(self) -> bool: ...
    def is_line_2d(self) -> bool: ...
    def is_line_3d(self) -> bool: ...
    def is_none(self) -> bool: ...
    def is_normal(self) -> bool: ...
    def is_normal_2d(self) -> bool: ...
    def is_normal_3d(self) -> bool: ...
    def is_point(self) -> bool: ...
    def is_point_2d(self) -> bool: ...
    def is_point_3d(self) -> bool: ...
    def is_workplane(self) -> bool: ...
    def normal(self) -> Entity: ...
    def normal_force_to(self, arg0: Quaternion) -> None: ...
    def normal_get_num(self) -> Quaternion: ...
    def normal_n(self) -> Vector: ...
    def normal_u(self) -> Vector: ...
    def normal_v(self) -> Vector: ...
    def params(self) -> typing.List[hParam]: ...
    def point_force_param_to(self, arg0: Vector) -> None: ...
    def point_force_quaternion_to(self, arg0: Quaternion) -> None: ...
    def point_force_to(self, arg0: Vector) -> None: ...
    def point_get_num(self) -> Vector: ...
    def point_get_quaternion(self) -> Quaternion: ...
    def vector_get_num(self) -> Vector: ...
    def vector_get_ref_point(self) -> Vector: ...
    def vector_get_start_point(self) -> Vector: ...
    def workplane_get_offset(self) -> Vector: ...
    @property
    def aspect_ratio(self) -> float:
        """
        :type: float
        """
    @aspect_ratio.setter
    def aspect_ratio(self, arg0: float) -> None:
        pass
    @property
    def group(self) -> hGroup:
        """
        :type: hGroup
        """
    @group.setter
    def group(self, arg0: hGroup) -> None:
        pass
    @property
    def h(self) -> hEntity:
        """
        :type: hEntity
        """
    @h.setter
    def h(self, arg0: hEntity) -> None:
        pass
    @property
    def num_distance(self) -> float:
        """
        :type: float
        """
    @num_distance.setter
    def num_distance(self, arg0: float) -> None:
        pass
    @property
    def num_normal(self) -> Quaternion:
        """
        :type: Quaternion
        """
    @num_normal.setter
    def num_normal(self, arg0: Quaternion) -> None:
        pass
    @property
    def num_point(self) -> Vector:
        """
        :type: Vector
        """
    @num_point.setter
    def num_point(self, arg0: Vector) -> None:
        pass
    @property
    def param(self) -> typing.List[hParam[8]]:
        """
        :type: typing.List[hParam[8]]
        """
    @param.setter
    def param(self, arg0: typing.List[hParam[8]]) -> None:
        pass
    @property
    def point(self) -> typing.List[hEntity[12]]:
        """
        :type: typing.List[hEntity[12]]
        """
    @point.setter
    def point(self, arg0: typing.List[hEntity[12]]) -> None:
        pass
    @property
    def str(self) -> str:
        """
        :type: str
        """
    @str.setter
    def str(self, arg0: str) -> None:
        pass
    @property
    def tag(self) -> int:
        """
        :type: int
        """
    @tag.setter
    def tag(self, arg0: int) -> None:
        pass
    @property
    def times_applied(self) -> int:
        """
        :type: int
        """
    @times_applied.setter
    def times_applied(self, arg0: int) -> None:
        pass
    @property
    def type(self) -> SolveSpace::EntityBase::Type:
        """
        :type: SolveSpace::EntityBase::Type
        """
    @type.setter
    def type(self, arg0: SolveSpace::EntityBase::Type) -> None:
        pass
    @property
    def workplane(self) -> hEntity:
        """
        :type: hEntity
        """
    @workplane.setter
    def workplane(self, arg0: hEntity) -> None:
        pass
    FREE_IN_3D: solvespace.slvs.Entity # value = <solvespace.Entity:h:0 group:0 workplane:0 point: [] normal:0 distance:0 type: 0 param: [] id=0>
    NO_ENTITY: solvespace.slvs.Entity # value = <solvespace.Entity:h:0 group:0 workplane:0 point: [] normal:0 distance:0 type: 0 param: [] id=0>
    pass
class Equation():
    def clear(self) -> None: ...
    @property
    def h(self) -> hEquation:
        """
        :type: hEquation
        """
    @h.setter
    def h(self, arg0: hEquation) -> None:
        pass
    @property
    def tag(self) -> int:
        """
        :type: int
        """
    @tag.setter
    def tag(self, arg0: int) -> None:
        pass
    pass
class Group():
    def __init__(self) -> None: ...
    def __repr__(self) -> str: ...
    @property
    def all_dims_referenc(self) -> bool:
        """
        :type: bool
        """
    @all_dims_referenc.setter
    def all_dims_referenc(self, arg0: bool) -> None:
        pass
    @property
    def allow_redundant(self) -> bool:
        """
        :type: bool
        """
    @allow_redundant.setter
    def allow_redundant(self, arg0: bool) -> None:
        pass
    @property
    def h(self) -> hGroup:
        """
        :type: hGroup
        """
    @h.setter
    def h(self, arg0: hGroup) -> None:
        pass
    @property
    def relax_constraints(self) -> bool:
        """
        :type: bool
        """
    @relax_constraints.setter
    def relax_constraints(self, arg0: bool) -> None:
        pass
    @property
    def suppress_dof_calculation(self) -> bool:
        """
        :type: bool
        """
    @suppress_dof_calculation.setter
    def suppress_dof_calculation(self, arg0: bool) -> None:
        pass
    pass
class Param():
    def __repr__(self) -> str: ...
    @property
    def free(self) -> bool:
        """
        :type: bool
        """
    @free.setter
    def free(self, arg0: bool) -> None:
        pass
    @property
    def h(self) -> hParam:
        """
        :type: hParam
        """
    @h.setter
    def h(self, arg0: hParam) -> None:
        pass
    @property
    def known(self) -> bool:
        """
        :type: bool
        """
    @known.setter
    def known(self, arg0: bool) -> None:
        pass
    @property
    def tag(self) -> int:
        """
        :type: int
        """
    @tag.setter
    def tag(self, arg0: int) -> None:
        pass
    @property
    def val(self) -> float:
        """
        :type: float
        """
    @val.setter
    def val(self, arg0: float) -> None:
        pass
    pass
class Quaternion():
    def inverse(self) -> Quaternion: ...
    def magnitude(self) -> float: ...
    @staticmethod
    @typing.overload
    def make(arg0: Vector, arg1: Vector) -> Quaternion: ...
    @staticmethod
    @typing.overload
    def make(arg0: Vector, arg1: float) -> Quaternion: ...
    @staticmethod
    @typing.overload
    def make(arg0: float, arg1: float, arg2: float, arg3: float) -> Quaternion: ...
    def minus(self, arg0: Quaternion) -> Quaternion: ...
    def mirror(self) -> Quaternion: ...
    def plus(self, arg0: Quaternion) -> Quaternion: ...
    def rotate(self, arg0: Vector) -> Vector: ...
    def rotation_n(self) -> Vector: ...
    def rotation_u(self) -> Vector: ...
    def rotation_v(self) -> Vector: ...
    def scaled_by(self, arg0: float) -> Quaternion: ...
    def times(self, arg0: Quaternion) -> Quaternion: ...
    def to_the(self, arg0: float) -> Quaternion: ...
    def with_magnitude(self, arg0: float) -> Quaternion: ...
    @property
    def vx(self) -> float:
        """
        :type: float
        """
    @vx.setter
    def vx(self, arg0: float) -> None:
        pass
    @property
    def vy(self) -> float:
        """
        :type: float
        """
    @vy.setter
    def vy(self, arg0: float) -> None:
        pass
    @property
    def vz(self) -> float:
        """
        :type: float
        """
    @vz.setter
    def vz(self, arg0: float) -> None:
        pass
    @property
    def w(self) -> float:
        """
        :type: float
        """
    @w.setter
    def w(self, arg0: float) -> None:
        pass
    pass
class Sketch():
    def __init__(self) -> None: ...
    def add_arc(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity, arg4: Entity) -> Entity: ...
    def add_circle(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity) -> Entity: ...
    def add_constraint(self, arg0: Constraint.Type, arg1: Entity, arg2: float, arg3: Entity, arg4: Entity, arg5: Entity, arg6: Entity, arg7: Entity, arg8: Entity, arg9: int, arg10: int) -> Constraint: ...
    def add_cubic(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity, arg4: Entity) -> Entity: ...
    def add_distance(self, arg0: float, arg1: Entity) -> Entity: ...
    def add_group(self, arg0: Group) -> None: ...
    def add_line_2d(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Entity: ...
    def add_line_3d(self, arg0: Entity, arg1: Entity) -> Entity: ...
    def add_normal_2d(self, arg0: Entity) -> Entity: ...
    def add_normal_3d(self, arg0: float, arg1: float, arg2: float, arg3: float) -> Entity: ...
    def add_param(self, arg0: float) -> Param: ...
    def add_point_2d(self, arg0: float, arg1: float, arg2: Entity) -> Entity: ...
    def add_point_3d(self, arg0: float, arg1: float, arg2: float) -> Entity: ...
    def add_workplane(self, arg0: Entity, arg1: Entity) -> Entity: ...
    def angle(self, arg0: Entity, arg1: Entity, arg2: float, arg3: Entity, arg4: bool) -> Constraint: ...
    def clear(self) -> None: ...
    def coincident(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def constraints(self) -> typing.List[Constraint]: ...
    def create_2d_base(self) -> Entity: ...
    def diameter(self, arg0: Entity, arg1: float) -> Constraint: ...
    def distance(self, arg0: Entity, arg1: Entity, arg2: float, arg3: Entity) -> Constraint: ...
    def distance_proj(self, arg0: Entity, arg1: Entity, arg2: float) -> Constraint: ...
    def dragged(self, arg0: Entity, arg1: Entity) -> Constraint: ...
    def entities(self) -> typing.List[Entity]: ...
    def equal(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def equal_angle(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity, arg4: Entity) -> Constraint: ...
    def equal_point_to_line(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity, arg4: Entity) -> Constraint: ...
    def get_active_group(self) -> Group: ...
    def get_constraint(self, arg0: hConstraint) -> Constraint: 
        """
        Get a constraint by it's handle
        """
    def get_entity(self, arg0: hEntity) -> Entity: 
        """
        Get an entity by it's handle
        """
    def get_param(self, arg0: hParam) -> Param: 
        """
        Get a param by it's handle
        """
    def horizontal(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def length_diff(self, arg0: Entity, arg1: Entity, arg2: float, arg3: Entity) -> Constraint: ...
    def midpoint(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def parallel(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    @typing.overload
    def params(self) -> typing.List[Param]: ...
    @typing.overload
    def params(self, arg0: typing.List[hParam]) -> typing.List[float]: ...
    def perpendicular(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: bool) -> Constraint: ...
    def ratio(self, arg0: Entity, arg1: Entity, arg2: float, arg3: Entity) -> Constraint: ...
    def same_orientation(self, arg0: Entity, arg1: Entity) -> Constraint: ...
    def set_active_group(self, arg0: Group) -> None: ...
    def symmetric(self, arg0: Entity, arg1: Entity, arg2: Entity, arg3: Entity) -> Constraint: ...
    def symmetric_h(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def symmetric_v(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def tangent(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    def vertical(self, arg0: Entity, arg1: Entity, arg2: Entity) -> Constraint: ...
    @property
    def constraint(self) -> SolveSpace::IdList<SolveSpace::ConstraintBase, SolveSpace::hConstraint>:
        """
        :type: SolveSpace::IdList<SolveSpace::ConstraintBase, SolveSpace::hConstraint>
        """
    @constraint.setter
    def constraint(self, arg0: SolveSpace::IdList<SolveSpace::ConstraintBase, SolveSpace::hConstraint>) -> None:
        pass
    @property
    def entity(self) -> SolveSpace::IdList<SolveSpace::EntityBase, SolveSpace::hEntity>:
        """
        :type: SolveSpace::IdList<SolveSpace::EntityBase, SolveSpace::hEntity>
        """
    @entity.setter
    def entity(self, arg0: SolveSpace::IdList<SolveSpace::EntityBase, SolveSpace::hEntity>) -> None:
        pass
    @property
    def group(self) -> SolveSpace::IdList<SolveSpace::GroupBase, SolveSpace::hGroup>:
        """
        :type: SolveSpace::IdList<SolveSpace::GroupBase, SolveSpace::hGroup>
        """
    @group.setter
    def group(self, arg0: SolveSpace::IdList<SolveSpace::GroupBase, SolveSpace::hGroup>) -> None:
        pass
    @property
    def param(self) -> SolveSpace::IdList<SolveSpace::Param, SolveSpace::hParam>:
        """
        :type: SolveSpace::IdList<SolveSpace::Param, SolveSpace::hParam>
        """
    @param.setter
    def param(self, arg0: SolveSpace::IdList<SolveSpace::Param, SolveSpace::hParam>) -> None:
        pass
    pass
class SolveResult():
    """
    Members:

      OKAY

      DIDNT_CONVERGE

      REDUNDANT_OKAY

      REDUNDANT_DIDNT_CONVERGE

      TOO_MANY_UNKNOWNS
    """
    def __eq__(self, other: object) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: int) -> None: ...
    @property
    def name(self) -> str:
        """
        :type: str
        """
    @property
    def value(self) -> int:
        """
        :type: int
        """
    DIDNT_CONVERGE: solvespace.slvs.SolveResult # value = <SolveResult.DIDNT_CONVERGE: 10>
    OKAY: solvespace.slvs.SolveResult # value = <SolveResult.OKAY: 0>
    REDUNDANT_DIDNT_CONVERGE: solvespace.slvs.SolveResult # value = <SolveResult.REDUNDANT_DIDNT_CONVERGE: 12>
    REDUNDANT_OKAY: solvespace.slvs.SolveResult # value = <SolveResult.REDUNDANT_OKAY: 11>
    TOO_MANY_UNKNOWNS: solvespace.slvs.SolveResult # value = <SolveResult.TOO_MANY_UNKNOWNS: 20>
    __members__: dict # value = {'OKAY': <SolveResult.OKAY: 0>, 'DIDNT_CONVERGE': <SolveResult.DIDNT_CONVERGE: 10>, 'REDUNDANT_OKAY': <SolveResult.REDUNDANT_OKAY: 11>, 'REDUNDANT_DIDNT_CONVERGE': <SolveResult.REDUNDANT_DIDNT_CONVERGE: 12>, 'TOO_MANY_UNKNOWNS': <SolveResult.TOO_MANY_UNKNOWNS: 20>}
    pass
class SolverResult():
    @property
    def bad(self) -> typing.List[hConstraint]:
        """
        :type: typing.List[hConstraint]
        """
    @property
    def dof(self) -> int:
        """
        :type: int
        """
    @property
    def rank(self) -> int:
        """
        :type: int
        """
    @property
    def status(self) -> SolveResult:
        """
        :type: SolveResult
        """
    pass
class SolverSystem():
    def __init__(self) -> None: ...
    def solve(self, arg0: Group) -> SolverResult: ...
    pass
class Vector():
    @staticmethod
    def at_intersection_of_lines(arg0: Vector, arg1: Vector, arg2: Vector, arg3: Vector, arg4: bool, arg5: float, arg6: float) -> Vector: ...
    @staticmethod
    def at_intersection_of_plane_and_line(arg0: Vector, arg1: float, arg2: Vector, arg3: Vector, arg4: bool) -> Vector: ...
    @staticmethod
    @typing.overload
    def at_intersection_of_planes(arg0: Vector, arg1: float, arg2: Vector, arg3: float) -> Vector: ...
    @staticmethod
    @typing.overload
    def at_intersection_of_planes(arg0: Vector, arg1: float, arg2: Vector, arg3: float, arg4: Vector, arg5: float, arg6: bool) -> Vector: ...
    @staticmethod
    def bounding_box_intersects_line(arg0: Vector, arg1: Vector, arg2: Vector, arg3: Vector, arg4: bool) -> bool: ...
    @staticmethod
    def bounding_boxes_disjoint(arg0: Vector, arg1: Vector, arg2: Vector, arg3: Vector) -> bool: ...
    def clamp_within(self, arg0: float, arg1: float) -> Vector: ...
    def closest_ortho(self) -> Vector: ...
    @staticmethod
    def closest_point_between_lines(arg0: Vector, arg1: Vector, arg2: Vector, arg3: Vector, arg4: float, arg5: float) -> None: ...
    def closest_point_on_line(self, arg0: Vector, arg1: Vector) -> Vector: ...
    def cross(self, arg0: Vector) -> Vector: ...
    def direction_cosine_with(self, arg0: Vector) -> float: ...
    def distance_to_line(self, arg0: Vector, arg1: Vector) -> float: ...
    def distance_to_plane(self, arg0: Vector, arg1: Vector) -> float: ...
    def div_projected(self, arg0: Vector) -> float: ...
    def dot(self, arg0: Vector) -> float: ...
    def dot_in_to_csys(self, arg0: Vector, arg1: Vector, arg2: Vector) -> Vector: ...
    def element(self, arg0: int) -> float: ...
    def equals(self, arg0: Vector, arg1: float) -> bool: ...
    def equals_exactly(self, arg0: Vector) -> bool: ...
    def in_perspective(self, arg0: Vector, arg1: Vector, arg2: Vector, arg3: Vector, arg4: float) -> Vector: ...
    def mag_squared(self) -> float: ...
    def magnitude(self) -> float: ...
    @staticmethod
    def make(arg0: float, arg1: float, arg2: float) -> Vector: ...
    def make_max_min(self, arg0: Vector, arg1: Vector) -> None: ...
    def minus(self, arg0: Vector) -> Vector: ...
    def normal(self, arg0: int) -> Vector: ...
    def on_line_segment(self, arg0: Vector, arg1: Vector, arg2: float) -> bool: ...
    def outside_and_not_on(self, arg0: Vector, arg1: Vector) -> bool: ...
    def plus(self, arg0: Vector) -> Vector: ...
    def scale_out_of_csys(self, arg0: Vector, arg1: Vector, arg2: Vector) -> Vector: ...
    def scaled_by(self, arg0: float) -> Vector: ...
    def with_magnitude(self, arg0: float) -> Vector: ...
    @property
    def x(self) -> float:
        """
        :type: float
        """
    @x.setter
    def x(self, arg0: float) -> None:
        pass
    @property
    def y(self) -> float:
        """
        :type: float
        """
    @y.setter
    def y(self, arg0: float) -> None:
        pass
    @property
    def z(self) -> float:
        """
        :type: float
        """
    @z.setter
    def z(self, arg0: float) -> None:
        pass
    pass
class hConstraint():
    def equation(self, arg0: int) -> hEquation: ...
    def param(self, arg0: int) -> hParam: ...
    @property
    def v(self) -> int:
        """
        :type: int
        """
    @v.setter
    def v(self, arg0: int) -> None:
        pass
    pass
class hEntity():
    def equation(self, arg0: int) -> hEquation: ...
    def group(self) -> hGroup: ...
    @property
    def v(self) -> int:
        """
        :type: int
        """
    @v.setter
    def v(self, arg0: int) -> None:
        pass
    pass
class hEquation():
    def constraint(self) -> hConstraint: ...
    def is_from_constraint(self) -> bool: ...
    @property
    def v(self) -> int:
        """
        :type: int
        """
    @v.setter
    def v(self, arg0: int) -> None:
        pass
    pass
class hGroup():
    def entity(self, arg0: int) -> hEntity: ...
    def equation(self, arg0: int) -> hEquation: ...
    def param(self, arg0: int) -> hParam: ...
    @property
    def v(self) -> int:
        """
        :type: int
        """
    @v.setter
    def v(self, arg0: int) -> None:
        pass
    pass
class hParam():
    def __repr__(self) -> str: ...
    @property
    def v(self) -> int:
        """
        :type: int
        """
    @v.setter
    def v(self, arg0: int) -> None:
        pass
    pass
SK: solvespace.slvs.Sketch
