# Experimental STEP reader and native conversion proofs

This is **not STEP file linking yet**. The opt-in reader targets read
schema-backed Part 21 data. A separate native conversion target now creates
`SShell` geometry for a deliberately narrow planar-solid slice, described below.
No constraint entity, group, file-dialog filter, or link dispatch has been
added. No OCCT/Open Cascade code or wrapper is used.

The production goal remains one immutable linked group per STEP file, using
native `SShell` geometry and useful constraint references, without recovering
feature history or editing STEP data. There will be no automatic mesh fallback.
Partial conversion may skip **whole unsupported solids**, never selected
faces or cavity shells; it must report omissions and reject a link with no
usable geometry. The native proof implements that whole-solid boundary, but
does not create links or groups. Reported parse errors still reject the entire
document, and a successfully read document need not contain a convertible solid.

## Build and run

The normal build is unchanged: `ENABLE_STEP_IMPORT_PROOF` defaults to `OFF`.
Use a native build with the existing SolveSpace prerequisites; no additional
lexer/parser generator or geometry kernel is required.

```sh
git submodule update --init extlib/stepcode
cmake -S . -B build-step -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_STEP_IMPORT_PROOF=ON -DENABLE_TESTS=ON \
    "-DSTEP_IMPORT_SCHEMAS=AP203;AP214;AP242"
cmake --build build-step --target solvespace-step-probe test_step_import
build-step/bin/solvespace-step-probe path/to/model.step
```

On Windows use the matching Visual Studio developer environment and the `.exe`
suffix. The example uses POSIX line continuations; in PowerShell put the CMake
arguments on one line or use PowerShell's backtick continuation. With no file
arguments the probe lists the compiled bindings. With files it prints schema,
instance and topology counts; a rejected file gives a nonzero exit status and
the captured reader diagnostics. It never writes a STEP file.

`STEP_IMPORT_SCHEMAS` explicitly selects a nonempty subset of `AP203;AP214;AP242`
(the default is all three). For the smallest proof build use
`-DSTEP_IMPORT_SCHEMAS=AP203`. Unknown selection names fail configuration;
disabled and unknown `FILE_SCHEMA` names are rejected at runtime. Tests require
SolveSpace's normal `ENABLE_TESTS` prerequisites; the STEP test executable
itself links only the private reader and STEPcode libraries, not the solver or
GUI. Set `SLVS_STEP_TEST_FILTER` to a test-name substring for a focused run.
Multi-config generators additionally require CMake 3.20 or newer; select the
configuration with `cmake --build build-step --config Debug --target ...`.

## Dependency and schema selection

The submodule is pinned to STEPcode development revision
`24783b448511b8f6bce2829286a3f00015430236` (upstream reports 0.8.1). This proof uses
that revision's compact **API v2, late-bound, structural-metadata** backend:

```text
exp2cxx -V 2 --late-bound --metadata structural <complete-schema.exp>
```

| Selection | EXPRESS input under `extlib/stepcode/data` | `FILE_SCHEMA` base name | Entity types |
| --- | --- | --- | ---: |
| AP203 | `ap203/ap203.exp` | `CONFIG_CONTROL_DESIGN` | 254 |
| AP214 | `ap214e3/AP214E3_2010.exp` | `AUTOMOTIVE_DESIGN` | 915 |
| AP242 | `ap242/242_mim_lf.exp` | `AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF` | 2407 |

These are specific schema revisions, not promises to accept every edition of
each AP. AP203 identifies amendment 1, WG3 N916, with an upstream semantics-neutral
EXPRESS fix. AP214 identifies version 3 (its source header describes the
ISO/DIS 10303-214:2007 long form, dated 2009-06-30). AP242 identifies WG12 N11521,
superseding WG3 N11273. It is not labelled here as a verified blanket edition
range. All three bindings initialize and are used in the same executable.

The complete supplied EXPRESS schemas are retained unchanged. Structural mode
omits executable EXPRESS functions/rules and their full metadata; it does not
trim entity definitions to match planned converter support. A toroidal surface,
for example, remains readable even if a future converter cannot handle it.

