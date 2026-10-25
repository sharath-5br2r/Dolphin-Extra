# Android Shader Parameter Editor — Design

**Date:** 2026-10-15
**Status:** approved for implementation (scope decided in §3)
**Related:** [librashader desktop runtimes](2026-09-16-librashader-desktop-runtimes-design.md),
[librashader Android post-processing](2026-08-22-librashader-android-postprocessing-design.md)
**Reference implementation:** the desktop editor,
`Source/Core/DolphinQt/Config/Graphics/ShaderParametersDialog.{h,cpp}`

## 1. Problem

On Windows and macOS, the Graphics → Enhancements page has a **Shader Parameters…** button that
opens an editor for the selected librashader preset's `#pragma parameter` values (scanline
strength, mask type, …): one row per parameter with a slider, a numeric field and a Reset, plus
Reset All. Edits are stored globally and picked up by a running game without a reload.

Android lets the user pick a preset but offers no way to tune it. Presets such as crt-royale
are only usable at their defaults.

## 2. What is already established

### 2.1 The parameter model is in core and already Android-ready

`VideoCommon::LibrashaderParameters` (`VideoCommon/PostProcessing/LibrashaderParameters.h`)
owns everything that is not a widget:

| Function | Role |
| --- | --- |
| `Enumerate(path, &infos, &error)` | preset-wide parameter list via librashader; never touches a GPU |
| `KeyForPreset(path, spec)` | the `[LibrashaderParameters]` key a preset's values live under |
| `Load(key)` / `Save(key, overrides)` | base-layer `GFX` storage; only non-default values are kept |
| `CurrentGeneration()` | bumped by `Save`; `LibrashaderPostProcessing` polls it per frame |
| `DecimalsForStep`, `IsDefaultValue` | display precision and "is this the default" |

`videocommon` is already linked into the Android JNI library, `librashader.so` ships for
`arm64-v8a` (`Source/Android/app/src/main/jniLibs`), and the Vulkan backend has a librashader
runtime. Live reload therefore already works on Android: a `Save` from any thread is seen by
the running chain on its next frame.

### 2.2 What is *not* in core: the editor's own rules

The desktop dialog holds model logic that Android needs identically and that has no tests:

1. **Slider mapping.** A slider gets one integer position per whole step, capped at
   `MAX_SLIDER_STEPS = 10000` (the increment stretches past the cap). The last position is the
   maximum exactly; the position nearest the default *is* the default (float noise on wide ranges
   would otherwise leave Reset enabled). A range narrower than one step, a non-positive step or
   an inverted range gets no slider.
2. **Value clamping.** A stored value outside the declared range is clamped on load; the numeric
   field clamps to the range when the parameter has one.
3. **Collecting overrides.** Only values that differ from the default (`IsDefaultValue`) are
   saved, so Reset removes the entry instead of pinning the default.

### 2.3 Android constraints found in the code

- **Material `Slider` cannot be reused.** The settings slider dialog
  (`SettingsAdapter.onSliderClick`) passes `valueFrom/valueTo/stepSize` straight through;
  Material throws when the range is not a multiple of the step or the value is off-grid, which
  shader presets routinely are. The editor must drive a slider over integer positions (§2.2.1).
- **Settings are written to disk by the settings screen.** `Settings.saveSettings()` saves the
  base layer only for global settings and the game layer only for per-game settings
  (`Settings.kt:52-62`). `LibrashaderParameters::Save` writes the base layer in memory. A screen
  reached from per-game settings would never reach disk unless it saves the base layer itself.
- **Default backend.** Android lists OpenGL first (`VideoBackendBase.cpp:215`). On GLES,
  `OGLLibrashaderRuntime::IsSupported()` is false and the built-in engine runs; it ignores
  stored parameter values. The editor is therefore only meaningful on Vulkan.
- **Native handle pattern.** Stateful native objects are a Kotlin class with a `@Keep private
  val pointer: Long` whose field ID is cached in `IDCache` (`RiivolutionPatches`,
  `GraphicsModGroup`). Compose screens follow `NetplaySetupActivity`: an `AppCompatActivity`
  with `DolphinTheme { … }` and a `ViewModel`.

## 3. Scope

**In scope**

- Android editor for the configured preset's parameters, reachable from the Graphics →
  Enhancements settings page (global and per-game), usable while a game runs.
