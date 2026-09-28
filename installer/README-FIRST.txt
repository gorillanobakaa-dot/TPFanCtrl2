Gorilla TPFanControl
====================

Fan control for Lenovo ThinkPad laptops. A fork of TPFanCtrl2
(https://github.com/mews-se/TPFanCtrl2) with a readable window, a normal
title bar, a manual fan level that survives a restart, and Gorilla mode
(cooling first). BIOS, Smart and Manual work exactly as upstream.

WHAT THIS INSTALLS
  - The program, in Program Files.
  - The PawnIO driver (version 2.2.0), if it is missing or older. PawnIO is
    the signed driver that lets the program talk to the laptop's fan chip.
    It is bundled, so no internet connection is needed.
  - The fan control as a Windows service ("TPFanControl"). It starts when
    the computer starts, before anyone signs in, without asking you.
  - The window, which starts at every sign-in. It is a remote control for
    the service and needs no administrator rights.
  - An earlier version's start-at-sign-in task is removed.

WHAT IT NEEDS
  - A Lenovo ThinkPad. The installer checks and refuses on anything else.
  - 64-bit Windows 10 (version 1809 or newer) or Windows 11, on an Intel
    or AMD processor. Not ARM laptops.

HOW IT BEHAVES
  - Smart mode follows the fan curve in TPFanControl.ini.
  - Manual mode holds the level you pick, and it is remembered after a
    restart. If anything reaches 78 C it switches back to Smart by itself.
  - Gorilla mode keeps the CPU at the temperature you pick (40, 45 or 50 C):
    full speed at that temperature and above, level 7 within 5 C below it,
    and never slower than the minimum you pick. Noise is not a factor.
    It is remembered after a restart.
  - Close (X) hides the window in the tray; the fan control keeps running.
    Exit in the tray menu stops the fan control too, and the fan goes back
    to BIOS control until the next restart.

UNINSTALL
  Settings > Apps > Installed apps > Gorilla TPFanControl. The fan goes
  back to BIOS control. Your settings file and the PawnIO driver are kept.
