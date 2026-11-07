# Experimental native STEP linking

The application can now link `.step` / `.stp` through **New Group → Link /
Assemble…**, when built with `ENABLE_STEP_IMPORT=ON`. One file becomes one
immutable linked group with native `SShell` solids and real selectable vertex,
straight-edge and planar-face constraint references. Existing linked-group
translation, quaternion rotation and uniform scale provide placement. No feature
history is recovered and STEP data is not edited.

The native converter accepts direct B-reps on planes and generic rational or
non-rational B-spline curves/surfaces of degree 1–3, including exact multi-span
decomposition, curved trims, concave faces and inner loops. Support is based on
the native `SBezier`/`SSurface` representation, not cylinder or extrusion
recognition. **No OCCT/Open Cascade library, wrapper or automatic mesh fallback
is used.** Unsupported encodings or unsafe trimming produce entity-specific
diagnostics; this is not blanket support for every STEP encoding of an otherwise
representable shape. Partial conversion
may omit **whole unsupported solids**, never individual faces or cavity shells.
Omissions are reported; a new link with no usable solid is refused. Parse errors
still reject the entire document. Assemblies/mapped occurrences inside STEP
remain unsupported, even though multiple complete direct solids can be linked
in a single group.

## Build and run

Both `ENABLE_STEP_IMPORT` and `ENABLE_STEP_IMPORT_PROOF` default to `OFF`.
Use a native build with the existing SolveSpace prerequisites; no additional
lexer/parser generator or geometry kernel is required.

```sh
git submodule update --init extlib/stepcode
cmake -S . -B build-step -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_STEP_IMPORT=ON -DENABLE_STEP_IMPORT_PROOF=ON -DENABLE_TESTS=ON \
    "-DSTEP_IMPORT_SCHEMAS=AP203;AP214;AP242"
cmake --build build-step --target solvespace solvespace-step-probe test_step_import test_step_geometry test_step_link
build-step/bin/solvespace-step-probe path/to/model.step
```

For reader/native proofs **without application linking**, enable only
`ENABLE_STEP_IMPORT_PROOF`. Reader-only builds still work with both GUI and CLI
off. No STEP dependency is added to the standalone solver in either mode.

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
  before strict reading rejects them. Build-tree runtime/header mirrors route
  C++ console diagnostics through the private thread-local sink in
  `cmake/stepcode/diagnostics.*`, including inline/default-argument output.
  No application standard-stream buffers are replaced.
- `cmake/stepcode/GenerateSchema.cmake`: split generated complex-name string
  literals at encoded NULs to satisfy MSVC's per-literal limit. Bytes and table
  offsets are unchanged; the pinned tables fit its concatenated-literal limit.
- Private per-binding definitions rename the four otherwise-global entry
  points `SchemaInit`, `InitSchemasAndEnts`, `gencomplex`, and `GetModelContents`.

Review these contracts and rerun the tests whenever the pin changes.
The BSD notices, including generated-code coverage, are in `THIRD_PARTIES.txt`
and the upstream `COPYING`, `AUTHORS` and `INTENT.md` files.

Cross compilation is deliberately rejected when either STEP option is enabled:
a target `exp2cxx` cannot safely be executed as a host tool. A production build
needs an explicit host-tools build plus target runtime, or reproducibly
pre-generated bindings with the generator/schema versions recorded. Neither path is silently
selected or implemented here. Keep both options off for those builds.

## Reader boundary and limitations

Only `src/step/stepreader.cpp` and private binding targets consume STEPcode
headers, macros, types and ownership. Nothing is added to `slvs_deps`,
`slvs-solver` or `slvs-interface`. With `ENABLE_STEP_IMPORT=ON`, the application
core privately links the reader and includes the native converter's object
files. With only the proof option, application executables stay independent of
the reader/converter. The reader probe/tests include only `stepreader.h`.

`Document` owns its registry and instances and returns value snapshots. It can
query inheritance, complex components, references, primitive/SELECT values and
nested aggregates needed for topology, units and rational splines. References
use Part 21 instance IDs. Strings retain STEP quoting/escapes; opaque values are
explicitly not decoded. This is not a complete SDAI wrapper or public solver API.

