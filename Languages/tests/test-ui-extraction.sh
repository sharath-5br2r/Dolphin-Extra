#!/bin/bash

# Checks that strings which live in Qt Designer .ui files still reach the .pot
# file. Run by hand; this fork has no CI.
#
# Set UIC if uic is not on PATH. Homebrew's Qt keeps it in libexec.

cd "$(dirname "$0")"

# Absolute, so the generator can be run from a scratch directory as well as from here.
GENERATOR=$(cd .. && pwd)/generate-ui-strings.py

SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

failures=0

fail()
{
	echo "FAIL: $1"
	failures=$((failures + 1))
}

# Writes $1 from the remaining arguments. xgettext writes no file at all when it
# finds no strings, so the file is created first to keep the callers simple.
extract()
{
	local pot=$1
	shift
	local keywords=$1
	shift
	: > "$pot"
	xgettext -o "$pot" $keywords --add-comments=i18n --from-code=utf-8 "$@"
}

BOTH_KEYWORDS="--keyword=translate:2 --keyword=translate:2,3c"

HEADER=$SCRATCH/headers/ui_UiStringFixture.h
python3 ../generate-ui-strings.py "$SCRATCH/headers" . > /dev/null || fail "generator exited non-zero"

if [ ! -f "$HEADER" ]; then
	echo "FAIL: no header generated from UiStringFixture.ui"
	exit 1
fi

grep -q '// i18n: A hexadecimal address, not a street address' "$HEADER" ||
	fail "extracomment was not injected into the generated header"

grep -q '// i18n: Button to open a file browser' "$HEADER" ||
	fail "non-ASCII string's extracomment was not injected into the generated header"

grep -q '// i18n: A label spanning two lines' "$HEADER" ||
	fail "multiline string's extracomment was not injected into the generated header"

# C3: a string holding a literal backslash and n. uic emits it on one line with the
# backslash doubled, so a search key cut at the first "\n" in the *escaped* literal
# truncated to something uic never wrote and hard-failed the whole generator.
grep -q '// i18n: The two characters backslash and n, not a line break' "$HEADER" ||
	fail "literal-escape string's extracomment was not injected into the generated header"

# C6: a carriage return written as the numeric reference &#13;. XML line-ending
# normalisation turns a raw CR byte into LF, and ElementTree agrees, so a raw CR needs no
# handling -- but a numeric reference escapes that normalisation and survives as a real CR.
# uic drops it from the literal, which is why the two words run together below, so
# cpp_literal() has to drop it too or nothing matches and the whole run exits 1.
grep -q '// i18n: A carriage return written as a numeric reference' "$HEADER" ||
	fail "numeric-CR string's extracomment was not injected into the generated header"

# C5: two *different* multi-line strings that happen to share a first line. Keying on the
# first segment alone made them collide, and the collision error told the author to add a
# disambiguation comment= -- which becomes the msgctxt, cannot change the text, and so
# cannot change a first-segment key. Matching the whole literal is what separates them.
# -A2 rather than a bare grep: the point is not that both comments exist but that each one
# sits above the call for its own string, whose second segment is two lines below it. That
# shape also catches a comment wrongly inserted *between* the two segments, which is what
# happens if annotate() stops requiring a match's line to be a translate() call -- see the
# substring label below for how such a match arises.
grep -A2 '// i18n: The GameCube variant of a two-line label' "$HEADER" |
	grep -q '^"for GameCube"' ||
	fail "GameCube label's extracomment is not above its own translate() call"
grep -A2 '// i18n: The Wii variant of a two-line label' "$HEADER" |
	grep -q '^"for Wii"' ||
	fail "Wii label's extracomment is not above its own translate() call"

# This label's whole literal is also the GameCube label's continuation line, so matching
# whole literals finds it in two places: its own translate() call and, spuriously, inside
# the longer literal. annotate() rejects the second because that line is not a translate()
# call. Measured: with that test removed the short label still gets this comment, so what
# fails is the GameCube assertion above, on the comment wrongly inserted between the two
# segments. The assertion here pins the short label's own attachment; the fixture exists to
# create the overlap that gives the two assertions above something to catch.
grep -A1 "// i18n: A short label whose whole text is another label's second line" "$HEADER" |
	grep -q 'translate(.*"for GameCube"' ||
	fail "substring label's extracomment is not above its own translate() call"

