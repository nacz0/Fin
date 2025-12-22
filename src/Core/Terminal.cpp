#include "Terminal.h"
#include <iostream>

Terminal::Terminal() {
#ifdef _WIN32
    ZeroMemory(&m_pi, sizeof(m_pi));
#endif
}

Terminal::~Terminal() {
    Stop();
}

bool Terminal::Start() {
    if (m_running) return true;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutWrite = NULL;
    HANDLE hStdInRead = NULL;

    if (!CreatePipe(&m_hStdOutRead, &hStdOutWrite, &sa, 0)) return false;
    if (!SetHandleInformation(m_hStdOutRead, HANDLE_FLAG_INHERIT, 0)) return false;

    if (!CreatePipe(&hStdInRead, &m_hStdInWrite, &sa, 0)) return false;
    if (!SetHandleInformation(m_hStdInWrite, HANDLE_FLAG_INHERIT, 0)) return false;

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.hStdInput = hStdInRead;
    si.dwFlags |= STARTF_USESTDHANDLES;

    char cmdLine[] = "cmd.exe /Q /K echo Fin Terminal :)"; // /Q for no echo of commands, /K to keep open

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &m_pi)) {
        return false;
    }

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdInRead);

    m_running = true;
    m_readThread = std::thread(&Terminal::ReadLoop, this);
    return true;
#else
    return false;
#endif
}

void Terminal::Stop() {
    bool expected = true;
    if (!m_running.compare_exchange_strong(expected, false)) return;

#ifdef _WIN32
    if (m_pi.hProcess) {
        TerminateProcess(m_pi.hProcess, 0);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
        m_pi.hProcess = NULL;
        m_pi.hThread = NULL;
    }
    if (m_hStdInWrite) { CloseHandle(m_hStdInWrite); m_hStdInWrite = NULL; }
    if (m_hStdOutRead) { CloseHandle(m_hStdOutRead); m_hStdOutRead = NULL; }
#endif

    if (m_readThread.joinable()) m_readThread.detach();
}

void Terminal::SendInput(const std::string& input) {
    if (!m_running || !m_hStdInWrite) return;
    
    std::string data = input + "\n";
    DWORD written;
    WriteFile(m_hStdInWrite, data.c_str(), (DWORD)data.length(), &written, NULL);
}

std::string Terminal::GetOutput() {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    std::string out = m_outputBuffer;
    m_outputBuffer.clear();
    return out;
}

void Terminal::ReadLoop() {
    char buffer[4096];
    while (m_running) {
        if (!m_hStdOutRead) break;
        DWORD read;
        if (!ReadFile(m_hStdOutRead, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) break;
        
        buffer[read] = '\0';
        {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            m_outputBuffer += std::string(buffer, read);
        }
    }
}
