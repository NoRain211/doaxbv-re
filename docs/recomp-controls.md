# Native controller controls

The Windows host currently reads one XInput controller on port 0. Xbox Series
controller menu navigation and scored Exhibition play have been observed.
Physical two-controller play remains unproven.

The original game contains separate digital and analog control modes, including
button assignments. The current candidate starts in Digital, including when
loading an older Analog save. An explicit later choice in Controller Settings
is remembered as a PC preference. A manual restart restored an explicit Analog
choice, followed by the player selecting and storing Digital. Use its digital mode for an XInput
controller: the host's face buttons report released or fully pressed, while
triggers retain their
analog values. The original digital volleyball defaults are:

| Xbox button | Action |
|---|---|
| A | Strong attack |
| X | Soft attack |
| B | Strong receive |
| Y | Soft receive |

The pool-jump input path reuses the digital strength choice: with these default
bindings, A/B select a long jump (two pads), and X/Y select a short jump (one
pad). These mappings pass isolated checks of the current compiled game code;
natural short/long jumps in the minigame still need manual verification.

These are existing game bindings, not newly added host remapping. The traced
volleyball pressure path selects soft or strong using a calibrated threshold.
The downstream action checks use that category, not a pressure gradient within
soft or strong. An exhaustive isolated check of all 256 pressure values, both
action channels and three thresholds passes (1,536 cases). This establishes the
traced input decision, not identical shot outcomes across different timing or
game states. No distinct medium tier has been found. Button configuration can
change the defaults above.

The runtime input model and adapter preserve all eight pressure bytes. The
current Windows XInput backend does not expose analog face-button pressure;
support for a pressure-capable device requires a backend that supplies it.

Scripted game input has selected Digital Control, exited the controller menu,
and reopened it with the setting retained and the soft/strong labels visible.
In a running Exhibition match, scripted full-pressure face-button inputs also
produced the distinct strong masks for A/B and soft masks for X/Y. Actual shot
outcomes and physical-controller strength selection still need verification.
Earlier candidates restored an existing vacation save's Analog setting; the
current candidate applies the separate explicit PC preference after loading.

Scripted input also opened Analog calibration from a paused Exhibition match
and returned to visible active play twice without crashing (#31 complete).
The native Digital configuration prototype is accepted (#10 complete).
Physical shot outcomes and pool-jump verification remain under offline
acceptance #16; the prototype does not establish complete controller support.

Manual sessions use physical host input without scripted pulses. Keyboard
bindings apply only while the game process owns the foreground window. A
reported hands-off menu back-out remains under investigation; this focus guard
is not established as its cause or complete fix.
