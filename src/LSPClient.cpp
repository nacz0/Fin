#include "LSPClient.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

LSPClient::LSPClient() {}

LSPClient::~LSPClient() {
    Stop();
}

bool LSPClient::Start(const std::string& clangdPath) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hChildStdOutRead, &m_hChildStdOutWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(m_hChildStdOutRead, HANDLE_FLAG_INHERIT, 0)) return false;

    if (!CreatePipe(&m_hChildStdInRead, &m_hChildStdInWrite, &saAttr, 0)) return false;
    if (!SetHandleInformation(m_hChildStdInWrite, HANDLE_FLAG_INHERIT, 0)) return false;

    STARTUPINFOA siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = m_hChildStdOutWrite;
    siStartInfo.hStdOutput = m_hChildStdOutWrite;
    siStartInfo.hStdInput = m_hChildStdInRead;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&m_pi, sizeof(PROCESS_INFORMATION));

    std::string cmd = clangdPath + " --log=error";
    if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &m_pi)) {
        return false;
    }

    m_running = true;
    m_readThread = std::thread(&LSPClient::ReadLoop, this);
    return true;
#else
    return false; // Implementacja dla innych systemów jeśli potrzebna
#endif
}

void LSPClient::Stop() {
    m_running = false;
#ifdef _WIN32
    if (m_pi.hProcess) {
        TerminateProcess(m_pi.hProcess, 0);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
    }
    if (m_hChildStdInWrite) CloseHandle(m_hChildStdInWrite);
    if (m_hChildStdOutRead) CloseHandle(m_hChildStdOutRead);
#endif
    if (m_readThread.joinable()) m_readThread.join();
}

void LSPClient::Initialize(const std::string& rootPath) {
    std::string path = rootPath;
    std::replace(path.begin(), path.end(), '\\', '/');
    json params = {
        {"processId", GetCurrentProcessId()},
        {"rootPath", path},
        {"rootUri", "file:///" + path},
        {"capabilities", json::object()}
    };
    SendRequest("initialize", params);
    SendNotification("initialized", json::object());
}

void LSPClient::DidOpen(const std::string& uri, const std::string& text) {
    std::string path = uri;
    std::replace(path.begin(), path.end(), '\\', '/');
    std::cout << "[LSP] DidOpen: file:///" << path << " (" << text.length() << " bytes)" << std::endl;
    json params = {
        {"textDocument", {
            {"uri", "file:///" + path},
            {"languageId", "cpp"},
            {"version", 1},
            {"text", text}
        }}
    };
    SendNotification("textDocument/didOpen", params);
}

void LSPClient::DidChange(const std::string& uri, const std::string& text) {
    std::string path = uri;
    std::replace(path.begin(), path.end(), '\\', '/');
    json params = {
        {"textDocument", {
            {"uri", "file:///" + path},
            {"version", 2} // Uproszczone: wersja powinna rosnąć
        }},
        {"contentChanges", json::array({{{"text", text}}})}
    };
    SendNotification("textDocument/didChange", params);
}

void LSPClient::RequestCompletion(const std::string& uri, int line, int character, std::function<void(const std::vector<LSPCompletionItem>&)> cb) {
    int id = m_nextId++;
    std::string path = uri;
    std::replace(path.begin(), path.end(), '\\', '/');
    std::cout << "[LSP] RequestCompletion: file:///" << path << " at line=" << line << " char=" << character << std::endl;
    json params = {
        {"textDocument", {{"uri", "file:///" + path}}},
        {"position", {{"line", line}, {"character", character}}}
    };

    {
        std::lock_guard<std::mutex> lock(m_handlersMutex);
        m_responseHandlers[id] = [cb](const json& result) {
            try {
                std::vector<LSPCompletionItem> items;
                json list;
                if (result.is_array()) {
                    list = result;
                } else if (result.is_object() && result.contains("items") && result["items"].is_array()) {
                    list = result["items"];
                } else {
                    cb(items);
                    return;
                }

                std::cout << "[LSP] Received completion result with " << list.size() << " items" << std::endl;
                for (auto& i : list) {
                    LSPCompletionItem item;
                    item.label = i.value("label", "???");
                    item.detail = i.value("detail", "");
                    item.insertText = i.value("insertText", item.label);
                    items.push_back(item);
                }
                cb(items);
            } catch (const std::exception& e) {
                std::cerr << "[LSP] Exception in completion handler: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[LSP] Unknown exception in completion handler" << std::endl;
            }
        };
    }

    std::cout << "[LSP] Sending completion request at " << line << ":" << character << std::endl;
    json req = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "textDocument/completion"},
        {"params", params}
    };
    WriteToPipe(req.dump());
}

