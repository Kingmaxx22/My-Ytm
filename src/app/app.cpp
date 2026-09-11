#include "app/app.h"

#include "app/logger.h"
#include "auth/auth_manager.h"
#include "auth/credential_store.h"
#include "config/config.h"
#include "platform/audio_backend.h"
#include "player/mock_player.h"
#include "player/queue.h"
#include "ui/auth_screen.h"
#include "ui/help_screen.h"
#include "ui/history_screen.h"
#include "ui/home_screen.h"
#include "ui/input.h"
#include "ui/keymap.h"
#include "ui/library_screen.h"
#include "ui/playlists_screen.h"
#include "ui/queue_screen.h"
#include "ui/renderer.h"
#include "ui/search_screen.h"
#include "ui/status_bar.h"
#include "ui/terminal.h"
#include "youtube/http_client.h"
#include "youtube/innertube.h"
#include "youtube/youtube_client.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace myytm::app {

int App::run()
{
    // Non-interactive fallback: keep Phase-1 behavior for pipes/tests.
    bool interactive = isatty(fileno(stdin)) && isatty(fileno(stdout));
    if (!interactive) {
        std::cout << "My-Ytm\n";
        return 0;
    }

    using namespace myytm::ui;

    config::ConfigManager cfgMgr;
    cfgMgr.load();
    Logger logger;
    logger.info("My-Ytm starting");

    Terminal term;
    try {
        term.init();
    } catch (const std::exception& e) {
        logger.error(std::string("Terminal init failed: ") + e.what());
        std::cerr << "Unable to initialize terminal. " << e.what() << "\n";
        return 1;
    }

    Renderer renderer(term);
    StatusBar status;

    // Auth: browser-based, DPAPI secure storage (AGENTS.md: Security)
    auto credentialStore = std::make_unique<auth::SecureCredentialStore>();
    auto authManager = std::make_shared<auth::AuthManager>(std::move(credentialStore));
    if (authManager->restore()) logger.info("Auth restored: " + std::string(auth::toString(authManager->state())));
    else logger.info("Auth not restored: signed out");

    // Platform layer → WinHTTP → OAuth loopback → YouTube API → Audio
    // Player / Queue — real audio backend (WinMM waveOut) isolated in src/platform per AGENTS.md
    auto queue = std::make_shared<player::Queue>();
    std::shared_ptr<player::Player> player = std::make_shared<platform::PlatformAudioPlayer>(queue);
    player->setVolume(cfgMgr.get().volume);

    // YouTubeClient — InnerTube POST with API key + client context, auth header, fallback mock offline
    auto ytClient = std::make_shared<youtube::YouTubeClient>(std::make_unique<youtube::WinHttpClient>());
    ytClient->setAuthHeaderProvider([authManager](){ return authManager->authorizationHeader(); });
    {
        youtube::InnertubeConfig icfg;
        icfg.apiKey = cfgMgr.get().youtubeApiKey;
        icfg.clientName = cfgMgr.get().youtubeClientName;
        icfg.clientVersion = cfgMgr.get().youtubeClientVersion;
        icfg.baseUrl = cfgMgr.get().youtubeBaseUrl;
        ytClient->setInnertubeConfig(icfg);
    }

    // Screens: 0=Home,1=Search,2=Library(pl),3=Playlists(pl),4=History(pl),5=Account,6=Queue
    std::vector<std::unique_ptr<Screen>> screens;
    auto searchScreen = std::make_unique<SearchScreen>(ytClient);
    searchScreen->setQueue(queue);
    searchScreen->setPlayer(player);
    screens.emplace_back(std::make_unique<HomeScreen>());
    screens.emplace_back(std::move(searchScreen));
    screens.emplace_back(std::make_unique<LibraryScreen>(ytClient, queue, player));
    screens.emplace_back(std::make_unique<PlaylistsScreen>(ytClient, queue, player));
    screens.emplace_back(std::make_unique<HistoryScreen>(ytClient, queue, player));
    screens.emplace_back(std::make_unique<AuthScreen>(authManager));
    screens.emplace_back(std::make_unique<QueueScreen>(queue, player));
    auto helpScreen = std::make_unique<HelpScreen>();

    Screen* current = screens[0].get();
    Screen* previous = nullptr;
    bool inHelp = false;
    current->onEnter();

    auto renderAll = [&]() {
        term.refreshSize();
        renderer.clear();
        current->render(renderer);
        status.render(renderer, current->name());
        std::fflush(stdout);
    };

    renderAll();

    while (true) {
        Key k = readKey();

        // Global bindings
        if (keymap::isHelp(k)) {
            if (!inHelp) {
                previous = current;
                current->onExit();
                current = helpScreen.get();
                inHelp = true;
                current->onEnter();
                status.setMessage("Help — q/Esc to go back");
            } else {
                // already in help, ignore or refresh
            }
            renderAll();
            continue;
        }

        // When in help, q/Esc goes back
        if (inHelp && keymap::isQuit(k)) {
            current->onExit();
            current = previous ? previous : screens[0].get();
            previous = nullptr;
            inHelp = false;
            status.clearMessage();
            renderAll();
            continue;
        }

        // Global navigation 1-5 (not when inHelp? allow)
        if (!inHelp) {
            if (keymap::isHome(k)) {
                if (current != screens[0].get()) {
                    current->onExit();
                    current = screens[0].get();
                    current->onEnter();
                    status.clearMessage();
                    renderAll();
                }
                continue;
            }
            if (keymap::isSearchScreen(k)) {
                current->onExit();
                current = screens[1].get();
                current->onEnter();
                renderAll();
                continue;
            }
            if (keymap::isLibrary(k)) {
                current->onExit();
                current = screens[2].get();
                current->onEnter();
                renderAll();
                continue;
            }
            if (keymap::isPlaylists(k)) {
                current->onExit();
                current = screens[3].get();
                current->onEnter();
                renderAll();
                continue;
            }
            if (keymap::isHistory(k)) {
                current->onExit();
                current = screens[4].get();
                current->onEnter();
                renderAll();
                continue;
            }
            if (k.code == KeyCode::Char && k.ch == '0') {
                current->onExit();
                current = screens[5].get();
                current->onEnter();
                renderAll();
                continue;
            }
            if (k.code == KeyCode::Char && k.ch == '6') {
                current->onExit();
                current = screens[6].get();
                current->onEnter();
                renderAll();
                continue;
            }

            // Global playback — vim spec Space ] [ H L + - (not when Search is in input mode)
            {
                bool inSearchInput = false;
                if (current == screens[1].get()) {
                    auto* ss = static_cast<SearchScreen*>(current);
                    inSearchInput = ss->isInputMode();
                }
                if (!inSearchInput) {
                    if (keymap::isPlayPause(k)) {
                        auto st = player->state();
                        if (st.status == models::PlaybackStatus::Playing) player->pause();
                        else player->play();
                        status.setMessage(st.status == models::PlaybackStatus::Playing ? "Paused" : "Playing");
                        renderAll(); continue;
                    }
                    if (keymap::isNext(k)) { player->next(); status.setMessage("Next"); renderAll(); continue; }
                    if (keymap::isPrev(k)) { player->previous(); status.setMessage("Previous"); renderAll(); continue; }
                    if (keymap::isSeekBack(k)) { player->seek(-10); status.setMessage("Seek -10s"); renderAll(); continue; }
                    if (keymap::isSeekForward(k)) { player->seek(10); status.setMessage("Seek +10s"); renderAll(); continue; }
                    if (keymap::isVolUp(k)) { player->volumeUp(); cfgMgr.setVolume(player->state().volume); status.setMessage("Volume " + std::to_string(player->state().volume)); renderAll(); continue; }
                    if (keymap::isVolDown(k)) { player->volumeDown(); cfgMgr.setVolume(player->state().volume); status.setMessage("Volume " + std::to_string(player->state().volume)); renderAll(); continue; }
                }
            }

            // q quits from home, backs otherwise (for now quit everywhere)
            if (keymap::isQuit(k)) {
                break;
            }
        }

        // Delegate to screen
        bool consumed = current->handleKey(k);
        // Show transient feedback for unhandled keys
        if (!consumed) {
            // Could show message for unknown, but keep quiet to avoid noise
        }
        renderAll();
    }

    current->onExit();
    // Persist config (volume/lastScreen) — no secrets (AGENTS.md: Config vs Auth)
    try {
        cfgMgr.setLastScreen(std::string(current->name()));
        cfgMgr.setVolume(player->state().volume);
        if (!cfgMgr.save()) logger.warn("Failed to save config to " + cfgMgr.filePath().string());
        else logger.info("Config saved");
    } catch (const std::exception& e) {
        logger.error(std::string("Config save failed: ") + e.what());
    }

    try {
        term.restore();
    } catch (const std::exception& e) {
        logger.error(std::string("Terminal restore failed: ") + e.what());
    }
    logger.info("My-Ytm exiting");
    return 0;
}

} // namespace myytm::app
