# Declarative UI Slice 0: Foundation — Implementation Plan

**Status:** Complete

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the config-binding layer, its test harness, and the translation-extraction tooling that every later `.ui` migration slice depends on, with no user-visible change.

**Architecture:** Config binding moves from widget subclasses (`ConfigControl<Derived>`, which takes its config location as a constructor argument and therefore cannot be produced by `uic`) to bind-after-construction over stock Qt widgets. A `ConfigBinding : QObject` is attached as a child of each bound widget, holding the location, layer, re-entrancy guard and description text, connected to a config-change relay and installed as the widget's event filter. Pure config and arithmetic logic is extracted into Qt-free headers tested in the existing `tests` binary; widget round-trip behaviour is tested in a new `qt-tests` binary.

**Tech Stack:** C++20, Qt 6.8.3 Widgets, CMake, gtest, gettext/xgettext, `uic`.

**Spec:** `docs/superpowers/specs/2026-09-18-declarative-desktop-ui-design.md`

## Global Constraints

- Target Qt 6.8.3. Do **not** require Qt 6.10, `Qt6Test`, `Qt6UiPlugin`, `Qt6Designer`, `Qt6UiTools`, or LinguistTools (`lupdate`/`lrelease`). None are in the bundled Windows Qt at `Externals/Qt/Qt6.8.3/x64`.
- Do not fork or modify the `Externals/Qt` submodule.
- `Config::Info<T>` is copy-constructible but has deleted move and deleted assignment. Store it by value via member-init list only.
- Every new file carries `// Copyright 2026 Dolphin Emulator Project` and `// SPDX-License-Identifier: GPL-2.0-or-later`.
- No PCSX2 source, XML, stylesheet, icon or asset is copied. Reference only.
- `ConfigControls/` and `ToolTipControls/` must remain in the tree, compiling and functional, for the whole of this slice. Nothing in this slice deletes or edits them.
- The existing `tests` binary must remain Qt-free. Never add Qt to it or to the `add_dolphin_test` macro.
- No user-visible behaviour change in this slice. `dolphin-emu` must look and behave identically before and after.
- xgettext keyword spec for `.ui` strings is exactly `--keyword=translate:2 --keyword=translate:2,3c`. Using `translate:2,3c` alone silently drops every string `uic` emits with a `nullptr` third argument.

---

## File Structure

**New, in `Source/Core/DolphinQt/Config/Binder/`** (a new subdirectory; the binder becomes its own CMake target so `qt-tests` can link it without dragging in the rest of DolphinQt):

| File | Responsibility |
|---|---|
| `ConfigBindingLogic.h` | Header-only, **Qt-free**. Read/write/is-local/clear against `Config::Info<T>` and an optional `Config::Layer*`. The layer-vs-global decision logic, isolated so it can be tested in the Qt-free `tests` binary. |
| `ConfigSliderMapping.h` | Header-only, **Qt-free**, pure arithmetic. Float↔integer slider position mapping with min/max/step. |
| `ConfigChangeBroadcaster.h/.cpp` | A `QObject` relay with a single `Changed()` signal. Production wires `Settings::ConfigChanged` into it; `qt-tests` triggers it directly. Keeps the binder from depending on `Settings` (1,016 LOC, 33 includes). |
| `ConfigBinding.h/.cpp` | `ConfigBinding : QObject` — the non-template base holding location, layer, `m_updating` guard, description text, and the event filter. Template subclasses (no `Q_OBJECT`) hold the typed `Config::Info<T>`. |
| `ConfigWidgetBinder.h/.cpp` | The public `ConfigWidget::Bind(...)` / `SetDescription(...)` API. The only header pane code includes. |
| `ConfigSettingRegistry.h/.cpp` | Records each bind keyed by `Config::Location`, for the OSD follow-up's settings browser. |
| `BalloonTipFilter.h/.cpp` | The tooltip event-filter behaviour and the per-widget-type tooltip anchor position, re-homed from `ToolTipWidget`. |

**New tests:**

| File | Responsibility |
|---|---|
| `Source/UnitTests/Core/Config/ConfigBindingLogicTest.cpp` | Layer-vs-global read/write semantics. Runs in `tests`. |
| `Source/UnitTests/Core/Config/ConfigSliderMappingTest.cpp` | Float/int mapping boundaries. Runs in `tests`. |
| `Source/QtTests/CMakeLists.txt` | The `qt-tests` target. |
| `Source/QtTests/QtTestsMain.cpp` | `QApplication` + gtest entry point. |
| `Source/QtTests/SignalRecorder.h` | ~15-line stand-in for the absent `QSignalSpy`. |
| `Source/QtTests/ConfigChangeBroadcasterTest.cpp` | The relay delivers, and stops delivering after the last binding dies. |
| `Source/QtTests/ConfigBinderRoundTripTest.cpp` | Per-widget-type round-trip, global and layered. |
| `Source/QtTests/ConfigBinderMappedTest.cpp` | Mapped combos, string choices, tick sliders, scaled sliders. |
| `Source/QtTests/ConfigBinderFloatAndPathTest.cpp` | Float-slider quantisation, label mirroring, user paths. |
| `Source/QtTests/ConfigBinderOverrideTest.cpp` | Bold-when-overridden, right-click-clears-key. |
| `Source/QtTests/BalloonTipFilterTest.cpp` | Tooltip show/hide conditions and the per-widget-type anchor. |
| `Source/QtTests/ConfigComplexBindingTest.cpp` | Two-location combos: read fallback, both-keys clear, either-key bold. |
| `Source/QtTests/ConfigSettingRegistryTest.cpp` | Registry enumeration. |
| `Languages/tests/UiStringFixture.ui` | Fixture form covering the `nullptr`-context case, the disambiguation case and an `extracomment`. |
| `Languages/tests/PainterFixture.cpp` | Not compiled. Proves the new keywords do not extract `QPainter::translate` calls. |
| `Languages/tests/test-ui-extraction.sh` | Asserts the fixture's strings, `msgctxt` and translator comment appear in extracted output. |

**New tooling:**

| File | Responsibility |
|---|---|
| `Languages/generate-ui-strings.py` | Runs `uic` over every `.ui` file into a scratch tree and re-injects the `// i18n:` comments `uic` discards, so `xgettext` can see form strings. |

**Modified:**

| File | Change |
|---|---|
| `Source/Core/DolphinQt/CMakeLists.txt:9-10` | Add `set(CMAKE_AUTOUIC ON)` beside `AUTOMOC`/`AUTORCC`; add the binder library target; link it into `dolphin-emu`. |
| `Source/CMakeLists.txt:88-89` | Add `add_subdirectory(QtTests)` under `ENABLE_TESTS` and `ENABLE_QT`. |
| `Source/UnitTests/Core/CMakeLists.txt` | Register the two new Qt-free tests. |
| `Source/Core/DolphinQt/Settings.cpp` | One `connect` wiring `ConfigChanged` into `ConfigChangeBroadcaster`. |
| `Languages/update-source-strings.sh` | Call `generate-ui-strings.py`, add the generated headers to the `xgettext` file list with the two `translate` keywords, and rewrite the resulting `#:` references back to the `.ui` files. |
| `.gitignore` | Ignore `/Languages/.ui-scratch/`. |

**Task order.** Tasks 1 to 11 are strictly sequential: each builds on the interfaces the previous
one produced. Task 12 touches no C++ and can be done at any point, including first.

| # | Deliverable | Tests after |
|---|---|---|
| 1 | `ConfigBindingLogic.h` — Qt-free read/write/is-local/clear | 8 in `tests` |
| 2 | `ConfigSliderMapping.h` — Qt-free float↔position arithmetic | 14 in `tests` |
| 3 | `AUTOUIC`, the binder target, `ConfigChangeBroadcaster`, the `qt-tests` target | 2 in `qt-tests` |
| 4 | `ConfigBinding` base + `CheckBoxBinding` + `FindBinding` | 9 |
| 5 | `ValueBinding<Widget, T>` + combo, spin, slider, radio, line-edit | 16 |
| 6 | Mapped combos, string choices, tick sliders, scaled sliders | 25 |
| 7 | Float sliders, label mirroring, user paths | 32 |
| 8 | Font mirroring and right-click-to-clear | 38 |
| 9 | `BalloonTipFilter` and the tooltip anchors | 48 |
| 10 | `ComplexBinding` — one control, two config locations | 58 |
| 11 | `ConfigSettingRegistry` | 67 |
| 12 | `.ui` string extraction | `test-ui-extraction.sh` |

**Why no pane is migrated in this slice:** the binder's first production consumer is slice 5. `qt-tests` round-trips against real `Config` layers and real Qt widgets, so it is genuine validation rather than a mock, and pulling a pane forward would migrate it before the settings shell it lives in.

---

## Task 1: Qt-free config binding logic

**Files:**
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigBindingLogic.h`
- Create: `Source/UnitTests/Core/Config/ConfigBindingLogicTest.cpp`
- Modify: `Source/UnitTests/Core/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: namespace `ConfigWidget::Logic` with, for any `T`:
  - `T ReadValue(const Config::Info<T>& setting, Config::Layer* layer)`
  - `void WriteValue(const Config::Info<T>& setting, const Config::Location& location, Config::Layer* layer, const T& value)`
  - `bool IsLocal(const Config::Location& location, Config::Layer* layer)`
  - `void ClearLocal(const Config::Location& location, Config::Layer* layer)`

These four replicate `ConfigControl::ReadValue`, `SaveValue`, `IsConfigLocal` and the right-click branch of `mousePressEvent` respectively, with the layer decision made explicit and testable.

- [x] **Step 1: Write the failing test**

Create `Source/UnitTests/Core/Config/ConfigBindingLogicTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"

namespace
{
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderTest", "Int"}, 7};
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderTest", "Bool"}, false};

class ConfigBindingLogicTest : public ::testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }
};
}  // namespace

TEST_F(ConfigBindingLogicTest, GlobalReadFallsBackToDefault)
{
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 7);
}

TEST_F(ConfigBindingLogicTest, GlobalWriteThenReadRoundTrips)
{
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), nullptr, 42);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 42);
}

TEST_F(ConfigBindingLogicTest, LayerReadFallsBackToBaseWhenKeyAbsent)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 11);
}

TEST_F(ConfigBindingLogicTest, LayerReadPrefersLayerValueWhenKeyPresent)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), &layer, 99);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 99);
  EXPECT_EQ(Config::GetBase(TEST_INT), 11) << "writing to a layer must not touch the base layer";
}

TEST_F(ConfigBindingLogicTest, IsLocalIsFalseForUntouchedGlobalSetting)
{
  EXPECT_FALSE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), nullptr));
}

TEST_F(ConfigBindingLogicTest, IsLocalIsTrueOnceLayerKeyExists)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  EXPECT_FALSE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), &layer));
  ConfigWidget::Logic::WriteValue(TEST_BOOL, TEST_BOOL.GetLocation(), &layer, true);
  EXPECT_TRUE(ConfigWidget::Logic::IsLocal(TEST_BOOL.GetLocation(), &layer));
}

TEST_F(ConfigBindingLogicTest, ClearLocalRemovesTheLayerKeyAndRestoresBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::WriteValue(TEST_INT, TEST_INT.GetLocation(), &layer, 99);
  ConfigWidget::Logic::ClearLocal(TEST_INT.GetLocation(), &layer);
  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, &layer), 11);
}

TEST_F(ConfigBindingLogicTest, ClearLocalOnGlobalBindingIsANoOp)
{
  Config::SetBase(TEST_INT, 11);
  ConfigWidget::Logic::ClearLocal(TEST_INT.GetLocation(), nullptr);
  EXPECT_EQ(ConfigWidget::Logic::ReadValue(TEST_INT, nullptr), 11);
}
```

Register it in `Source/UnitTests/Core/CMakeLists.txt`, after the existing `add_dolphin_test(PatchAllowlistTest ...)` line:

```cmake
add_dolphin_test(ConfigBindingLogicTest Config/ConfigBindingLogicTest.cpp)
```

The test target must be able to find `DolphinQt/…` headers. In the same file, immediately after that line:

```cmake
target_include_directories(ConfigBindingLogicTest PRIVATE ${CMAKE_SOURCE_DIR}/Source/Core)
```

- [x] **Step 2: Run the test to verify it fails**

Run:
```bash
cmake --build build --target tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time with `fatal error: 'DolphinQt/Config/Binder/ConfigBindingLogic.h' file not found`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/ConfigBindingLogic.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"

// Qt-free half of the config binder: everything about deciding *which* config layer a bound
// widget reads from and writes to. Kept free of Qt so it can be tested in the Qt-free `tests`
// binary. Mirrors the semantics of ConfigControl::ReadValue / SaveValue / IsConfigLocal.
namespace ConfigWidget::Logic
{
// A null `layer` means the binding edits global config. A non-null `layer` means it edits a
// per-game layer, falling back to the base layer for display when the key is absent there.
template <typename T>
T ReadValue(const Config::Info<T>& setting, Config::Layer* layer)
{
  if (layer != nullptr)
  {
    if (layer->Exists(setting.GetLocation()))
      return layer->Get(setting);
    // There is no way to know which game is being edited, so GlobalGame settings cannot be
    // shown; the base layer is the closest meaningful value.
    return Config::GetBase(setting);
  }
  return Config::Get(setting);
}

template <typename T>
void WriteValue(const Config::Info<T>& setting, const Config::Location& location,
                Config::Layer* layer, const T& value)
{
  if (layer != nullptr)
  {
    layer->Set(location, value);
    Config::OnConfigChanged();
    return;
  }
  Config::SetBaseOrCurrent(setting, value);
}

// Whether the value shown comes from somewhere other than the base layer. Drives the bold font
// that marks an overridden setting.
inline bool IsLocal(const Config::Location& location, Config::Layer* layer)
{
  if (layer != nullptr)
    return layer->Exists(location);
  return Config::GetActiveLayerForConfig(location) != Config::LayerType::Base;
}

// Drops a per-game override so the setting reverts to the global value. A no-op for global
// bindings, which have no override to drop.
inline void ClearLocal(const Config::Location& location, Config::Layer* layer)
{
  if (layer == nullptr)
    return;
  layer->DeleteKey(location);
  Config::OnConfigChanged();
}
}  // namespace ConfigWidget::Logic
```

- [x] **Step 4: Run the test to verify it passes**

Run:
```bash
cmake --build build --target tests -j8 && ./build/Binaries/Tests/tests --gtest_filter='ConfigBindingLogicTest.*'
```
Expected: PASS, 8 tests.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigBindingLogic.h \
        Source/UnitTests/Core/Config/ConfigBindingLogicTest.cpp \
        Source/UnitTests/Core/CMakeLists.txt
git commit -m "DolphinQt: extract Qt-free config binding logic

Isolates the layer-vs-global read/write decision from ConfigControl so it can
be unit-tested in the Qt-free tests binary. Semantics match
ConfigControl::ReadValue, SaveValue and IsConfigLocal exactly; no behaviour
change, no existing call site touched.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 2: Float slider mapping arithmetic

**Files:**
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigSliderMapping.h`
- Create: `Source/UnitTests/Core/Config/ConfigSliderMappingTest.cpp`
- Modify: `Source/UnitTests/Core/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `struct ConfigWidget::FloatSliderRange { float minimum; float maximum; float step; }` with
  - `int PositionForValue(float value) const`
  - `float ValueForPosition(int position) const`
  - `int PositionCount() const`

`QSlider` positions are integers, so a float setting bound to one needs a stable quantisation in both directions. This is the arithmetic `ConfigFloatSlider` does inline; extracting it makes the boundary behaviour testable.

- [x] **Step 1: Write the failing test**

Create `Source/UnitTests/Core/Config/ConfigSliderMappingTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/ConfigSliderMapping.h"

namespace
{
constexpr ConfigWidget::FloatSliderRange UNIT_RANGE{0.0f, 1.0f, 0.01f};
constexpr ConfigWidget::FloatSliderRange SCALE_RANGE{0.5f, 2.0f, 0.25f};
}  // namespace

TEST(ConfigSliderMapping, MinimumAndMaximumMapToTheEndPositions)
{
  EXPECT_EQ(UNIT_RANGE.PositionForValue(0.0f), 0);
  EXPECT_EQ(UNIT_RANGE.PositionForValue(1.0f), UNIT_RANGE.PositionCount());
  EXPECT_FLOAT_EQ(UNIT_RANGE.ValueForPosition(0), 0.0f);
  EXPECT_FLOAT_EQ(UNIT_RANGE.ValueForPosition(UNIT_RANGE.PositionCount()), 1.0f);
}

TEST(ConfigSliderMapping, PositionCountIsTheNumberOfSteps)
{
  EXPECT_EQ(UNIT_RANGE.PositionCount(), 100);
  EXPECT_EQ(SCALE_RANGE.PositionCount(), 6);
}

TEST(ConfigSliderMapping, RoundTripsEveryStepExactly)
{
  for (int position = 0; position <= SCALE_RANGE.PositionCount(); ++position)
    EXPECT_EQ(SCALE_RANGE.PositionForValue(SCALE_RANGE.ValueForPosition(position)), position)
        << "position " << position;
}

TEST(ConfigSliderMapping, SnapsAnOffStepValueToTheNearestPosition)
{
  // 0.6 sits between the 0.5 and 0.75 steps, nearer 0.5.
  EXPECT_EQ(SCALE_RANGE.PositionForValue(0.6f), 0);
  // 0.7 is nearer 0.75.
  EXPECT_EQ(SCALE_RANGE.PositionForValue(0.7f), 1);
}

TEST(ConfigSliderMapping, ClampsValuesOutsideTheRange)
{
  EXPECT_EQ(SCALE_RANGE.PositionForValue(-5.0f), 0);
  EXPECT_EQ(SCALE_RANGE.PositionForValue(100.0f), SCALE_RANGE.PositionCount());
}

TEST(ConfigSliderMapping, ClampsPositionsOutsideTheRange)
{
  EXPECT_FLOAT_EQ(SCALE_RANGE.ValueForPosition(-3), 0.5f);
  EXPECT_FLOAT_EQ(SCALE_RANGE.ValueForPosition(999), 2.0f);
}
```

Register it in `Source/UnitTests/Core/CMakeLists.txt`, below the Task 1 entry:

```cmake
add_dolphin_test(ConfigSliderMappingTest Config/ConfigSliderMappingTest.cpp)
target_include_directories(ConfigSliderMappingTest PRIVATE ${CMAKE_SOURCE_DIR}/Source/Core)
```

- [x] **Step 2: Run the test to verify it fails**

Run:
```bash
cmake --build build --target tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time with `'DolphinQt/Config/Binder/ConfigSliderMapping.h' file not found`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/ConfigSliderMapping.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

namespace ConfigWidget
{
// QSlider positions are integers, so a float setting bound to a slider needs a quantisation that
// round-trips. Position 0 is `minimum`; each position advances by `step`.
struct FloatSliderRange
{
  float minimum;
  float maximum;
  float step;

  constexpr int PositionCount() const
  {
    return static_cast<int>(std::lround((maximum - minimum) / step));
  }

  int PositionForValue(float value) const
  {
    const float clamped = std::clamp(value, minimum, maximum);
    const int position = static_cast<int>(std::lround((clamped - minimum) / step));
    return std::clamp(position, 0, PositionCount());
  }

  float ValueForPosition(int position) const
  {
    const int clamped = std::clamp(position, 0, PositionCount());
    return std::clamp(minimum + static_cast<float>(clamped) * step, minimum, maximum);
  }
};
}  // namespace ConfigWidget
```

