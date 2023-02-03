from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

solvespace_extension = Extension(
    name="solvespace",
    sources=["src/lib.pyx"],
    libraries=["slvs"],
    library_dirs=["build-arm64/bin"],
    include_dirs=["include"]
)
setup(
    name="solvespace",
    ext_modules=cythonize([solvespace_extension])
)
