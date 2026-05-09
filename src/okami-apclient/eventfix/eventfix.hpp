#pragma once

namespace eventfix
{

// Install all registered event bypasses. Call once during late game
// init, after wolf is fully bound. Walks each per-map module's bypass
// list and MinHooks the first-link callback with its replacement stub.
//
// See docs/event-triggers-runtime.md for the runtime model and the
// "first-link" rule.
void initialize();

} // namespace eventfix
