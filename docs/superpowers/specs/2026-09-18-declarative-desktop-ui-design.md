# Declarative DolphinQt UI + PCSX2-Style Parity — Design

**Date:** 2026-09-18
**Status:** approved
**Reference implementation:** PCSX2 (`~/work/pcsx2`), primarily `pcsx2-qt/Settings/SettingWidgetBinder.h`,
`pcsx2-qt/MainWindow.ui`, `pcsx2-qt/GameList/GameListWidget.ui`, `pcsx2-qt/Settings/SettingsWindow.ui`

This document supersedes an earlier untracked draft of the same filename, which proposed a
shells-only migration split into four projects and described its direction as approved without
that approval having been given. The parts of it that survive are noted in §11.

## 1. Goal

Move **all** DolphinQt layout authoring into Qt Designer `.ui` files, so the application has one
UI-building system rather than two, and adopt PCSX2's desktop information architecture where it is
better than Dolphin's — style, main-window shell, game library, settings shell, and a selected set
of PCSX2-only dialogs.

Two explicit exclusions, both decided rather than deferred:

- **Controller mapping gains no parity features.** PCSX2's mapping UI is built around DualShock2,
  Jogcon, Negcon, Pop'n and Guitar pad diagrams, none of which apply to GameCube controllers,
  Wiimotes, Nunchuks, Classic Controllers or GBA links. Adopting its model would also cost
  `MappingIndicator`'s live visualisation and `IOWindow`'s expression editor, both of which are
  richer than PCSX2's equivalents. Mapping is migrated to `.ui` and otherwise left alone.
- **OSD / Big Picture is a separate project.** Scoped in
  `2026-09-18-fullscreen-osd-followup-design.md`, not implemented here.

Emulation behaviour does not change.

## 2. Approved direction

Layout and static widget properties live in `.ui`. Config binding happens **after** construction,
against stock Qt widgets, through a new `ConfigWidgetBinder`. This is PCSX2's model.

The alternatives were considered and rejected:

- **Two-phase `ConfigControls` promoted via `<customwidget>` XML** would be the smallest diff, but
  the bundled Windows Qt at `Externals/Qt/Qt6.8.3/x64/lib/cmake` contains no `Qt6UiPlugin`,
  `Qt6Designer` or `Qt6UiTools`, so no Designer custom-widget plugin can be built. Every promoted
  control would render as an unrenderable placeholder, leaving `.ui` files editable only as text.
  That is one authoring system in name and a worse one in practice.
- **Declarative binding via dynamic properties** (`configKey="GFX/…"` in the XML) would minimise
  C++ boilerplate but replaces Dolphin's compile-time-typed `Config::Info<T>` with stringly-typed
  runtime lookup, turning a mistyped key from a build error into a silent no-op.

Binding after construction is also the only option under which the `.ui` files open correctly in
Qt Designer, which is the point of the exercise.

The migration targets Dolphin's existing Qt 6.8.3 baseline. It does not require Qt 6.10.

## 3. Architecture: `ConfigWidgetBinder`

### 3.1 What it replaces

Two stacked widget-subclass hierarchies, both subclass-only by construction and therefore both
incompatible with `uic` output:

- `Config/ConfigControls/` — 1,112 LOC, 16 classes. `ConfigControl<Derived>` takes
  `Config::Location` and `Config::Layer*` as **constructor arguments** and calls `ConnectConfig()`
  in the constructor, so `uic`'s `new ConfigBool(parent)` cannot compile.
- `Config/ToolTipControls/` — 744 LOC, 7 widget classes plus `BalloonTip`. `ToolTipWidget<Derived>`
  overrides `enterEvent`, `leaveEvent`, `hideEvent` and `timerEvent`, and declares a pure virtual
  `GetToolTipPosition()` that each subclass answers differently.

`ConfigBool` is `ConfigControl<ToolTipCheckBox>`, and `ToolTipCheckBox` is
`ToolTipWidget<QCheckBox>`. Both layers must be re-homed for stock widgets to appear in `.ui`.

### 3.2 Design

`Bind()` attaches a `ConfigBinding : QObject` as a **child of the widget**. The child holds the
per-widget state the CRTP base holds today — location, layer, the `m_updating` re-entrancy guard,
and the description text — connects to `Settings::ConfigChanged`, and installs itself as the
widget's event filter. QObject parenting gives it exactly the widget's lifetime, so call sites do
no ownership bookkeeping.

