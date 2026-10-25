# Android GPU Driver Manager + Post-Processing Perf (D1/D2/D3) — Design

**Date:** 2026-09-13
**Branch:** `feature/android-gpu-driver-manager-postfx-perf` (off `origin/master`)
**Status:** written autonomously from the 2026-09-13 feasibility analysis; the user asked for
implementation without a review pause. Every decision below is a stated assumption the user can
overturn.

## Goals

1. **Experiment.** Measure Dolphin on the AYN Thor (Adreno 740, Android 13) with the system
   Qualcomm driver versus the installed Turnip driver, before and after the perf changes.
2. **GPU driver manager (Android).** Bring Dolphin's single-slot, local-zip-only custom-driver
   support to parity with ARMSX2: in-app downloads from GitHub-release sources, several installed
   drivers with selection and deletion, lenient `meta.json` handling (including synthesis for bare
   `.so` packs), GPU detection with a recommended source, and a real settings screen for all of it.
3. **Perf (Vulkan librashader path), no presented pixel may change.**
   - **D1** enable `VK_KHR_dynamic_rendering` and hand librashader a populated options struct.
   - **D2** render the chain straight into the swapchain image when the draw rect covers the
     whole backbuffer, skipping the intermediate target and blit.
   - **D3** stop splitting the backbuffer render pass: defer the clear so clear + first draw
     share one pass instead of clear-store followed by load.

## Non-goals

- Copying ARMSX2 Kotlin (GPLv3; Dolphin is GPLv2-or-later). Designs are reimplemented.
- Force-max-GPU-clocks wiring, DriverDetails entries for Freedreno, DolphinQt changes.
- Any change to the built-in multipass engine beyond what D3 gives it for free.

---

## Part A — Experiment protocol

- Game: *Star Wars: Rogue Squadron II — Rogue Leader* (GC), launched by `am start` with its SAF
  content URI; the attract demo runs unattended.
- Settings as on device: IR 4x, librashader engine, RetroCrisis "RC GDV-NTSC - PS2" 1080p preset,
  `WaitForShadersBeforeStarting`. Add `LogRenderTimeToFile = True`; pull
  `Logs/render_times.txt` and `Logs/vblank_times.txt`.
- Each run: launch, wait 120 s, force-stop. Discard the first 30 s (boot + shader warm-up).
  Report mean / median / p95 frame time and derived FPS.
- Matrix: {current APK, new APK} × {system driver, installed Turnip}; plus new APK with a pack
  downloaded through the new UI if one differs from the installed one.
- Driver toggled by editing `DriverLibName` in `Config/GFX.ini` (shared storage, no UI
  automation). The original GFX.ini / Logger.ini are restored at the end.

## Part B — GPU driver manager

### B.1 Storage and native selection

```
filesDir/GPUDrivers/
  Extracted/            legacy single slot (kept readable; migrated on first list)
  Installed/<id>/       one dir per driver: meta.json + <libraryName>.so
  Tmp/  FileRedirect/   unchanged (shared by all drivers)
```

- New config key `GFX_DRIVER_PACKAGE` = `[Settings] DriverPackage` (string, default "").
  `VideoConfig::customDriverPackage` mirrors it. Kotlin `StringSetting.GFX_DRIVER_PACKAGE`.
- `VulkanLoader::OpenVulkanLibrary`: driver dir is `Installed/<package>/` when the package is
  set, else legacy `Extracted/`. New `D_GPU_DRIVERS_INSTALLED` in `FileUtil`.
  adrenotools concatenates dir + name, so a trailing `/` on the dir is all it needs.
- `<id>` is dlopen-safe: `[A-Za-z0-9._-]` only, lower-cased, derived from
  `<sourcePrefix>-<tag>-<assetBase>` for downloads and `local-<filename>` for imports.
- **Migration:** when listing, if `Extracted/meta.json` exists, move `Extracted/*` to
  `Installed/<id>` (id from the metadata name + version) and, if `DriverLibName` is non-empty,
  set `DriverPackage` to that id. Idempotent; runs at most once.

### B.2 Metadata (`GpuDriverMetadata`)

- Lenient decoder: `ignoreUnknownKeys`, every field optional with defaults, `schemaVersion`
  optional (absent or 1 accepted; other values still rejected).
