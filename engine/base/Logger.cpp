#include "base/Logger.h"

#include <Windows.h>

namespace Logger
{
    // UTF-8からUTF-16への変換関数
    std::wstring Utf8ToUtf16(const std::string& utf8)
    {
        if (utf8.empty()) {
            return std::wstring();
        }
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring utf16(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &utf16[0], size_needed);
        return utf16;
    }

    void Log(const std::string& message)
    {
        OutputDebugStringW(Utf8ToUtf16(message).c_str());
    }
}
