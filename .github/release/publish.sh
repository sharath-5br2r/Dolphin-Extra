#!/usr/bin/env bash
# Creates the GitHub release for TAG from the three packages in PACKAGES_DIR.
#
# The notes come from docs/release-notes/<tag>.md in the tagged commit. The script appends a
# Verification section built from the jobs' verify-*.md files (unless the notes already have one),
# the Packages SHA-256 table and the Full Changelog link. Without a notes file the release is
# created as a draft, to be written and published by hand. An existing draft is updated; an
# existing published release is left alone.
set -euo pipefail

: "${TAG:?}" "${PACKAGES_DIR:?}"
files=(
  "$PACKAGES_DIR/dolphin-$TAG-arm64-v8a.apk"
  "$PACKAGES_DIR/dolphin-$TAG-macos-arm64.zip"
  "$PACKAGES_DIR/dolphin-$TAG-windows-x64.zip"
)
for f in "${files[@]}"; do
  test -f "$f" || { echo "::error::$(basename "$f") is missing"; exit 1; }
done

notes_source="docs/release-notes/$TAG.md"
body="$RUNNER_TEMP/release-notes.md"
draft=false
if [[ -f "$notes_source" ]]; then
  cp "$notes_source" "$body"
else
  draft=true
  echo "::warning::$notes_source does not exist; creating a draft release"
  printf 'Fork release built from `%s`.\n\n<!-- Write the release notes here, then publish the draft. -->\n' "${COMMIT:0:10}" > "$body"
fi

if ! grep -q '^## Verification' "$body"; then
  {
    echo
    echo "## Verification"
    echo
    for platform in windows macos android; do
      cat "$PACKAGES_DIR/verify-$platform.md"
    done
  } >> "$body"
fi

{
  echo
  echo "## Packages"
  echo
  echo "| File | SHA-256 |"
  echo "|---|---|"
  for f in "${files[@]}"; do
    echo "| \`$(basename "$f")\` | \`$(sha256sum "$f" | cut -d' ' -f1)\` |"
  done
  if [[ -n "${PREV_TAG:-}" ]] && ! grep -q '^\*\*Full Changelog' "$body"; then
    echo
    echo "**Full Changelog:** https://github.com/$GITHUB_REPOSITORY/compare/$PREV_TAG...$TAG"
  fi
} >> "$body"

if existing=$(gh release view "$TAG" --repo "$GITHUB_REPOSITORY" --json isDraft --jq .isDraft 2>/dev/null); then
  if [[ "$existing" != true ]]; then
    echo "::error::release $TAG is already published; delete it or use a new tag to rebuild"
    exit 1
  fi
  gh release upload "$TAG" "${files[@]}" --repo "$GITHUB_REPOSITORY" --clobber
  gh release edit "$TAG" --repo "$GITHUB_REPOSITORY" --notes-file "$body"
  echo "Updated draft release $TAG"
  exit 0
fi

if $draft; then
  gh release create "$TAG" "${files[@]}" --repo "$GITHUB_REPOSITORY" --verify-tag --title "$TAG" \
    --notes-file "$body" --draft
  echo "Created draft release $TAG"
else
  gh release create "$TAG" "${files[@]}" --repo "$GITHUB_REPOSITORY" --verify-tag --title "$TAG" \
    --notes-file "$body" --latest
  echo "Published release $TAG"
fi