- Rendering pipelines in scope for parameter editing: **Vulkan**, plus **D3D11/D3D12** on
  Windows (already served by the desktop dialog, unchanged here).
- A shared core `ShaderParameterEditor` holding the §2.2 rules, used by Android and by the
  desktop dialog so the two cannot drift.

**Out of scope**

- OpenGL ES (explicitly not a target of the librashader pipeline) and the built-in engine.
- An in-game overlay; the editor is a settings screen (values still apply live).
- Per-game parameter values. As on desktop, values are global; the screen says so when opened
  from per-game settings.
- Changing desktop's button gating (desktop OGL and Metal keep working as today).

## 4. Design

### 4.1 Core: `VideoCommon::ShaderParameterEditor`

New files `VideoCommon/PostProcessing/ShaderParameterEditor.{h,cpp}`.

```cpp
namespace VideoCommon
{
struct SliderMapping
{
  int positions = 0;           // 0 = no slider (degenerate range); else the last position.
  float increment = 0.0f;      // value per position
};
SliderMapping MakeSliderMapping(const LibrashaderParameters::ParameterInfo& info);
float SliderValueAt(const ParameterInfo& info, const SliderMapping& m, int position);
int SliderPositionFor(const ParameterInfo& info, const SliderMapping& m, float value);

enum class ParameterEditingAvailability { Available, NoPreset, LibraryUnavailable,
                                          UnsupportedBackend };
ParameterEditingAvailability GetParameterEditingAvailability(bool library_available,
                                                             std::string_view video_backend,
                                                             std::string_view configured_preset);
bool IsParameterEditingBackend(std::string_view video_backend);  // Vulkan, D3D, D3D12

class ShaderParameterEditor
{
public:
  enum class Status { Ok, NoParameters, Error };
  struct Row { ParameterInfo info; SliderMapping slider; float value; };

  // Production: resolve the configured spec, enumerate, apply stored values.
  static std::unique_ptr<ShaderParameterEditor> Open(const std::string& configured_preset);
  // Test seam and the tail of Open(): a known key, list and stored values.
  ShaderParameterEditor(std::string key, std::vector<ParameterInfo> infos,
                        const LibrashaderParameters::Overrides& stored);
  static std::unique_ptr<ShaderParameterEditor> Failed(std::string key, std::string error);

  Status GetStatus() const; const std::string& GetError() const; const std::string& GetKey() const;
  const std::vector<Row>& GetRows() const;
  int GetSliderPosition(size_t index) const;
  bool IsDefault(size_t index) const;
  std::string FormatValue(size_t index) const;   // fixed, DecimalsForStep places, '.' separator

  // Each mutator persists through LibrashaderParameters::Save and returns whether the value
  // changed (an unchanged value does not write or bump the generation).
  bool SetValue(size_t index, float value);       // clamps to the range when there is one
  bool SetSliderPosition(size_t index, int position);
  bool Reset(size_t index);
  bool ResetAll();
  LibrashaderParameters::Overrides CollectOverrides() const;
};
}
```

Rules, each a unit test:

- `MakeSliderMapping`: `has_range = step > 0 && max > min`; `whole = round((max-min)/step)`;
  `whole < 1` → `positions = 0`; else `positions = min(whole, 10000)` and `increment = step`
  if `whole ≤ 10000`, otherwise `(max-min)/10000`.
- `SliderValueAt`: last position → `max`; otherwise `min + p*increment`, snapped to `initial`
  when within half an increment.
- `SliderPositionFor`: `lround((v-min)/increment)` clamped to `[0, positions]`.
- Constructor: each row starts at the stored value if present (clamped when `max > min`), else
  `initial`. Status is `NoParameters` when `infos` is empty.
- `SetValue`: clamps to `[min, max]` when `has_range`; ignores a change below `1e-7`.
- `CollectOverrides`: non-default rows only, in preset order.
- Availability: empty preset → `NoPreset`; then library → `LibraryUnavailable`; then backend
  → `UnsupportedBackend`. Backend names are the `CONFIG_NAME`s `"Vulkan"`, `"D3D"`, `"D3D12"`.

The desktop dialog is moved onto the editor (its widgets unchanged): the mapping math,
`ApplyOverrides`, `CollectOverrides` and `IsDefault` disappear from DolphinQt.