The adapter uses STEPcode's header and two data passes on the caller's seekable
stream, rather than its public `AppendFile()` path, which reopens a narrow
filename for pass two. It sets the classic stream numeric locale for parsing
and restores the caller's locale. The CLI probe still uses ordinary narrow
command-line filenames; application linking uses SolveSpace's `Platform::Path`
and Unicode-aware `ReadFile`, including Unicode/spaced filenames on Windows.

The GUI reads and converts synchronously, then destroys the `Document` before
caching native output. A recursive mutex serializes all adapter entry into
STEPcode, including queries and destruction, because the pinned runtime has
global descriptors/token helpers. Its console diagnostics use a private
thread-local stream, capped to 64 KiB per read. Tests observe the application's
`std::cout`/`std::cerr` during parsing (not just afterwards) and exercise concurrent
reads, snapshot queries and destruction. This containment is not an asynchronous
import service or a hardened parser for arbitrary untrusted files. Resource
limits, cancellation, exception-safety auditing and sanitizer/fuzz testing remain
work. Registry/instance lifetime, header destruction order and EOF handling are
contained by the adapter/overlay.

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

These are the original proof-layer measurements, before the linked-group and
diagnostic-containment changes. Current integration results are recorded below.

| Generator / configuration | Bindings | Validation result |
| --- | --- | --- |
| Ninja / Debug | AP203, AP214, AP242 | `test_step_import_nist`: 98 cases, 5176 checks, 0 failures, including the normal reader suite |
| Ninja / Release | AP203 only | `test_step_import`: 35 cases, 1727 checks, 0 failures; disabled schemas rejected |
| Ninja Multi-Config / Debug | AP203 only | `test_step_import`: 35 cases, 1729 checks, 0 failures |
| Ninja Multi-Config / Release | AP203 only | `test_step_import`: 35 cases, 1727 checks, 0 failures |
| Ninja / Release, proof left off | None | Solver-only `slvs` builds; no STEP proof targets exist |

Before application integration, the Debug GUI, CLI, testsuite and debugtool built
with the proof enabled, and `test_solvespace` passed **302 cases, 1257 checks,
0 skipped**.
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

## Native exact B-rep conversion

`src/step/stepgeometry.h/.cpp` consumes only the private `Document` API and
creates native geometry. STEPcode headers/types/macros do not enter this layer.
`solvespace-step-geometry` is an object target. Proof executables consume its
objects alongside the existing headless core; with application linking enabled,
the objects instead enter `solvespace-core` and clients use that core. This avoids
a core/converter static-library cycle. It is available under either default-off
STEP option when application geometry is available (`ENABLE_GUI` or `ENABLE_CLI`).
The dependency-only reader/probe still build with both application options off.
An AP203-only Release reader/solver build with both options off has also been
verified; it contains no native-conversion targets, and the solver command
graph contains no STEP inputs even while either STEP option is enabled.

With `ENABLE_CLI=ON` and `ENABLE_TESTS=ON`, run:

```sh
cmake --build build-step --target solvespace-step-geometry-probe test_step_geometry
build-step/bin/solvespace-step-geometry-probe path/to/model.step
```

The native probe is separate from the dependency-only `solvespace-step-probe`.
It uses SolveSpace's Unicode-aware argument/path handling, reports converted
solid/surface/curve counts, length factors and bounds, and triangulates the
**already-created native shell** to report signed volume. It writes nothing.
Exit status is 0 for complete conversion, 1 for rejection/read failure, or 2
when some complete solids were omitted. This diagnostic triangulation is not
mesh conversion or a fallback. Geometry tests can be filtered by the substring
environment variable `SLVS_STEP_GEOMETRY_TEST_FILTER`.

### Accepted envelope and exact construction

- Direct `SOLID_MODEL` items in 3D `SHAPE_REPRESENTATION` records, with one
  unambiguous representation per solid: `MANIFOLD_SOLID_BREP` with a direct,
  connected, orientable `CLOSED_SHELL`. Concave solids and through-holes are
  supported; separate cavity shells are not.
- `ADVANCED_FACE` with one simple outer `EDGE_LOOP` and optional disjoint inner
  loops; `ORIENTED_EDGE` uses of `EDGE_CURVE`; `VERTEX_POINT` vertices. The same
  source edge cannot be used twice on one face in the current trimming path.
