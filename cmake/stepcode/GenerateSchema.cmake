file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(
    COMMAND "${EXP2CXX}" -V 2 --late-bound --metadata structural "${EXPRESS}"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    OUTPUT_FILE "${OUTPUT_DIR}/exp2cxx.stdout.log"
    ERROR_FILE "${OUTPUT_DIR}/exp2cxx.stderr.log"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "exp2cxx failed (${result}); see ${OUTPUT_DIR}/exp2cxx.*.log")
endif()

# This compact mode has a fixed, small source list. Verify it against the
# generator's own manifest instead of building a configure-time schema scanner.
# Fail visibly if an upstream update changes that contract.
include("${OUTPUT_DIR}/${PREFIX}.sources.cmake")
set(expected SdaiAll.cc compstructs.cc schema.cc "${PREFIX}.cc" "${PREFIX}.init.cc")
if(NOT STEPCODE_GENERATED_SCHEMA_SOURCES STREQUAL expected)
    message(FATAL_ERROR "STEPcode generated an unexpected source list; review StepCode.cmake")
endif()
foreach(source IN LISTS expected)
    if(NOT EXISTS "${OUTPUT_DIR}/${source}")
        message(FATAL_ERROR "STEPcode did not generate ${source}")
    endif()
endforeach()

# Upstream emits the complex-entity name table as one long string literal.
# AP214/AP242 exceed MSVC's per-literal limit. Splitting at encoded NULs keeps
# exactly the same bytes/offsets while staying below that limit. These pinned
# tables also fit MSVC's 64-KiB limit on the concatenated string.
file(READ "${OUTPUT_DIR}/compstructs.cc" complex)
string(REPLACE "\\000" "\\000\"\n    \"" complex "${complex}")
file(CONFIGURE OUTPUT "${OUTPUT_DIR}/compstructs.cc" CONTENT "${complex}" @ONLY)
