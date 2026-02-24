#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Core/Terminal.h"

#include <string>

namespace fst {
class Context;
}

namespace fin {

void RenderTerminalPanel(
    fst::Context& ctx,
    Terminal& terminal,
    std::string& terminalHistory,
    std::string& terminalInput);

} // namespace fin
