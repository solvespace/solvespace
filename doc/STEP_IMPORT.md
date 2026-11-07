# Experimental STEP dependency proof

This is **not STEP linking or a geometry importer yet**. The opt-in targets read
schema-backed Part 21 data and expose enough typed attributes to investigate a
future native SolveSpace converter. No `SShell`, mesh, constraint entity, group,
file-dialog filter, or link dispatch has been added. No OCCT/Open Cascade code
or wrapper is used.

The production goal remains one immutable linked group per STEP file, using
native `SShell` geometry and useful constraint references, without recovering
feature history or editing STEP data. There will be no automatic mesh fallback.
Future partial conversion may skip **whole unsupported solids**, never selected
faces or cavity shells; it must report omissions and reject a link with no
usable geometry. The current parser proof does not implement that policy:
reported parse errors reject the entire document, and a successfully read
document need not contain a convertible solid.

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

## Validated configurations and measured cost

The following results are from native **Windows, MSVC 2019 / 19.29, x86** builds
with **CMake 3.29.5** and Ninja. Debug uses `/MTd`; Release uses `/MT`, matching
SolveSpace. These results do not establish Linux/macOS, 64-bit, Visual Studio
generator, or sanitizer compatibility. Cross compilation is intentionally
rejected, as described above.

| Generator / configuration | Bindings | Validation result |
| --- | --- | --- |
| Ninja / Debug | AP203, AP214, AP242 | `test_step_import_nist`: 97 cases, 5167 checks, 0 failures, including the normal reader suite |
| Ninja / Release | AP203 only | `test_step_import`: 34 cases, 1718 checks, 0 failures; disabled schemas rejected |
| Ninja Multi-Config / Debug | AP203 only | `test_step_import`: 34 cases, 1720 checks, 0 failures |
| Ninja Multi-Config / Release | AP203 only | `test_step_import`: 34 cases, 1718 checks, 0 failures |
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
installation-size deltas. The validated Debug probe imports only `SHLWAPI.dll`
and `KERNEL32.dll`; STEPcode and the MSVC runtime are linked statically.

## Before native conversion

Keep geometry conversion separate from dependency work. Remaining gates include
broader real AP203/edition coverage, portable builds and parser hardening;
representation/assembly selection and transforms; length/angle unit resolution;
oriented topology and tolerances; exact native curve/surface conversion and
trimming; complete-solid/cavity failure handling; stable constraint-reference
identities; reload/serialization/undo behavior; and GUI-safe diagnostics.