- `LINE` and `PLANE`, plus rational/non-rational `B_SPLINE_CURVE_WITH_KNOTS`
  and tensor-product `B_SPLINE_SURFACE_WITH_KNOTS`. Degrees 1–3 are supported in
  each parameter direction, including nonuniform, non-clamped finite domains
  and multiple spans. Standard `BEZIER_*`, `UNIFORM_*` and `QUASI_UNIFORM_*`
  entities supply their defined implicit knots. A bare spline with no implemented
  knot definition is not guessed. Complex rational entities retain their weights.
- Explicit finite, strictly increasing distinct knots and valid multiplicities,
  consistent control counts, rectangular surface nets and finite positive
  rational weights. Closed flags are checked against finite boundary geometry;
  unknown logical flags do not bypass geometry validation. A declared
  self-intersection is rejected. Ambiguous parameter branches are not guessed.
- Explicit context length units: SI metre with prefixes, or positive typed
  conversion-based length measures resolving recursively to SI. Positions and
  sizes are converted to mm. Cycles, ambiguous/missing length units and
  inconsistent serialized dimensions are rejected. The implemented analytic
  line/plane and Cartesian spline paths do not need angle units.
- Actual plane positions/directions and EXPRESS optional-axis defaults, plus
  `EDGE_CURVE.same_sense`, `ORIENTED_EDGE.orientation`, face-bound orientation
  and `ADVANCED_FACE.same_sense`. Coordinates may describe rotated/translated
  geometry directly; this is **not** assembly-transform support.

`src/step/stepnurbs.h/.cpp` performs homogeneous knot insertion and exact
decomposition into rational Bézier spans/patches. An unchanged clamped single
span retains its serialized controls and weights directly, without normalizing
their common weight scale. Edge restriction, orientation reversal and shared
boundary splits are exact rational-polynomial operations, not fitting. Surface
parameter axes are transposed when face orientation requires it. Source
boundaries are split at native patch seams and shared by both incident patches.
Internal knot seams are implementation details, not new source edges/vertices.

Edge vertices and sampled face boundaries must lie on their original **finite**
spline domains; an infinite supporting line/plane is not substituted to permit
extrapolation. Projection is used to find/check parameters, never to move source
geometry onto a surface. There is no welding, healing, flattening, degree fitting,
polygonal replacement, uncertainty-based tolerance relaxation or inferred unit.

The native kernel now distinguishes geometric planarity (`IsPlanar`) from an
affine, constant-weight UV map (`IsAffine`). Skew affine inversion uses dual
directions; weighted or non-affine maps use the native numerical inverse.
Raycasting, intersections, coincident-plane handling and triangulation no longer
assume that degree (1,1) implies a plane. A warped bilinear patch remains curved;
a rational or higher-degree plane remains planar. Recognition is limited to
floating-point roundoff rather than flattening at model tolerance. Imported nets
and weights stay intact. Boolean classification refines its probe point onto
every exact trim, including a straight XYZ line with a nonlinear UV image.
Ordinary Boolean output may reparameterize a genuinely
planar result, as the native coincident-plane merger already does.

### Validation, settings and bounded numerical envelope

Validation covers source-ID loop closure, boundary/surface consistency, regularity
samples, effective UV winding, self-crossing/touching loops, hole containment,
two distinct opposing incident faces per source edge, connectedness, genus-aware
Euler characteristic, single-cycle half-edge vertex links and distinct source
vertices. Native construction is followed by directed trim assembly, complete
trimmed-UV area coverage, oriented nondegenerate triangles, positive enclosed
volume and bounded triangle-intersection checks. Tests also verify native UV BSP
classification: triangulation alone can normalize and hide reversed trims.

Native curve PWLs support trimming, display and Booleans; validation tessellation
is only a check of the already-created native shell. **Neither is returned as
replacement geometry.** Every accepted converter curve remains an exact
`SBezier`. Rejected solids are destroyed completely, including partially
allocated native lists, and accepted solids outlive their reader `Document`.
These bounded numerical checks are not an exhaustive mathematical proof of
embeddedness or comprehensive STEP geometric validation.

`GeometryOptions` makes conversion independent of the current sketch's display
or export tessellation. By default, trim/validation chord tolerance is 0.1% of
the boundary-control bounding-box diagonal, bounded to `[32*LENGTH_EPS, 0.01]`
mm, with `maxSegments=128`. Explicit diagnostic options can request finer
measurement. The converter temporarily sets and restores the native global
tessellation settings on success **and rejection**. Conversion remains
synchronous, not a thread-safe native import service.

