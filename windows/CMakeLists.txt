cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME "flutter_soloud")
project(${PROJECT_NAME} LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_NAME "${PROJECT_NAME}_plugin")

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# Include common plugin sources and SoLoud configuration
include(${SRC_DIR}/CMakeLists.txt)

# Fork-specific sources (buses, reverbs, HRTF, LibPD)
list(APPEND PLUGIN_SOURCES
  "${SRC_DIR}/synth/soloud_libpd.cpp"
  "${SRC_DIR}/synth/pd_bridge.cpp"
"${SRC_DIR}/filters/dattorro.cpp"
  "${SRC_DIR}/filters/soloud_reverbsc.cpp"
  "${SRC_DIR}/filters/soloud_convolutionfilter.cpp"
  "${SRC_DIR}/filters/soloud_dattorro_filter.cpp"
  "${SRC_DIR}/filters/soloud_hrtf_filter.cpp"
  "${SRC_DIR}/filters/fftconvolver/AudioFFT.cpp"
  "${SRC_DIR}/filters/fftconvolver/Utilities.cpp"
  "${SRC_DIR}/filters/fftconvolver/FFTConvolver.cpp"
)

# ─── Build libpd from source (static) ───────────────────────────────────────
# libpd's CMake requires pthreads on MSVC. We use the vcpkg PThreads4W package.
# Override these on the CMake command line if your vcpkg lives elsewhere, e.g.
#   -DPTHREADS_ROOT=C:/path/to/vcpkg/installed/x64-windows
if(NOT DEFINED PTHREADS_ROOT)
  set(PTHREADS_ROOT "C:/Users/atsog/dev/vcpkg/installed/x64-windows")
endif()
set(PTHREADS_LIB         "${PTHREADS_ROOT}/lib/pthreadVC3.lib" CACHE FILEPATH "" FORCE)
set(PTHREADS_INCLUDE_DIR "${PTHREADS_ROOT}/include"            CACHE PATH     "" FORCE)
set(PTHREADS_DLL         "${PTHREADS_ROOT}/bin/pthreadVC3.dll")

# Ensure libpd (C sources) compiles with the same static MSVC runtime (/MT,/MTd)
# as the plugin, so the static archive links without __imp_* CRT mismatches.
# libpd doesn't honor CMAKE_MSVC_RUNTIME_LIBRARY here (policy not NEW + C flags),
# so patch CMAKE_C_FLAGS_* directly before add_subdirectory, mirroring the
# /MD->/MT replacement the plugin applies to its C++ flags below.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
  foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_${CONFIG} "${CMAKE_C_FLAGS_${CONFIG}}")
  endforeach()
  string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()

set(LIBPD_STATIC ON  CACHE BOOL "" FORCE)
set(LIBPD_SHARED OFF CACHE BOOL "" FORCE)
set(PD_UTILS  ON  CACHE BOOL "" FORCE)
set(PD_EXTRA  ON  CACHE BOOL "" FORCE)
set(PD_MULTI  OFF CACHE BOOL "" FORCE)
set(PD_LOCALE ON  CACHE BOOL "" FORCE)

add_subdirectory(
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libpd"
  "${CMAKE_CURRENT_BINARY_DIR}/libpd_build"
  EXCLUDE_FROM_ALL
)

add_library(${PLUGIN_NAME} SHARED
  ${PLUGIN_SOURCES}
)
apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)

target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_compile_definitions(${PLUGIN_NAME} PRIVATE SIGNALSMITH_USE_PFFFT)
target_compile_definitions(${PLUGIN_NAME} PRIVATE NOMINMAX)
target_include_directories(${PLUGIN_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ─── LibPD linkage ───────────────────────────────────────────────────────────
# libpd_static carries its include dirs (libpd_wrapper, pure-data/src) PUBLIC,
# so soloud_libpd.cpp finds z_libpd.h. Ws2_32 is needed because pd's s_net.c
# uses Winsock and the static lib (unlike the shared target) doesn't pull it in.
# libpd headers (z_libpd.h, m_pd.h). libpd_static exposes these transitively,
# but we add them explicitly so soloud_libpd.cpp/pd_bridge.cpp find them
# regardless of link order or a stale CMake cache.
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libpd/libpd_wrapper"
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libpd/pure-data/src"
)

# PD_INTERNAL flips m_pd.h's EXTERN from __declspec(dllimport) (the default,
# which expects an import lib) to dllexport, so the libpd_* references in
# soloud_libpd.cpp / pd_bridge.cpp resolve as plain symbols from the static lib.
target_compile_definitions(${PLUGIN_NAME} PRIVATE PD_INTERNAL=1)
target_link_libraries(${PLUGIN_NAME} PRIVATE libpd_static Ws2_32 "${PTHREADS_LIB}")

# pthreadVC3.dll must sit next to the executable at runtime.
add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PTHREADS_DLL}"
    "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/pthreadVC3.dll"
)

