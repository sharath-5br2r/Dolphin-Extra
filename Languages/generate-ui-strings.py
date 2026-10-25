#!/usr/bin/env python3

"""Runs uic over every .ui file under a source tree and restores the translator
comments uic discards, so that xgettext can see the strings that live in Qt
Designer forms.

Prints the path of each generated header, one per line, for the caller to append
to its xgettext file list."""

import bisect
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ElementTree


def find_uic():
    if os.environ.get("UIC"):
        return os.environ["UIC"]

    in_path = shutil.which("uic")
    if in_path:
        return in_path

    # Homebrew's Qt keeps uic in libexec, which is not on PATH.
    for qmake in ("qmake6", "qmake"):
        if not shutil.which(qmake):
            continue
        libexec = subprocess.run([qmake, "-query", "QT_HOST_LIBEXECS"],
                                 capture_output=True, text=True).stdout.strip()
        candidate = os.path.join(libexec, "uic")
        if libexec and os.path.exists(candidate):
            return candidate

    bundled = os.path.join("Externals", "Qt", "Qt6.8.3", "x64", "bin", "uic.exe")
    if os.path.exists(bundled):
        return bundled

    sys.exit("generate-ui-strings.py: uic not found. Set UIC to its path.")


def cpp_literal(text):
    """The literal uic would emit for this string, so it can be matched by text.

    uic escapes every non-ASCII byte as three-digit octal, so quoting the decoded text
    never matches a string containing an accent or an ellipsis. It also drops carriage
    returns, which reach here only from a numeric reference (&#13;): XML line-ending
    normalisation turns a raw CR into LF, and ElementTree does the same, but a numeric
    reference escapes that normalisation by design."""
    out = []
    for byte in text.encode("utf-8"):
        if byte >= 0x80:
            out.append("\\{:03o}".format(byte))
        elif byte in (0x22, 0x5C):  # " and backslash
            out.append("\\" + chr(byte))
        elif byte == 0x0A:
            out.append("\\n")
        elif byte == 0x0D:
            continue  # uic drops a carriage return from the literal entirely.
        else:
            out.append(chr(byte))
    return '"{}"'.format("".join(out))


def expected_literal(text):
    """Exactly the text uic emits for this string, splits included.

    uic breaks a string at every newline into adjacent literals, the continuation at
    column 0. Matching the whole thing rather than the first segment is what keeps two
    different strings that happen to share a first line from looking like one string.

    The split is made in the source text and only then escaped, never searched for in the
    escaped literal: there, a real newline and a source backslash followed by an n are
    both spelled with a backslash, so splitting a string that documents an escape
    sequence would cut it into pieces uic never emitted."""
    segments = text.split("\n")
    parts = [cpp_literal(segment + "\n") for segment in segments[:-1]]
    parts.append(cpp_literal(segments[-1]))
    return "\n".join(parts)


def translator_comments(ui_path):
    """Maps the literal uic will emit for each string to its extracomment, if it has one."""
    comments = {}
    seen = {}  # Maps literal to (text, note) for collision detection
    for element in ElementTree.parse(ui_path).iter("string"):
        # Skip non-translatable strings. A notr string with a comment is not an error;
        # the comment is a note to the next developer, not to a translator.
        if element.get("notr") == "true":
            continue
        note = element.get("extracomment")
        # Whitespace-only text is the other case uic answers with setText(QString())
        # instead of a translate() call, so it is skipped just as silently: a string with
        # nothing to translate carrying a note to the next developer is not an error.
        # element.text is None for Designer's <string/>; strip() is what catches the
        # strings that are not empty but hold only spaces, tabs or newlines.
        if note and element.text and element.text.strip():
            literal = expected_literal(element.text)
            if literal in comments:
                if comments[literal] != note:
                    # Two strings with the same text but different translator comments
                    # need a disambiguation comment= to give them distinct msgctxt values.
                    # Because the literal is matched whole, two strings reaching here
                    # really are the same string, so that remedy does apply.
                    # repr() keeps a text containing a newline on one line of output.
                    prev_text, prev_note = seen[literal]
                    sys.exit(
                        'generate-ui-strings.py: {}: two strings with text {} have '
                        'different extracomments ("{}" and "{}"). Add a disambiguation '
                        'comment= to one or both.'.format(
                            ui_path, repr(element.text), prev_note, note))
                # Same literal, same comment: harmless duplicate, stay silent.
            else:
                comments[literal] = note
                seen[literal] = (element.text, note)
    return comments