- [x] **Step 4: Run the test to verify it passes**

Run:
```bash
cmake --build build --target tests -j8 && ./build/Binaries/Tests/tests --gtest_filter='ConfigSliderMapping.*'
```
Expected: PASS, 6 tests.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigSliderMapping.h \
        Source/UnitTests/Core/Config/ConfigSliderMappingTest.cpp \
        Source/UnitTests/Core/CMakeLists.txt
git commit -m "DolphinQt: extract float slider position mapping

Pure arithmetic for binding a float setting to an integer QSlider position,
with round-trip and clamping coverage at both range ends. Currently inline in
ConfigFloatSlider, which is left untouched.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 3: The binder library target, the `qt-tests` harness, and the change broadcaster

**Files:**
- Modify: `Source/Core/DolphinQt/CMakeLists.txt:9-10` and the `dolphin-emu` link block at `:454`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.h`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.cpp`
- Create: `Source/QtTests/CMakeLists.txt`
- Create: `Source/QtTests/QtTestsMain.cpp`
- Create: `Source/QtTests/SignalRecorder.h`
- Create: `Source/QtTests/ConfigChangeBroadcasterTest.cpp`
- Modify: `Source/CMakeLists.txt:88-89`
- Modify: `Source/Core/DolphinQt/Settings.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - CMake target `dolphinqt-config-binder` (STATIC), linking `Qt6::Widgets` and `core`. Later tasks add sources to it via `target_sources`.
  - CMake target `qt-tests` (executable), linking `dolphinqt-config-binder`, `gtest::gtest`, `core`, `uicommon`.
  - `class ConfigWidget::ConfigChangeBroadcaster : public QObject` with `static ConfigChangeBroadcaster& Instance()`, signal `void Changed()`, and slot `void Broadcast()`.
  - `template <typename Signal> class SignalRecorder` in `Source/QtTests/SignalRecorder.h`, with `int Count() const` and `void Reset()`.

**Why a broadcaster instead of using `Settings::ConfigChanged` directly:** `Settings` is 1,016 LOC with 33 includes and pulls in most of DolphinQt, so linking it into `qt-tests` would defeat the point of a small test target. Production wires `Settings::ConfigChanged` into the broadcaster with one `connect`, so the coalescing and GUI-thread marshalling at `Settings.cpp:60-74` still governs when bindings refresh — behaviour is unchanged by construction rather than by re-implementation.

- [x] **Step 1: Write the failing test**

Create `Source/QtTests/SignalRecorder.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

// QSignalSpy lives in Qt6Test, which is absent from the bundled Windows Qt
// (Externals/Qt/Qt6.8.3/x64/lib/cmake has no Qt6Test). This is the small part of it we need.
class SignalRecorder
{
public:
  template <typename Sender, typename Signal>
  SignalRecorder(Sender* sender, Signal signal)
  {
    QObject::connect(sender, signal, &m_context, [this] { ++m_count; });
  }

  int Count() const { return m_count; }
  void Reset() { m_count = 0; }

private:
  QObject m_context;
  int m_count = 0;
};
```

Create `Source/QtTests/QtTestsMain.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <gtest/gtest.h>

// Widget tests need a QApplication. macOS and Linux runs pass -platform offscreen; the bundled
// Windows Qt has no offscreen plugin (only qwindows and qdirect2d), so Windows runs use the
// desktop session on the UAT host.
int main(int argc, char** argv)
{
  QApplication app{argc, argv};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
```

Create `Source/QtTests/ConfigChangeBroadcasterTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "SignalRecorder.h"

TEST(ConfigChangeBroadcaster, InstanceIsASingleton)
{
  EXPECT_EQ(&ConfigWidget::ConfigChangeBroadcaster::Instance(),
            &ConfigWidget::ConfigChangeBroadcaster::Instance());
}

TEST(ConfigChangeBroadcaster, BroadcastEmitsChangedOncePerCall)
{
  auto& broadcaster = ConfigWidget::ConfigChangeBroadcaster::Instance();
  SignalRecorder recorder{&broadcaster, &ConfigWidget::ConfigChangeBroadcaster::Changed};

  broadcaster.Broadcast();
  EXPECT_EQ(recorder.Count(), 1);

  broadcaster.Broadcast();
  broadcaster.Broadcast();
  EXPECT_EQ(recorder.Count(), 3);
}
```

Create `Source/QtTests/CMakeLists.txt`:

```cmake
add_executable(qt-tests EXCLUDE_FROM_ALL
  QtTestsMain.cpp
  SignalRecorder.h
  ConfigChangeBroadcasterTest.cpp
)
set_target_properties(qt-tests PROPERTIES FOLDER Tests)
target_include_directories(qt-tests PRIVATE
  ${CMAKE_SOURCE_DIR}/Source/Core
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(qt-tests PRIVATE
  dolphinqt-config-binder
  gtest::gtest
  core
  uicommon
)
add_test(NAME qt-tests COMMAND qt-tests)
add_dependencies(unittests qt-tests)
```

Add to `Source/CMakeLists.txt` immediately after the existing `add_subdirectory(UnitTests)` line, inside the same `if (ENABLE_TESTS)` block:

```cmake
  if (ENABLE_QT)
    add_subdirectory(QtTests)
  endif()
```

- [x] **Step 2: Run the test to verify it fails**

Run:
```bash
cmake -S . -B build 2>&1 | tail -20
```
Expected: FAIL at configure time with `Target "qt-tests" links to target "dolphinqt-config-binder" ... but the target was not found`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

namespace ConfigWidget
{
// Relays "config changed" to every ConfigBinding without the binder library having to depend on
// DolphinQt's Settings class. dolphin-emu connects Settings::ConfigChanged to Broadcast() once at
// startup, so the coalescing and GUI-thread marshalling in Settings.cpp still decides when this
// fires. qt-tests calls Broadcast() directly.
class ConfigChangeBroadcaster final : public QObject
{
  Q_OBJECT

public:
  static ConfigChangeBroadcaster& Instance();

  void Broadcast();

signals:
  void Changed();

private:
  ConfigChangeBroadcaster() = default;
};
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"

namespace ConfigWidget
{
ConfigChangeBroadcaster& ConfigChangeBroadcaster::Instance()
{
  static ConfigChangeBroadcaster instance;
  return instance;
}

void ConfigChangeBroadcaster::Broadcast()
{
  emit Changed();
}
}  // namespace ConfigWidget
```

In `Source/Core/DolphinQt/CMakeLists.txt`, add `AUTOUIC` next to the existing auto-tool settings at lines 9-10:

```cmake
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
```

Then, after the `find_package(Qt6 ...)` block and before `add_executable(dolphin-emu`, add the binder library:

```cmake
# The config binder is its own target so qt-tests can link it without pulling in the rest of
# DolphinQt. Later tasks add sources here with target_sources().
add_library(dolphinqt-config-binder STATIC
  Config/Binder/ConfigBindingLogic.h
  Config/Binder/ConfigSliderMapping.h
  Config/Binder/ConfigChangeBroadcaster.cpp
  Config/Binder/ConfigChangeBroadcaster.h
)
target_include_directories(dolphinqt-config-binder PUBLIC ${CMAKE_SOURCE_DIR}/Source/Core)
target_link_libraries(dolphinqt-config-binder PUBLIC Qt6::Widgets PRIVATE core)
```

Add it to the `dolphin-emu` link list at line 454:

```cmake
  dolphinqt-config-binder
```

In `Source/Core/DolphinQt/Settings.cpp`, add the include alongside the existing includes:

```cpp
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
```

and immediately after the `m_config_changed_callback_id = Config::AddConfigChangedCallback(...)` block that ends at line 74, add:

```cpp
  // Drive config-bound widgets from the same coalesced, GUI-thread-marshalled signal the rest of
  // DolphinQt uses, so the binder library needs no dependency on Settings.
  connect(this, &Settings::ConfigChanged, &ConfigWidget::ConfigChangeBroadcaster::Instance(),
          &ConfigWidget::ConfigChangeBroadcaster::Broadcast);
```

- [x] **Step 4: Run the test to verify it passes**

Run:
```bash
cmake -S . -B build && cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 2 tests.

Then confirm the application still builds and the Qt-free test binary is unaffected:
```bash
cmake --build build --target dolphin-emu tests -j8
```
Expected: both succeed.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/CMakeLists.txt Source/CMakeLists.txt \
        Source/Core/DolphinQt/Settings.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.h \
        Source/Core/DolphinQt/Config/Binder/ConfigChangeBroadcaster.cpp \
        Source/QtTests/
git commit -m "DolphinQt: add AUTOUIC, a config-binder target and the qt-tests harness

- Enable CMAKE_AUTOUIC beside AUTOMOC/AUTORCC so .ui files build once slices
  begin adding them.
- Split the config binder into its own static library so a test binary can link
  it without pulling in the rest of DolphinQt.
- Add qt-tests: QApplication plus gtest, deliberately avoiding Qt6Test, which is
  absent from the bundled Windows Qt. SignalRecorder replaces QSignalSpy.
- Relay config changes through ConfigChangeBroadcaster, wired from
  Settings::ConfigChanged, so refresh timing stays governed by the existing
  coalescing in Settings.cpp and the binder needs no Settings dependency.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 4: `ConfigBinding` base and the first widget type

**Files:**
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Create: `Source/QtTests/ConfigBinderRoundTripTest.cpp`
- Modify: `Source/Core/DolphinQt/CMakeLists.txt` (binder target sources)
- Modify: `Source/QtTests/CMakeLists.txt` (test sources)

**Interfaces:**
- Consumes: `ConfigWidget::Logic::{ReadValue, WriteValue, IsLocal, ClearLocal}` (Task 1); `ConfigWidget::ConfigChangeBroadcaster` (Task 3).
- Produces:
  - `class ConfigWidget::ConfigBinding : public QObject` — non-template base. Constructed with `(QWidget* widget, Config::Location location, Config::Layer* layer)`, parents itself to `widget`, installs itself as `widget`'s event filter, and connects `ConfigChangeBroadcaster::Changed` to a refresh. Public: `const Config::Location& GetLocation() const`, `Config::Layer* GetLayer() const` — the binder's own free functions reach these through `FindBinding`. Protected: `QWidget* GetWidget() const`, `bool IsUpdating() const`, `void RefreshFromConfig()`. Pure virtual: `void LoadFromConfig() = 0`.
  - `void ConfigWidget::Bind(QCheckBox*, const Config::Info<bool>&, Config::Layer* = nullptr, bool reverse = false)`.
  - `ConfigBinding* ConfigWidget::FindBinding(QWidget*)` — returns the attached binding or `nullptr`. Later tasks use it to attach descriptions and to assert single-binding.

**Critical implementation note for the implementer:** `moc` cannot process a class template. `ConfigBinding` is the only class here with `Q_OBJECT`; every per-widget-type subclass must **omit** `Q_OBJECT` and add no signals or slots of its own. Typed state (`Config::Info<T>`) lives in those subclasses. `Config::Info<T>` has a deleted assignment operator and deleted move constructor, so it can only be initialised in a member-initialiser list.

- [x] **Step 1: Write the failing test**

Create `Source/QtTests/ConfigBinderRoundTripTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderRoundTrip", "Bool"}, false};

class ConfigBinderRoundTripTest : public ::testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }

  // Bindings refresh on ConfigChangeBroadcaster::Changed, which dolphin-emu drives from
  // Settings::ConfigChanged. Tests drive it directly.
  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }
};
}  // namespace

TEST_F(ConfigBinderRoundTripTest, CheckBoxAdoptsTheConfigValueWhenBound)
{
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ASSERT_FALSE(box.isChecked());

  ConfigWidget::Bind(&box, TEST_BOOL);

  EXPECT_TRUE(box.isChecked());
}

TEST_F(ConfigBinderRoundTripTest, CheckBoxFollowsLaterConfigChanges)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);
  ASSERT_FALSE(box.isChecked());

  Config::SetBase(TEST_BOOL, true);
  NotifyConfigChanged();

  EXPECT_TRUE(box.isChecked());
}