if(NOT DEFINED ENV{NO_XIPH_LIBS})
    message("NO_XIPH_LIBS has not been set. Linking Opus and Ogg libraries!")

    if(MSVC)
      # Enable static linking of the MSVC runtime
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      # For the plugin
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()

    # Add opus library
    set(OPUS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/opus.lib)
    # Link the library to your target
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${OPUS_LIBRARY})

    # Add ogg library
    set(OGG_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/ogg.lib)
    # Link the library to your target
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${OGG_LIBRARY})

    # Add vorbis library
    set(VORBIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/vorbis.lib)
    set(VORBISFILE_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/vorbisfile.lib)
    # Link the library to your target
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${VORBIS_LIBRARY})
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${VORBISFILE_LIBRARY})

    # Add FLAC library
    set(FLAC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libs/FLAC.lib)
    # Link the library to your target
    target_link_libraries(${PLUGIN_NAME} PRIVATE ${FLAC_LIBRARY})

    # Set the ogg, opus, vorbis, vorbisfile and flac include directory
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

    # Copy DLLs to output directory for both Debug and Release configurations
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/opus.dll"
        "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/opus.dll"
    )

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/ogg.dll"
        "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/ogg.dll"
    )

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/vorbis.dll"
        "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/vorbis.dll"
    )

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/vorbisfile.dll"
        "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/vorbisfile.dll"
    )

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/FLAC.dll"
        "${CMAKE_BINARY_DIR}/runner/$<CONFIG>/FLAC.dll"
    )
else()
    message("NO_XIPH_LIBS has been set. Not linking Opus and Ogg libraries!")
    add_definitions(-DNO_XIPH_LIBS)
endif()

if(MSVC)
  # Force optimize flags for all configurations
  string(REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "/Od" "/Ox" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Od" "/Ox" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  
  # Remove RTC1 from debug flags
  string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  
  # Use static runtime for all configurations
  foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_${CONFIG} "${CMAKE_CXX_FLAGS_${CONFIG}}")
    string(REPLACE "/MDd" "/MT" CMAKE_CXX_FLAGS_${CONFIG} "${CMAKE_CXX_FLAGS_${CONFIG}}")
  endforeach()

  # Advanced optimization flags
  set(OPTIMIZATION_FLAGS
    /GL     # Whole program optimization
    /Gy     # Function-level linking
    /Oi     # Generate intrinsic functions
    /Ot     # Favor fast code
    /Ox     # Full optimization
    /fp:fast # Fast floating-point model
    /arch:SSE2  # Use SSE2 instructions
    /Qpar   # Auto-parallelize loops
    /O2     # Maximize speed
    /RTC-   # Disable runtime checks explicitly
  )

  # Add debug info generation for release builds
  set(OPTIMIZATION_FLAGS "${OPTIMIZATION_FLAGS} /Zi")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
  
  # Apply optimization flags to all configurations
  foreach(FLAG ${OPTIMIZATION_FLAGS})
    target_compile_options(${PLUGIN_NAME} PRIVATE ${FLAG})
  endforeach()

  # Linker optimization flags
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG /OPT:REF /OPT:ICF")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LTCG /OPT:REF /OPT:ICF")

  # Replace existing compile options with optimized ones
  target_compile_options(${PLUGIN_NAME} PRIVATE 
    /W4     # Warning level 4 
    /WX-    # Disable warnings as errors
    ${OPTIMIZATION_FLAGS}
  )

  # Create a custom target for copying PDB files
  # This target will only be part of the default build for configurations other than Profile.
  add_custom_target(copy_pdbs $<IF:$<NOT:$<CONFIG:Profile>>,ALL,>>
    # copy to "build" dir
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE_DIR:${PLUGIN_NAME}>/${PLUGIN_NAME}.pdb"
      "${CMAKE_BINARY_DIR}/../../${PLUGIN_NAME}.pdb"
    DEPENDS ${PLUGIN_NAME}
    COMMENT "Copying PDB files to build directory"
    VERBATIM
  )

endif()

# List of absolute paths to libraries that should be bundled with the plugin.
set(flutter_soloud_bundled_libraries
  $<TARGET_FILE:${PLUGIN_NAME}>
  PARENT_SCOPE
)