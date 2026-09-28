Gorilla TPFanControl - portable
===============================

Fan control for Lenovo ThinkPad laptops, as a folder that runs from
anywhere (a USB stick too) without installing. It is the same program as
the installer: a fork of TPFanCtrl2 with a readable window, a normal title
bar and a manual level that survives a restart. The fan logic is unchanged.

ONLY FOR THINKPADS
  It controls the fan by writing to ThinkPad embedded-controller registers.
  On any other laptop the same registers can mean something else. Do not
  run it on anything but a ThinkPad.

  Needs 64-bit Windows 10 (version 1809 or newer) or Windows 11, on an
  Intel or AMD processor. Not ARM laptops.

FIRST TIME ON A COMPUTER
  1. Double-click "1 - Install PawnIO driver (needs admin).cmd".
     PawnIO is the signed driver that lets the program reach the fan chip.
     It is installed once per computer; its installer is in the PawnIO folder.
  2. Double-click "2 - Run TPFanControl (needs admin).cmd".

  Windows asks for administrator rights every time you start it; the fan
  chip is only reachable with them. To start it at sign-in without a
  prompt, use the installer (Gorilla-TPFanControl-...-Setup.exe) instead.

SETTINGS
  TPFanControl.ini in this folder. A mode or level you pick in the window is
  saved there and used again next time. Minimise hover colour:
  CaptionHoverColor=RRGGBB.

STOPPING
  Close (X) hides the window in the tray; the fan control keeps running.
  Use Exit in the tray menu to stop it. On exit, the fan goes back to BIOS
  control.

LICENCES
  See the licences folder: TPFanControl (Unlicense), the LpcACPIEC module
  (LGPL-2.1), and the PawnIO driver (GPL-2.0, source at
  https://github.com/namazso/PawnIO).
