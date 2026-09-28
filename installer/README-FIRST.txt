Gorilla TPFanControl
====================

Fan control for Lenovo ThinkPad laptops. A fork of TPFanCtrl2
(https://github.com/mews-se/TPFanCtrl2) with a readable window, a normal
title bar, and a manual fan level that survives a restart. The fan logic
itself is unchanged from upstream.

WHAT THIS INSTALLS
  - The program, in Program Files.
  - The PawnIO driver (version 2.2.0), if it is missing or older. PawnIO is
    the signed driver that lets the program talk to the laptop's fan chip.
    It is bundled, so no internet connection is needed.
  - A start-at-sign-in task, so the fan control starts with administrator
    rights at every sign-in without asking you.

WHAT IT NEEDS
  - A Lenovo ThinkPad. The installer checks and refuses on anything else.
  - 64-bit Windows 10 (version 1809 or newer) or Windows 11, on an Intel
    or AMD processor. Not ARM laptops.

HOW IT BEHAVES
  - Smart mode follows the fan curve in TPFanControl.ini.
  - Manual mode holds the level you pick, and it is remembered after a
    restart. If anything reaches 78 C it switches back to Smart by itself.
  - Close (X) hides the window in the tray; the fan control keeps running.
    Exit is in the tray menu. On exit, the fan goes back to BIOS control.

UNINSTALL
  Settings > Apps > Installed apps > Gorilla TPFanControl. The fan goes
  back to BIOS control. Your settings file and the PawnIO driver are kept.
