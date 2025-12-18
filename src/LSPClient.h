#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

struct LSPDiagnostic {
    int line;
    int range_start;
    int range_end;
    std::string message;
    int severity; // 1: Error, 2: Warning
};

struct LSPCompletionItem {
    std::string label;
    std::string detail;
    std::string insertText;
};

class LSPClient {
public:
    LSPClient();
    ~LSPClient();

    bool Start(const std::string& clangdPath = "clangd");
    void Stop();

    void Initialize(const std::string& rootPath);
    void DidOpen(const std::string& uri, const std::string& text);
    void DidChange(const std::string& uri, const std::string& text);

    // Completion
    void RequestCompletion(const std::string& uri, int line, int character, std::function<void(const std::vector<LSPCompletionItem>&)> cb);

    // Diagnostics callback
    void SetDiagnosticsCallback(std::function<void(const std::string&, const std::vector<LSPDiagnostic>&)> cb);

private:
    void SendRequest(const std::string& method, nlohmann::json params);
    void SendNotification(const std::string& method, nlohmann::json params);
    void WriteToPipe(const std::string& data);
    void ReadLoop();

    bool m_running = false;
    std::thread m_readThread;
    std::function<void(const std::string&, const std::vector<LSPDiagnostic>&)> m_diagCallback;

#ifdef _WIN32
    HANDLE m_hChildStdInRead = NULL;
    HANDLE m_hChildStdInWrite = NULL;
    HANDLE m_hChildStdOutRead = NULL;
    HANDLE m_hChildStdOutWrite = NULL;
    PROCESS_INFORMATION m_pi;
#endif

    int m_nextId = 1;
    std::mutex m_handlersMutex;
    std::map<int, std::function<void(const nlohmann::json&)>> m_responseHandlers;
};
