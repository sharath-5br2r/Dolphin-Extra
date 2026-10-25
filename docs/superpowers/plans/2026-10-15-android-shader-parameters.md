# Android Shader Parameter Editor — Implementation Plan

**Goal:** Give Android a shader parameter editor equivalent to the desktop
`ShaderParametersDialog`, with the editor's rules moved into shared, tested core code.

**Design:** [2026-10-15-android-shader-parameters-design.md](../specs/2026-10-15-android-shader-parameters-design.md)

**Method:** each task is red → green → refactor. A task is done when its tests were seen failing,
then passing, and the full `tests` / `qt-tests` suites still pass.

## Files

| File | Task |
| --- | --- |
| `Source/Core/VideoCommon/PostProcessing/ShaderParameterEditor.{h,cpp}` (new) | 1–3 |
| `Source/Core/VideoCommon/CMakeLists.txt` | 1 |
| `Source/UnitTests/VideoCommon/PostProcessing/ShaderParameterEditorTest.cpp` (new) | 1–3 |
| `Source/UnitTests/VideoCommon/CMakeLists.txt` | 1 |
| `Source/Core/DolphinQt/Config/Graphics/ShaderParametersDialog.{h,cpp}` | 4 |
| `Source/Android/jni/Config/ShaderParameterEditor.cpp` (new), `Config/PostProcessing.cpp`, `jni/CMakeLists.txt`, `AndroidCommon/IDCache.{h,cpp}` | 5 |
| `features/settings/model/ShaderParameterEditor.kt` (new), `PostProcessing.kt` | 5 |
| `features/settings/ui/shaderparameters/*` (new), `AndroidManifest.xml`, `strings.xml` | 6 |
| `features/settings/ui/SettingsFragmentPresenter.kt` | 7 |
| `src/test/.../ShaderParameterInputTest.kt` (new) | 6 |

## Task 1 — Slider mapping and availability (pure functions)

1. Write `ShaderParameterEditorTest.cpp` with:
   - `SliderMapping.WholeStepsGetOnePositionEach` (0..1 step 0.05 → 20 positions, increment 0.05)
   - `SliderMapping.CapsAtTenThousandAndStretchesIncrement` (0..1e6 step 1)
   - `SliderMapping.DegenerateRangesGetNoSlider` (step 0, step < 0, max == min, max < min,
     range narrower than one step)
   - `SliderMapping.NonMultipleRangeEndsExactlyAtMaximum` (0..1 step 0.3 → 3 positions; last = 1.0)
   - `SliderMapping.PositionNearestTheDefaultIsTheDefault`
   - `SliderMapping.PositionForValueRoundsAndClamps`
   - `Availability.*` ordering (preset, then library, then backend) and backend names
     (Vulkan/D3D/D3D12 yes; OGL/Metal/Software Renderer/Null/"" no).
2. Add the test to `Source/UnitTests/VideoCommon/CMakeLists.txt`; build; see it fail to compile.
3. Implement `MakeSliderMapping`, `SliderValueAt`, `SliderPositionFor`,
   `IsParameterEditingBackend`, `GetParameterEditingAvailability`; add files to
   `videocommon`. Tests green.

## Task 2 — Editor construction and queries

1. Tests: rows start at initial values; stored values applied; out-of-range stored value clamped;
   stored names not in the preset ignored; empty list → `NoParameters`; `Failed()` → `Error`
   with message and no rows; `IsDefault`; `FormatValue` uses `DecimalsForStep` and '.'.
2. Implement constructor, `Failed`, getters, `FormatValue`. Green.

## Task 3 — Editor mutators and persistence

1. Tests (Config fixture with a `NullLoader` base layer, as `LibrashaderParametersStorage`):
   `SetValue` clamps and persists the `name=value` string; unchanged value returns false and
   does not bump `CurrentGeneration`; `SetSliderPosition` maps through the slider; `Reset`
   removes the entry; `ResetAll` removes the key; `CollectOverrides` keeps preset order.
2. Implement mutators and `Open()` (`ResolveConfiguredPreset` → `ResolvePresetPath` →
   `KeyForPreset` → `Enumerate` → `Load`). Green.

## Task 4 — Desktop dialog onto the editor (refactor, no behaviour change)

1. `ShaderParametersDialog` keeps its widgets and holds a `std::unique_ptr<ShaderParameterEditor>`.
   Slider ranges come from `GetRows()[i].slider`, slider edits call `SetSliderPosition`, spin
   edits `SetValue`, Reset/Reset All the editor; the dialog only refreshes widgets.
