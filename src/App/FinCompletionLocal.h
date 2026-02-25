#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "App/FinTypes.h"

#include <vector>

namespace fin {

std::vector<LSPCompletionItem> CollectLocalCompletions(
    const DocumentTab& tab,
    const fst::TextPosition& cursor);

} // namespace fin
