cmake_minimum_required(VERSION 3.16)
project(cef_bridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CEF root directory (downloaded by scripts/download-cef.ts)
if(NOT DEFINED CEF_ROOT)
    set(CEF_ROOT "cef/linux64")
endif()
if(IS_ABSOLUTE "${CEF_ROOT}")
    set(CEF_ROOT_ABS "${CEF_ROOT}")
else()
    set(CEF_ROOT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${CEF_ROOT}")
endif()

# Wrapper library
set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")
if(NOT EXISTS "${WRAPPER_LIBRARY}")
    message(FATAL_ERROR "Wrapper library not found at ${WRAPPER_LIBRARY}. Run 'bun run download:cef' first.")
endif()

# Include directories
include_directories(
    ${CEF_ROOT_ABS}
    ${CEF_ROOT_ABS}/include
    ${CEF_ROOT_ABS}/libcef_dll
    ${CEF_ROOT_ABS}/libcef_dll/wrapper
)

# Common compile options (helper - no exceptions, no rtti)
set(COMPILE_FLAGS_HELPER
    -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
    -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
    -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
    -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64 -fno-exceptions
    -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
    -Wsign-compare -Wno-literal-suffix -Wno-narrowing
    -Wno-attributes -Wno-array-bounds -Wno-stringop-overflow
    -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
    -D_FORTIFY_SOURCE=2
)

# Addon compile options (with exceptions for napi)
set(COMPILE_FLAGS_ADDON
    -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
    -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
    -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
    -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64
    -fexceptions -frtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
    -Wsign-compare -Wno-literal-suffix -Wno-narrowing
    -Wno-attributes -Wno-array-bounds -Wno-stringop-overflow
    -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
    -D_FORTIFY_SOURCE=2
)

set(COMPILE_DEFS
    -DCEF_USE_SANDBOX
    -D_FILE_OFFSET_BITS=64
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
)

# Find node - use pkg-config or manual path
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(NODE nodejs)
endif()

# Fallback: find node headers manually
if(NOT NODE_FOUND)
    execute_process(
        COMMAND node -p "require('path').join(require('path').dirname(require('path').dirname(process.execPath)), 'include', 'node')"
        OUTPUT_VARIABLE NODE_INCLUDE_DIRS
    )
    string(STRIP "${NODE_INCLUDE_DIRS}" NODE_INCLUDE_DIRS)
    message(STATUS "Node include: ${NODE_INCLUDE_DIRS}")
    set(NODE_FOUND TRUE)
endif()

message(STATUS "NODE_INCLUDE_DIRS: ${NODE_INCLUDE_DIRS}")

if(NOT NODE_FOUND)
    message(FATAL_ERROR "Node.js development files not found")
endif()

# -----------------------------------------------------------------------------
# Helper executable (CEF subprocess)
# -----------------------------------------------------------------------------
add_executable(cef_bridge_helper src/cef_bridge.cc)
target_compile_options(cef_bridge_helper PRIVATE ${COMPILE_FLAGS_HELPER})
target_compile_definitions(cef_bridge_helper PRIVATE ${COMPILE_DEFS} -DCEF_HELPER_BINARY)
target_link_libraries(cef_bridge_helper
    ${CEF_ROOT_ABS}/Release/libcef.so
    ${WRAPPER_LIBRARY}
    X11
)

# Copy helper to output directory
add_custom_command(TARGET cef_bridge_helper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cef_bridge_helper> $<TARGET_FILE_DIR:cef_bridge_helper>
)

# -----------------------------------------------------------------------------
# Node addon
# -----------------------------------------------------------------------------
add_library(cef_bridge SHARED src/cef_bridge.cc)
set_target_properties(cef_bridge PROPERTIES PREFIX "" SUFFIX ".node")
target_compile_options(cef_bridge PRIVATE ${COMPILE_FLAGS_ADDON})
target_compile_definitions(cef_bridge PRIVATE ${COMPILE_DEFS})
target_include_directories(cef_bridge PRIVATE ${NODE_INCLUDE_DIRS})
target_link_libraries(cef_bridge
    ${CEF_ROOT_ABS}/Release/libcef.so
    ${WRAPPER_LIBRARY}
    X11
    dl
)
target_link_options(cef_bridge PRIVATE "-Wl,-rpath,\$ORIGIN")

# Copy all CEF distribution files to build/ directory
# CEF requires resources to be in same directory as libcef.so
add_custom_command(TARGET cef_bridge POST_BUILD
    # Resources directory with icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CEF_ROOT_ABS}/Resources ${CMAKE_BINARY_DIR}/Resources
    # ICU data file needs to be directly in build/ for CEF's internal finder
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/icudtl.dat ${CMAKE_BINARY_DIR}/icudtl.dat
    # .pak files also need to be in build/ root for CEF's internal path resolution
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/chrome_100_percent.pak ${CMAKE_BINARY_DIR}/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/chrome_200_percent.pak ${CMAKE_BINARY_DIR}/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/resources.pak ${CMAKE_BINARY_DIR}/resources.pak
    # Chrome sandbox
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/chrome-sandbox ${CMAKE_BINARY_DIR}/chrome-sandbox
    # Main CEF library
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libcef.so ${CMAKE_BINARY_DIR}/libcef.so
    # Graphics libraries
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libEGL.so ${CMAKE_BINARY_DIR}/libEGL.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libGLESv2.so ${CMAKE_BINARY_DIR}/libGLESv2.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libvk_swiftshader.so ${CMAKE_BINARY_DIR}/libvk_swiftshader.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libvulkan.so.1 ${CMAKE_BINARY_DIR}/libvulkan.so.1
    # V8 snapshot
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/v8_context_snapshot.bin ${CMAKE_BINARY_DIR}/v8_context_snapshot.bin
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/vk_swiftshader_icd.json ${CMAKE_BINARY_DIR}/vk_swiftshader_icd.json
    # Helper executable
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cef_bridge_helper> ${CMAKE_BINARY_DIR}/cef_bridge_helper
)