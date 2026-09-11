#pragma once

#include <functional>
#include <string_view>
#include <windows.h>

namespace frostmonitor {
    class TrayIcon {
    public:
        static constexpr UINT kTrayMessage = WM_APP + 1;
        static constexpr UINT kMenuPause = 1001;
        static constexpr UINT kMenuOpenConfig = 1002;
        static constexpr UINT kMenuExit = 1003;
        static constexpr std::wstring_view kWindowClass = L"FrostMonitorTrayWindow";

        struct Callbacks {
            std::function<void()> onTogglePause;
            std::function<void()> onOpenConfig;
            std::function<void()> onExit;
        };

        TrayIcon(HINSTANCE instance, Callbacks callbacks);
        ~TrayIcon();

        TrayIcon(const TrayIcon &) = delete;
        TrayIcon(TrayIcon &&) = delete;
        TrayIcon &operator = (const TrayIcon &) = delete;
        TrayIcon &operator = (TrayIcon &&) = delete;

        void setPaused(bool paused);
        [[nodiscard]] auto window() const noexcept -> HWND;

    private:
        static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
        void addIcon();
        void removeIcon();
        void showMenu();
        void handleCommand(UINT id);

        HINSTANCE instance_{nullptr};
        HWND hwnd_{nullptr};
        HMENU menu_{nullptr};
        UINT taskbarCreatedMsg_{0};
        bool paused_{false};
        Callbacks callbacks_;
    };
}