#pragma once

#include <span>

#include "registry.hpp"

namespace eventfix::cave_of_nagi
{

// Bypass records for Cave of Nagi's forced-tutorial softlock. See
// docs/event-triggers-runtime.md for the runtime model and chain.
std::span<const EventBypass> getBypasses();

} // namespace eventfix::cave_of_nagi
