#include "../include/frostmonitor/tray.hpp"

#include <spdlog/spdlog.h>

#include <cstring>
#include <shellapi.h>
#include <stdexcept>
#include <bit>

namespace frostmonitor {
    namespace {
        void fillIconData(NOTIFYICONDATAW &data, HWND hwnd, HICON icon, const wchar_t *tip){
            data = {};
            data.cbSize = sizeof(data);
            data.hWnd = hwnd;
            data.uID = 1;
            data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            data.uCallbackMessage = TrayIcon::kTrayMessage;
            data.hIcon = icon;
            wcscpy_s(data.szTip, tip);
        }
    }

    TrayIcon::TrayIcon(HINSTANCE instance, Callbacks callbacks)
        : instance_(instance), callbacks_(std::move(callbacks)),
          taskbarCreatedMsg_(RegisterWindowMessageW(L"TaskbarCreated"))
    {
        WNDCLASSEXW cls{};

        cls.cbSize = sizeof(cls);
        cls.lpfnWndProc = &TrayIcon::wndProc;
        cls.hInstance = instance_;
        cls.lpszClassName = L"FrostMonitorTrayWindow";

        if(RegisterClassExW(&cls) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("tray: RegisterClassEx failed");

        hwnd_ = CreateWindowExW(0, L"FrostMonitorTrayWindow", L"", WS_OVERLAPPED,
                                0, 0, 0, 0, nullptr, nullptr, instance_, this);

        if(hwnd_ == nullptr)
            throw std::runtime_error("tray: CreateWndowEx failed");

        menu_ = CreatePopupMenu();

        if(menu_ == nullptr)
            throw std::runtime_error("tray: CreatePopupMenu failed");

        AppendMenuW(menu_, MF_STRING, kMenuPause, L"Pause");
        AppendMenuW(menu_, MF_STRING, kMenuOpenConfig, L"Open Config");
        AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu_, MF_STRING, kMenuExit, L"Exit");

        addIcon();
    }

    TrayIcon::~TrayIcon(){
        removeIcon();

        if(menu_ != nullptr)
            DestroyMenu(menu_);

        if(hwnd_ != nullptr)
            DestroyWindow(hwnd_);
    }

    void TrayIcon::setPaused(bool paused){
        paused_ = paused;

        ModifyMenuW(
            menu_, 
            kMenuPause, 
            MF_BYCOMMAND | MF_STRING, 
            kMenuPause, 
            paused ? L"Resume" : L"Pause"
        );

        NOTIFYICONDATAW data{};

        fillIconData(
            data, 
            hwnd_, 
            LoadIconW(nullptr, IDI_APPLICATION), 
            paused ? L"FrostMonitor (paused)" : L"FrostMonitor"
        );
        
        Shell_NotifyIconW(NIM_MODIFY, &data);
    }

    auto TrayIcon::window() const noexcept -> HWND {
        return hwnd_;
    }

    void TrayIcon::addIcon(){
        NOTIFYICONDATAW data{};
        
        fillIconData(
            data, 
            hwnd_, 
            LoadIconW(nullptr, IDI_APPLICATION), 
            paused_ ? L"FrostMonitor (paused)" : L"FrostMonitor"
        );

        if(Shell_NotifyIconW(NIM_ADD, &data) == FALSE)
            spdlog::warn("tray: NIM_ADD failed, will retry on TaskbarCreated");
    }

    void TrayIcon::removeIcon(){
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = hwnd_;
        data.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }

    void TrayIcon::showMenu() {
        POINT cursor{};
        GetCursorPos(&cursor);
        SetForegroundWindow(hwnd_);

        const UINT id = TrackPopupMenu(
            menu_,
            TPM_RETURNCMD | TPM_RIGHTBUTTON, 
            cursor.x, 
            cursor.y, 
            0, 
            hwnd_,
            nullptr
        );

        PostMessageW(hwnd_, WM_NULL, 0, 0);

        if(id != 0)
            handleCommand(id);
    }

    void TrayIcon::handleCommand(UINT id) {
        switch(id){
            case kMenuPause:
                if(callbacks_.onTogglePause)
                    callbacks_.onTogglePause();

                break;
            case kMenuOpenConfig:
                if(callbacks_.onOpenConfig)
                    callbacks_.onOpenConfig();

                break;
            case kMenuExit:
                if(callbacks_.onExit)
                    callbacks_.onExit();

                break;
                
            default:
                break;
        }
    }

    LRESULT CALLBACK TrayIcon::wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam){
        auto *self = std::bit_cast<TrayIcon *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if(msg == WM_NCCREATE){
            auto *create = std::bit_cast<CREATESTRUCTW *>(lparam);
            self = reinterpret_cast<TrayIcon *>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }

        if(self != nullptr && msg == self->taskbarCreatedMsg_){
            self->addIcon();
            return 0;
        }

        if(self != nullptr && msg == kTrayMessage){
            if(lparam == WM_RBUTTONUP)
                self->showMenu();
            else if(lparam == WM_LBUTTONDBLCLK && self->callbacks_.onTogglePause)
                self->callbacks_.onTogglePause();

            return 0;
        }

        switch(msg){
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            default:
                return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
    }
}