- `libraryName` default `libvulkan_freedreno.so`.
- `synthesize(id, libraryName)` builds a minimal v1 manifest for packs with no `meta.json`
  (freedreno-builder style). Applied on install when exactly one `*.so` is present.
- Pure Kotlin (kotlinx-serialization only), unit-tested on the JVM.

### B.3 Install / list / delete (`GpuDriverHelper`)

- `installFromStream(stream, id)`: extract into `Installed/<id>.tmp` flattening nested paths,
  reject `..`/absolute entries, synthesize `meta.json` if needed, validate the `.so` exists and
  `minApi <= SDK_INT`, then rename into place. Returns `GpuDriverInstallResult`.
- `listInstalled()`, `delete(id)`, `getInstalledMetadata(id)`.
- `getSystemGpuInfo()` via JNI: `getSystemDriverInfo` now returns
  `[driverId, driverVersion, deviceName, vendorId, apiVersion]`.

### B.4 Catalog (`GpuDriverCatalog`)

- `DriverSource(label, releasesApiUrl, idPrefix, note)`; seven sources, in display order:
  K11MCH1/AdrenoToolsDrivers, MrPurple666/purple-turnip, StevenMXZ/Adreno-Tools-Drivers,
  crueter/GameHub-8Elite-Drivers, PojavLauncherTeam/freedreno-builder,
  WearyConcern1165/ExynosTools, Balemuni/Balemunis-Aurora.
- `parseReleases(json, source): List<RemoteGpuDriver>` — one entry per `.zip` asset
  (`browser_download_url`, size, tag, release name, published date). Pure, unit-tested.
- `fetchAll()` iterates sources sequentially with `HttpURLConnection` (15 s timeout,
  `User-Agent: Dolphin-Android/<version>`); a failing source yields an empty list, never an
  exception. `download(remote, onProgress)` streams the zip to `Tmp/` then installs.

### B.5 GPU detection (`GpuInfo`)

- Input: Vulkan `deviceName` + `vendorId` from the system driver probe.
- `recommend(deviceName, vendorId)`: Adreno 6xx → purple-turnip; Adreno 7xx → Balemuni Aurora;
  Adreno 8xx → crueter GameHub; Xclipse → ExynosTools; anything else → `null` (system driver).
  Pure, unit-tested.

### B.6 UI (settings framework, no Compose)

`MenuTag.GPU_DRIVERS` becomes a normal submenu (drop the dialog special-case in
`SettingsFragment.loadSubMenu`). `SettingsFragmentPresenter.addGpuDriverSettings` builds:

1. **Header** — "GPU: Adreno (TM) 740 · Vulkan 1.3 · Qualcomm 512.676.53" and
   "Recommended source: Balemuni · Aurora" (or "System driver is recommended").
2. **Installed drivers** — one `RunRunnable` for *System driver* and one per installed driver
   (title `name vX`, subtitle `author · vendor · driverVersion`, "● Active" suffix on the
   selected one). Tap → dialog *Use this driver / Delete / Cancel*. Selecting writes
   `DriverPackage` + `DriverLibName`; system clears both.
3. **Install from file…** — existing SAF picker, now installs into a new slot.
4. **Download drivers…** — progress dialog while fetching; then a source picker
   (recommended source first), then a single-choice list of assets
   (`release · asset (MB)`); download with progress, install, select, refresh the list.
5. **Reload** of the list after every change via `loadSettingsList()`.

All strings added to `strings.xml`. Only shown when emulation is not running (existing gate).

### B.7 Tests

- Add `testImplementation("junit:junit:4.13.2")`; tests under
  `Source/Android/app/src/test/java/org/dolphinemu/dolphinemu/`:
  `GpuDriverMetadataTest`, `GpuDriverCatalogTest`, `GpuInfoTest`, `GpuDriverIdTest`.
- Run with `./gradlew :app:testDebugUnitTest` (no device needed).

## Part C — Perf changes (Vulkan)

### C.1 D1 — dynamic rendering + chain options

- `VulkanContext`: request instance API 1.3 when the loader supports it (the ladder stops at
  1.2 today). Select `VK_KHR_dynamic_rendering` when the device is < 1.3; probe the
  `dynamicRendering` feature through `vkGetPhysicalDeviceFeatures2` (loaded as an optional
  entry point) and chain `VkPhysicalDeviceDynamicRenderingFeatures{VK_TRUE}` into
  `VkDeviceCreateInfo::pNext`. Expose `SupportsDynamicRendering()`.