def annotate(header_path, comments):
    """Injects `// i18n:` above each translate() call whose string had one.

    uic emits no comment at all, so without this the 176 existing `// i18n:`
    notes would be lost the moment their strings move into a .ui file."""
    if not comments:
        return

    with open(header_path, encoding="utf-8") as header:
        lines = header.readlines()

    # A multi-line string's literal spans lines, so the search runs over the whole body
    # and each match's offset is mapped back to the line it began on -- the translate()
    # call line, which is where the comment belongs.
    body = "".join(lines)
    line_starts = []
    offset = 0
    for line in lines:
        line_starts.append(offset)
        offset += len(line)

    notes_by_line = {}
    injected = set()
    for literal, note in comments.items():
        # Every occurrence, not just the first: the same string with the same note is a
        # harmless duplicate that translator_comments() stays silent about, and both of
        # its call sites need annotating.
        start = body.find(literal)
        while start != -1:
            index = bisect.bisect_right(line_starts, start) - 1
            column = start - line_starts[index]
            prefix = lines[index][:column]
            # The form class is translate()'s first argument on every generated call. If
            # a message has the same text, matching the literal alone finds every context
            # argument as well as the one message argument. Require this occurrence to
            # begin exactly where the second argument begins.
            if re.search(r'\btranslate\(\s*"(?:\\.|[^"\\])*"\s*,\s*$', prefix):
                notes_by_line.setdefault(index, note)
                injected.add(literal)
            start = body.find(literal, start + 1)

    # Reaching here means one string's literal matched nothing, which happens two ways, and
    # either way one authored string takes down every translation update for everyone.
    #
    # 1. uic emitted no translate() call for it. It does not emit one for every <string>;
    #    the cases known so far are filtered out in translator_comments() above --
    #    notr="true", and text that is empty or whitespace-only.
    # 2. uic emitted one, but spelled the literal differently than expected_literal()
    #    rebuilt it. The spellings accounted for are in cpp_literal() -- octal for
    #    non-ASCII, escaped quotes and backslashes, \n, and a dropped carriage return --
    #    plus the split at every newline that expected_literal() reproduces.
    #
    # So a new failure here is a case to add to one of those two lists, not a reason to
    # soften the diagnostic: exiting is what stops strings silently leaving the .pot.
    missed = set(comments) - injected
    if missed:
        sys.exit("generate-ui-strings.py: {}: no translate() call matched {}. "
                 "uic's escaping and cpp_literal() have diverged.".format(
                     header_path, ", ".join(sorted(missed))))

    output = []
    for index, line in enumerate(lines):
        if index in notes_by_line:
            indent = line[:len(line) - len(line.lstrip())]
            output.append("{}// i18n: {}\n".format(indent, notes_by_line[index]))
        output.append(line)

    with open(header_path, "w", encoding="utf-8") as header:
        header.writelines(output)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit("usage: generate-ui-strings.py <output-dir> [source-dir]")

    output_root = sys.argv[1]
    source_root = sys.argv[2] if len(sys.argv) == 3 else "Source"
    # Always present, so the caller can list it even before any pane is migrated.
    os.makedirs(output_root, exist_ok=True)

    ui_files = []
    for directory, _, names in os.walk(source_root):
        ui_files += [os.path.join(directory, n) for n in names if n.endswith(".ui")]

    # No forms yet, so uic is not needed to run the script at all.
    if not ui_files:
        return

    uic = find_uic()
    # normpath first, or a trailing slash ("Source/") makes basename return "".
    source_name = os.path.basename(os.path.normpath(source_root))

    for ui_path in sorted(ui_files):
        # The output tree mirrors the source tree: two forms can share a file name. The
        # path is rebuilt from source_name plus a relative path rather than joined with
        # ui_path directly, because os.path.join discards everything before an absolute
        # component: an absolute source_root would otherwise write the headers next to
        # the .ui files in the source tree. ui-reference-rewrite.sed rewrites
        # <output>/<source_name>/<subdirs>/ui_<name>.h back into the .ui path, so this
        # shape is load-bearing and must not change for a relative source_root.
        header_dir = os.path.normpath(os.path.join(
            output_root, source_name,
            os.path.relpath(os.path.dirname(ui_path), source_root)))
        os.makedirs(header_dir, exist_ok=True)
        name = os.path.basename(ui_path)[: -len(".ui")]
        header_path = os.path.join(header_dir, "ui_{}.h".format(name))

        subprocess.run([uic, ui_path, "-o", header_path], check=True)
        annotate(header_path, translator_comments(ui_path))
        print(header_path)


if __name__ == "__main__":
    main()
