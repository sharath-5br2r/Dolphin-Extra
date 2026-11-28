set(LIBMGBA_ONLY ON)
set(USE_LZMA ON)
add_subdirectory(mgba EXCLUDE_FROM_ALL)

target_compile_definitions(mgba PUBLIC HAVE_CRC32 ENABLE_VFS ENABLE_DIRECTORIES)
target_link_libraries(mgba ZLIB::ZLIB)

if(NOT MSVC)
  target_compile_options(mgba PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()
target_compile_options(mgba PRIVATE $<$<C_COMPILER_ID:Clang>:-Wno-incompatible-pointer-types>)

if(ANDROID)
  target_compile_definitions(mgba PUBLIC ENABLE_VFS_FD)
  target_compile_definitions(mgba PRIVATE -Dfutimes=futimens)
endif()

add_library(mGBA::mgba ALIAS mgba)
