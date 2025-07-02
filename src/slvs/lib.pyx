#cython: language_level=3
from enum import IntEnum, auto
from libc.stdint cimport uint32_t
from libc.stdlib cimport free

cdef extern from "slvs.h" nogil:
    ctypedef uint32_t Slvs_hEntity
    ctypedef uint32_t Slvs_hGroup
    ctypedef uint32_t Slvs_hConstraint
    ctypedef uint32_t Slvs_hParam

    ctypedef struct Slvs_Entity:
        Slvs_hEntity h
        Slvs_hGroup group
        int type
        Slvs_hEntity wrkpl
        Slvs_hEntity point[4]
        Slvs_hEntity normal
        Slvs_hEntity distance
        Slvs_hParam param[4]

    ctypedef struct Slvs_Constraint:
        Slvs_hConstraint h
        Slvs_hGroup group
        int type
        Slvs_hEntity wrkpl
        double valA
        Slvs_hEntity ptA
        Slvs_hEntity ptB
        Slvs_hEntity entityA
        Slvs_hEntity entityB
        Slvs_hEntity entityC
        Slvs_hEntity entityD
        int other
        int other2

    ctypedef struct Slvs_SolveResult:
        int result
        int dof
        int nbad

    void Slvs_QuaternionU(double qw, double qx, double qy, double qz,
                             double *x, double *y, double *z)
    void Slvs_QuaternionV(double qw, double qx, double qy, double qz,
                             double *x, double *y, double *z)
    void Slvs_QuaternionN(double qw, double qx, double qy, double qz,
                             double *x, double *y, double *z)

    void Slvs_MakeQuaternion(double ux, double uy, double uz,
                             double vx, double vy, double vz,
                             double *qw, double *qx, double *qy, double *qz)

    Slvs_Entity Slvs_AddPoint2D(Slvs_hGroup grouph, double u, double v, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddPoint3D(Slvs_hGroup grouph, double x, double y, double z)
    Slvs_Entity Slvs_AddNormal2D(Slvs_hGroup grouph, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddNormal3D(Slvs_hGroup grouph, double qw, double qx, double qy, double qz)
    Slvs_Entity Slvs_AddDistance(Slvs_hGroup grouph, double value, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddLine2D(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddLine3D(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB)
    Slvs_Entity Slvs_AddCubic(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity ptC, Slvs_Entity ptD, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddArc(Slvs_hGroup grouph, Slvs_Entity normal, Slvs_Entity center, Slvs_Entity start, Slvs_Entity end, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddCircle(Slvs_hGroup grouph, Slvs_Entity normal, Slvs_Entity center, Slvs_Entity radius, Slvs_Entity workplane)
    Slvs_Entity Slvs_AddWorkplane(Slvs_hGroup grouph, Slvs_Entity origin, Slvs_Entity nm)
    Slvs_Entity Slvs_AddBase2D(Slvs_hGroup grouph)

    Slvs_Constraint Slvs_AddConstraint(Slvs_hGroup grouph, int type, Slvs_Entity workplane, double val, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity entityC, Slvs_Entity entityD, int other, int other2)
    Slvs_Constraint Slvs_Coincident(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Distance(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, double value, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Equal(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_EqualAngle(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity entityC, Slvs_Entity entityD, Slvs_Entity workplane)
    Slvs_Constraint Slvs_EqualPointToLine(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity entityC, Slvs_Entity entityD, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Ratio(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, double value, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Symmetric(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity entityC, Slvs_Entity workplane)
    Slvs_Constraint Slvs_SymmetricH(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_SymmetricV(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Midpoint(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Horizontal(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity workplane, Slvs_Entity entityB)
    Slvs_Constraint Slvs_Vertical(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity workplane, Slvs_Entity entityB)
    Slvs_Constraint Slvs_Diameter(Slvs_hGroup grouph, Slvs_Entity entityA, double value)
    Slvs_Constraint Slvs_SameOrientation(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB)
    Slvs_Constraint Slvs_Angle(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, double value, Slvs_Entity workplane, int inverse)
    Slvs_Constraint Slvs_Perpendicular(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity workplane, int inverse)
    Slvs_Constraint Slvs_Parallel(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Tangent(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, Slvs_Entity workplane)
    Slvs_Constraint Slvs_DistanceProj(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity ptB, double value)
    Slvs_Constraint Slvs_LengthDiff(Slvs_hGroup grouph, Slvs_Entity entityA, Slvs_Entity entityB, double value, Slvs_Entity workplane)
    Slvs_Constraint Slvs_Dragged(Slvs_hGroup grouph, Slvs_Entity ptA, Slvs_Entity workplane)

    void Slvs_MarkDragged(Slvs_Entity ptA)
    Slvs_SolveResult Slvs_SolveSketch(Slvs_hGroup hg, Slvs_hConstraint **bad) nogil
    double Slvs_GetParamValue(int ph)
    double Slvs_SetParamValue(int ph, double value)
    void Slvs_ClearSketch()

    cdef Slvs_Entity _E_NONE "SLVS_E_NONE"
    cdef Slvs_Entity _E_FREE_IN_3D "SLVS_E_FREE_IN_3D"

    cdef int _SLVS_C_POINTS_COINCIDENT "SLVS_C_POINTS_COINCIDENT"
    cdef int _SLVS_C_PT_PT_DISTANCE "SLVS_C_PT_PT_DISTANCE"
    cdef int _SLVS_C_PT_PLANE_DISTANCE "SLVS_C_PT_PLANE_DISTANCE"
    cdef int _SLVS_C_PT_LINE_DISTANCE "SLVS_C_PT_LINE_DISTANCE"
    cdef int _SLVS_C_PT_FACE_DISTANCE "SLVS_C_PT_FACE_DISTANCE"
    cdef int _SLVS_C_PT_IN_PLANE "SLVS_C_PT_IN_PLANE"
    cdef int _SLVS_C_PT_ON_LINE "SLVS_C_PT_ON_LINE"
    cdef int _SLVS_C_PT_ON_FACE "SLVS_C_PT_ON_FACE"
    cdef int _SLVS_C_EQUAL_LENGTH_LINES "SLVS_C_EQUAL_LENGTH_LINES"
    cdef int _SLVS_C_LENGTH_RATIO "SLVS_C_LENGTH_RATIO"
    cdef int _SLVS_C_EQ_LEN_PT_LINE_D "SLVS_C_EQ_LEN_PT_LINE_D"
    cdef int _SLVS_C_EQ_PT_LN_DISTANCES "SLVS_C_EQ_PT_LN_DISTANCES"
    cdef int _SLVS_C_EQUAL_ANGLE "SLVS_C_EQUAL_ANGLE"
    cdef int _SLVS_C_EQUAL_LINE_ARC_LEN "SLVS_C_EQUAL_LINE_ARC_LEN"
    cdef int _SLVS_C_SYMMETRIC "SLVS_C_SYMMETRIC"
    cdef int _SLVS_C_SYMMETRIC_HORIZ "SLVS_C_SYMMETRIC_HORIZ"
    cdef int _SLVS_C_SYMMETRIC_VERT "SLVS_C_SYMMETRIC_VERT"
    cdef int _SLVS_C_SYMMETRIC_LINE "SLVS_C_SYMMETRIC_LINE"
    cdef int _SLVS_C_AT_MIDPOINT "SLVS_C_AT_MIDPOINT"
    cdef int _SLVS_C_HORIZONTAL "SLVS_C_HORIZONTAL"
    cdef int _SLVS_C_VERTICAL "SLVS_C_VERTICAL"
    cdef int _SLVS_C_DIAMETER "SLVS_C_DIAMETER"
    cdef int _SLVS_C_PT_ON_CIRCLE "SLVS_C_PT_ON_CIRCLE"
    cdef int _SLVS_C_SAME_ORIENTATION "SLVS_C_SAME_ORIENTATION"
    cdef int _SLVS_C_ANGLE "SLVS_C_ANGLE"
    cdef int _SLVS_C_PARALLEL "SLVS_C_PARALLEL"
    cdef int _SLVS_C_PERPENDICULAR "SLVS_C_PERPENDICULAR"
    cdef int _SLVS_C_ARC_LINE_TANGENT "SLVS_C_ARC_LINE_TANGENT"
    cdef int _SLVS_C_CUBIC_LINE_TANGENT "SLVS_C_CUBIC_LINE_TANGENT"
    cdef int _SLVS_C_EQUAL_RADIUS "SLVS_C_EQUAL_RADIUS"
    cdef int _SLVS_C_PROJ_PT_DISTANCE "SLVS_C_PROJ_PT_DISTANCE"
    cdef int _SLVS_C_WHERE_DRAGGED "SLVS_C_WHERE_DRAGGED"
    cdef int _SLVS_C_CURVE_CURVE_TANGENT "SLVS_C_CURVE_CURVE_TANGENT"
    cdef int _SLVS_C_LENGTH_DIFFERENCE "SLVS_C_LENGTH_DIFFERENCE"
    cdef int _SLVS_C_ARC_ARC_LEN_RATIO "SLVS_C_ARC_ARC_LEN_RATIO"
    cdef int _SLVS_C_ARC_LINE_LEN_RATIO "SLVS_C_ARC_LINE_LEN_RATIO"
    cdef int _SLVS_C_ARC_ARC_DIFFERENCE "SLVS_C_ARC_ARC_DIFFERENCE"
    cdef int _SLVS_C_ARC_LINE_DIFFERENCE "SLVS_C_ARC_LINE_DIFFERENCE"

    cdef int _SLVS_E_POINT_IN_3D "SLVS_E_POINT_IN_3D"
    cdef int _SLVS_E_POINT_IN_2D "SLVS_E_POINT_IN_2D"
    cdef int _SLVS_E_NORMAL_IN_3D "SLVS_E_NORMAL_IN_3D"
    cdef int _SLVS_E_NORMAL_IN_2D "SLVS_E_NORMAL_IN_2D"
    cdef int _SLVS_E_DISTANCE "SLVS_E_DISTANCE"
    cdef int _SLVS_E_WORKPLANE "SLVS_E_WORKPLANE"
    cdef int _SLVS_E_LINE_SEGMENT "SLVS_E_LINE_SEGMENT"
    cdef int _SLVS_E_CUBIC "SLVS_E_CUBIC"
    cdef int _SLVS_E_CIRCLE "SLVS_E_CIRCLE"
    cdef int _SLVS_E_ARC_OF_CIRCLE "SLVS_E_ARC_OF_CIRCLE"

    cdef int _SLVS_RESULT_OKAY "SLVS_RESULT_OKAY"
    cdef int _SLVS_RESULT_INCONSISTENT "SLVS_RESULT_INCONSISTENT"
    cdef int _SLVS_RESULT_DIDNT_CONVERGE "SLVS_RESULT_DIDNT_CONVERGE"
    cdef int _SLVS_RESULT_TOO_MANY_UNKNOWNS "SLVS_RESULT_TOO_MANY_UNKNOWNS"
    cdef int _SLVS_RESULT_REDUNDANT_OKAY "SLVS_RESULT_REDUNDANT_OKAY"

E_NONE = _E_NONE
E_FREE_IN_3D = _E_FREE_IN_3D

# quaternion
cpdef tuple quaternion_u(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of U axis.
    Where `qw`, `qx`, `qy`, `qz` are corresponded to the W, X, Y, Z value of
    quaternion.
    """
    cdef double x, y, z
    Slvs_QuaternionU(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple quaternion_v(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of V axis.
    Signature is same as [quaternion_u](#quaternion_u).
    """
    cdef double x, y, z
    Slvs_QuaternionV(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple quaternion_n(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of normal.
    Signature is same as [quaternion_u](#quaternion_u).
    """
    cdef double x, y, z
    Slvs_QuaternionN(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple make_quaternion(double ux, double uy, double uz, double vx, double vy, double vz):
    """Input two unit vector, return quaternion.
    Where `ux`, `uy`, `uz` are corresponded to the value of U vector;
    `vx`, `vy`, `vz` are corresponded to the value of V vector.
    """
    cdef double qw, qx, qy, qz
    Slvs_MakeQuaternion(ux, uy, uz, vx, vy, vz, &qw, &qx, &qy, &qz)
    return qw, qx, qy, qz

# entities
def add_point_2d(grouph: int, u: float, v: float, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddPoint2D(grouph, u, v, workplane)

def add_point_3d(grouph: int, x: float, y: float, z: float) -> Slvs_Entity:
    return Slvs_AddPoint3D(grouph, x, y, z)

def add_normal_2d(grouph: int, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddNormal2D(grouph, workplane)

def add_normal_3d(grouph: int, qw: float, qx: float, qy: float, qz: float) -> Slvs_Entity:
    return Slvs_AddNormal3D(grouph, qw, qx, qy, qz)

def add_distance(grouph: int, value: float, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddDistance(grouph, value, workplane)

def add_line_2d(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddLine2D(grouph, ptA, ptB, workplane)

def add_line_3d(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddLine3D(grouph, ptA, ptB)

def add_cubic(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, ptC: Slvs_Entity, ptD: Slvs_Entity, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddCubic(grouph, ptA, ptB, ptC, ptD, workplane)

def add_arc(grouph: int, normal: Slvs_Entity, center: Slvs_Entity, start: Slvs_Entity, end: Slvs_Entity, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddArc(grouph, normal, center, start, end, workplane)

def add_circle(grouph: int, normal: Slvs_Entity, center: Slvs_Entity, radius: Slvs_Entity, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddCircle(grouph, normal, center, radius, workplane)

def add_workplane(grouph: int, origin: Slvs_Entity, nm: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddWorkplane(grouph, origin, nm)

def add_base_2d(grouph: int) -> Slvs_Entity:
    return Slvs_AddBase2D(grouph)

# constraints
def add_constraint(grouph: int, c_type: int, workplane: Slvs_Entity, val: float, ptA: Slvs_Entity = E_NONE,
        ptB: Slvs_Entity = E_NONE, entityA: Slvs_Entity = E_NONE,
        entityB: Slvs_Entity = E_NONE, entityC: Slvs_Entity = E_NONE,
        entityD: Slvs_Entity = E_NONE, other: int = 0, other2: int = 0) -> Slvs_Constraint:
    return Slvs_AddConstraint(grouph, c_type, workplane, val, ptA, ptB, entityA, entityB, entityC, entityD, other, other2)

def coincident(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Coincident(grouph, entityA, entityB, workplane)

def distance(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: float, workplane: Slvs_Entity) -> Slvs_Constraint:
    return Slvs_Distance(grouph, entityA, entityB, value, workplane)

def equal(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Equal(grouph, entityA, entityB, workplane)

def equal_angle(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, entityC: Slvs_Entity,
                                        entityD: Slvs_Entity,
                                        workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_EqualAngle(grouph, entityA, entityB, entityC, entityD, workplane)

def equal_point_to_line(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity,
                                        entityC: Slvs_Entity, entityD: Slvs_Entity,
                                        workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_EqualPointToLine(grouph, entityA, entityB, entityC, entityD, workplane)

def ratio(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: float, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Ratio(grouph, entityA, entityB, value, workplane)

def symmetric(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, entityC: Slvs_Entity = E_NONE, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Symmetric(grouph, entityA, entityB, entityC, workplane)

def symmetric_h(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_SymmetricH(grouph, ptA, ptB, workplane)

def symmetric_v(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_SymmetricV(grouph, ptA, ptB, workplane)

def midpoint(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Midpoint(grouph, ptA, ptB, workplane)

def horizontal(grouph: int, entityA: Slvs_Entity, workplane: Slvs_Entity, entityB: Slvs_Entity = E_NONE) -> Slvs_Constraint:
    return Slvs_Horizontal(grouph, entityA, workplane, entityB)

def vertical(grouph: int, entityA: Slvs_Entity, workplane: Slvs_Entity, entityB: Slvs_Entity = E_NONE) -> Slvs_Constraint:
    return Slvs_Vertical(grouph, entityA, workplane, entityB)

def diameter(grouph: int, entityA: Slvs_Entity, value: float) -> Slvs_Constraint:
    return Slvs_Diameter(grouph, entityA, value)

def same_orientation(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity) -> Slvs_Constraint:
    return Slvs_SameOrientation(grouph, entityA, entityB)

def angle(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: float, workplane: Slvs_Entity = E_FREE_IN_3D, inverse: bool = False) -> Slvs_Constraint:
    return Slvs_Angle(grouph, entityA, entityB, value, workplane, inverse)

def perpendicular(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D, inverse: bool = False) -> Slvs_Constraint:
    return Slvs_Perpendicular(grouph, entityA, entityB, workplane, inverse)

def parallel(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Parallel(grouph, entityA, entityB, workplane)

def tangent(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Tangent(grouph, entityA, entityB, workplane)

def distance_proj(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, value: float) -> Slvs_Constraint:
    return Slvs_DistanceProj(grouph, ptA, ptB, value)

def length_diff(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: float, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_LengthDiff(grouph, entityA, entityB, value, workplane)

def dragged(grouph: int, ptA: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Dragged(grouph, ptA, workplane)

# solver
class ResultFlag(IntEnum):
    """Symbol of the result flags."""
    OKAY = _SLVS_RESULT_OKAY
    INCONSISTENT = _SLVS_RESULT_INCONSISTENT
    DIDNT_CONVERGE = _SLVS_RESULT_DIDNT_CONVERGE
    TOO_MANY_UNKNOWNS = _SLVS_RESULT_TOO_MANY_UNKNOWNS
    REDUNDANT_OKAY = _SLVS_RESULT_REDUNDANT_OKAY

class ConstraintType(IntEnum):
    """Symbol of the constraint types."""
    POINTS_COINCIDENT = _SLVS_C_POINTS_COINCIDENT
    PT_PT_DISTANCE = _SLVS_C_PT_PT_DISTANCE
    PT_PLANE_DISTANCE = _SLVS_C_PT_PLANE_DISTANCE
    PT_LINE_DISTANCE = _SLVS_C_PT_LINE_DISTANCE
    PT_FACE_DISTANCE = _SLVS_C_PT_FACE_DISTANCE
    PT_IN_PLANE = _SLVS_C_PT_IN_PLANE
    PT_ON_LINE = _SLVS_C_PT_ON_LINE
    PT_ON_FACE = _SLVS_C_PT_ON_FACE
    EQUAL_LENGTH_LINES = _SLVS_C_EQUAL_LENGTH_LINES
    LENGTH_RATIO = _SLVS_C_LENGTH_RATIO
    EQ_LEN_PT_LINE_D = _SLVS_C_EQ_LEN_PT_LINE_D
    EQ_PT_LN_DISTANCES = _SLVS_C_EQ_PT_LN_DISTANCES
    EQUAL_ANGLE = _SLVS_C_EQUAL_ANGLE
    EQUAL_LINE_ARC_LEN = _SLVS_C_EQUAL_LINE_ARC_LEN
    SYMMETRIC = _SLVS_C_SYMMETRIC
    SYMMETRIC_HORIZ = _SLVS_C_SYMMETRIC_HORIZ
    SYMMETRIC_VERT = _SLVS_C_SYMMETRIC_VERT
    SYMMETRIC_LINE = _SLVS_C_SYMMETRIC_LINE
    AT_MIDPOINT = _SLVS_C_AT_MIDPOINT
    HORIZONTAL = _SLVS_C_HORIZONTAL
    VERTICAL = _SLVS_C_VERTICAL
    DIAMETER = _SLVS_C_DIAMETER
    PT_ON_CIRCLE = _SLVS_C_PT_ON_CIRCLE
    SAME_ORIENTATION = _SLVS_C_SAME_ORIENTATION
    ANGLE = _SLVS_C_ANGLE
    PARALLEL = _SLVS_C_PARALLEL
    PERPENDICULAR = _SLVS_C_PERPENDICULAR
    ARC_LINE_TANGENT = _SLVS_C_ARC_LINE_TANGENT
    CUBIC_LINE_TANGENT = _SLVS_C_CUBIC_LINE_TANGENT
    EQUAL_RADIUS = _SLVS_C_EQUAL_RADIUS
    PROJ_PT_DISTANCE = _SLVS_C_PROJ_PT_DISTANCE
    WHERE_DRAGGED = _SLVS_C_WHERE_DRAGGED
    CURVE_CURVE_TANGENT = _SLVS_C_CURVE_CURVE_TANGENT
    LENGTH_DIFFERENCE = _SLVS_C_LENGTH_DIFFERENCE
    ARC_ARC_LEN_RATIO = _SLVS_C_ARC_ARC_LEN_RATIO
    ARC_LINE_LEN_RATIO = _SLVS_C_ARC_LINE_LEN_RATIO
    ARC_ARC_DIFFERENCE = _SLVS_C_ARC_ARC_DIFFERENCE
    ARC_LINE_DIFFERENCE = _SLVS_C_ARC_LINE_DIFFERENCE

class EntityType(IntEnum):
    POINT_IN_3D = _SLVS_E_POINT_IN_3D
    POINT_IN_2D = _SLVS_E_POINT_IN_2D
    NORMAL_IN_3D = _SLVS_E_NORMAL_IN_3D
    NORMAL_IN_2D = _SLVS_E_NORMAL_IN_2D
    DISTANCE = _SLVS_E_DISTANCE
    WORKPLANE = _SLVS_E_WORKPLANE
    LINE_SEGMENT = _SLVS_E_LINE_SEGMENT
    CUBIC = _SLVS_E_CUBIC
    CIRCLE = _SLVS_E_CIRCLE
    ARC_OF_CIRCLE = _SLVS_E_ARC_OF_CIRCLE

def mark_dragged(ptA: Slvs_Entity):
    Slvs_MarkDragged(ptA)

def solve_sketch(grouph: int, calculateFaileds: bool):
    cdef Slvs_hConstraint *badp = NULL
    if not calculateFaileds:
        return Slvs_SolveSketch(grouph, NULL)
    else:
        result = Slvs_SolveSketch(grouph, &badp)
        bad = []
        if badp != NULL:
            for i in range(0, result.nbad):
                bad.append(badp[i])
            free(badp)
        return result, bad

def get_param_value(ph: int):
    return Slvs_GetParamValue(ph)

def set_param_value(ph: int, value: float):
    Slvs_SetParamValue(ph, value)

def clear_sketch():
    Slvs_ClearSketch()