TEST_F(ConfigBinderRoundTripTest, TogglingTheCheckBoxWritesConfig)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  box.setChecked(true);

  EXPECT_TRUE(Config::Get(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, ReverseInvertsBothDirections)
{
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, nullptr, true);
  EXPECT_FALSE(box.isChecked()) << "reverse means a true setting shows as unchecked";

  box.setChecked(true);
  EXPECT_FALSE(Config::Get(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, LayeredBindingWritesToTheLayerNotTheBase)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  box.setChecked(true);

  EXPECT_TRUE(layer.Exists(TEST_BOOL.GetLocation()));
  EXPECT_TRUE(layer.Get(TEST_BOOL));
  EXPECT_FALSE(Config::GetBase(TEST_BOOL));
}

TEST_F(ConfigBinderRoundTripTest, RefreshingFromConfigDoesNotWriteBack)
{
  // ConfigControl guards against: refresh sets the widget -> widget emits toggled -> save ->
  // OnConfigChanged -> refresh. Without the guard this recurses or clobbers a layer.
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  NotifyConfigChanged();

  EXPECT_TRUE(box.isChecked()) << "shows the base value";
  EXPECT_FALSE(layer.Exists(TEST_BOOL.GetLocation()))
      << "a refresh must not create a per-game override";
}

TEST_F(ConfigBinderRoundTripTest, FindBindingReturnsTheAttachedBinding)
{
  QCheckBox unbound;
  EXPECT_EQ(ConfigWidget::FindBinding(&unbound), nullptr);

  QCheckBox bound;
  ConfigWidget::Bind(&bound, TEST_BOOL);
  EXPECT_NE(ConfigWidget::FindBinding(&bound), nullptr);
}
```

Add to `Source/QtTests/CMakeLists.txt`, in the `add_executable(qt-tests ...)` source list:

```cmake
  ConfigBinderRoundTripTest.cpp
```

- [x] **Step 2: Run the test to verify it fails**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time with `'DolphinQt/Config/Binder/ConfigWidgetBinder.h' file not found`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

#include "Common/Config/ConfigInfo.h"

class QEvent;
class QWidget;

namespace Config
{
class Layer;
}

namespace ConfigWidget
{
// Attached as a child of the widget it binds, so it lives exactly as long as the widget and call
// sites do no ownership bookkeeping. Replaces ConfigControl<Derived>, whose config location was a
// constructor argument and therefore could not be produced by uic.
//
// This is the only class in the binder with Q_OBJECT: moc cannot process templates, so typed
// state lives in non-Q_OBJECT subclasses that add no signals or slots.
class ConfigBinding : public QObject
{
  Q_OBJECT

public:
  ConfigBinding(QWidget* widget, Config::Location location, Config::Layer* layer);
  ~ConfigBinding() override;

  const Config::Location& GetLocation() const { return m_location; }
  Config::Layer* GetLayer() const { return m_layer; }

protected:
  QWidget* GetWidget() const;

  // True while LoadFromConfig() is running. Subclasses must not write config in that window:
  // refresh sets the widget, the widget emits its value-changed signal, and a naive handler would
  // save straight back, clobbering a per-game layer or recursing.
  bool IsUpdating() const { return m_updating; }

  // Re-reads config into the widget and re-applies the overridden-value font.
  void RefreshFromConfig();

  virtual void LoadFromConfig() = 0;

  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  void ApplyOverrideFont();

  const Config::Location m_location;
  Config::Layer* m_layer;
  bool m_updating = false;
};
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigBinding.h"

#include <QFont>
#include <QWidget>

#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"

namespace ConfigWidget
{
ConfigBinding::ConfigBinding(QWidget* widget, Config::Location location, Config::Layer* layer)
    : QObject(widget), m_location(std::move(location)), m_layer(layer)
{
  widget->installEventFilter(this);
  connect(&ConfigChangeBroadcaster::Instance(), &ConfigChangeBroadcaster::Changed, this,
          &ConfigBinding::RefreshFromConfig);
}

ConfigBinding::~ConfigBinding() = default;

QWidget* ConfigBinding::GetWidget() const
{
  return qobject_cast<QWidget*>(parent());
}

void ConfigBinding::RefreshFromConfig()
{
  ApplyOverrideFont();

  m_updating = true;
  LoadFromConfig();
  m_updating = false;
}

void ConfigBinding::ApplyOverrideFont()
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return;

  QFont font = widget->font();
  font.setBold(Logic::IsLocal(m_location, m_layer));
  widget->setFont(font);
}

bool ConfigBinding::eventFilter(QObject* watched, QEvent* event)
{
  return QObject::eventFilter(watched, event);
}
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/ConfigInfo.h"

class QCheckBox;
class QWidget;

namespace Config
{
class Layer;
}

// Binds stock Qt widgets to typed config settings after construction, so layouts can be authored
// in Qt Designer .ui files. A null `layer` binds global config; a non-null `layer` binds a
// per-game layer. Config::Info<T> already carries the key, type and default, so no key or default
// argument is needed.
namespace ConfigWidget
{
class ConfigBinding;

void Bind(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer = nullptr,
          bool reverse = false);

// The binding attached to `widget`, or nullptr if it has none.
ConfigBinding* FindBinding(QWidget* widget);
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

#include <QCheckBox>

#include "DolphinQt/Config/Binder/ConfigBinding.h"
#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"

namespace ConfigWidget
{
namespace
{
// No Q_OBJECT: moc cannot process templates, and this adds no signals or slots of its own.
class CheckBoxBinding final : public ConfigBinding
{
public:
  CheckBoxBinding(QCheckBox* box, const Config::Info<bool>& setting, Config::Layer* layer,
                  bool reverse)
      : ConfigBinding(box, setting.GetLocation(), layer), m_setting(setting), m_reverse(reverse)
  {
    connect(box, &QCheckBox::toggled, this, &CheckBoxBinding::OnToggled);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const box = qobject_cast<QCheckBox*>(GetWidget());
    if (box == nullptr)
      return;
    box->setChecked(Logic::ReadValue(m_setting, GetLayer()) ^ m_reverse);
  }

  void OnToggled(bool checked)
  {
    if (IsUpdating())
      return;
    Logic::WriteValue(m_setting, GetLocation(), GetLayer(), checked ^ m_reverse);
  }

  const Config::Info<bool> m_setting;
  const bool m_reverse;
};
}  // namespace

void Bind(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer, bool reverse)
{
  new CheckBoxBinding{widget, setting, layer, reverse};
}

ConfigBinding* FindBinding(QWidget* widget)
{
  return widget->findChild<ConfigBinding*>(QString{}, Qt::FindDirectChildrenOnly);
}
}  // namespace ConfigWidget
```

Add to the `dolphinqt-config-binder` source list in `Source/Core/DolphinQt/CMakeLists.txt`:

```cmake
  Config/Binder/ConfigBinding.cpp
  Config/Binder/ConfigBinding.h
  Config/Binder/ConfigWidgetBinder.cpp
  Config/Binder/ConfigWidgetBinder.h
```

- [x] **Step 4: Run the test to verify it passes**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen \
     --gtest_filter='ConfigBinderRoundTripTest.*'
```
Expected: PASS, 9 tests — the 2 broadcaster tests from Task 3 and 7 new. Every count below is
the whole `qt-tests` binary, which is what gtest prints.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigBinding.h \
        Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/Core/DolphinQt/CMakeLists.txt \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigBinderRoundTripTest.cpp
git commit -m "DolphinQt: add ConfigBinding and bind-after-construction for QCheckBox

ConfigBinding attaches to a widget as a child, so it lives exactly as long as
the widget and call sites do no ownership bookkeeping. It is the only class in
the binder carrying Q_OBJECT; typed state lives in plain subclasses, because moc
cannot process templates.

Covers the re-entrancy case ConfigControl guards today: a refresh must not write
back, or a per-game layer gains an override nobody asked for.

ConfigControls/ is untouched and still in use; nothing changes for users yet.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 5: The remaining stock widget types

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h` (add the `ValueBinding` template)
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Modify: `Source/QtTests/ConfigBinderRoundTripTest.cpp`

**Interfaces:**
- Consumes: `ConfigWidget::ConfigBinding` and `ConfigWidget::Bind(QCheckBox*, ...)` (Task 4).
- Produces:
  - `template <typename Widget, typename T> class ConfigWidget::ValueBinding : public ConfigBinding`
    with protected `Widget* GetTypedWidget() const`, `T Read() const`, `void Save(const T& value)`.
    `Save` is a no-op while `IsUpdating()`.
  - `void Bind(QComboBox*, const Config::Info<int>&, Config::Layer* = nullptr)` — binds the
    **current index**. Index→value mapping is `BindMapped` in Task 6.
  - `void Bind(QSpinBox*, const Config::Info<int>&, Config::Layer* = nullptr)`
  - `void Bind(QSlider*, const Config::Info<int>&, Config::Layer* = nullptr)`
  - `void Bind(QRadioButton*, const Config::Info<int>&, int value, Config::Layer* = nullptr)`
  - `void Bind(QLineEdit*, const Config::Info<std::string>&, Config::Layer* = nullptr)`

**Notes for the implementer:**
- `Bind` never touches a spin box's or slider's `minimum`/`maximum`. Those come from the `.ui`
  file, which is the point of the migration: the range is layout data, not binding data.
- `QLineEdit` saves on `editingFinished`, not `textChanged`, so a path being typed does not write a
  half-finished string to the config file on every keystroke.
- A `QRadioButton` writes only when it becomes checked. Qt's autoexclusive grouping already emits
  `toggled(false)` on the button losing check, and acting on that would race the winner.

- [x] **Step 1: Write the failing tests**

Append to `Source/QtTests/ConfigBinderRoundTripTest.cpp`. Extend the includes at the top of the
file:

```cpp
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
```

Add to the anonymous namespace beside `TEST_BOOL`:

```cpp
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderRoundTrip", "Int"}, 0};
const Config::Info<std::string> TEST_STRING{{Config::System::Main, "BinderRoundTrip", "String"},
                                            ""};
```

Then append the tests:

```cpp
TEST_F(ConfigBinderRoundTripTest, ComboBoxRoundTripsTheCurrentIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
  Config::SetBase(TEST_INT, 2);

  ConfigWidget::Bind(&box, TEST_INT);
  EXPECT_EQ(box.currentIndex(), 2);

  box.setCurrentIndex(1);
  EXPECT_EQ(Config::Get(TEST_INT), 1);
}

TEST_F(ConfigBinderRoundTripTest, SpinBoxRoundTripsAndKeepsItsDesignerRange)
{
  QSpinBox spin;
  spin.setRange(10, 90);
  Config::SetBase(TEST_INT, 42);

  ConfigWidget::Bind(&spin, TEST_INT);
  EXPECT_EQ(spin.value(), 42);
  EXPECT_EQ(spin.minimum(), 10) << "Bind must not overwrite the range from the .ui file";
  EXPECT_EQ(spin.maximum(), 90);

  spin.setValue(55);
  EXPECT_EQ(Config::Get(TEST_INT), 55);
}

TEST_F(ConfigBinderRoundTripTest, SliderRoundTrips)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 100);
  Config::SetBase(TEST_INT, 30);

  ConfigWidget::Bind(&slider, TEST_INT);
  EXPECT_EQ(slider.value(), 30);

  slider.setValue(70);
  EXPECT_EQ(Config::Get(TEST_INT), 70);
}

TEST_F(ConfigBinderRoundTripTest, RadioButtonChecksOnlyWhenItsValueIsSelected)
{
  QWidget parent;
  auto* const first = new QRadioButton{&parent};
  auto* const second = new QRadioButton{&parent};
  Config::SetBase(TEST_INT, 1);

  ConfigWidget::Bind(first, TEST_INT, 0);
  ConfigWidget::Bind(second, TEST_INT, 1);

  EXPECT_FALSE(first->isChecked());
  EXPECT_TRUE(second->isChecked());
}

TEST_F(ConfigBinderRoundTripTest, CheckingARadioButtonWritesItsOwnValue)
{
  QWidget parent;
  auto* const first = new QRadioButton{&parent};
  auto* const second = new QRadioButton{&parent};
  ConfigWidget::Bind(first, TEST_INT, 0);
  ConfigWidget::Bind(second, TEST_INT, 1);

  second->setChecked(true);
  EXPECT_EQ(Config::Get(TEST_INT), 1);

  // Qt unchecks `second` as part of checking `first`. Only the winner may write.
  first->setChecked(true);
  EXPECT_EQ(Config::Get(TEST_INT), 0);
}

TEST_F(ConfigBinderRoundTripTest, LineEditWritesOnEditingFinishedNotOnEveryKeystroke)
{
  Config::SetBase(TEST_STRING, "before");
  QLineEdit edit;
  ConfigWidget::Bind(&edit, TEST_STRING);
  EXPECT_EQ(edit.text(), QStringLiteral("before"));

  edit.setText(QStringLiteral("partial-p"));
  EXPECT_EQ(Config::Get(TEST_STRING), "before") << "a keystroke must not write config";

  edit.setText(QStringLiteral("after"));
  emit edit.editingFinished();
  EXPECT_EQ(Config::Get(TEST_STRING), "after");
}

TEST_F(ConfigBinderRoundTripTest, EveryWidgetTypeRefreshesWithoutWritingBack)
{
  // Same re-entrancy guard as the check box case, for every type.
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_INT, 5);
  Config::SetBase(TEST_STRING, "base");

  QComboBox combo;
  combo.addItems({QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"),
                  QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5")});
  QSpinBox spin;
  spin.setRange(0, 100);
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 100);
  QLineEdit edit;

  ConfigWidget::Bind(&combo, TEST_INT, &layer);
  ConfigWidget::Bind(&spin, TEST_INT, &layer);
  ConfigWidget::Bind(&slider, TEST_INT, &layer);
  ConfigWidget::Bind(&edit, TEST_STRING, &layer);

  NotifyConfigChanged();

  EXPECT_FALSE(layer.Exists(TEST_INT.GetLocation()));
  EXPECT_FALSE(layer.Exists(TEST_STRING.GetLocation()));
}
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `Bind` overload matches `QComboBox*`.

- [x] **Step 3: Write the minimal implementation**

Add to `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`, after the `ConfigBinding` class and
inside `namespace ConfigWidget`:

```cpp
// Shared state for a binding whose widget holds one value of type T. A template, so it carries no
// Q_OBJECT and declares no signals or slots; concrete per-widget subclasses connect the widget's
// value-changed signal and implement LoadFromConfig().
template <typename Widget, typename T>
class ValueBinding : public ConfigBinding
{
public:
  ValueBinding(Widget* widget, const Config::Info<T>& setting, Config::Layer* layer)
      : ConfigBinding(widget, setting.GetLocation(), layer), m_setting(setting)
  {
  }

protected:
  Widget* GetTypedWidget() const { return static_cast<Widget*>(GetWidget()); }

  T Read() const { return Logic::ReadValue(m_setting, GetLayer()); }

  void Save(const T& value)
  {
    if (IsUpdating())
      return;
    Logic::WriteValue(m_setting, GetLocation(), GetLayer(), value);
  }

private:
  // Config::Info<T> has a deleted assignment operator and no move constructor, so it can only be
  // initialised in the member-initialiser list above.
  const Config::Info<T> m_setting;
};
```

`ConfigBinding.h` needs `#include "DolphinQt/Config/Binder/ConfigBindingLogic.h"` for that
template, and `GetWidget()` must move from `protected` to a `protected` position visible to it —
it already is, so no access change is needed.

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
#include <string>
```

```cpp
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
```

```cpp
// Binds the combo box's current index. For an index-to-value mapping use BindMapped.
void Bind(QComboBox* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
// The spin box's and slider's minimum and maximum come from the .ui file and are left alone.
void Bind(QSpinBox* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
void Bind(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer = nullptr);
// Checked exactly when the setting equals `value`; writes `value` when it becomes checked.
void Bind(QRadioButton* widget, const Config::Info<int>& setting, int value,
          Config::Layer* layer = nullptr);
// Saves on editingFinished, so a half-typed path is never written to the config file.
void Bind(QLineEdit* widget, const Config::Info<std::string>& setting,
          Config::Layer* layer = nullptr);
```

In `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`, extend the includes:

```cpp
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
```

Rewrite `CheckBoxBinding` onto the new base and add the rest, all inside the existing anonymous
namespace. None of these declare `Q_OBJECT`:

```cpp
class CheckBoxBinding final : public ValueBinding<QCheckBox, bool>
{
public:
  CheckBoxBinding(QCheckBox* box, const Config::Info<bool>& setting, Config::Layer* layer,
                  bool reverse)
      : ValueBinding(box, setting, layer), m_reverse(reverse)
  {
    connect(box, &QCheckBox::toggled, this, &CheckBoxBinding::OnToggled);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setChecked(Read() ^ m_reverse); }
  void OnToggled(bool checked) { Save(checked ^ m_reverse); }

  const bool m_reverse;
};

class ComboBoxBinding final : public ValueBinding<QComboBox, int>
{
public:
  ComboBoxBinding(QComboBox* box, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(box, setting, layer)
  {
    connect(box, &QComboBox::currentIndexChanged, this, &ComboBoxBinding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setCurrentIndex(Read()); }
  void OnIndexChanged(int index) { Save(index); }
};

class SpinBoxBinding final : public ValueBinding<QSpinBox, int>
{
public:
  SpinBoxBinding(QSpinBox* spin, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(spin, setting, layer)
  {
    connect(spin, &QSpinBox::valueChanged, this, &SpinBoxBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setValue(Read()); }
  void OnValueChanged(int value) { Save(value); }
};

class SliderBinding final : public ValueBinding<QSlider, int>
{
public:
  SliderBinding(QSlider* slider, const Config::Info<int>& setting, Config::Layer* layer)
      : ValueBinding(slider, setting, layer)
  {
    connect(slider, &QSlider::valueChanged, this, &SliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setValue(Read()); }
  void OnValueChanged(int value) { Save(value); }
};

class RadioButtonBinding final : public ValueBinding<QRadioButton, int>
{
public:
  RadioButtonBinding(QRadioButton* button, const Config::Info<int>& setting, int value,
                     Config::Layer* layer)
      : ValueBinding(button, setting, layer), m_value(value)
  {
    connect(button, &QRadioButton::toggled, this, &RadioButtonBinding::OnToggled);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override { GetTypedWidget()->setChecked(Read() == m_value); }

  void OnToggled(bool checked)
  {
    // Autoexclusive grouping unchecks the previous button as part of checking the new one. Only
    // the button that won may write, or the two saves race.
    if (checked)
      Save(m_value);
  }

  const int m_value;
};

class LineEditBinding final : public ValueBinding<QLineEdit, std::string>
{
public:
  LineEditBinding(QLineEdit* edit, const Config::Info<std::string>& setting, Config::Layer* layer)
      : ValueBinding(edit, setting, layer)
  {
    connect(edit, &QLineEdit::editingFinished, this, &LineEditBinding::OnEditingFinished);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    GetTypedWidget()->setText(QString::fromStdString(Read()));
  }

  void OnEditingFinished() { Save(GetTypedWidget()->text().toStdString()); }
};
```

And the five entry points beside the existing `Bind(QCheckBox*, ...)`:

```cpp
void Bind(QComboBox* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  new ComboBoxBinding{widget, setting, layer};
}

void Bind(QSpinBox* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  new SpinBoxBinding{widget, setting, layer};
}

void Bind(QSlider* widget, const Config::Info<int>& setting, Config::Layer* layer)
{
  new SliderBinding{widget, setting, layer};
}

void Bind(QRadioButton* widget, const Config::Info<int>& setting, int value, Config::Layer* layer)
{
  new RadioButtonBinding{widget, setting, value, layer};
}

void Bind(QLineEdit* widget, const Config::Info<std::string>& setting, Config::Layer* layer)
{
  new LineEditBinding{widget, setting, layer};
}
```

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen \
     --gtest_filter='ConfigBinderRoundTripTest.*'
```
Expected: PASS, 16 tests, 7 new. The seven from Task 4 must still pass — `CheckBoxBinding` was rebased
onto `ValueBinding` in this task and those tests are what proves the rebase was behaviour-preserving.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigBinding.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/QtTests/ConfigBinderRoundTripTest.cpp
git commit -m "DolphinQt: bind combo boxes, spin boxes, sliders, radios and line edits

Adds a ValueBinding<Widget, T> template for the state every single-value
binding shares, and rebases the check box onto it. Templates carry no Q_OBJECT,
so each concrete binding connects its own widget signal by member pointer.

Bind leaves a spin box's or slider's range alone: minimum and maximum are
layout data that belongs to the .ui file.

Radio buttons write only when they become checked, because autoexclusive
grouping also emits toggled(false) on the button losing the check.

Line edits save on editingFinished, so typing a path does not write a
half-finished string to the config file on every keystroke.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 6: Mapped and scaled bindings

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Create: `Source/QtTests/ConfigBinderMappedTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ConfigWidget::ValueBinding<Widget, T>` (Task 5).
- Produces:
  - `void Bind(QComboBox*, const Config::Info<u32>&, Config::Layer* = nullptr)` — index-based, the
    `u32` counterpart of Task 5's `int` overload.
  - `template <typename T> void BindMapped(QComboBox*, const Config::Info<T>&, std::span<const T> values, Config::Layer* = nullptr)`
  - `template <typename T> void BindMapped(QComboBox*, const Config::Info<T>&, std::span<const std::pair<QString, T>> options, Config::Layer* = nullptr)`
  - `void BindStringChoice(QComboBox*, const Config::Info<std::string>&, std::span<const std::string> options, Config::Layer* = nullptr)`
  - `void BindStringChoice(QComboBox*, const Config::Info<std::string>&, std::span<const std::pair<QString, QString>> options, Config::Layer* = nullptr)`
  - `void BindMapped(QSlider*, const Config::Info<int>&, std::span<const int> tick_values, Config::Layer* = nullptr)`
  - `void BindScaled(QSlider*, const Config::Info<u32>&, u32 scale, Config::Layer* = nullptr)`
  - `template <typename Widget, typename T> class ConfigWidget::detail::MappedBinding` — the shared
    value↔index machinery both mapped overloads use.

**Why two `BindMapped` combo overloads:** the `std::span<const T>` form pairs `.ui`-authored items
with values by index, which keeps the display text in the uic translation path — that is the
preferred form and what most call sites become. The pair form exists for the option sets that are
computed at runtime and therefore cannot be authored in Designer: `AdvancedPane`'s CPU-core list
depends on which JIT this platform builds, and `WiiPane`'s SD-card sizes are generated.

**Existing call sites this must cover** (measured, for the implementer's sanity check):
`ConfigChoiceMap` 5, instantiated with `bool`, `u32`, `u64`, `PowerPC::CPUCore` and `StereoMode` —
so this must stay a template rather than a set of explicit instantiations. `ConfigStringChoice` 7,
`ConfigChoiceU32` 2, `ConfigSliderU32` 4.

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/ConfigBinderMappedTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <string>
#include <utility>

#include <QComboBox>
#include <QSlider>
#include <QString>
#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
enum class Mode
{
  Off = 0,
  SideBySide = 4,
  Anaglyph = 9,
};

const Config::Info<Mode> TEST_MODE{{Config::System::Main, "BinderMapped", "Mode"}, Mode::Off};
const Config::Info<int> TEST_TICKS{{Config::System::Main, "BinderMapped", "Ticks"}, 0};
const Config::Info<u32> TEST_U32{{Config::System::Main, "BinderMapped", "U32"}, 0};
const Config::Info<std::string> TEST_STR{{Config::System::Main, "BinderMapped", "Str"}, ""};

class ConfigBinderMappedTest : public ::testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }

  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }
};

constexpr std::array<Mode, 3> MODE_VALUES{Mode::Off, Mode::SideBySide, Mode::Anaglyph};
}  // namespace

TEST_F(ConfigBinderMappedTest, MappedComboPairsDesignerItemsWithValuesByIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
  Config::SetBase(TEST_MODE, Mode::Anaglyph);

  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES});
  EXPECT_EQ(box.currentIndex(), 2);
  EXPECT_EQ(box.count(), 3) << "the .ui items must not be duplicated or replaced";

  box.setCurrentIndex(1);
  EXPECT_EQ(Config::Get(TEST_MODE), Mode::SideBySide) << "saves the value, not the index";
}

TEST_F(ConfigBinderMappedTest, MappedComboSelectsNothingWhenNoValueMatches)
{
  // ConfigChoiceMap sets index -1 rather than snapping to the first option.
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("Side-by-Side"), QStringLiteral("Anaglyph")});
  Config::SetBase(TEST_MODE, static_cast<Mode>(77));

  ConfigWidget::BindMapped(&box, TEST_MODE, std::span<const Mode>{MODE_VALUES});
  EXPECT_EQ(box.currentIndex(), -1);
}

TEST_F(ConfigBinderMappedTest, MappedComboPairFormPopulatesTheCombo)
{
  const std::array<std::pair<QString, Mode>, 2> options{
      std::pair{QStringLiteral("Off"), Mode::Off},
      std::pair{QStringLiteral("Anaglyph"), Mode::Anaglyph}};
  QComboBox box;
  Config::SetBase(TEST_MODE, Mode::Anaglyph);

  ConfigWidget::BindMapped(&box, TEST_MODE,
                           std::span<const std::pair<QString, Mode>>{options});

  EXPECT_EQ(box.count(), 2);
  EXPECT_EQ(box.itemText(1), QStringLiteral("Anaglyph"));
  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigBinderMappedTest, U32ComboRoundTripsTheIndex)
{
  QComboBox box;
  box.addItems({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")});
  Config::SetBase(TEST_U32, 2u);

  ConfigWidget::Bind(&box, TEST_U32);
  EXPECT_EQ(box.currentIndex(), 2);

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_U32), 0u);
}

TEST_F(ConfigBinderMappedTest, StringChoiceTreatsTheOptionTextAsTheData)
{
  const std::array<std::string, 2> options{"Vulkan", "OpenGL"};
  QComboBox box;
  Config::SetBase(TEST_STR, "OpenGL");

  ConfigWidget::BindStringChoice(&box, TEST_STR, std::span<const std::string>{options});
  EXPECT_EQ(box.currentIndex(), 1);

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_STR), "Vulkan");
}

TEST_F(ConfigBinderMappedTest, StringChoiceSeparatesDisplayTextFromData)
{
  const std::array<std::pair<QString, QString>, 2> options{
      std::pair{QStringLiteral("Vulkan"), QStringLiteral("vulkan")},
      std::pair{QStringLiteral("OpenGL"), QStringLiteral("ogl")}};
  QComboBox box;
  Config::SetBase(TEST_STR, "ogl");

  ConfigWidget::BindStringChoice(
      &box, TEST_STR, std::span<const std::pair<QString, QString>>{options});
  EXPECT_EQ(box.currentIndex(), 1);
  EXPECT_EQ(box.itemText(1), QStringLiteral("OpenGL"));

  box.setCurrentIndex(0);
  EXPECT_EQ(Config::Get(TEST_STR), "vulkan") << "saves the data, not the display text";
}

