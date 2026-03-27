cmake_minimum_required(VERSION 3.16)
project(cef_bridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CEF root directory (downloaded by scripts/download-cef.ts)
if(NOT DEFINED CEF_ROOT)
    if(WIN32)
        set(DEFAULT_PLATFORM "windows64")
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
            set(DEFAULT_PLATFORM "macosarm64")
        else()
            set(DEFAULT_PLATFORM "macosx64")
        endif()
    else()
        set(DEFAULT_PLATFORM "linux64")
    endif()
    set(CEF_ROOT "cef/${DEFAULT_PLATFORM}")
endif()
if(IS_ABSOLUTE "${CEF_ROOT}")
    set(CEF_ROOT_ABS "${CEF_ROOT}")
else()
    set(CEF_ROOT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${CEF_ROOT}")
endif()

# Platform-specific settings
if(WIN32)
    set(CEF_LIB "libcef.lib")
    set(CEF_DLL "libcef.dll")
    set(WRAPPER_LIB "libcef_dll_wrapper.lib")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS ws2_32 winmm gdi32 user32 comdlg32 uuid)
elseif(APPLE)
    set(CEF_LIB "Chromium Embedded Framework")
    set(CEF_DLL "libcef.dylib")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release/Chromium Embedded Framework.framework")
    find_library(COCOA_LIBRARY Cocoa)
    find_library(APPKIT_LIBRARY AppKit)
    find_library(CEF_LIBRARY "Chromium Embedded Framework" PATHS "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS ${COCOA_LIBRARY} ${APPKIT_LIBRARY})
else()
    set(CEF_LIB "libcef.so")
    set(CEF_DLL "libcef.so")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS X11)
endif()

# Wrapper library
if(WIN32)
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/Release/${WRAPPER_LIB}")
elseif(APPLE)
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/Release/${WRAPPER_LIB}")
else()
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/${WRAPPER_LIB}")
endif()

if(NOT EXISTS "${WRAPPER_LIBRARY}")
    message(FATAL_ERROR "Wrapper library not found at ${WRAPPER_LIBRARY}. Run 'bun run download:cef' first.")
endif()

# CEF include directories
include_directories(
    ${CEF_ROOT_ABS}
    ${CEF_ROOT_ABS}/include
    ${CEF_ROOT_ABS}/libcef_dll
    ${CEF_ROOT_ABS}/libcef_dll/wrapper
)

# Common compile definitions
set(COMPILE_DEFS
    CEF_USE_SANDBOX
    _FILE_OFFSET_BITS=64
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
)

# Platform-specific compile flags
if(WIN32)
    set(HELPER_FLAGS
        /O2 /Ob2 /Oi /Ot /GL /MT /EHsc /W3 /WX-
        /wd4100 /wd4127 /wd4244 /wd4267 /wd4996 /utf-8
        /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_UNICODE /DUNICODE /DNOMINMAX
        /D_WIN32_WINNT=0x0601 /DWIN32 /D_USRDLL
        /D__STDC_CONSTANT_MACROS /D__STDC_FORMAT_MACROS
        /std:c++20 /MP /Gy /Gm-
    )
    set(ADDON_FLAGS ${HELPER_FLAGS})
elseif(APPLE)
    set(HELPER_FLAGS
        -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
        -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
        -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
        -Wno-comment -Wno-deprecated-declarations -fno-exceptions
        -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
        -Wsign-compare -Wno-narrowing
        -Wno-array-bounds
        -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=2 -mmacosx-version-min=10.15
    )
    set(ADDON_FLAGS
        -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
        -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
        -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
        -Wno-comment -Wno-deprecated-declarations -fexceptions -frtti
        -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
        -Wsign-compare -Wno-narrowing
        -Wno-array-bounds
        -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=2 -mmacosx-version-min=10.15
    )
else()
    set(HELPER_FLAGS
        -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
        -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
        -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
        -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64 -fno-exceptions
        -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
        -Wsign-compare -Wno-narrowing
        -Wno-array-bounds
        -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=2
    )
    set(ADDON_FLAGS
        -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
        -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
        -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
        -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64 -fexceptions -frtti
        -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
        -Wsign-compare -Wno-narrowing
        -Wno-array-bounds
        -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=2
    )
endif()

# -----------------------------------------------------------------------------
# Find Node.js headers - try executable first, then download
# -----------------------------------------------------------------------------
# Try node/bun executables first
foreach(PROG node bun)
    find_program(NODE_EXE ${PROG})
    if(NODE_EXE)
        execute_process(
            COMMAND ${NODE_EXE} -p "require('path').join(require('path').dirname(require('path').dirname(process.execPath)), 'include', 'node')"
            OUTPUT_VARIABLE NODE_INCLUDE_DIRS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NODE_INCLUDE_DIRS AND EXISTS "${NODE_INCLUDE_DIRS}")
            message(STATUS "Found Node.js headers via ${PROG}: ${NODE_INCLUDE_DIRS}")
            break()
        endif()
    endif()
endforeach()

# If not found, download Node.js headers
if(NOT NODE_INCLUDE_DIRS OR NOT EXISTS "${NODE_INCLUDE_DIRS}")
    message(STATUS "Downloading Node.js headers...")
    
    # Determine platform and arch
    if(WIN32)
        set(NAPI_PLATFORM "win32")
    elseif(APPLE)
        set(NAPI_PLATFORM "darwin")
    else()
        set(NAPI_PLATFORM "linux")
    endif()
    
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(NAPI_ARCH "arm64")
    else()
        set(NAPI_ARCH "x64")
    endif()
    
    set(NAPI_VERSION "22.16.0")
    set(NAPI_DEST "${CMAKE_BINARY_DIR}/node-${NAPI_VERSION}")
    set(NAPI_HEADERS_URL "https://nodejs.org/download/release/v${NAPI_VERSION}/node-v${NAPI_VERSION}-headers.tar.gz")
    
    if(NOT EXISTS "${NAPI_DEST}/include/node")
        message(STATUS "Downloading from ${NAPI_HEADERS_URL}")
        file(DOWNLOAD "${NAPI_HEADERS_URL}" "${CMAKE_BINARY_DIR}/node.tar.gz" SHOW_PROGRESS)
        file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/node.tar.gz" DESTINATION "${CMAKE_BINARY_DIR}")
        file(REMOVE "${CMAKE_BINARY_DIR}/node.tar.gz")
    endif()
    
    set(NODE_INCLUDE_DIRS "${NAPI_DEST}/include/node")
    
    # For Windows, also download node.lib
    if(WIN32)
        set(NODE_LIB_URL "https://nodejs.org/download/release/v${NAPI_VERSION}/win-${NAPI_ARCH}/node.lib")
        set(NODE_LIB "${NAPI_DEST}/lib/node.lib")
        if(NOT EXISTS "${NODE_LIB}")
            message(STATUS "Downloading node.lib from ${NODE_LIB_URL}")
            file(DOWNLOAD "${NODE_LIB_URL}" "${NODE_LIB}" SHOW_PROGRESS)
        endif()
        set(NODE_LIB_PATH "${NODE_LIB}")
    endif()
    
    message(STATUS "Downloaded Node.js headers to: ${NODE_INCLUDE_DIRS}")
endif()

if(NOT NODE_INCLUDE_DIRS OR NOT EXISTS "${NODE_INCLUDE_DIRS}")
    message(FATAL_ERROR "Node.js development files not found.")
endif()

# -----------------------------------------------------------------------------
# Helper executable (CEF subprocess)
# -----------------------------------------------------------------------------
add_executable(cef_bridge_helper src/cef_bridge.cc)
target_compile_options(cef_bridge_helper PRIVATE ${HELPER_FLAGS})
target_compile_definitions(cef_bridge_helper PRIVATE ${COMPILE_DEFS} -DCEF_HELPER_BINARY)
target_include_directories(cef_bridge_helper PRIVATE ${CEF_ROOT_ABS}/include)

target_link_libraries(cef_bridge_helper
    ${CEF_LIB_PATH}/${CEF_LIB}
    ${WRAPPER_LIBRARY}
    ${SYSTEM_LIBS}
)

add_custom_command(TARGET cef_bridge_helper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cef_bridge_helper> $<TARGET_FILE_DIR:cef_bridge_helper>
)

# -----------------------------------------------------------------------------
# Node addon
# -----------------------------------------------------------------------------
add_library(cef_bridge SHARED src/cef_bridge.cc)
set_target_properties(cef_bridge PROPERTIES PREFIX "" SUFFIX ".node")
target_compile_options(cef_bridge PRIVATE ${ADDON_FLAGS})
target_compile_definitions(cef_bridge PRIVATE ${COMPILE_DEFS})
target_include_directories(cef_bridge PRIVATE ${NODE_INCLUDE_DIRS} ${CEF_ROOT_ABS}/include)

if(APPLE)
    target_link_libraries(cef_bridge PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${COCOA_LIBRARY} ${APPKIT_LIBRARY} ${CEF_LIBRARY}
    )
elseif(WIN32)
    target_link_libraries(cef_bridge PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${SYSTEM_LIBS}
    )
else()
    target_link_libraries(cef_bridge PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${SYSTEM_LIBS} dl
    )
endif()

# Post-build commands
if(WIN32)
    add_custom_command(TARGET cef_bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Resources ${CMAKE_BINARY_DIR}/Resources
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/${CEF_DLL} ${CMAKE_BINARY_DIR}/${CEF_DLL}
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/chrome_elf.dll ${CMAKE_BINARY_DIR}/chrome_elf.dll
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/d3dcompiler_*.dll ${CMAKE_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/icudtl.dat ${CMAKE_BINARY_DIR}/icudtl.dat
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/resources.pak ${CMAKE_BINARY_DIR}/resources.pak
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:cef_bridge_helper> ${CMAKE_BINARY_DIR}/cef_bridge_helper.exe
    )
elseif(APPLE)
    add_custom_command(TARGET cef_bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Resources ${CMAKE_BINARY_DIR}/Resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Release/Chromium\ Embedded\ Framework.framework ${CMAKE_BINARY_DIR}/cef_bridge_helper.framework/Frameworks/Chromium\ Embedded\ Framework
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Release/Chromium\ Embedded\ Framework.framework/Libraries ${CMAKE_BINARY_DIR}/cef_bridge_helper.framework/Libraries
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/icudtl.dat ${CMAKE_BINARY_DIR}/icudtl.dat
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/resources.pak ${CMAKE_BINARY_DIR}/resources.pak
    )
else()
    add_custom_command(TARGET cef_bridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Resources ${CMAKE_BINARY_DIR}/Resources
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Resources/icudtl.dat ${CMAKE_BINARY_DIR}/icudtl.dat
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Resources/chrome_100_percent.pak ${CMAKE_BINARY_DIR}/chrome_100_percent.pak
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Resources/chrome_200_percent.pak ${CMAKE_BINARY_DIR}/chrome_200_percent.pak
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Resources/resources.pak ${CMAKE_BINARY_DIR}/resources.pak
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/chrome-sandbox ${CMAKE_BINARY_DIR}/chrome-sandbox
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/${CEF_DLL} ${CMAKE_BINARY_DIR}/${CEF_DLL}
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/libEGL.so ${CMAKE_BINARY_DIR}/libEGL.so
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/libGLESv2.so ${CMAKE_BINARY_DIR}/libGLESv2.so
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/libvk_swiftshader.so ${CMAKE_BINARY_DIR}/libvk_swiftshader.so
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/libvulkan.so.1 ${CMAKE_BINARY_DIR}/libvulkan.so.1
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/v8_context_snapshot.bin ${CMAKE_BINARY_DIR}/v8_context_snapshot.bin
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/vk_swiftshader_icd.json ${CMAKE_BINARY_DIR}/vk_swiftshader_icd.json
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:cef_bridge_helper> ${CMAKE_BINARY_DIR}/cef_bridge_helper
    )
endif()