```cpp
// DolphinQt/Config/ConfigWidgetBinder.h
// layer is null for global panes, non-null for per-game panes.
// Config::Info<T> already carries key, type and default, so no key/default arguments are needed.
namespace ConfigWidget
{
// Integer ranges (minimum, maximum, step, tick interval) come from the .ui file, so no bind takes
// them. BindFloat is the exception: a float range cannot be expressed on a Designer slider.
void Bind(QCheckBox*, const Config::Info<bool>&, Config::Layer* = nullptr, bool reverse = false);
void Bind(QComboBox*, const Config::Info<int>&, Config::Layer* = nullptr);
void Bind(QComboBox*, const Config::Info<u32>&, Config::Layer* = nullptr);
void Bind(QSpinBox*, const Config::Info<int>&, Config::Layer* = nullptr);
void Bind(QSlider*, const Config::Info<int>&, Config::Layer* = nullptr);
void Bind(QRadioButton*, const Config::Info<int>&, int value, Config::Layer* = nullptr);
void Bind(QLineEdit*, const Config::Info<std::string>&, Config::Layer* = nullptr);

// Preferred: the items are authored in the .ui file, so their text stays in the uic translation
// path; `values` pairs with them by index.
template <typename T>
void BindMapped(QComboBox*, const Config::Info<T>&, std::span<const T> values,
                Config::Layer* = nullptr);
// For option sets computed at runtime — the JIT cores available on this platform, the SD card
// sizes — which cannot be authored in Designer. Populates the combo itself.
template <typename T>
void BindMapped(QComboBox*, const Config::Info<T>&, std::span<const std::pair<QString, T>> options,
                Config::Layer* = nullptr);
// Today's ConfigStringChoice, whose two forms are options-are-data and display-text-plus-data.
void BindStringChoice(QComboBox*, const Config::Info<std::string>&,
                      std::span<const std::string> options, Config::Layer* = nullptr);
void BindStringChoice(QComboBox*, const Config::Info<std::string>&,
                      std::span<const std::pair<QString, QString>> options,
                      Config::Layer* = nullptr);
// Today's tick-value ConfigSlider: position i means tick_values[i]. Disables the slider when the
// config value matches no tick, as the existing control does.
void BindMapped(QSlider*, const Config::Info<int>&, std::span<const int> tick_values,
                Config::Layer* = nullptr);
// Today's ConfigSliderU32: stored value is the slider position times `scale`.
void BindScaled(QSlider*, const Config::Info<u32>&, u32 scale, Config::Layer* = nullptr);
// Today's ConfigFloatSlider. Returns a handle because four call sites read the mapped float back
// (ConfigFloatSlider::GetValue) to drive a value label; the handle avoids a downcast, and DolphinQt
// uses qobject_cast exclusively, which cannot reach a non-Q_OBJECT binding subclass.
struct FloatSliderHandle
{
  QSlider* slider;
  FloatSliderRange range;
  float Value() const;
};
FloatSliderHandle BindFloat(QSlider*, const Config::Info<float>&, float min, float max, float step,
                            Config::Layer* = nullptr);
// Replaces the hand-written slider-to-label updates in GameConfigWidget and EnhancementsWidget.
void MirrorFloatValue(QLabel*, FloatSliderHandle, const QString& format);
void BindUserPath(QLineEdit*, unsigned dir_index, const Config::Info<std::string>&,
                  Config::Layer* = nullptr);

// Today's ConfigSliderLabel / ConfigIntegerLabel / ConfigFloatLabel. All three mirror the
// control's *font*, not its value, so a label goes bold beside a control a game INI overrides.
// One overload replaces all three, because none of them differ.
void MirrorFont(QLabel*, QWidget* control);

// Today's ConfigComplexChoice: two settings driving one combo. Callers must add options after
// binding, so this returns a builder rather than binding in one call.
using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>;
ComplexBinding* BindComplex(QComboBox*, const InfoVariant& setting1, const InfoVariant& setting2,
                            Config::Layer* = nullptr);
// ComplexBinding exposes Add(name, option1, option2), SetDefault(index), Refresh() and Reset(),
// matching the existing class.

void SetDescription(QWidget*, QString title, QString description);
}
```

### 3.3 Behaviours that must be preserved, not dropped