# C7: a string whose text is the form's <class> name. uic writes that name as translate()'s
# *first* argument on every call it emits, so this string's literal occurs once per
# translate() line in the file on top of its own msgid. annotate() must reject a match that
# sits at translate()'s first argument; without that, this string -- declared first, so it
# is matched first -- claims every line, and because notes_by_line is first-wins every
# other string's own comment is then silently dropped. The exit status stays 0, because
# every literal did match something, so only these assertions can catch it.
grep -A1 '// i18n: A label whose text equals the form class name' "$HEADER" |
	grep -q 'translate("UiStringFixture", "UiStringFixture"' ||
	fail "class-name label's extracomment is not above its own translate() call"
# The assertion that matters: an unrelated string must still carry its *own* comment. The
# positive check above passes with or without the fix, because the class-name string's own
# translate() line is one of the lines it wrongly claims.
grep -B1 'translate("UiStringFixture", "Address:"' "$HEADER" |
	grep -q '// i18n: A hexadecimal address, not a street address' ||
	fail "address label's own comment was displaced by the class-name label's"
# And the class-name comment must appear exactly once. The lines it wrongly claims include
# the calls for strings that have no comment of their own, which the assertion above cannot
# see: those strings would reach translators carrying a note about a different string.
class_name_notes=$(grep -c '// i18n: A label whose text equals the form class name' "$HEADER")
[ "$class_name_notes" -eq 1 ] ||
	fail "class-name label's extracomment appears $class_name_notes times, expected 1"

extract "$SCRATCH/both.pot" "$BOTH_KEYWORDS" "$HEADER"

grep -q '^msgid "Enable Progressive Scan"$' "$SCRATCH/both.pot" ||
	fail "plain string missing from the .pot"
grep -q '^msgid "Address:"$' "$SCRATCH/both.pot" ||
	fail "annotated string missing from the .pot"
grep -q '^msgid "Browse…"$' "$SCRATCH/both.pot" ||
	fail "non-ASCII string missing from the .pot"
grep -B1 '^msgid "Limit"$' "$SCRATCH/both.pot" | grep -q '^msgctxt "Refers to the speed limiter"$' ||
	fail "disambiguation did not become a msgctxt"
grep -q '^#\. i18n: A hexadecimal address, not a street address$' "$SCRATCH/both.pot" ||
	fail "translator comment missing from the .pot"
grep -q '^#\. i18n: Button to open a file browser$' "$SCRATCH/both.pot" ||
	fail "non-ASCII string's translator comment missing from the .pot"

grep -q '^#\. i18n: A label spanning two lines$' "$SCRATCH/both.pot" ||
	fail "multiline string's translator comment missing from the .pot"

# xgettext re-escapes the backslash, so the .pot holds two of them. Measured, not guessed.
grep -q '^msgid "Use \\\\n for a line break"$' "$SCRATCH/both.pot" ||
	fail "literal-escape string missing from the .pot"
grep -q '^#\. i18n: The two characters backslash and n, not a line break$' "$SCRATCH/both.pot" ||
	fail "literal-escape string's translator comment missing from the .pot"

grep -q '^msgid "Carriagereturn"$' "$SCRATCH/both.pot" ||
	fail "numeric-CR string missing from the .pot"
grep -q '^#\. i18n: A carriage return written as a numeric reference$' "$SCRATCH/both.pot" ||
	fail "numeric-CR string's translator comment missing from the .pot"

# Both halves of C5 must survive into the .pot as two separate entries.
grep -q '^#\. i18n: The GameCube variant of a two-line label$' "$SCRATCH/both.pot" ||
	fail "GameCube label's translator comment missing from the .pot"
grep -q '^#\. i18n: The Wii variant of a two-line label$' "$SCRATCH/both.pot" ||
	fail "Wii label's translator comment missing from the .pot"
grep -q '^"for GameCube"$' "$SCRATCH/both.pot" ||
	fail "GameCube label missing from the .pot"
grep -q '^"for Wii"$' "$SCRATCH/both.pot" ||
	fail "Wii label missing from the .pot"

