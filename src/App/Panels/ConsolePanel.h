#pragma once

#include "App/FinTypes.h"
#include "Core/Compiler.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fst {
class Context;
}

namespace fin {

using ClampActiveTabFn = std::function<void()>;
using OpenDocumentFn = std::function<bool(const std::string&)>;

void RenderConsolePanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int& activeTab,
    const std::vector<ParsedError>& errorList,
    std::string& compilationOutput,
    const OpenDocumentFn& openDocument,
    const ClampActiveTabFn& clampActiveTab);

} // namespace fin