Three behaviours currently implemented as virtual overrides move into the event filter. They are
the highest-risk part of the binder because the *mechanism* changes, not just the call site:

- **Bold-when-overridden.** On `ConfigChanged`, re-apply the bold font, as
  `ConfigControl::ConnectConfig` does today — and additionally at bind time. This is the binder's
  one intentional behavioural divergence: `ConnectConfig` only ever applies the font from its
  `ConfigChanged` handler, so a per-game pane that is opened and not touched shows no bold at all.
- **Right-click clears the per-game key.** Today an override of `mousePressEvent`; becomes a
  `QEvent::MouseButtonPress` case in the filter. Only active when a layer is present.
- **Balloon tooltips.** `ToolTipWidget`'s four event overrides become filter cases. Its one
  genuinely per-type piece, `GetToolTipPosition()`, becomes a small type switch inside the binder
  (roughly 40 LOC for the 7 widget types).

Two smaller behaviours are easy to lose because they live in classes whose names suggest otherwise:

- **Label fonts follow their control.** `ConfigSliderLabel`, `ConfigIntegerLabel` and
  `ConfigFloatLabel` exist only to copy the control's font on `ConfigChanged`, so the label goes
  bold with it. `MirrorFont()` replaces all three.
- **A tick-value slider disables itself** when the config value matches none of its ticks, and
  re-enables when it does. That is the existing control's way of showing an out-of-range value
  rather than silently snapping it.

Description **text** stays in the binder while its **presentation** is a policy. This is
deliberate: balloon behaviour survives each behaviour-preserving migration commit unchanged, and
PCSX2's persistent help pane can be added later as a second presentation without revisiting the
call sites.

### 3.4 Setting registry

`Bind()` records what it is already given — config location, type, range or choice list, layer,
and the label and help text from `SetDescription()` — into a registry keyed by `Config::Location`.

This adds no API surface callers must think about and no abstraction that is not already
implied by the arguments. It exists for one reason: 194 bind sites will be written across 27
slices, and adding the recording afterwards means revisiting all of them. It is the enabling
input for the OSD follow-up's settings browser (§13), which is otherwise a from-scratch
re-authoring of every setting.

### 3.5 Scale

Roughly 1,400 LOC of new binder replacing 1,856 LOC of subclasses; 194 construction sites and
187 `SetDescription` calls rewritten. Net LOC approximately flat. The gain is that every `.ui`
file contains only stock widgets and opens correctly in Designer.

## 4. The migration rule

**Static structure and layout go in `.ui`. Dynamic content stays in C++.**

`MenuBar.cpp` is the test case: 2,011 LOC, 155 action creations, 24 conditional or
rebuilt-at-runtime sites. The 155 actions move into `MainWindow.ui` — PCSX2 declares 99 the same
way — while the 24 dynamic sites (recent files, symbol maps, backend-dependent lists) keep
populating in C++.

Custom-painted views are not a layout system and are not affected: `CodeViewWidget`,
`MemoryViewWidget` and `MappingIndicator` keep their `paintEvent` implementations in C++ while
their containers move to `.ui`.

After the migration, no C++ file in DolphinQt positions a widget. A CI-style guard rejecting new
`new Q*Layout` in DolphinQt lands in the final slice.

**Always-buildable invariant:** `ConfigControls/` and `ToolTipControls/` stay in the tree,
untouched and compiling, until the last consumer is migrated; they are deleted in slice 26. At no
point are both systems being *authored* — only one is being removed.

## 5. Slice sequence

Each slice is independently shippable and ends with a working build. Slices with a parity feature
get **two commits**: a behaviour-preserving `.ui` migration, then the feature. Migration-only
slices get one.

