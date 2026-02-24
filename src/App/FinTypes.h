#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/LSPClient.h"
#include "fastener/fastener.h"

#include <string>
#include <vector>

namespace fin {

struct DocumentTab {
    std::string id;
    std::string name;
    std::string path;
    std::string lspDocumentPath;
    fst::TextEditor editor;
    std::string savedText;
    bool dirty = false;

    bool lspOpened = false;
    std::string lspTextSnapshot;
    std::vector<LSPDiagnostic> lspDiagnostics;
};

} // namespace fin
