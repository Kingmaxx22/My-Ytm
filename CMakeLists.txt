cmake_minimum_required(VERSION 3.25)

project(MyYtm
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MyYtm
    src/main.cpp
    src/app/app.cpp
    src/app/logger.cpp
    src/config/config.cpp
    src/ui/terminal.cpp
    src/ui/input.cpp
    src/ui/home_screen.cpp
    src/ui/help_screen.cpp
    src/ui/placeholder_screen.cpp
    src/ui/search_screen.cpp
    src/youtube/http_client.cpp
    src/youtube/youtube_client.cpp
    src/auth/credential_store.cpp
    src/auth/auth_manager.cpp
    src/ui/auth_screen.cpp
    src/player/queue.cpp
    src/player/mock_player.cpp
    src/ui/queue_screen.cpp
    src/ui/library_screen.cpp
    src/ui/playlists_screen.cpp
    src/ui/history_screen.cpp
    src/platform/browser/BrowserLauncherWin.cpp
    src/platform/crypto/CryptoWin.cpp
    src/platform/credentials/CredentialStoreWin.cpp
    src/platform/terminal/TerminalWin.cpp
    src/platform/http/WinHttpClient.cpp
    src/platform/oauth/OAuthLoopbackWin.cpp
    src/platform/audio_backend.cpp
)

target_include_directories(MyYtm
    PRIVATE
        src
)

if(WIN32)
    target_link_libraries(MyYtm PRIVATE crypt32 shell32 winhttp ws2_32 winmm bcrypt)
endif()

enable_testing()

add_executable(MyYtmUiTests
    tests/test_ui.cpp
    src/app/logger.cpp
    src/config/config.cpp
    src/ui/terminal.cpp
    src/ui/input.cpp
    src/ui/home_screen.cpp
    src/ui/help_screen.cpp
    src/ui/placeholder_screen.cpp
    src/ui/search_screen.cpp
    src/youtube/http_client.cpp
    src/youtube/youtube_client.cpp
    src/auth/credential_store.cpp
    src/auth/auth_manager.cpp
    src/ui/auth_screen.cpp
    src/player/queue.cpp
    src/player/mock_player.cpp
    src/ui/queue_screen.cpp
    src/ui/library_screen.cpp
    src/ui/playlists_screen.cpp
    src/ui/history_screen.cpp
    src/platform/browser/BrowserLauncherWin.cpp
    src/platform/crypto/CryptoWin.cpp
    src/platform/credentials/CredentialStoreWin.cpp
    src/platform/terminal/TerminalWin.cpp
    src/platform/http/WinHttpClient.cpp
    src/platform/oauth/OAuthLoopbackWin.cpp
    src/platform/audio_backend.cpp
)

target_include_directories(MyYtmUiTests PRIVATE src)

if(WIN32)
    target_link_libraries(MyYtmUiTests PRIVATE crypt32 shell32 winhttp ws2_32 winmm bcrypt)
endif()

add_test(NAME UiTests COMMAND MyYtmUiTests)

add_executable(MyYtmOAuthHardeningTests
    tests/test_oauth_hardening.cpp
    src/auth/credential_store.cpp
    src/auth/auth_manager.cpp
    src/platform/browser/BrowserLauncherWin.cpp
    src/platform/crypto/CryptoWin.cpp
    src/platform/credentials/CredentialStoreWin.cpp
    src/platform/http/WinHttpClient.cpp
    src/platform/oauth/OAuthLoopbackWin.cpp
)
target_include_directories(MyYtmOAuthHardeningTests PRIVATE src)
if(WIN32)
    target_link_libraries(MyYtmOAuthHardeningTests PRIVATE crypt32 shell32 winhttp ws2_32 bcrypt)
endif()
add_test(NAME OAuthHardeningTests COMMAND MyYtmOAuthHardeningTests)

# Windows packaging (Phase 8) — ZIP + NSIS + MSI (WIX) with choosable install dir
include(InstallRequiredSystemLibraries)
install(TARGETS MyYtm DESTINATION bin)

set(CPACK_PACKAGE_NAME "MyYtm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "MyYtm")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "My-Ytm — Windows-first YouTube Music TUI")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "MyYtm")
set(CPACK_PACKAGE_CONTACT "https://github.com/MyYtm/MyYtm")

set(CPACK_GENERATOR "ZIP;NSIS" CACHE STRING "Generators")

# NSIS — allows directory chooser (MUI_PAGE_DIRECTORY = $PROGRAMFILES64)
set(CPACK_NSIS_DISPLAY_NAME "My-Ytm")
set(CPACK_NSIS_PACKAGE_NAME "MyYtm")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)

# MSI via WiX Toolset v4 (dotnet wix) — see packaging/MyYtm.wxs
# Build MSI with choosable install dir (WixUI_InstallDir):
#   wix build packaging/MyYtm.wxs -o build/MyYtm-0.1.0.msi -arch x64 -ext WixToolset.UI.wixext
# Or:  cmake --build build --target msi
# CPack WIX generator (requires WiX v3 candle/light) kept for CI fallback:
set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-4A1B-8C9D-0E1F2A3B4C5D")
set(CPACK_WIX_PRODUCT_GUID "F6E5D4C3-B2A1-4F6E-8D9C-0A1B2C3D4E5F")
set(CPACK_WIX_UI_REF "WixUI_InstallDir")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "MyYtm")
set(CPACK_WIX_PROPERTY_ARPCOMMENTS "Lightweight terminal YouTube Music client")
set(CPACK_WIX_CULTURES "en-US")

include(CPack)

# Custom MSI target via WiX v4 (choosable INSTALLFOLDER via WixUI_InstallDir)
find_program(WIX_EXE wix HINTS "$ENV{USERPROFILE}/.dotnet/tools")
if(WIX_EXE)
  add_custom_target(msi
    COMMAND "${WIX_EXE}" build "${CMAKE_SOURCE_DIR}/packaging/MyYtm.wxs"
            -o "${CMAKE_BINARY_DIR}/MyYtm-${PROJECT_VERSION}.msi"
            -arch x64 -ext WixToolset.UI.wixext
    DEPENDS MyYtm
    COMMENT "Building MSI with WiX v4 (WixUI_InstallDir — Browse for install folder)"
    VERBATIM
  )
endif()
