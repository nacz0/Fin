#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

class Terminal {
public:
    Terminal();
    ~Terminal();

    bool Start();
    void Stop();
    bool IsRunning() const { return m_running; }

    void SendInput(const std::string& input);
    std::string GetOutput();

private:
    void ReadLoop();

    std::atomic<bool> m_running{false};
    std::thread m_readThread;
    
    std::string m_outputBuffer;
    std::mutex m_bufferMutex;

#ifdef _WIN32
    PROCESS_INFORMATION m_pi;
    HANDLE m_hStdInWrite = NULL;
    HANDLE m_hStdOutRead = NULL;
#endif
};
