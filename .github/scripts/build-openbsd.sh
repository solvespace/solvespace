
mkdir -p build
cd build

cmake \
  -DCMAKE_BUILD_TYPE="Release" \
  -DUSE_GTK4="ON" \
  ..

make -j$(sysctl -n hw.ncpuonline)