TEST_F(ConfigBinderMappedTest, TickSliderMapsPositionsToArbitraryValues)
{
  constexpr std::array<int, 3> ticks{1, 2, 4};
  QSlider slider{Qt::Horizontal};
  Config::SetBase(TEST_TICKS, 4);

  ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks});
  EXPECT_EQ(slider.minimum(), 0);
  EXPECT_EQ(slider.maximum(), 2) << "a tick slider's range is derived from the tick count";
  EXPECT_EQ(slider.value(), 2);

  slider.setValue(1);
  EXPECT_EQ(Config::Get(TEST_TICKS), 2);
}

TEST_F(ConfigBinderMappedTest, TickSliderDisablesItselfWhenNoTickMatches)
{
  constexpr std::array<int, 3> ticks{1, 2, 4};
  QSlider slider{Qt::Horizontal};
  Config::SetBase(TEST_TICKS, 3);

  ConfigWidget::BindMapped(&slider, TEST_TICKS, std::span<const int>{ticks});
  EXPECT_FALSE(slider.isEnabled()) << "an unrepresentable value must not be silently snapped";

  Config::SetBase(TEST_TICKS, 2);
  NotifyConfigChanged();
  EXPECT_TRUE(slider.isEnabled());
  EXPECT_EQ(slider.value(), 1);
}

TEST_F(ConfigBinderMappedTest, ScaledSliderMultipliesThePositionOnSaveAndDividesOnLoad)
{
  QSlider slider{Qt::Horizontal};
  slider.setRange(0, 10);
  Config::SetBase(TEST_U32, 400u);

  ConfigWidget::BindScaled(&slider, TEST_U32, 100u);
  EXPECT_EQ(slider.value(), 4);

  slider.setValue(7);
  EXPECT_EQ(Config::Get(TEST_U32), 700u);
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  ConfigBinderMappedTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `BindMapped`, `BindStringChoice` or `BindScaled`.

- [x] **Step 3: Write the minimal implementation**

`Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h` gains real includes, because
`BindMapped` is a template and its body must be visible to callers. Replace the `class QComboBox;`
forward declaration with includes, and keep the other forward declarations as they are:

```cpp
#include <algorithm>
#include <span>
#include <string>
#include <utility>

#include <QComboBox>
#include <QString>

#include "Common/CommonTypes.h"
#include "DolphinQt/Config/Binder/ConfigBinding.h"
```

Add the shared mapped machinery and the two template entry points:

```cpp
namespace ConfigWidget
{
namespace detail
{
// value <-> combo-index mapping, shared by both BindMapped overloads. No Q_OBJECT: it is a
// template.
template <typename T>
class MappedComboBinding final : public ValueBinding<QComboBox, T>
{
public:
  MappedComboBinding(QComboBox* box, const Config::Info<T>& setting, std::vector<T> values,
                     Config::Layer* layer)
      : ValueBinding<QComboBox, T>(box, setting, layer), m_values(std::move(values))
  {
    QObject::connect(box, &QComboBox::currentIndexChanged, this,
                     &MappedComboBinding::OnIndexChanged);
    this->RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    const T value = this->Read();
    const auto it = std::ranges::find(m_values, value);
    // -1 when nothing matches, as ConfigChoiceMap does: better an empty combo than a wrong
    // selection that the user then saves by touching something else.
    const int index =
        it == m_values.end() ? -1 : static_cast<int>(std::distance(m_values.begin(), it));
    this->GetTypedWidget()->setCurrentIndex(index);
  }

  void OnIndexChanged(int index)
  {
    if (index < 0 || static_cast<size_t>(index) >= m_values.size())
      return;
    this->Save(m_values[static_cast<size_t>(index)]);
  }

  const std::vector<T> m_values;
};
}  // namespace detail

// The items are authored in the .ui file; `values` pairs with them by index, so the display text
// stays in the uic translation path.
template <typename T>
void BindMapped(QComboBox* widget, const Config::Info<T>& setting, std::span<const T> values,
                Config::Layer* layer = nullptr)
{
  new detail::MappedComboBinding<T>{widget, setting, std::vector<T>(values.begin(), values.end()),
                                    layer};
}

// For option sets computed at runtime, which cannot be authored in Designer. Populates the combo.
template <typename T>
void BindMapped(QComboBox* widget, const Config::Info<T>& setting,
                std::span<const std::pair<QString, T>> options, Config::Layer* layer = nullptr)
{
  std::vector<T> values;
  values.reserve(options.size());
  for (const auto& [text, value] : options)
  {
    widget->addItem(text);
    values.push_back(value);
  }
  new detail::MappedComboBinding<T>{widget, setting, std::move(values), layer};
}
}  // namespace ConfigWidget
```

Declare the non-template additions in the same header:

```cpp
void Bind(QComboBox* widget, const Config::Info<u32>& setting, Config::Layer* layer = nullptr);

// Two forms, matching ConfigStringChoice: the option text is the stored data, or display text and
// stored data are given separately.
void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::string> options, Config::Layer* layer = nullptr);
void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::pair<QString, QString>> options,
                      Config::Layer* layer = nullptr);

// Slider position i means tick_values[i]. Sets the slider's range from the tick count and disables
// the slider while the config value matches no tick.
void BindMapped(QSlider* widget, const Config::Info<int>& setting,
                std::span<const int> tick_values, Config::Layer* layer = nullptr);
// Stored value is the slider position times `scale`.
void BindScaled(QSlider* widget, const Config::Info<u32>& setting, u32 scale,
                Config::Layer* layer = nullptr);
```

In `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`, add to the anonymous namespace:

```cpp
class ComboBoxU32Binding final : public ValueBinding<QComboBox, u32>
{
public:
  ComboBoxU32Binding(QComboBox* box, const Config::Info<u32>& setting, Config::Layer* layer)
      : ValueBinding(box, setting, layer)
  {
    connect(box, &QComboBox::currentIndexChanged, this, &ComboBoxU32Binding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    GetTypedWidget()->setCurrentIndex(static_cast<int>(Read()));
  }
  void OnIndexChanged(int index)
  {
    if (index >= 0)
      Save(static_cast<u32>(index));
  }
};

class StringChoiceBinding final : public ValueBinding<QComboBox, std::string>
{
public:
  // `data` is empty when the item text is itself the stored value.
  StringChoiceBinding(QComboBox* box, const Config::Info<std::string>& setting,
                      std::vector<QString> data, Config::Layer* layer)
      : ValueBinding(box, setting, layer), m_data(std::move(data))
  {
    connect(box, &QComboBox::currentIndexChanged, this, &StringChoiceBinding::OnIndexChanged);
    RefreshFromConfig();
  }

private:
  QString DataAt(int index) const
  {
    if (m_data.empty())
      return GetTypedWidget()->itemText(index);
    return m_data[static_cast<size_t>(index)];
  }

  void LoadFromConfig() override
  {
    auto* const box = GetTypedWidget();
    const QString value = QString::fromStdString(Read());
    for (int i = 0; i < box->count(); ++i)
    {
      if (DataAt(i) == value)
      {
        box->setCurrentIndex(i);
        return;
      }
    }
    box->setCurrentIndex(-1);
  }

  void OnIndexChanged(int index)
  {
    if (index >= 0 && index < GetTypedWidget()->count())
      Save(DataAt(index).toStdString());
  }

  const std::vector<QString> m_data;
};

class TickSliderBinding final : public ValueBinding<QSlider, int>
{
public:
  TickSliderBinding(QSlider* slider, const Config::Info<int>& setting, std::vector<int> ticks,
                    Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_ticks(std::move(ticks))
  {
    ASSERT(!m_ticks.empty());
    // Unlike a plain slider, a tick slider's range is derived, not authored: it is exactly one
    // position per tick.
    slider->setMinimum(0);
    slider->setMaximum(static_cast<int>(m_ticks.size() - 1));
    slider->setPageStep(1);
    slider->setTickPosition(QSlider::TicksBelow);

    connect(slider, &QSlider::valueChanged, this, &TickSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    auto* const slider = GetTypedWidget();
    const int value = Read();
    const auto it = std::ranges::find(m_ticks, value);
    if (it == m_ticks.end())
    {
      // The config holds a value this slider cannot represent. Disable rather than snap, so the
      // user is not shown a value that is not the one in effect.
      slider->setEnabled(false);
      return;
    }
    slider->setEnabled(true);
    slider->setValue(static_cast<int>(std::distance(m_ticks.begin(), it)));
  }

  void OnValueChanged(int position)
  {
    if (position >= 0 && static_cast<size_t>(position) < m_ticks.size())
      Save(m_ticks[static_cast<size_t>(position)]);
  }

  const std::vector<int> m_ticks;
};

class ScaledSliderBinding final : public ValueBinding<QSlider, u32>
{
public:
  ScaledSliderBinding(QSlider* slider, const Config::Info<u32>& setting, u32 scale,
                      Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_scale(scale)
  {
    ASSERT(scale != 0);
    connect(slider, &QSlider::valueChanged, this, &ScaledSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    GetTypedWidget()->setValue(static_cast<int>(Read() / m_scale));
  }
  void OnValueChanged(int position)
  {
    Save(static_cast<u32>(position) * m_scale);
  }

  const u32 m_scale;
};
```

`ConfigWidgetBinder.cpp` needs `#include <QSlider>`, `#include <vector>`, `#include <algorithm>`
and `#include "Common/Assert.h"` for the above.

And the entry points:

```cpp
void Bind(QComboBox* widget, const Config::Info<u32>& setting, Config::Layer* layer)
{
  new ComboBoxU32Binding{widget, setting, layer};
}

void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::string> options, Config::Layer* layer)
{
  for (const std::string& option : options)
    widget->addItem(QString::fromStdString(option));
  new StringChoiceBinding{widget, setting, {}, layer};
}

void BindStringChoice(QComboBox* widget, const Config::Info<std::string>& setting,
                      std::span<const std::pair<QString, QString>> options, Config::Layer* layer)
{
  std::vector<QString> data;
  data.reserve(options.size());
  for (const auto& [text, value] : options)
  {
    widget->addItem(text);
    data.push_back(value);
  }
  new StringChoiceBinding{widget, setting, std::move(data), layer};
}

void BindMapped(QSlider* widget, const Config::Info<int>& setting,
                std::span<const int> tick_values, Config::Layer* layer)
{
  new TickSliderBinding{widget, setting, std::vector<int>(tick_values.begin(), tick_values.end()),
                        layer};
}

void BindScaled(QSlider* widget, const Config::Info<u32>& setting, u32 scale, Config::Layer* layer)
{
  new ScaledSliderBinding{widget, setting, scale, layer};
}
```

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 25 tests — 16 from Tasks 3 to 5, 9 new.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigBinderMappedTest.cpp
git commit -m "DolphinQt: bind mapped combo boxes, string choices and scaled sliders

Covers what ConfigChoiceMap, ConfigStringChoice, ConfigChoiceU32,
ConfigSliderU32 and the tick-value ConfigSlider do today.

BindMapped has two combo forms. The span-of-values form pairs items authored in
the .ui file with values by index, which is what keeps display text in the uic
translation path. The pair form populates the combo, for the option sets that
are computed at runtime -- the JIT cores this platform builds, the generated SD
card sizes -- and so cannot be authored in Designer.

Two existing behaviours preserved deliberately: a mapped combo selects index -1
rather than snapping when no value matches, and a tick slider disables itself
when the config value matches no tick.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 7: Float sliders and user paths

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Create: `Source/QtTests/ConfigBinderFloatAndPathTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ConfigWidget::FloatSliderRange` (Task 2); `ConfigWidget::ValueBinding` (Task 5).
- Produces:
  - `struct ConfigWidget::FloatSliderHandle { QSlider* slider; FloatSliderRange range; float Value() const; }`
  - `FloatSliderHandle BindFloat(QSlider*, const Config::Info<float>&, float minimum, float maximum, float step, Config::Layer* = nullptr)`
  - `void MirrorFloatValue(QLabel*, FloatSliderHandle, const QString& format)`
  - `void BindUserPath(QLineEdit*, unsigned dir_index, const Config::Info<std::string>&, Config::Layer* = nullptr)`

**Notes for the implementer:**
- `BindFloat` is the one bind that *does* set the slider's `minimum` and `maximum`. It has to: a
  Designer slider is integer-valued, so the position range is derived from the float range the
  caller passes. Every other slider bind leaves the `.ui` range alone.
- `BindFloat` returns a handle rather than exposing a downcast because four call sites
  (`GameConfigWidget.cpp:169,170,252,257`) read `ConfigFloatSlider::GetValue()` to drive a value
  label. DolphinQt uses `qobject_cast` exclusively and it cannot reach a non-`Q_OBJECT` binding
  subclass, so the value comes back through the handle instead.
- `BindUserPath` reproduces `ConfigUserPath` exactly, including the part that is easy to miss: the
  **displayed** text is the config value if non-empty, otherwise `File::GetUserPath(dir_index)`. An
  empty config value means "use the current user path", and saving calls `File::SetUserPath` as
  well as writing config.
- **Deliberate coverage gap:** `ConfigUserPath::Update` shows a `ModalMessageBox` when the field is
  emptied. `ModalMessageBox::warning` calls `exec()`, which would block `qt-tests` forever, so the
  empty-field path is asserted only up to "config is left unchanged and the text is restored", by
  routing the warning through an injectable hook that the test replaces. Do not call `exec()` in a
  test.

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/ConfigBinderFloatAndPathTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>

#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
const Config::Info<float> TEST_FLOAT{{Config::System::Main, "BinderFloat", "Depth"}, 0.0f};
const Config::Info<std::string> TEST_PATH{{Config::System::Main, "BinderFloat", "Path"}, ""};

class ConfigBinderFloatAndPathTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    m_saved_user_path = File::GetUserPath(D_WIIROOT_IDX);
    m_warnings = 0;
    ConfigWidget::SetPathWarningHandlerForTesting([this](QWidget*, const QString&) {
      ++m_warnings;
    });
  }

  void TearDown() override
  {
    ConfigWidget::SetPathWarningHandlerForTesting({});
    File::SetUserPath(D_WIIROOT_IDX, m_saved_user_path);
    Config::Shutdown();
  }

  std::string m_saved_user_path;
  int m_warnings = 0;
};
}  // namespace

TEST_F(ConfigBinderFloatAndPathTest, FloatSliderDerivesItsPositionRangeFromTheFloatRange)
{
  QSlider slider{Qt::Horizontal};
  Config::SetBase(TEST_FLOAT, 50.0f);

  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.0f, 200.0f, 1.0f);

  EXPECT_EQ(slider.minimum(), 0);
  EXPECT_EQ(slider.maximum(), 200);
  EXPECT_EQ(slider.value(), 50);
  EXPECT_FLOAT_EQ(handle.Value(), 50.0f);
}

TEST_F(ConfigBinderFloatAndPathTest, FloatSliderRoundTripsThroughTheStep)
{
  QSlider slider{Qt::Horizontal};
  Config::SetBase(TEST_FLOAT, 1.0f);
  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.5f, 2.0f, 0.25f);
  EXPECT_EQ(slider.value(), 2) << "(1.0 - 0.5) / 0.25";

  slider.setValue(5);
  EXPECT_FLOAT_EQ(Config::Get(TEST_FLOAT), 1.75f);
  EXPECT_FLOAT_EQ(handle.Value(), 1.75f);
}

TEST_F(ConfigBinderFloatAndPathTest, MirrorFloatValueTracksTheSlider)
{
  QSlider slider{Qt::Horizontal};
  QLabel label;
  Config::SetBase(TEST_FLOAT, 0.0f);
  const auto handle = ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.0f, 200.0f, 1.0f);

  ConfigWidget::MirrorFloatValue(&label, handle, QStringLiteral("%.0f%%"));
  EXPECT_EQ(label.text(), QStringLiteral("0%")) << "the label is set immediately, not only on move";

  slider.setValue(75);
  EXPECT_EQ(label.text(), QStringLiteral("75%"));
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathShowsTheUserPathWhenConfigIsEmpty)
{
  File::SetUserPath(D_WIIROOT_IDX, "/tmp/dolphin-test-wiiroot");
  Config::SetBase(TEST_PATH, "");
  QLineEdit edit;

  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-wiiroot"))
      << "an empty config value means 'use the current user path'";
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathPrefersTheConfigValueWhenItIsSet)
{
  File::SetUserPath(D_WIIROOT_IDX, "/tmp/dolphin-test-wiiroot");
  Config::SetBase(TEST_PATH, "/tmp/dolphin-test-configured");
  QLineEdit edit;

  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-configured"));
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathWritesBothConfigAndTheUserPath)
{
  QLineEdit edit;
  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  edit.setText(QStringLiteral("  /tmp/dolphin-test-new  "));
  emit edit.editingFinished();

  EXPECT_EQ(Config::Get(TEST_PATH), "/tmp/dolphin-test-new") << "and it is trimmed";
  EXPECT_EQ(File::GetUserPath(D_WIIROOT_IDX), "/tmp/dolphin-test-new");
}

