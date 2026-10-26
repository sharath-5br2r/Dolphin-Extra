# Big Picture home layout — 2026-10-26

Implemented the four-action initial screen from the supplied PCSX2 screenshot.
Reference: E:/work/pcsx2, revision adca25d5995fbec3dbabbc061518d332a5ea935b.
The renderer and routing are independently written. Artwork/font provenance and licenses
are in Data/Sys/Resources/BigPicture/README.md.

## Result

Idle entry now opens Home: Game List, Start Game, Settings and Exit. The screen uses
full-width header/footer bars, Dolphin branding, a local clock, the same four SVG icons,
Roboto text and PromptFont keycaps as the reference. Action areas measure 250 x 320
reference pixels, with 30-pixel spacing/padding and 150-pixel icon boxes. Hover and
navigation focus highlight the selected action; other areas stay unfilled. Scaling and
overscan fit the complete row within the usable display.

Game List opens the existing scanned library. Start Game offers the controller file picker,
installed GameCube IPLs and Wii System Menu, subject to existing firmware/core checks.
Back returns to Home and restores its focus. Exit/Escape open desktop/application exit
choices; application exit keeps its confirmation. Desktop handoff drains the graphics owner,
and reentry opens Home. F1 opens About, F11 toggles fullscreen, and Space opens Game List.

When RetroAchievements is enabled and signed in, its public display name and available
avatar appear beside the clock. Public profile data is copied under the achievement manager
lock into immutable, bounded snapshots. GPU uploads stay on the graphics owner. Long names
are clipped before the clock, and absent profiles have no placeholder username. Translations,
user theme selection and controller navigation remain supported.

## Verification

- Release Dolphin, core/Qt tests and GPU visual fixture build successfully.
- Core tests: 1,666 passed; two optional external-preset tests skipped.
- Qt tests: 259 passed. Translation extraction passed.
- Added Home/child routing, About/back, file-picker nesting, console availability, exit
  confirmation/cancellation, horizontal keyboard/controller navigation, focus restoration
  and overscan coverage. Updated former library-root expectations.
- Native integration passed on D3D12, Vulkan, D3D11 and OpenGL: cold Home presentation,
  game list, controller file picker, settings, exit cancellation, desktop handoff/graphics
  drain, reentry and application exit. Isolated profiles were used.
- GPU fixture captures cover Dark/Light/AMOLED at 1280x720, 1920x1080, 3840x2160 and
  3440x1440, plus maximum overscan/scale, a mock signed-in account and long-name clipping.
  Default Dark, stressed Light, account and long-name fixtures were visually reviewed.
- Temporary native instrumentation was removed from the ordinary Dolphin executable;
  its generated build rules contain neither NativeHomeProbe nor NativeThemeProbe.

Ignored evidence is under build/visual-refinement: home-final-core.log, home-final-qt.log,
home-native-{D3D12,Vulkan,D3D,OGL}/home-native.log, home-captures/, home-stress-captures/,
home-account-captures/ and home-account-long-captures/.

The account captures use synthetic profile data. A live RetroAchievements login/avatar
refresh was not exercised; the existing public profile APIs and decoded badge are reused.
No emulator-core behavior was changed.

## User testing follow-up: artwork filtering and input switching

Addressed the three issues reported after the initial-screen build:

- The native ImGui renderer used a point sampler, unlike the visual fixture and PCSX2.
  It now uses linear/trilinear sampling. Home icons upload a complete alpha-coverage mip
  chain, with white transparent texels to avoid dark fringes when filtering/downscaling.
  Avatar/logo colors remain unchanged.
- Controller hints now format actual configured button bindings as PromptFont glyphs,
  with Xbox/PlayStation/Nintendo layouts where the device identifies the family. Confirm/back
  swapping changes the physical glyph labels as well as the input mapping. Unknown custom
  expressions retain their binding text with a neutral controller glyph. The text font reserves
  the used PromptFont codepoints so ordinary Unicode arrows cannot shadow the controller icons.
  On Home, the previous/next page controls now open Game List/toggle fullscreen as advertised.
- The render owner releases held keys from the previous input family on a new device press,
  preventing a lingering keyboard direction or controller input from overriding the new method.
  Releases arriving late do not change the active method. Menus also restore retained selection
  if ImGui loses its navigation window, without needing another device to restore focus.

Validation: 1,669 core tests passed, two optional preset tests skipped; all 259 Qt tests passed.
New regressions cover held-key source changes, delayed releases, immediate activation after
switching, navigation-focus recovery, physical glyph mapping and swapped confirm/back hints.
Native injected-input checks passed on D3D12, Vulkan, D3D11 and OpenGL while leaving keyboard
Right, keyboard Enter or controller Back held across device switches. Each check activated
Settings/Start Game and returned Home, then exited normally. These tests inject controller
navigation events into the real renderer; a physical controller was not operated by the agent.
Xbox and PlayStation footer fixtures were rendered and visually reviewed, including their
face-button, shoulder and D-pad glyphs. Temporary native input instrumentation is removed
from the ordinary user-test executable.

Evidence: build/visual-refinement/feedback-all-{core,qt}.log,
feedback-native-{D3D12,Vulkan,D3D,OGL}/input-native.log, feedback-keyboard-captures/,
feedback-xbox-captures/ and feedback-sony-captures/.

## DirectInput DualSense numbered-button follow-up

The saved Big Picture profile uses DInput/0/DualSense Wireless Controller, with raw
DirectInput button numbers. The previous formatter recognized symbolic SDL/XInput
names only, so it fell back to generic controller glyphs followed by Button N text.

Navigation status now carries the input backend as well as the device name. For
identified DS4/DualSense DirectInput devices, numbered HID buttons resolve to the
PlayStation face/shoulder glyphs; the mapping is consistent with SDL's Windows
PS4/PS5 database. Single qualified bindings resolve using their own backend/device.
An unrelated qualified binding no longer removes the identity needed by other hints.
Unknown devices/compound expressions retain their neutral fallback, and DirectInput
numbering is never applied to SDL or XInput bindings. Saved bindings are unchanged.

Regressions reproduce the saved Confirm=Button 2, Back=Button 1, Previous=Button 4,
Next=Button 5 profile, including swapped confirm/back and qualified bindings.
Core tests: 1,671 passed, two optional preset skips. Qt tests: 259 passed.
A native D3D12 probe using an isolated copy of the Big Picture profile verified the
connected DirectInput DualSense metadata and glyph hints from the real input publisher,
then passed the held-key device-switch/navigation checks. No physical button presses
were synthesized at the hardware level. GPU fixtures matching the saved numbered
bindings were rendered at 720p/1080p/2160p/ultrawide and visually reviewed.

Evidence: build/visual-refinement/dualsense-{prompt-tests,all-core,all-qt}.log,
dualsense-native-D3D12/input-native.log, and dualsense-dinput-captures/.
