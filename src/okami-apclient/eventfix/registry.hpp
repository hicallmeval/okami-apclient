#pragma once

#include <cstdint>

namespace eventfix
{

// A bypass record: replace the function at `funcOffset` (module-relative
// to main.dll) with `stub`, a __fastcall void() replacement that does
// NOT call the original. The stub typically clears cutscene-mode bits
// via clearCutsceneModeBits() and runs whichever side-effect the
// dispatch chain still requires (e.g. grantBrush()).
//
// Map gating and AP-conditional behavior, if needed, are the stub's
// responsibility -- there's no shared predicate. Keep stubs cheap; they
// run on the game's main thread.
struct EventBypass
{
    const char *name;
    uintptr_t funcOffset;
    void (*stub)();
};

} // namespace eventfix