TEST_F(ConfigBinderFloatAndPathTest, UserPathRejectsAnEmptyValueAndRestoresTheText)
{
  Config::SetBase(TEST_PATH, "/tmp/dolphin-test-keep");
  QLineEdit edit;
  ConfigWidget::BindUserPath(&edit, D_WIIROOT_IDX, TEST_PATH);

  edit.setText(QStringLiteral("   "));
  emit edit.editingFinished();

  EXPECT_EQ(m_warnings, 1);
  EXPECT_EQ(Config::Get(TEST_PATH), "/tmp/dolphin-test-keep");
  EXPECT_EQ(edit.text(), QStringLiteral("/tmp/dolphin-test-keep"));
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  ConfigBinderFloatAndPathTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `BindFloat`, `MirrorFloatValue`, `BindUserPath` or
`SetPathWarningHandlerForTesting`.

- [x] **Step 3: Write the minimal implementation**

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
#include <functional>

#include "DolphinQt/Config/Binder/ConfigSliderMapping.h"
```

```cpp
class QLabel;
class QSlider;
```

```cpp
// A float slider maps its float range onto integer positions, so unlike every other slider bind
// this one *does* set minimum and maximum: a Designer slider has no float range to author.
// The handle exists because four call sites read the mapped value back to drive a value label.
struct FloatSliderHandle
{
  QSlider* slider = nullptr;
  FloatSliderRange range{};

  float Value() const;
};

FloatSliderHandle BindFloat(QSlider* widget, const Config::Info<float>& setting, float minimum,
                            float maximum, float step, Config::Layer* layer = nullptr);

// `format` is a printf-style format taking one double, e.g. "%.0f%%". Sets the label immediately
// and on every subsequent slider move.
void MirrorFloatValue(QLabel* label, FloatSliderHandle handle, const QString& format);

// Displays the config value if it is non-empty, otherwise File::GetUserPath(dir_index): an empty
// config value means "use the current user path". Saving sets both.
void BindUserPath(QLineEdit* widget, unsigned int dir_index,
                  const Config::Info<std::string>& setting, Config::Layer* layer = nullptr);

// Replaces the modal warning shown when a user path field is emptied. Passing an empty function
// restores the default. Exists so qt-tests can assert the rejection without a modal exec() that
// would block forever.
using PathWarningHandler = std::function<void(QWidget* parent, const QString& message)>;
void SetPathWarningHandlerForTesting(PathWarningHandler handler);
```

In `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`, add to the anonymous namespace:

```cpp
PathWarningHandler s_path_warning_handler;

void WarnAboutPath(QWidget* parent, const QString& message)
{
  if (s_path_warning_handler)
  {
    s_path_warning_handler(parent, message);
    return;
  }
  ModalMessageBox::warning(parent, QObject::tr("Empty Value"), message);
}

class FloatSliderBinding final : public ValueBinding<QSlider, float>
{
public:
  FloatSliderBinding(QSlider* slider, const Config::Info<float>& setting, FloatSliderRange range,
                     Config::Layer* layer)
      : ValueBinding(slider, setting, layer), m_range(range)
  {
    slider->setMinimum(0);
    slider->setMaximum(m_range.PositionCount());
    slider->setTickInterval(1);

    connect(slider, &QSlider::valueChanged, this, &FloatSliderBinding::OnValueChanged);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    GetTypedWidget()->setValue(m_range.PositionForValue(Read()));
  }
  void OnValueChanged(int position) { Save(m_range.ValueForPosition(position)); }

  const FloatSliderRange m_range;
};

class UserPathBinding final : public ValueBinding<QLineEdit, std::string>
{
public:
  UserPathBinding(QLineEdit* edit, unsigned int dir_index,
                  const Config::Info<std::string>& setting, Config::Layer* layer)
      : ValueBinding(edit, setting, layer), m_dir_index(dir_index)
  {
    connect(edit, &QLineEdit::editingFinished, this, &UserPathBinding::OnEditingFinished);
    RefreshFromConfig();
  }

private:
  void LoadFromConfig() override
  {
    // An empty config value means "use the current user path"; config only seeds UserPath at
    // startup, so the effective path is what the field must show.
    const std::string config_value = Read();
    const std::string effective =
        config_value.empty() ? File::GetUserPath(m_dir_index) : config_value;
    GetTypedWidget()->setText(QString::fromStdString(effective));
  }

  void OnEditingFinished()
  {
    auto* const edit = GetTypedWidget();
    const QString trimmed = edit->text().trimmed();
    if (trimmed.isEmpty())
    {
      WarnAboutPath(edit,
                    QObject::tr("This field cannot be left empty. Please enter a value."));
      RefreshFromConfig();
      return;
    }

    const std::string value = trimmed.toStdString();
    File::SetUserPath(m_dir_index, value);
    Save(value);
  }

  const unsigned int m_dir_index;
};
```

`ConfigWidgetBinder.cpp` needs `#include <QLabel>`, `#include "Common/FileUtil.h"` and
`#include "DolphinQt/QtUtils/ModalMessageBox.h"`.

And the entry points:

```cpp
float FloatSliderHandle::Value() const
{
  return range.ValueForPosition(slider->value());
}

FloatSliderHandle BindFloat(QSlider* widget, const Config::Info<float>& setting, float minimum,
                            float maximum, float step, Config::Layer* layer)
{
  const FloatSliderRange range{minimum, maximum, step};
  new FloatSliderBinding{widget, setting, range, layer};
  return FloatSliderHandle{widget, range};
}

void MirrorFloatValue(QLabel* label, FloatSliderHandle handle, const QString& format)
{
  const auto update = [label, handle, format] {
    label->setText(QString::asprintf(format.toUtf8().constData(),
                                     static_cast<double>(handle.Value())));
  };
  QObject::connect(handle.slider, &QSlider::valueChanged, label, update);
  update();
}

void BindUserPath(QLineEdit* widget, unsigned int dir_index,
                  const Config::Info<std::string>& setting, Config::Layer* layer)
{
  new UserPathBinding{widget, dir_index, setting, layer};
}

void SetPathWarningHandlerForTesting(PathWarningHandler handler)
{
  s_path_warning_handler = std::move(handler);
}
```

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 32 tests — 25 from Tasks 3 to 6, 7 new.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigBinderFloatAndPathTest.cpp
git commit -m "DolphinQt: bind float sliders and user path fields

BindFloat is the one bind that sets a slider's range, because a Designer slider
is integer-valued and the float range only exists in code. It returns a handle
so a value label can read the mapped float back without a downcast that
qobject_cast cannot express.

BindUserPath keeps ConfigUserPath's rule that an empty config value means 'use
the current user path', so the field shows the effective path rather than an
empty box, and saving sets File::SetUserPath as well as config.

The empty-field warning goes through an injectable handler, because
ModalMessageBox::warning calls exec() and would block qt-tests forever.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 8: Override semantics — bold, right-click-to-clear, label font mirroring

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Create: `Source/QtTests/ConfigBinderOverrideTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ConfigWidget::Logic::{IsLocal, ClearLocal}` (Task 1); `ConfigBinding` (Task 4).
- Produces:
  - `void ConfigBinding::AddFontMirror(QWidget* follower)` — the follower's font is set alongside
    the binding's own whenever the override state is re-applied.
  - `void ConfigWidget::MirrorFont(QLabel* label, QWidget* control)` — replaces
    `ConfigSliderLabel`, `ConfigIntegerLabel` and `ConfigFloatLabel`, all three of which do only
    this.
  - `ConfigBinding::eventFilter` gains the `QEvent::MouseButtonPress` right-button case.

**This is the highest-risk task in the slice.** Two behaviours change *mechanism*, not just call
site: a virtual `mousePressEvent` override becomes an event-filter case, and three label classes
become one function. Reference the original at
`Source/Core/DolphinQt/Config/ConfigControls/ConfigControl.h:96-115`.

**Exact semantics to reproduce** (`ConfigControl.h:104-115`):
- Right button **and** a layer is present → `DeleteKey`, `Config::OnConfigChanged()`, and the event
  is **swallowed**: it never reaches the widget.
- Any other case → the event is forwarded unchanged. A global (layerless) binding must not swallow
  right-clicks, or context menus stop working.

**One intentional divergence, and it is a fix:** `ConfigControl::ConnectConfig` applies the bold
font only from the `ConfigChanged` handler, so a per-game pane that is opened and not touched shows
no bold at all until something else changes the config. The binder applies it at bind time as well,
via the `RefreshFromConfig()` each binding already calls in its constructor. Call this out in the
commit message. It changes no behaviour in this slice — nothing binds yet — and it is why the
"no visible behaviour change" constraint stays satisfiable when panes migrate in slice 5 and later:
the panes will look *more* correct, not different.

**Why `AddFontMirror` and not another broadcaster subscriber:** the existing label classes subscribe
to `Settings::ConfigChanged` and then read the control's font, which only works because the
control's subscription happens to have been connected first. Hanging the mirror off the binding
removes the ordering dependency entirely.

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/ConfigBinderOverrideTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderOverride", "Bool"}, false};

class ConfigBinderOverrideTest : public ::testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }

  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }

  static void RightClick(QWidget* widget)
  {
    QMouseEvent press{QEvent::MouseButtonPress, QPointF{1, 1},   QPointF{1, 1},
                      Qt::RightButton,          Qt::RightButton, Qt::NoModifier};
    QCoreApplication::sendEvent(widget, &press);
  }
};
}  // namespace

TEST_F(ConfigBinderOverrideTest, LayeredOverrideShowsBoldAndClearingRemovesIt)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  EXPECT_FALSE(box.font().bold());

  box.setChecked(true);  // creates the per-game override
  NotifyConfigChanged();
  EXPECT_TRUE(box.font().bold());

  RightClick(&box);
  EXPECT_FALSE(layer.Exists(TEST_BOOL.GetLocation()));
  EXPECT_FALSE(box.font().bold());
  EXPECT_FALSE(box.isChecked()) << "the widget falls back to the base value";
}

TEST_F(ConfigBinderOverrideTest, GlobalBindingIsBoldWhenANonBaseLayerIsActive)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);
  EXPECT_FALSE(box.font().bold());

  Config::SetCurrent(TEST_BOOL, true);
  NotifyConfigChanged();
  EXPECT_TRUE(box.font().bold()) << "the active layer is no longer Base";
}

TEST_F(ConfigBinderOverrideTest, RightClickOnAGlobalBindingIsForwardedNotSwallowed)
{
  Config::SetBase(TEST_BOOL, true);
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  RightClick(&box);

  // Nothing to clear without a layer, and the widget must still get its own right-click so context
  // menus keep working.
  EXPECT_TRUE(Config::Get(TEST_BOOL));
  EXPECT_TRUE(box.isChecked());
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelFollowsTheControlsFont)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_BOOL, false);
  QCheckBox box;
  QLabel label{QStringLiteral("Setting:")};
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);
  ConfigWidget::MirrorFont(&label, &box);
  EXPECT_FALSE(label.font().bold());

  box.setChecked(true);
  NotifyConfigChanged();

  EXPECT_TRUE(box.font().bold());
  EXPECT_TRUE(label.font().bold()) << "the label marks the override alongside its control";
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelIsBoldImmediatelyWhenAlreadyOverridden)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  layer.Set(TEST_BOOL.GetLocation(), true);
  QCheckBox box;
  QLabel label{QStringLiteral("Setting:")};
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  ConfigWidget::MirrorFont(&label, &box);

  EXPECT_TRUE(label.font().bold()) << "MirrorFont applies the current state, not just future ones";
}

TEST_F(ConfigBinderOverrideTest, MirroredLabelSurvivesItsControlBeingDestroyed)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QLabel label{QStringLiteral("Setting:")};
  {
    QCheckBox box;
    ConfigWidget::Bind(&box, TEST_BOOL, &layer);
    ConfigWidget::MirrorFont(&label, &box);
  }

  NotifyConfigChanged();  // must not touch the destroyed control or the freed binding
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  ConfigBinderOverrideTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `MirrorFont`. After adding only the declaration, the
right-click and bold cases fail at runtime.

- [x] **Step 3: Write the minimal implementation**

In `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`, add to the public section:

```cpp
  // `follower`'s font is set alongside this binding's whenever the override state is re-applied.
  // Used by MirrorFont so a label goes bold beside the control it names.
  void AddFontMirror(QWidget* follower);
```

and to the private section:

```cpp
  // QPointer so a follower outliving its control is safe rather than a dangling read.
  std::vector<QPointer<QWidget>> m_font_mirrors;
```

with `#include <vector>` and `#include <QPointer>`.

In `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`, replace `ApplyOverrideFont` and
`eventFilter`:

```cpp
void ConfigBinding::AddFontMirror(QWidget* follower)
{
  m_font_mirrors.emplace_back(follower);
  ApplyOverrideFont();
}

void ConfigBinding::ApplyOverrideFont()
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return;

  const bool local = Logic::IsLocal(m_location, m_layer);

  QFont font = widget->font();
  font.setBold(local);
  widget->setFont(font);

  for (const QPointer<QWidget>& mirror : m_font_mirrors)
  {
    if (mirror.isNull())
      continue;
    QFont mirror_font = mirror->font();
    mirror_font.setBold(local);
    mirror->setFont(mirror_font);
  }
}

bool ConfigBinding::eventFilter(QObject* watched, QEvent* event)
{
  // Was ConfigControl::mousePressEvent. Right-click clears a per-game override; with no layer
  // there is nothing to clear and the widget must still receive the click so context menus work.
  if (m_layer != nullptr && event->type() == QEvent::MouseButtonPress &&
      static_cast<QMouseEvent*>(event)->button() == Qt::RightButton)
  {
    Logic::ClearLocal(m_location, m_layer);
    Config::OnConfigChanged();
    // Refresh directly as well: Config::OnConfigChanged reaches other bound widgets through
    // Settings, which qt-tests does not construct.
    RefreshFromConfig();
    return true;
  }

  return QObject::eventFilter(watched, event);
}
```

`ConfigBinding.cpp` needs `#include <QMouseEvent>` and `#include "Common/Config/Config.h"`.

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
// Replaces ConfigSliderLabel, ConfigIntegerLabel and ConfigFloatLabel, which exist only to copy
// their control's font so the label goes bold beside an overridden setting. `control` must already
// be bound.
void MirrorFont(QLabel* label, QWidget* control);
```

and to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`:

```cpp
void MirrorFont(QLabel* label, QWidget* control)
{
  ConfigBinding* const binding = FindBinding(control);
  ASSERT_MSG(COMMON, binding != nullptr, "MirrorFont called on an unbound control");
  if (binding == nullptr)
    return;
  binding->AddFontMirror(label);
}
```

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 38 tests — 32 from Tasks 3 to 7, 6 new.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigBinding.h \
        Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigBinderOverrideTest.cpp
git commit -m "DolphinQt: move override marking and right-click-to-clear into the binder

Right-click clearing was a virtual mousePressEvent override; it becomes an event
filter case. It swallows the click only when a layer is present, so a global
control still gets its own right-click and context menus keep working.

MirrorFont replaces ConfigSliderLabel, ConfigIntegerLabel and ConfigFloatLabel.
All three did only one thing -- copy the control's font so the label goes bold
beside an overridden setting -- and all three depended on being connected to
ConfigChanged after the control was. Hanging the mirror off the binding removes
that ordering dependency.

One intentional divergence: ConfigControl applies the bold font only from its
ConfigChanged handler, so an untouched per-game pane showed no bold at all. The
binder applies it at bind time too.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 9: Descriptions and balloon tooltips

**Files:**
- Create: `Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.h`
- Create: `Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.cpp`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Modify: `Source/Core/DolphinQt/CMakeLists.txt` (binder target sources)
- Create: `Source/QtTests/BalloonTipFilterTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks. This is deliberately independent of `ConfigBinding`.
- Produces:
  - `class ConfigWidget::BalloonTipFilter : public QObject` — attached as a child of the widget it
    watches. Public: `void SetText(QString title, QString description)`,
    `bool HasPendingTooltipForTesting() const`, `void ShowTooltipNowForTesting()`.
  - `void ConfigWidget::SetDescription(QWidget*, QString title, QString description)` — creates or
    updates the widget's filter. An empty `title` falls back to the widget's own `text` property.
  - `QPoint ConfigWidget::ToolTipAnchor(const QWidget*)` — the balloon's arrow tip, in the widget's
    own coordinates.
  - `QString ConfigWidget::ToolTipDescription(const QWidget*)` — for tests and for the Task 11
    registry.

**Why this is not part of `ConfigBinding`:** `ToolTipPushButton` has balloon tooltips and no config
setting at all. Tying descriptions to a config binding would leave those buttons with nowhere to
put their text, so the filter stands alone and either kind of widget can have one.

**Two obstacles the implementer will hit, and their resolutions:**

1. **`initStyleOption` is protected.** `ToolTipCheckBox`, `ToolTipRadioButton` and `ToolTipSlider`
   call it to get the indicator or handle rectangle. A filter is not a subclass and cannot. Rebuild
   the style option from public API instead: `QStyleOption::initFrom(widget)` is public and sets
   `rect`, `state`, `direction`, `fontMetrics` and `palette`, which is all
   `SE_CheckBoxIndicator` and `SE_RadioButtonIndicator` consult. `QStyleOptionSlider` needs the
   range fields as well, every one of which `QSlider` exposes publicly. Keep the existing fallbacks
   for a null `style()`: 18 px indicator width, a 15×15 handle.
2. **`parentWidget()` is dereferenced unguarded today.** `ToolTipWidget::timerEvent` does
   `this->parentWidget()->mapToGlobal(GetToolTipPosition())` and `GetToolTipPosition()` returns
   `pos() + offset`. Because `pos()` is the widget's position *within* its parent,
   `parent->mapToGlobal(pos() + offset)` is identical to `widget->mapToGlobal(offset)` — so drop
   the `pos()` term, map through the widget itself, and the null-parent case stops being a crash.
   `ToolTipAnchor` therefore returns widget-local coordinates.

**Anchor positions to reproduce** (from the six `GetToolTipPosition` implementations):

| Widget | Anchor, in widget-local coordinates |
|---|---|
| `QComboBox`, `QSpinBox`, `QPushButton` | `(width/2, height/2)` |
| `QCheckBox` | `(indicator_width/2, height/2)` |
| `QRadioButton` | `(indicator_width/2, height/2)` |
| `QSlider` | centre of the handle rectangle |
| anything else | `(width/2, height/2)` |

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/BalloonTipFilterTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QSlider>
#include <gtest/gtest.h>

#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"
#include "DolphinQt/Config/Binder/BalloonTipFilter.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
class BalloonTipFilterTest : public ::testing::Test
{
protected:
  void TearDown() override { BalloonTip::HideBalloon(); }

  static void SendEnter(QWidget* widget)
  {
    QEnterEvent event{QPointF{1, 1}, QPointF{1, 1}, QPointF{1, 1}};
    QCoreApplication::sendEvent(widget, &event);
  }

  static void SendLeave(QWidget* widget)
  {
    QEvent event{QEvent::Leave};
    QCoreApplication::sendEvent(widget, &event);
  }
};
}  // namespace

TEST_F(BalloonTipFilterTest, AnchorIsTheCentreForAComboBox)
{
  QComboBox box;
  box.resize(200, 30);

  EXPECT_EQ(ConfigWidget::ToolTipAnchor(&box), QPoint(100, 15));
}

TEST_F(BalloonTipFilterTest, AnchorIsOverTheIndicatorForACheckBox)
{
  QCheckBox box{QStringLiteral("A rather long check box label")};
  box.resize(300, 24);

  const QPoint anchor = ConfigWidget::ToolTipAnchor(&box);

  // Style-independent assertion: the arrow points at the box, not at the middle of the label.
  EXPECT_GT(anchor.x(), 0);
  EXPECT_LT(anchor.x(), 40) << "anchored on the indicator, not the centre of a 300px widget";
  EXPECT_EQ(anchor.y(), 12);
}

TEST_F(BalloonTipFilterTest, AnchorFollowsASliderHandle)
{
  QSlider slider{Qt::Horizontal};
  slider.resize(200, 20);
  slider.setRange(0, 100);

  slider.setValue(0);
  const QPoint low = ConfigWidget::ToolTipAnchor(&slider);
  slider.setValue(100);
  const QPoint high = ConfigWidget::ToolTipAnchor(&slider);

  EXPECT_LT(low.x(), high.x()) << "the anchor tracks the handle, not the widget centre";
}

TEST_F(BalloonTipFilterTest, AnchorIsWidgetLocalSoAParentlessWidgetIsSafe)
{
  QComboBox orphan;  // no parent: the old ToolTipWidget dereferenced parentWidget() here
  orphan.resize(100, 20);

  EXPECT_EQ(ConfigWidget::ToolTipAnchor(&orphan), QPoint(50, 10));
}

TEST_F(BalloonTipFilterTest, SetDescriptionStoresTheText)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));

  EXPECT_EQ(ConfigWidget::ToolTipDescription(&box), QStringLiteral("Body"));
}

TEST_F(BalloonTipFilterTest, SetDescriptionTwiceUpdatesRatherThanStacksFilters)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("First"));
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Second"));

  EXPECT_EQ(ConfigWidget::ToolTipDescription(&box), QStringLiteral("Second"));
  EXPECT_EQ(box.findChildren<ConfigWidget::BalloonTipFilter*>().size(), 1);
}

TEST_F(BalloonTipFilterTest, AnEmptyTitleFallsBackToTheWidgetsOwnText)
{
  // ToolTipCheckBox and ToolTipRadioButton called SetTitle(label) in their constructors.
  QCheckBox box{QStringLiteral("Enable Progressive Scan")};
  ConfigWidget::SetDescription(&box, QString{}, QStringLiteral("Body"));

  EXPECT_EQ(ConfigWidget::ToolTipTitle(&box), QStringLiteral("Enable Progressive Scan"));
}

TEST_F(BalloonTipFilterTest, HoveringSchedulesTheTooltipAndLeavingCancelsIt)
{
  QComboBox box;
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());

  SendEnter(&box);
  EXPECT_TRUE(filter->HasPendingTooltipForTesting());

  SendLeave(&box);
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
}

TEST_F(BalloonTipFilterTest, HidingTheWidgetCancelsAPendingTooltip)
{
  QWidget parent;
  auto* const box = new QComboBox{&parent};
  ConfigWidget::SetDescription(box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box->findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  SendEnter(box);
  ASSERT_TRUE(filter->HasPendingTooltipForTesting());

  box->hide();
  EXPECT_FALSE(filter->HasPendingTooltipForTesting());
}

TEST_F(BalloonTipFilterTest, ShowingTheTooltipActivatesABalloonForThatWidget)
{
  QComboBox box;
  box.resize(200, 30);
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));
  auto* const filter = box.findChild<ConfigWidget::BalloonTipFilter*>();
  ASSERT_NE(filter, nullptr);

  // Skips the 300ms delay and takes the same path the timer does, so this stays deterministic.
  filter->ShowTooltipNowForTesting();

  EXPECT_TRUE(BalloonTip::IsWidgetBalloonTipActive(box));
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  BalloonTipFilterTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `BalloonTipFilter.h`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QObject>
#include <QString>

class QEvent;
class QWidget;

namespace ConfigWidget
{
// The balloon-tooltip behaviour of ToolTipWidget<Derived>, as an event filter rather than four
// virtual overrides, so it can attach to a widget built by uic. Attached as a child of the widget
// it watches, so it lives exactly as long as that widget.
//
// Deliberately independent of ConfigBinding: ToolTipPushButton has a balloon and no config setting.
class BalloonTipFilter final : public QObject
{
  Q_OBJECT

public:
  explicit BalloonTipFilter(QWidget* widget);
  ~BalloonTipFilter() override;

  void SetText(QString title, QString description);
  const QString& GetTitle() const { return m_title; }
  const QString& GetDescription() const { return m_description; }

  bool HasPendingTooltipForTesting() const { return m_timer_id.has_value(); }
  // Takes the timer's path without the delay, so tests need no sleep.
  void ShowTooltipNowForTesting() { ShowTooltip(); }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void timerEvent(QTimerEvent* event) override;

private:
  QWidget* GetWidget() const;
  void ShowTooltip();
  void CancelPendingTooltip();

  std::optional<int> m_timer_id;
  QString m_title;
  QString m_description;
};
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/BalloonTipFilter.h"

#include <QEvent>
#include <QTimerEvent>
#include <QWidget>

#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"
#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"

namespace ConfigWidget
{
namespace
{
constexpr int TOOLTIP_DELAY_MS = 300;
}

BalloonTipFilter::BalloonTipFilter(QWidget* widget) : QObject(widget)
{
  widget->installEventFilter(this);
}

BalloonTipFilter::~BalloonTipFilter() = default;

void BalloonTipFilter::SetText(QString title, QString description)
{
  m_title = std::move(title);
  m_description = std::move(description);
}

QWidget* BalloonTipFilter::GetWidget() const
{
  return qobject_cast<QWidget*>(parent());
}

bool BalloonTipFilter::eventFilter(QObject* watched, QEvent* event)
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return QObject::eventFilter(watched, event);

  switch (event->type())
  {
  case QEvent::Enter:
    // Don't restart if a timer is already running, or if the cursor is re-entering the widget after
    // having hovered the balloon itself.
    if (!m_timer_id && !BalloonTip::IsWidgetBalloonTipActive(*widget))
      m_timer_id = startTimer(TOOLTIP_DELAY_MS);
    break;

  case QEvent::Leave:
    // If the cursor is still inside the widget's bounding box but the balloon is covering that part
    // of it, keep the balloon open: it tracks the cursor and closes itself.
    if (!BalloonTip::IsCursorInsideWidgetBoundingBox(*widget) || !BalloonTip::IsCursorOnBalloonTip())
      CancelPendingTooltip();
    break;

  case QEvent::Hide:
    CancelPendingTooltip();
    break;

  default:
    break;
  }

  // Never swallow: these are observations, and the widget still needs its own hover handling.
  return QObject::eventFilter(watched, event);
}

void BalloonTipFilter::timerEvent(QTimerEvent* event)
{
  if (!m_timer_id || event->timerId() != *m_timer_id)
  {
    QObject::timerEvent(event);
    return;
  }

  killTimer(*m_timer_id);
  m_timer_id.reset();
  ShowTooltip();
}

void BalloonTipFilter::ShowTooltip()
{
  QWidget* const widget = GetWidget();
  if (widget == nullptr)
    return;

  // ToolTipWidget mapped through parentWidget(), which crashed for a parentless widget. The anchor
  // is widget-local, so mapping through the widget itself gives the same point and no crash.
  BalloonTip::ShowBalloon(m_title, m_description, widget->mapToGlobal(ToolTipAnchor(widget)),
                          widget);
}

void BalloonTipFilter::CancelPendingTooltip()
{
  if (m_timer_id)
  {
    killTimer(*m_timer_id);
    m_timer_id.reset();
  }
  BalloonTip::HideBalloon();
}
}  // namespace ConfigWidget
```

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
// Balloon tooltip text. An empty `title` falls back to the widget's own `text` property, which is
// what ToolTipCheckBox and ToolTipRadioButton did in their constructors.
void SetDescription(QWidget* widget, QString title, QString description);
QString ToolTipTitle(const QWidget* widget);
QString ToolTipDescription(const QWidget* widget);

