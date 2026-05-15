#pragma once

#include <span>

#include "registry.hpp"

namespace eventfix::kamiki_village
{

std::span<const EventBypass> getBypasses();

} // namespace eventfix::kamiki_village
