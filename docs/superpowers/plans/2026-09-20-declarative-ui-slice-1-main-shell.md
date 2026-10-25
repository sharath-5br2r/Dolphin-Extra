# Declarative UI Slice 1: Main Shell

**Status:** Complete

**Goal:** Put the first production DolphinQt layout under AUTOUIC and add the PCSX2-style
emulation status area without changing menu, toolbar, debugger, render, or game-launch behavior.

This plan separates the first production form and status behavior from the later shell parity
work. The split keeps the 2,000-line `MenuBar` action graph out of the same review as the first
production form.

## Task 1: Testable emulation status widget

- [x] Add a Qt test that supplies an idle and a running status snapshot.
- [x] Implement a status widget showing renderer, resolution, FPS, VPS, speed, and volume.
- [x] Keep all formatting and visibility behavior inside the widget.
- [x] Run `qt-tests`.

## Task 2: Expose the latest emulated framebuffer size

- [x] Add a getter beside `PerformanceMetrics::SetLatestFrameBufferSize`.
- [x] Add a unit test proving the getter returns the latest complete width/height pair.
- [x] Run the focused unit test.

## Task 3: Introduce `MainWindow.ui`

- [x] Author the central `QStackedWidget`, game-list page layout, and `QStatusBar` in Designer XML.
- [x] Run `setupUi()` before constructing dynamic application components.
- [x] Insert `GameList` and `SearchBar` into the authored game-list layout.
- [x] Keep the existing `MenuBar` and `ToolBar` classes attached by their existing connection code.
- [x] Build `dolphin-emu` and run the noninteractive launch check.

## Task 4: Move game count and live emulation state to the real status bar

- [x] Preserve the existing game-count text and visibility preference while emulation is stopped.
- [x] Show live metrics while emulation is running or paused.
- [x] Refresh metrics on a GUI-thread timer using thread-safe performance getters.
- [x] Hide unavailable fields instead of displaying stale values.
- [x] Run the full unit and Qt test binaries.

## Task 5: Flatten the remaining shell structure

- [x] Move static toolbar actions into `MainWindow.ui` and convert `ToolBar` to a controller.
- [x] Move static menus and actions into `MainWindow.ui` and convert `MenuBar` to a controller.
- [x] Leave runtime-built menus and conditional debugger sections in C++.
- [x] Verify saved toolbar and dock state compatibility.

## Definition of done

- [x] `MainWindow.ui` is compiled by AUTOUIC and used by production.
- [x] The main status bar reports renderer, resolution, FPS, VPS, speed, and volume during
      emulation.
- [x] The stopped-state game-count preference still works.
- [x] Existing menu, toolbar, render-stack, and debugger behavior is preserved.
- [x] `tests`, `qt-tests`, `.ui` extraction, and `dolphin-emu` are green on macOS.
- [x] Windows build and interactive visual verification are complete.
