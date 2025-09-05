# -*- coding: utf-8 -*-

from typing import Tuple, TypedDict
from enum import IntEnum, auto

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

class ResultFlag(IntEnum):
    """Symbol of the result flags."""
    OKAY = 0
    INCONSISTENT = auto()
    DIDNT_CONVERGE = auto()
    TOO_MANY_UNKNOWNS = auto()
    REDUNDANT_OKAY = auto()

class Slvs_Entity(TypedDict):
  h: int
  group: int
  type: int
  wrkpl: int
  point: list[int]
  normal: int
  distance: int
  param: list[int]

class Slvs_Constraint(TypedDict):
  h: int
  group: int
  type: int
  wrkpl: int
  valA: float
  ptA: int
  ptB: int
  entityA: int
  entityB: int
  entityC: int
  entityD: int
  other: int
  other2: int

class Slvs_SolveResult(TypedDict):
  result: int
  dof: int
  rank: int
  bad: int

E_FREE_IN_3D : Slvs_Entity
E_NONE : Slvs_Entity

def quaternion_u(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def quaternion_v(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def quaternion_n(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def make_quaternion(
    ux: float,
    uy: float,
    uz: float,
    vx: float,
    vy: float,
    vz: float
) -> Tuple[float, float, float, float]:
    ...

def add_base_2d(grouph: int) -> Slvs_Entity:
    ...

def add_point_2d(grouph: int, u: float, v: float, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_point_3d(grouph: int, x: float, y: float, z: float) -> Slvs_Entity:
    ...

def add_normal_2d(grouph: int, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_normal_3d(grouph: int, qw: float, qx: float, qy: float, qz: float) -> Slvs_Entity:
    ...

def add_distance(grouph: int, d: float, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_line_2d(grouph: int, p1: Slvs_Entity, p2: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_line_3d(grouph: int, p1: Slvs_Entity, p2: Slvs_Entity) -> Slvs_Entity:
    ...

def add_cubic(grouph: int, p1: Slvs_Entity, p2: Slvs_Entity, p3: Slvs_Entity, p4: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_arc(grouph: int, nm: Slvs_Entity, ct: Slvs_Entity, start: Slvs_Entity, end: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_circle(grouph: int, nm: Slvs_Entity, ct: Slvs_Entity, radius: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Entity:
    ...

def add_workplane(grouph: int, origin: Slvs_Entity, nm: Slvs_Entity) -> Slvs_Entity:
    ...

def add_constraint(
    grouph: int,
    c_type: ConstraintType,
    wp: Slvs_Entity,
    v: float,
    p1: Slvs_Entity = E_NONE,
    p2: Slvs_Entity = E_NONE,
    e1: Slvs_Entity = E_NONE,
    e2: Slvs_Entity = E_NONE,
    e3: Slvs_Entity = E_NONE,
    e4: Slvs_Entity = E_NONE,
    other: int = 0,
    other2: int = 0
) -> Slvs_Constraint:
    ...

def coincident(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def distance(
    grouph: int,
    e1: Slvs_Entity,
    e2: Slvs_Entity,
    value: float,
    wp: Slvs_Entity = E_FREE_IN_3D
) -> Slvs_Constraint:
    ...

def equal(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def equal_angle(
    grouph: int,
    e1: Slvs_Entity,
    e2: Slvs_Entity,
    e3: Slvs_Entity,
    e4: Slvs_Entity,
    wp: Slvs_Entity = E_FREE_IN_3D
) -> Slvs_Constraint:
    ...

def equal_point_to_line(
    grouph: int,
    e1: Slvs_Entity,
    e2: Slvs_Entity,
    e3: Slvs_Entity,
    e4: Slvs_Entity,
    wp: Slvs_Entity = E_FREE_IN_3D
) -> Slvs_Constraint:
    ...

def ratio(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, value: float, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def symmetric(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, e3: Slvs_Entity = E_NONE, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def symmetric_h(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Constraint:
    ...

def symmetric_v(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Constraint:
    ...

def midpoint(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def horizontal(grouph: int, e1: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Constraint:
    ...

def vertical(grouph: int, e1: Slvs_Entity, wp: Slvs_Entity) -> Slvs_Constraint:
    ...

def diameter(grouph: int, e1: Slvs_Entity, value: float) -> Slvs_Constraint:
    ...

def same_orientation(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity) -> Slvs_Constraint:
    ...

def angle(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, value: float, wp: Slvs_Entity = E_FREE_IN_3D, inverse: bool = False) -> Slvs_Constraint:
    ...

def perpendicular(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D, inverse: bool = False) -> Slvs_Constraint:
    ...

def parallel(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def tangent(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def distance_proj(grouph: int, e1: Slvs_Entity, e2: Slvs_Entity, value: float) -> Slvs_Constraint:
    ...

def length_diff(grouph: int, Slvs_entityA: Slvs_Entity, Slvs_entityB: Slvs_Entity, value: float, workplane: Slvs_Entity) -> Slvs_Constraint:
    ...

def dragged(grouph: int, e1: Slvs_Entity, wp: Slvs_Entity = E_FREE_IN_3D) -> Slvs_Constraint:
    ...

def clear_sketch() -> None:
    ...

def solve_sketch(grouph: int, calculateFaileds: bool) -> Slvs_SolveResult:
    ...

def get_param_value(ph: int) -> float:
    ...

def set_param_value(ph: int, value: float) -> None:
    ...