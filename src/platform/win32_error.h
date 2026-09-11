#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace myytm::platform {

inline std::string lastWin32Error(unsigned long code = 0) {
#ifdef _WIN32
    if (code == 0) code = GetLastError();
    char* buf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<char*>(&buf), 0, nullptr);
    std::string s = buf ? buf : "Unknown error";
    if (buf) LocalFree(buf);
    // Trim
    while (!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ')) s.pop_back();
    return s + " (" + std::to_string(code) + ")";
#else
    (void)code;
    return "not windows";
#endif
}

} // namespace myytm::platform
