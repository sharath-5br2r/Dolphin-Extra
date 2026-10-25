# Copyright 2026 Dolphin Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

function(dolphin_check_qt_form_layout_boundaries source_root)
  # These form-backed controllers deliberately create layouts for runtime-generated content rather
  # than for the permanent shell owned by their .ui file.
  set(dynamic_layout_exceptions
    RiivolutionBootWidget.cpp
    Settings/TriforcePane.cpp
    TAS/TASInputWindow.cpp
    TAS/WiiTASInputWindow.cpp
  )

  # CONFIGURE_DEPENDS makes a newly added same-name .ui/.cpp pair join the boundary check without
  # requiring a second hand-maintained source list.
  file(GLOB_RECURSE qt_form_files CONFIGURE_DEPENDS "${source_root}/*.ui")
  foreach(qt_form_file IN LISTS qt_form_files)
    get_filename_component(qt_form_directory "${qt_form_file}" DIRECTORY)
    get_filename_component(qt_form_name "${qt_form_file}" NAME_WE)
    set(qt_controller_file "${qt_form_directory}/${qt_form_name}.cpp")
    if(NOT EXISTS "${qt_controller_file}")
      continue()
    endif()

    file(RELATIVE_PATH qt_controller_relative "${source_root}" "${qt_controller_file}")
    if(qt_controller_relative IN_LIST dynamic_layout_exceptions)
      continue()
    endif()

    file(READ "${qt_controller_file}" qt_controller_source)
    string(REGEX MATCH "new[ \t\r\n]+Q[A-Za-z0-9_]*Layout" qt_direct_layout
                 "${qt_controller_source}")
    if(qt_direct_layout)
      message(FATAL_ERROR
        "${qt_controller_relative} constructs ${qt_direct_layout}, but its same-name .ui file "
        "owns the permanent layout. Move the layout to the form, or document a runtime-generated "
        "layout in dynamic_layout_exceptions.")
    endif()
  endforeach()
endfunction()
