cmake_minimum_required(VERSION 3.16)
project(cef_bridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CEF root directory (downloaded by scripts/download-cef.ts)
# Can be overridden via CEF_ROOT environment variable
if(NOT DEFINED CEF_ROOT)
    # Detect platform from environment or use cmake system info
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
    set(LIB_PREFIX "")
    set(LIB_SUFFIX ".lib")
    set(CEF_LIB "libcef.lib")
    set(CEF_DLL "libcef.dll")
    set(WRAPPER_LIB "libcef_dll_wrapper.lib")
    set(SHADOW_ENABLED FALSE)
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
elseif(APPLE)
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".dylib")
    set(CEF_LIB "Chromium Embedded Framework")
    set(CEF_DLL "libcef.dylib")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(SHADOW_ENABLED FALSE)
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release/Chromium Embedded Framework.framework")
else()
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".so")
    set(CEF_LIB "libcef.so")
    set(CEF_DLL "libcef.so")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(SHADOW_ENABLED TRUE)
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
endif()

# Platform-specific system libraries
if(WIN32)
    set(SYSTEM_LIBS ws2_32 winmm gdi32 user32 comdlg32 uuid)
    set(ADDON_LIBS ${SYSTEM_LIBS})
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(APPKIT_LIBRARY AppKit)
    find_library(CEF_LIBRARY "Chromium Embedded Framework" PATHS "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS ${COCOA_LIBRARY} ${APPKIT_LIBRARY})
    set(ADDON_LIBS ${COCOA_LIBRARY} ${APPKIT_LIBRARY} ${CEF_LIBRARY})
else()
    set(SYSTEM_LIBS X11)
    set(ADDON_LIBS X11 dl)
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
    CEF_USE_SANDBOX
    _FILE_OFFSET_BITS=64
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
)

# Platform-specific compile flags
if(WIN32)
    set(COMPILE_FLAGS_HELPER
        /O2 /Ob2 /Oi /Ot /GL /MT /EHsc /W3 /WX-
        /wd4100 /wd4127 /wd4244 /wd4267 /wd4996 /utf-8
        /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_UNICODE /DUNICODE /DNOMINMAX
        /D_WIN32_WINNT=0x0601 /DWIN32 /D_USRDLL
        /D__STDC_CONSTANT_MACROS /D__STDC_FORMAT_MACROS
        /std:c++20 /MP /Gy /Gm-
    )
    set(COMPILE_FLAGS_ADDON
        /O2 /Ob2 /Oi /Ot /GL /MT /EHsc /W3 /WX-
        /wd4100 /wd4127 /wd4244 /wd4267 /wd4996 /utf-8
        /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D_UNICODE /DUNICODE /DNOMINMAX
        /D_WIN32_WINNT=0x0601 /DWIN32 /D_USRDLL
        /D__STDC_CONSTANT_MACROS /D__STDC_FORMAT_MACROS
        /std:c++20 /MP /Gy /Gm-
    )
elseif(APPLE)
    set(COMPILE_FLAGS_HELPER
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
    set(COMPILE_FLAGS_ADDON
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
    set(COMPILE_FLAGS_HELPER
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
    set(COMPILE_FLAGS_ADDON
        -O3 -DNDEBUG -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables
        -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror
        -Wno-missing-field-initializers -Wno-unused-parameter -Wno-error=comment
        -Wno-comment -Wno-deprecated-declarations -m64 -march=x86-64
        -fexceptions -frtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=c++20
        -Wsign-compare -Wno-narrowing
        -Wno-array-bounds
        -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE
        -D_FORTIFY_SOURCE=2
    )
endif()

# Find node - use pkg-config or manual path
# First check environment variable (can be set by CI)
if(DEFINED ENV{NODE_INCLUDE_DIRS})
    set(NODE_INCLUDE_DIRS $ENV{NODE_INCLUDE_DIRS})
    message(STATUS "Using NODE_INCLUDE_DIRS from environment: ${NODE_INCLUDE_DIRS}")
else()
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(NODE nodejs)
    endif()
endif()

# Fallback: find node headers manually
if(NOT NODE_FOUND)
    find_program(NODE_EXECUTABLE node)
    if(NOT NODE_EXECUTABLE)
        find_program(NODE_EXECUTABLE bun)
    endif()
    if(NODE_EXECUTABLE)
        execute_process(
            COMMAND ${NODE_EXECUTABLE} -p "require('path').join(require('path').dirname(require('path').dirname(process.execPath)), 'include', 'node')"
            OUTPUT_VARIABLE NODE_INCLUDE_DIRS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
endif()

if(NOT NODE_INCLUDE_DIRS OR NOT EXISTS "${NODE_INCLUDE_DIRS}")
    # Try common Windows locations (with different possible paths)
    if(WIN32)
        set(POSSIBLE_NODE_PATHS
            "C:/Program Files/nodejs/include/node"
            "C:/Program Files (x86)/nodejs/include/node"
            "$ENV{PROGRAMFILES}/nodejs/include/node"
            "$ENV{LOCALAPPDATA}/Programs/node/include/node"
        )
        foreach(PATH ${POSSIBLE_NODE_PATHS})
            if(EXISTS "${PATH}")
                set(NODE_INCLUDE_DIRS "${PATH}")
                break()
            endif()
        endforeach()
    elseif(APPLE)
        set(POSSIBLE_NODE_PATHS
            "/usr/local/include/node"
            "/opt/homebrew/include/node"
        )
        foreach(PATH ${POSSIBLE_NODE_PATHS})
            if(EXISTS "${PATH}")
                set(NODE_INCLUDE_DIRS "${PATH}")
                break()
            endif()
        endforeach()
    endif()
endif()

message(STATUS "NODE_INCLUDE_DIRS: ${NODE_INCLUDE_DIRS}")

if(NOT NODE_INCLUDE_DIRS OR NOT EXISTS "${NODE_INCLUDE_DIRS}")
    message(FATAL_ERROR "Node.js development files not found. Set NODE_INCLUDE_DIRS manually.")
endif()

# -----------------------------------------------------------------------------
# Helper executable (CEF subprocess)
# -----------------------------------------------------------------------------
add_executable(cef_bridge_helper src/cef_bridge.cc)
target_compile_options(cef_bridge_helper PRIVATE ${COMPILE_FLAGS_HELPER})
target_compile_definitions(cef_bridge_helper PRIVATE ${COMPILE_DEFS} -DCEF_HELPER_BINARY)

target_link_libraries(cef_bridge_helper
    ${CEF_LIB_PATH}/${CEF_LIB}
    ${WRAPPER_LIBRARY}
    ${SYSTEM_LIBS}
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
    ${CEF_LIB_PATH}/${CEF_LIB}
    ${WRAPPER_LIBRARY}
    ${ADDON_LIBS}
)

if(WIN32)
    # ---
elseif(APPLE)
    target_link_options(cef_bridge PRIVATE "-Wl,-rpath,@executable_path/../Frameworks")
else()
    target_link_options(cef_bridge PRIVATE "-Wl,-rpath,\$ORIGIN")
endif()

# Copy all CEF distribution files to build/ directory
# CEF requires resources to be in same directory as libcef.so
if(WIN32)
    # Windows: copy DLLs and resources
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
    # macOS: copy Frameworks and resources
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
    # Linux: copy .so files and resources
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
            ${CEF_ROOT_ABS}/Release/${CEF_DLL} ${CMAKE_BINARY_DIR}/${CEF_DLL}
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
endif()
