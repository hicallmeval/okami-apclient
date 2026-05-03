#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

class CheckMan;

namespace warpwindow
{

/**
 * @brief Initialize Warp Window state
 *
 * Call this once during mod initialization. The CheckMan reference is
 * used to suppress check sending during a warp transition; the post-load
 * onPlayStart callback re-enables sending automatically.
 */
void initialize(CheckMan &checkMan);

/**
 * @brief Cleanup Warp Window resources
 */
void shutdown();

/**
 * @brief Draw Warp Window content
 *
 * Called by the GUI manager inside the ImGui frame.
 * Shows warp preset selection and manual coordinate entry.
 */
void draw();

/**
 * @brief Check if window is visible
 */
bool isVisible();

/**
 * @brief Set window visibility
 */
void setVisible(bool visible);

/**
 * @brief Toggle warp window visibility
 */
void toggle();

} // namespace warpwindow