// The balloon's arrow tip, in the widget's own coordinates.
QPoint ToolTipAnchor(const QWidget* widget);
```

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`:

```cpp
namespace
{
// initStyleOption is protected on QCheckBox, QRadioButton and QSlider, so a filter cannot call it.
// QStyleOption::initFrom is public and sets rect, state, direction, palette and fontMetrics, which
// is everything the indicator sub-element rects consult.
int IndicatorWidth(const QWidget* widget, QStyle::SubElement sub_element)
{
  constexpr int FALLBACK_WIDTH = 18;
  QStyle* const style = widget->style();
  if (style == nullptr)
    return FALLBACK_WIDTH;

  QStyleOptionButton opt;
  opt.initFrom(widget);
  opt.rect = widget->rect();
  const int width = style->subElementRect(sub_element, &opt, widget).width();
  return width > 0 ? width : FALLBACK_WIDTH;
}

QRect SliderHandleRect(const QSlider* slider)
{
  constexpr QRect FALLBACK_RECT{0, 0, 15, 15};
  QStyle* const style = slider->style();
  if (style == nullptr)
    return FALLBACK_RECT;

  // The fields QSlider::initStyleOption sets, all of them publicly readable.
  QStyleOptionSlider opt;
  opt.initFrom(slider);
  opt.rect = slider->rect();
  opt.orientation = slider->orientation();
  opt.minimum = slider->minimum();
  opt.maximum = slider->maximum();
  opt.sliderPosition = slider->sliderPosition();
  opt.sliderValue = slider->value();
  opt.singleStep = slider->singleStep();
  opt.pageStep = slider->pageStep();
  opt.upsideDown = slider->invertedAppearance();
  if (slider->orientation() == Qt::Horizontal)
    opt.state |= QStyle::State_Horizontal;

  const QRect rect =
      style->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, slider);
  return rect.isEmpty() ? FALLBACK_RECT : rect;
}
}  // namespace

QPoint ToolTipAnchor(const QWidget* widget)
{
  if (const auto* const slider = qobject_cast<const QSlider*>(widget))
    return SliderHandleRect(slider).center();
  if (qobject_cast<const QCheckBox*>(widget) != nullptr)
    return {IndicatorWidth(widget, QStyle::SE_CheckBoxIndicator) / 2, widget->height() / 2};
  if (qobject_cast<const QRadioButton*>(widget) != nullptr)
    return {IndicatorWidth(widget, QStyle::SE_RadioButtonIndicator) / 2, widget->height() / 2};

  return {widget->width() / 2, widget->height() / 2};
}

void SetDescription(QWidget* widget, QString title, QString description)
{
  if (title.isEmpty())
    title = widget->property("text").toString();

  auto* filter = widget->findChild<BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  if (filter == nullptr)
    filter = new BalloonTipFilter{widget};
  filter->SetText(std::move(title), std::move(description));
}

QString ToolTipTitle(const QWidget* widget)
{
  const auto* const filter =
      widget->findChild<const BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  return filter == nullptr ? QString{} : filter->GetTitle();
}

QString ToolTipDescription(const QWidget* widget)
{
  const auto* const filter =
      widget->findChild<const BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  return filter == nullptr ? QString{} : filter->GetDescription();
}
```

`ConfigWidgetBinder.cpp` needs `#include <QCheckBox>`, `#include <QRadioButton>`,
`#include <QStyle>`, `#include <QStyleOption>` and
`#include "DolphinQt/Config/Binder/BalloonTipFilter.h"`. `ConfigWidgetBinder.h` needs
`#include <QPoint>`.

Add to the `dolphinqt-config-binder` source list in `Source/Core/DolphinQt/CMakeLists.txt`:

```cmake
  Config/Binder/BalloonTipFilter.cpp
  Config/Binder/BalloonTipFilter.h
```

Because the binder now calls `BalloonTip`, add `Config/ToolTipControls/BalloonTip.cpp` and
`BalloonTip.h` to the binder target's sources as well, and remove them from `dolphin-emu`'s source
list so the class is defined exactly once. `ToolTipWidget.h` and the six `ToolTip*` subclasses stay
where they are and keep working; they are removed in the last migration slice, not here.

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 48 tests — 38 from Tasks 3 to 8, 10 new.

Also build the application, since this task moves a source file between targets:
```bash
cmake --build build --target dolphin-emu -j8 2>&1 | tail -5
```
Expected: links, with no duplicate-symbol error for `BalloonTip`.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.h \
        Source/Core/DolphinQt/Config/Binder/BalloonTipFilter.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/Core/DolphinQt/CMakeLists.txt \
        Source/QtTests/CMakeLists.txt Source/QtTests/BalloonTipFilterTest.cpp
git commit -m "DolphinQt: re-home balloon tooltips onto an event filter

ToolTipWidget's four virtual overrides become one filter that attaches to any
widget, including one built by uic. It is independent of ConfigBinding on
purpose: ToolTipPushButton has a balloon and no config setting.

Two things could not be carried over literally. initStyleOption is protected, so
the check box, radio button and slider anchors rebuild their style option from
public API instead. And GetToolTipPosition returned parent-relative coordinates
that ToolTipWidget then mapped through parentWidget(), crashing for a parentless
widget; the anchor is now widget-local and maps through the widget itself, which
is the same point without the crash.

BalloonTip moves into the binder target so both it and dolphin-emu link one copy.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 10: Two settings driving one combo box

**Files:**
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`
- Create: `Source/QtTests/ConfigComplexBindingTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: `ConfigBinding` and its override behaviours (Tasks 4, 8).
- Produces:
  - `void ConfigBinding::SetSecondaryLocation(Config::Location)` (protected) — a second location
    that also counts for the bold font and is also cleared on right-click.
  - `class ConfigWidget::ComplexBinding : public ConfigBinding`, declared in
    `ConfigWidgetBinder.h` so panes can reach it, with
    `using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>`,
    `using OptionVariant = std::variant<Config::DefaultState, u32, int, bool>`, and public
    `void Add(const QString& name, OptionVariant option1, OptionVariant option2)`,
    `void SetDefault(int index)`, `void Reset()`,
    `std::pair<Config::Location, Config::Location> GetLocations() const`.
  - `ComplexBinding* BindComplex(QComboBox*, const InfoVariant& setting1, const InfoVariant& setting2, Config::Layer* = nullptr)`

**Why this one returns a pointer:** every other bind is fire-and-forget, but callers must `Add()`
their options after construction — the option list is a list of *pairs of values*, not display
strings, so it cannot be authored in Designer. This mirrors today's `ConfigComplexChoice`, which the
three call sites also populate after constructing.

**Why the base gains a second location rather than this being a standalone class:** bold-when-
overridden and right-click-to-clear are the two riskiest behaviours in the binder. A standalone
class would be a second copy of both. One optional secondary location is the smallest change that
keeps a single copy, and two is the largest number any existing control uses.

**One behaviour reproduced although it looks wrong.** `ConfigComplexChoice::UpdateComboIndex` reads
a per-game value with `m_layer->Get(setting)`, which falls back to the setting's *default* when the
layer has no key. Every other control reads through `Exists` → `Config::GetBase`, falling back to
the *global* value. So a per-game complex choice can show a different value than a per-game plain
combo for the same setting. Reproduce it exactly here, comment it, and leave it alone: this slice
migrates mechanism, and changing a read path is a behaviour change that belongs in its own commit
with its own justification. The test below pins the existing behaviour so a later fix is a
deliberate, visible edit.

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/ConfigComplexBindingTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QCoreApplication>
#include <QMouseEvent>
#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigChangeBroadcaster.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
const Config::Info<bool> TEST_ENABLED{{Config::System::Main, "BinderComplex", "Enabled"}, false};
const Config::Info<int> TEST_MODE{{Config::System::Main, "BinderComplex", "Mode"}, 0};

class ConfigComplexBindingTest : public ::testing::Test
{
protected:
  void SetUp() override { Config::Init(); }
  void TearDown() override { Config::Shutdown(); }

  static void NotifyConfigChanged()
  {
    ConfigWidget::ConfigChangeBroadcaster::Instance().Broadcast();
  }

  static void RightClick(QWidget* widget)
  {
    QMouseEvent press{QEvent::MouseButtonPress, QPointF{1, 1},   QPointF{1, 1},
                      Qt::RightButton,          Qt::RightButton, Qt::NoModifier};
    QCoreApplication::sendEvent(widget, &press);
  }

  // The shape all three existing call sites use: an "off" row plus two "on" rows.
  static ConfigWidget::ComplexBinding* BindThreeOptions(QComboBox* box, Config::Layer* layer)
  {
    auto* const binding = ConfigWidget::BindComplex(box, TEST_ENABLED, TEST_MODE, layer);
    binding->Add(QStringLiteral("Off"), false, 0);
    binding->Add(QStringLiteral("On, mode 1"), true, 1);
    binding->Add(QStringLiteral("On, mode 2"), true, 2);
    return binding;
  }
};
}  // namespace

TEST_F(ConfigComplexBindingTest, SelectingAnOptionWritesBothSettings)
{
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  box.setCurrentIndex(2);

  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);
}

TEST_F(ConfigComplexBindingTest, TheIndexReflectsTheCurrentPairOfValues)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 1);
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigComplexBindingTest, AddingOptionsDoesNotWriteConfig)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 2);
  QComboBox box;

  BindThreeOptions(&box, nullptr);

  // Add() must be signal-blocked: the first addItem selects index 0, which would otherwise save
  // "Off" over the values that were already there.
  EXPECT_TRUE(Config::Get(TEST_ENABLED));
  EXPECT_EQ(Config::Get(TEST_MODE), 2);
}

TEST_F(ConfigComplexBindingTest, ADefaultStateOptionMatchesTheSettingsOwnDefault)
{
  QComboBox box;
  auto* const binding = ConfigWidget::BindComplex(&box, TEST_ENABLED, TEST_MODE, nullptr);
  binding->Add(QStringLiteral("Auto"), Config::DefaultState::Enabled,
               Config::DefaultState::Enabled);
  binding->Add(QStringLiteral("On, mode 2"), true, 2);

  // Both settings are untouched, so both hold their defaults.
  EXPECT_EQ(box.currentIndex(), 0);
}

TEST_F(ConfigComplexBindingTest, NoMatchingOptionSelectsTheConfiguredDefaultIndex)
{
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 99);
  QComboBox box;
  auto* const binding = BindThreeOptions(&box, nullptr);

  binding->SetDefault(1);
  NotifyConfigChanged();

  EXPECT_EQ(box.currentIndex(), 1);
}

TEST_F(ConfigComplexBindingTest, WithoutADefaultIndexNoMatchSelectsNothing)
{
  Config::SetBase(TEST_MODE, 99);
  QComboBox box;
  BindThreeOptions(&box, nullptr);

  EXPECT_EQ(box.currentIndex(), -1);
}

TEST_F(ConfigComplexBindingTest, ResetClearsBothTheItemsAndTheOptions)
{
  QComboBox box;
  auto* const binding = BindThreeOptions(&box, nullptr);
  ASSERT_EQ(box.count(), 3);

  binding->Reset();

  EXPECT_EQ(box.count(), 0);
  binding->Add(QStringLiteral("Only"), false, 0);
  EXPECT_EQ(box.count(), 1);
}

TEST_F(ConfigComplexBindingTest, EitherOverriddenSettingMakesTheComboBold)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QComboBox box;
  BindThreeOptions(&box, &layer);
  EXPECT_FALSE(box.font().bold());

  layer.Set(TEST_MODE.GetLocation(), 2);
  NotifyConfigChanged();

  EXPECT_TRUE(box.font().bold()) << "the second setting alone is enough";
}

TEST_F(ConfigComplexBindingTest, RightClickClearsBothKeys)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QComboBox box;
  BindThreeOptions(&box, &layer);
  box.setCurrentIndex(2);
  ASSERT_TRUE(layer.Exists(TEST_ENABLED.GetLocation()));
  ASSERT_TRUE(layer.Exists(TEST_MODE.GetLocation()));

  RightClick(&box);

  EXPECT_FALSE(layer.Exists(TEST_ENABLED.GetLocation()));
  EXPECT_FALSE(layer.Exists(TEST_MODE.GetLocation()));
}

