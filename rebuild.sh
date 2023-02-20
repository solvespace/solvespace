#!/bin/bash

set -e

source ../emsdk/emsdk_env.sh
mkdir build-emscripten || true
cd build-emscripten
emcmake cmake .. -DCMAKE_RELEASE_TYPE=Debug -DENABLE_GUI="OFF" -DENABLE_CLI="OFF" -DENABLE_TESTS="OFF" -DENABLE_COVERAGE="OFF" -DENABLE_OPENMP="OFF" -DFORCE_VENDORED_Eigen3="ON" -DENABLE_LTO="ON" -DENABLE_EMSCRIPTEN_LIB="ON"
cmake --build . -j10
# CMAKE_BUILD_PARALLEL_LEVEL=10 python3.11 -m pip install . -vv
# cd python && python3.11 tests
cd bin
cat << EOF > index.html
<script src="slvs.js"></script>
<script type="text/javascript">
  Module.onRuntimeInitialized = () => {
    slvs = Module;

    slvs.clearSketch()
    g = 1
    wp = slvs.add2DBase(g)
    p0 = slvs.addPoint2D(g, 0, 0, wp)
    slvs.dragged(g, p0, wp)
    p1 = slvs.addPoint2D(g, 90, 0, wp)
    slvs.dragged(g, p1, wp)
    line0 = slvs.addLine2D(g, p0, p1, wp)
    p2 = slvs.addPoint2D(g, 20, 20, wp)
    p3 = slvs.addPoint2D(g, 0, 10, wp)
    p4 = slvs.addPoint2D(g, 30, 20, wp)
    slvs.distance(g, p2, p3, 40, wp)
    slvs.distance(g, p2, p4, 40, wp)
    slvs.distance(g, p3, p4, 70, wp)
    slvs.distance(g, p0, p3, 35, wp)
    slvs.distance(g, p1, p4, 70, wp)
    line1 = slvs.addLine2D(g, p0, p3, wp)
    slvs.angle(g, line0, line1, 45, wp, false)
    // slvs.addConstraint(g, ConstraintType.ANGLE, wp, 45.0, E_NONE, E_NONE, line0, line1)

    result = slvs.solveSketch(g, false)
    console.log(result['result'])
    x = slvs.getParamValue(p2, 0)
    y = slvs.getParamValue(p2, 1)
    console.log(39.54852, x)
    console.log(61.91009, y)
  }
</script>
EOF
python3 -m http.server 8080