{
  lib,
  rev ? lib.sources.commitIdFromGitRepo ./../.git, 
  stdenv,
  cmake,
  pkg-config,
  wrapGAppsHook3,
  at-spi2-core,
  dbus,
  glew,
  libdatrie,
  libepoxy,
  libselinux,
  libsepol,
  libthai,
  libxkbcommon,
  eigen,
  useInTreeEigen ? true,
  zlib,
  useInTreeZlib ? true,
  libpng,
  useInTreeLibpng ? true,
  cairo,
  useInTreeCairo ? true,
  freetype,
  useInTreeFreetype ? true,
  json_c,
  fontconfig,
  gtkmm3,
  pangomm,
  pcre2,
  util-linuxMinimal,
  xorg,
  libGL,
  libGLU,
  libspnav,
  libsysprof-capture,
  lerc,
}:
let
  inherit (stdenv) mkDerivation;
  inherit (lib) optional;
  inherit (lib.sources) cleanSource;
in
mkDerivation {
  pname = "solvespace";
  version = rev;

  src = cleanSource ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
    wrapGAppsHook3
  ];

  buildInputs = [
    at-spi2-core
    dbus
    lerc
    fontconfig
    glew
    gtkmm3
    json_c
    libdatrie
    libepoxy
    libGL
    libGLU
    libselinux
    libsepol
    libspnav
    libthai
    libxkbcommon
    pangomm
    pcre2
    util-linuxMinimal
    xorg.libpthreadstubs
    xorg.libXdmcp
    xorg.libXtst
    libsysprof-capture
  ]
  ++ (optional (!useInTreeCairo) cairo)
  ++ (optional (!useInTreeEigen) eigen)
  ++ (optional (!useInTreeFreetype) freetype)
  ++ (optional (!useInTreeLibpng) libpng)
  ++ (optional (!useInTreeZlib) zlib);

  postPatch = ''
    patch CMakeLists.txt <<EOF
    @@ -20,9 +20,9 @@
     # NOTE TO PACKAGERS: The embedded git commit hash is critical for rapid bug triage when the builds
     # can come from a variety of sources. If you are mirroring the sources or otherwise build when
     # the .git directory is not present, please comment the following line:
    -include(GetGitCommitHash)
    +# include(GetGitCommitHash)
     # and instead uncomment the following, adding the complete git hash of the checkout you are using:
    -# set(GIT_COMMIT_HASH 0000000000000000000000000000000000000000)
    +set(GIT_COMMIT_HASH ${rev})
    EOF
  '';

  cmakeFlags = [ "-DENABLE_OPENMP=ON" ];
}
