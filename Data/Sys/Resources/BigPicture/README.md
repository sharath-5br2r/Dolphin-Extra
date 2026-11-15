# Big Picture artwork and fonts

The home-screen and launch icons are the same artwork used by the local PCSX2 fork at
adca25d5995fbec3dbabbc061518d332a5ea935b, in bin/resources/fullscreenui:
media-cdrom.svg, start-game.svg, applications-system.svg, exit.svg, start-file.svg,
drive-cdrom.svg, and start-bios.svg.
Their original SVG Repo metadata is retained in the SVG sources. They are
redistributed from PCSX2 under its GPL-3.0-or-later terms; see COPYING.GPLv3.
The original applications-system artwork is also offered under CC0 at
https://www.svgrepo.com/svg/451997/applications-system .

The 512 x 512 transparent PNGs are rasterizations of these SVGs, without changes
to their shapes or padding. Rendering tints their white pixels to the active theme.
No PCSX2 implementation code is included.

Roboto-Regular.ttf: Copyright 2015 Google Inc., Apache-2.0.
The upstream packaging notice is in Roboto-LICENSE.txt; the full license text is
in Apache-2.0.txt. Source: https://github.com/google/roboto .

promptfont.otf: PromptFont, SIL Open Font License 1.1; see promptfont-LICENSE.txt.
Source: https://github.com/Shinmera/promptfont .
The font supplies the keyboard keycaps in the home and pause-screen footers.

fa-solid-900.ttf: Font Awesome Free, SIL Open Font License 1.1; see
FontAwesome-LICENSE.txt. This is the same font file as bin/resources/fonts/fa-solid-900.ttf
in the local PCSX2 fork. The settings tabs and menu controls use its semantic icons
alongside PromptFont's system and controller glyphs.
Source and license: https://github.com/FortAwesome/Font-Awesome .

The Dolphin logo uses the existing ../dolphin_logo.png resource.
