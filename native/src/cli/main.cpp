// canalis 実行ファイルの入口 (src/cli.ts の main)。コマンドの中身は cli/commands。
// 例外は `canalis: <message>` を標準エラーへ出して exit 1 (TS と同じ)。
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {

void write_all(std::FILE* f, std::string_view s) {
    std::fwrite(s.data(), 1, s.size(), f);
    std::fflush(f);
}

std::optional<std::string> read_stdin(std::size_t max_bytes) {
    std::string data;
    char buf[65536];
    while (true) {
        const std::size_t n = std::fread(buf, 1, sizeof buf, stdin);
        if (n == 0) break;
        if (data.size() + n > max_bytes) return std::nullopt;
        data.append(buf, n);
    }
    return data;
}

bool stdin_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

/** 端末のエコーを止めて 1 行読む (パスワード入力の定石)。 */
std::string prompt_hidden(std::string_view label) {
    write_all(stderr, label);
    std::string line;
#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool got = GetConsoleMode(in, &mode) != 0;
    if (got) SetConsoleMode(in, mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    std::wstring wline;
    wchar_t ch = 0;
    DWORD read = 0;
    while (ReadConsoleW(in, &ch, 1, &read, nullptr) && read == 1 && ch != L'\n') {
        if (ch != L'\r') wline.push_back(ch);
        if (wline.size() > 65536) break;
    }
    if (got) SetConsoleMode(in, mode);
    if (!wline.empty()) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, wline.data(), static_cast<int>(wline.size()), nullptr, 0, nullptr, nullptr);
        line.assign(static_cast<std::size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wline.data(), static_cast<int>(wline.size()), line.data(), n, nullptr, nullptr);
    }
#else
    termios old{};
    const bool got = tcgetattr(STDIN_FILENO, &old) == 0;
    if (got) {
        termios silent = old;
        silent.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &silent);
    }
    std::getline(std::cin, line);
    if (got) tcsetattr(STDIN_FILENO, TCSANOW, &old);
#endif
    write_all(stderr, "\n");
    return line;
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    // 出力は UTF-8 のバイト列のまま書く (改行の変換もしない)。
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::vector<std::string> args;
#ifdef _WIN32
    // argv は ANSI コードページなので、日本語の URL やパスを UTF-8 で受け取るためにワイド文字の引数から作り直す。
    (void)argc;
    (void)argv;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    for (int i = 1; wargv && i < wargc; ++i) {
        const int len = static_cast<int>(wcslen(wargv[i]));
        const int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], len, nullptr, 0, nullptr, nullptr);
        std::string arg(static_cast<std::size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], len, arg.data(), n, nullptr, nullptr);
        args.push_back(std::move(arg));
    }
    if (wargv) LocalFree(wargv);
#else
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
#endif
    canalis::cli::Io io;
    io.out = [](std::string_view s) { write_all(stdout, s); };
    io.err = [](std::string_view s) { write_all(stderr, s); };
    io.read_stdin = read_stdin;
    io.stdin_is_tty = stdin_is_tty;
    io.prompt_hidden = prompt_hidden;
    try {
        return canalis::cli::run(args, io);
    } catch (const std::exception& e) {
        write_all(stderr, std::string("canalis: ") + e.what() + "\n");
        return 1;
    }
}