| # | Slice | Migration commit | Parity commit |
|---|---|---|---|
| 0 | Foundation | AUTOUIC, `ConfigWidgetBinder` + registry + `qt-tests`, `ui_*.h` gettext step | — no visible change |
| 1 | Main window shell | `MainWindow.ui`: menus, toolbar, central stack, status bar | Real status bar: renderer, resolution, FPS/VPS, speed, volume |
| 2 | Style | — | Palette-driven theme set; reconcile with `dark.qss` |
| 3 | Game list | `GameListWidget.ui` | Always-visible search, platform/region filters, grid-scale slider |
| 4 | Settings shell | `SettingsWindow.ui`: sidebar, stack, footer | Persistent help pane + sidebar icons |
| 5–15 | Each settings pane: General, Graphics, Controllers, Interface, On-Screen Display, Audio, Paths, GameCube, Wii, Triforce, Advanced | one `.ui` per pane, `Bind()` at each site | per pane, where PCSX2 has something to take |
| 16 | Per-game properties | reuse global pane `.ui` with a `Config::Layer` | "Use Global Setting [X]" override rows |
| 17 | Setup wizard | — | new, PCSX2-only |
| 18 | Cover management | — | new, PCSX2-only |
| 19–22 | Standalone dialogs: Achievements, NetPlay, FIFO, and the remaining root-level dialogs | grouped `.ui` migrations | — migration-only |
| 23 | Controller mapping (27 files) | `.ui` migration | — **no parity features**, per §1 |
| 24 | TAS (4 files) | containers to `.ui`; generated input grids stay C++ | — migration-only |
| 25 | Debugger (15 files) | containers to `.ui`; `paintEvent` views stay C++ | — migration-only |
| 26 | Cleanup | delete `ConfigControls/` + `ToolTipControls/`; add the `new Q*Layout` guard | — |

**Style is sequenced early, at slice 2, deliberately.** It is independent of `.ui` and could go
anywhere, but every pane slice ends in a screenshot review; reviewing eleven panes against a
palette that later changes means reviewing them twice.

**Slices 23–25 are the cost of "complete."** Controller mapping, TAS and the debugger are 46 of
the 129 layout files and carry no parity value. They exist in this plan because the requirement is
one UI system rather than two. They are also the lowest-risk slices — migration-only means the
oracle is "looks and behaves identically" — and therefore the natural candidates if scope is cut
later.

**Estimate:** 27 slices at roughly 2–4 working days each, depending on the slice's size and on
visual-gate turnaround; about 3–5 months overall, with slices 0–4 carrying most of the visible
parity and landing in the first three to four weeks. Human visual-gate cadence, not code
generation, is the schedule driver.

**Plan decomposition.** This design is deliberately larger than one implementation plan should be.
The first plan covers **slices 0–4** — the foundation plus the four surfaces that carry the visible
parity. Later slice groups get their own plans, written against this design once the binder and the
per-slice gates have been exercised in practice.

## 6. Translation pipeline

`Languages/update-source-strings.sh` runs `xgettext` over `*.cpp/*.h/*.c` under `Source`, so `.ui`
XML is invisible to it. Migrating strings into XML without addressing this silently removes them
from the 3,139-entry `.pot` and from 29 `.po` locales.

`uic` emits each `.ui` string as `QCoreApplication::translate("ClassName", "msgid", disambiguation)`.

**Keyword spec — use both forms:**

```
--keyword=translate:2 --keyword=translate:2,3c
```

`translate:2,3c` **alone is a silent data-loss trap**: it extracts only strings whose third
argument is a string literal, dropping every string where `uic` emitted `nullptr` — which is
almost all of them. Measured on a probe form, it extracted 1 of 3 strings. Both specs together
extracted all 3, with the disambiguated string correctly carrying its `msgctxt`, and no
duplicates. Only 12 of the current 3,139 entries use `msgctxt`, but handling it correctly is free.

**Existing translations survive.** `uic` places the bare source string in argument 2,
byte-identical to what `tr()` produced, so a string moving from C++ into `.ui` keeps its msgid and
all 29 locales keep their translations. No re-translation, no translator round-trip. Only the `#:`
line references churn.

**Translator comments need tooling.** `uic` discards `extracomment` — it emits no comment at all.
DolphinQt has 176 `// i18n:` notes today, on exactly the strings that needed help. The
string-update script gains a post-process (~30 LOC) that reads each `.ui`'s `extracomment` and
injects `// i18n: …` above the matching `translate()` line in the generated header. Verified
working end to end: the note appears as `#. i18n: …` in the extracted output.

**Where the headers come from.** Generated `ui_*.h` are build artifacts. Rather than making the
script depend on a configured build tree, it runs `uic` itself into a scratch directory,
annotates, and appends those files to the existing `find`. The script stays self-contained and
buildless, as it is today.

