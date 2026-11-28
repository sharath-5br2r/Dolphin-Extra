cmake_minimum_required(VERSION 3.13)

# version number
set(DOLPHIN_VERSION_MAJOR "2609")
set(DOLPHIN_VERSION_MINOR "0")
set(DOLPHIN_VERSION_PATCH ${DOLPHIN_WC_REVISION})

# for revision info
if(GIT_FOUND)
  # defines DOLPHIN_WC_REVISION
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      OUTPUT_VARIABLE DOLPHIN_WC_REVISION
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  # defines DOLPHIN_WC_SHORT_HASH (10 characters)
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --short=10 HEAD
      OUTPUT_VARIABLE DOLPHIN_WC_SHORT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  # check dirty state
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} status --porcelain
      OUTPUT_VARIABLE DOLPHIN_WC_STATUS
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(DOLPHIN_WC_STATUS)
    set(DOLPHIN_WC_DIRTY "-dirty")
  else()
    set(DOLPHIN_WC_DIRTY "")
  endif()

  # defines DOLPHIN_WC_DESCRIBE using latest 4-digit release tag + diverging commit count
  if(NOT DOLPHIN_WC_DESCRIBE)
    execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${GIT_EXECUTABLE} tag --list "[0-9][0-9][0-9][0-9]" --sort=-v:refname
        OUTPUT_VARIABLE DOLPHIN_WC_TAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(DOLPHIN_WC_TAGS)
      string(REGEX REPLACE "\n.*" "" DOLPHIN_WC_LATEST_TAG "${DOLPHIN_WC_TAGS}")
      execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
          COMMAND ${GIT_EXECUTABLE} rev-list --count "${DOLPHIN_WC_LATEST_TAG}..HEAD"
          OUTPUT_VARIABLE DOLPHIN_WC_COMMITS_SINCE_TAG
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
    endif()
    if(DOLPHIN_WC_LATEST_TAG AND NOT "${DOLPHIN_WC_COMMITS_SINCE_TAG}" STREQUAL "")
      set(DOLPHIN_WC_DESCRIBE "${DOLPHIN_WC_LATEST_TAG}-${DOLPHIN_WC_COMMITS_SINCE_TAG}${DOLPHIN_WC_DIRTY}")
    elseif(DOLPHIN_WC_SHORT_HASH)
      set(DOLPHIN_WC_DESCRIBE "${DOLPHIN_VERSION_MAJOR}-${DOLPHIN_WC_SHORT_HASH}${DOLPHIN_WC_DIRTY}")
    else()
      set(DOLPHIN_WC_DESCRIBE "${DOLPHIN_VERSION_MAJOR}.${DOLPHIN_VERSION_MINOR}")
    endif()
  endif()

  # defines DOLPHIN_WC_BRANCH
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
      OUTPUT_VARIABLE DOLPHIN_WC_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  # defines DOLPHIN_WC_COMMITS_AHEAD_MASTER
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD ^master
      OUTPUT_VARIABLE DOLPHIN_WC_COMMITS_AHEAD_MASTER
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  # defines DOLPHIN_WC_TAG
  execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} COMMAND ${GIT_EXECUTABLE} describe --exact-match HEAD
    OUTPUT_VARIABLE DOLPHIN_WC_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()

string(TIMESTAMP DOLPHIN_WC_BUILD_DATE "%Y-%m-%d" UTC)

# If Dolphin is not built from a Git repository, default the version info to
# reasonable values.
if(NOT DOLPHIN_WC_REVISION)
  set(DOLPHIN_WC_DESCRIBE "${DOLPHIN_VERSION_MAJOR}.${DOLPHIN_VERSION_MINOR}")
  set(DOLPHIN_WC_REVISION "${DOLPHIN_WC_DESCRIBE} (no further info)")
  set(DOLPHIN_WC_BRANCH "master")
  set(DOLPHIN_WC_COMMITS_AHEAD_MASTER 0)
endif()

# If this is a tag (i.e. a release), then set the current patch version and
# the number of commits ahead to zero.
if(DOLPHIN_WC_TAG)
  set(DOLPHIN_VERSION_PATCH "0")
  set(DOLPHIN_WC_COMMITS_AHEAD_MASTER 0)
endif()

function(configure_source_file path)
  configure_file(
    "${PROJECT_SOURCE_DIR}/${path}.in"
    "${PROJECT_BINARY_DIR}/${path}.tmp"
  )

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_BINARY_DIR}/${path}.tmp" "${PROJECT_BINARY_DIR}/${path}")

  file(REMOVE "${PROJECT_BINARY_DIR}/${path}.tmp")
endfunction()

configure_source_file("Source/Core/Common/scmrev.h")

if(APPLE)
  configure_source_file("Source/Core/VersionInfo.plist")
endif()

if(LINUX)
  configure_source_file("Distribution/flatpak/org.DolphinEmu.dolphin-emu.metainfo.xml")
endif()
