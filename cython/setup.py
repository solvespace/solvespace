# -*- coding: utf-8 -*-

"""Compile the Cython libraries of Python-Solvespace."""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"

import os
import re
import codecs
from setuptools import setup, Extension, find_packages
from platform import system
from distutils import sysconfig

here = os.path.abspath(os.path.dirname(__file__))
include_path = '../include/'
src_path = '../src/'
platform_path = src_path + 'platform/'
ver = sysconfig.get_config_var('VERSION')
lib = sysconfig.get_config_var('BINDIR')


def read(*parts):
    with codecs.open(os.path.join(here, *parts), 'r') as f:
        return f.read()


def get_version(*file_paths):
    doc = read(*file_paths)
    m1 = re.search(r"^set\(solvespace_VERSION_MAJOR (\d)\)", doc, re.M)
    m2 = re.search(r"^set\(solvespace_VERSION_MINOR (\d)\)", doc, re.M)
    if m1 and m2:
        return f"{m1.group(1)}.{m2.group(1)}"
    raise RuntimeError("Unable to find version string.")


macros = [
    ('_hypot', 'hypot'),
    ('M_PI', 'PI'),  # C++ 11
    ('ISOLATION_AWARE_ENABLED', None),
    ('LIBRARY', None),
    ('EXPORT_DLL', None),
    ('_CRT_SECURE_NO_WARNINGS', None),
]

compile_args = [
    '-O3',
    '-Wno-cpp',
    '-g',
    '-Wno-write-strings',
    '-fpermissive',
    '-fPIC',
    '-std=c++11',
]

sources = [
    'python_solvespace/' + 'slvs.pyx',
    src_path + 'util.cpp',
    src_path + 'entity.cpp',
    src_path + 'expr.cpp',
    src_path + 'constrainteq.cpp',
    src_path + 'constraint.cpp',
    src_path + 'system.cpp',
    src_path + 'lib.cpp',
]

if system() == 'Windows':
    # Avoid compile error with CYTHON_USE_PYLONG_INTERNALS.
    # https://github.com/cython/cython/issues/2670#issuecomment-432212671
    macros.append(('MS_WIN64', None))
    # Disable format warning
    compile_args.append('-Wno-format')

    # Solvespace arguments
    macros.append(('WIN32', None))

    # Platform sources
    sources.append(platform_path + 'w32util.cpp')
    sources.append(platform_path + 'platform.cpp')
else:
    sources.append(platform_path + 'unixutil.cpp')

setup(
    name="python_solvespace",
    version=get_version('..', 'CMakeLists.txt'),
    author=__author__,
    author_email=__email__,
    description="Python library of Solvespace",
    long_description=read("README.md"),
    url="https://github.com/solvespace/solvespace",
    packages=find_packages(exclude=('tests',)),
    ext_modules=[Extension(
        "slvs",
        sources,
        language="c++",
        include_dirs=[include_path, src_path, platform_path],
        define_macros=macros,
        extra_compile_args=compile_args
    )],
    python_requires=">=3.6",
    setup_requires=[
        'setuptools',
        'wheel',
        'cython',
    ],
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Cython",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Operating System :: OS Independent",
    ]
)
