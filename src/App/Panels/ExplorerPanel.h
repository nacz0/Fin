#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace fst {
class Context;
}

namespace fin {

using OpenDocumentFn = std::function<bool(const std::string&)>;

void RenderExplorerPanel(
    fst::Context& ctx,
    std::filesystem::path& currentPath,
    const OpenDocumentFn& openDocument);

} // namespace fin