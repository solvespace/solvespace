[![Build status](https://ci.appveyor.com/api/projects/status/b2o8jw7xnfqghqr5?svg=true)](https://ci.appveyor.com/project/KmolYuan/solvespace)
[![Build status](https://travis-ci.org/KmolYuan/solvespace.svg)](https://travis-ci.org/KmolYuan/solvespace)
![OS](https://img.shields.io/badge/OS-Windows%2C%20Mac%20OS%2C%20Ubuntu-blue.svg)
[![GitHub license](https://img.shields.io/badge/license-GPLv3+-blue.svg)](https://raw.githubusercontent.com/KmolYuan/solvespace/master/LICENSE)

python-solvespace
===

Python library from solver of SolveSpace.

Feature for CDemo and Python interface can see [here](https://github.com/solvespace/solvespace/blob/master/exposed/DOC.txt).

Build and Test
===

Requirement:

+ [Cython]

Build and install the module:

```bash
python setup.py install
```

Run unit test:

```bash
python tests
```

Uninstall the module:

```bash
pip uninstall python_solvespace
```

[GNU Make]: https://sourceforge.net/projects/mingw-w64/files/latest/download?source=files
[Cython]: https://cython.org/
