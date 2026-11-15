# Tests and packages the Windows x64 build, following the local 2606-fork-13 build-windows.ps1.
# Expects the environment from prepare-checkout.sh plus BUILD_DIR (the CMake build directory) and
# OUT_DIR (where the package, logs and verify-windows.md go). Runs under PowerShell 7.
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$version = $env:VERSION
$commit = $env:COMMIT
$package = "$env:PACKAGE-windows-x64"
$bin = Join-Path $env:BUILD_DIR 'Binaries'
$out = $env:OUT_DIR
New-Item -ItemType Directory -Force -Path $out | Out-Null

# Unit tests. The Qt tests need the build's own Qt DLLs and plugins.
$env:PATH = "$bin;$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $bin 'QtPlugins'
Push-Location $bin
& Tests\tests.exe "--gtest_output=json:$out\core-results.json" *> "$out\core-tests.log"
$core = $LASTEXITCODE
& Tests\qt-tests.exe "--gtest_output=json:$out\qt-results.json" *> "$out\qt-tests.log"
$qt = $LASTEXITCODE
Pop-Location
if ($core -ne 0) { Get-Content "$out\core-tests.log" -Tail 60; throw 'Core tests failed' }
if ($qt -ne 0) { Get-Content "$out\qt-tests.log" -Tail 60; throw 'Qt tests failed' }

function Get-TestSummary([string] $path) {
  $json = Get-Content $path -Raw | ConvertFrom-Json
  $skipped = @($json.testsuites.testsuite | Where-Object { $_.result -eq 'SKIPPED' }).Count
  [pscustomobject]@{ Tests = $json.tests; Failures = $json.failures; Disabled = $json.disabled; Skipped = $skipped }
}
$coreSummary = Get-TestSummary "$out\core-results.json"
$qtSummary = Get-TestSummary "$out\qt-results.json"

# Stage Binaries without the test executables and the CLI tool.
$stage = Join-Path $env:RUNNER_TEMP "stage\$package"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Get-ChildItem -LiteralPath $bin | Where-Object { $_.Name -notin @('Tests', 'DolphinTool.exe') } |
  Copy-Item -Destination $stage -Recurse
$required = @('Dolphin.exe', 'Updater.exe', 'emusync.dll', 'librashader.dll', 'Qt6Core.dll', 'Qt6Gui.dll',
  'Qt6Widgets.dll', 'Qt6Svg.dll', 'QtPlugins/platforms/qwindows.dll', 'Languages', 'Sys', 'Licenses', 'COPYING',
  'Sys/Resources/BigPicture/promptfont.otf')
foreach ($file in $required) {
  if (-not (Test-Path -LiteralPath (Join-Path $stage $file))) { throw "Missing Windows package file: $file" }
}

$exe = Join-Path $stage 'Dolphin.exe'
$exeText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($exe))
if (-not $exeText.Contains($commit) -or -not $exeText.Contains($version)) { throw 'Dolphin.exe does not embed the expected version and commit' }

$run = Start-Process -FilePath $exe -ArgumentList '--version' -Wait -PassThru -NoNewWindow `
  -RedirectStandardOutput "$out\windows-version.txt" -RedirectStandardError "$out\windows-version-error.txt"
$versionText = (Get-Content "$out\windows-version.txt" -Raw).Trim()
if ($run.ExitCode -ne 0 -or $versionText -ne "Dolphin $version") {
  throw "Version smoke test failed (exit $($run.ExitCode)): '$versionText', expected 'Dolphin $version'"
}
$run = Start-Process -FilePath $exe -ArgumentList '--invalid-release-option' -Wait -PassThru -NoNewWindow `
  -RedirectStandardOutput "$out\windows-invalid.txt" -RedirectStandardError "$out\windows-invalid-error.txt"
if ($run.ExitCode -ne 2) { throw "Invalid-option smoke test failed (exit $($run.ExitCode), expected 2)" }

# Compare the file set with the previous release's Windows package. Differences are reported, not
# fatal: new upstream resources are expected from time to time.
$new = Get-ChildItem -LiteralPath $stage -Recurse -File |
  ForEach-Object { $_.FullName.Substring($stage.Length + 1).Replace('\', '/') } | Sort-Object
$filesetNote = 'No previous release to compare the file set with.'
if ($env:PREV_TAG) {
  $prevDir = Join-Path $env:RUNNER_TEMP 'previous-windows'
  gh release download $env:PREV_TAG --repo $env:GITHUB_REPOSITORY --pattern '*-windows-x64.zip' --dir $prevDir --clobber
  if ($LASTEXITCODE -ne 0) { throw "Could not download the $env:PREV_TAG Windows package" }
  $prevZip = Get-ChildItem -LiteralPath $prevDir -Filter '*.zip' | Select-Object -First 1
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $previous = [IO.Compression.ZipFile]::OpenRead($prevZip.FullName)
  try {
    $root = [IO.Path]::GetFileNameWithoutExtension($prevZip.Name)
    $old = $previous.Entries | Where-Object { $_.Name } |
      ForEach-Object { $_.FullName.Replace('\', '/') -replace "^$([regex]::Escape($root))/", '' } | Sort-Object
  } finally { $previous.Dispose() }
  $diff = Compare-Object $old $new
  $diff | Format-Table -AutoSize | Out-String -Width 400 | Set-Content "$out\windows-fileset-diff.txt"
  if (-not $diff) {
    $filesetNote = "The file set matches ``$env:PREV_TAG``."
  } else {
    $added = @($diff | Where-Object SideIndicator -eq '=>' | ForEach-Object { "``$($_.InputObject)``" })
    $removed = @($diff | Where-Object SideIndicator -eq '<=' | ForEach-Object { "``$($_.InputObject)``" })
    $filesetNote = "Compared with ``$env:PREV_TAG``: $($added.Count) file(s) added"
    if ($added) { $filesetNote += " ($($added -join ', '))" }
    $filesetNote += ", $($removed.Count) removed"
    if ($removed) { $filesetNote += " ($($removed -join ', '))" }
    $filesetNote += '.'
  }
}

$zip = Join-Path $out "$package.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $true)
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()

$msvc = if ($env:VCToolsInstallDir) { Split-Path -Leaf $env:VCToolsInstallDir.TrimEnd('\') } else { 'unknown' }
@"
- **Windows build and tests:** x64 Release build (MSVC ``$msvc``). Core tests: $($coreSummary.Tests) run, $($coreSummary.Failures) failed, $($coreSummary.Skipped) skipped. Qt tests: $($qtSummary.Tests) run, $($qtSummary.Failures) failed, $($qtSummary.Skipped) skipped.
- **Windows package:** ``Dolphin.exe`` embeds the release version and commit, and the required runtimes, Qt plugins, resources, translations and licenses are present. $filesetNote ``--version`` prints ``Dolphin $version`` and exits with status 0; invalid command-line options exit with status 2.
"@ | Set-Content -Encoding utf8 (Join-Path $out 'verify-windows.md')

if ($env:GITHUB_STEP_SUMMARY) {
  "### Windows`n" | Add-Content $env:GITHUB_STEP_SUMMARY
  Get-Content (Join-Path $out 'verify-windows.md') | Add-Content $env:GITHUB_STEP_SUMMARY
  "- ``$package.zip``: ``$hash```n" | Add-Content $env:GITHUB_STEP_SUMMARY
}
"$package.zip $((Get-Item $zip).Length) bytes, SHA-256 $hash"
