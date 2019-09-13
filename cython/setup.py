# -*- coding: utf-8 -*-

"""Compile the Cython libraries of Python-Solvespace."""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"

import os
from os.path import (
    abspath,
    dirname,
    join as pth_join,
)
import re
import codecs
from textwrap import dedent
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
from platform import system
from distutils import sysconfig

here = abspath(dirname(__file__))
include_path = '../include/'
src_path = '../src/'
platform_path = src_path + 'platform/'
ver = sysconfig.get_config_var('VERSION')
lib = sysconfig.get_config_var('BINDIR')


def write(doc, *parts):
    with codecs.open(pth_join(here, *parts), 'w') as f:
        f.write(doc)


def read(*parts):
    with codecs.open(pth_join(here, *parts), 'r') as f:
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
    sources.append(platform_path + 'utilwin.cpp')
    sources.append(platform_path + 'platform.cpp')
else:
    sources.append(platform_path + 'utilunix.cpp')


class Build(build_ext):
    def run(self):
        # Generate "config.h", actually not used.
        config_h = src_path + "config.h"
        write(dedent(f"""\
            #ifndef SOLVESPACE_CONFIG_H
            #define SOLVESPACE_CONFIG_H
            #endif
        """), config_h)
        super(Build, self).run()
        os.remove(config_h)


setup(
    name="python_solvespace",
    version=get_version('..', 'CMakeLists.txt'),
    author=__author__,
    author_email=__email__,
    description="Python library of Solvespace",
    long_description=read("README.md"),
    url="https://github.com/solvespace/solvespace",
    packages=find_packages(exclude=('tests',)),
    package_data={'': ["*.pyi"]},
    ext_modules=[Extension(
        "python_solvespace.slvs",
        sources,
        language="c++",
        include_dirs=[include_path, src_path, platform_path],
        define_macros=macros,
        extra_compile_args=compile_args
    )],
    cmdclass={'build_ext': Build},
    python_requires=">=3.6",
    install_requires=read('requirements.txt').splitlines(),
    test_suite="tests",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Cython",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Operating System :: OS Independent",
    ]
)
