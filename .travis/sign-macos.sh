#!/bin/sh -xe

cd build

# app="bin/SolveSpace.app"
dmg="bin/SolveSpace.dmg"
bundle_id="com.solvespace.solvespace"

# get the signing certificate (this is the Developer ID:Application: Your Name, exported to a p12 file, then converted to base64, e.g.: cat ~/Desktop/certificate.p12 | base64 | pbcopy)
echo $MACOS_CERTIFICATE_P12 | base64 --decode > certificate.p12

# create a keychain
security create-keychain -p secret build.keychain
security default-keychain -s build.keychain
security unlock-keychain -p secret build.keychain

# import the key
security import certificate.p12 -k build.keychain -P $MACOS_CERTIFICATE_PASSWORD -T /usr/bin/codesign

security set-key-partition-list -S apple-tool:,apple: -s -k secret build.keychain
#security set-keychain-settings -t 3600 -u build.keychain

# check if all is good
security find-identity -v

# sign the .dmg
codesign -s "${MACOS_DEVELOPER_ID}" --timestamp --options runtime -f --deep "${dmg}"

# notarize and store request uuid in variable
notarize_uuid=$(xcrun altool --notarize-app --primary-bundle-id "${bundle_id}" --username "${MACOS_APPSTORE_USERNAME}" --password "${MACOS_APPSTORE_APP_PASSWORD}" --file "${dmg}" 2>&1 | grep RequestUUID | awk '{print $3'})

echo $notarize_uuid

# wait a bit so we don't get errors during checking
sleep 5

success=0
for (( ; ; ))
do
    echo "Checking progress..."
    progress=$(xcrun altool --notarization-info "${notarize_uuid}" -u "${MACOS_APPSTORE_USERNAME}" -p "${MACOS_APPSTORE_APP_PASSWORD}" 2>&1)
    echo "${progress}"
 
    if [ $? -ne 0 ] || [[  "${progress}" =~ "Invalid" ]] ; then
        echo "Error with notarization. Exiting"
        break
    fi
 
    if [[  "${progress}" =~ "success" ]]; then
        success=1
        break
    else
        echo "Not completed yet. Sleeping for 10 seconds"
    fi
    sleep 10
done

# staple
xcrun stapler staple $app