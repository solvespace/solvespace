echo off

REM Usage: set_pycompiler C:\Python37
REM Where %PYTHON_DIR% is the directory of your Python installation.
REM In Pyslvs project.
set HERE=%~dp0
set PYTHON_DIR=%1

REM Create "distutils.cfg"
echo [build]>> "%PYTHON_DIR%\Lib\distutils\distutils.cfg"
echo compiler=mingw32>> "%PYTHON_DIR%\Lib\distutils\distutils.cfg"

REM Apply the patch of "cygwinccompiler.py".
REM Unix "patch" command of Msys.
patch "%PYTHON_DIR%\lib\distutils\cygwinccompiler.py" "%HERE%patch.diff"

REM Copy "vcruntime140.dll" to "libs".
copy "%PYTHON_DIR%\vcruntime140.dll" "%PYTHON_DIR%\libs"