The envelope uses fixed `LENGTH_EPS` (1e-6 mm), coordinates within `+/-1e8 mm`,
and at most 512 source faces, 2048 edges and 1024 vertices per solid. There are
limits of 4096 controls per curve/control net, 4096 patches and 32768 native
curve spans per solid, 16384 trim-validation/PWL points per loop/curve, and
32768 validation triangles. Subdivision/intersection work is bounded separately.
Positive weights must be in `[1e-100,1e100]` with ratio at most `1e12`; knots
that collapse under native parameter normalization are explicitly refused.
Exceeding an envelope/work limit reports unsupported conversion, not silent
simplification. Production tolerances, large-model optimization and parser
resource hardening remain work.

### Omissions and source identities

Unsupported or invalid solids produce diagnostics carrying representation,
solid and offending entity IDs. The result is `Okay`, `Partial`, or `Rejected`;
with no usable solid it is always `Rejected`. An unreferenced solid is omitted
with a diagnostic rather than guessed into an arbitrary unit context.

**All of `BREP_WITH_VOIDS`**, degrees above three, singular/unresolvable patches,
ambiguous periodic/pcurve trims and unimplemented entity encodings are rejected
in their entirety. Analytic `CIRCLE`, `ELLIPSE`, `CYLINDRICAL_SURFACE` and other
analytic curved-surface entities, and curve/surface wrappers without a conversion
path, remain unsupported even when an equivalent rational B-spline encoding
could be represented. The cylinder acceptance fixture below already uses generic
rational B-splines and needs no analytic whitelist or angle-unit workaround.
No faces or cavity shells are retained from a rejected solid.
Representation relationships (even
untransformed ones), mappings, assembly usage, duplicate occurrences and
unsupported non-solid shape items currently reject the entire conversion:
until selection and transforms exist, loading components at their definition
origins would be unsafe. These conservative restrictions are intentional, not
reasons to change schemas or discard unknown STEP records in the reader.

Each `NativeSolid` records its representation/context/solid/outer-shell IDs,
length factor, source vertex and Cartesian-point IDs with mm positions,
source edge and curve-geometry IDs with endpoints and source-face adjacency,
and source face/surface-geometry IDs with effective source boundary loops.
An edge/face can map to multiple native curve/surface handles; planar faces
additionally record an outward supporting-plane normal. The first retained
span/patch uses its positive source instance ID; extra native spans/patches use
deterministic local handles after the source range. They are independent of
source record order, but are not source identities or new constraint references.
They are not globally unique across files/occurrences or
stable semantic names across arbitrary re-exports. The converter leaves
`SSurface.face` / `SBezier.entity` unset. The application link layer, described
below, allocates actual sketch entity identities and binds them to this geometry;
source IDs are never passed to picking as fake sketch handles.

### Native validation

Independent synthetic fixtures cover cubes, tetrahedra, SI metre and nested
inch/foot conversions, transformations/orientation flags, multiple/partially
convertible solids, source-record reordering and reader-independent geometry
lifetime. Generic additions cover rational compound-cubic patches that are not
extrusions, rational quadratic/cubic non-extruded lofts, nonuniform multi-span
nets, curved trims crossing a patch grid, nonplanar bilinear patches, concave
faces/solids, through-holes and native Boolean unions. Independent Cox–de Boor
evaluation checks curve and tensor-product knot decomposition, including
non-clamped domains. Monotone collinear cubic references are recognized from
the actual rational curve, not control-point order; controls/weights are retained.

Malformed net/weight/knot cases, numerical/degree limits, conversion independence
and restoration under empty/extreme scene settings, and repeated successful and
failed conversion are tested. MSVC Debug checks outstanding normal CRT
blocks/bytes after warm-up; this is not sanitizer coverage. New baseline kernel
regressions exercise skew/rational/non-affine inversion, planarity, raycasting,
straight-trim Boolean probe points and bilinear triangulation independently of
STEP. Current measured results are recorded with the integration checks below.

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