TEST_F(ConfigComplexBindingTest, PerGameReadFallsBackToTheSettingDefaultNotTheGlobalValue)
{
  // Pins existing ConfigComplexChoice behaviour, which differs from every other per-game control:
  // it reads m_layer->Get(), so an absent key yields the setting's default rather than the global
  // value. Reproduced deliberately in this slice. If this test is ever changed, the change is the
  // fix and belongs in its own commit.
  Config::Layer layer{Config::LayerType::LocalGame};
  Config::SetBase(TEST_ENABLED, true);
  Config::SetBase(TEST_MODE, 2);
  QComboBox box;

  BindThreeOptions(&box, &layer);

  EXPECT_EQ(box.currentIndex(), 0) << "the defaults are false/0, which is the 'Off' row";
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  ConfigComplexBindingTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `BindComplex` and no `ComplexBinding`.

- [x] **Step 3: Write the minimal implementation**

In `Source/Core/DolphinQt/Config/Binder/ConfigBinding.h`, add to the protected section:

```cpp
  // A second location that also counts for the bold font and is also cleared on right-click.
  // Only ComplexBinding uses it; two is the most any control drives.
  void SetSecondaryLocation(Config::Location location);
```

and to the private section:

```cpp
  std::optional<Config::Location> m_secondary_location;
```

with `#include <optional>`.

In `Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp`, add and amend:

```cpp
void ConfigBinding::SetSecondaryLocation(Config::Location location)
{
  m_secondary_location = std::move(location);
  ApplyOverrideFont();
}
```

In `ApplyOverrideFont`, replace the single `local` computation with:

```cpp
  const bool local = Logic::IsLocal(m_location, m_layer) ||
                     (m_secondary_location.has_value() &&
                      Logic::IsLocal(*m_secondary_location, m_layer));
```

and in `eventFilter`, after `Logic::ClearLocal(m_location, m_layer);`, add:

```cpp
    if (m_secondary_location.has_value())
      Logic::ClearLocal(*m_secondary_location, m_layer);
```

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h`:

```cpp
#include <variant>

#include "Common/Config/Enums.h"
```

```cpp
// Two settings driving one combo box: today's ConfigComplexChoice. Returned by BindComplex rather
// than bound in one call, because the option list is pairs of *values*, which cannot be authored in
// Designer, so callers add them afterwards.
class ComplexBinding final : public ConfigBinding
{
public:
  using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>;
  using OptionVariant = std::variant<Config::DefaultState, u32, int, bool>;

  ComplexBinding(QComboBox* box, const InfoVariant& setting1, const InfoVariant& setting2,
                 Config::Layer* layer);

  void Add(const QString& name, OptionVariant option1, OptionVariant option2);
  // Index selected when no option matches the current values. -1, meaning "select nothing", until
  // set.
  void SetDefault(int index);
  void Reset();

  std::pair<Config::Location, Config::Location> GetLocations() const;

private:
  void LoadFromConfig() override;
  void OnIndexChanged(int index);

  const InfoVariant m_setting1;
  const InfoVariant m_setting2;
  std::vector<std::pair<OptionVariant, OptionVariant>> m_options;
  int m_default_index = -1;
};

ComplexBinding* BindComplex(QComboBox* widget, const ComplexBinding::InfoVariant& setting1,
                            const ComplexBinding::InfoVariant& setting2,
                            Config::Layer* layer = nullptr);
```

Add to `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`:

```cpp
namespace
{
Config::Location LocationOf(const ComplexBinding::InfoVariant& info)
{
  return std::visit([](const auto& setting) { return setting.GetLocation(); }, info);
}
}  // namespace

ComplexBinding::ComplexBinding(QComboBox* box, const InfoVariant& setting1,
                               const InfoVariant& setting2, Config::Layer* layer)
    : ConfigBinding(box, LocationOf(setting1), layer), m_setting1(setting1), m_setting2(setting2)
{
  SetSecondaryLocation(LocationOf(setting2));
  connect(box, &QComboBox::currentIndexChanged, this, &ComplexBinding::OnIndexChanged);
  RefreshFromConfig();
}

void ComplexBinding::Add(const QString& name, OptionVariant option1, OptionVariant option2)
{
  auto* const box = static_cast<QComboBox*>(GetWidget());
  // Blocked because the first addItem selects index 0, which would otherwise save that row over
  // whatever the config already holds.
  const QSignalBlocker blocker{box};
  box->addItem(name);
  m_options.emplace_back(std::move(option1), std::move(option2));
}

void ComplexBinding::SetDefault(int index)
{
  m_default_index = index;
}

void ComplexBinding::Reset()
{
  auto* const box = static_cast<QComboBox*>(GetWidget());
  const QSignalBlocker blocker{box};
  box->clear();
  m_options.clear();
}

std::pair<Config::Location, Config::Location> ComplexBinding::GetLocations() const
{
  return {LocationOf(m_setting1), LocationOf(m_setting2)};
}

void ComplexBinding::LoadFromConfig()
{
  // Deliberately NOT Logic::ReadValue. ConfigComplexChoice reads a per-game value with
  // Layer::Get(), which yields the setting's *default* when the layer has no key, where every other
  // control falls back to the global value. Reproduced as-is so this slice changes mechanism only;
  // ConfigComplexBindingTest pins it.
  const auto read = [this](const auto& setting) -> OptionVariant {
    if (GetLayer() != nullptr)
      return static_cast<OptionVariant>(GetLayer()->Get(setting));
    return static_cast<OptionVariant>(Config::Get(setting));
  };
  const auto default_of = [](const auto& setting) -> OptionVariant {
    return OptionVariant(setting.GetDefaultValue());
  };

  const auto matches = [&](const InfoVariant& info, const OptionVariant& option) {
    const OptionVariant wanted = std::holds_alternative<Config::DefaultState>(option) ?
                                     std::visit(default_of, info) :
                                     option;
    return std::visit(read, info) == wanted;
  };

  const auto it = std::ranges::find_if(m_options, [&](const auto& option) {
    return matches(m_setting1, option.first) && matches(m_setting2, option.second);
  });

  const int index = it == m_options.end() ?
                        m_default_index :
                        static_cast<int>(std::distance(m_options.begin(), it));

  auto* const box = static_cast<QComboBox*>(GetWidget());
  const QSignalBlocker blocker{box};
  box->setCurrentIndex(index);
}

void ComplexBinding::OnIndexChanged(int index)
{
  if (IsUpdating() || index < 0 || static_cast<size_t>(index) >= m_options.size())
    return;

  const auto set = [this](const auto& setting, const auto& value) {
    if (Config::Layer* const layer = GetLayer())
    {
      layer->Set(setting.GetLocation(), value);
      Config::OnConfigChanged();
      return;
    }
    Config::SetBaseOrCurrent(setting, value);
  };

  std::visit(set, m_setting1, m_options[static_cast<size_t>(index)].first);
  std::visit(set, m_setting2, m_options[static_cast<size_t>(index)].second);
}

ComplexBinding* BindComplex(QComboBox* widget, const ComplexBinding::InfoVariant& setting1,
                            const ComplexBinding::InfoVariant& setting2, Config::Layer* layer)
{
  return new ComplexBinding{widget, setting1, setting2, layer};
}
```

`ConfigWidgetBinder.cpp` needs `#include <QSignalBlocker>`.

**Note on `LoadFromConfig` blocking signals:** it does so on top of the base's `m_updating` guard,
matching the original's "will crash if not blocked" comment. Keep both — `m_updating` protects
against a save, the blocker protects against Qt re-entering `setCurrentIndex` during a model change.

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 58 tests — 48 from Tasks 3 to 9, 10 new.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigBinding.h \
        Source/Core/DolphinQt/Config/Binder/ConfigBinding.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.h \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigComplexBindingTest.cpp
git commit -m "DolphinQt: bind two settings to one combo box

Covers ConfigComplexChoice. BindComplex returns the binding, because the option
list is pairs of values rather than display strings and so cannot be authored in
Designer; callers add options afterwards, as they do today.

ConfigBinding gains one optional secondary location instead of this being a
standalone class, so bold-when-overridden and right-click-to-clear exist in one
copy rather than two.

ConfigComplexChoice reads per-game values with Layer::Get, which falls back to
the setting's default where every other control falls back to the global value.
That is reproduced here rather than fixed, and a test pins it, so the fix stays
a deliberate separate change.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 11: The setting registry

**Files:**
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.h`
- Create: `Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.cpp`
- Modify: `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp` (record from each bind)
- Modify: `Source/Core/DolphinQt/CMakeLists.txt` (binder target sources)
- Create: `Source/QtTests/ConfigSettingRegistryTest.cpp`
- Modify: `Source/QtTests/CMakeLists.txt`

**Interfaces:**
- Consumes: every `Bind*` entry point (Tasks 4 to 10); `SetDescription` (Task 9).
- Produces:
  - `enum class ConfigWidget::SettingKind { Bool, Int, U32, Float, String, Path, Choice, Complex }`
  - `struct ConfigWidget::SettingEntry` — `location`, `kind`, `per_game`, `title`, `description`,
    `choices`, `minimum`, `maximum`, `step`.
  - `class ConfigWidget::ConfigSettingRegistry` with `static ConfigSettingRegistry& Instance()`,
    `void Record(SettingEntry)`, `void SetText(const Config::Location&, QString title, QString description)`,
    `const SettingEntry* Find(const Config::Location&) const`,
    `std::span<const SettingEntry> Entries() const`, `void ClearForTesting()`.

**Why this exists now rather than later:** it is the one decision in the desktop migration that
changes the OSD follow-up's cost. 194 bind sites get written across 27 slices; adding the recording
afterwards means revisiting all of them. See
`docs/superpowers/specs/2026-09-18-fullscreen-osd-followup-design.md` §5. It adds no argument any
caller has to think about — every field is something `Bind` was already handed or can read off the
widget it was handed.

**Scope limits, stated so the implementer does not over-build:**
- `Record` **merges** by location. The same setting is bound in both a global pane and a per-game
  pane, and a pane can be opened twice, so recording must be idempotent rather than appending.
  Merging fills empty fields and leaves non-empty ones alone.
- `choices` is captured at bind time from the widget, which works because every choice bind either
  finds the items already in place from the `.ui` file or adds them itself before binding.
  `ComplexBinding` is the exception — it is populated afterwards — so it records `kind = Complex`
  with empty `choices` and records nothing further. Do not try to make it complete; the follow-up
  project's settings browser will special-case complex settings anyway.
- Enumeration order is bind order, so it is deterministic and a browser can rely on it.

- [x] **Step 1: Write the failing tests**

Create `Source/QtTests/ConfigSettingRegistryTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <span>

#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <gtest/gtest.h>

#include "Common/Config/Config.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"
#include "DolphinQt/Config/Binder/ConfigWidgetBinder.h"

namespace
{
const Config::Info<bool> TEST_BOOL{{Config::System::Main, "BinderRegistry", "Bool"}, false};
const Config::Info<int> TEST_INT{{Config::System::Main, "BinderRegistry", "Int"}, 0};
const Config::Info<float> TEST_FLOAT{{Config::System::Main, "BinderRegistry", "Float"}, 0.0f};

class ConfigSettingRegistryTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Config::Init();
    // A process-wide registry, so each test starts from empty.
    ConfigWidget::ConfigSettingRegistry::Instance().ClearForTesting();
  }
  void TearDown() override { Config::Shutdown(); }

  static const ConfigWidget::SettingEntry* Find(const Config::Location& location)
  {
    return ConfigWidget::ConfigSettingRegistry::Instance().Find(location);
  }
};
}  // namespace

TEST_F(ConfigSettingRegistryTest, BindingRecordsTheLocationAndKind)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  const auto* const entry = Find(TEST_BOOL.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Bool);
  EXPECT_FALSE(entry->per_game);
}

TEST_F(ConfigSettingRegistryTest, ABindingWithALayerIsMarkedPerGame)
{
  Config::Layer layer{Config::LayerType::LocalGame};
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL, &layer);

  ASSERT_NE(Find(TEST_BOOL.GetLocation()), nullptr);
  EXPECT_TRUE(Find(TEST_BOOL.GetLocation())->per_game);
}

TEST_F(ConfigSettingRegistryTest, BindingTheSameSettingTwiceMergesIntoOneEntry)
{
  // The same setting appears in a global pane and a per-game pane, and panes reopen.
  QCheckBox global;
  Config::Layer layer{Config::LayerType::LocalGame};
  QCheckBox per_game;

  ConfigWidget::Bind(&global, TEST_BOOL);
  ConfigWidget::Bind(&per_game, TEST_BOOL, &layer);

  EXPECT_EQ(ConfigWidget::ConfigSettingRegistry::Instance().Entries().size(), 1u);
}

TEST_F(ConfigSettingRegistryTest, SetDescriptionFillsInTheTitleAndDescription)
{
  QCheckBox box;
  ConfigWidget::Bind(&box, TEST_BOOL);

  ConfigWidget::SetDescription(&box, QStringLiteral("Progressive Scan"),
                               QStringLiteral("Enables 480p output."));

  const auto* const entry = Find(TEST_BOOL.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->title, QStringLiteral("Progressive Scan"));
  EXPECT_EQ(entry->description, QStringLiteral("Enables 480p output."));
}

TEST_F(ConfigSettingRegistryTest, SetDescriptionOnAnUnboundWidgetRecordsNothing)
{
  QCheckBox box;  // a plain tooltip, no config setting: ToolTipPushButton's case
  ConfigWidget::SetDescription(&box, QStringLiteral("Title"), QStringLiteral("Body"));

  EXPECT_TRUE(ConfigWidget::ConfigSettingRegistry::Instance().Entries().empty());
}

TEST_F(ConfigSettingRegistryTest, SpinBoxRangeIsReadOffTheWidget)
{
  QSpinBox spin;
  spin.setRange(10, 90);  // as the .ui file would have set it

  ConfigWidget::Bind(&spin, TEST_INT);

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  ASSERT_TRUE(entry->minimum.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 10.0);
  EXPECT_DOUBLE_EQ(*entry->maximum, 90.0);
}

TEST_F(ConfigSettingRegistryTest, FloatSliderRecordsItsFloatRangeNotItsPositionRange)
{
  QSlider slider{Qt::Horizontal};
  ConfigWidget::BindFloat(&slider, TEST_FLOAT, 0.5f, 2.0f, 0.25f);

  const auto* const entry = Find(TEST_FLOAT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Float);
  ASSERT_TRUE(entry->step.has_value());
  EXPECT_DOUBLE_EQ(*entry->minimum, 0.5);
  EXPECT_DOUBLE_EQ(*entry->maximum, 2.0);
  EXPECT_DOUBLE_EQ(*entry->step, 0.25);
}

TEST_F(ConfigSettingRegistryTest, ChoiceBindingRecordsTheItemTexts)
{
  QComboBox box;
  box.addItems({QStringLiteral("Off"), QStringLiteral("On")});

  ConfigWidget::Bind(&box, TEST_INT);

  const auto* const entry = Find(TEST_INT.GetLocation());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->kind, ConfigWidget::SettingKind::Choice);
  ASSERT_EQ(entry->choices.size(), 2u);
  EXPECT_EQ(entry->choices[1], QStringLiteral("On"));
}

TEST_F(ConfigSettingRegistryTest, EntriesAreInBindOrder)
{
  QCheckBox box;
  QSpinBox spin;
  ConfigWidget::Bind(&spin, TEST_INT);
  ConfigWidget::Bind(&box, TEST_BOOL);

  const auto entries = ConfigWidget::ConfigSettingRegistry::Instance().Entries();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].location, TEST_INT.GetLocation());
  EXPECT_EQ(entries[1].location, TEST_BOOL.GetLocation());
}
```

Add to the `add_executable(qt-tests ...)` source list in `Source/QtTests/CMakeLists.txt`:

```cmake
  ConfigSettingRegistryTest.cpp
```

- [x] **Step 2: Run the tests to verify they fail**

Run:
```bash
cmake --build build --target qt-tests -j8 2>&1 | tail -20
```
Expected: FAIL at compile time — no `ConfigSettingRegistry.h`.

- [x] **Step 3: Write the minimal implementation**

Create `Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.h`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <span>
#include <vector>

#include <QString>

#include "Common/Config/ConfigInfo.h"

namespace ConfigWidget
{
enum class SettingKind
{
  Bool,
  Int,
  U32,
  Float,
  String,
  Path,
  Choice,
  Complex,
};

// What Bind() was handed, or could read off the widget it was handed. Nothing here is a new
// argument callers must supply.
struct SettingEntry
{
  Config::Location location;
  SettingKind kind = SettingKind::Bool;
  // True if any binding for this setting was given a Config::Layer, i.e. it is editable per game.
  bool per_game = false;
  QString title;
  QString description;
  // Choice kinds only. Empty for Complex, which is populated after binding.
  std::vector<QString> choices;
  // Numeric kinds only. Doubles so one field covers int, u32 and float ranges.
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::optional<double> step;
};

// Every Bind() records here, so a settings interface can be driven from the desktop panes' own
// definitions instead of re-authoring all of them. Its consumer is the fullscreen-OSD follow-up
// project; nothing in DolphinQt reads it yet.
class ConfigSettingRegistry
{
public:
  static ConfigSettingRegistry& Instance();

  // Idempotent: merges into any existing entry for the same location, filling empty fields and
  // leaving populated ones alone. The same setting is bound in both a global and a per-game pane,
  // and panes reopen.
  void Record(SettingEntry entry);
  void SetText(const Config::Location& location, QString title, QString description);

  const SettingEntry* Find(const Config::Location& location) const;
  // In bind order, so enumeration is deterministic.
  std::span<const SettingEntry> Entries() const { return m_entries; }

  void ClearForTesting();

private:
  ConfigSettingRegistry() = default;

  std::vector<SettingEntry> m_entries;
  std::map<Config::Location, size_t> m_index;
};
}  // namespace ConfigWidget
```

Create `Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Binder/ConfigSettingRegistry.h"

namespace ConfigWidget
{
ConfigSettingRegistry& ConfigSettingRegistry::Instance()
{
  static ConfigSettingRegistry instance;
  return instance;
}

void ConfigSettingRegistry::Record(SettingEntry entry)
{
  const auto [it, inserted] = m_index.try_emplace(entry.location, m_entries.size());
  if (inserted)
  {
    m_entries.push_back(std::move(entry));
    return;
  }

  SettingEntry& existing = m_entries[it->second];
  // A setting is per-game if *any* binding for it was layered.
  existing.per_game = existing.per_game || entry.per_game;
  if (existing.choices.empty())
    existing.choices = std::move(entry.choices);
  if (!existing.minimum.has_value())
    existing.minimum = entry.minimum;
  if (!existing.maximum.has_value())
    existing.maximum = entry.maximum;
  if (!existing.step.has_value())
    existing.step = entry.step;
  if (existing.title.isEmpty())
    existing.title = std::move(entry.title);
  if (existing.description.isEmpty())
    existing.description = std::move(entry.description);
}

void ConfigSettingRegistry::SetText(const Config::Location& location, QString title,
                                    QString description)
{
  const auto it = m_index.find(location);
  if (it == m_index.end())
    return;

  SettingEntry& entry = m_entries[it->second];
  entry.title = std::move(title);
  entry.description = std::move(description);
}

const SettingEntry* ConfigSettingRegistry::Find(const Config::Location& location) const
{
  const auto it = m_index.find(location);
  return it == m_index.end() ? nullptr : &m_entries[it->second];
}

void ConfigSettingRegistry::ClearForTesting()
{
  m_entries.clear();
  m_index.clear();
}
}  // namespace ConfigWidget
```

In `Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp`, add a recording helper to the
anonymous namespace and call it from every `Bind*` entry point:

```cpp
void RecordSetting(const Config::Location& location, SettingKind kind, Config::Layer* layer,
                   const QWidget* widget)
{
  SettingEntry entry;
  entry.location = location;
  entry.kind = kind;
  entry.per_game = layer != nullptr;

  if (const auto* const box = qobject_cast<const QComboBox*>(widget))
  {
    for (int i = 0; i < box->count(); ++i)
      entry.choices.push_back(box->itemText(i));
  }
  else if (const auto* const spin = qobject_cast<const QSpinBox*>(widget))
  {
    entry.minimum = spin->minimum();
    entry.maximum = spin->maximum();
    entry.step = spin->singleStep();
  }
  else if (const auto* const slider = qobject_cast<const QSlider*>(widget))
  {
    entry.minimum = slider->minimum();
    entry.maximum = slider->maximum();
  }

  ConfigSettingRegistry::Instance().Record(std::move(entry));
}
```

Every entry point gains one line after creating its binding, for example:

```cpp
void Bind(QCheckBox* widget, const Config::Info<bool>& setting, Config::Layer* layer, bool reverse)
{
  new CheckBoxBinding{widget, setting, layer, reverse};
  RecordSetting(setting.GetLocation(), SettingKind::Bool, layer, widget);
}
```

