# Android GPU Driver Manager + Post-Processing Perf — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development for Track B
> (Kotlin/JNI) while the main agent executes Track C (Vulkan). TDD applies to every task with
> testable logic; mechanical wiring (strings, CMake lists) may skip it and must say so in the
> commit message. Steps use `- [ ]` checkboxes.

**Spec:** `docs/superpowers/specs/2026-09-13-android-driver-manager-and-postfx-perf-design.md`
**Branch:** `feature/android-gpu-driver-manager-postfx-perf` off `origin/master`
**Device:** AYN Thor (serial `64dc3c35`), Android 13, Adreno 740, Dolphin `2606-206` installed
with a Turnip driver (`vulkan.purple.so`) in the legacy `Extracted/` slot.

## Global constraints

- No presented pixel may change from Track C (D1–D3). D2 only fires when the draw rect equals the
  full backbuffer, where `OutputSize` is identical either way.
- Android arm64-v8a must build and run; desktop `videovulkan` and the `tests` target must still
  compile (host build uses the macOS libc++ workaround baked into `build/`).
- Do not copy ARMSX2 Kotlin (GPLv3). Reimplement from the spec.
- Commit after each task; show the commit message in chat first (user preference).
- Kotlin unit tests run with `cd Source/Android && ./gradlew :app:testDebugUnitTest`.
  C++ tests: `cmake --build build --target tests && ./build/Binaries/Tests/tests --gtest_filter=…`.

---

## Track A — Baseline experiment (before any code change)

### Task A1: Baseline matrix on the current APK

- [ ] Config already pushed: `GFX.ini` with `LogRenderTimeToFile = True`, `Logger.ini` verbose.
      Variants in `/tmp/thor-exp/GFX.ini.sys` (`DriverLibName =`) and `GFX.ini.turnip`.
- [ ] Launch path: find the exported entry point (EmulationActivity is not exported); fall back
      to MainActivity + `am start` of the game via Dolphin's own launcher if needed.
- [ ] For each of {sys, turnip}: push GFX.ini, clear `Logs/render_times.txt`, launch Rogue
      Leader, wait 120 s, pull `render_times.txt` + `vblank_times.txt`, `am force-stop`.
      Confirm in logcat which driver loaded (`Loading system driver` vs `Successfully loaded
      driver`).
- [ ] Compute mean/median/p95 frame time and FPS over samples after the first 30 s; store under
      `/tmp/thor-exp/results-baseline.md`.

---

## Track B — GPU driver manager (Kotlin + JNI + C++ config)

### Task B1: `DriverPackage` config key + `Installed/<id>/` loader path

**Files:**
- Modify: `Source/Core/Core/Config/GraphicsSettings.h/.cpp` (add `GFX_DRIVER_PACKAGE`)
- Modify: `Source/Core/VideoCommon/VideoConfig.h/.cpp` (`customDriverPackage`)
- Modify: `Source/Core/Common/CommonPaths.h` (`GPU_DRIVERS_INSTALLED "Installed"`),
  `Source/Core/Common/FileUtil.h/.cpp` (`D_GPU_DRIVERS_INSTALLED`)