This runs the normal suite plus an external-cube check. Before application
integration, with the substring filter `SLVS_STEP_GEOMETRY_TEST_FILTER=external/`,
the converter check alone passed **1 case, 299 checks**: units/bounds, topology,
provenance, UV classification, native triangulation, area/volume, and output
lifetime after destroying the reader. A native Boolean union with a copy shifted by 0.5 mm also succeeds,
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

The current `external/` filter passes **2 cases, 595 checks** with application
linking enabled. The converter/source comparison regenerates
the source sketch in memory through the existing headless test path and
compares its native shell, not serialized cached geometry: 8 vertices,
12 edge segments, 6 outward supporting planes, bounds, volume and area all
match the converted STEP solid. All source sketch groups solve, and both solids
remain native shells; the STEP cube also passes the Boolean-union check above.
No STEP re-export or fixture save/upgrade is involved. With application linking
enabled, an additional `external/link_cube_1mm` case exercises the real link menu,
generated reference entities and vertex/edge/face picking on the unchanged STEP.
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
`B_SPLINE_SURFACE_WITH_KNOTS` faces now fit the converter's native envelope,
but are never reached for that unsupported schema declaration.
That original file is rejected at schema selection, before native conversion;
no successful native conversion of that original file is claimed. It has not
been relabelled, aliased to another schema, modified or vendored. The unsupported
`AUTOMOTIVE_DESIGN_CC2` base name and the AP203/AP242 header limits have explicit
reader regression tests. None of the supplied fixtures was edited, relabelled
or vendored. These are support limits, not claims that the cubes are invalid.
Using an OCCT-produced exchange file as a fixture does not introduce an OCCT
library dependency.

### Independent rational 5 mm cylinder acceptance

The unchanged user-provided `build/bin/OpenCascade_5mmCylinder.step`, from
**Open CASCADE STEP processor 7.6**, declares
`AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }`. Its walls and edges are generic
rational B-splines, not analytic `CYLINDRICAL_SURFACE`/`CIRCLE` entities.
The same conversion path as the free-form fixtures produces **6 native
surfaces, 12 exact curves and 8 source vertices**, retaining the serialized
quadratic weight **0.7071067812** and the original control positions.
SHA256: `9233fb1d2dbc7957c7677a909c824fd4b98ae3bfc825d672024708f3742f4151`.

The supplied original `build/bin/5mmCylinder.slvs` is independently regenerated
in memory for comparison, not read as cached exchange output or re-exported.
Its SHA256 is `cb6e56e9c9381713a6669cbeb31e4559b6ef415d467579832873e3756ec5e3ae`.
Neither file is saved, upgraded, modified or vendored by the tests.

```sh
build/bin/solvespace-step-geometry-testsuite --cylinder-5mm build/bin/OpenCascade_5mmCylinder.step --source-slvs build/bin/5mmCylinder.slvs
```

With `SLVS_STEP_GEOMETRY_TEST_FILTER=external/`, the converter/source and real
linked-group cases pass **2 cases, 10917 checks**. Checks include source vertices,
sampled edge/surface agreement and outward normals, native topology and UV
classification, bounds `(-2.5,-2.5,0)`–`(2.5,2.5,5)` mm, and area/volume near
`37.5*pi` mm² / `31.25*pi` mm³. Measurement uses explicit `1e-4` mm chord
tolerance and 2048 segments; the mesh is only a measurement of native geometry.
Native union with an overlapping translated copy is checked against the
independent disk-union area/perimeter formulas.

The actual link menu, placement, point/line/plane constraints, save/reopen and
explicit reload are exercised. This file supplies **14 truthful constraint
references: 8 points, 4 straight edges and 2 planes**. Curved wall surfaces remain
native surfaces, not selectable constraint planes. Rational curved edges remain
exact curves, not invented circle/radius references. Artificial seams, where
another file needs native patch splitting, likewise add no source references.

## Linked groups and reference identities

`src/importstep.cpp` bridges the owned reader/native converter into the existing
`Group::Type::LINKED`, not a new editable STEP group type. The group defaults to
`ASSEMBLE`. Complete native solids are concatenated into one shell with unique
curve/surface handles, rewritten trims and incident-surface associations. No
Boolean, tessellation or mesh conversion is used to build this linked shell.
Normal native Booleans remain available as subsequent group combinations.

