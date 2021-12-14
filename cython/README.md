[![PyPI](https://img.shields.io/pypi/v/python-solvespace.svg)](https://pypi.org/project/python-solvespace/)
[![GitHub license](https://img.shields.io/badge/license-GPLv3+-blue.svg)](https://raw.githubusercontent.com/KmolYuan/solvespace/master/LICENSE)

# python-solvespace

Python library from the solver of SolveSpace, an open source CAD software.

+ [Python API](https://pyslvs-ui.readthedocs.io/en/stable/python-solvespace-api/)
+ [C API](https://github.com/solvespace/solvespace/blob/master/exposed/DOC.txt)

The example extracted from unit test:

```python
from python_solvespace import SolverSystem, ResultFlag

sys = SolverSystem()
wp = sys.create_2d_base()  # Workplane (Entity)
p0 = sys.add_point_2d(0, 0, wp)  # Entity
sys.dragged(p0, wp)  # Make a constraint with the entity
...
line0 = sys.add_line_2d(p0, p1, wp)  # Create entity with others
...
line1 = sys.add_line_2d(p0, p3, wp)
sys.angle(line0, line1, 45, wp)  # Constrain two entities
line1 = sys.entity(-1)  # Entity handle can be re-generated and negatively indexed
...
if sys.solve() == ResultFlag.OKAY:
   # Get the result (unpack from the entity or parameters)
   # x and y are actually float type
   dof = sys.dof()
   x, y = sys.params(p2.params)
   ...
else:
   # Error!
   # Get the list of all constraints
   failures = sys.failures()
   ...
```

Solver can also be serialized and copied, but can not modify or undo last step.

```python
import pickle
print(pickle.dumps(sys))

sys_new = sys.copy()
```

The entity and parameter handles should have the same lifetime to the solver.

# Install

```bash
pip install python-solvespace
```

# Build and Test (Repository)

First build and install the module from the repo:

```bash
git submodule update --init extlib/mimalloc
cd cython
pip install -e .
```

Build the module:

```bash
pip install -e . --no-deps
```

Run the unit tests:

```bash
python -m unittest
```

Uninstall the module:

```bash
pip uninstall python-solvespace
```