### 4.2 JNI

`Source/Android/jni/Config/ShaderParameterEditor.cpp`, class
`org.dolphinemu.dolphinemu.features.settings.model.ShaderParameterEditor`:

| Kotlin | Native |
| --- | --- |
| `companion open(presetSpec): ShaderParameterEditor` → `initialize(spec): Long` | `ShaderParameterEditor::Open` |
| `finalize()` | `delete` |
| `status: Int`, `error: String`, `parameterCount: Int` | getters |
| `getName/getDescription/getValueText(i)`, `getValue/getMinimum/getMaximum/getInitial(i)` | row fields |
| `getSliderPositions(i)`, `getSliderPosition(i)`, `isDefault(i)` | |
| `setSliderPosition(i, p)`, `setValue(i, v)`, `reset(i)`, `resetAll()` → `Boolean` | mutators |

`PostProcessing.getParameterEditingAvailability(backend, presetSpec): Int` wraps the core
function with `Librashader::GetAvailability().available`. The pointer field is registered in
`IDCache` like `RiivolutionPatches`.

### 4.3 Android UI

- **Entry row.** `SettingsFragmentPresenter`, directly under the post-processing picker:
  `RunRunnable` "Shader Parameters", `worksDuringEmulation = true`. Its subtitle is the
  availability reason when unavailable ("Select a post-processing shader first", "Requires the
  Vulkan backend", "The librashader library could not be loaded on this device"), otherwise
  "Adjust the selected shader's settings". Tapping when unavailable shows the reason as a toast;
  otherwise it launches the screen with the configured preset and whether the settings are
  per-game.
- **`ShaderParametersActivity`** (Compose, `DolphinTheme`, edge-to-edge, `DolphinScaffold`).
  Title "Shader Parameters - <preset leaf>" (the desktop title), back. The list opens with a
  header row: the note "Parameter values are global…" when opened from per-game settings, and a
  Reset All button, enabled while any value differs from its default. (`DolphinScaffold` has no
  app-bar actions slot. A floating button was tried first and dropped after device testing: it
  sits over the right-hand column, which is where every row keeps its value and Reset controls.)
  Body states: loading (spinner), error / no-parameters (centred message), list.
- **Row.** Label = description (fallback: name), secondary text = name. A `Slider` over
  `0f..positions` with no tick steps (thousands of ticks are unusable), rounded to an integer
  position on change. The value text is a button that opens a numeric entry dialog (parses
  `.` or `,`); a parameter without a slider shows only that button. A Reset icon button,
  enabled when not default.
- **`ShaderParametersViewModel`.** Opens the editor on `Dispatchers.IO`, then exposes an
  immutable `UiState`. Every edit calls the editor on the main thread and re-reads the row.
- **Persistence.** `onStop` calls `NativeConfig.save(NativeConfig.LAYER_BASE)` (§2.3), so values
  reach `GFX.ini` regardless of which settings page opened the screen.

### 4.4 Error handling

| Situation | Behaviour |
| --- | --- |
| Preset fails to enumerate | message "Could not load preset: …" and Reset All disabled |
| Preset has no parameters | "This preset has no adjustable parameters." |
| Stored value out of range | clamped on open; the clamped value is saved on the next edit |
| Numeric entry not a number | dialog keeps the field and shows an error |
| Backend switched to OGL after opening | editor still edits; values apply once on Vulkan |

## 5. Testing

- **C++ (gtest, `Source/UnitTests/VideoCommon/PostProcessing/ShaderParameterEditorTest.cpp`),
  written first:** slider mapping (normal, capped, degenerate, inverted, non-multiple range,
  snap-to-default, last-position-is-max, round trip), availability ordering and backend names,
  editor construction (stored, clamped, unknown stored names ignored, empty list), mutators
  (clamp, no-op on unchanged, reset, reset all, persisted string, generation bump only on
  change), `FormatValue`.
- **Kotlin JVM unit test** for the numeric entry parser (`.`/`,`, blanks, garbage).
- **Desktop regression:** existing `qt-tests` and `tests` pass after the dialog moves onto the
  editor.
- **Manual (device):** Vulkan + crt-royale: open from global and per-game settings; drag, type,
  reset, reset all; values survive an app restart; live change visible after returning to the
  game; OpenGL shows the "Requires the Vulkan backend" reason.