Stored numerical entities are created for every accepted source vertex, linear
edge and planar face. A face has a real boundary vertex as its associated point
and an outward unit normal. `Group::CopyEntity` generates the placed points,
segments and `FACE_N_ROT_TRANS` entities; the shell's face IDs and exact Bezier
edge IDs are remapped to those **actual** sketch handles. Display triangles carry
the face handles for ordinary face picking; the triangles are display output,
not a replacement for the shell. Face normals remain unit length under uniform
scale (including orientation reversal for a negative scale).

Only geometrically eligible source edges/faces receive line/plane references.
An arbitrary rational curve is not a sketch cubic or circle, and a curved wall
is not a plane. Such geometry remains in the native shell with zero reference
handles instead of a chord, tangent plane or seam masquerading as source
geometry. All native spans of one genuine straight source edge share its one
line reference; all planar patches of one source face share its plane reference.

Edge remapping only uses mappings already created by entity generation; native
extrusions may retain their generating sketch curve IDs, which must not allocate
unrelated entries in a step-and-repeat group's persistent remap table.

Reference provenance includes representation, solid, kind and source instance
IDs, plus **exact source connectivity**, not a coordinate hash. Each linked
group saves its own monotonically allocated source-token table in
`Group.stepRefs`; ordinary `Group.remap` supplies the final group namespace.
Unchanged reloads and reordered source records preserve references. Geometry can
resize/move while retaining its topology. Removed source identities, or IDs
repurposed with different connectivity, are not recycled or silently retargeted.
The importer reports missing previous identities, and SolveSpace's normal
regeneration prunes dependent sketch elements. The 65535-entry group/remap limit,
including retired identities, is checked before generation; exceeding it refuses
the link rather than asserting. This is **not** semantic naming across arbitrary
STEP re-exports/renumbering; such exports may break references conservatively.
Native split counts and artificial seams are excluded from provenance. A source
face becoming curved, or a source straight edge becoming curved, retires the
inapplicable plane/line reference without retargeting its constraints. Other
unchanged source identities, including its real vertices, remain stable.

The separate `linkedStepFiles` cache contains exact source bytes, owned native
geometry, stored entities, provenance and diagnostics, including failed reads.
It never contains a vendor document or a null editable `Sketch`. Explicit reload
checks content; normal generation/dragging reuses the cache without reopening or
parsing the source. Each linked instance receives its own deep native copy and
reference namespace. Nested `.slvs` links retain STEP content in their existing
dependency-invalidation tracking. Cache pruning, clearing and undo/reopen use the
existing link lifecycle.

Readable but invalid/unsupported STEP is distinguished from a missing file and
never falls through to the `.slvs` loader or the missing-file relocation dialog.
New unusable links do not create an empty group or change the undo stack.
Whole-solid omissions, conversion rejection and changed references are reported
only at explicit loading/reloading, after the sketch is safe to repaint, not on
every generation/drag. The property browser identifies STEP as read-only and
shows accepted/omitted solid counts. If an **existing** source becomes unusable,
no stale/partial face geometry is silently substituted: the group has no imported
geometry, and normal dependency pruning applies. Keep backups for lifecycle
trials, especially tests that deliberately remove/change source topology.

### Headless integration checks and GUI trial

`test_step_link` runs the `link/` subset of the native testsuite. Tests use fresh
synthetic files, not edits of user-supplied STEP fixtures. They cover the real
`Group::MenuGroup(Command::GROUP_LINK, path)` entry point; Unicode/spaced paths and
mixed-case `.stp` dispatch; transformed/scaled numerical and expression references;
actual point/edge/face picking; point coincidence, point-on-line, point-on-face
and face-parallel solving; constraining another linked instance's placement;
complete multiple solids; native Boolean unions; whole-solid omission/rejection;
cache reuse; missing/changed reference identity; bounded handles; nested `.slvs`
invalidation; and save/reopen/undo/redo smoke coverage for constrained references.
Curved tests additionally verify truthful zero handles, scaled/rotated native
placement, unchanged identities across knot/patch splitting, and constraint
retirement when planes or straight edges become curved.
The two existing menu paths used by these tests tolerate missing headless menu
items/windows; interactive behavior is unchanged. A Unicode group-name character
classification bug exposed by these path tests is also fixed.

Current integration validation on Windows/MSVC 2019 x86 (Ninja, including
Ninja Multi-Config):

