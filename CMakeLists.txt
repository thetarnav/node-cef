cmake_minimum_required(VERSION 3.15...3.31)
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
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/Release/${WRAPPER_LIB}")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS ws2_32 winmm gdi32 user32 comdlg32 uuid)
elseif(APPLE)
    set(CEF_LIB "Chromium Embedded Framework")
    set(CEF_DLL "libcef.dylib")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/Release/${WRAPPER_LIB}")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release/Chromium Embedded Framework.framework")
    find_library(COCOA_LIBRARY Cocoa)
    find_library(APPKIT_LIBRARY AppKit)
    find_library(CEF_LIBRARY "Chromium Embedded Framework" PATHS "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS ${COCOA_LIBRARY} ${APPKIT_LIBRARY})
else()
    set(CEF_LIB "libcef.so")
    set(CEF_DLL "libcef.so")
    set(WRAPPER_LIB "libcef_dll_wrapper.a")
    set(WRAPPER_LIBRARY "${CEF_ROOT_ABS}/build/libcef_dll_wrapper/${WRAPPER_LIB}")
    set(CEF_LIB_PATH "${CEF_ROOT_ABS}/Release")
    set(SYSTEM_LIBS X11)
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

if(LINUX)
    set_target_properties(cef_bridge_helper PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

add_custom_command(TARGET cef_bridge_helper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:cef_bridge_helper> $<TARGET_FILE_DIR:cef_bridge_helper>
)

# -----------------------------------------------------------------------------
# Node addon (using cmake-js)
# -----------------------------------------------------------------------------
add_library(${PROJECT_NAME} SHARED src/cef_bridge.cc ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions -frtti)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFS} NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFS})
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${CEF_ROOT_ABS}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_JS_LIB})

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "@loader_path/Frameworks"
        INSTALL_RPATH "@loader_path/Frameworks"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${COCOA_LIBRARY} ${APPKIT_LIBRARY} ${CEF_LIBRARY}
    )
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${SYSTEM_LIBS}
    )
    if(CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
        execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
    endif()
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CEF_LIB_PATH}/${CEF_LIB}
        ${WRAPPER_LIBRARY}
        ${SYSTEM_LIBS} dl
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Post-build commands
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Resources ${CMAKE_BINARY_DIR}/Resources
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/${CEF_DLL} ${CMAKE_BINARY_DIR}/${CEF_DLL}
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Release/chrome_elf.dll ${CMAKE_BINARY_DIR}/chrome_elf.dll
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/Release/cef_bridge.node ${CMAKE_BINARY_DIR}/cef_bridge.node
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CEF_ROOT_ABS}/Resources/icudtl.dat ${CMAKE_BINARY_DIR}/icudtl.dat
    )
elseif(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Release/Chromium\ Embedded\ Framework.framework ${CMAKE_BINARY_DIR}/Frameworks/Chromium\ Embedded\ Framework.framework
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CEF_ROOT_ABS}/Release/Chromium\ Embedded\ Framework.framework/Resources ${CMAKE_BINARY_DIR}/Resources
        COMMAND /usr/bin/install_name_tool -change @executable_path/../Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework @loader_path/Frameworks/Chromium\ Embedded\ Framework.framework/Chromium\ Embedded\ Framework ${CMAKE_BINARY_DIR}/cef_bridge.node
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
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
