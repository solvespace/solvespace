from __future__ import annotations
import solvespace
import typing
from solvespace.slvs import Constraint
from solvespace.slvs import Entity
from solvespace.slvs import Equation
from solvespace.slvs import Group
from solvespace.slvs import Param
from solvespace.slvs import Quaternion
from solvespace.slvs import Sketch
from solvespace.slvs import SolveResult
from solvespace.slvs import SolverResult
from solvespace.slvs import SolverSystem
from solvespace.slvs import Vector
from solvespace.slvs import hConstraint
from solvespace.slvs import hEntity
from solvespace.slvs import hEquation
from solvespace.slvs import hGroup
from solvespace.slvs import hParam
import solvespace.slvs

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
    "hParam",
    "slvs"
]


SK: solvespace.slvs.Sketch
