# -*- coding: utf-8 -*-

"""'python_solvespace' module is a wrapper of
Python binding Solvespace solver libraries.
"""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"
__version__ = "3.0.6"

from .slvs import (
    quaternion_u,
    quaternion_v,
    quaternion_n,
    make_quaternion,
    Constraint,
    ResultFlag,
    Params,
    Entity,
    SolverSystem,
)

__all__ = [
    'quaternion_u',
    'quaternion_v',
    'quaternion_n',
    'make_quaternion',
    'Constraint',
    'ResultFlag',
    'Params',
    'Entity',
    'SolverSystem',
]
