# Explicitly opt-in corpus, never a configure/build dependency. Keep the exact
# source revision and archive digest here so both online and offline runs use
# the same unmodified samples. No NIST sample is copied into the source tree.
set(url "https://raw.githubusercontent.com/usnistgov/SFA/1236017676c0fa1f24f0fb4642fb088437a406f7/Release/NIST-PMI-STEP-Files.zip")
set(sha256 "1fb91bb8ff0fe02032b948fda0775bc74591cd0bebc0988347d32574e5884f90")
if(NOT EXISTS "${TEST_EXE}" OR NOT CORPUS_DIR)
    message(FATAL_ERROR "Run this script through the test_step_import_nist target")
endif()
file(MAKE_DIRECTORY "${CORPUS_DIR}")
if(NOT ARCHIVE)
    set(ARCHIVE "${CORPUS_DIR}/NIST-PMI-STEP-Files.zip")
    if(NOT EXISTS "${ARCHIVE}")
        message(STATUS "Downloading pinned NIST PMI STEP corpus (about 13 MB)")
        file(DOWNLOAD "${url}" "${ARCHIVE}.download"
            TLS_VERIFY ON TIMEOUT 120 INACTIVITY_TIMEOUT 30
            EXPECTED_HASH "SHA256=${sha256}" STATUS status)
        list(GET status 0 code)
        if(NOT code EQUAL 0)
            message(FATAL_ERROR
                "NIST corpus download failed: ${status}. "
                "Use STEP_IMPORT_NIST_ARCHIVE to supply the pinned archive offline.")
        endif()
        file(RENAME "${ARCHIVE}.download" "${ARCHIVE}")
    endif()
endif()
if(NOT EXISTS "${ARCHIVE}")
    message(FATAL_ERROR "NIST archive not found: ${ARCHIVE}")
endif()
file(SHA256 "${ARCHIVE}" actual)
if(NOT actual STREQUAL sha256)
    message(FATAL_ERROR "NIST archive SHA256 mismatch: expected ${sha256}, got ${actual}")
endif()

file(ARCHIVE_EXTRACT INPUT "${ARCHIVE}" DESTINATION "${CORPUS_DIR}"
    PATTERNS
        "NIST-PMI-STEP-Files/nist_ctc_01_asme1_ap242-e1.stp"
        "NIST-PMI-STEP-Files/nist_ctc_03_asme1_ap242-e2.stp"
        "NIST-PMI-STEP-Files/AP203 geometry only/nist_ftc_11_asme1_rb.stp")
execute_process(COMMAND "${TEST_EXE}" --nist-dir "${CORPUS_DIR}/NIST-PMI-STEP-Files"
    TIMEOUT 60 RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "NIST STEP dependency tests failed: ${result}")
endif()
