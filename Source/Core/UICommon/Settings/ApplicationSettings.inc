// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Explicit typed metadata transcribed from the named desktop controls.
// No Qt widgets or desktop dialogs are constructed to populate this catalog.

// Settings/InterfacePane.ui: useBuiltinTitleDatabaseCheckBox
{
  auto& entry =
      add(Config::MAIN_USE_BUILT_IN_TITLE_DATABASE, "native.main_use_built_in_title_database",
          "Interface", _trans("Use Built-In Database of Game Names"), Control::Toggle, false,
          ApplyPolicy::Live);
  entry.descriptor.help = _trans("Uses Dolphin's database of properly formatted names in the game "
                                 "list's Title column.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: collapseMultiDiscGamesCheckBox
{
  auto& entry = add(Config::MAIN_COLLAPSE_MULTI_DISC_GAMES, "native.main_collapse_multi_disc_games",
                    "Interface", _trans("Collapse Multi-Disc Games into a Single Entry"),
                    Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the discs of a multi-disc game as a single library entry and asks which "
             "disc to start when you launch it. Different game revisions and regions remain "
             "separate.\n\nIf unsure, leave this unchecked.");
}

// Settings/InterfacePane.ui: useCoversCheckBox
{
  auto& entry = add(Config::MAIN_USE_GAME_COVERS, "native.main_use_game_covers", "Interface",
                    _trans("Download Game Covers from GameTDB.com for Use in Grid Mode"),
                    Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Downloads full game covers from GameTDB.com to display in the game list's Grid View. "
             "If this setting is unchecked, the game list displays a banner from the game's save "
             "data, and if the game has no save file, displays a generic banner instead.\n\nList "
             "View will always use the save file banners.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: focusedHotkeysCheckBox
{
  auto& entry =
      add(Config::MAIN_FOCUSED_HOTKEYS, "native.main_focused_hotkeys", "Interface",
          _trans("Hotkeys Require Window Focus"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Requires the render window to be focused for hotkeys to take "
                                 "effect.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: disableScreensaverCheckBox
{
  auto& entry = add(Config::MAIN_DISABLE_SCREENSAVER, "native.main_disable_screensaver",
                    "Interface", _trans("Inhibit Screensaver During Emulation"), Control::Toggle,
                    false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Disables your screensaver while running a game.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: timeTrackingCheckBox
{
  auto& entry = add(Config::MAIN_TIME_TRACKING, "native.main_time_tracking", "Interface",
                    _trans("Enable Play Time Tracking"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Tracks the time you spend playing games and shows it in the List "
                                 "View (as hours/minutes).\n\nThis setting cannot be changed while "
                                 "emulation is active.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: keepWindowOnTopCheckBox
{
  auto& entry = add(Config::MAIN_KEEP_WINDOW_ON_TOP, "native.main_keep_window_on_top", "Interface",
                    _trans("Keep Window on Top"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Forces the render window to stay on top of other windows and "
                                 "applications.\n\nIf unsure, leave this unchecked.");
}

// Settings/InterfacePane.ui: confirmOnStopCheckBox
{
  auto& entry = add(Config::MAIN_CONFIRM_ON_STOP, "native.main_confirm_on_stop", "Interface",
                    _trans("Confirm on Stop"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Prompts you to confirm that you want to end emulation when you "
                                 "press Stop.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: usePanicHandlersCheckBox
{
  auto& entry = add(Config::MAIN_USE_PANIC_HANDLERS, "native.main_use_panic_handlers", "Interface",
                    _trans("Use Panic Handlers"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "In the event of an error, Dolphin will halt to inform you of the error and present choices "
      "on how to proceed. With this option disabled, Dolphin will \"ignore\" all errors. Emulation "
      "will not be halted and you will not be notified.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: showActiveTitleCheckBox
{
  auto& entry =
      add(Config::MAIN_SHOW_ACTIVE_TITLE, "native.main_show_active_title", "Interface",
          _trans("Show Active Title in Window Title"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows the active game title in the render window's title "
                                 "bar.\n\nIf unsure, leave this checked.");
}

// Settings/InterfacePane.ui: pauseOnFocusLossCheckBox
{
  auto& entry =
      add(Config::MAIN_PAUSE_ON_FOCUS_LOST, "native.main_pause_on_focus_lost", "Interface",
          _trans("Pause on Focus Loss"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Pauses the game whenever the render window isn't focused.\n\nIf "
                                 "unsure, leave this unchecked.");
}

// Settings/InterfacePane.ui: cursorOnMovementRadioButton
{
  auto& entry = add(Config::MAIN_SHOW_CURSOR, "native.main_show_cursor", "Interface",
                    _trans("On Movement"), Control::Choice, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows the mouse cursor briefly whenever it has recently moved, "
                                 "then hides it.\n\nIf unsure, select this mode.");
  entry.descriptor.choices = {
      {ToValue(static_cast<int>(Config::ShowCursor::OnMovement)), _trans("On Movement")},
      {ToValue(static_cast<int>(Config::ShowCursor::Never)), _trans("Never")},
      {ToValue(static_cast<int>(Config::ShowCursor::Constantly)), _trans("Always")},
  };
}

// Settings/InterfacePane.ui: lockMouseCursorCheckBox
{
  auto& entry = add(Config::MAIN_LOCK_CURSOR, "native.main_lock_cursor", "Interface",
                    _trans("Lock Mouse Cursor"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Locks the mouse cursor to the Render Widget as long as it has focus. You can set a "
             "hotkey to unlock it.\n\nIf unsure, leave this unchecked.");
}

// Settings/GeneralPane.ui: dualCoreCheckBox
{
  auto& entry =
      add(Config::MAIN_CPU_THREAD, "native.main_cpu_thread", "General",
          _trans("Enable Dual Core (speedhack)"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help =
      _trans("Separates CPU and GPU emulation work to separate threads. Reduces single-thread "
             "burden by spreading Dolphin's heaviest load across two cores, which usually improves "
             "performance. However, it can result in glitches and crashes.\n\nThis setting cannot "
             "be changed while emulation is active.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/GeneralPane.ui: cheatsCheckBox
{
  auto& entry = add(Config::MAIN_ENABLE_CHEATS, "native.main_enable_cheats", "General",
                    _trans("Enable Cheats"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans(
      "Enables the use of AR and Gecko cheat codes which can be used to modify games' behavior. "
      "These codes can be configured with the Cheats Manager in the Tools menu.\n\nThis setting "
      "cannot be changed while emulation is active.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/GeneralPane.ui: loadGameIntoMemoryCheckBox
{
  auto& entry =
      add(Config::MAIN_LOAD_GAME_INTO_MEMORY, "native.main_load_game_into_memory", "General",
          _trans("Load Whole Game Into Memory"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help =
      _trans("Loads the running game into memory in the background.\n\nThis may improve "
             "performance with slow or high-latency storage.\nSystem memory requirements will be "
             "much higher with this setting enabled.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/GeneralPane.ui: overrideRegionSettingsCheckBox
{
  auto& entry = add(Config::MAIN_OVERRIDE_REGION_SETTINGS, "native.main_override_region_settings",
                    "General", _trans("Allow Mismatched Region Settings"), Control::Toggle, false,
                    ApplyPolicy::IdleOnly);
  entry.descriptor.help =
      _trans("Lets you use languages and other region-related settings that the game may not be "
             "designed for. May cause various crashes and bugs.\n\nThis setting cannot be changed "
             "while emulation is active.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/GeneralPane.ui: autoDiscChangeCheckBox
{
  auto& entry =
      add(Config::MAIN_AUTO_DISC_CHANGE, "native.main_auto_disc_change", "General",
          _trans("Change Discs Automatically"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans(
      "Automatically changes the game disc when requested by games with two discs. This feature "
      "requires the game to be launched in one of the following ways:\n- From the game list, with "
      "both discs being present in the game list.\n- With File > Open or the command line "
      "interface, with the paths to both discs being provided.\n- By launching an M3U file with "
      "File > Open or the command line interface.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Config/Graphics/GeneralWidget.ui: customAspectWidthSpinBox
{
  auto& entry =
      add(Config::GFX_CUSTOM_ASPECT_RATIO_WIDTH, "native.gfx_custom_aspect_ratio_width", "Display",
          _trans("Custom Aspect Ratio"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Scales the game image by a whole-number multiple of its rendered resolution whenever "
             "the window is large enough. This produces more uniform pixels and gives "
             "post-processing shaders an integer-sized output, at the cost of larger black "
             "borders.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.range = Range{1, 10000, 1, 1, 0, ""};
}

// Config/Graphics/GeneralWidget.ui: fullscreenCheckBox
{
  auto& entry = add(Config::MAIN_FULLSCREEN, "native.main_fullscreen", "Display",
                    _trans("Start in Fullscreen"), Control::Toggle, true, ApplyPolicy::IdleOnly);
  entry.descriptor.help =
      _trans("Uses the entire screen for rendering.\n\nIf disabled, a render window will be "
             "created instead.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/GeneralWidget.ui: fullscreenOnDoubleClickCheckBox
{
  auto& entry = add(
      Config::MAIN_FULLSCREEN_ON_DOUBLE_CLICK, "native.main_fullscreen_on_double_click", "Display",
      _trans("Toggle Fullscreen on Double-Click"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Toggles between windowed and fullscreen mode when you double-click the render "
             "window with the left mouse button.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/GeneralWidget.ui: precisionFrameTimingCheckBox
{
  auto& entry =
      add(Config::MAIN_PRECISION_FRAME_TIMING, "native.main_precision_frame_timing", "Display",
          _trans("Precision Frame Timing"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Automatically adjusts the window size to the internal "
                                 "resolution.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/GeneralWidget.ui: autoAdjustWindowSizeCheckBox
{
  auto& entry =
      add(Config::MAIN_RENDER_WINDOW_AUTOSIZE, "native.main_render_window_autosize", "Display",
          _trans("Auto-Adjust Window Size"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Auto-Adjust Window Size");
}

// Config/Graphics/GeneralWidget.ui: renderToMainWindowCheckBox
{
  auto& entry = add(Config::MAIN_RENDER_TO_MAIN, "native.main_render_to_main", "Display",
                    _trans("Render to Main Window"), Control::Toggle, true, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Uses the main Dolphin window for rendering rather than a "
                                 "separate render window.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/GeneralWidget.ui: specializedShaderRadioButton
{
  auto& entry =
      add(Config::GFX_SHADER_COMPILATION_MODE, "native.gfx_shader_compilation_mode", "Display",
          _trans("Specialized (Default)"), Control::Choice, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Ubershaders are never used. Stuttering will occur during shader compilation, but GPU "
      "demands are low.\n\nRecommended for low-end hardware. \n\nIf unsure, select this mode.");
  entry.descriptor.choices = {
      {ToValue(0), _trans("Specialized (Default)")},
      {ToValue(1), _trans("Exclusive Ubershaders")},
      {ToValue(2), _trans("Hybrid Ubershaders")},
      {ToValue(3), _trans("Skip Drawing")},
  };
}

// Config/Graphics/GeneralWidget.ui: waitForShadersCheckBox
{
  auto& entry =
      add(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING,
          "native.gfx_wait_for_shaders_before_starting", "Display",
          _trans("Compile Shaders Before Starting"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Waits for all shaders to finish compiling before starting a game. Enabling this "
             "option may reduce stuttering or hitching for a short time after the game is started, "
             "at the cost of a longer delay before the game starts. For systems with two or fewer "
             "cores, it is recommended to enable this option, as a large shader queue may reduce "
             "frame rates.\n\nOtherwise, if unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: scaledEfbCopyCheckBox
{
  auto& entry =
      add(Config::GFX_HACK_COPY_EFB_SCALED, "native.gfx_hack_copy_efb_scaled", "Enhancements",
          _trans("Scaled EFB Copy"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Greatly increases the quality of textures generated using render-to-texture "
             "effects.\n\nSlightly increases GPU load and causes relatively few graphical issues. "
             "Raising the internal resolution will improve the effect of this setting. \n\nIf "
             "unsure, leave this checked.");
}

// Config/Graphics/EnhancementsWidget.ui: perPixelLightingCheckBox
{
  auto& entry =
      add(Config::GFX_ENABLE_PIXEL_LIGHTING, "native.gfx_enable_pixel_lighting", "Enhancements",
          _trans("Per-Pixel Lighting"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
      "appearance of lit polygons and making individual triangles less noticeable.\n\nRarely "
      "causes slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: widescreenHackCheckBox
{
  auto& entry = add(Config::GFX_WIDESCREEN_HACK, "native.gfx_widescreen_hack", "Enhancements",
                    _trans("Widescreen Hack"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Forces the game to output graphics at any aspect ratio by expanding the view frustum "
      "without stretching the image.\nThis is a hack, and its results will vary widely game to "
      "game (it often causes the UI to stretch).\nGame-specific AR/Gecko-code aspect ratio patches "
      "are preferable over this if available.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: disableFogCheckBox
{
  auto& entry = add(Config::GFX_DISABLE_FOG, "native.gfx_disable_fog", "Enhancements",
                    _trans("Disable Fog"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Makes distant objects more visible by removing fog, thus increasing the overall "
             "detail.\n\nDisabling fog will break some games which rely on proper fog "
             "emulation.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: force24BitColorCheckBox
{
  auto& entry =
      add(Config::GFX_ENHANCE_FORCE_TRUE_COLOR, "native.gfx_enhance_force_true_color",
          "Enhancements", _trans("Force 24-Bit Color"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Forces the game to render the RGB color channels in 24-bit, thereby increasing "
             "quality by reducing color banding.\n\nHas no impact on performance and causes few "
             "graphical issues.\n\nIf unsure, leave this checked.");
}

// Config/Graphics/EnhancementsWidget.ui: disableCopyFilterCheckBox
{
  auto& entry =
      add(Config::GFX_ENHANCE_DISABLE_COPY_FILTER, "native.gfx_enhance_disable_copy_filter",
          "Enhancements", _trans("Disable Copy Filter"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Disables the blending of adjacent rows when copying the EFB. This is known in some "
             "games as \"deflickering\" or \"smoothing\".\n\nDisabling the filter has no effect on "
             "performance, but may result in a sharper image. Causes few graphical issues.\n\nIf "
             "unsure, leave this checked.");
}

// Config/Graphics/EnhancementsWidget.ui: arbitraryMipmapDetectionCheckBox
{
  auto& entry = add(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
                    "native.gfx_enhance_arbitrary_mipmap_detection", "Enhancements",
                    _trans("Arbitrary Mipmap Detection"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Enables detection of arbitrary mipmaps, which some games use for special distance-based "
      "effects.\n\nMay have false positives that result in blurry textures at increased internal "
      "resolution, such as in games that use very low resolution mipmaps. Disabling this can also "
      "reduce stutter in games that frequently load new textures.\n\nIf this setting is enabled, "
      "GPU Texture Decoding will be disabled.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: hdrCheckBox
{
  auto& entry = add(Config::GFX_ENHANCE_HDR_OUTPUT, "native.gfx_enhance_hdr_output", "Enhancements",
                    _trans("HDR Post-Processing"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Enables scRGB HDR output (if supported by your graphics backend and monitor). "
             "Fullscreen might be required.\n\nThis gives post process shaders more room for "
             "accuracy, allows \"AutoHDR\" post-process shaders to work, and allows to fully "
             "display the PAL and NTSC-J color spaces.\n\nNote that games still render in SDR "
             "internally.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: stereoDepthSlider
{
  auto& entry = add(Config::GFX_STEREO_DEPTH, "native.gfx_stereo_depth", "Enhancements",
                    _trans("Depth"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Controls the separation distance between the virtual cameras.\n\nA higher value "
             "creates a stronger feeling of depth while a lower value is more comfortable.");
  entry.descriptor.range = Range{0, Config::GFX_STEREO_DEPTH_MAXIMUM, 1.0f, 1, 0, ""};
}

// Config/Graphics/EnhancementsWidget.ui: stereoConvergenceSlider
{
  auto& entry = add(Config::GFX_STEREO_CONVERGENCE, "native.gfx_stereo_convergence", "Enhancements",
                    _trans("Convergence"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Controls the distance of the convergence plane. This is the distance at which "
             "virtual objects will appear to be in front of the screen.\n\nA higher value creates "
             "stronger out-of-screen effects while a lower value is more comfortable.");
  entry.descriptor.range = Range{0, Config::GFX_STEREO_CONVERGENCE_MAXIMUM, 0.01f, 1, 0, ""};
}

// Config/Graphics/EnhancementsWidget.ui: swapEyesCheckBox
{
  auto& entry = add(Config::GFX_STEREO_SWAP_EYES, "native.gfx_stereo_swap_eyes", "Enhancements",
                    _trans("Swap Eyes"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Swaps the left and right eye. Most useful in side-by-side "
                                 "stereoscopy mode.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/EnhancementsWidget.ui: fullResolutionPerEyeCheckBox
{
  auto& entry =
      add(Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL, "native.gfx_stereo_per_eye_resolution_full",
          "Enhancements", _trans("Use Full Resolution Per Eye"), Control::Toggle, true,
          ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Whether each eye gets full or half image resolution when using side-by-side or "
             "above-and-below 3D.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/HacksWidget.ui: ignoreFormatChangesCheckBox
{
  auto& entry = add(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
                    "native.gfx_hack_efb_emulate_format_changes", "GraphicsHacks",
                    _trans("Ignore Format Changes"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Ignores any changes to the EFB format.\n\nImproves performance in many games without "
             "any negative effect. Causes graphical defects in a small number of other "
             "games.\n\nIf unsure, leave this checked.");
  entry.descriptor.reverse = true;
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: storeEfbCopiesCheckBox
{
  auto& entry = add(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, "native.gfx_hack_skip_efb_copy_to_ram",
                    "GraphicsHacks", _trans("Store EFB Copies to Texture Only"), Control::Toggle,
                    true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Stores EFB copies exclusively on the GPU, bypassing system memory. Causes graphical "
             "defects in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = "
             "EFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: deferEfbCopiesCheckBox
{
  auto& entry =
      add(Config::GFX_HACK_DEFER_EFB_COPIES, "native.gfx_hack_defer_efb_copies", "GraphicsHacks",
          _trans("Defer EFB Copies to RAM"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Waits until the game synchronizes with the emulated GPU before writing the contents "
             "of EFB copies to RAM.\n\nReduces the overhead of EFB RAM copies, providing a "
             "performance boost in many games, at the risk of breaking those which do not safely "
             "synchronize with the emulated GPU.\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: gpuTextureDecodingCheckBox
{
  auto& entry = add(Config::GFX_ENABLE_GPU_TEXTURE_DECODING,
                    "native.gfx_enable_gpu_texture_decoding", "GraphicsHacks",
                    _trans("GPU Texture Decoding"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("GPU Texture Decoding");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: storeXfbCopiesCheckBox
{
  auto& entry = add(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, "native.gfx_hack_skip_xfb_copy_to_ram",
                    "GraphicsHacks", _trans("Store XFB Copies to Texture Only"), Control::Toggle,
                    true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Stores XFB copies exclusively on the GPU, bypassing system memory. Causes graphical "
             "defects in a small number of games.\n\nEnabled = XFB Copies to Texture\nDisabled = "
             "XFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: immediateXfbCheckBox
{
  auto& entry =
      add(Config::GFX_HACK_IMMEDIATE_XFB, "native.gfx_hack_immediate_xfb", "GraphicsHacks",
          _trans("Immediately Present XFB"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Displays XFB copies as soon as they are created, instead of waiting for "
             "scanout.\n\nCan cause graphical defects in some games if the game doesn't expect all "
             "XFB copies to be displayed. However, turning this setting on reduces "
             "latency.\n\nEnabling this also forces an effect equivalent to the Skip Presenting "
             "Duplicate Frames setting.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: skipDuplicateXfbsCheckBox
{
  auto& entry = add(Config::GFX_HACK_SKIP_DUPLICATE_XFBS, "native.gfx_hack_skip_duplicate_xfbs",
                    "GraphicsHacks", _trans("Skip Presenting Duplicate Frames"), Control::Toggle,
                    true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Skips presentation of duplicate frames (XFB copies) in 25fps/30fps games. This may "
             "improve performance on low-end devices, while making frame pacing less "
             "consistent.\n\nDisable this option for optimal frame pacing.\n\nThis setting is "
             "unavailable when Immediately Present XFB or VBI Skip is enabled. In those cases, "
             "duplicate frames are never presented.\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: fastDepthCalculationCheckBox
{
  auto& entry = add(Config::GFX_FAST_DEPTH_CALC, "native.gfx_fast_depth_calc", "GraphicsHacks",
                    _trans("Fast Depth Calculation"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Uses a less accurate algorithm to calculate depth values.\n\nCauses issues in a few "
             "games, but can result in a decent speed increase depending on the game and/or "
             "GPU.\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: disableBoundingBoxCheckBox
{
  auto& entry = add(Config::GFX_HACK_BBOX_ENABLE, "native.gfx_hack_bbox_enable", "GraphicsHacks",
                    _trans("Disable Bounding Box"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Disable Bounding Box");
  entry.descriptor.reverse = true;
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: vertexRoundingCheckBox
{
  auto& entry =
      add(Config::GFX_HACK_VERTEX_ROUNDING, "native.gfx_hack_vertex_rounding", "GraphicsHacks",
          _trans("Vertex Rounding"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Rounds 2D vertices to whole pixels and rounds the viewport size to a whole number.\n\nFixes "
      "graphical problems in some games at higher internal resolutions. This setting has no effect "
      "when native internal resolution is used.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: saveTextureCacheStateCheckBox
{
  auto& entry =
      add(Config::GFX_SAVE_TEXTURE_CACHE_TO_STATE, "native.gfx_save_texture_cache_to_state",
          "GraphicsHacks", _trans("Save Texture Cache to State"), Control::Toggle, true,
          ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Includes the contents of the embedded frame buffer (EFB) and upscaled EFB copies in "
             "save states. Fixes missing and/or non-upscaled textures/objects when loading states "
             "at the cost of additional save/load time.\n\nIf unsure, leave this checked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/HacksWidget.ui: viSkipCheckBox
{
  auto& entry = add(Config::GFX_HACK_VI_SKIP, "native.gfx_hack_vi_skip", "GraphicsHacks",
                    _trans("VBI Skip"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Skips Vertical Blank Interrupts when lag is detected, allowing for smooth audio "
             "playback when emulation speed is not 100%. \n\nEnabling this also forces the effect "
             "of the Skip Presenting Duplicate Frames setting.\n\nWARNING: Can cause freezes and "
             "compatibility issues. \n\nIf unsure, leave this unchecked.");
  entry.descriptor.netplay_safe = false;
}

// Config/Graphics/AdvancedWidget.ui: wireframeCheckBox
{
  auto& entry = add(Config::GFX_ENABLE_WIREFRAME, "native.gfx_enable_wireframe", "GraphicsAdvanced",
                    _trans("Enable Wireframe"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Renders the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: textureFormatOverlayCheckBox
{
  auto& entry =
      add(Config::GFX_TEXFMT_OVERLAY_ENABLE, "native.gfx_texfmt_overlay_enable", "GraphicsAdvanced",
          _trans("Texture Format Overlay"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Modifies textures to show the format they're encoded in.\n\nMay require an emulation "
             "reset to apply.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: apiValidationCheckBox
{
  auto& entry = add(Config::GFX_ENABLE_VALIDATION_LAYER, "native.gfx_enable_validation_layer",
                    "GraphicsAdvanced", _trans("Enable API Validation Layers"), Control::Toggle,
                    true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Enables validation of API calls made by the video backend, which may assist in "
             "debugging graphical issues. On the Vulkan and D3D backends, this also enables debug "
             "symbols for the compiled shaders.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: logRenderTimeCheckBox
{
  auto& entry = add(Config::GFX_LOG_RENDER_TIME_TO_FILE, "native.gfx_log_render_time_to_file",
                    "GraphicsAdvanced", _trans("Log Render Time to File"), Control::Toggle, true,
                    ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Logs the render time of every frame to User/Logs/render_time.txt.\n\nUse this "
             "feature to measure Dolphin's performance.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: loadCustomTexturesCheckBox
{
  auto& entry = add(Config::GFX_HIRES_TEXTURES, "native.gfx_hires_textures", "GraphicsAdvanced",
                    _trans("Load Custom Textures"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Loads custom textures from User/Load/Textures/<game_id>/ and "
             "User/Load/DynamicInputTextures/<game_id>/.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: prefetchCustomTexturesCheckBox
{
  auto& entry =
      add(Config::GFX_CACHE_HIRES_TEXTURES, "native.gfx_cache_hires_textures", "GraphicsAdvanced",
          _trans("Prefetch Custom Textures"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Caches custom textures to system RAM on startup.\n\nThis can require exponentially "
             "more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: dumpEfbTargetCheckBox
{
  auto& entry = add(Config::GFX_DUMP_EFB_TARGET, "native.gfx_dump_efb_target", "GraphicsAdvanced",
                    _trans("Dump EFB Target"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Dumps the contents of EFB copies to User/Dump/Textures/.\n\nIf "
                                 "unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: dumpXfbTargetCheckBox
{
  auto& entry = add(Config::GFX_DUMP_XFB_TARGET, "native.gfx_dump_xfb_target", "GraphicsAdvanced",
                    _trans("Dump XFB Target"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Dumps the contents of XFB copies to User/Dump/Textures/.\n\nIf "
                                 "unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: disableVramCopiesCheckBox
{
  auto& entry = add(Config::GFX_HACK_DISABLE_COPY_TO_VRAM, "native.gfx_hack_disable_copy_to_vram",
                    "GraphicsAdvanced", _trans("Disable EFB VRAM Copies"), Control::Toggle, true,
                    ApplyPolicy::Live);
  entry.descriptor.help = _trans("Disables the VRAM copy of the EFB, forcing a round-trip to RAM. "
                                 "Inhibits all upscaling.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: enableGraphicsModsCheckBox
{
  auto& entry = add(Config::GFX_MODS_ENABLE, "native.gfx_mods_enable", "GraphicsAdvanced",
                    _trans("Enable Graphics Mods"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Loads graphics mods from User/Load/GraphicsMods/.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: dumpTexturesCheckBox
{
  auto& entry = add(Config::GFX_DUMP_TEXTURES, "native.gfx_dump_textures", "GraphicsAdvanced",
                    _trans("Enable"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Dumps decoded game textures based on the other flags to "
             "User/Dump/Textures/<game_id>/.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: dumpBaseTexturesCheckBox
{
  auto& entry =
      add(Config::GFX_DUMP_BASE_TEXTURES, "native.gfx_dump_base_textures", "GraphicsAdvanced",
          _trans("Dump Base Textures"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Whether to dump base game textures to User/Dump/Textures/<game_id>/.  This includes "
             "arbitrary base textures if 'Arbitrary Mipmap Detection' is enabled in "
             "Enhancements.\n\nIf unsure, leave this checked.");
}

// Config/Graphics/AdvancedWidget.ui: dumpMipTexturesCheckBox
{
  auto& entry =
      add(Config::GFX_DUMP_MIP_TEXTURES, "native.gfx_dump_mip_textures", "GraphicsAdvanced",
          _trans("Dump Mip Maps"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Whether to dump mipmapped game textures to User/Dump/Textures/<game_id>/.  This "
             "includes arbitrary mipmapped textures if 'Arbitrary Mipmap Detection' is enabled in "
             "Enhancements.\n\nIf unsure, leave this checked.");
}

// Config/Graphics/AdvancedWidget.ui: resolutionTypeComboBox
{
  auto& entry =
      add(Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE, "native.gfx_frame_dumps_resolution_type",
          "GraphicsAdvanced", _trans("Resolution Type"), Control::Choice, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Selects how frame dumps (videos) and screenshots are going to be captured.\nIf the game or "
      "window resolution change during a recording, multiple video files might be created.\nNote "
      "that color correction and cropping are always ignored by the captures.\n\nWindow "
      "Resolution: Uses the output window resolution (without black bars).\nThis is a simple "
      "dumping option that will capture the image more or less as you see it.\nAspect Ratio "
      "Corrected Internal Resolution: Uses the Internal Resolution (XFB size), and corrects it by "
      "the target aspect ratio.\nThis option will consistently dump at the specified Internal "
      "Resolution regardless of how the image is displayed during recording.\nRaw Internal "
      "Resolution: Uses the Internal Resolution (XFB size) without correcting it with the target "
      "aspect ratio.\nThis will provide a clean dump without any aspect ratio correction so users "
      "have as raw as possible input for external editing software.\n\nIf unsure, leave this at "
      "\"Aspect Ratio Corrected Internal Resolution\".");
  entry.descriptor.choices = {
      {ToValue(static_cast<SettingType<decltype(Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE)>>(0)),
       _trans("Window Resolution")},
      {ToValue(static_cast<SettingType<decltype(Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE)>>(1)),
       _trans("Aspect Ratio Corrected Internal Resolution")},
      {ToValue(static_cast<SettingType<decltype(Config::GFX_FRAME_DUMPS_RESOLUTION_TYPE)>>(2)),
       _trans("Raw Internal Resolution")},
  };
}

// Config/Graphics/AdvancedWidget.ui: pngCompressionSpinBox
{
  auto& entry =
      add(Config::GFX_PNG_COMPRESSION_LEVEL, "native.gfx_png_compression_level", "GraphicsAdvanced",
          _trans("PNG Compression Level"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Specifies the zlib compression level to use when saving PNG images (both for "
             "screenshots and framedumping).\n\nSince PNG uses lossless compression, this does not "
             "affect the image quality; instead, it is a trade-off between file size and "
             "compression time.\n\nA value of 0 uses no compression at all.  A value of 1 uses "
             "very little compression, while the maximum value of 9 applies a lot of compression.  "
             "However, for PNG files, levels between 3 and 6 are generally about as good as level "
             "9 but finish in significantly less time.\n\nIf unsure, leave this at 6.");
  entry.descriptor.range = Range{0, 9, 1, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: losslessCodecCheckBox
{
  auto& entry =
      add(Config::GFX_USE_LOSSLESS, "native.gfx_use_lossless", "GraphicsAdvanced",
          _trans("Use Lossless Codec (Ut Video)"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Encodes frame dumps using the Ut Video codec. If this option is unchecked, a lossy "
             "Xvid codec will be used.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: bitrateSpinBox
{
  auto& entry = add(Config::GFX_BITRATE_KBPS, "native.gfx_bitrate_kbps", "GraphicsAdvanced",
                    _trans("Bitrate (kbps)"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Bitrate (kbps)");
  entry.descriptor.range = Range{0, 1000000, 1000, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: cropToAspectRatioCheckBox
{
  auto& entry =
      add(Config::GFX_CROP_TO_ASPECT_RATIO, "native.gfx_crop_to_aspect_ratio", "GraphicsAdvanced",
          _trans("To Aspect Ratio"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Crops the picture from its native aspect ratio (which rarely exactly matches 4:3 or "
             "16:9, because it often includes overscan) to the specific user target aspect ratio "
             "(e.g. 4:3 or 16:9).\n\nThis option was previously known as "
             "Graphics/Advanced/Misc/Crop.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: customCropCheckBox
{
  auto& entry = add(Config::GFX_CROP_CUSTOM, "native.gfx_crop_custom", "GraphicsAdvanced",
                    _trans("Custom"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Enables options to crop the picture by discrete native pixels (independent of the "
             "internal resolution setting) in order to attain a specific user target aspect ratio. "
             "Useful to resolve letterboxing issues. \n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: cropLeftSpinBox
{
  auto& entry = add(Config::GFX_CROP_CUSTOM_LEFT, "native.gfx_crop_custom_left", "GraphicsAdvanced",
                    _trans("Left"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Crops the picture left side by discrete native pixels (independent of the internal "
             "resolution setting) in order to attain a specific user target aspect ratio.  \n\nIf "
             "unsure, leave this value to 0.");
  entry.descriptor.range = Range{0, 640, 1, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: cropTopSpinBox
{
  auto& entry = add(Config::GFX_CROP_CUSTOM_TOP, "native.gfx_crop_custom_top", "GraphicsAdvanced",
                    _trans("Top"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Crops the picture top side by discrete native pixels (independent of the internal "
             "resolution setting) in order to attain a specific user target aspect ratio.  \n\nIf "
             "unsure, leave this value to 0.");
  entry.descriptor.range = Range{0, 528, 1, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: cropRightSpinBox
{
  auto& entry = add(Config::GFX_CROP_CUSTOM_RIGHT, "native.gfx_crop_custom_right",
                    "GraphicsAdvanced", _trans("Right"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Crops the picture right side by discrete native pixels (independent of the internal "
             "resolution setting) in order to attain a specific user target aspect ratio.  \n\nIf "
             "unsure, leave this value to 0.");
  entry.descriptor.range = Range{0, 640, 1, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: cropBottomSpinBox
{
  auto& entry = add(Config::GFX_CROP_CUSTOM_BOTTOM, "native.gfx_crop_custom_bottom",
                    "GraphicsAdvanced", _trans("Bottom"), Control::Number, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Crops the picture bottom side by discrete native pixels (independent of the internal "
             "resolution setting) in order to attain a specific user target aspect ratio.  \n\nIf "
             "unsure, leave this value to 0.");
  entry.descriptor.range = Range{0, 528, 1, 1, 0, ""};
}

// Config/Graphics/AdvancedWidget.ui: progressiveScanCheckBox
{
  auto& entry =
      add(Config::SYSCONF_PROGRESSIVE_SCAN, "native.sysconf_progressive_scan", "GraphicsAdvanced",
          _trans("Enable Progressive Scan"), Control::Toggle, true, ApplyPolicy::IdleOnly);
  entry.descriptor.help =
      _trans("Enables progressive scan if supported by the emulated software. Most games don't "
             "have any issue with this.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: backendMultithreadingCheckBox
{
  auto& entry = add(Config::GFX_BACKEND_MULTITHREADING, "native.gfx_backend_multithreading",
                    "GraphicsAdvanced", _trans("Backend Multithreading"), Control::Toggle, true,
                    ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Enables multithreaded command submission in backends where supported. Enabling this option "
      "may result in a performance improvement on systems with more than two CPU cores. Currently, "
      "this is limited to the Vulkan backend.\n\nIf unsure, leave this checked.");
}

// Config/Graphics/AdvancedWidget.ui: preferVsExpansionCheckBox
{
  auto& entry =
      add(Config::GFX_PREFER_VS_FOR_LINE_POINT_EXPANSION,
          "native.gfx_prefer_vs_for_line_point_expansion", "GraphicsAdvanced",
          _trans("Prefer VS for Point/Line Expansion"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Prefer VS for Point/Line Expansion");
}

// Config/Graphics/AdvancedWidget.ui: cpuCullCheckBox
{
  auto& entry = add(Config::GFX_CPU_CULL, "native.gfx_cpu_cull", "GraphicsAdvanced",
                    _trans("Cull Vertices on the CPU"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Cull vertices on the CPU to reduce the number of draw calls required.  May affect "
             "performance and draw statistics.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: borderlessFullscreenCheckBox
{
  auto& entry =
      add(Config::GFX_BORDERLESS_FULLSCREEN, "native.gfx_borderless_fullscreen", "GraphicsAdvanced",
          _trans("Borderless Fullscreen"), Control::Toggle, true, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Implements fullscreen mode with a borderless window spanning the whole screen "
             "instead of using exclusive mode. Allows for faster transitions between fullscreen "
             "and windowed mode, but slightly increases input latency, makes movement less smooth "
             "and slightly decreases performance.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: deferEfbInvalidationCheckBox
{
  auto& entry =
      add(Config::GFX_HACK_EFB_DEFER_INVALIDATION, "native.gfx_hack_efb_defer_invalidation",
          "GraphicsAdvanced", _trans("Defer EFB Cache Invalidation"), Control::Toggle, true,
          ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Defers invalidation of the EFB access cache until a GPU synchronization command is "
             "executed. If disabled, the cache will be invalidated with every draw call. \n\nMay "
             "improve performance in some games which rely on CPU EFB Access at the cost of "
             "stability.\n\nIf unsure, leave this unchecked.");
}

// Config/Graphics/AdvancedWidget.ui: manualTextureSamplingCheckBox
{
  auto& entry = add(Config::GFX_HACK_FAST_TEXTURE_SAMPLING, "native.gfx_hack_fast_texture_sampling",
                    "GraphicsAdvanced", _trans("Manual Texture Sampling"), Control::Toggle, true,
                    ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Use a manual implementation of texture sampling instead of the graphics backend's built-in "
      "functionality.\n\nThis setting can fix graphical issues in some games on certain GPUs, most "
      "commonly vertical lines on FMVs. In addition to this, enabling Manual Texture Sampling will "
      "allow for correct emulation of texture wrapping special cases (at 1x IR or when scaled EFB "
      "is disabled, and with custom textures disabled) and better emulates Level of Detail "
      "calculation.\n\nThis comes at the cost of potentially worse performance, especially at "
      "higher internal resolutions.\n\nIf this setting is enabled, the Texture Filtering setting "
      "will be disabled.\n\nIf unsure, leave this unchecked.");
  entry.descriptor.reverse = true;
}

// Settings/AudioPane.ui: dolbyProLogicCheckBox
{
  auto& entry =
      add(Config::MAIN_DPL2_DECODER, "native.main_dpl2_decoder", "Audio",
          _trans("Dolby Pro Logic II Decoder"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enables Dolby Pro Logic II emulation using 5.1 surround. Certain "
                                 "backends only. \n\nIf unsure, leave this unchecked.");
}

// Settings/AudioPane.ui: audioBufferSizeSlider
{
  auto& entry = add(Config::MAIN_AUDIO_BUFFER_SIZE, "native.main_audio_buffer_size", "Audio",
                    _trans("Audio Buffer Size"), Control::Number, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Controls the number of audio samples buffered. Lower values reduce latency but may "
             "cause more crackling or stuttering.\n\nIf unsure, set this to 80 ms.");
  entry.descriptor.range = Range{16, 512, 8, 1, 0, ""};
}

// Settings/AudioPane.ui: audioFillGapsCheckBox
{
  auto& entry = add(Config::MAIN_AUDIO_FILL_GAPS, "native.main_audio_fill_gaps", "Audio",
                    _trans("Fill Audio Gaps"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Repeat existing audio during lag spikes to prevent "
                                 "stuttering.\n\nIf unsure, leave this checked.");
}

// Settings/AudioPane.ui: audioPreservePitchCheckBox
{
  auto& entry = add(Config::MAIN_AUDIO_PRESERVE_PITCH, "native.main_audio_preserve_pitch", "Audio",
                    _trans("Preserve Audio Pitch"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Keeps audio at normal pitch when changing emulation speed. Without this, audio pitch "
             "changes proportionally with speed.\n\nIf unsure, leave this unchecked.");
}

// Settings/AudioPane.ui: muteOnUnlimitedSpeedCheckBox
{
  auto& entry =
      add(Config::MAIN_AUDIO_MUTE_ON_DISABLED_SPEED_LIMIT,
          "native.main_audio_mute_on_disabled_speed_limit", "Audio",
          _trans("Mute When Disabling Speed Limit"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Mutes the audio when overriding the emulation speed limit "
                                 "(default hotkey: Tab). \n\nIf unsure, leave this unchecked.");
}

// Settings/AudioPane.ui: wiimoteRoutingCheckBox
{
  auto& entry =
      add(Config::MAIN_WIIMOTE_AUDIO_ROUTING_ENABLED, "native.main_wiimote_audio_routing_enabled",
          "Audio", _trans("Enable Wii Remote Audio Routing"), Control::Toggle, false,
          ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Routes each Wii Remote's speaker audio to a separate audio output device. This setting "
      "cannot be changed while emulation is active.\n\nThis setting is disabled when Enable "
      "Speaker Data is disabled, Passthrough a Bluetooth adapter is selected, or an audio backend "
      "other than Cubeb is selected.\n\nIf unsure, leave this unchecked.");
}

// Settings/AudioPane.ui: gbaRoutingCheckBox
{
  auto& entry =
      add(Config::MAIN_GBA_AUDIO_ROUTING_ENABLED, "native.main_gba_audio_routing_enabled", "Audio",
          _trans("Enable GBA Audio Routing"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Routes each GBA's speaker audio to a separate audio output device. This setting "
             "cannot be changed while emulation is active.\n\nThis setting is disabled when an "
             "audio backend other than Cubeb is selected.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showMessagesCheckBox
{
  auto& entry = add(Config::MAIN_OSD_MESSAGES, "native.main_osd_messages", "OSD",
                    _trans("Show Messages"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows on-screen display messages over the render window. These messages disappear "
             "after several seconds.\n\nIf unsure, leave this checked.");
}

// Settings/OnScreenDisplayPane.ui: fontSizeSpinBox
{
  auto& entry = add(Config::MAIN_OSD_FONT_SIZE, "native.main_osd_font_size", "OSD",
                    _trans("Font Size"), Control::Number, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Changes the font size of the On-Screen Display. Affects features such as performance "
             "statistics, frame counter, and netplay chat.\n\nThe font can be changed by placing a "
             "TTF font file into Dolphin's User/Load folder, and renaming it OSD_Font.ttf.\n\nIf "
             "unsure, leave this at 13.");
  entry.descriptor.range = Range{12, 40, 1, 1, 0, ""};
}

// Settings/OnScreenDisplayPane.ui: showFpsCheckBox
{
  auto& entry = add(Config::GFX_SHOW_FPS, "native.gfx_show_fps", "OSD", _trans("Show FPS"),
                    Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the number of distinct frames rendered per second as a measure of visual "
             "smoothness.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showFrameTimesCheckBox
{
  auto& entry = add(Config::GFX_SHOW_FTIMES, "native.gfx_show_ftimes", "OSD",
                    _trans("Show Frame Times"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the average time in ms between each distinct rendered frame alongside the "
             "standard deviation.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showVpsCheckBox
{
  auto& entry = add(Config::GFX_SHOW_VPS, "native.gfx_show_vps", "OSD", _trans("Show VPS"),
                    Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows the number of frames rendered per second as a measure of "
                                 "emulation speed.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showVblankTimesCheckBox
{
  auto& entry = add(Config::GFX_SHOW_VTIMES, "native.gfx_show_vtimes", "OSD",
                    _trans("Show VBlank Times"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the average time in ms between each rendered frame alongside the standard "
             "deviation.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showSpeedCheckBox
{
  auto& entry = add(Config::GFX_SHOW_SPEED, "native.gfx_show_speed", "OSD", _trans("Show % Speed"),
                    Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans(
      "Shows the % speed of emulation compared to full speed.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showGraphsCheckBox
{
  auto& entry = add(Config::GFX_SHOW_GRAPHS, "native.gfx_show_graphs", "OSD",
                    _trans("Show Performance Graphs"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows frametime graph along with statistics as a representation "
                                 "of emulation performance.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showSpeedColorsCheckBox
{
  auto& entry = add(Config::GFX_SHOW_SPEED_COLORS, "native.gfx_show_speed_colors", "OSD",
                    _trans("Show Speed Colors"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Changes the color of the FPS counter depending on emulation "
                                 "speed.\n\nIf unsure, leave this checked.");
}

// Settings/OnScreenDisplayPane.ui: performanceSampleWindowSpinBox
{
  auto& entry =
      add(Config::GFX_PERF_SAMP_WINDOW, "native.gfx_perf_samp_window", "OSD",
          _trans("Performance Sample Window (ms)"), Control::Number, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("The amount of time the FPS and VPS counters will sample over.\n\nThe higher the "
             "value, the more stable the FPS/VPS counter will be, but the slower it will be to "
             "update.\n\nIf unsure, leave this at 1000ms.");
  entry.descriptor.range = Range{0, 10000, 100, 1, 0, ""};
}

// Settings/OnScreenDisplayPane.ui: showMovieWindowCheckBox
{
  auto& entry = add(Config::MAIN_MOVIE_SHOW_OSD, "native.main_movie_show_osd", "OSD",
                    _trans("Show Movie Window"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows a window that can be filled with information related to "
                                 "movie recordings. The other options in this group determine what "
                                 "appears in the window. \n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showRerecordCounterCheckBox
{
  auto& entry = add(Config::MAIN_MOVIE_SHOW_RERECORD, "native.main_movie_show_rerecord", "OSD",
                    _trans("Show Rerecord Counter"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows how many times the input recording has been overwritten by "
                                 "using savestates.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showLagCounterCheckBox
{
  auto& entry = add(Config::MAIN_SHOW_LAG, "native.main_show_lag", "OSD",
                    _trans("Show Lag Counter"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows how many frames have occurred without controller inputs being checked. Resets "
             "to 1 when inputs are processed. \n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showFrameCounterCheckBox
{
  auto& entry = add(Config::MAIN_SHOW_FRAME_COUNT, "native.main_show_frame_count", "OSD",
                    _trans("Show Frame Counter"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows how many frames have passed. \n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showInputDisplayCheckBox
{
  auto& entry = add(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY, "native.main_movie_show_input_display",
                    "OSD", _trans("Show Input Display"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the controls currently being input.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showSystemClockCheckBox
{
  auto& entry = add(Config::MAIN_MOVIE_SHOW_RTC, "native.main_movie_show_rtc", "OSD",
                    _trans("Show System Clock"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows current system time.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showNetplayPingCheckBox
{
  auto& entry = add(Config::GFX_SHOW_NETPLAY_PING, "native.gfx_show_netplay_ping", "OSD",
                    _trans("Show NetPlay Ping"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows the player's maximum ping while playing on NetPlay.\n\nIf "
                                 "unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showNetplayChatCheckBox
{
  auto& entry = add(Config::GFX_SHOW_NETPLAY_MESSAGES, "native.gfx_show_netplay_messages", "OSD",
                    _trans("Show NetPlay Chat"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help = _trans("Shows chat messages, buffer changes, and desync alerts while "
                                 "playing NetPlay.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showStatisticsCheckBox
{
  auto& entry = add(Config::GFX_OVERLAY_STATS, "native.gfx_overlay_stats", "OSD",
                    _trans("Show Statistics"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows various rendering statistics.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showProjectionStatisticsCheckBox
{
  auto& entry =
      add(Config::GFX_OVERLAY_PROJ_STATS, "native.gfx_overlay_proj_stats", "OSD",
          _trans("Show Projection Statistics"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows various projection statistics.\n\nIf unsure, leave this unchecked.");
}

// Settings/OnScreenDisplayPane.ui: showXfbResolutionCheckBox
{
  auto& entry =
      add(Config::GFX_SHOW_INTERNAL_RESOLUTION, "native.gfx_show_internal_resolution", "OSD",
          _trans("Show XFB Resolution"), Control::Toggle, false, ApplyPolicy::Live);
  entry.descriptor.help =
      _trans("Shows the size of the emulated external frame buffer (XFB) in pixels, as a product "
             "of width and height.\n\nIf unsure, leave this unchecked.");
}

// Settings/AdvancedPane.ui: enableMmuCheckBox
{
  auto& entry = add(Config::MAIN_MMU, "native.main_mmu", "Advanced", _trans("Enable MMU"),
                    Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable MMU");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: pauseOnPanicCheckBox
{
  auto& entry = add(Config::MAIN_PAUSE_ON_PANIC, "native.main_pause_on_panic", "Advanced",
                    _trans("Pause on Panic"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Pause on Panic");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: accurateCpuCacheCheckBox
{
  auto& entry =
      add(Config::MAIN_ACCURATE_CPU_CACHE, "native.main_accurate_cpu_cache", "Advanced",
          _trans("Enable Write-Back Cache (slow)"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable Write-Back Cache (slow)");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: correctTimeDriftCheckBox
{
  auto& entry = add(Config::MAIN_CORRECT_TIME_DRIFT, "native.main_correct_time_drift", "Advanced",
                    _trans("Correct Time Drift"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Correct Time Drift");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: rushFramePresentationCheckBox
{
  auto& entry =
      add(Config::MAIN_RUSH_FRAME_PRESENTATION, "native.main_rush_frame_presentation", "Advanced",
          _trans("Rush Frame Presentation"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Rush Frame Presentation");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: smoothEarlyPresentationCheckBox
{
  auto& entry = add(Config::MAIN_SMOOTH_EARLY_PRESENTATION, "native.main_smooth_early_presentation",
                    "Advanced", _trans("Smooth Early Presentation"), Control::Toggle, false,
                    ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Smooth Early Presentation");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: vbiOverrideCheckBox
{
  auto& entry =
      add(Config::MAIN_VI_OVERCLOCK_ENABLE, "native.main_vi_overclock_enable", "Advanced",
          _trans("Enable VBI Frequency Override"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable VBI Frequency Override");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: memoryOverrideCheckBox
{
  auto& entry = add(Config::MAIN_RAM_OVERRIDE_ENABLE, "native.main_ram_override_enable", "Advanced",
                    _trans("Enable Emulated Memory Size Override"), Control::Toggle, false,
                    ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable Emulated Memory Size Override");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/AdvancedPane.ui: customRtcCheckBox
{
  auto& entry = add(Config::MAIN_CUSTOM_RTC_ENABLE, "native.main_custom_rtc_enable", "Advanced",
                    _trans("Enable Custom RTC"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable Custom RTC");
  entry.descriptor.netplay_safe = false;
  entry.descriptor.hardcore_safe = false;
}

// Settings/GameCubePane.ui: skipMainMenuCheckBox
{
  auto& entry = add(Config::MAIN_SKIP_IPL, "native.main_skip_ipl", "GameCube",
                    _trans("Skip Main Menu"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Skip Main Menu");
  entry.descriptor.netplay_safe = false;
}

// Settings/GameCubePane.ui: systemLanguageComboBox
{
  auto& entry = add(Config::MAIN_GC_LANGUAGE, "native.main_gc_language", "GameCube",
                    _trans("System Language"), Control::Choice, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("System Language");
  entry.descriptor.choices = {
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(0)), _trans("English")},
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(1)), _trans("German")},
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(2)), _trans("French")},
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(3)), _trans("Spanish")},
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(4)), _trans("Italian")},
      {ToValue(static_cast<SettingType<decltype(Config::MAIN_GC_LANGUAGE)>>(5)), _trans("Dutch")},
  };
  entry.descriptor.netplay_safe = false;
}

// Settings/GameCubePane.ui: gbaBiosLineEdit
{
  auto& entry = add(Config::MAIN_GBA_BIOS_PATH, "native.main_gba_bios_path", "GameCube",
                    _trans("BIOS"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("BIOS");
  entry.descriptor.netplay_safe = false;
}

// Settings/GameCubePane.ui: gbaSaveInRomPathCheckBox
{
  auto& entry = add(Config::MAIN_GBA_SAVES_IN_ROM_PATH, "native.main_gba_saves_in_rom_path",
                    "GameCube", _trans("Save in Same Directory as the ROM"), Control::Toggle, false,
                    ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Save in Same Directory as the ROM");
  entry.descriptor.netplay_safe = false;
}

// Settings/GameCubePane.ui: gbaSavesLineEdit
{
  auto& entry = add(Config::MAIN_GBA_SAVES_PATH, "native.main_gba_saves_path", "GameCube",
                    _trans("Saves"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Saves");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: pal60ModeCheckBox
{
  auto& entry =
      add(Config::SYSCONF_PAL60, "native.sysconf_pal60", "Wii", _trans("Use PAL60 Mode (EuRGB60)"),
          Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Use PAL60 Mode (EuRGB60)");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: screenSaverCheckBox
{
  auto& entry = add(Config::SYSCONF_SCREENSAVER, "native.sysconf_screensaver", "Wii",
                    _trans("Enable Screen Saver"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable Screen Saver");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: wiiLinkCheckBox
{
  auto& entry =
      add(Config::MAIN_WII_WIILINK_ENABLE, "native.main_wii_wiilink_enable", "Wii",
          _trans("Enable WiiConnect24 via WiiLink"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable WiiConnect24 via WiiLink");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: connectKeyboardCheckBox
{
  auto& entry = add(Config::MAIN_WII_KEYBOARD, "native.main_wii_keyboard", "Wii",
                    _trans("Connect USB Keyboard"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Connect USB Keyboard");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: systemLanguageComboBox
{
  auto& entry = add(Config::SYSCONF_LANGUAGE, "native.sysconf_language", "Wii",
                    _trans("System Language"), Control::Choice, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("System Language");
  entry.descriptor.choices = {
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(0)),
       _trans("Japanese")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(1)), _trans("English")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(2)), _trans("German")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(3)), _trans("French")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(4)), _trans("Spanish")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(5)), _trans("Italian")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(6)), _trans("Dutch")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(7)),
       _trans("Simplified Chinese")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(8)),
       _trans("Traditional Chinese")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_LANGUAGE)>>(9)), _trans("Korean")},
  };
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: soundModeComboBox
{
  auto& entry = add(Config::SYSCONF_SOUND_MODE, "native.sysconf_sound_mode", "Wii", _trans("Sound"),
                    Control::Choice, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Sound");
  entry.descriptor.choices = {
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_SOUND_MODE)>>(0)), _trans("Mono")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_SOUND_MODE)>>(1)),
       _trans("Stereo")},
      {ToValue(static_cast<SettingType<decltype(Config::SYSCONF_SOUND_MODE)>>(2)),
       _trans("Surround")},
  };
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: insertSdCardCheckBox
{
  auto& entry = add(Config::MAIN_WII_SD_CARD, "native.main_wii_sd_card", "Wii",
                    _trans("Insert SD Card"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Insert SD Card");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: allowSdWritesCheckBox
{
  auto& entry =
      add(Config::MAIN_ALLOW_SD_WRITES, "native.main_allow_sd_writes", "Wii",
          _trans("Allow Writes to SD Card"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Allow Writes to SD Card");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: sdCardPathLineEdit
{
  auto& entry = add(Config::MAIN_WII_SD_CARD_IMAGE_PATH, "native.main_wii_sd_card_image_path",
                    "Wii", _trans("SD Card Path"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("SD Card Path");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: syncSdFolderCheckBox
{
  auto& entry =
      add(Config::MAIN_WII_SD_CARD_ENABLE_FOLDER_SYNC, "native.main_wii_sd_card_enable_folder_sync",
          "Wii", _trans("Automatically Sync with Folder"), Control::Toggle, false,
          ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Automatically Sync with Folder");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: sdSyncFolderLineEdit
{
  auto& entry =
      add(Config::MAIN_WII_SD_CARD_SYNC_FOLDER_PATH, "native.main_wii_sd_card_sync_folder_path",
          "Wii", _trans("SD Sync Folder"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("SD Sync Folder");
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: irSensitivitySlider
{
  auto& entry = add(Config::SYSCONF_SENSOR_BAR_SENSITIVITY, "native.sysconf_sensor_bar_sensitivity",
                    "Wii", _trans("IR Sensitivity"), Control::Number, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("IR Sensitivity");
  entry.descriptor.range =
      Range{(1) * double(1), (5) * double(1), (1) * double(1), 1.0 / (1), 0, ""};
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: speakerVolumeSlider
{
  auto& entry = add(Config::SYSCONF_SPEAKER_VOLUME, "native.sysconf_speaker_volume", "Wii",
                    _trans("Speaker Volume"), Control::Number, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Speaker Volume");
  entry.descriptor.range =
      Range{(0) * double(1), (127) * double(1), (1) * double(1), 1.0 / (1), 0, ""};
  entry.descriptor.netplay_safe = false;
}

// Settings/WiiPane.ui: wiiRemoteRumbleCheckBox
{
  auto& entry = add(Config::SYSCONF_WIIMOTE_MOTOR, "native.sysconf_wiimote_motor", "Wii",
                    _trans("Enable Rumble"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Enable Rumble");
  entry.descriptor.netplay_safe = false;
}

// Settings/PathPane.ui: recursivePathsCheckBox
{
  auto& entry = add(Config::MAIN_RECURSIVE_ISO_PATHS, "native.main_recursive_iso_paths", "Paths",
                    _trans("Search Subfolders"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Search Subfolders");
}

// Settings/PathPane.ui: defaultIsoLineEdit
{
  auto& entry = add(Config::MAIN_DEFAULT_ISO, "native.main_default_iso", "Paths",
                    _trans("Default ISO"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Default ISO");
}

// Settings/PathPane.ui: nandRootLineEdit
{
  auto& entry = add(Config::MAIN_FS_PATH, "native.main_fs_path", "Paths", _trans("Wii NAND Root"),
                    Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Wii NAND Root");
}

// Settings/PathPane.ui: dumpPathLineEdit
{
  auto& entry = add(Config::MAIN_DUMP_PATH, "native.main_dump_path", "Paths", _trans("Dump Path"),
                    Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Dump Path");
}

// Settings/PathPane.ui: loadPathLineEdit
{
  auto& entry = add(Config::MAIN_LOAD_PATH, "native.main_load_path", "Paths", _trans("Load Path"),
                    Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Load Path");
}

// Settings/PathPane.ui: resourcePackPathLineEdit
{
  auto& entry = add(Config::MAIN_RESOURCEPACK_PATH, "native.main_resourcepack_path", "Paths",
                    _trans("Resource Pack Path"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Resource Pack Path");
}

// Settings/PathPane.ui: wfsPathLineEdit
{
  auto& entry = add(Config::MAIN_WFS_PATH, "native.main_wfs_path", "Paths", _trans("WFS Path"),
                    Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("WFS Path");
}

// Settings/MemoryCardSyncPane.ui: enabledCheckBox
{
  auto& entry =
      add(Config::MAIN_MEMCARD_SYNC_ENABLED, "native.main_memcard_sync_enabled", "MemoryCards",
          _trans("Sync Memory Cards"), Control::Toggle, false, ApplyPolicy::IdleOnly);
  entry.descriptor.help = _trans("Sync Memory Cards");
}

// Settings/MemoryCardSyncPane.ui: storageLineEdit
{
  auto& entry = add(Config::MAIN_MEMCARD_SYNC_REMOTE_URI, "native.main_memcard_sync_remote_uri",
                    "MemoryCards", _trans("Storage"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Storage");
}

// Settings/MemoryCardSyncPane.ui: accessKeyIdLineEdit
{
  auto& entry =
      add(Config::MAIN_MEMCARD_SYNC_ACCESS_KEY_ID, "native.main_memcard_sync_access_key_id",
          "MemoryCards", _trans("Access Key ID"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Access Key ID");
}

// Settings/MemoryCardSyncPane.ui: deviceNameLineEdit
{
  auto& entry =
      add(Config::MAIN_MEMCARD_SYNC_DEVICE_NAME, "native.main_memcard_sync_device_name",
          "MemoryCards", _trans("Device Name"), Control::Path, false, ApplyPolicy::NextBoot);
  entry.descriptor.help = _trans("Device Name");
}
