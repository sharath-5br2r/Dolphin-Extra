#!/usr/bin/env bash
# Checks and zips the macOS app bundle, following the local 2606-fork-13 build-macos-and-android.sh.
# Expects the environment from prepare-checkout.sh plus BUILD_DIR (the CMake build directory) and
# OUT_DIR (where the package, logs and verify-macos.md go).
set -euo pipefail

app="$BUILD_DIR/Binaries/Dolphin.app"
package="$PACKAGE-macos-arm64"
mkdir -p "$OUT_DIR"

codesign --verify --deep --strict --verbose=2 "$app"
file "$app/Contents/MacOS/Dolphin"
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$app/Contents/Info.plist"
codesign -d --entitlements :- "$app" > "$OUT_DIR/macos-entitlements.plist"
grep -q com.apple.security.cs.disable-library-validation "$OUT_DIR/macos-entitlements.plist"
grep -q com.apple.security.cs.allow-jit "$OUT_DIR/macos-entitlements.plist"

"$app/Contents/MacOS/Dolphin" --version | tee "$OUT_DIR/macos-version.txt"
if ! grep -qxF "Dolphin $VERSION" "$OUT_DIR/macos-version.txt"; then
  echo "::error::--version does not print 'Dolphin $VERSION'"
  exit 1
fi
set +e
"$app/Contents/MacOS/Dolphin" --invalid-release-option > /dev/null 2>&1
invalid=$?
set -e
if [[ $invalid != 2 ]]; then
  echo "::error::invalid command-line options exit with $invalid, expected 2"
  exit 1
fi

for lib in librashader.dylib libemusync.dylib libMoltenVK.dylib; do
  test -f "$app/Contents/Frameworks/$lib" || { echo "::error::$lib is missing from the bundle"; exit 1; }
done
# FFmpeg (frame dumping) is found by CMake only if installed, so check it was bundled. Its file
# names carry the library version.
for lib in avcodec avformat avutil swresample swscale; do
  if ! compgen -G "$app/Contents/Frameworks/lib$lib.*.dylib" > /dev/null; then
    echo "::error::FFmpeg's lib$lib is missing from the bundle"
    exit 1
  fi
done

# Every Mach-O in the bundle must load only system libraries and bundled ones.
find "$app" -type f -exec sh -c 'file "$1" | grep -q Mach-O && { echo "== $1"; otool -L "$1"; } || true' sh {} \; \
  > "$OUT_DIR/macos-libraries.txt"
macho_count=$(grep -c '^== ' "$OUT_DIR/macos-libraries.txt")
if grep -E '^[[:space:]]+(/opt/homebrew|/usr/local)' "$OUT_DIR/macos-libraries.txt"; then
  echo "::error::bundled binaries load libraries from outside the bundle"
  exit 1
fi

rm -f "$OUT_DIR/$package.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$OUT_DIR/$package.zip"
hash=$(shasum -a 256 "$OUT_DIR/$package.zip" | cut -d' ' -f1)

cat > "$OUT_DIR/verify-macos.md" <<EOF
- **macOS build:** native Apple Silicon Release build (Xcode $(xcodebuild -version | head -n 1 | cut -d' ' -f2), Qt $(brew list --versions qt | cut -d' ' -f2), FFmpeg $(brew list --versions ffmpeg | cut -d' ' -f2), MoltenVK ${MOLTENVK_VERSION:-unknown}). The ad-hoc bundle signature and the required JIT and library-loading entitlements were verified. All $macho_count bundled Mach-O binaries were checked for external load dependencies, and the bundled librashader, emusync, MoltenVK and FFmpeg libraries are present. \`--version\` prints \`Dolphin $VERSION\` and exits with status 0; invalid command-line options exit with status 2.
EOF

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  { echo "### macOS"; echo; cat "$OUT_DIR/verify-macos.md"; echo "- \`$package.zip\`: \`$hash\`"; echo; } >> "$GITHUB_STEP_SUMMARY"
fi
echo "$package.zip SHA-256 $hash"
