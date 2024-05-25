#!/bin/bash -xe

lipo \
    -create \
        build/bin/SolveSpace.app/Contents/Resources/libomp.dylib \
        build-arm64/bin/SolveSpace.app/Contents/Resources/libomp.dylib \
    -output \
        build/bin/SolveSpace.app/Contents/Resources/libomp.dylib

lipo \
    -create \
        build/bin/SolveSpace.app/Contents/MacOS/SolveSpace \
        build-arm64/bin/SolveSpace.app/Contents/MacOS/SolveSpace \
    -output \
        build/bin/SolveSpace.app/Contents/MacOS/SolveSpace

lipo \
    -create \
        build/bin/SolveSpace.app/Contents/MacOS/solvespace-cli \
        build-arm64/bin/SolveSpace.app/Contents/MacOS/solvespace-cli \
    -output \
        build/bin/SolveSpace.app/Contents/MacOS/solvespace-cli

cd build

openmp="bin/SolveSpace.app/Contents/Resources/libomp.dylib"
app="bin/SolveSpace.app"
dmg="bin/SolveSpace.dmg"
bundle_id="com.solvespace.solvespace"

if [ "$CI" = "true" ]; then
    # get the signing certificate (this is the Developer ID:Application: Your Name, exported to a p12 file, then converted to base64, e.g.: cat ~/Desktop/certificate.p12 | base64 | pbcopy)
    echo $MACOS_CERTIFICATE_P12 | base64 --decode > certificate.p12

    # create a keychain
    security create-keychain -p secret build.keychain
    security default-keychain -s build.keychain
    security unlock-keychain -p secret build.keychain

    # import the key
    security import certificate.p12 -k build.keychain -P "${MACOS_CERTIFICATE_PASSWORD}" -T /usr/bin/codesign

    security set-key-partition-list -S apple-tool:,apple: -s -k secret build.keychain

    # check if all is good
    security find-identity -v
fi

# sign openmp
codesign -s "${MACOS_DEVELOPER_ID}" --timestamp --options runtime -f --deep "${openmp}"

# sign the .app
codesign -s "${MACOS_DEVELOPER_ID}" --timestamp --options runtime -f --deep "${app}"

# create the .dmg from the signed .app
hdiutil create -srcfolder "${app}" "${dmg}"

# sign the .dmg
codesign -s "${MACOS_DEVELOPER_ID}" --timestamp --options runtime -f --deep "${dmg}"

if ! command -v xcrun >/dev/null || ! xcrun --find notarytool >/dev/null; then
    echo "Notarytool is not present in the system. Notarization has failed."
    exit 1
fi

# Submit the package for notarization
notarization_output=$(
    xcrun notarytool submit "${dmg}" \
        --apple-id "hello@koenschmeets.nl" \
        --password "${MACOS_APPSTORE_APP_PASSWORD}" \
        --team-id "8X77K9NDG3" \
        --wait 2>&1)

if [ $? -eq 0 ]; then
    # Extract the operation ID from the output
    operation_id=$(echo "$notarization_output" | awk '/RequestUUID/ {print $NF}')
    echo "Notarization submitted. Operation ID: $operation_id"
    exit 0
  else
    echo "Notarization failed. Error: $notarization_output"
    exit 1
  fi
fi

# staple
xcrun stapler staple "${dmg}"