| Configuration | Result |
| --- | --- |
| All-binding Debug, import enabled | GUI, CLI, debugtool, probes and standalone solver build. Full native/link suite with the unchanged cylinder STEP and original source `.slvs`: **328 cases, 81311 checks, 0 failures**. |
| All-binding Debug, linked-group subset | The `link/` subset used by `test_step_link`: **53 cases, 11007 checks, 0 failures**. |
| All-binding Debug, reader plus optional NIST corpus | **100 cases, 5184 checks, 0 failures**, including **99 cases, 5165 checks** in the normal dependency-only reader suite. The optional pinned NIST corpus passed in an isolated run after compilation. |
| All-binding Debug, existing SolveSpace suite plus kernel regressions | **308 cases, 1696 checks, 0 skipped**; original test fixtures are unchanged. Focused native raycast/inverse/trim regressions: **8 cases, 445 checks**. |
| All-binding Debug, cube33 and original source sketch | External converter/source and real linked-group checks: **2 cases, 595 checks, 0 failures**. |
| All-binding Debug, other working AP214 cube | External converter and real linked-group checks: **2 cases, 543 checks, 0 failures**. |
| AP203-only Release, import enabled and proof option off | CLI, native probe/tests, reader tests and standalone solver build. Native/link suite: **114 cases, 25902 checks, 0 failures**. Reader suite: **37 cases, 1735 checks, 0 failures**; disabled bindings are rejected. |
| AP203-only Ninja Multi-Config, proof on and import off | Debug reader/native suites: **37 cases, 1737 checks** / **95 cases, 22229 checks**. Release: **37 cases, 1735 checks** / **95 cases, 22225 checks**. All pass; CLI, probes and standalone solver build in both configurations. Both CLI command graphs contain no reader/converter/vendor inputs. |
| Application options off, import/proof enabled | Reader probe and standalone solver build; no native-conversion targets. Solver command graph has no STEP inputs. |
| Both STEP options left off | Standalone solver builds with no STEP inputs. |

The integrated application's standalone solver command graph is also free of
STEP inputs, in both Debug and AP203 Release. All eight checked solver/CLI
command graphs are isolated from the reader, converter and STEPcode as required.
GCC 14.1 C++11 syntax checks pass for the changed production sources, probe and
tests with later-standard extensions treated as errors; this is **not** a full
portable build or sanitizer run. User fixture hashes listed above were rechecked
unchanged, as was the pinned STEPcode checkout. These are headless/build checks,
not completion of the broader interactive lifecycle trial.

The 60-second NIST test should be run after heavy compilation; a concurrent
attempt previously exceeded that limit. The final isolated run passed using the
existing hash-verified archive, with no download needed for this validation.

For the interactive trial, run `build/bin/solvespace.exe`, create a new sketch,
choose **New Group → Link / Assemble…**, select **STEP file (native solids)** and
open `OpenCascade_5mmCylinder.step` (or `OpenCascade_1mmCube33_AP2142DIS.step`).
For the cylinder, expect the 14 references above; no radius constraint on an
invented circle or plane constraint on the curved wall is offered.
Use the normal vertex/edge/face selection and constraint commands. Save a **new**
containing `.slvs`, reopen it, and try undo/redo and missing/changed source handling
on copies. Reload sources with **Edit → Regenerate All (Space)**, with the final
group active for downstream updates; ordinary dragging does not watch files.
Do not automatically resave the existing `build/bin/StepLinkTest.slvs` trial.
The earlier cube GUI trial was user-confirmed; the refreshed curved support
still needs an interactive trial. Headless smoke coverage does not replace it.
Do not use the deferred AP203/AP242/header-incompatible variants
as successful-conversion expectations; their explicit limitations remain above.

## Remaining native conversion and production work

Keep geometry conversion separate from reader and linking work. Remaining gates
include more independent real-exporter coverage, portable full builds and
parser/sanitizer hardening; representation/assembly selection and transforms;
analytic curve/surface and wrapper conversion, angle units and broader unit
layouts; explicit periodic/pcurve branch handling and valid cavity conversion;
production tolerances and geometric validation; broader interactive lifecycle
validation and performance/resource-limit work. Degrees above native cubic must
remain explicit refusals unless an exact native decomposition is available;
fitting or a silent mesh fallback is not an alternative.
The export/schema mismatch recorded above remains deferred and untouched.
