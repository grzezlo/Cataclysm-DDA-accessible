#!/usr/bin/env bash
#
# Check the translation layers with fixture catalogs: compilation
# precedence, --no-machine, the machine gap step, override pruning, and
# validation.
#
# Usage: lang/test_access_po.sh

set -eu

script="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
lang_dir="$(dirname "$script")"

python=""
for candidate in python3 python
do
    if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c "import polib" >/dev/null 2>&1
    then
        python="$candidate"
        break
    fi
done
if [ -z "$python" ]
then
    if [ -z "${CATA_ACCESS_PO_UV:-}" ] && command -v uv >/dev/null 2>&1
    then
        export CATA_ACCESS_PO_UV=1
        exec uv run --no-project --with polib bash "$script" "$@"
    fi
    echo "Error: Python 3 with polib is required."
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
mkdir -p lang/po lang/access lang/machine
catalog="$lang_dir/access_catalog.py"
failures=0

header()
{
    printf 'msgid ""\nmsgstr ""\n'
    printf '"%s\\n"\n' "Project-Id-Version: test" "PO-Revision-Date: 2026-01-01 00:00+0000" \
        "Last-Translator: test" "Language-Team: xx" "Language: xx" "MIME-Version: 1.0" \
        "Content-Type: text/plain; charset=UTF-8" "Content-Transfer-Encoding: 8bit" \
        "Plural-Forms: nplurals=2; plural=(n != 1);"
    printf '\n'
}

# entry <msgid> <msgstr> [flag]
entry()
{
    if [ -n "${3:-}" ]
    then
        printf '#, %s\n' "$3"
    fi
    printf 'msgid "%s"\nmsgstr "%s"\n\n' "$1" "$2"
}

# expect <description> <mo> <msgid> <expected msgstr, empty for none>
expect()
{
    local actual
    actual=$("$python" - "$2" "$3" <<'EOF'
import sys
import polib
for entry in polib.mofile(sys.argv[1]):
    if entry.msgid == sys.argv[2]:
        print(entry.msgstr)
EOF
)
    actual="${actual%$'\r'}"
    if [ "$actual" = "$4" ]
    then
        echo "ok: $1"
    else
        echo "FAILED: $1: expected '$4', got '$actual'"
        failures=$((failures + 1))
    fi
}

# expect_entries <description> <po> <expected msgid list, sorted, spaces>
expect_entries()
{
    local actual
    actual=$("$python" - "$2" <<'EOF'
import sys
import polib
print(" ".join(sorted(entry.msgid for entry in polib.pofile(sys.argv[1])
                      if not entry.obsolete)))
EOF
)
    actual="${actual%$'\r'}"
    if [ "$actual" = "$3" ]
    then
        echo "ok: $1"
    else
        echo "FAILED: $1: expected '$3', got '$actual'"
        failures=$((failures + 1))
    fi
}

{
    header
    entry fork "upstream fork"
    entry override "upstream override"
    entry fuzzy "upstream fuzzy" fuzzy
    entry gap ""
    entry upstream "upstream"
    entry stale ""
} > lang/po/xx.po
{
    header
    entry fork "access fork"
    entry upstream ""
    entry draft "access draft"
    entry stale "access stale" fuzzy
} > lang/access/xx.po
{
    header
    entry fork "override fork"
    entry override "override"
    entry fuzzy "override stale" fuzzy
} > lang/machine/xx-overrides.po
{
    header
    entry override "machine override"
    entry fuzzy "machine fuzzy"
    entry gap "machine gap"
    entry stale "machine stale" fuzzy
} > lang/machine/xx.po

bash "$lang_dir/compile_access_mo.sh" xx > /dev/null
mo=lang/mo/xx/LC_MESSAGES/cataclysm-dda.mo
expect "fork catalog wins" "$mo" fork "access fork"
expect "override beats upstream" "$mo" override "override"
expect "upstream fuzzy beats machine" "$mo" fuzzy "upstream fuzzy"
expect "machine fills a gap" "$mo" gap "machine gap"
expect "untranslated fork entry keeps upstream" "$mo" upstream "upstream"
expect "fork-only entry is compiled" "$mo" draft "access draft"
expect "fuzzy fork and machine entries stay out" "$mo" stale ""

bash "$lang_dir/compile_access_mo.sh" --no-machine xx > /dev/null
expect "--no-machine ignores overrides" "$mo" override "upstream override"
expect "--no-machine ignores machine gaps" "$mo" gap ""
expect "--no-machine keeps the fork catalog" "$mo" fork "access fork"

# The gap step leaves out fork strings and upstream translations, so an
# upstream translation retires its machine entry.
{
    header
    entry fork ""
    entry override ""
    entry fuzzy ""
    entry gap ""
    entry upstream ""
    entry new ""
} > lang/po/template.pot
{
    header
    entry fork ""
} > lang/po/fork.pot
"$python" "$catalog" gaps lang/po/template.pot lang/po/fork.pot lang/po/xx.po lang/po/gaps.pot > /dev/null
expect_entries "gaps hold untranslated upstream strings" lang/po/gaps.pot "gap new"
msgmerge --quiet --previous lang/machine/xx.po lang/po/gaps.pot | msgattrib --no-obsolete -o lang/machine/xx.po
expect_entries "upstream translations retire machine entries" lang/machine/xx.po "gap new"

{
    header
    entry override "upstream override"
    entry gone "override gone"
    entry fuzzy "override fuzzy"
} > lang/machine/xx-overrides.po
"$python" "$catalog" overrides lang/machine/xx-overrides.po lang/po/template.pot lang/po/fork.pot lang/po/xx.po > /dev/null
expect_entries "identical and obsolete overrides are dropped" lang/machine/xx-overrides.po "fuzzy"

{
    header
    entry "<color_red>%s</color>" "<color_red>%s</color>" c-format
    entry "<npcname> waves" "<npcname> macha"
    entry "%d items" "%s przedmiotów" c-format
    entry "<color_red>Stop</color>" "<color_red>Stój"
    entry "<info>note</info>" "<info>notatka</infos>"
} > lang/machine/bad.po
if "$python" "$catalog" validate lang/machine/bad.po > /dev/null
then
    echo "FAILED: validation accepts broken translations"
    failures=$((failures + 1))
else
    echo "ok: validation rejects broken translations"
fi
"$python" "$catalog" validate --clear lang/machine/bad.po > /dev/null
msgfmt -o bad.mo lang/machine/bad.po
expect "valid translation is kept" bad.mo "<npcname> waves" "<npcname> macha"
expect "printf mismatch is cleared" bad.mo "%d items" ""
expect "missing closing tag is cleared" bad.mo "<color_red>Stop</color>" ""
expect "misspelled tag is cleared" bad.mo "<info>note</info>" ""

if [ $failures -ne 0 ]
then
    echo "$failures translation layer checks failed."
    exit 1
fi
echo "All translation layer checks passed."
