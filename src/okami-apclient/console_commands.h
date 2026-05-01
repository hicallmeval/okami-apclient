#pragma once

class ISocket;
class RewardMan;

namespace console_commands
{

/// Register the AP-related developer console commands with WOLF.
/// Idempotent; safe to call once during late init. Lifetime of socket and
/// rewardMan must outlast the registration (currently both are owned for the
/// duration of the mod).
void registerAll(ISocket &socket, RewardMan &rewardMan);

} // namespace console_commands