- Modify: `Source/Core/VideoBackends/Vulkan/VulkanLoader.cpp:57-71`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/model/StringSetting.kt`

- [ ] Add `const Info<std::string> GFX_DRIVER_PACKAGE{{System::GFX, "Settings", "DriverPackage"}, ""};`
- [ ] `VideoConfig::Refresh`: `customDriverPackage = Config::Get(Config::GFX_DRIVER_PACKAGE);`
- [ ] `FileUtil`: new enum value + `case D_GPU_DRIVERS_INSTALLED: return s_android_driver_directory + DIR_SEP "Installed" DIR_SEP;`
- [ ] `VulkanLoader`:
  ```cpp
  std::string driver_dir = g_Config.customDriverPackage.empty() ?
      File::GetGpuDriverDirectory(D_GPU_DRIVERS_EXTRACTED) :
      File::GetGpuDriverDirectory(D_GPU_DRIVERS_INSTALLED) + g_Config.customDriverPackage + DIR_SEP;
  ```
  Log both package and library name.
- [ ] Kotlin: `GFX_DRIVER_PACKAGE(Settings.FILE_GFX, Settings.SECTION_GFX_SETTINGS, "DriverPackage", "")`.
- [ ] Build check: `cmake --build build --target videovulkan` (host) compiles.
- [ ] Commit: `Config/Vulkan: add DriverPackage and load custom drivers from Installed/<id>/`

### Task B2: Lenient metadata + multi-slot helper (+ JVM tests)

**Files:**
- Modify: `Source/Android/app/build.gradle.kts` (`testImplementation("junit:junit:4.13.2")`)
- Rewrite: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/model/DriverPackageMetadata.kt`
  (class `GpuDriverMetadata`)
- Rewrite: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/GpuDriverHelper.kt`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/DirectoryInitialization.kt`
  (`getInstalledDriversDirectory()`)
