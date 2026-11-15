# Big Picture achievements view — 2026-10-27

The pause menu includes Achievements. Its trophy entry is disabled and grey when
RetroAchievements is disabled, unavailable in the build, or has no authenticated client user.
A saved API token alone does not enable the entry. Controller policy checks login again when
processing the action. The view remains in Big Picture, with Back returning to pause.

The view follows the supplied references and uses the existing theme/font/icon system:

- A game/subset badge and title, earned/total achievements and points, and completion bar.
- A subset sidebar with keyboard, mouse and controller selection; Page Up/Down and shoulder
  buttons cycle subsets. Enter on a sidebar item moves focus into the achievement list.
- Collapsible Unlocked, Almost There, Locked and other runtime groups.
- Achievement badges, names, descriptions, points, lock/unlock icons, unlock dates and type tags.
- Measured progress bars and runtime-formatted values for incomplete achievements.
- A pinned header and thin footer; independently scrolling content preserves focus/scroll across
  refreshes, subset changes and return from pause. Empty or logged-out views remain dismissible.

The Qt owner reads rcheevos under the achievement manager lock and publishes immutable values.
The renderer never retains client pointers, reads files or downloads images. The adapter reuses
badge pixel snapshots until their content changes; the graphics owner uploads visible badges
into a bounded texture cache. Subset badges use the manager's existing asynchronous image/cache
pipeline and are cleared with the game. Progress buckets aggregate multiple subsets in rcheevos,
so membership is taken from its lock-state list before distributing progress groups by subset.

The open view refreshes through the existing 500 ms host timer. Counts, grouping, measured
progress, unlocks and arriving badges update without rebuilding the navigation stack. Missing
client/game data produces a loading or empty view, and logout cannot expose a cached list.

Validation:

- Windows Release/MSVC build with Qt 6.8.3 succeeded.
- Full core suite: 1,727 passed, two optional external-preset tests skipped.
- Full Qt suite: all 271 passed.
- All 161 Big Picture core tests pass. Added coverage includes login policy, pause routing,
  disabled entries, data hiding after logout, subset cycling, collapse/expand navigation, live
  snapshot replacement, return focus, empty-state dismissal and maximum-scale visibility.
- D3D11 captures use the production menu builder, font loading, image upload and renderer.
  Captures cover unlocked/progress/locked groups, scrolling, empty data, logged-out pause,
  Light/Dark themes, 1636 x 977 and 800 x 600, and maximum scale/safe area.
- Captures contain synthetic metadata and badge pixels; a live authenticated RetroAchievements
  account/game has not been exercised. Real unlock notifications and subset badge fetching
  should be checked in-game. Probe code, captures and logs remain ignored in build/pause-view.

Follow-up alignment and progress investigation:

- Point labels now use the first description line's baseline, including wrapped titles.
- A percentage without a current formatted measurement no longer creates a progress bar.
  The snapshot adapter clears unavailable/invalid measurements and the renderer requires text.
- All 49 renderer/navigation tests pass, including glyph-geometry baseline checks and a
  stale-percentage case that starts rendering again when a valid measurement arrives.
- The reported MGS dog-tag values are non-empty measurements from rcheevos. The user confirmed
  they appear only during boot/title screens and clear a few seconds after game boot. These
  transient runtime readings are distinct from server unlock totals. No game-specific startup
  filtering is added: the public achievement API does not expose a separate gameplay-validity
  flag. The stale-percentage guard addresses missing measurements, rather than these temporary
  non-empty readings.
- RetroAchievements documents that Measured values reflect the current evaluated game state,
  and that MeasuredIf/PauseIf should guard measurements when game memory is not valid:
  https://docs.retroachievements.org/developer-docs/flags/measured.html
