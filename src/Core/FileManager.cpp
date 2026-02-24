#include "FileManager.h"
#include <fstream>
#include <sstream>
#include <array>
#include <cstdio>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "Comdlg32.lib")
#endif

void SaveFile(const std::string& filename, const std::string& text) {
    std::ofstream out(filename);
    out << text;
    out.close();
}

std::string OpenFile(const std::string& filename) {
    std::ifstream in(filename);
    if (in) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return "";
}

std::string ExecCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) return "popen() failed!";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string ShowOpenFileDialog(const std::string& initialDir) {
#ifdef _WIN32
    auto utf8ToWide = [](const std::string& value) -> std::wstring {
        if (value.empty()) {
            return std::wstring();
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (len <= 0) {
            return std::wstring();
        }
        std::wstring out(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), len);
        if (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    };
    auto wideToUtf8 = [](const std::wstring& value) -> std::string {
        if (value.empty()) {
            return std::string();
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return std::string();
        }
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), len, nullptr, nullptr);
        if (!out.empty() && out.back() == '\0') {
            out.pop_back();
        }
        return out;
    };

    std::vector<wchar_t> fileBuffer(4096, L'\0');
    std::wstring initialDirW = utf8ToWide(initialDir);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    ofn.lpstrFilter = L"Pliki kodu\0*.cpp;*.c;*.h;*.hpp;*.cc;*.cxx;*.hh\0Wszystkie pliki\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initialDirW.empty() ? nullptr : initialDirW.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn)) {
        return std::string();
    }
    return wideToUtf8(ofn.lpstrFile);
#else
    (void)initialDir;
    return std::string();
#endif
}

std::string ShowSaveFileDialog(const std::string& suggestedName, const std::string& initialDir) {
#ifdef _WIN32
    auto utf8ToWide = [](const std::string& value) -> std::wstring {
        if (value.empty()) {
            return std::wstring();
        }
        const int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (len <= 0) {
            return std::wstring();
        }
        std::wstring out(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), len);
        if (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    };
    auto wideToUtf8 = [](const std::wstring& value) -> std::string {
        if (value.empty()) {
            return std::string();
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) {
            return std::string();
        }
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), len, nullptr, nullptr);
        if (!out.empty() && out.back() == '\0') {
            out.pop_back();
        }
        return out;
    };

    std::vector<wchar_t> fileBuffer(4096, L'\0');
    const std::wstring suggestedNameW = utf8ToWide(suggestedName);
    const std::wstring initialDirW = utf8ToWide(initialDir);
    if (!suggestedNameW.empty()) {
        wcsncpy_s(fileBuffer.data(), fileBuffer.size(), suggestedNameW.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    ofn.lpstrFilter = L"Pliki C++\0*.cpp\0Pliki naglowkowe\0*.h;*.hpp\0Wszystkie pliki\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = initialDirW.empty() ? nullptr : initialDirW.c_str();
    ofn.lpstrDefExt = L"cpp";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    if (!GetSaveFileNameW(&ofn)) {
        return std::string();
    }
    return wideToUtf8(ofn.lpstrFile);
#else
    (void)suggestedName;
    (void)initialDir;
    return std::string();
#endif
}