- Create: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/GpuDriverId.kt`
- Create tests: `Source/Android/app/src/test/java/org/dolphinemu/dolphinemu/model/GpuDriverMetadataTest.kt`,
  `…/utils/GpuDriverIdTest.kt`

- [ ] **Tests first** (`GpuDriverMetadataTest`): full v1 JSON parses; missing `schemaVersion`
      parses; unknown key ignored; missing `libraryName` → `libvulkan_freedreno.so`; missing
      `minApi` → 0; `schemaVersion: 2` → `SerializationException`; `synthesize("x","a.so")`
      round-trips through `parse`.
- [ ] **Tests first** (`GpuDriverIdTest`): `makeId("balemuni","v1.2","Apex A740.zip")` →
      `balemuni-v1.2-apex_a740`; empty prefix omits the dash; `.ZIP` stripped; only
      `[a-z0-9._-]` survive.
- [ ] Implement `GpuDriverMetadata` (kotlinx `Json { ignoreUnknownKeys = true; isLenient = true }`,
      `@Serializable` DTO with defaults, `parse(String)`, `deserialize(File)`, `synthesize`,
      `toJson()`), `GpuDriverId.makeId`.
- [ ] Implement `GpuDriverHelper`:
  - `data class InstalledGpuDriver(id, metadata, dir)`; `libraryFile`.
  - `listInstalled()`: migrate legacy first (see spec B.1), then scan `Installed/*/meta.json`,
    skip dirs whose `.so` is missing, sort by name.
  - `installFromStream(stream, id): GpuDriverInstallResult` per spec B.3.
  - `delete(id)`, `getInstalledMetadata(id)`.
  - `getSystemGpuInfo(): SystemGpuInfo?` from the 5-element JNI array (B3 extends JNI; until
    then tolerate a 2-element array).
  - Keep `supportsCustomDriverLoading()` and `getSystemDriverMetadata(context)`.
- [ ] `./gradlew :app:testDebugUnitTest` green.
- [ ] Commit: `Android: lenient GPU driver metadata + multi-slot install/list/delete`

### Task B3: JNI GPU info + catalog + recommendation (+ JVM tests)

**Files:**
- Modify: `Source/Android/jni/GpuDriver.cpp` (`getSystemDriverInfo` → 5 strings)
- Create: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/GpuDriverCatalog.kt`
- Create: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/GpuInfo.kt`
- Create tests: `…/utils/GpuDriverCatalogTest.kt`, `…/utils/GpuInfoTest.kt`

- [ ] **Tests first** (`GpuDriverCatalogTest`): a two-release fixture with `.zip` and `.txt`
      assets yields only zips; id uses prefix + tag + asset base; `browser_download_url`, size,
      `published_at` carried; malformed JSON → empty list; release without `assets` skipped.
- [ ] **Tests first** (`GpuInfoTest`): `recommend("Adreno (TM) 740", 0x5143)` → Balemuni;
      `"Adreno (TM) 650"` → purple-turnip; `"Adreno (TM) 830"` → crueter; `"Samsung Xclipse 940"`
      → ExynosTools; `"Mali-G715"` → null; vendor 0x5143 with unknown model → K11MCH1.
- [ ] JNI: append `properties.deviceName`, `fmt::format("{:#x}", properties.vendorID)`,
      `"{}.{}.{}"` of `properties.apiVersion` to the returned array (non-ARM stub returns 0).
- [ ] Implement `GpuDriverCatalog` (`DriverSource` list, `parseReleases`, `fetchAll`,
      `download` via `HttpURLConnection`, 15 s / 60 s timeouts, User-Agent
      `Dolphin-Android/<versionName>`), `GpuInfo.recommend`.
- [ ] Tests green. Commit: `Android: GPU driver catalog (GitHub releases) + GPU recommendation`

### Task B4: Settings screen

**Files:**
- Modify: `SettingsFragment.kt` (remove GPU_DRIVERS special-case; keep `askForDriverFile`; add
  `showDriverActionDialog`, `showDriverDownloadDialog`)
- Modify: `SettingsFragmentView.kt` (new callbacks), `SettingsFragmentPresenter.kt`
  (`addGpuDriverSettings`, `selectDriver`, `deleteDriver`, `downloadDrivers`, remove old
  `installDriver/useSystemDriver` bodies in favour of the new helper)
- Modify: `res/values/strings.xml`
- Optional: `MenuTag.kt` unchanged (GPU_DRIVERS exists)

- [ ] Presenter `loadSettingsList`: `MenuTag.GPU_DRIVERS -> addGpuDriverSettings(sl)`.
- [ ] `addGpuDriverSettings` builds the list per spec B.6 (header, system row, installed rows,
      install-from-file, download). Active marker from `GFX_DRIVER_PACKAGE`.
- [ ] Row tap → `fragmentView.showDriverActionDialog(driver)` (Use / Delete / Cancel).
- [ ] Download flow: `ThreadUtil.runOnThreadAndShowResult`-style progress → source picker →
      asset single-choice → download with `ProgressDialog`-equivalent (indeterminate is
      acceptable) → install → select → `loadSettingsList()`.
- [ ] Strings: `gpu_driver_header_gpu`, `gpu_driver_recommended`, `gpu_driver_recommended_system`,
      `gpu_driver_installed_header`, `gpu_driver_active_suffix`, `gpu_driver_install_from_file`,
      `gpu_driver_download`, `gpu_driver_download_fetching`, `gpu_driver_download_pick_source`,
      `gpu_driver_download_pick_asset`, `gpu_driver_downloading`, `gpu_driver_download_failed`,
      `gpu_driver_use`, `gpu_driver_delete`, `gpu_driver_deleted`, `gpu_driver_selected`.
- [ ] Build: `./gradlew :app:assembleDebug` compiles. Commit:
      `Android: GPU driver manager settings screen`

---

## Track C — Vulkan perf (main agent)

### Task C1: Policy helpers + tests

**Files:**
- Create: `Source/Core/VideoCommon/PostProcessing/ChainOutputPolicy.h`
- Create: `Source/UnitTests/VideoCommon/PostProcessing/ChainOutputPolicyTest.cpp`
- Modify: `Source/UnitTests/VideoCommon/CMakeLists.txt`

- [ ] Tests: `ShouldRenderChainDirectly({0,0,1920,1080},1920,1080)` true; offset rect false;
      half-width false; `ChooseDynamicRendering(true,true)` true; either false → false.
- [ ] Header with `constexpr` functions + `static_assert`s. Commit:
      `VideoCommon: chain output / dynamic rendering policy helpers`

### Task C2: D1 — API 1.3 + VK_KHR_dynamic_rendering + chain options

**Files:**
- Modify: `Source/Core/VideoBackends/Vulkan/VulkanContext.h/.cpp`
- Modify: `Source/Core/VideoBackends/Vulkan/VulkanEntryPoints.inl` (optional
  `vkGetPhysicalDeviceFeatures2`, `vkGetPhysicalDeviceFeatures2KHR`)
- Modify: `Source/Core/VideoBackends/Vulkan/LibrashaderPostProcessing.cpp` (chain create)

- [ ] `CreateVulkanInstance`: extend the ladder with `VK_API_VERSION_1_3`.
- [ ] `SelectDeviceExtensions`: `AddExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, false)`
      when `m_device_info.apiVersion < VK_API_VERSION_1_3`.
- [ ] `CreateDevice`: probe `dynamicRendering` via features2 (if the entry point loaded and
      either the extension was enabled or api ≥ 1.3); chain
      `VkPhysicalDeviceDynamicRenderingFeatures` into `device_info.pNext`; set
      `m_supports_dynamic_rendering`. Log.
- [ ] `LibrashaderPostProcessing`: build `filter_chain_vk_opt_t` and pass `&opt`; log
      `dynamic rendering on/off`.
- [ ] Host build of `videovulkan`; commit:
      `Vulkan: request API 1.3, enable dynamic rendering, pass librashader chain options`

### Task C3: D3 — deferred backbuffer clear

**Files:**
- Modify: `StateTracker.h/.cpp` (pending clear), `VKGfx.cpp` (`BindBackbuffer`,
  `PresentBackbuffer`)

- [ ] `StateTracker`: `SetPendingClear(VKFramebuffer*, const VkClearValue&)`,
      `DiscardPendingClear()`, `HasPendingClear() const`; in `BeginRenderPass()` when
      `m_pending_clear_fb == m_framebuffer` → `BeginClearRenderPass(full rect, values)` using
      the framebuffer's attachment layout (color [+depth] [+additional]) and reset.
- [ ] `BindBackbuffer`: `SetFramebuffer(fb); StateTracker::SetPendingClear(fb, clear)`.
- [ ] `PresentBackbuffer`: before `EndRenderPass`, if a clear is still pending for the swapchain
      framebuffer: `SetFramebuffer`, `BeginRenderPass`, `EndRenderPass`.
- [ ] Host build; commit: `Vulkan: defer the backbuffer clear into the first render pass`

### Task C4: D2 — direct-to-swapchain chain output

**Files:**
- Modify: `LibrashaderPostProcessing.cpp/.h`

- [ ] In `BlitFromTexture`: if `ShouldRenderChainDirectly(dst, fb_w, fb_h)` → output image is
      the framebuffer color attachment; `StateTracker::DiscardPendingClear()`; transition to
      `COLOR_ATTACHMENT_OPTIMAL`; run chain; `OverrideImageLayout(COLOR_ATTACHMENT_OPTIMAL)`;
      return (no blit). Else existing path.
- [ ] Host build; commit:
      `Vulkan/librashader: render the chain straight into the backbuffer when the draw rect is full`

---

## Track D — Integrate, build, verify

- [ ] `cmake --build build --target tests && ./build/Binaries/Tests/tests` green.
- [ ] `./gradlew :app:testDebugUnitTest` green.
- [ ] Signed release APK: `./gradlew :app:assembleRelease -Pkeystore=$HOME/.android/debug.keystore
      -Pstorepass=android -Pkeyalias=androiddebugkey -Pkeypass=android`; `adb install -r`.
- [ ] logcat: `dynamic rendering on`, driver load line, no validation errors / panics.
- [ ] Driver manager: migrated Turnip shows active; download Balemuni Aurora; select; relaunch;
      load line names the new library.
- [ ] Re-run the Track A matrix on the new APK (+ Balemuni). Write
      `docs/superpowers/specs/2026-09-13-thor-driver-and-postfx-perf-results.md`.
- [ ] Restore the device's original `GFX.ini`/`Logger.ini` (keep the user's selected driver).
