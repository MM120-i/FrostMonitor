#include "../include/frostmonitor/autostart.hpp"

#include <spdlog/spdlog.h>

#include <memory>
#include <windows.h>

namespace frostmonitor {
    namespace {
        constexpr std::wstring_view kRunKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
        using RegKey = std::unique_ptr<std::remove_pointer_t<HKEY>, decltype(&RegCloseKey)>;
    }

    RegKey openRunKey(){
        HKEY raw = nullptr;

        if(RegCreateKeyExW(HKEY_CURRENT_USER, std::wstring(kRunKey).c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &raw, nullptr) != ERROR_SUCCESS)
            return {nullptr, &RegCloseKey};

        return {raw, &RegCloseKey};
    }    

    std::wstring readEntry(HKEY key){
        DWORD bytes = 0;

        if(RegQueryValueExW(key, std::wstring(kAutostartValueName).c_str(), nullptr, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
            return {};
        
        std::wstring value(bytes / sizeof(wchar_t), L'\0');
        DWORD type = 0;
        DWORD read = bytes;

        if(RegQueryValueExW(key, std::wstring(kAutostartValueName).c_str(), nullptr, &type, reinterpret_cast<BYTE *>(value.data()), &read) != ERROR_SUCCESS || type != REG_SZ)
            return {};

        value.resize(read / sizeof(wchar_t));

        while(!value.empty() && value.back() == L'\0')
            value.pop_back();

        return value;
    }

    std::wstring buildAutoStartCommand(const path &exePath, const path &configPath){
        return L"\"" + exePath.wstring() + L"\" --tray \"" + configPath.wstring() + L"\"";
    }

    path currentExePath(){
        std::wstring buffer(32768, L'\0');

        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size())
        );

        buffer.resize(length);

        return path{buffer};
    }

    bool syncAutoStart(bool enabled, const path &exePath, const path &configPath){
        auto key = openRunKey();

        if(!key){
            spdlog::warn("autostart: cannot open HKCU Run Key");
            return false;
        }

        const std::wstring name{kAutostartValueName};
        const std::wstring current = readEntry(key.get());

        if(!enabled){
            if(current.empty())
                return true;

            if(RegDeleteValueW(key.get(), name.c_str()) != ERROR_SUCCESS){
                spdlog::warn("autostart: cannot remove Run entry");
                return false;
            }

            spdlog::info("autostart: removed login entry");
            return true;
        }

        const std::wstring desired = buildAutoStartCommand(exePath, configPath);

        if(current == desired)
            return true;

        const auto bytes = static_cast<DWORD>((desired.size() + 1) * sizeof(wchar_t));

        if(RegSetValueExW(key.get(), name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE *>(desired.c_str()), bytes) != ERROR_SUCCESS){
            spdlog::warn("autostart: cannot write Run entry");
            return false;
        }

        spdlog::info("autostart: login entry set");
        
        return true;
    }
}