- `LibrashaderPostProcessing::CreateChain`: fill `filter_chain_vk_opt_t`
  `{LIBRASHADER_CURRENT_VERSION, frames_in_flight = 0 (default 3 ≥ Dolphin's 2),
  force_no_mipmaps = false, use_dynamic_rendering, disable_cache = false}` where
  `use_dynamic_rendering = ctx.SupportsDynamicRendering() &&
  vkGetDeviceProcAddr(device, "vkCmdBeginRendering") != nullptr`. Log which path was taken.
- Pixel risk: none. Failure mode: librashader falls back to render passes on its own.

### C.2 D2 — direct-to-swapchain when the draw rect is the whole backbuffer

- Pure policy `ShouldRenderChainDirectly(dst, fb_width, fb_height)`: true iff
  `dst == {0,0,fb_width,fb_height}`. Stereo SBS/TAB half-rects and pillarboxed rects stay on
  the intermediate target, so `OutputSize` semantics (the moiré fix) are untouched.
- When true: `out` is the framebuffer's color attachment (already
  `COLOR_ATTACHMENT_OPTIMAL` after `BindBackbuffer`), viewport = full image, no blit.
  Afterwards `OverrideImageLayout(COLOR_ATTACHMENT_OPTIMAL)` so ImGui's LOAD pass and the
  PRESENT transition see the right layout. The pending clear (C.3) is discarded because the
  chain's final pass overwrites every pixel.

### C.3 D3 — deferred backbuffer clear

- `StateTracker` gains `SetPendingClear(VKFramebuffer*, VkClearValue)`,
  `DiscardPendingClear()`, `HasPendingClear()`. `BeginRenderPass()` begins the framebuffer's
  **clear** render pass over its full rect when a pending clear targets `m_framebuffer`, then
  clears the pending state; otherwise the load pass as today.
- `VKGfx::BindBackbuffer` replaces `SetAndClearFramebuffer` with `SetFramebuffer` +
  `SetPendingClear`. `VKGfx::PresentBackbuffer` flushes a still-pending clear (begin + end) so
  a frame with no draws still presents black, exactly as before.
- Intermediate binds (downscale target, chain passes) leave the pending clear alone; the first
  draw into the swapchain framebuffer (post-process blit, built-in final pass, or ImGui) pays
  one CLEAR-load pass instead of clear-store + load. `InvalidateCachedState()` does not touch
  the pending clear (it is not cached state; a mid-frame submit just records the clear later,
  still before any draw).
- Pixel risk: none — the same pixels are cleared, once, before anything reads or writes them.

### C.4 Tests

- `Source/UnitTests/VideoCommon/PostProcessing/ChainOutputPolicyTest.cpp` for
  `ShouldRenderChainDirectly` and `ChooseDynamicRendering(supports_feature, has_proc)`.
- Full `tests` target must stay green (host build with the macOS workarounds).

## Part D — Verification on device

1. Build signed release APK (debug keystore), install over the existing package.
2. logcat must show `Librashader: filter chain created … (dynamic rendering on)` and the
   driver load line for the selected driver.
3. Screenshot compare (`adb exec-out screencap`) system-vs-Turnip and old-vs-new at the same
   attract-mode frame is a sanity check only; the D2/D3 argument for pixel identity is structural.
4. Driver manager: list shows the migrated Turnip driver as active; download a Balemuni pack,
   select it, relaunch, confirm the load line names the new library.
5. Re-run the Part A matrix and report.

## Commit plan (one branch, reviewable commits)

1. `Config/Vulkan: add DriverPackage and load custom drivers from Installed/<id>/`
2. `Android: lenient GPU driver metadata + multi-slot install/list/delete (+ tests)`
3. `Android: GPU driver catalog (GitHub releases) + GPU recommendation (+ tests)`
4. `Android: GPU driver manager settings screen`
5. `Vulkan: request API 1.3 and enable VK_KHR_dynamic_rendering`
6. `Vulkan/librashader: populate chain options, use dynamic rendering`
7. `Vulkan: defer backbuffer clear into the first render pass (D3)`
8. `Vulkan/librashader: render chain directly into the backbuffer when the draw rect is full (D2)`
9. `docs: experiment results`