void LSPClient::SendRequest(const std::string& method, json params) {
    json req = {
        {"jsonrpc", "2.0"},
        {"id", m_nextId++},
        {"method", method},
        {"params", params}
    };
    WriteToPipe(req.dump());
}

void LSPClient::SendNotification(const std::string& method, json params) {
    json notif = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    WriteToPipe(notif.dump());
}

void LSPClient::WriteToPipe(const std::string& data) {
    std::string header = "Content-Length: " + std::to_string(data.length()) + "\r\n\r\n";
    std::string full = header + data;
    DWORD written;
    WriteFile(m_hChildStdInWrite, full.c_str(), (DWORD)full.length(), &written, NULL);
}

void LSPClient::ReadLoop() {
    char buffer[4096];
    std::string leftover;
    while (m_running) {
        DWORD read;
        if (!ReadFile(m_hChildStdOutRead, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) break;
        buffer[read] = '\0';
        leftover += std::string(buffer, read);

        while (true) {
            size_t pos = leftover.find("Content-Length: ");
            if (pos == std::string::npos) break;
            size_t endHeader = leftover.find("\r\n\r\n", pos);
            if (endHeader == std::string::npos) break;

            int len = 0;
            try {
                len = std::stoi(leftover.substr(pos + 16, endHeader - (pos + 16)));
            } catch (...) {
                // Uszkodzony nagłówek - usuń i kontynuuj
                leftover.erase(0, endHeader + 4);
                continue;
            }
            if (len <= 0 || leftover.length() < endHeader + 4 + len) break;

            std::string body = leftover.substr(endHeader + 4, len);
            leftover.erase(0, endHeader + 4 + len);

            try {
                json msg = json::parse(body);
                if (msg.contains("method") && msg["method"] == "textDocument/publishDiagnostics") {
                    if (msg.contains("params") && msg["params"].is_object()) {
                        auto params = msg["params"];
                        if (params.contains("uri") && params["uri"].is_string()) {
                            std::string uri = params["uri"];
                            std::vector<LSPDiagnostic> diags;
                            if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
                                for (auto& d : params["diagnostics"]) {
                                    if (d.contains("range") && d["range"].is_object()) {
                                        LSPDiagnostic ld;
                                        ld.line = d["range"]["start"]["line"];
                                        ld.range_start = d["range"]["start"]["character"];
                                        ld.range_end = d["range"]["end"]["character"];
                                        ld.message = d.value("message", "");
                                        ld.severity = d.value("severity", 1);
                                        diags.push_back(ld);
                                    }
                                }
                            }
                            if (m_diagCallback) m_diagCallback(uri, diags);
                        }
                    }
                } else if (msg.contains("id") && (msg["id"].is_number() || msg["id"].is_string())) {
                    // LSP może zwracać ID jako liczbę lub string
                    int id = -1;
                    if (msg["id"].is_number()) id = msg["id"];
                    else if (msg["id"].is_string()) {
                        try { id = std::stoi(msg["id"].get<std::string>()); } catch(...) {}
                    }
                    
                    if (id != -1) {
                        std::function<void(const json&)> handler;
                        {
                            std::lock_guard<std::mutex> lock(m_handlersMutex);
                            if (m_responseHandlers.count(id)) {
                                handler = m_responseHandlers[id];
                                m_responseHandlers.erase(id);
                            }
                        }
                        if (handler && msg.contains("result")) {
                            handler(msg["result"]);
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[LSP] Error parsing message body: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[LSP] Unknown error parsing message body" << std::endl;
            }
        }
    }
}

void LSPClient::SetDiagnosticsCallback(std::function<void(const std::string&, const std::vector<LSPDiagnostic>&)> cb) {
    m_diagCallback = cb;
}
