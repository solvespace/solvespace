# -*- coding: utf-8 -*-

"""Compile the Cython libraries of Python-Solvespace."""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"

import sys
from os import walk
from os.path import dirname, isdir, join as pth_join
import re
import codecs
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
from setuptools.command.sdist import sdist
from distutils import file_util, dir_util
from platform import system

include_path = pth_join('python_solvespace', 'include')
src_path = pth_join('python_solvespace', 'src')
platform_path = pth_join(src_path, 'platform')


def write(doc, *parts):
    with codecs.open(pth_join(*parts), 'w') as f:
        f.write(doc)


def read(*parts):
    with codecs.open(pth_join(*parts), 'r') as f:
        return f.read()


def find_version(*file_paths):
    m = re.search(r"^__version__ = ['\"]([^'\"]*)['\"]", read(*file_paths), re.M)
    if m:
        return m.group(1)
    raise RuntimeError("Unable to find version string.")


macros = [
    ('M_PI', 'PI'),
    ('_USE_MATH_DEFINES', None),
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
    '-std=c++17',
]
sources = [
    pth_join('python_solvespace', 'slvs.pyx'),
    pth_join(src_path, 'util.cpp'),
    pth_join(src_path, 'entity.cpp'),
    pth_join(src_path, 'expr.cpp'),
    pth_join(src_path, 'constrainteq.cpp'),
    pth_join(src_path, 'constraint.cpp'),
    pth_join(src_path, 'system.cpp'),
    pth_join(src_path, 'lib.cpp'),
]
if {'sdist', 'bdist'} & set(sys.argv):
    for s in ('utilwin', 'utilunix', 'platform'):
        sources.append(pth_join(platform_path, f'{s}.cpp'))
elif system() == 'Windows':
    # Disable format warning
    compile_args.append('-Wno-format')
    # Solvespace arguments
    macros.append(('WIN32', None))
    # Platform sources
    sources.append(pth_join(platform_path, 'utilwin.cpp'))
    sources.append(pth_join(platform_path, 'platform.cpp'))
    if sys.version_info < (3, 7):
        macros.append(('_hypot', 'hypot'))
else:
    sources.append(pth_join(platform_path, 'utilunix.cpp'))


def copy_source(dry_run):
    dir_util.copy_tree(pth_join('..', 'include'), include_path, dry_run=dry_run)
    dir_util.mkpath(src_path)
    for root, _, files in walk(pth_join('..', 'src')):
        for f in files:
            if not f.endswith('.h'):
                continue
            f = pth_join(root, f)
            f_new = f.replace('..', 'python_solvespace')
            if not isdir(dirname(f_new)):
                dir_util.mkpath(dirname(f_new))
            file_util.copy_file(f, f_new, dry_run=dry_run)
    for f in sources[1:]:
        file_util.copy_file(f.replace('python_solvespace', '..'), f, dry_run=dry_run)
    open(pth_join(platform_path, 'config.h'), 'a').close()


class Build(build_ext):
    def build_extensions(self):
        compiler = self.compiler.compiler_type
        if compiler in {'mingw32', 'unix'}:
            for e in self.extensions:
                e.define_macros = macros
                e.extra_compile_args = compile_args
        elif compiler == 'msvc':
            for e in self.extensions:
                e.define_macros = macros[1:]
                e.libraries = ['shell32']
        super(Build, self).build_extensions()

    def run(self):
        has_src = isdir(include_path) and isdir(src_path)
        if not has_src:
            copy_source(self.dry_run)
        super(Build, self).run()
        if not has_src:
            dir_util.remove_tree(include_path, dry_run=self.dry_run)
            dir_util.remove_tree(src_path, dry_run=self.dry_run)


class PackSource(sdist):
    def run(self):
        copy_source(self.dry_run)
        super(PackSource, self).run()
        if not self.keep_temp:
            dir_util.remove_tree(include_path, dry_run=self.dry_run)
            dir_util.remove_tree(src_path, dry_run=self.dry_run)


setup(
    name="python_solvespace",
    version=find_version('python_solvespace', '__init__.py'),
    author=__author__,
    author_email=__email__,
    description="Python library of Solvespace.",
    long_description=read("README.md"),
    long_description_content_type='text/markdown',
    url="https://github.com/KmolYuan/solvespace",
    packages=find_packages(exclude=('tests',)),
    package_data={'': ["*.pyi", "*.pxd"], 'python_solvespace': ['py.typed']},
    ext_modules=[Extension(
        "python_solvespace.slvs",
        sources,
        language="c++",
        include_dirs=[include_path, src_path, platform_path]
    )],
    cmdclass={'build_ext': Build, 'sdist': PackSource},
    zip_safe=False,
    python_requires=">=3.6",
    install_requires=read('requirements.txt').splitlines(),
    test_suite='tests',
    classifiers=[
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Cython",
        "License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)",
        "Operating System :: OS Independent",
    ]
)
