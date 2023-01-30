"""'solvespace' module is a wrapper of
Python binding Solvespace solver libraries.
"""
from __future__ import annotations
import solvespace
import typing
from solvespace.slvs import Constraint
from solvespace.slvs import Entity
from solvespace.slvs import Equation
from solvespace.slvs import Expr
from solvespace.slvs import Group
from solvespace.slvs import IdListEquation
from solvespace.slvs import IdListParam
from solvespace.slvs import ListhConstraint
from solvespace.slvs import ListhGroup
from solvespace.slvs import ListhParam
from solvespace.slvs import Param
from solvespace.slvs import Quaternion
from solvespace.slvs import Sketch
from solvespace.slvs import SolveResult
from solvespace.slvs import SolverResult
from solvespace.slvs import SolverSystem
from solvespace.slvs import Vector
from solvespace.slvs import hConstraint
from solvespace.slvs import hEntity
from solvespace.slvs import hGroup
from solvespace.slvs import hParam
import solvespace.slvs

__all__ = [
    "Constraint",
    "Entity",
    "Equation",
    "Expr",
    "Group",
    "IdListEquation",
    "IdListParam",
    "ListhConstraint",
    "ListhGroup",
    "ListhParam",
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
    "hGroup",
    "hParam",
    "slvs"
]


SK: solvespace.slvs.Sketch
__author__ = 'Koen Schmeets'
__copyright__ = 'Copyright (C) 2023-2024'
__email__ = 'koen@schmeets.de'
__license__ = 'GPLv3+'
__version__ = '0.0.1'
