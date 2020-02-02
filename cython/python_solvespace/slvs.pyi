# -*- coding: utf-8 -*-

from typing import Tuple, List, Sequence, Counter, ClassVar

def quaternion_u(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    """Input quaternion, return unit vector of U axis.

    Where `qw`, `qx`, `qy`, `qz` are corresponded to the W, X, Y, Z value of quaternion.
    """
    ...

def quaternion_v(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    """Input quaternion, return unit vector of V axis.

    Signature is same as [quaternion_u](#quaternion_u).
    """
    ...

def quaternion_n(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    """Input quaternion, return unit vector of normal.

    Signature is same as [quaternion_u](#quaternion_u).
    """
    ...

def make_quaternion(
    ux: float,
    uy: float,
    uz: float,
    vx: float,
    vy: float,
    vz: float
) -> Tuple[float, float, float, float]:
    """Input two unit vector, return quaternion.

    Where `ux`, `uy`, `uz` are corresponded to the value of U vector;
    `vx`, `vy`, `vz` are corresponded to the value of V vector.
    """
    ...

class Params:

    """The handles of parameters."""

    def __repr__(self) -> str:
        ...

class Entity:

    """The handles of entities."""

    FREE_IN_3D: ClassVar[Entity] = ...
    NONE: ClassVar[Entity] = ...
    params: Params

    def is_3d(self) -> bool:
        """Return True if this is a 3D entity."""
        ...

    def is_none(self) -> bool:
        """Return True if this is a empty entity."""
        ...

    def is_point_2d(self) -> bool:
        """Return True if this is a 2D point."""
        ...

    def is_point_3d(self) -> bool:
        """Return True if this is a 3D point."""
        ...

    def is_point(self) -> bool:
        """Return True if this is a point."""
        ...

    def is_normal_2d(self) -> bool:
        """Return True if this is a 2D normal."""
        ...

    def is_normal_3d(self) -> bool:
        """Return True if this is a 3D normal."""
        ...

    def is_normal(self) -> bool:
        """Return True if this is a normal."""
        ...

    def is_distance(self) -> bool:
        """Return True if this is a distance."""
        ...

    def is_work_plane(self) -> bool:
        """Return True if this is a work plane."""
        ...

    def is_line_2d(self) -> bool:
        """Return True if this is a 2D line."""
        ...

    def is_line_3d(self) -> bool:
        """Return True if this is a 3D line."""
        ...

    def is_line(self) -> bool:
        """Return True if this is a line."""
        ...

    def is_cubic(self) -> bool:
        """Return True if this is a cubic."""
        ...

    def is_circle(self) -> bool:
        """Return True if this is a circle."""
        ...

    def is_arc(self) -> bool:
        """Return True if this is a arc."""
        ...

    def __repr__(self) -> str:
        ...

class SolverSystem:

    """A solver system of Python-Solvespace.

    The operation of entities and constraints are using the methods of this class.
    """

    def __init__(self) -> None:
        """Initialization method. Create a solver system."""
        ...

    def clear(self) -> None:
        """Clear the system."""
        ...

    def set_group(self, g: int) -> None:
        """Set the current group (`g`)."""
        ...

    def group(self) -> int:
        """Return the current group."""
        ...

    def set_params(self, p: Params, params: Sequence[float]) -> None:
        """Set the parameters from a [Params] handle (`p`) belong to this system.
        The values is come from `params`, length must be equal to the handle.
        """
        ...

    def params(self, p: Params) -> Tuple[float, ...]:
        """Get the parameters from a [Params] handle (`p`) belong to this system.
        The length of tuple is decided by handle.
        """
        ...

    def dof(self) -> int:
        """Return the degrees of freedom of current group.
        Only can be called after solving.
        """
        ...

    def constraints(self) -> Counter[str]:
        """Return the number of each constraint type.
        The name of constraints is represented by string.
        """
        ...

    def failures(self) -> List[int]:
        """Return a list of failed constraint numbers."""
        ...

    def solve(self) -> int:
        """Start the solving, return the result flag."""
        ...

    def create_2d_base(self) -> Entity:
        """Create a 2D system on current group,
        return the handle of work plane.
        """
        ...

    def add_point_2d(self, u: float, v: float, wp: Entity) -> Entity:
        """Add a 2D point to specific work plane (`wp`) then return the handle.

        Where `u`, `v` are corresponded to the value of U, V axis on the work plane.
        """
        ...

    def add_point_3d(self, x: float, y: float, z: float) -> Entity:
        """Add a 3D point then return the handle.

        Where `x`, `y`, `z` are corresponded to the value of X, Y, Z axis.
        """
        ...

    def add_normal_2d(self, wp: Entity) -> Entity:
        """Add a 2D normal orthogonal to specific work plane (`wp`)
        then return the handle.
        """
        ...

    def add_normal_3d(self, qw: float, qx: float, qy: float, qz: float) -> Entity:
        """Add a 3D normal from quaternion then return the handle.

        Where `qw`, `qx`, `qy`, `qz` are corresponded to
        the W, X, Y, Z value of quaternion.
        """
        ...

    def add_distance(self, d: float, wp: Entity) -> Entity:
        """Add a distance to specific work plane (`wp`) then return the handle.

        Where `d` is distance value.
        """
        ...

    def add_line_2d(self, p1: Entity, p2: Entity, wp: Entity) -> Entity:
        """Add a 2D line to specific work plane (`wp`) then return the handle.

        Where `p1` is the start point; `p2` is the end point.
        """
        ...

    def add_line_3d(self, p1: Entity, p2: Entity) -> Entity:
        """Add a 3D line then return the handle.

        Where `p1` is the start point; `p2` is the end point.
        """
        ...

    def add_cubic(self, p1: Entity, p2: Entity, p3: Entity, p4: Entity, wp: Entity) -> Entity:
        """Add a cubic curve to specific work plane (`wp`) then return the handle.

        Where `p1` to `p4` is the control points.
        """
        ...

    def add_arc(self, nm: Entity, ct: Entity, start: Entity, end: Entity, wp: Entity) -> Entity:
        """Add an arc to specific work plane (`wp`) then return the handle.

        Where `nm` is the orthogonal normal; `ct` is the center point;
        `start` is the start point; `end` is the end point.
        """
        ...

    def add_circle(self, nm: Entity, ct: Entity, radius: Entity, wp: Entity) -> Entity:
        """Add an circle to specific work plane (`wp`) then return the handle.

        Where `nm` is the orthogonal normal;
        `ct` is the center point;
        `radius` is the distance value represent radius.
        """
        ...

    def add_work_plane(self, origin: Entity, nm: Entity) -> Entity:
        """Add a work plane then return the handle.

        Where `origin` is the origin point of the plane;
        `nm` is the orthogonal normal.
        """
        ...

    def add_constraint(
        self,
        c_type: int,
        wp: Entity,
        v: float,
        p1: Entity,
        p2: Entity,
        e1: Entity,
        e2: Entity,
        e3: Entity = Entity.NONE,
        e4: Entity = Entity.NONE,
        other: int = 0,
        other2: int = 0
    ) -> None:
        """Add a constraint by type code `c_type`.
        This is an origin function mapping to different constraint methods.

        Where `wp` represents work plane; `v` represents constraint value;
        `p1` and `p2` represent point entities; `e1` to `e4` represent other types of entity;
        `other` and `other2` are control options of the constraint.
        """
        ...

    def coincident(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        """Coincident two entities.

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_point] | [is_point] | Optional |
        | [is_point] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point] | [is_line] | Optional |
        | [is_point] | [is_circle] | Optional |
        """
        ...

    def distance(
        self,
        e1: Entity,
        e2: Entity,
        value: float,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        """Distance constraint between two entities.

        If `value` is equal to zero, then turn into [coincident](#solversystemcoincident)

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_point] | [is_point] | Optional |
        | [is_point] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point] | [is_line] | Optional |
        """
        ...

    def equal(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        """Equal constraint between two entities.

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_line] | [is_line] | Optional |
        | [is_line] | [is_arc] | Optional |
        | [is_line] | [is_circle] | Optional |
        | [is_arc] | [is_arc] | Optional |
        | [is_arc] | [is_circle] | Optional |
        | [is_circle] | [is_circle] | Optional |
        | [is_circle] | [is_arc] | Optional |
        """
        ...

    def equal_included_angle(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity,
        e4: Entity,
        wp: Entity
    ) -> None:
        """Constraint that 2D line 1 (`e1`) and line 2 (`e2`),
        line 3 (`e3`) and line 4 (`e4`) must have same included angle on work plane `wp`.
        """
        ...

    def equal_point_to_line(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity,
        e4: Entity,
        wp: Entity
    ) -> None:
        """Constraint that point 1 (`e1`) and line 1 (`e2`),
        point 2 (`e3`) and line 2  (`e4`) must have same distance on work plane `wp`.
        """
        ...

    def ratio(self, e1: Entity, e2: Entity, value: float, wp: Entity) -> None:
        """The ratio (`value`) constraint between two 2D lines (`e1` and `e2`)."""
        ...

    def symmetric(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity = Entity.NONE,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        """Symmetric constraint between two points.

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Entity 3 (`e3`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:---------------:|:-----------------:|
        | [is_point_3d] | [is_point_3d] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point_2d] | [is_point_2d] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point_2d] | [is_point_2d] | [is_line_2d] | Is not [Entity.FREE_IN_3D] |
        """
        ...

    def symmetric_h(self, e1: Entity, e2: Entity, wp: Entity) -> None:
        """Symmetric constraint between two 2D points (`e1` and `e2`)
        with horizontal line on the work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        ...

    def symmetric_v(self, e1: Entity, e2: Entity, wp: Entity) -> None:
        """Symmetric constraint between two 2D points (`e1` and `e2`)
        with vertical line on the work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        ...

    def midpoint(
        self,
        e1: Entity,
        e2: Entity,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        """Midpoint constraint between a point (`e1`) and
        a line (`e2`) on work plane (`wp`).
        """
        ...

    def horizontal(self, e1: Entity, wp: Entity) -> None:
        """Horizontal constraint of a 2d point (`e1`) on
        work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        ...

    def vertical(self, e1: Entity, wp: Entity) -> None:
        """Vertical constraint of a 2d point (`e1`) on
        work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        ...

    def diameter(self, e1: Entity, value: float, wp: Entity) -> None:
        """Diameter (`value`) constraint of a circular entities.

        | Entity 1 (`e1`) | Work plane (`wp`) |
        |:---------------:|:-----------------:|
        | [is_arc] | Optional |
        | [is_circle] | Optional |
        """
        ...

    def same_orientation(self, e1: Entity, e2: Entity) -> None:
        """Equal orientation constraint between two 3d normals (`e1` and `e2`)."""
        ...

    def angle(self, e1: Entity, e2: Entity, value: float, wp: Entity, inverse: bool = False) -> None:
        """Degrees angle (`value`) constraint between two 2d lines (`e1` and `e2`)
        on the work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        ...

    def perpendicular(self, e1: Entity, e2: Entity, wp: Entity, inverse: bool = False) -> None:
        """Perpendicular constraint between two 2d lines (`e1` and `e2`)
        on the work plane (`wp` can not be [Entity.FREE_IN_3D]) with `inverse` option.
        """
        ...

    def parallel(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        """Parallel constraint between two lines (`e1` and `e2`) on
        the work plane (`wp`).
        """
        ...

    def tangent(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        """Parallel constraint between two entities (`e1` and `e2`) on the work plane (`wp`).

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_arc] | [is_line_2d] | Is not [Entity.FREE_IN_3D] |
        | [is_cubic] | [is_line_3d] | [Entity.FREE_IN_3D] |
        | [is_arc] | [is_cubic] | Is not [Entity.FREE_IN_3D] |
        | [is_arc] | [is_arc] | Is not [Entity.FREE_IN_3D] |
        | [is_cubic] | [is_cubic] | Optional |
        """
        ...

    def distance_proj(self, e1: Entity, e2: Entity, value: float) -> None:
        """Projected distance (`value`) constraint between
        two 3d points (`e1` and `e2`).
        """
        ...

    def dragged(self, e1: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        """Dragged constraint of a point (`e1`) on the work plane (`wp`)."""
        ...