**Why not Qt Linguist `.ts`.** `Externals/Qt/Qt6.8.3/x64/bin/` ships `uic.exe` but not `lupdate`
or `lrelease`. Homebrew's Qt on macOS has them, so a `.ts` pipeline would build on one platform
and not the other. Staying on gettext keeps 3,139 strings, 29 locales and the Transifex
integration untouched.

## 7. Testing

### 7.1 The `qt-tests` target

`ConfigWidgetBinder` lives in its own small library target that both `dolphin-emu` and a new
`qt-tests` binary link. `qt-tests` drives a `QApplication` using `Qt6::Widgets` and gtest.

`Qt6Test` is **not** used: it is absent from the bundled Windows Qt. The only capability lost is
`QSignalSpy`, replaced by a ~15-line lambda-based signal recorder. The bundled Windows Qt also has
no offscreen platform plugin — `plugins/platforms/` contains only `qwindows.dll` and
`qdirect2d.dll` — so macOS runs with `-platform offscreen` while Windows runs on the existing UAT
host's interactive session.

Two test families:

- **Binder round-trip**, per `Config::Info<T>` type: set config, assert widget; change widget,
  assert config; repeat against a per-game `Config::Layer`. This covers the one migration defect
  that is both likely across 194 sites and invisible in a screenshot review — a control that
  renders perfectly and silently stops writing config.
- **Override semantics**: bold-when-overridden, and right-click-clears-key. These change mechanism
  in §3.3, so they are where a regression is most expected.

The existing Qt-free `tests` binary is unchanged; nothing is added to it.

`dolphin-emu` is an `add_executable`, not a library, so nothing else in DolphinQt is linkable from
a test binary. Splitting it into an object library plus a thin `main` — which would enable
per-form `setupUi` smoke tests — is **deliberately deferred**. It adds build risk to the
foundation slice, and whether dropped-widget mistakes actually happen is better learned from a few
shipped slices than assumed.

### 7.2 Pure logic under TDD

The binder's decision logic has no Qt in it and is extracted as a header-only pure unit, tested
failing-first: given a config value and layer state, what value and bold-state should the widget
show; given a widget value, what is written and to which layer. Coverage includes every
`Config::Info<T>` type, the layer-present and layer-absent cases, the `reverse` bool case,
float-slider quantisation, and the `m_updating` re-entrancy guard that exists today to stop a
save-loop.

### 7.3 What TDD is skipped for, and why

The `.ui` XML and the layout rewiring. They are mechanical build inputs with no independent
runtime behaviour; their oracle is `uic` plus the full `dolphin-emu` build. This skip is stated
rather than left silent.

### 7.4 Per-slice gates

Every slice's definition of done includes:

1. **Binding-coverage diff.** Record the set of `Config::Info` keys a pane binds before migration;
   record what the `.ui` plus `Bind()` calls produce after. A dropped or mistyped binding fails
   the gate.
2. **`.pot` msgid diff.** Regenerate and compare. Any msgid removed without an intentional string
   deletion fails the gate. This catches the §6 `nullptr` trap mechanically.
3. **`qt-tests` and `tests` both green.**
4. **Visual review** on macOS locally and Windows via the UAT host, in light and dark palettes:
   keyboard traversal and tab order, focus indicators, checked-state visible through the native
   style, accessible names on icon-only controls, narrow-window compression, window-state restore.

This fork has **no CI** — `.github/` contains only `modernize`, and there are no workflow files.
Every gate is run by whoever implements the slice. The gates are only as good as the checklist
that invokes them.

### 7.5 Residual risk, stated plainly

There is no automated proof that a migrated pane *looks* like the old one. Migration commits are
behaviour-preserving by intent, verified by human comparison. The mitigations are that each slice
is small enough to eyeball against the previous build, and each is independently revertible.

## 8. Visual and accessibility rules

Native Qt metrics and the active Dolphin palette. No fixed colours, custom fonts, animated
decoration, or web-derived styling.

- Every icon-only control has a tooltip and an accessible name.
- Tab order follows the visible left-to-right control order.
- Checked list/grid state is visible through the native style, not by colour alone.
- Keyboard focus indicators are preserved.
- The empty library keeps a direct action for adding a game directory.
- Narrow windows compress combo boxes and search before clipping view buttons.

## 9. Measured inventory

Established by direct measurement, not estimated:

