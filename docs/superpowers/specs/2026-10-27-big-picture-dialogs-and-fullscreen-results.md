# Big Picture dialogs and fullscreen — 2026-10-27

Exit chooser and confirmation prompts now share a dedicated dialog renderer. The panel uses
600 reference pixels of width, a 44-pixel title bar, content-dependent height, 22/14-pixel
label/help typography, the active theme palette, neutral rounded focus outlines, a mouse
close control and fixed footer hints. Long content scrolls within the panel. Geometry respects
interface scale and safe area. Exit dialogs show a non-interactive home view behind the panel.
About and error dialogs use the same panel style. Confirmation requests retain Cancel as the
initial selection; a new request resets that safe default, while resizing retains selection.

F11 is handled by the public NavigationToolkit entry point shared by every presentation.
OnScreenUI dispatches the resulting presentation request without also activating a selected
row. The controller permits fullscreen while confirmations or errors are visible and executes
it without replacing the screen stack, clearing the error, or consuming the confirmation token.
Fullscreen remains blocked during pending host operations and boot/stop transitions. The Qt
callback supports both the idle standalone renderer and an active game's render widget. The
controller page-next binding still toggles fullscreen only on Home, preserving its navigation
meaning elsewhere.

Validation:

- Release Dolphin, the production-layout visual fixture and regression targets build.
- Full suites: 1,700 core tests passed with two optional external-preset tests skipped;
  266 Qt tests passed after relinking against the current controller.
- Shared-renderer tests exercise F11 on Home, Start Game, generic lists, pause, game library,
  file pickers, settings and dialogs. Other checks verify compact geometry, safe Cancel,
  retained selection on resize, reset on a new confirmation, large scale/safe area, preserved
  approval/error state, and transition restrictions.
- An isolated D3D12 native session toggled fullscreen in Start Game, the file picker, game
  library, settings, graphics-backend details, exit chooser and exit confirmation. It verified
  that the exit token survived, cancelled safely, then explicitly confirmed exit with a clean
  process exit. No game was booted.
- 53 visual captures cover exit, confirmation and translated/wrapped confirmation content at
  720p, 1080p, 1634 x 977, 4K and ultrawide in Dark, Light, AMOLED and additional themes.
- Temporary native hooks and visual fixtures remain under ignored build/visual-refinement.
  They are excluded from the normal Dolphin.exe testing build.

Actual fullscreen switching during game execution was not exercised by the native probe;
active-mode dispatch is covered by the controller tests and uses the existing Qt fullscreen slot.
