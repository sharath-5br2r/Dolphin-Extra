#!/usr/bin/env bash
# Puts a CI checkout into the state the release builds expect, then exports the version
# information the later steps use (VERSION, COMMIT, TAG, IS_RELEASE, PREV_TAG, PACKAGE).
#
# Run from the repository root after actions/checkout with fetch-depth: 0. Needs GH_TOKEN for
# the previous-release lookup.
set -euo pipefail

commit=$(git rev-parse HEAD)
tag=""
is_release=false
if [[ "${GITHUB_REF:-}" == refs/tags/* ]]; then
  tag=${GITHUB_REF#refs/tags/}
  is_release=true
  # Fetch the tag object itself: the version scripts use `git describe` without --tags, which
  # only sees annotated tags.
  git fetch --force --no-recurse-submodules origin "refs/tags/$tag:refs/tags/$tag"
  if [[ $(git cat-file -t "$tag") != tag ]]; then
    echo "::error::$tag is a lightweight tag; release tags must be annotated (git tag -a)"
    exit 1
  fi
  if [[ $(git rev-parse "$tag^{commit}") != "$commit" ]]; then
    echo "::error::$tag does not point at the checked-out commit $commit"
    exit 1
  fi
fi

# CMake/ScmRevGen.cmake reads the branch name and counts commits ahead of master. Build from a
# branch named master, as the local release builds did, so --version prints no branch.
git checkout -q -B master "$commit"

if [[ "${RUNNER_OS:-}" == Windows ]]; then
  git config --global core.longpaths true
fi
git submodule sync --recursive
git submodule update --init --recursive --depth 1 --jobs 8

# The same description CMake (ScmRevGen.cmake) and Gradle (getGitVersion) derive.
version=$(git describe --always --long | sed -E 's/(-0)?-[^-]+$//')
if $is_release && [[ "$version" != "$tag" ]]; then
  echo "::error::git describe gives $version, expected $tag"
  exit 1
fi

# The most recent published fork release other than this one: the baseline for the Windows
# file-set comparison, the APK certificate and versionCode checks, and the changelog link.
# A failed lookup stops the build rather than silently skipping those checks.
releases=$(gh release list --repo "$GITHUB_REPOSITORY" --exclude-drafts --limit 50 --json tagName --jq '.[].tagName')
prev_tag=$(grep -E -- '-fork-[0-9]+$' <<< "$releases" | grep -vxF -- "${tag:-<none>}" | head -n 1 || true)

{
  echo "VERSION=$version"
  echo "COMMIT=$commit"
  echo "TAG=$tag"
  echo "IS_RELEASE=$is_release"
  echo "PREV_TAG=$prev_tag"
  echo "PACKAGE=dolphin-$version"
} >> "$GITHUB_ENV"

echo "Version $version from $commit (release: $is_release, previous release: ${prev_tag:-none})"