# C1: notr="true" string with extracomment must not appear in the .pot
grep -q '^msgid "0x80000000"$' "$SCRATCH/both.pot" &&
	fail "notr string appeared in the .pot"

# C4: a whitespace-only string with an extracomment. uic emits setText(QString()) for it
# and no translate() call, so there is nothing to annotate and nothing to extract. The
# other half of this check is the generator's exit status asserted above: before the fix
# the string got a literal of its own, matched nothing, and took the whole run down with it.
grep -q '^msgid "   "$' "$SCRATCH/both.pot" &&
	fail "whitespace-only string appeared in the .pot"

# Count is 11: class-name, plain, disambiguated, annotated, nonascii, escape, numeric-CR,
# multiline, the two shared-first-line labels and the substring label. Not counted: the notr
# string and the whitespace-only string, for neither of which uic emits a translate() call
# at all. Note: xgettext represents multiline strings as msgid "" followed by quoted lines,
# so we count all msgid lines minus 1 for the header's empty msgid.
count=$(grep -c '^msgid' "$SCRATCH/both.pot")
count=$((count - 1))
[ "$count" -eq 11 ] || fail "expected 11 strings, got $count (duplicates or over-extraction)"

# The reason both keyword forms are listed: translate:2,3c alone drops every
# string whose disambiguation uic emitted as nullptr, which is nearly all of
# them. Removing either form must break this test rather than silently lose
# strings from 29 locales.
extract "$SCRATCH/ctx-only.pot" "--keyword=translate:2,3c" "$HEADER"
ctx_only=$(grep -c '^msgid' "$SCRATCH/ctx-only.pot")
ctx_only=$((ctx_only - 1))
[ "$ctx_only" -eq 1 ] ||
	fail "expected translate:2,3c alone to extract 1 of 11 strings, got $ctx_only"

# QPainter::translate is also called `translate`, so the keywords must not turn
# painter transforms into msgids.
extract "$SCRATCH/painter.pot" "$BOTH_KEYWORDS" PainterFixture.cpp
# When xgettext finds nothing, it writes an empty file with 0 msgid lines.
# When it finds strings, there's 1+ msgid lines (header + strings).
painter_msgids=$(grep -c '^msgid' "$SCRATCH/painter.pot")
if [ "$painter_msgids" -gt 0 ]; then
	painter=$((painter_msgids - 1))
else
	painter=0
fi
[ "$painter" -eq 0 ] || fail "non-string translate() calls were extracted ($painter)"

# F1: the reference-rewrite sed expression must handle multi-reference lines.
# Use the shared sed file that update-source-strings.sh uses.
rewritten=$(printf '#: Languages/.ui-scratch/Source/A/ui_X.h:5 Languages/.ui-scratch/Source/B/ui_Y.h:9\n' |
  sed -f ../ui-reference-rewrite.sed)
expected='#: Source/A/X.ui Source/B/Y.ui'
[ "$rewritten" = "$expected" ] ||
	fail "reference-rewrite expression produced '$rewritten', expected '$expected'"
grep -q 'ui-scratch' <<< "$rewritten" && fail ".ui-scratch leaked through the rewrite"

# M1: collision test - two strings with same text but different extracomments must fail
mkdir -p "$SCRATCH/collision-source"
cat > "$SCRATCH/collision-source/CollisionFixture.ui" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>CollisionFixture</class>
 <widget class="QWidget" name="CollisionFixture">
  <layout class="QVBoxLayout" name="layout">
   <item>
    <widget class="QLabel" name="a">
     <property name="text">
      <string extracomment="The first meaning">Limit</string>
     </property>
    </widget>
   </item>
   <item>
    <widget class="QLabel" name="b">
     <property name="text">
      <string extracomment="The second meaning">Limit</string>
     </property>
    </widget>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
EOF

if python3 ../generate-ui-strings.py "$SCRATCH/collision-out" "$SCRATCH/collision-source" > /dev/null 2>&1; then
	fail "collision fixture should have failed but exited 0"
