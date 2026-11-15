# Fork release builds

`.github/workflows/release.yml` builds the three release packages on GitHub-hosted runners and
publishes them as a GitHub release. The scripts in this folder hold the build checks. They follow
the local `2606-fork-13` release scripts.

| Job | Runner | Package |
|---|---|---|
| Windows x64 | `windows-2025-vs2026` | `dolphin-<version>-windows-x64.zip` |
| macOS arm64 | `macos-26` | `dolphin-<version>-macos-arm64.zip` |
| Android | `ubuntu-24.04` | `dolphin-<version>-arm64-v8a.apk` (arm64-v8a and x86_64 native code) |

## Making a release

1. Land the release's changes on `master`.
2. Write the notes as `docs/release-notes/<tag>.md` and commit them on `master`. Use the usual
   layout: intro, feature sections, Windows, macOS, Android. CI appends a **Verification** section
   built from the build checks, unless the notes already have one. It also appends the
   **Packages** SHA-256 table and the **Full Changelog** link.
3. Tag that commit with an annotated tag and push it:

   ```sh
   git tag -a 2606-fork-14 -m 2606-fork-14
   git push origin 2606-fork-14
   ```

The workflow builds and checks all three packages. When every job passes, it publishes the
release as Latest. If the notes file is missing, it creates a draft release instead; write the
notes there and publish it by hand. A failed run publishes nothing. Re-run it from the Actions tab.
A re-run only updates a release that is still a draft.

## What each job checks

- **All jobs:** the tag is annotated and on the built commit. The build runs on a branch named
  `master`, so `--version` prints no branch. `git describe` matches the tag.
- **Windows:** core and Qt unit tests pass. The package has the required runtimes, Qt plugins and
  resources. `Dolphin.exe` embeds the version and commit. `--version` prints `Dolphin <version>`
  and invalid options exit with status 2. The file set is compared with the previous release's
  zip; differences are reported in the notes but do not fail the build.
- **macOS:** the ad-hoc signature and the JIT and library-validation entitlements are checked. The
  `--version` and invalid-option checks match Windows. librashader, emusync, MoltenVK and
  Homebrew's FFmpeg (for frame dumping) must be bundled. No bundled Mach-O may load from `/opt/homebrew` or `/usr/local`. MoltenVK is the
  upstream `v1.4.1` release, checked against its SHA-256.
- **Android:** the JVM unit tests pass. The APK is zip-aligned (16 KB pages) and signed with v2
  only. The signing certificate must match the previous release, and the versionCode must be
  higher than the previous release's. The package name, version name and versionCode are checked,
  and both ABIs' `libmain.so` must embed the version and commit.

Each job's logs, test results and package are kept as a workflow artifact, also when it fails.

## Test builds

Pull requests that change the workflow or these scripts run the three build jobs without
publishing. A run can also be started by hand from the Actions tab (**Run workflow**) once the
workflow is on `master`. Those packages are named after `git describe`, such as
`dolphin-2606-fork-13-4-windows-x64.zip`. If no signing keystore secret is available, the APK is
left unsigned.

## Signing secret

The Android job signs with the keystore that signed the earlier fork releases. Store it as these
repository secrets (**Settings → Secrets and variables → Actions**):

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | the keystore file, base64-encoded |
| `ANDROID_KEYSTORE_PASSWORD` | the keystore password |
| `ANDROID_KEY_ALIAS` | the key alias |
| `ANDROID_KEY_PASSWORD` | the key password |

With the GitHub CLI:

```sh
base64 -w0 path/to/release.keystore | gh secret set ANDROID_KEYSTORE_BASE64 -R ilyalissoboi/dolphin
gh secret set ANDROID_KEYSTORE_PASSWORD -R ilyalissoboi/dolphin
gh secret set ANDROID_KEY_ALIAS -R ilyalissoboi/dolphin
gh secret set ANDROID_KEY_PASSWORD -R ilyalissoboi/dolphin
```

A tag build fails if the keystore secret is missing.
