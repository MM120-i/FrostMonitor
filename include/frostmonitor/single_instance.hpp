#pragma once

#include <string_view>
#include <windows.h>

namespace frostmonitor {
    constexpr std::wstring_view kSingleInstanceMutexName = L"FrostMonitorSingleInstance";

    class SingleInstanceGuard {
    private:
        HANDLE mutex_{nullptr};
        bool alreadyExists_{false};

    public:
        explicit SingleInstanceGuard(std::wstring_view name);
        ~SingleInstanceGuard();

        SingleInstanceGuard(const SingleInstanceGuard &) = delete;
        SingleInstanceGuard(SingleInstanceGuard &&) = delete;
        SingleInstanceGuard &operator = (const SingleInstanceGuard &) = delete;
        SingleInstanceGuard &operator = (SingleInstanceGuard &&) = delete;

        [[nodiscard]] auto isPrimary() const noexcept -> bool;
    };
}