else
	collision_output=$(python3 ../generate-ui-strings.py "$SCRATCH/collision-out" "$SCRATCH/collision-source" 2>&1)
	echo "$collision_output" | grep -q 'CollisionFixture.ui' ||
		fail "collision error did not name the .ui file"
	# repr(), so the text stays on one line of output when it contains a newline. That
	# renders a plain string in single quotes rather than double.
	echo "$collision_output" | grep -q "'Limit'" ||
		fail "collision error did not name the string text"
	echo "$collision_output" | grep -q 'different extracomments' ||
		fail "collision error did not describe the problem"
fi

# M2: the generator must keep its headers inside the output directory, and must keep
# putting them where ui-reference-rewrite.sed expects to find them.
mkdir -p "$SCRATCH/abs-source"
cat > "$SCRATCH/abs-source/AbsFixture.ui" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>AbsFixture</class>
 <widget class="QWidget" name="AbsFixture">
  <layout class="QVBoxLayout" name="layout">
   <item>
    <widget class="QLabel" name="annotated">
     <property name="text">
      <string extracomment="A note so annotate() has work to do">Frames</string>
     </property>
    </widget>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
EOF

# os.path.join discards everything before an absolute component, so joining the output
# root with an absolute source path yields the source path: the generated headers landed
# next to the .ui file in the source tree, untracked and ahead of the build tree's copy
# on a quoted #include.
python3 "$GENERATOR" "$SCRATCH/abs-out" "$SCRATCH/abs-source" > /dev/null ||
	fail "generator exited non-zero for an absolute source root"
strays=$(find "$SCRATCH/abs-source" -name 'ui_*.h' | wc -l | tr -d ' ')
[ "$strays" -eq 0 ] ||
	fail "absolute source root wrote $strays ui_*.h into the source tree"

# ui-reference-rewrite.sed rewrites Languages/.ui-scratch/<path>/ui_<name>.h into
# <path>/<name>.ui, so a form under Source/Core/DolphinQt must produce a header under
# <out>/Source/Core/DolphinQt. This assertion exists so that a future simplification of
# the path arithmetic cannot silently strip the Source/ prefix off every #: reference in
# dolphin-emu.pot; it pins the layout rather than reporting a bug.
mkdir -p "$SCRATCH/layout/Source/Core/DolphinQt"
cp "$SCRATCH/abs-source/AbsFixture.ui" "$SCRATCH/layout/Source/Core/DolphinQt/LayoutFixture.ui"
( cd "$SCRATCH/layout" && python3 "$GENERATOR" out Source > /dev/null ) ||
	fail "generator exited non-zero for a relative source root"
[ -f "$SCRATCH/layout/out/Source/Core/DolphinQt/ui_LayoutFixture.h" ] ||
	fail "header did not land at <out>/Source/Core/DolphinQt/ui_LayoutFixture.h"

# F2: verify update-source-strings.sh has both required keyword forms
SCRIPT=../update-source-strings.sh
# The follow-set is what defeats the prefix trap: --keyword=translate:2 is a substring of
# --keyword=translate:2,3c, and "," is not in the follow-set, so the first pattern cannot
# be satisfied by a 2,3c line alone. It admits end-of-line as well as a continuation
# backslash, because a keyword list reordered so that a translate: keyword comes last is
# still correct and used to fail here. The line anchor is kept as well, so a commented-out
# keyword cannot satisfy either check on its own.
grep -qE -- '^[[:space:]]*--keyword=translate:2([[:space:]]|\\|$)' "$SCRIPT" ||
	fail "update-source-strings.sh missing --keyword=translate:2"
grep -qE -- '^[[:space:]]*--keyword=translate:2,3c([[:space:]]|\\|$)' "$SCRIPT" ||
	fail "update-source-strings.sh missing --keyword=translate:2,3c"
# Also verify the count is exactly 2 translate keywords
translate_kw_count=$(grep -c '^[[:space:]]*--keyword=translate:' "$SCRIPT")
[ "$translate_kw_count" -eq 2 ] ||
	fail "expected exactly 2 translate keywords in script, got $translate_kw_count"

if [ "$failures" -eq 0 ]; then
	echo "PASS: .ui strings reach the .pot with context and translator comments"
	exit 0
fi

echo "$failures check(s) failed"
exit 1
