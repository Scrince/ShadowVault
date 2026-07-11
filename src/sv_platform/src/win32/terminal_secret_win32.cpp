#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../terminal_secret_detail.hpp"

#include <conio.h>
#include <iostream>
#include <string>

namespace sv::platform::detail {

namespace {

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.data(),
        static_cast<int>(wide.size()),
        utf8.data(),
        required,
        nullptr,
        nullptr);
    return utf8;
}

Result<std::string> platform_error(const char* context) {
    const DWORD error = GetLastError();
    return Result<std::string>::err(
        ErrorCode::PlatformError,
        std::string(context) + " (Win32 error " + std::to_string(error) + ")");
}

Result<std::string> read_console_no_echo() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE) {
        return platform_error("GetStdHandle failed");
    }

    DWORD mode = 0;
    if (GetConsoleMode(input, &mode) == 0) {
        return platform_error("GetConsoleMode failed");
    }

    const DWORD original_mode = mode;
    mode &= ~static_cast<DWORD>(ENABLE_ECHO_INPUT);
    if (SetConsoleMode(input, mode) == 0) {
        return platform_error("SetConsoleMode failed");
    }

    std::wstring wide;
    wchar_t buffer[256] {};
    DWORD read = 0;

    while (true) {
        if (ReadConsoleW(input, buffer, static_cast<DWORD>(std::size(buffer)), &read, nullptr) == 0) {
            SetConsoleMode(input, original_mode);
            return platform_error("ReadConsoleW failed");
        }

        if (read == 0) {
            break;
        }

        for (DWORD i = 0; i < read; ++i) {
            const wchar_t ch = buffer[i];
            if (ch == L'\r' || ch == L'\n') {
                SetConsoleMode(input, original_mode);
                return Result<std::string>::ok(wide_to_utf8(wide));
            }
            if (ch == L'\b') {
                if (!wide.empty()) {
                    wide.pop_back();
                }
                continue;
            }
            if (ch == 0x03) { // Ctrl+C
                SetConsoleMode(input, original_mode);
                return Result<std::string>::err(ErrorCode::PlatformError, "input interrupted");
            }
            wide.push_back(ch);
        }
    }

    SetConsoleMode(input, original_mode);
    return Result<std::string>::ok(wide_to_utf8(wide));
}

Result<std::string> read_getch_no_echo() {
    std::string value;
    while (true) {
        const int ch = _getch();
        if (ch == '\r' || ch == '\n') {
            break;
        }
        if (ch == '\b') {
            if (!value.empty()) {
                value.pop_back();
            }
            continue;
        }
        if (ch == 3) {
            return Result<std::string>::err(ErrorCode::PlatformError, "input interrupted");
        }
        value.push_back(static_cast<char>(ch));
    }
    return Result<std::string>::ok(std::move(value));
}

} // namespace

Result<std::string> read_line_no_echo() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (input != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(input, &mode) != 0) {
            return read_console_no_echo();
        }
    }

    return read_getch_no_echo();
}

} // namespace sv::platform::detail

#endif