| Quantity | Value |
|---|---|
| DolphinQt files / LOC | 410 / 67,641 |
| Files building layouts in C++ (migration targets) | 129 |
| `ConfigControl` construction sites | 194 |
| `SetDescription` calls | 187 |
| `SetTitle` calls | 40 |
| `QDialog`/`QWidget`/`QMainWindow` subclasses | 99 |
| `// i18n:` translator notes | 176 |
| `.pot` msgids / `msgctxt` entries / `.po` locales | 3,139 / 12 / 29 |
| `ConfigControls/` + `ToolTipControls/` LOC | 1,112 + 744 |
| `MenuBar.cpp` LOC / action creations / dynamic sites | 2,011 / 155 / 24 |

Layout-building files by area, summing to 129: `Config/Mapping` 27, `Config` 23, `Debugger` 15,
`Settings` 11, `NetPlay` 7, `Config/Graphics` 7, `Achievements` 6, `TAS` 4,
`Config/ControllerInterface` 3, 15 root-level single-file dialogs, 9 across `SkylanderPortal`,
`QtUtils`, `FIFO`, `EmulatedUSB` and `InfinityBase`, and 2 in `Config/ToolTipControls` and
`Config/SDLHints`.

## 10. Dependency position

Assumed gaps that do not exist: Qt Svg and an SVG-first `Resources.cpp` loader are already
present; Dolphin's config layering is richer than PCSX2's (7 layers vs 5); its input abstraction is
far richer (23 backends, expression-based `ControlReference`); the settings sidebar IA already
matches PCSX2's list-plus-stack; and Dolphin is CMake-only, which is an advantage.

Real constraints: `Externals/Qt` is a curated subset — a submodule of upstream
`dolphin-emu/ext-win-qt`, 410 MB, providing Core, Gui, Widgets, Svg, SvgWidgets and the Core/Gui/
Widgets tools, and **not** LinguistTools, Qt Concurrent, Qt Test, the offscreen platform plugin,
or any Designer/UiPlugin/UiTools package. Nothing in this design requires forking it.

## 11. Licensing and reference boundaries

PCSX2 is `GPL-3.0-or-later`; Dolphin is `GPL-2.0-or-later`. Copying PCSX2 source into this tree
would work in aggregate but would make the fork effectively GPLv3 and block upstreaming. Therefore
PCSX2 provides **behavioural and layout reference only**. New `.ui`, C++, strings and resources
carry Dolphin copyright headers and SPDX identifiers. No PCSX2 icon, stylesheet, XML or source
block is copied.

The same ordinary Qt widget types and general arrangement may be used, because those are interface
concepts; names, implementation structure and assets remain Dolphin-owned.

This boundary, the `MainWindow.ui` / `GameListWidget.ui` composition detail, and the header-only
testable-helper pattern are the parts carried forward from the superseded draft.

## 12. Success criteria

1. `dolphin-emu` builds with `AUTOUIC` on macOS and Windows using the existing Qt packages.
2. No C++ file in DolphinQt constructs a layout; the slice-26 guard enforces this.
3. `ConfigControls/` and `ToolTipControls/` are deleted.
4. All 194 config bindings round-trip in both the global and per-game layer cases, proven by
   `qt-tests`.
5. Bold-when-overridden and right-click-clears-key behave as they do today.
6. The `.pot` contains no fewer msgids than before the migration, excluding intentional deletions;
   all 29 locales retain their translations.
7. Existing menus, toolbar actions, debugger docks, drag-and-drop, state restoration, batch mode,
   game launching and render-window modes retain their behaviour.
8. Search is always visible and `Ctrl+F` focuses it; list/grid buttons stay synchronised with the
   View menu; the scale slider stays synchronised with the zoom shortcuts.
9. The main window has a real status bar reporting renderer, resolution, FPS/VPS, speed and volume.
10. Focus order, accessible names, tooltips and native focus indicators work in light and dark
    palettes on both platforms.

## 13. Out of scope

- OSD / Big Picture — separate spec, `2026-09-18-fullscreen-osd-followup-design.md`.
- Controller-mapping parity features (§1).
- Qt 6.10 upgrade; forking `ext-win-qt`.
- Migrating to Qt Linguist `.ts`.
- Splitting `dolphin-emu` into a library (§7.1); revisit after several slices ship.
- Importing PCSX2 icons or themes.
- Custom animated game-list backgrounds.
- Runtime graphics-backend switching; debugger docking changes.
- The Android UI.
