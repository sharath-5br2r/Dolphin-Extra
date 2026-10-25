#!/bin/bash

# This script updates the dolphin-emu.pot file to match the strings in
# the source code.

cd "$(dirname "$0")/.."

# Scan the source code for strings and put them in dolphin-emu.pot
SRCDIR=Source

# .ui files are XML, so xgettext cannot see them. Generate the headers uic would
# generate, restore the translator comments uic drops, and scan those too.
UI_SCRATCH=Languages/.ui-scratch
rm -rf "$UI_SCRATCH"
python3 ./Languages/generate-ui-strings.py "$UI_SCRATCH" $SRCDIR > /dev/null || exit 1

{ find $SRCDIR -name '*.cpp' -o -name '*.h' -o -name '*.c'
  find "$UI_SCRATCH" -name 'ui_*.h'
} | sort -fd | \
	xgettext -p ./Languages/po -o dolphin-emu.pot --package-name="Dolphin Emulator" \
	--keyword=_ \
	--keyword=AskYesNoFmtT \
	--keyword=CriticalAlertFmtT \
	--keyword=PanicAlertFmtT \
	--keyword=PanicYesNoFmtT \
	--keyword=SuccessAlertFmtT \
	--keyword=GetStringT \
	--keyword=_trans \
	--keyword=tr:1,1t \
	--keyword=tr:1,2c \
	--keyword=QT_TR_NOOP \
	--keyword=FmtFormatT \
	--keyword=translate:2 \
	--keyword=translate:2,3c \
	--add-comments=i18n --from-code=utf-8 -f -

# Copy strings from qt-strings.pot to dolphin-emu.pot
xgettext -p ./Languages/po -o dolphin-emu.pot --package-name="Dolphin Emulator" \
  -j ./Languages/po/qt-strings.pot

sed -i "s/SOME DESCRIPTIVE TITLE\./Translation of dolphin-emu.pot to LANGUAGE/" Languages/po/dolphin-emu.pot
sed -i "s/YEAR THE PACKAGE'S COPYRIGHT HOLDER/2003-2013/" Languages/po/dolphin-emu.pot
sed -i "s/license as the PACKAGE package/license as the dolphin-emu package/" Languages/po/dolphin-emu.pot

# Point references at the .ui file rather than at the header uic generated from it.
sed -i -f Languages/ui-reference-rewrite.sed Languages/po/dolphin-emu.pot

rm -rf "$UI_SCRATCH"
