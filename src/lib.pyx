#cython: language_level=3
from enum import IntEnum, auto

cdef extern from "slvs.h" nogil:
    ctypedef int Slvs_hEntity
    ctypedef int Slvs_hGroup
    ctypedef int Slvs_hConstraint
    ctypedef int Slvs_hParam

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
        int rank
        int bad

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

    Slvs_SolveResult Slvs_SolveSketch(Slvs_hGroup hg, int calculateFaileds) nogil
    double Slvs_GetParamValue(Slvs_Entity e, int i)
    void Slvs_ClearSketch()

    cdef Slvs_Entity _E_NONE "SLVS_E_NONE"
    cdef Slvs_Entity _E_FREE_IN_3D "SLVS_E_FREE_IN_3D"

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
def add_point_2d(grouph: int, u: double, v: double, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddPoint2D(grouph, u, v, workplane)

def add_point_3d(grouph: int, x: double, y: double, z: double) -> Slvs_Entity:
    return Slvs_AddPoint3D(grouph, x, y, z)

def add_normal_2d(grouph: int, workplane: Slvs_Entity) -> Slvs_Entity:
    return Slvs_AddNormal2D(grouph, workplane)

def add_normal_3d(grouph: int, qw: double, qx: double, qy: double, qz: double) -> Slvs_Entity:
    return Slvs_AddNormal3D(grouph, qw, qx, qy, qz)

def add_distance(grouph: int, value: double, workplane: Slvs_Entity) -> Slvs_Entity:
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
def add_constraint(grouph: int, c_type: int, workplane: Slvs_Entity, val: double, ptA: Slvs_Entity = E_NONE,
        ptB: Slvs_Entity = E_NONE, entityA: Slvs_Entity = E_NONE,
        entityB: Slvs_Entity = E_NONE, entityC: Slvs_Entity = E_NONE,
        entityD: Slvs_Entity = E_NONE, other: int = 0, other2: int = 0) -> Slvs_Constraint:
    return Slvs_AddConstraint(grouph, c_type, workplane, val, ptA, ptB, entityA, entityB, entityC, entityD, other, other2)

def coincident(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Coincident(grouph, entityA, entityB, workplane)

def distance(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: double, workplane: Slvs_Entity) -> Slvs_Constraint:
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

def ratio(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: double, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
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

def diameter(grouph: int, entityA: Slvs_Entity, value: double) -> Slvs_Constraint:
    return Slvs_Diameter(grouph, entityA, value)

def same_orientation(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity) -> Slvs_Constraint:
    return Slvs_SameOrientation(grouph, entityA, entityB)

def angle(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: double, workplane: Slvs_Entity = E_FREE_IN_3D, inverse: int = 0) -> Slvs_Constraint:
    return Slvs_Angle(grouph, entityA, entityB, value, workplane, inverse)

def perpendicular(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D, inverse: int = 0) -> Slvs_Constraint:
    return Slvs_Perpendicular(grouph, entityA, entityB, workplane, inverse)

def parallel(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Parallel(grouph, entityA, entityB, workplane)

def tangent(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Tangent(grouph, entityA, entityB, workplane)

def distance_proj(grouph: int, ptA: Slvs_Entity, ptB: Slvs_Entity, value: double) -> Slvs_Constraint:
    return Slvs_DistanceProj(grouph, ptA, ptB, value)

def length_diff(grouph: int, entityA: Slvs_Entity, entityB: Slvs_Entity, value: double, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_LengthDiff(grouph, entityA, entityB, value, workplane)

def dragged(grouph: int, ptA: Slvs_Entity, workplane: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    return Slvs_Dragged(grouph, ptA, workplane)

# solver
class ResultFlag(IntEnum):
    """Symbol of the result flags."""
    OKAY = 0
    INCONSISTENT = auto()
    DIDNT_CONVERGE = auto()
    TOO_MANY_UNKNOWNS = auto()

class ConstraintType(IntEnum):
    """Symbol of the constraint types."""
    POINTS_COINCIDENT = 100000
    PT_PT_DISTANCE = 100001
    PT_PLANE_DISTANCE = 100002
    PT_LINE_DISTANCE = 100003
    PT_FACE_DISTANCE = 100004
    PT_IN_PLANE = 100005
    PT_ON_LINE = 100006
    PT_ON_FACE = 100007
    EQUAL_LENGTH_LINES = 100008
    LENGTH_RATIO = 100009
    EQ_LEN_PT_LINE_D = 100010
    EQ_PT_LN_DISTANCES = 100011
    EQUAL_ANGLE = 100012
    EQUAL_LINE_ARC_LEN = 100013
    SYMMETRIC = 100014
    SYMMETRIC_HORIZ = 100015
    SYMMETRIC_VERT = 100016
    SYMMETRIC_LINE = 100017
    AT_MIDPOINT = 100018
    HORIZONTAL = 100019
    VERTICAL = 100020
    DIAMETER = 100021
    PT_ON_CIRCLE = 100022
    SAME_ORIENTATION = 100023
    ANGLE = 100024
    PARALLEL = 100025
    PERPENDICULAR = 100026
    ARC_LINE_TANGENT = 100027
    CUBIC_LINE_TANGENT = 100028
    EQUAL_RADIUS = 100029
    PROJ_PT_DISTANCE = 100030
    WHERE_DRAGGED = 100031
    CURVE_CURVE_TANGENT = 100032
    LENGTH_DIFFERENCE = 100033
    ARC_ARC_LEN_RATIO = 100034
    ARC_LINE_LEN_RATIO = 100035
    ARC_ARC_DIFFERENCE = 100036
    ARC_LINE_DIFFERENCE = 100037

class EntityType(IntEnum):
    POINT_IN_3D = 50000
    POINT_IN_2D = 50001
    NORMAL_IN_3D = 60000
    NORMAL_IN_2D = 60001
    DISTANCE = 70000
    WORKPLANE = 80000
    LINE_SEGMENT = 80001
    CUBIC = 80002
    CIRCLE = 80003
    ARC_OF_CIRCLE = 80004

def solve_sketch(grouph: int, calculateFaileds: int):
    return Slvs_SolveSketch(grouph, calculateFaileds)

def get_param(e: Slvs_Entity, index: int):
    return Slvs_GetParamValue(e, index)

def clear():
    Slvs_ClearSketch()
