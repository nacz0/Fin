#pragma once

#include "App/FinTypes.h"

#include <memory>
#include <vector>

namespace fst {
class Context;
}

namespace fin {

void RenderLspDiagnosticsPanel(
    fst::Context& ctx,
    std::vector<std::unique_ptr<DocumentTab>>& docs,
    int activeTab);

} // namespace fin
