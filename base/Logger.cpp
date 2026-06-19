#include "manager/editor/ConsoleLog.h"
#include <Windows.h>

namespace Logger
{
    // UTF-8からUTF-16への変換関数
    std::wstring Utf8ToUtf16(const std::string& utf8)
    {
        if (utf8.empty()) {
            return std::wstring();
        }
        // 必要なバッファサイズを計算
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring utf16(size_needed, 0);
        // 変換を実行
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &utf16[0], size_needed);
        return utf16;
    }

    void Log(const std::string& message, LogLevel level)
    {
        // UTF-16に変換してデバッグ出力
        OutputDebugStringW(Utf8ToUtf16(message).c_str());

        // ConsoleLogが有効であればログを追加
        if (ConsoleLog::HasInstance())
        {
            ConsoleLog::GetInstance()->AddLog(message, level);
        }
    }
}