The reader selects **one registry per document**, using only the normalized
base name of its single `FILE_SCHEMA` identifier. A brace-delimited numeric
object identifier is retained in `DeclaredSchema()`, not interpreted as a
guarantee of edition compatibility. Structural layout mismatches still fail.
There is no cross-schema retry, extension dropping, or merged all-AP registry.

## Build isolation

`cmake/StepCode.cmake` builds static runtime libraries and the host `exp2cxx`
tool through an isolated `ExternalProject`. Its own cache, configuration
headers, CMake options and flags stay in `build-step/src/step/runtime`.
Compiler, architecture/generator settings and MSVC `/MT[d]` flags follow the
parent build. Cached upstream scanner/parser sources avoid requiring re2c,
Lemon or Perplex installations. Building the proof never downloads tools or
updates submodules automatically.

Bindings are generated in `build-step/src/step/schemas/<AP>` at build time
(with an additional configuration directory for multi-config generators).
The fixed compact source list is checked against the generator's own source
manifest; SolveSpace does not invoke the configure-time schema scanner.
Generation depends on the EXPRESS input, generator executable and adjustment
script. An unchanged build checks the external build, but does not regenerate
or recompile schema bindings.

The submodule stays unmodified. Reproducible adjustments are confined to:

- `cmake/stepcode/SC_Targets.cmake`: remove upstream's redundant unscoped
  `target_link_libraries` call after its `PUBLIC` calls. Select a build-tree
  copy of `STEPfile.cc` whose temporary header instance manager owns instances
  on early error/EOF returns; normal header transfer retains upstream ownership.
  Also free the copied temporary aggregates used to default missing headers
  before strict reading rejects them.
- `cmake/stepcode/GenerateSchema.cmake`: split generated complex-name string
  literals at encoded NULs to satisfy MSVC's per-literal limit. Bytes and table
  offsets are unchanged; the pinned tables fit its concatenated-literal limit.
- Private per-binding definitions rename the four otherwise-global entry
  points `SchemaInit`, `InitSchemasAndEnts`, `gencomplex`, and `GetModelContents`.

Review these contracts and rerun the tests whenever the pin changes.
The BSD notices, including generated-code coverage, are in `THIRD_PARTIES.txt`
and the upstream `COPYING`, `AUTHORS` and `INTENT.md` files.

Cross compilation is deliberately rejected when the proof is enabled: a target
`exp2cxx` cannot safely be executed as a host tool. A production build needs an
explicit host-tools build plus target runtime, or reproducibly pre-generated
bindings with the generator/schema versions recorded. Neither path is silently
selected or implemented here. Keep the option off for those builds.

## Reader boundary and limitations

Only `src/step/stepreader.cpp` and private binding targets consume STEPcode
headers, macros, types and ownership. Nothing is added to `slvs_deps`,
`slvs-solver`, `slvs-interface`, or the existing application targets. The probe
and standalone tests include only `src/step/stepreader.h`.

`Document` owns its registry and instances and returns value snapshots. It can
query inheritance, complex components, references, primitive/SELECT values and
nested aggregates needed for topology, units and rational splines. References
use Part 21 instance IDs. Strings retain STEP quoting/escapes; opaque values are
explicitly not decoded. This is not a complete SDAI wrapper or public solver API.

The adapter uses STEPcode's header and two data passes on the caller's seekable
stream, rather than its public `AppendFile()` path, which reopens a narrow
filename for pass two. It sets the classic stream numeric locale for parsing
and restores the caller's locale. The CLI probe still uses ordinary narrow
command-line filenames; future linking must use SolveSpace's `Platform::Path`
handling, including Unicode on Windows.

**Single-threaded proof only, including queries and destruction.** STEPcode has
global descriptor state and writes console diagnostics directly. The adapter
temporarily redirects `std::cout`/`std::cerr`; it is not suitable for GUI or
concurrent/background loading as written. Registry/instance lifetime, header
destruction order and EOF handling are contained by the adapter/overlay, but
this is not a hardened parser for arbitrary untrusted files. Resource limits,
cancellation, exception-safety auditing and sanitizer/fuzz testing remain work.

