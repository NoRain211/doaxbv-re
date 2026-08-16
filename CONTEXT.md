# DOAXBV Native Port

This context names the product and custody boundaries of the native PC port.
It keeps public planning precise without exposing locally derived game content.

## Product boundary

**Host shell**:
The distributable, game-independent Windows executable that provides the native
runtime and loads a locally prepared game payload.
_Avoid_: Complete game binary, emulator

**Game import**:
The local transformation of a user-owned verified ISO or XISO into the private
inputs consumed by the host shell.
_Avoid_: Game download, bundled installer

**Private game payload**:
The locally derived game logic and data consumed by the host shell, including
generated modules and extracted assets that must never enter the public tree.
_Avoid_: Public game assets, redistributable payload

**Verified disc revision**:
The one explicitly authenticated retail disc identity supported by the first
feature-complete target.
_Avoid_: Any valid disc, generic retail build

## Completion boundary

**Feature complete**:
Every offline mode, activity, media path, and save flow on the verified disc
revision is usable through the host shell.
_Avoid_: Fully decompiled, cycle accurate

**Functional parity**:
The scoped game produces equivalent player-observable outcomes while documented
presentation and timing differences remain acceptable.
_Avoid_: Byte matching, pixel matching

**Presentation modernization**:
The supported modern display and rendering behavior layered over functional
parity, including high-resolution, widescreen, high-refresh, and visual options.
_Avoid_: Accuracy mode, hardware emulation

**Pressure tier**:
One of the low, medium, or high virtual face-button intensities that a user can
bind to modern digital controller or keyboard input.
_Avoid_: Analog face button