2. Delete the dialog's `MAX_SLIDER_STEPS`, slider math, `ApplyOverrides`, `CollectOverrides`,
   `IsDefault`, `SaveOverrides`.
3. Build `dolphin-emu`, run `qt-tests` and `tests`.

## Task 5 — JNI bridge

1. `ShaderParameterEditor.kt`: `@Keep private val pointer`, `external` members per the design's
   §4.2 table, `companion fun open(spec)`.
2. `jni/Config/ShaderParameterEditor.cpp` implementing them; register the class and pointer
   field in `IDCache`; add the file to `jni/CMakeLists.txt`.
3. `PostProcessing.getParameterEditingAvailability(backend, spec)`.
4. Constants for status and availability shared through companion objects, mirroring the C++
   enum order (documented on both sides).

## Task 6 — Compose screen

1. `ShaderParameterInput.parse(text): Float?` + `ShaderParameterInputTest` (JVM): accepts
   `1.5`, `1,5`, `-0.25`, surrounding blanks; rejects `""`, `abc`, `1.2.3`, `NaN`, `Infinity`.
2. `ShaderParametersViewModel` (`UiState` = `Loading | LoadFailed(reason) | NoParameters | Loaded(rows)`),
   `ShaderParametersScreen`, `ShaderParametersActivity` (`launch(context, preset, perGame)`,
   `onStop` saves the base layer), manifest entry, strings.

## Task 7 — Settings entry row

1. Row under the picker per design §4.3, availability from `PostProcessing`,
   `StringSetting.MAIN_GFX_BACKEND`, `StringSetting.GFX_ENHANCE_POST_SHADER`.
2. Per-game detection from `settings.isGameSpecific`.

## Task 8 — Verification

1. Windows: `dolphin-emu`, `tests`, `qt-tests` build and pass.
2. Android: `./gradlew assembleDebug testDebugUnitTest` (needs the Android SDK/NDK), then the
   manual checklist in design §5.

## Verification results (2026-10-15)

| Check | Result |
| --- | --- |
| `tests` (Windows, MSVC 19.51) | 1540 passed, 2 skipped; includes 26 new `SliderMapping` / `ParameterEditingAvailability` / `ShaderParameterEditor*` tests |
| `qt-tests` (`-platform windows`) | 192 passed |
| `dolphin-emu` (Windows) | builds with the dialog on the shared editor |
| `./gradlew :app:testDebugUnitTest` | `ShaderParameterInputTest` 5/5; existing 33 JVM tests pass |
| `./gradlew :app:assembleDebug` (NDK 30.0.15729638, arm64-v8a + x86_64) | builds; `libmain.so` exports all 18 `ShaderParameterEditor` natives and `getParameterEditingAvailability` |
| Manual device checklist (design §5) | run on an AYN Thor (Snapdragon 8 Gen 2, Android 13), see below |

### On-device results (AYN Thor, debug APK, test preset `uitest/param-test`)

| Check | Result |
| --- | --- |
| Row with no effect selected | "Select a post-processing effect first."; tap shows it as a toast |
| Row on OpenGL with a preset | "Requires the Vulkan video backend." (librashader loaded, so the library check passed) |
| Row on Vulkan | available; opens the screen in ~300 ms |
| Preset librashader cannot parse | "Could not load preset: PreprocessError(…)" shown in place of the list |
| Rows | description as title with the id below; id not repeated when equal; precision per step (`1.00`, `0.0`, `1`) |
| Slider, whole steps | 0.05-step parameter lands on `1.50` |
| Slider, range not a multiple of the step (0..1 step 0.3) | end position gives `1.0`, not `0.9`; no crash |
| Slider, 1,000,000 whole steps | capped: mid-track gives `499500` |
| Empty range | no slider, value button only |
| Typed entry | `abc` rejected with "Enter a number."; `2,0` accepted as `2` |
| Leaving the screen | `GFX.ini` gets `[LibrashaderParameters] uitest/param-test.slangp = brightness=1.500000;…` |
| App restart | values restored |
| Row Reset / Reset All | value back to default; Reset All removes the key from `GFX.ini` |
| Crashes / errors in logcat | none |

**Bug found and fixed:** the floating Reset All button covered the value and Reset controls of the
row beneath it. Reset All moved into a header row at the top of the list.

**Not exercised on the device:** the per-game note and a live change while a game runs. The debug
app had no games, so there was no per-game settings page and no running game.
