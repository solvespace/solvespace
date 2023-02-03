from enum import IntEnum, auto

cdef extern from "slvs.h":
    ctypedef int Slvs_hEntity
    ctypedef int Slvs_hGroup
    ctypedef int Slvs_hConstraint
    ctypedef int Slvs_hParam
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

    Slvs_hEntity Slvs_AddPoint2D(Slvs_hGroup grouph, double u, double v, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddPoint3D(Slvs_hGroup grouph, double x, double y, double z)
    Slvs_hEntity Slvs_AddNormal2D(Slvs_hGroup grouph, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddNormal3D(Slvs_hGroup grouph, double qw, double qx, double qy, double qz)
    Slvs_hEntity Slvs_AddDistance(Slvs_hGroup grouph, double value, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddLine2D(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddLine3D(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh)
    Slvs_hEntity Slvs_AddCubic(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, Slvs_hEntity ptCh, Slvs_hEntity ptDh, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddArc(Slvs_hGroup grouph, Slvs_hEntity normalh, Slvs_hEntity centerh, Slvs_hEntity starth, Slvs_hEntity endh, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddCircle(Slvs_hGroup grouph, Slvs_hEntity normalh, Slvs_hEntity centerh, Slvs_hEntity radiush, Slvs_hEntity workplaneh)
    Slvs_hEntity Slvs_AddWorkplane(Slvs_hGroup grouph, Slvs_hEntity originh, Slvs_hEntity nmh)
    Slvs_hEntity Slvs_Add2DBase(Slvs_hGroup grouph)

    Slvs_hConstraint Slvs_AddConstraint(Slvs_hGroup grouph, int type, Slvs_hEntity workplaneh, double val, Slvs_hEntity ptAh,
        Slvs_hEntity ptBh, Slvs_hEntity entityAh,
        Slvs_hEntity entityBh, Slvs_hEntity entityCh,
        Slvs_hEntity entityDh, int other, int other2)
    Slvs_hConstraint Slvs_Coincident(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Distance(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Equal(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_EqualAngle(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, Slvs_hEntity entityCh,
                                        Slvs_hEntity entityDh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_EqualPointToLine(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity entityCh, Slvs_hEntity entityDh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Ratio(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Symmetric(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity entityCh  ,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_SymmetricH(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_SymmetricV(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Midpoint(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Horizontal(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity workplaneh,
                                        Slvs_hEntity entityBh)
    Slvs_hConstraint Slvs_Vertical(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity workplaneh,
                                        Slvs_hEntity entityBh)
    Slvs_hConstraint Slvs_Diameter(Slvs_hGroup grouph, Slvs_hEntity entityAh, double value)
    Slvs_hConstraint Slvs_SameOrientation(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh)
    Slvs_hConstraint Slvs_Angle(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                        Slvs_hEntity workplaneh,
                                        int inverse)
    Slvs_hConstraint Slvs_Perpendicular(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity workplaneh,
                                        int inverse)
    Slvs_hConstraint Slvs_Parallel(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Tangent(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_DistanceProj(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, double value)
    Slvs_hConstraint Slvs_LengthDiff(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                        Slvs_hEntity workplaneh)
    Slvs_hConstraint Slvs_Dragged(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity workplaneh)

    Slvs_SolveResult Slvs_SolveSketch(Slvs_hGroup hg, int calculateFaileds)
    double Slvs_GetParamValue(Slvs_hEntity e, int i)
    Slvs_hEntity Slvs_GetPoint(Slvs_hParam e, int i)
    void Slvs_ClearSketch()

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
def add_point_2d(grouph: Slvs_hGroup, u: double, v: double, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddPoint2D(grouph, u, v, workplaneh)

def add_point_3d(grouph: Slvs_hGroup, x: double, y: double, z: double) -> Slvs_hEntity:
    return Slvs_AddPoint3D(grouph, x, y, z)

def add_normal_2d(grouph: Slvs_hGroup, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddNormal2D(grouph, workplaneh)

def add_normal_3d(grouph: Slvs_hGroup, qw: double, qx: double, qy: double, qz: double) -> Slvs_hEntity:
    return Slvs_AddNormal3D(grouph, qw, qx, qy, qz)

def add_distance(grouph: Slvs_hGroup, value: double, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddDistance(grouph, value, workplaneh)

def add_line_2d(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddLine2D(grouph, ptAh, ptBh, workplaneh)

def add_line_3d(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddLine3D(grouph, ptAh, ptBh)

def add_cubic(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, ptCh: Slvs_hEntity, ptDh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddCubic(grouph, ptAh, ptBh, ptCh, ptDh, workplaneh)

def add_arc(grouph: Slvs_hGroup, normalh: Slvs_hEntity, centerh: Slvs_hEntity, starth: Slvs_hEntity, endh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddArc(grouph, normalh, centerh, starth, endh, workplaneh)

def add_circle(grouph: Slvs_hGroup, normalh: Slvs_hEntity, centerh: Slvs_hEntity, radiush: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddCircle(grouph, normalh, centerh, radiush, workplaneh)

def add_workplane(grouph: Slvs_hGroup, originh: Slvs_hEntity, nmh: Slvs_hEntity) -> Slvs_hEntity:
    return Slvs_AddWorkplane(grouph, originh, nmh)

def add_2d_base(grouph: Slvs_hGroup) -> Slvs_hEntity:
    return Slvs_Add2DBase(grouph)

# constraints
def add_constraint(grouph: Slvs_hGroup, c_type: int, workplaneh: Slvs_hEntity, val: double, ptAh: Slvs_hEntity,
        ptBh: Slvs_hEntity, entityAh: Slvs_hEntity,
        entityBh: Slvs_hEntity, entityCh: Slvs_hEntity,
        entityDh: Slvs_hEntity, other: int, other2: int) -> Slvs_hConstraint:
    return Slvs_AddConstraint(grouph, c_type, workplaneh, val, ptAh,
        ptBh, entityAh,
        entityBh, entityCh,
        entityDh, other, other2)

def coincident(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Coincident(grouph, entityAh, entityBh, workplaneh)

def distance(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, value: double, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Distance(grouph, entityAh, entityBh, value, workplaneh)

def equal(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Equal(grouph, entityAh, entityBh, workplaneh)

def equal_angle(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, entityCh: Slvs_hEntity,
                                        entityDh: Slvs_hEntity,
                                        workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_EqualAngle(grouph, entityAh, entityBh, entityCh, entityDh, workplaneh)

def equal_point_to_line(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity,
                                        entityCh: Slvs_hEntity, entityDh: Slvs_hEntity,
                                        workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_EqualPointToLine(grouph, entityAh, entityBh, entityCh, entityDh, workplaneh)

def ratio(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, value: double, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Ratio(grouph, entityAh, entityBh, value, workplaneh)

def symmetric(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, entityCh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Symmetric(grouph, entityAh, entityBh, entityCh, workplaneh)

def symmetric_h(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_SymmetricH(grouph, ptAh, ptBh, workplaneh)

def symmetric_v(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_SymmetricV(grouph, ptAh, ptBh, workplaneh)

def midpoint(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Midpoint(grouph, ptAh, ptBh, workplaneh)

def horizontal(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, workplaneh: Slvs_hEntity, entityBh: Slvs_hEntity = 0) -> Slvs_hConstraint:
    return Slvs_Horizontal(grouph, entityAh, workplaneh, entityBh)

def vertical(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, workplaneh: Slvs_hEntity, entityBh: Slvs_hEntity = 0) -> Slvs_hConstraint:
    return Slvs_Vertical(grouph, entityAh, workplaneh, entityBh)

def diameter(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, value: double) -> Slvs_hConstraint:
    return Slvs_Diameter(grouph, entityAh, value)

def same_orientation(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_SameOrientation(grouph, entityAh, entityBh)

def angle(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, value: double, workplaneh: Slvs_hEntity, inverse: int) -> Slvs_hConstraint:
    return Slvs_Angle(grouph, entityAh, entityBh, value, workplaneh, inverse)

def perpendicular(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, workplaneh: Slvs_hEntity, inverse: int) -> Slvs_hConstraint:
    return Slvs_Perpendicular(grouph, entityAh, entityBh, workplaneh, inverse)

def parallel(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Parallel(grouph, entityAh, entityBh, workplaneh)

def tangent(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Tangent(grouph, entityAh, entityBh, workplaneh)

def distance_proj(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, ptBh: Slvs_hEntity, value: double) -> Slvs_hConstraint:
    return Slvs_DistanceProj(grouph, ptAh, ptBh, value)

def length_diff(grouph: Slvs_hGroup, entityAh: Slvs_hEntity, entityBh: Slvs_hEntity, value: double, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_LengthDiff(grouph, entityAh, entityBh, value, workplaneh)

def dragged(grouph: Slvs_hGroup, ptAh: Slvs_hEntity, workplaneh: Slvs_hEntity) -> Slvs_hConstraint:
    return Slvs_Dragged(grouph, ptAh, workplaneh)

# solver
class ResultFlag(IntEnum):
    """Symbol of the result flags."""
    OKAY = 0
    INCONSISTENT = auto()
    DIDNT_CONVERGE = auto()
    TOO_MANY_UNKNOWNS = auto()

def solve_sketch(grouph: Slvs_hGroup, calculateFaileds: int):
    return Slvs_SolveSketch(grouph, calculateFaileds)

def get_param(e: Slvs_hEntity, index: int):
    return Slvs_GetParamValue(e, index)

def get_point(p: Slvs_hParam, index: int):
    return Slvs_GetPoint(p, index)

def clear():
    Slvs_ClearSketch()