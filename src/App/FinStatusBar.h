#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "App/FinTypes.h"
#include "fastener/fastener.h"

#include <memory>
#include <string>
#include <vector>

namespace fin {

void RenderStatusBar(
    fst::Context& ctx,
    float windowW,
    float windowH,
    float statusBarHeight,
    const std::string& statusText,
    const std::vector<std::unique_ptr<DocumentTab>>& docs,
    int activeTab);

} // namespace fin