Use `SettingKind::Choice` for both combo `Bind` overloads, `BindMapped`'s combo forms and
`BindStringChoice`; `Int` for `QSpinBox`, `QSlider` and `QRadioButton`; `U32` for `BindScaled` and
the `u32` combo; `String` for `QLineEdit`; `Path` for `BindUserPath`; `Complex` for `BindComplex`.

`BindFloat` records its float range rather than the slider's position range, so it does not use the
`QSlider` branch above:

```cpp
  SettingEntry entry;
  entry.location = setting.GetLocation();
  entry.kind = SettingKind::Float;
  entry.per_game = layer != nullptr;
  entry.minimum = minimum;
  entry.maximum = maximum;
  entry.step = step;
  ConfigSettingRegistry::Instance().Record(std::move(entry));
```

`SetDescription` forwards to the registry when the widget is bound:

```cpp
void SetDescription(QWidget* widget, QString title, QString description)
{
  if (title.isEmpty())
    title = widget->property("text").toString();

  auto* filter = widget->findChild<BalloonTipFilter*>(QString{}, Qt::FindDirectChildrenOnly);
  if (filter == nullptr)
    filter = new BalloonTipFilter{widget};
  filter->SetText(title, description);

  // Unbound widgets get a tooltip and no registry entry: ToolTipPushButton has no setting.
  if (const ConfigBinding* const binding = FindBinding(widget))
  {
    ConfigSettingRegistry::Instance().SetText(binding->GetLocation(), std::move(title),
                                             std::move(description));
  }
}
```

Add to the `dolphinqt-config-binder` source list in `Source/Core/DolphinQt/CMakeLists.txt`:

```cmake
  Config/Binder/ConfigSettingRegistry.cpp
  Config/Binder/ConfigSettingRegistry.h
```

- [x] **Step 4: Run the tests to verify they pass**

Run:
```bash
cmake --build build --target qt-tests -j8 \
  && ./build/Binaries/Tests/qt-tests -platform offscreen
```
Expected: PASS, 67 tests — 58 from Tasks 3 to 10, 9 new.

- [x] **Step 5: Commit**

```bash
git add Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.h \
        Source/Core/DolphinQt/Config/Binder/ConfigSettingRegistry.cpp \
        Source/Core/DolphinQt/Config/Binder/ConfigWidgetBinder.cpp \
        Source/Core/DolphinQt/CMakeLists.txt \
        Source/QtTests/CMakeLists.txt Source/QtTests/ConfigSettingRegistryTest.cpp
git commit -m "DolphinQt: record every binding in a setting registry

Records only what Bind was already handed or can read off the widget it was
handed, so it adds no argument any caller has to think about.

Nothing in DolphinQt reads it yet. It is here now because 194 bind sites get
written across 27 migration slices, and adding the recording afterwards means
revisiting all of them; the fullscreen-OSD follow-up drives its settings browser
from this instead of re-authoring every setting in ImGui.

Recording is idempotent by location: the same setting is bound in both a global
and a per-game pane, and panes reopen.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 12: Keep `.ui` strings in the `.pot`

**Files:**
- Create: `Languages/generate-ui-strings.py`
- Create: `Languages/tests/UiStringFixture.ui`
- Create: `Languages/tests/PainterFixture.cpp`
- Create: `Languages/tests/test-ui-extraction.sh`
- Modify: `Languages/update-source-strings.sh`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: nothing from earlier tasks. This task is independent of Tasks 1 to 11 and could be done
  first; it is last because it is the one piece with no C++ in it.
- Produces: `Languages/generate-ui-strings.py <output-dir> [source-dir]` — runs `uic` over every
  `.ui` under `source-dir` into a mirror of that tree under `output-dir`, injects the `// i18n:`
  comments `uic` discards, and prints each generated header path on its own line. Always creates
  `output-dir`, and needs no `uic` at all when there are no `.ui` files.

**Why this must land before any pane is migrated:** `update-source-strings.sh` runs `xgettext` over
`*.cpp/*.h/*.c` under `Source`, so `.ui` XML is invisible to it. The first migrated string moves
silently out of the `.pot` and out of 29 locales. See spec §6.

**Everything below is measured, not assumed** — `uic` 6.11.1 and the fixture in this task:

- `uic` emits `QCoreApplication::translate("Class", "msgid", nullptr)`, and
  `<string comment="…">` becomes the third argument rather than a comment.
- `uic` discards `extracomment` entirely: it emits no comment at all.
- `--keyword=translate:2,3c` alone extracted **1 of 3** fixture strings. Both forms together
  extracted 3, with the disambiguated one carrying its `msgctxt`, and no duplicates.
- Adding both keywords to the real script today produces a `.pot` **byte-identical** to the current
  one apart from `POT-Creation-Date` — verified against the full `Source` tree. `QPainter::translate`
  calls contribute nothing, because `xgettext` only extracts string literals.

**A trap for whoever runs gate 2 (`.pot` msgid diff, spec §7.4):** the committed
`Languages/po/dolphin-emu.pot` is already stale against current `master` — 32 msgids in it are gone
from the source and 17 source strings are missing from it. Diff against a **freshly regenerated**
`.pot`, never against the committed one, or every slice will appear to delete 32 strings.

- [x] **Step 1: Write the failing test**

Create `Languages/tests/UiStringFixture.ui` — one plain string, one disambiguated, one annotated:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>UiStringFixture</class>
 <widget class="QWidget" name="UiStringFixture">
  <layout class="QVBoxLayout" name="layout">
   <item>
    <widget class="QCheckBox" name="plain">
     <property name="text">
      <string>Enable Progressive Scan</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QLabel" name="disambiguated">
     <property name="text">
      <string comment="Refers to the speed limiter">Limit</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QLabel" name="annotated">
     <property name="text">
      <string extracomment="A hexadecimal address, not a street address">Address:</string>
     </property>
    </widget>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
```

Create `Languages/tests/PainterFixture.cpp`:

```cpp
// Not compiled. It exists so the string-extraction test can prove that adding
// the two `translate` keywords does not pull in QPainter::translate calls.

void Draw(QPainter& p, int width, int height)
{
  p.translate(0, height);
  p.translate(width / 2.0, height / 2.0);
}
```

Create `Languages/tests/test-ui-extraction.sh` and `chmod +x` it:

```bash
#!/bin/bash

# Checks that strings which live in Qt Designer .ui files still reach the .pot
# file. Run by hand; this fork has no CI.
#
# Set UIC if uic is not on PATH. Homebrew's Qt keeps it in libexec.

cd "$(dirname "$0")"

SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

failures=0

fail()
{
	echo "FAIL: $1"
	failures=$((failures + 1))
}

# Writes $1 from the remaining arguments. xgettext writes no file at all when it
# finds no strings, so the file is created first to keep the callers simple.
extract()
{
	local pot=$1
	shift
	local keywords=$1
	shift
	: > "$pot"
	xgettext -o "$pot" $keywords --add-comments=i18n --from-code=utf-8 "$@"
}

BOTH_KEYWORDS="--keyword=translate:2 --keyword=translate:2,3c"

HEADER=$SCRATCH/headers/ui_UiStringFixture.h
python3 ../generate-ui-strings.py "$SCRATCH/headers" . > /dev/null || fail "generator exited non-zero"

if [ ! -f "$HEADER" ]; then
	echo "FAIL: no header generated from UiStringFixture.ui"
	exit 1
fi

grep -q '// i18n: A hexadecimal address, not a street address' "$HEADER" ||
	fail "extracomment was not injected into the generated header"

extract "$SCRATCH/both.pot" "$BOTH_KEYWORDS" "$HEADER"

grep -q '^msgid "Enable Progressive Scan"$' "$SCRATCH/both.pot" ||
	fail "plain string missing from the .pot"
grep -q '^msgid "Address:"$' "$SCRATCH/both.pot" ||
	fail "annotated string missing from the .pot"
grep -B1 '^msgid "Limit"$' "$SCRATCH/both.pot" | grep -q '^msgctxt "Refers to the speed limiter"$' ||
	fail "disambiguation did not become a msgctxt"
grep -q '^#\. i18n: A hexadecimal address, not a street address$' "$SCRATCH/both.pot" ||
	fail "translator comment missing from the .pot"

count=$(grep -c '^msgid "[^"]' "$SCRATCH/both.pot")
[ "$count" -eq 3 ] || fail "expected 3 strings, got $count (duplicates or over-extraction)"

# The reason both keyword forms are listed: translate:2,3c alone drops every
# string whose disambiguation uic emitted as nullptr, which is nearly all of
# them. Removing either form must break this test rather than silently lose
# strings from 29 locales.
extract "$SCRATCH/ctx-only.pot" "--keyword=translate:2,3c" "$HEADER"
ctx_only=$(grep -c '^msgid "[^"]' "$SCRATCH/ctx-only.pot")
[ "$ctx_only" -eq 1 ] ||
	fail "expected translate:2,3c alone to extract 1 of 3 strings, got $ctx_only"

# QPainter::translate is also called `translate`, so the keywords must not turn
# painter transforms into msgids.
extract "$SCRATCH/painter.pot" "$BOTH_KEYWORDS" PainterFixture.cpp
painter=$(grep -c '^msgid "[^"]' "$SCRATCH/painter.pot")
[ "$painter" -eq 0 ] || fail "non-string translate() calls were extracted ($painter)"

if [ "$failures" -eq 0 ]; then
	echo "PASS: .ui strings reach the .pot with context and translator comments"
	exit 0
fi

echo "$failures check(s) failed"
exit 1
```

- [x] **Step 2: Run the test to verify it fails**

Run:
```bash
./Languages/tests/test-ui-extraction.sh
```
Expected: FAIL, exit 1, with `can't open file '.../generate-ui-strings.py'`, then
`FAIL: generator exited non-zero` and `FAIL: no header generated from UiStringFixture.ui`.

- [x] **Step 3: Write the minimal implementation**

Create `Languages/generate-ui-strings.py` and `chmod +x` it:

```python
#!/usr/bin/env python3

"""Runs uic over every .ui file under a source tree and restores the translator
comments uic discards, so that xgettext can see the strings that live in Qt
Designer forms.

Prints the path of each generated header, one per line, for the caller to append
to its xgettext file list."""

import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ElementTree


def find_uic():
    if os.environ.get("UIC"):
        return os.environ["UIC"]

    in_path = shutil.which("uic")
    if in_path:
        return in_path

    # Homebrew's Qt keeps uic in libexec, which is not on PATH.
    for qmake in ("qmake6", "qmake"):
        if not shutil.which(qmake):
            continue
        libexec = subprocess.run([qmake, "-query", "QT_HOST_LIBEXECS"],
                                 capture_output=True, text=True).stdout.strip()
        candidate = os.path.join(libexec, "uic")
        if libexec and os.path.exists(candidate):
            return candidate

    bundled = os.path.join("Externals", "Qt", "Qt6.8.3", "x64", "bin", "uic.exe")
    if os.path.exists(bundled):
        return bundled

    sys.exit("generate-ui-strings.py: uic not found. Set UIC to its path.")


def cpp_literal(text):
    """The literal uic would emit for this string, so it can be matched by text."""
    escaped = text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return '"{}"'.format(escaped)


def translator_comments(ui_path):
    """Maps each string literal uic will emit to its extracomment, if it has one."""
    comments = {}
    for element in ElementTree.parse(ui_path).iter("string"):
        note = element.get("extracomment")
        if note and element.text:
            comments[cpp_literal(element.text)] = note
    return comments


def annotate(header_path, comments):
    """Injects `// i18n:` above each translate() call whose string had one.

    uic emits no comment at all, so without this the 176 existing `// i18n:`
    notes would be lost the moment their strings move into a .ui file."""
    if not comments:
        return

    output = []
    for line in open(header_path, encoding="utf-8"):
        for literal, note in comments.items():
            if "translate(" in line and literal in line:
                indent = line[:len(line) - len(line.lstrip())]
                output.append("{}// i18n: {}\n".format(indent, note))
                break
        output.append(line)

    with open(header_path, "w", encoding="utf-8") as header:
        header.writelines(output)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit("usage: generate-ui-strings.py <output-dir> [source-dir]")

    output_root = sys.argv[1]
    source_root = sys.argv[2] if len(sys.argv) == 3 else "Source"
    # Always present, so the caller can list it even before any pane is migrated.
    os.makedirs(output_root, exist_ok=True)

    ui_files = []
    for directory, _, names in os.walk(source_root):
        ui_files += [os.path.join(directory, n) for n in names if n.endswith(".ui")]

    # No forms yet, so uic is not needed to run the script at all.
    if not ui_files:
        return

    uic = find_uic()
    for ui_path in sorted(ui_files):
        # The output tree mirrors the source tree: two forms can share a file name.
        header_dir = os.path.normpath(os.path.join(output_root, os.path.dirname(ui_path)))
        os.makedirs(header_dir, exist_ok=True)
        name = os.path.basename(ui_path)[: -len(".ui")]
        header_path = os.path.join(header_dir, "ui_{}.h".format(name))

        subprocess.run([uic, ui_path, "-o", header_path], check=True)
        annotate(header_path, translator_comments(ui_path))
        print(header_path)


if __name__ == "__main__":
    main()
```

In `Languages/update-source-strings.sh`, replace the extraction block

```bash
# Scan the source code for strings and put them in dolphin-emu.pot
SRCDIR=Source
find $SRCDIR -name '*.cpp' -o -name '*.h' -o -name '*.c' | sort -fd | \
	xgettext -p ./Languages/po -o dolphin-emu.pot --package-name="Dolphin Emulator" \
```

with

```bash
# Scan the source code for strings and put them in dolphin-emu.pot
SRCDIR=Source

# .ui files are XML, so xgettext cannot see them. Generate the headers uic would
# generate, restore the translator comments uic drops, and scan those too.
UI_SCRATCH=Languages/.ui-scratch
rm -rf "$UI_SCRATCH"
python3 ./Languages/generate-ui-strings.py "$UI_SCRATCH" $SRCDIR > /dev/null

{ find $SRCDIR -name '*.cpp' -o -name '*.h' -o -name '*.c'
  find "$UI_SCRATCH" -name 'ui_*.h'
} | sort -fd | \
	xgettext -p ./Languages/po -o dolphin-emu.pot --package-name="Dolphin Emulator" \
```

add the two keywords to that same `xgettext` invocation, immediately after `--keyword=FmtFormatT`:

```bash
	--keyword=translate:2 \
	--keyword=translate:2,3c \
```

and after the existing three `sed -i` lines, point the generated-header references back at the forms
they came from and drop the scratch tree:

```bash
# Point references at the .ui file rather than at the header uic generated from it.
sed -i "s|Languages/\.ui-scratch/\(.*\)/ui_\([^/:]*\)\.h:[0-9]*|\1/\2.ui|g" Languages/po/dolphin-emu.pot

rm -rf "$UI_SCRATCH"
```

Add to `.gitignore`, after the `.flatpak-builder` entry:

```
# Ignore the scratch tree update-source-strings.sh uses for .ui files
/Languages/.ui-scratch/
```

Note on `sed -i`: the three existing lines already use the GNU form with no backup suffix, so this
script is GNU-sed-only today. Keep the new line consistent rather than fixing that here; run the
script on Linux, or with `gsed` aliased, as before.

- [x] **Step 4: Run the test to verify it passes**

Run:
```bash
./Languages/tests/test-ui-extraction.sh
```
Expected: `PASS: .ui strings reach the .pot with context and translator comments`, exit 0. `uic` is
found via `qmake -query QT_HOST_LIBEXECS` on a Homebrew Qt without setting anything.

Then prove the script change is a no-op on today's tree, which has no `.ui` files — this is the
gate-2 baseline the later slices diff against:

```bash
./Languages/update-source-strings.sh
git diff --stat Languages/po/dolphin-emu.pot
git diff Languages/po/dolphin-emu.pot | grep '^[-+]msgid' | head
```
Expected: the only changed lines are `POT-Creation-Date` and the `#:` references that were already
stale (see the trap noted above); **no** `+msgid`/`-msgid` line attributable to the keyword change.
If msgid lines do appear, they are the 32/17 pre-existing drift — confirm by re-running the same
command on a `git stash`ed tree and comparing the two outputs, then commit the refreshed `.pot`
separately from this task.

- [x] **Step 5: Commit**

```bash
git add Languages/generate-ui-strings.py Languages/update-source-strings.sh \
        Languages/tests/UiStringFixture.ui Languages/tests/PainterFixture.cpp \
        Languages/tests/test-ui-extraction.sh .gitignore
git commit -m "Languages: extract translatable strings from .ui files

xgettext only reads *.cpp/*.h/*.c, so the first string moved into a Qt Designer
form would drop out of dolphin-emu.pot and out of all 29 locales without a word.

generate-ui-strings.py runs uic into a scratch tree and re-injects the // i18n:
notes uic discards, so translator comments survive the move. The script stays
buildless: it does not need a configured build tree, and it does not need uic at
all until the first .ui file exists.

Both --keyword=translate:2 and --keyword=translate:2,3c are required. The
context form alone extracted 1 of 3 fixture strings, because uic emits nullptr
for the disambiguation of almost every string; test-ui-extraction.sh pins that
so nobody 'simplifies' it back into silent data loss.

On today's tree, which has no .ui files, the regenerated .pot is byte-identical
apart from POT-Creation-Date.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Definition of done

Spec §7.4 defines four per-slice gates. Slice 0 migrates no pane, so gates 1 and 4 have nothing to
compare; what remains:

- [x] `tests` green, including the 14 new Qt-free cases:
      `./build/Binaries/Tests/tests` — note that `ctest` from the build root finds nothing here,
      there is a single `tests` binary.
- [x] `qt-tests` green, 86 cases: `./build/Binaries/Tests/qt-tests -platform offscreen`. The 67 this
      line originally carried was a planning-time estimate; the review rounds on Tasks 5 to 11 added
      cases, and 86 is the count measured at `e18369c905`. Read the number off the suite rather than
      trusting this line.
- [x] `qt-tests` green on Windows through the UAT host's interactive session, with no `-platform`
      argument — the bundled Qt has no offscreen plugin. This is the one gate that cannot be run
      from macOS. This run also settles the one residual that macOS cannot check: whether
      `dolphinqt-config-binder` re-enables RTTI for the `ModalMessageBox.cpp` it pulls in. See entry
      9 of the slice's residuals. Later slice verification repeatedly exercised this target; the
      final cleanup run passed 189/189 Windows Qt tests.
- [x] `dolphin-emu` builds on macOS and starts far enough to process `--help`. `Bind()` has no
      production caller yet, so nothing user-visible changed.
- [x] `dolphin-emu` builds and launches on Windows. The Graphics and Interface panes were reviewed
      during their dedicated migration slices, and the final cleanup run rebuilt the application.
- [x] `./Languages/tests/test-ui-extraction.sh` passes.
- [x] `.pot` regeneration produces no msgid change attributable to this slice (see Task 12,
      Step 4).
- [x] `ConfigControls/` is byte-identical to its state at the start of the slice.
- [x] Existing `ToolTipControls/` behavior is preserved. Task 9 deliberately moved its palette
      calculation into `ToolTipStyle` so both the old controls and the binder use one implementation;
      therefore the earlier byte-identical requirement was incompatible with the planned work.

What slice 0 deliberately does **not** prove: that a migrated pane looks like the old one. There is
no pane yet. Spec §7.5 records that risk; it becomes live in slice 5.
