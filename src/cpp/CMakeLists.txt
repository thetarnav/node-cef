cmake_minimum_required(VERSION 3.16)
project(cef_host)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CEF root directory (downloaded by scripts/download-cef.ts)
# Override with -DCEF_ROOT=/path/to/cef on cmake command line if needed
if(NOT DEFINED CEF_ROOT)
    set(CEF_ROOT "../../../cef/linux64")
endif()
if(IS_ABSOLUTE "${CEF_ROOT}")
    set(CEF_ROOT_ABS "${CEF_ROOT}")
else()
    set(CEF_ROOT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${CEF_ROOT}")
endif()

# Wrapper library (built from source in CEF download)
set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/libcef_dll_wrapper.a")
if(NOT EXISTS "${WRAPPER_LIBRARY}")
    message(FATAL_ERROR "Wrapper library not found at ${WRAPPER_LIBRARY}. Run 'bun run download:cef' first.")
endif()

# Include directories
include_directories(
    ${CEF_ROOT_ABS}
    ${CEF_ROOT_ABS}/libcef_dll
    ${CEF_ROOT_ABS}/libcef_dll/wrapper
)

# Source files
set(SOURCES
    cefsimple_linux.cc
    simple_app.cc
    simple_handler.cc
    simple_handler_linux.cc
    ipc_client.cpp
    render_handler.cc
)

add_executable(cef_host ${SOURCES})

# Compiler flags
target_compile_options(cef_host PRIVATE
    -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
    -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
    -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
    -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64 -fno-exceptions
    -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
    -Wsign-compare -Wno-undefined-var-template -Wno-literal-suffix -Wno-narrowing
    -Wno-attributes -Wno-array-bounds -Wno-stringop-overflow
    -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
    -D_FORTIFY_SOURCE=2
)

target_compile_definitions(cef_host PRIVATE
    -DCEF_USE_SANDBOX
    -D_FILE_OFFSET_BITS=64
    -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS
)

target_link_options(cef_host PRIVATE "-Wl,-rpath,\$ORIGIN")

# Link against CEF wrapper and libcef (order matters: libcef.so first, then wrapper)
target_link_libraries(cef_host
    ${CEF_ROOT_ABS}/Release/libcef.so
    ${WRAPPER_LIBRARY}
    X11
)

# Copy CEF resources and binaries to output directory
add_custom_command(TARGET cef_host POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/chrome_100_percent.pak $<TARGET_FILE_DIR:cef_host>/chrome_100_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/chrome_200_percent.pak $<TARGET_FILE_DIR:cef_host>/chrome_200_percent.pak
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/resources.pak $<TARGET_FILE_DIR:cef_host>/resources.pak
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Resources/icudtl.dat $<TARGET_FILE_DIR:cef_host>/icudtl.dat
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CEF_ROOT_ABS}/Resources/locales $<TARGET_FILE_DIR:cef_host>/locales
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/chrome-sandbox $<TARGET_FILE_DIR:cef_host>/chrome-sandbox
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libcef.so $<TARGET_FILE_DIR:cef_host>/libcef.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libEGL.so $<TARGET_FILE_DIR:cef_host>/libEGL.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libGLESv2.so $<TARGET_FILE_DIR:cef_host>/libGLESv2.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libvk_swiftshader.so $<TARGET_FILE_DIR:cef_host>/libvk_swiftshader.so
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/libvulkan.so.1 $<TARGET_FILE_DIR:cef_host>/libvulkan.so.1
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/v8_context_snapshot.bin $<TARGET_FILE_DIR:cef_host>/v8_context_snapshot.bin
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CEF_ROOT_ABS}/Release/vk_swiftshader_icd.json $<TARGET_FILE_DIR:cef_host>/vk_swiftshader_icd.json
)
