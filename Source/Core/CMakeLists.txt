add_subdirectory(AudioCommon)
add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(DiscIO)
add_subdirectory(InputCommon)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
add_subdirectory(VideoBackends)

if(ENABLE_NOGUI)
  add_subdirectory(DolphinNoGUI)
endif()

if(ENABLE_CLI_TOOL)
  add_subdirectory(DolphinTool)
endif()

if(ENABLE_QT)
  add_subdirectory(DolphinQt)
endif()

if (APPLE OR WIN32)
  add_subdirectory(UpdaterCommon)
endif()

if (APPLE AND ENABLE_AUTOUPDATE)
  add_subdirectory(MacUpdater)
endif()

if (WIN32 AND ENABLE_AUTOUPDATE)
  add_subdirectory(WinUpdater)
endif()

if (APPLE AND ENABLE_QT)
  set(DOLPHIN_MAC_BUNDLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Dolphin.app")

  add_custom_target(build_final_bundle ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
    ${DOLPHIN_MAC_BUNDLE}
    
    COMMAND cp -R
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DolphinQt.app
    ${DOLPHIN_MAC_BUNDLE}

    # HACK: The Updater does not support setting the executable bit on new files,
    # so don't use the new executable name, and instead continue to use "Dolphin".
    COMMAND ${CMAKE_COMMAND} -E rename
    ${DOLPHIN_MAC_BUNDLE}/Contents/MacOS/DolphinQt
    ${DOLPHIN_MAC_BUNDLE}/Contents/MacOS/Dolphin

    COMMAND plutil
    -replace CFBundleExecutable -string Dolphin
    ${DOLPHIN_MAC_BUNDLE}/Contents/Info.plist
    
    DEPENDS dolphin-emu)
  
  if (ENABLE_AUTOUPDATE)
    add_dependencies(build_final_bundle MacUpdater)

    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers"

      COMMAND cp -R
      "$<TARGET_BUNDLE_DIR:MacUpdater>"
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")

    if (MACOS_CODE_SIGNING)
      add_custom_command(TARGET build_final_bundle
        POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
        "-t"
        "${MACOS_CODE_SIGNING_IDENTITY}"
        "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")
    endif()
  endif()

  if (MACOS_CODE_SIGNING)
    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
      "-t"
      "-e" "${CMAKE_SOURCE_DIR}/Source/Core/DolphinQt/DolphinEmu$<$<CONFIG:Debug>:Debug>.entitlements"
      "${MACOS_CODE_SIGNING_IDENTITY}"
      "${DOLPHIN_MAC_BUNDLE}")
  endif()
endif()

# Copy librashader.dll to the build output directory on Windows for all frontend targets.
# Both dolphin-emu (Qt) and dolphin-nogui need the DLL, whatever backends are enabled: on Windows
# librashader drives D3D11, D3D12 and OpenGL as well as Vulkan, so this is deliberately not gated
# on ENABLE_VULKAN. It cannot be -- a build with Vulkan off would ship no DLL and then report
# librashader as simply unavailable, which is a legitimate state and so would go unnoticed.
# Neither frontend overrides RUNTIME_OUTPUT_DIRECTORY, so both land in the global
# CMAKE_RUNTIME_OUTPUT_DIRECTORY (${CMAKE_BINARY_DIR}/Binaries, set in the root CMakeLists);
# one copy into that directory therefore serves every frontend.
#
# This must not use add_custom_command(TARGET ...), which requires the target to have been
# created in the *same* directory as the call: both frontends are created in subdirectories,
# so that form fails configure with "TARGET '...' was not created in this directory".
# if(TARGET) is global while add_custom_command(TARGET) is directory-scoped, so the guard
# passes and the command then hard-errors.
if(WIN32)
  set(LIBRASHADER_DLL "${CMAKE_SOURCE_DIR}/Externals/librashader/lib/windows-x64/librashader.dll")
  if(EXISTS "${LIBRASHADER_DLL}")
    if(TARGET dolphin-emu OR TARGET dolphin-nogui)
      add_custom_target(librashader-dll ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIBRASHADER_DLL}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMENT "Copying librashader.dll to the runtime output directory")
      # add_dependencies is not directory-scoped, so it may name targets from subdirectories.
      foreach(frontend dolphin-emu dolphin-nogui)
        if(TARGET ${frontend})
          add_dependencies(${frontend} librashader-dll)
        endif()
      endforeach()
    endif()
    # Windows has no install(TARGETS ...) rule for these executables yet, but keep the install
    # rule for when one is added.
    install(FILES "${LIBRASHADER_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
  else()
    message(STATUS "librashader.dll not found; the librashader post-processing renderer will be unavailable.")
  endif()
endif()