The supported envelope is one uncompressed Part 21 file with a conventional
header and one unparameterized `DATA` section. XML, ZIP containers, external
references, multiple/named data sections and later Part 21 features are not
implemented. The structural reader is **not full EXPRESS WHERE/global-rule or
geometric validation**. Nested aggregates use STEPcode's descriptor-backed
decoding; successful parsing does not prove manifoldness, units consistency,
valid trimming, or convertible geometry.

## Tests and compatibility findings

`test_step_import` is independent of `test_solvespace`. It covers each compiled
binding, topology/reference traversal, units and typed measures, cavity
references, rational B-spline weights/control nets, malformed/truncated input,
same- and cross-protocol document independence, locale restoration and repeated
successful/failed-load cleanup. MSVC Debug additionally compares outstanding
normal CRT blocks/bytes after warm-up and 20 repeated cycles. This is a leak
regression check, not a substitute for a sanitizer run. The synthetic cavity
fixture deliberately reuses a shell; it proves references, not valid geometry.

Real files already in the pinned dependency are tested without downloads:

| File | Result |
| --- | --- |
| AP214 `io1-cm-214.stp` | 917 instances, 1 solid, 29 advanced faces |
| AP214 `as1-oc-214.stp` | 6425 instances, 5 solids, 28 B-spline surfaces |
| AP242 `test_ap242_styled_item.stp` | 18 instances, including SELECT references |

