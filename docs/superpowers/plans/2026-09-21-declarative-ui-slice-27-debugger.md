# Declarative UI Slice 27: Debugger

**Status:** Complete

**Goal:** Move the permanent layouts and containers of DolphinQt's debugger tools to Qt Designer
forms without changing debugger state, models, disassembly, memory access, breakpoint behavior,
custom views, toolbar actions, or settings persistence.

This slice covers the 15 debugger controllers that still construct layouts in C++:
`AssembleInstructionDialog`, `AssemblerWidget`, `BranchWatchDialog`, `BreakpointDialog`,
`BreakpointWidget`, `CodeWidget`, `EditSymbolDialog`, `JITWidget`, `MemoryViewWidget`,
`MemoryWidget`, `NetworkWidget`, `PatchInstructionDialog`, `RegisterWidget`, `ThreadWidget`, and
`WatchWidget`.

## Task 1: Preserve the runtime boundary

- [x] Keep debugger models, table contents, proxy filtering, selection, and context menus in C++.
- [x] Keep assembly, disassembly, symbol, memory, breakpoint, watch, thread, network, and JIT
      operations in C++.
- [x] Keep custom-painted code and memory views, runtime toolbars and actions, generated rows, and
      settings persistence in C++.
- [x] Move permanent dialog and dock shells, group boxes, splitters, tabs, labels, button boxes,
      scroll areas, and placement layouts to `.ui` files.

## Task 2: Migrate compact dialogs and docks

- [x] Add forms for the assemble, patch-instruction, edit-symbol, and breakpoint dialogs.
- [x] Add forms for the breakpoint, register, and watch dock widgets.
- [x] Add a form for `MemoryViewWidget` while retaining its runtime table model/delegate and custom
      memory behavior.
- [x] Preserve button roles, tab order, table properties, toolbar behavior, and minimum sizing.

## Task 3: Migrate compound debugger surfaces

- [x] Add forms for the assembler, code, JIT, memory, network, thread, and branch-watch tools.
- [x] Retain named insertion layouts for runtime editors, custom views, toolbar groups, generated
      controls, tables, and status widgets.
- [x] Preserve splitter orientation and stretch behavior, group order, scroll behavior, filters,
      disabled states, and visibility decisions.
- [x] Remove C++ construction of every permanent layout covered by the slice.

## Task 4: Cover and verify

- [x] Add Qt form tests for hierarchy, orientation, insertion targets, defaults, buddy links,
      button roles, table properties, and focus order.
- [x] Run XML, formatting, translation extraction, targeted catalog parity, and a debugger
      `new Q*Layout` boundary check.
- [x] Build the application and both test binaries and run both suites on macOS and Windows.
- [x] Review representative compact and compound debugger forms with native Qt rendering on both
      platforms.
- [x] Remove temporary scripts, captures, harnesses, catalogs, and transferred changes.

## Definition of done

- [x] All 15 debugger controllers use forms for permanent shells and layout containers.
- [x] Runtime debugger logic, custom views, generated content, strings, shortcuts, and side effects
      remain intact.
- [x] No debugger C++ file constructs a Qt layout.
- [x] macOS and Windows builds, tests, extraction, and visual review pass.

## Verification

- Implementation commits: `f4759e8eec`, `f85ba27f66`, and `aa964affbf`.
- All 15 forms pass `xmllint` and compile with `uic`; `clang-format` and `git diff --check`
  pass.
- Translation extraction passes. A targeted before/after debugger catalog comparison preserves
  all 431 messages and normalized translator comments.
- The debugger source boundary check finds no construction of `QVBoxLayout`, `QHBoxLayout`,
  `QGridLayout`, or `QFormLayout` in C++.
- macOS builds the application and Qt/core test targets. Qt tests pass 190/190; core tests pass
  1,185/1,187 with the two environment-dependent shader preset tests skipped.
- Windows builds the application and Qt/core test targets with MSVC 19.51 and Qt 6.8.3. Qt tests
  pass 190/190; core tests pass 1,507/1,509 with the same two shader preset tests skipped.
- Native macOS and Windows review covers the assembler, code, branch watch, threads, memory,
  network, and JIT surfaces, including narrow dock geometry.
- The Windows worktree was restored to baseline `21ba25ee0e596a62b5709e5347890b14becc8739`;
  temporary profiles, scripts, catalogs, patches, logs, and captures were removed from both hosts.
