enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND} "--output-on-failure")

string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY "/Tests")

add_executable(tests EXCLUDE_FROM_ALL UnitTestsMain.cpp StubHost.cpp)
set_target_properties(tests PROPERTIES FOLDER Tests)
target_link_libraries(tests PRIVATE fmt::fmt gtest::gtest core uicommon)
add_test(NAME tests COMMAND tests)
add_custom_command(TARGET tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Sys"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Data/Sys" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Sys"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Source/UnitTests/VideoCommon/PostProcessing/TestData" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestData"
)

# Copy librashader into the test binary's directory so LibrashaderParameters.EnumerateFixturePreset
# can execute. File::GetBundleDirectory() falls back to the executable's own directory when there is
# no bundle, so fabricating Contents/Frameworks next to the non-bundle tests binary makes the loader
# resolve correctly. This is a test-harness convenience, not a fix for macOS dolphin-nogui, whose
# inability to load librashader is documented at DolphinQt/CMakeLists.txt:697-698.
#
# Not gated on ENABLE_VULKAN: none of the tests that need the library touch Vulkan, and gating it
# would silently skip LibrashaderParameters.EnumerateFixturePreset in a build with Vulkan off.
if(APPLE)
  set(LIBRASHADER_DYLIB "${CMAKE_SOURCE_DIR}/Externals/librashader/lib/macos-arm64/librashader.dylib")
  if(EXISTS "${LIBRASHADER_DYLIB}")
    add_custom_command(TARGET tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Contents/Frameworks"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${LIBRASHADER_DYLIB}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Contents/Frameworks/librashader.dylib"
    )
  endif()
endif()

# Copy librashader.dll next to the test binary so EnumerateFixturePreset can execute on Windows. The
# existing copy into Binaries (Source/Core/CMakeLists.txt:104-126) does not cover Binaries/Tests.
# Not gated on ENABLE_VULKAN, for the same reason as the APPLE branch above.
if(WIN32)
  set(LIBRASHADER_DLL "${CMAKE_SOURCE_DIR}/Externals/librashader/lib/windows-x64/librashader.dll")
  if(EXISTS "${LIBRASHADER_DLL}")
    add_custom_command(TARGET tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${LIBRASHADER_DLL}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/librashader.dll"
    )
  endif()
endif()

add_dependencies(unittests tests)

macro(add_dolphin_test target)
  add_library(${target} OBJECT ${ARGN})
  target_link_libraries(${target} PUBLIC fmt::fmt gtest::gtest PRIVATE core uicommon)
  target_link_libraries(tests PRIVATE ${target})
endmacro()

add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