A separate manual AP203 probe also accepts the unmodified SolidWorks 2014
[SparkFun ROB-12478 sample](https://raw.githubusercontent.com/sparkfun/3D_Models/53d298c3b8202cf8fce9ab61885855720c84c012/products/12478/step/ROB-12478.STEP):
178 instances, 1 solid, 4 advanced faces, and a conversion-based unit. The file
at the pinned revision has SHA256
`50e0c2af81012cf2147204009940f3861c981be447b70ae7bc6fd66e3c0b38f5`.
It is not vendored or downloaded by any test target.

An additional **explicit, optional** corpus target uses unmodified NIST files:

```sh
cmake --build build-step --target test_step_import_nist
```

It is available when AP242 is selected, and is never part of normal builds or
tests. `test/step/RunNist.cmake` downloads only on request, or accepts
`-DSTEP_IMPORT_NIST_ARCHIVE=/path/to/NIST-PMI-STEP-Files.zip` at configuration
for an offline run. It verifies SHA256
`1fb91bb8ff0fe02032b948fda0775bc74591cd0bebc0988347d32574e5884f90` of the archive
at [usnistgov/SFA revision 1236017](https://github.com/usnistgov/SFA/blob/1236017676c0fa1f24f0fb4642fb088437a406f7/Release/NIST-PMI-STEP-Files.zip),
extracts three selected files into the build tree, and runs with a bounded
timeout. The corpus is not vendored or installed.

- NIST CTC-01 AP242 edition-1 sample: 4350 instances, 1 solid, 117 faces.
- NIST CTC-03 AP242 edition-2 sample: 5920 instances, 1 solid, 120 faces.
- NIST FTC-11 AP203-labelled sample: expected rejection, because its presentation
  entities such as `COLOUR_RGB` are not in `CONFIG_CONTROL_DESIGN`.

Other deliberately recorded compatibility limits:

- SolveSpace's current **surface** exporter declares `CONFIG_CONTROL_DESIGN`
  but emits presentation entities absent from this AP203 binding. The proof
  rejects those exports rather than discarding records or switching APs.
- A SolveSpace wireframe export of
  `test/group/boolean_coplanar_union/normal.slvs` also failed strict reading,
  with dangling references and missing required relationship attributes.
  Exporter changes are outside this proof's scope.
- Upstream AP214 `dm1-id-214.stp` serializes conversion-unit dimensions (first
  failure: instance #25), but the selected AP214 E3 schema derives them and
  requires `*`. A matching base schema name alone is insufficient. The test
  fixture handles this difference explicitly without editing the EXPRESS schema.

## Reader validation and measured cost

The following results are from native **Windows, MSVC 2019 / 19.29, x86** builds
with **CMake 3.29.5** and Ninja. Debug uses `/MTd`; Release uses `/MT`, matching
SolveSpace. These results do not establish Linux/macOS, 64-bit, Visual Studio
generator, or sanitizer compatibility. Cross compilation is intentionally
rejected, as described above.

| Generator / configuration | Bindings | Validation result |
| --- | --- | --- |
| Ninja / Debug | AP203, AP214, AP242 | `test_step_import_nist`: 98 cases, 5176 checks, 0 failures, including the normal reader suite |
| Ninja / Release | AP203 only | `test_step_import`: 35 cases, 1727 checks, 0 failures; disabled schemas rejected |
| Ninja Multi-Config / Debug | AP203 only | `test_step_import`: 35 cases, 1729 checks, 0 failures |
| Ninja Multi-Config / Release | AP203 only | `test_step_import`: 35 cases, 1727 checks, 0 failures |
| Ninja / Release, proof left off | None | Solver-only `slvs` builds; no STEP proof targets exist |

The existing Debug GUI, CLI, testsuite and debugtool also build with the proof
enabled, and `test_solvespace` passes **302 cases, 1257 checks, 0 skipped**.
Their Ninja command graphs, and the standalone solver's, contain no STEPcode
or reader inputs. Unchanged single- and multi-config proof builds perform only
external-build checks/stamps, not binding generation or compilation.

Observed wall-clock costs for building the probe and reader testsuite:

| Build | Elapsed |
| --- | ---: |
| Clean AP203-only Release proof targets, including external STEPcode configuration but excluding parent SolveSpace configuration | 59.55 s |
| Unchanged AP203-only Release rebuild | 1.75 s |
| Unchanged all-three-binding Debug rebuild | 1.58 s |

These are individual local observations, not benchmark guarantees; no clean
all-three-binding build time was measured. The expanded Debug cleanup check
alone took 27.10 s in a traced run. The optional NIST command limits its test
process to 60 s; an attempt during simultaneous heavy compilation exceeded that
limit, while subsequent isolated runs passed.

Measured file sizes (bytes; schema archives exclude the reader and runtime):

| Artifact | All-three-binding Debug | AP203-only Release |
| --- | ---: | ---: |
| `solvespace-step-probe.exe` | 3,767,296 | 646,144 |
| `slvs-stepcode-AP203.lib` | 376,960 | 109,724 |
| `slvs-stepcode-AP214.lib` | 532,264 | Not built |
| `slvs-stepcode-AP242.lib` | 1,011,260 | Not built |

Debug and Release sizes are not directly comparable estimates of adding an AP.
These figures exclude PDBs and other build products and are not application
installation-size deltas. The validated dependency-only Debug probe imports
only `SHLWAPI.dll` and `KERNEL32.dll`; STEPcode and the MSVC runtime are linked
statically.

## Native planar conversion: first layer-2 slice

`src/step/stepgeometry.h/.cpp` consumes only the private `Document` API and
creates native geometry. STEPcode headers/types/macros do not enter this layer.
`solvespace-step-geometry` links the existing geometry core as a consumer; the
core, `slvs_deps`, standalone solver, GUI and CLI do not acquire a dependency on
the reader or converter. It is built under the same default-off proof option
when application geometry is available (`ENABLE_GUI` or `ENABLE_CLI`).
The dependency-only reader/probe still build with both application options off.
An AP203-only Release reader/solver build with both options off has also been
verified; it contains no native-conversion targets, and the solver command
graph contains no STEP inputs even while the reader proof is enabled.

With `ENABLE_CLI=ON` and `ENABLE_TESTS=ON`, run:

```sh
cmake --build build-step --target solvespace-step-geometry-probe test_step_geometry
build-step/bin/solvespace-step-geometry-probe path/to/cube.step
```

The native probe is separate from the dependency-only `solvespace-step-probe`.
It uses SolveSpace's Unicode-aware argument/path handling, reports converted
solid/surface/curve counts, length factors and bounds, and triangulates the
**already-created native shell** to report signed volume. It writes nothing.
Exit status is 0 for complete conversion, 1 for rejection/read failure, or 2
when some complete solids were omitted. This diagnostic triangulation is not
mesh conversion or a fallback. Geometry tests can be filtered by the substring
environment variable `SLVS_STEP_GEOMETRY_TEST_FILTER`.

### Accepted envelope

- Direct `SOLID_MODEL` items in 3D `SHAPE_REPRESENTATION` records, with one
  unambiguous representation per solid. The supported solid is a convex,
  genus-zero `MANIFOLD_SOLID_BREP` with a direct `CLOSED_SHELL`.
- `ADVANCED_FACE` on `PLANE`, with a single convex, simple `EDGE_LOOP` outer
  bound; `ORIENTED_EDGE` uses of `EDGE_CURVE` on `LINE`; `VERTEX_POINT` vertices.
- Exact linear/affine B-spline equivalents: non-rational
  `B_SPLINE_CURVE_WITH_KNOTS` of degree 1 with two controls, and
  `B_SPLINE_SURFACE_WITH_KNOTS` of degree (1,1) with a 2x2 affine control net.
  Each parameter direction must have two strictly increasing finite knots and
  endpoint multiplicities (2,2); closed/self-intersecting flags must be `.F.`.
  Knot intervals are normalized, not assumed to be (0,1). Edge endpoints and
  face boundaries must lie inside their original **finite** spline domains;
  an infinite supporting line/plane is not substituted to permit extrapolation.
- Explicit context length units: SI metre with prefixes, or positive typed
  conversion-based length measures resolving recursively to SI. All positions
  and sizes are converted to mm. Cycles, ambiguous/missing length units and
  inconsistent serialized dimensions are rejected. Plane/line conversion does
  not need angle units; angle-unit resolution remains for curved geometry.
- Actual plane positions/directions and EXPRESS optional-axis defaults, plus
  `EDGE_CURVE.same_sense`, `ORIENTED_EDGE.orientation`, face-bound orientation
  and `ADVANCED_FACE.same_sense`. Coordinates may describe rotated/translated
  geometry directly; this is **not** assembly-transform support.

The converter checks loop closure by source vertex IDs, planarity, line/vertex
consistency, winding, face/solid convexity, two opposing face uses per edge,
connectedness, Euler characteristic, single-cycle vertex links, distinct
vertices, and positive non-degenerate enclosed volume. It does not weld,
project/heal, relax tolerance from STEP uncertainty, repair winding, or infer
units. Tests verify the native UV classification as well as triangulation,
because native triangulation alone can normalize and hide reversed trims.

All validation precedes constructing a solid's native shell. Native line
curves use degree-1 exact `SBezier`s with shared endpoints, incident surfaces
and two-point trim PWLs; planar patches use bounded degree-(1,1) `SSurface`s.
Rectangular single-span spline patches retain their four source controls
directly, with parameter axes transposed when face orientation requires it.
Skew affine patches are exactly reparameterized onto orthogonal planar support
because the current native planar closest-point fast path assumes orthogonal
axes. Their source-domain checks still use the original control net; their
trimmed geometric regions and boundaries do not change. No native geometry
kernel code was changed for this accommodation. Tests check native UV-to-XYZ
round trips at edge midpoints and face interiors, not just corner positions.
Affine recognition allows only floating-point-roundoff-sized mixed terms,
capped at 1e-8 mm, rather than fitting at `LENGTH_EPS`. Non-affine bilinear
patches, including coplanar trapezoidal and slightly warped nets, remain
unsupported. Higher degrees, multiple spans and rational splines are also
unsupported; no controls, knots or weights are silently discarded.
The native trim direction is explicitly reversed from the effective STEP outer
bound. Successful solids own their geometry independently of `Document`;
destruction also clears the native raw-list storage.

The initial safety envelope is fixed `LENGTH_EPS` (1e-6 mm), coordinates within
`+/-1e8 mm`, and at most 512 faces, 2048 edges and 1024 vertices per solid.
Convexity/validation checks are intentionally bounded and not optimized for
large models. This is a conservative geometry proof, not comprehensive STEP
geometric validation or parser hardening.

### Omissions and source identities

Unsupported or invalid solids produce diagnostics carrying representation,
solid and offending entity IDs. The result is `Okay`, `Partial`, or `Rejected`;
with no usable solid it is always `Rejected`. An unreferenced solid is omitted
with a diagnostic rather than guessed into an arbitrary unit context.

Holes, **all of `BREP_WITH_VOIDS`**, curved geometry, non-convex shapes and other
unsupported solids are rejected in their entirety. No faces or cavity shells
are retained from a rejected solid. Representation relationships (even
untransformed ones), mappings, assembly usage, duplicate occurrences and
unsupported non-solid shape items currently reject the entire conversion:
until selection and transforms exist, loading components at their definition
origins would be unsafe. These conservative restrictions are intentional, not
reasons to change schemas or discard unknown STEP records in the reader.

Each `NativeSolid` records its representation/context/solid/outer-shell IDs,
length factor, source vertex and Cartesian-point IDs with mm positions,
source edge and curve-geometry IDs mapped to `hSCurve` with endpoints, and
source face and surface-geometry IDs mapped to `hSSurface` with outward plane
normals. Native curve/surface handles
use positive source instance IDs local to that solid, sorted independently of
source record order. They are not globally unique across files/occurrences or
stable semantic names across arbitrary re-exports. The later link layer must
namespace/remap them and allocate actual sketch `hEntity` references. No source
IDs are masqueraded as sketch entities in `SSurface.face` or `SBezier.entity`.

### Native validation

On the same MSVC 2019 x86 toolchain with Ninja, the standalone native suite
passes **213 cases, 24321 checks** across all three bindings in Debug and
**71 cases, 8105 checks** with AP203 only in Release. Its independent synthetic
fixtures cover a 10 mm cube (6 surfaces, 12 exact curves, 8 source vertices,
1000 mm^3, 600 mm^2), a tetrahedron, SI metre and nested inch/foot conversions,
rotations/translations, orientation flags, multiple and partially convertible
solids, rejection boundaries, source-record reordering, and reader-independent
geometry lifetime. Converted overlapping cubes also pass native shell Boolean
union with the expected 1500 mm^3 volume. Repeated successful and failed native
conversion, including linear/affine spline geometry, has no growth in
outstanding normal CRT blocks/bytes after warm-up.
The tests' triangle mesh is only a verification of native output; no mesh is
returned by the converter.

### Independent OCCT cube exports

The user-provided **Open CASCADE STEP processor 7.6** file
`build/bin/OpenCascade_1mmCube_AP2142DIS.step` converts **without modification**:
202 parsed instances, one solid, 6 native surfaces, 12 exact curves and 8 source
vertices, with 1x1x1 mm bounds, 1 mm^3 volume and 6 mm^2 area. Its schema is
`AUTOMOTIVE_DESIGN { 1 2 10303 214 0 1 1 1 }`; acceptance of this file is not a
blanket claim about every AP214 draft/edition. The file uses the single-span
linear/affine B-spline encodings described above. SHA256:
`f7cdf121cca6ff7f12bcdf8d1f0667adf24c62e2f06f5979df694d0b26665382`.

The native suite can explicitly test an externally supplied origin-aligned
1 mm cube without vendoring it or making ordinary tests depend on a local
build-tree file:

```sh
build/bin/solvespace-step-geometry-testsuite --cube-1mm build/bin/OpenCascade_1mmCube_AP2142DIS.step
```

This runs the normal suite plus an external-cube check. With the substring
filter `SLVS_STEP_GEOMETRY_TEST_FILTER=external/`, the external check alone passes
**1 case, 299 checks**: units/bounds, topology, provenance, UV classification,
native triangulation, area/volume, and output lifetime after destroying the
reader. A native Boolean union with a copy shifted by 0.5 mm also succeeds,
with the expected 1.5 mm^3 volume and 8 mm^2 area. All geometry exists as native
shells before this diagnostic triangulation; the fixture is only read.

The additional SolveSpace **3.3 solid export resaved by OCCT**,
`build/bin/OpenCascade_1mmCube33_AP2142DIS.step`, also converts unchanged, with
the same native counts, bounds, volume and area. SHA256:
`40ec14bb315618d9d711da3df773f5c55ee73abfc94732a47b764f2b770acfac`.
The supplied original `build/bin/1mmCube.slvs` has SHA256
`182428f119db340e26b4e1cc5b466e219adbadca87ad19b5acd62ca19f7ffea5`.
An optional source comparison is available:

```sh
build/bin/solvespace-step-geometry-testsuite --cube-1mm build/bin/OpenCascade_1mmCube33_AP2142DIS.step --source-slvs build/bin/1mmCube.slvs
```

With the `external/` filter this passes **1 case, 351 checks**. It regenerates
the source sketch in memory through the existing headless test path and
compares its native shell, not serialized cached geometry: 8 vertices,
12 edge segments, 6 outward supporting planes, bounds, volume and area all
match the converted STEP solid. All source sketch groups solve, and both solids
remain native shells; the STEP cube also passes the Boolean-union check above.
No STEP re-export, fixture save/upgrade, or new import/linking sketch entities
are involved.
The `.slvs` file and the OCCT resave remain unchanged and are not vendored.

The other supplied variants remain explicit reader compatibility limits:

- `build/bin/OpenCascade_1mmCube_AP203.step` declares
  `FILE_SCHEMA(('CONFIG_CONTROL_DESIGN','SHAPE_APPEARANCE_LAYER_MIM'))`.
  Multiple schema identifiers are outside the current single-registry contract;
  no identifier or extension records are discarded. SHA256:
  `a08f22885b1e53fd9d9b3271650f606271a8f4db4a1903d01a1f3093aff6eb94`.
- `build/bin/OpenCascade_1mmCube_AP2242DIS.step` declares
  `AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF. {1 0 10303 442 1 1 4 }`.
  The trailing period is part of its schema base name as currently parsed; it
  does not match the compiled AP242 binding. It is not silently stripped or
  aliased. SHA256:
  `976ba17f84ca586f8755ab84784c42137a418e99dc3e2e77e4757bfa54f11837`.

The original nominal 1 mm cube was also probed without modification:
`build/bin/OpenCascade_1mmCube.step`, SHA256
`cb9366f2f96c5893a723aae2f407ff2ae22d78391ed70bebf25d1a2d6f8f7cf8`.
It remains **unsupported**, not classified as an invalid cube: it declares
`AUTOMOTIVE_DESIGN_CC2 { 1 2 10303 214 -1 1 5 4 }`, not the selected AP214 E3
schema. Its degree-1 `B_SPLINE_CURVE_WITH_KNOTS` edges and degree-(1,1)
`B_SPLINE_SURFACE_WITH_KNOTS` faces now fit the converter's narrow envelope,
but are never reached for that unsupported schema declaration.
That original file is rejected at schema selection, before native conversion;
no successful native conversion of that original file is claimed. It has not
been relabelled, aliased to another schema, modified or vendored. The unsupported
`AUTOMOTIVE_DESIGN_CC2` base name and the AP203/AP242 header limits have explicit
reader regression tests. None of the supplied fixtures was edited, relabelled
or vendored. These are support limits, not claims that the cubes are invalid.
Using an OCCT-produced exchange file as a fixture does not introduce an OCCT
library dependency.

## Remaining native conversion and linking work

Keep geometry conversion separate from reader and linking work. Remaining gates
include broader independent real-exporter geometry coverage, portable builds
and parser hardening; representation/assembly selection and transforms; angle
units and broader length-unit layouts; concave/holed faces and valid cavity conversion;
exact analytic and rational B-spline curve/surface conversion and trimming;
production tolerances and geometric validation; actual stable constraint
reference entities; reload/serialization/undo behavior; and GUI-safe diagnostics.
The export/schema mismatch recorded above remains deferred and untouched.
