#include "../include/frostmonitor/single_instance.hpp"

#include <string>

namespace frostmonitor {
    SingleInstanceGuard::SingleInstanceGuard(std::wstring_view name)
        : mutex_(CreateMutexW(nullptr, FALSE, std::wstring(name).c_str()))
    {
        if(mutex_ != nullptr)
            alreadyExists_ = (GetLastError() == ERROR_ALREADY_EXISTS);
    }

    SingleInstanceGuard::~SingleInstanceGuard(){
        if(mutex_ != nullptr)
            CloseHandle(mutex_);
    }

    auto SingleInstanceGuard::isPrimary() const noexcept -> bool {
        return mutex_ != nullptr && !alreadyExists_;
    }
}