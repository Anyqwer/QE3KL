#pragma once
#include <thread>
#include "OS-ImGui/OS-ImGui.h"

// Initialize and start the ImGui overlay in a separate thread
void InitOverlay();

// Internal render loop function (called by ImGui each frame)
void RenderLoop();

// Render sniper crosshair for AWP and SSG08
void RenderSniperCrosshair();

// Thread function that runs the overlay
void OverlayThreadFunc();
