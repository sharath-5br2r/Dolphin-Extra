#!/usr/bin/env bash
# Aligns, signs and checks the release APK, following the local 2606-fork-13 sign-android.ps1.
# Expects the environment from prepare-checkout.sh plus OUT_DIR, BUILD_TOOLS (an Android SDK
# build-tools directory), UNSIGNED_APK, and the keystore in ANDROID_KEYSTORE_FILE with
# ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD.
set -euo pipefail

package="$PACKAGE-arm64-v8a"
output="$OUT_DIR/$package.apk"
aligned="$RUNNER_TEMP/$package-aligned.apk"
mkdir -p "$OUT_DIR"

"$BUILD_TOOLS/zipalign" -P 16 -f 4 "$UNSIGNED_APK" "$aligned"
"$BUILD_TOOLS/apksigner" sign --ks "$ANDROID_KEYSTORE_FILE" --ks-key-alias "$ANDROID_KEY_ALIAS" \
  --ks-pass env:ANDROID_KEYSTORE_PASSWORD --key-pass env:ANDROID_KEY_PASSWORD \
  --v1-signing-enabled false --v2-signing-enabled true --v3-signing-enabled false --v4-signing-enabled false \
  --out "$output" "$aligned"
"$BUILD_TOOLS/apksigner" verify --verbose --print-certs "$output" | tee "$OUT_DIR/android-signature.txt"
"$BUILD_TOOLS/zipalign" -c -P 16 4 "$output"

cert_of() { sed -n 's/.*certificate SHA-256 digest: \([0-9a-f]*\).*/\1/p' "$1" | head -n 1; }
cert=$(cert_of "$OUT_DIR/android-signature.txt")
test -n "$cert"

# Package name, version name and versionCode (first-parent commit count * 2, plus 1 unless an
# annotated tag is on HEAD), computed the way getBuildVersionCode() in
# Source/Android/app/build.gradle.kts does. That depends on the checkout, not on the workflow
# event, so a manual run on a tagged commit expects the release versionCode too.
"$BUILD_TOOLS/aapt2" dump badging "$output" > "$OUT_DIR/android-metadata.txt"
badging=$(head -n 1 "$OUT_DIR/android-metadata.txt")
version_code=$(( $(git rev-list --first-parent --count HEAD) * 2 ))
git describe --exact-match HEAD > /dev/null 2>&1 || version_code=$(( version_code + 1 ))
for expected in "name='org.dolphinemu.dolphinemu'" "versionCode='$version_code'" "versionName='$VERSION'"; do
  if [[ "$badging" != *"$expected"* ]]; then
    echo "::error::APK metadata lacks $expected: $badging"
    exit 1
  fi
done

# The certificate must match the previous release so the APK installs over it, and the
# versionCode must be higher than the previous release's.
cert_note="No previous release to compare the signing certificate with."
code_note=""
if [[ -n "$PREV_TAG" ]]; then
  prev_dir="$RUNNER_TEMP/previous-android"
  gh release download "$PREV_TAG" --repo "$GITHUB_REPOSITORY" --pattern '*.apk' --dir "$prev_dir" --clobber
  prev_apk=$(find "$prev_dir" -name '*.apk' | head -n 1)
  "$BUILD_TOOLS/apksigner" verify --print-certs "$prev_apk" > "$RUNNER_TEMP/previous-signature.txt"
  prev_cert=$(cert_of "$RUNNER_TEMP/previous-signature.txt")
  if [[ "$cert" != "$prev_cert" ]]; then
    echo "::error::the APK certificate ($cert) differs from $PREV_TAG ($prev_cert)"
    exit 1
  fi
  prev_code=$("$BUILD_TOOLS/aapt2" dump badging "$prev_apk" | sed -n "1s/.*versionCode='\([0-9]*\)'.*/\1/p")
  if (( version_code <= prev_code )); then
    echo "::error::versionCode $version_code is not above $PREV_TAG's $prev_code"
    exit 1
  fi
  cert_note="It has the same signing certificate as \`$PREV_TAG\`."
  code_note=", above the prior release's $prev_code"
fi

# Native libraries for both ABIs, each stamped with this version and commit.
libs="$RUNNER_TEMP/apk-libs"
rm -rf "$libs"
mkdir -p "$libs"
unzip -q -o "$output" 'lib/*' -d "$libs"
for lib in lib/arm64-v8a/libmain.so lib/x86_64/libmain.so lib/arm64-v8a/libemusync.so lib/x86_64/libemusync.so \
  lib/arm64-v8a/librashader.so; do
  test -f "$libs/$lib" || { echo "::error::the APK is missing $lib"; exit 1; }
done
for abi in arm64-v8a x86_64; do
  if ! grep -qaF "$COMMIT" "$libs/lib/$abi/libmain.so" || ! grep -qaF "$VERSION" "$libs/lib/$abi/libmain.so"; then
    echo "::error::lib/$abi/libmain.so does not embed the expected version and commit"
    exit 1
  fi
done

hash=$(sha256sum "$output" | cut -d' ' -f1)
cat >> "$OUT_DIR/verify-android.md" <<EOF
- **APK:** it reports version \`$VERSION\` (\`versionCode\` $version_code$code_note). Both native libraries contain the release commit and version. It passes signature and ZIP-alignment checks. $cert_note
EOF

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  { echo "### Android"; echo; cat "$OUT_DIR/verify-android.md"; echo "- \`$package.apk\`: \`$hash\`"; echo; } >> "$GITHUB_STEP_SUMMARY"
fi
echo "$package.apk SHA-256 $hash"
