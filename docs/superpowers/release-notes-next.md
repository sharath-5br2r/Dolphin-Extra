# Release Notes — Next Fork Release

Changes since [`2606-fork-11`](https://github.com/ilyalissoboi/dolphin/releases/tag/2606-fork-11)
(`9f91e052fb`): Big Picture performance work
([#34](https://github.com/ilyalissoboi/dolphin/pull/34),
[#41](https://github.com/ilyalissoboi/dolphin/pull/41)), per-game library options
([#35](https://github.com/ilyalissoboi/dolphin/pull/35)), controller-friendly path and number
settings ([#36](https://github.com/ilyalissoboi/dolphin/pull/36)), game folder management
([#37](https://github.com/ilyalissoboi/dolphin/pull/37)), shared menu view code
([#38](https://github.com/ilyalissoboi/dolphin/pull/38)) and controller mapping inside Big Picture
([#39](https://github.com/ilyalissoboi/dolphin/pull/39),
[#40](https://github.com/ilyalissoboi/dolphin/pull/40)). Earlier entries in this file shipped in
previous fork releases and remain in its history.

## Big Picture game options

- Open a game's options from the library with the new **Game Options** menu button (X on
  Xbox-style controllers by default), **F6**, or a right-click on its card. Right-click
  previously opened Library Options, which stays on R1/F3.
- **Start Game**, including the disc choice for grouped multi-disc games.
- **Load State** lists that game's ten save-state slots, with previews, without starting it,
  and boots the game from the chosen slot. Hardcore mode, NetPlay and movie playback block it,
  as they block loading in a running game.
- **Game Properties** edits that game's settings without starting it. Executables without a
  game ID have no per-game settings.
- **Reset Play Time** asks first, with Cancel selected.
- Menu-control configurations from earlier releases have no Game Options binding. Bind it in
  **Settings → Controllers → Big Picture Menu Controls → Mapping**, or use F6.

## Big Picture settings

- Path settings open a **Choose Folder** or **Choose File** browser inside the settings menu,
  starting at the current value. Folder settings list folders only; choices are stored with
  native separators, as the desktop path pickers store them. **Enter Path** remains for typing.
- Volume no longer lists 101 rows. On a number setting's options page, Left and Right step the
  value (with key repeat) within its allowed range; the shoulder buttons still change category.

## Big Picture game folders

- **Library Options → Game Folders** lists the configured folders, with Add Game Folder,
  Scan Subfolders and Refresh. Selecting a folder asks to remove it from the library; no files
  are deleted. Folder changes made in desktop settings appear while Big Picture is open.
- Subfolder scanning remains one setting for all folders.

## Big Picture controller mapping

- **Settings → Controllers → (controller) → Mapping** lists every input with its current
  binding. Select one and press a button, key, stick direction or trigger to bind it; with no
  input for a few seconds, the binding is unchanged. Row options offer **Bind New Input** and
  **Clear Binding**; **Default Mapping** restores Dolphin's defaults after confirmation.
- While waiting for an input, the menu ignores controller input, including Back and the Open
  Menu binding, so those buttons can be bound too.
- Wii Remotes offer the attached **Extension** (Nunchuk, Classic Controller and others) and
  its inputs, plus **Attach MotionPlus**.
- Stick, trigger, rumble and option settings, such as dead zone, range, and sideways or
  upright Wii Remote, can be tuned on the mapping page. Settings driven by input expressions
  stay read-only.
- **Save as profile** names the current mapping with the on-screen keyboard and saves it to
  the user profile folder, like the desktop profile box.
- **Big Picture Menu Controls** is now in the controller list, so the menu's own bindings can
  be mapped, tuned and saved without the desktop. Keyboard navigation remains available.
- Mapping applies globally and, like device and profile changes, is unavailable while a game
  runs, during NetPlay or during movies. Motion-sensor passthrough, calibration and controller
  types remain in desktop settings.

## Big Picture performance

- Menu pages are no longer rebuilt every frame; large libraries no longer allocate per game on
  each frame.
- Settings and library state are shared between updates instead of being rebuilt or copied
  for each change, decoded cover, or folder listing.
- Covers are kept across play-time, title, tag and scan updates and are reloaded only when a
  game's artwork changes or a game leaves and returns to the library. The cover caches hold
  64 covers, with a 64 MiB limit for decoded artwork.

## RetroAchievements offline queue (Android)

- **Settings → RetroAchievements → Queue Submissions While Offline** (on by default) saves
  achievement unlocks and leaderboard entries earned without a connection, and submits them
  together once the connection returns. The setting shows how many are waiting.
- Saved submissions are kept in `Config/RetroAchievementsDeferred.json` until the server accepts
  or refuses them, so closing the game or the app no longer loses them. They are resent with
  the original unlock time; API tokens are never written to the file.
- Submissions are queued as soon as Android reports no network, or when a request gets no
  response at all. They are sent when Android reports a connection, after logging in, and after
  any successful contact with the server.
- Starting Dolphin offline no longer logs you out of RetroAchievements: a login that fails
  because the server can't be reached keeps the saved token and retries when the network
  returns. Achievement data still has to be downloaded once per game session, so a game started
  offline has no achievements until it is restarted online.
- The queue lives in the shared core, so desktop builds use it too, controlled by
  `OfflineQueueEnabled` in `RetroAchievements.ini` (no desktop UI yet). Without a connectivity
  signal there, only requests that get no response are queued.
