#!/usr/bin/env bash
#
# Check the translation layers with fixture catalogs: compilation
# precedence, --no-machine, the machine gap step, override pruning, and
# validation.
#
# Usage: lang/test_access_po.sh

set -euo pipefail

script="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
lang_dir="$(dirname "$script")"
access_po_tools="$lang_dir"
source "$access_po_tools/access_po_update.sh"

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
merge_access_po lang/machine/xx.po lang/po/gaps.pot "$python" machine
expect_entries "upstream translations retire machine entries" lang/machine/xx.po "gap new"

{
    header
    entry override "upstream override"
    entry gone "override gone"
    entry fuzzy "override fuzzy"
} > lang/machine/xx-overrides.po
update_access_overrides lang/machine/xx-overrides.po lang/po/template.pot \
    lang/po/fork.pot lang/po/xx.po "$python" > /dev/null
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

# Batches follow player-visible order, never mix source groups, and merge
# back only valid, non-fuzzy translations of unchanged source strings.
{
    header
    printf '#: data/mods/Mod/a.json\nmsgid "mod"\nmsgstr ""\n\n'
    printf '#: data/json/npcs/a.json\nmsgid "talk"\nmsgstr ""\n\n'
    printf '#: data/json/items/a.json\nmsgid "item one"\nmsgstr ""\n\n'
    printf '#: data/json/items/b.json\nmsgid "item two"\nmsgstr ""\n\n'
    printf '#: data/json/items/c.json\nmsgid "item three"\nmsgstr ""\n\n'
    printf '#: data/json/items/d.json\nmsgid "done"\nmsgstr "gotowe"\n\n'
    printf '#: data/json/items/e.json\n#, fuzzy\n#| msgid "olde"\nmsgid "old"\nmsgstr "stare"\n\n'
    printf '#: src/a.cpp\nmsgid "%%d shots"\nmsgid_plural "%%d shots"\nmsgstr[0] ""\nmsgstr[1] ""\n\n'
    printf '#: src/b.cpp\nmsgid "gone"\nmsgstr ""\n\n'
} > lang/machine/batch.po
"$python" "$catalog" split --size 2 lang/machine/batch.po batches > /dev/null
expect_entries "interface strings come first" batches/0001.po "%d shots gone"
expect_entries "batches respect the size" batches/0002.po "item one item two"
expect_entries "fuzzy entries are retranslated" batches/0003.po "item three old"
expect_entries "core data precedes dialogue" batches/0004.po "talk"
expect_entries "mods come last" batches/0005.po "mod"
if "$python" "$catalog" split lang/machine/batch.po batches > /dev/null
then
    echo "FAILED: split overwrote existing batches"
    failures=$((failures + 1))
else
    echo "ok: split refuses to overwrite batches"
fi
if "$python" "$catalog" show batches/0001.po 2 | grep -q '^msgid: "gone"$'
then
    echo "ok: show prints numbered entries"
else
    echo "FAILED: show prints numbered entries"
    failures=$((failures + 1))
fi
printf '{"1": ["%%d strzał"], "3": "x"}' > bad.json
before=$(sha256sum batches/0001.po)
if "$python" "$catalog" fill batches/0001.po bad.json > /dev/null ||
    [ "$(sha256sum batches/0001.po)" != "$before" ]
then
    echo "FAILED: fill accepts wrong plural forms or entry numbers"
    failures=$((failures + 1))
else
    echo "ok: fill rejects wrong plural forms and entry numbers"
fi
printf '{"1": ["%%d strzał", "%%d strzały"], "2": "usunięte"}' > first.json
"$python" "$catalog" fill batches/0001.po first.json > /dev/null
printf '{"1": "<color_red>pierwszy", "2": "druga"}' > second.json
"$python" "$catalog" fill batches/0002.po second.json > /dev/null
printf '{"2": {"translation": "drugi", "reason": "gender"}}' > review.json
"$python" "$catalog" fill --log review.md batches/0002.po review.json > /dev/null
if grep -q 'reason: gender' review.md && ! grep -q pierwszy review.md
then
    echo "ok: fill logs changed translations only"
else
    echo "FAILED: fill logs changed translations only"
    failures=$((failures + 1))
fi
cp batches/0003.po fuzzy.po
printf '{"2": "dawne"}' > fuzzy.json
"$python" "$catalog" fill fuzzy.po fuzzy.json > /dev/null
if "$python" "$catalog" status fuzzy.po | grep -q ' 0 fuzzy'
then
    echo "ok: fill clears the fuzzy flag of a retranslated entry"
else
    echo "FAILED: fill clears the fuzzy flag of a retranslated entry"
    failures=$((failures + 1))
fi
if "$python" "$catalog" check batches/0001.po > /dev/null
then
    echo "ok: check accepts a complete batch"
else
    echo "FAILED: check accepts a complete batch"
    failures=$((failures + 1))
fi
if "$python" "$catalog" check fuzzy.po > /dev/null
then
    echo "FAILED: check accepts an untranslated entry"
    failures=$((failures + 1))
else
    echo "ok: check rejects an untranslated entry"
fi
if "$python" "$catalog" lookup 'item two' 'batches/*.po' | grep -q "drugi"
then
    echo "ok: lookup expands catalog patterns itself"
else
    echo "FAILED: lookup expands catalog patterns itself"
    failures=$((failures + 1))
fi
"$python" - <<'EOF'
import polib
third = polib.pofile("batches/0003.po")
third[0].msgstr = "trzeci"
third[0].flags.append("fuzzy")
third[1].msgstr = "dawne"
third[1].flags.remove("fuzzy")
third.save()
EOF
# "gone" leaves the catalog after the split, as when its source changes.
"$python" - <<'EOF'
import polib
catalog = polib.pofile("lang/machine/batch.po")
catalog[:] = [entry for entry in catalog if entry.msgid != "gone"]
catalog.save("lang/machine/merged.po")
EOF
if "$python" "$catalog" merge lang/machine/merged.po batches | grep -q 'source string changed'
then
    echo "ok: merge reports translations of changed source strings"
else
    echo "FAILED: merge reports translations of changed source strings"
    failures=$((failures + 1))
fi
msgfmt -o merged.mo lang/machine/merged.po
expect "a valid batch translation is merged" merged.mo "item two" "drugi"
expect "a retranslated fuzzy entry is merged" merged.mo "old" "dawne"
expect "an invalid batch translation is not merged" merged.mo "item one" ""
expect "a fuzzy batch translation is not merged" merged.mo "item three" ""
expect "earlier translations are kept" merged.mo "done" "gotowe"
plural=$("$python" -c 'import polib, sys
entry = polib.pofile(sys.argv[1]).find("%d shots")
print(entry.msgstr_plural[1])' lang/machine/merged.po)
if [ "${plural%$'\r'}" = "%d strzały" ]
then
    echo "ok: plural batch translations are merged"
else
    echo "FAILED: plural batch translations are merged: got '$plural'"
    failures=$((failures + 1))
fi
if "$python" "$catalog" lookup -i 'ITEM T' lang/machine/merged.po | grep -q "drugi"
then
    echo "ok: lookup finds translations by source"
else
    echo "FAILED: lookup finds translations by source"
    failures=$((failures + 1))
fi
if "$python" "$catalog" lookup --translation 'pierwszy' lang/machine/merged.po | grep -q .
then
    echo "FAILED: lookup shows untranslated entries"
    failures=$((failures + 1))
else
    echo "ok: lookup skips untranslated entries"
fi

# An original that mixes positional and plain arguments cannot be translated
# validly: split leaves it out, status does not count it, validate only
# warns, and merge and --clear drop its translation.
{
    header
    printf '#: src/a.cpp\n#, c-format\nmsgid "%%1s and %%2$s"\nmsgstr ""\n\n'
    printf '#: src/a.cpp\nmsgid "fine"\nmsgstr ""\n\n'
} > lang/machine/broken.po
"$python" "$catalog" split lang/machine/broken.po broken > /dev/null
expect_entries "split leaves out invalid originals" broken/0001.po "fine"
cp lang/machine/broken.po broken/0002.po
printf '{"1": "%%1s i %%2$s", "2": "dobrze"}' > broken.json
"$python" "$catalog" fill broken/0002.po broken.json > /dev/null
if "$python" "$catalog" validate broken/0002.po | grep -q 'warning: Invalid original'
then
    echo "ok: validate only warns about invalid originals"
else
    echo "FAILED: validate only warns about invalid originals"
    failures=$((failures + 1))
fi
cp lang/machine/broken.po broken/untranslated.po
if "$python" "$catalog" status broken/untranslated.po | grep -q ' 1 untranslated'
then
    echo "ok: status does not count invalid originals"
else
    echo "FAILED: status does not count invalid originals"
    failures=$((failures + 1))
fi
rm broken/untranslated.po
"$python" "$catalog" merge lang/machine/broken.po broken > /dev/null
msgfmt -o broken.mo lang/machine/broken.po
expect "merge drops translations of invalid originals" broken.mo "%1s and %2\$s" ""
expect "merge keeps other translations" broken.mo "fine" "dobrze"

# Negative cases use the production merge function, never tracked catalogs.
for layer in access machine
do
    for failure in template producer filter msgfmt validator
    do
        po="lang/${layer}/failure.po"
        cp lang/po/xx.po "$po"
        before=$(sha256sum "$po")
        printf 'msgid "unterminated\n' > broken.pot
        if (
            template=lang/po/template.pot
            validator="$python"
            case "$failure" in
                template) template=broken.pot ;;
                producer) msgmerge() { return 42; } ;;
                filter) msgattrib() { printf 'partial output'; return 43; } ;;
                msgfmt) msgfmt() { return 44; } ;;
                validator)
                    failing_validator() { return 45; }
                    validator=failing_validator
                    ;;
            esac
            merge_access_po "$po" "$template" "$validator" "$layer" || exit 1
            echo 'ALL DONE!'
        ) > failure.log 2>&1
        then
            echo "FAILED: $layer $failure failure reported success"
            failures=$((failures + 1))
        elif [ "$(sha256sum "$po")" != "$before" ] || grep -q 'ALL DONE!' failure.log
        then
            echo "FAILED: $layer $failure failure changed the catalog or printed success"
            failures=$((failures + 1))
        else
            echo "ok: $layer $failure failure preserves the catalog and exits nonzero"
        fi
    done
done

for failure in prune msgfmt validator
do
    po=lang/machine/failure-overrides.po
    cp lang/machine/xx-overrides.po "$po"
    before=$(sha256sum "$po")
    if (
        validator="$python"
        case "$failure" in
            prune)
                failing_validator() { return 45; }
                validator=failing_validator
                ;;
            msgfmt) msgfmt() { return 44; } ;;
            validator)
                real_python="$python"
                failing_validator() {
                    if [ "$2" = validate ]; then return 45; fi
                    "$real_python" "$@"
                }
                validator=failing_validator
                ;;
        esac
        update_access_overrides "$po" lang/po/template.pot lang/po/fork.pot \
            lang/po/xx.po "$validator" || exit 1
        echo 'ALL DONE!'
    ) > failure.log 2>&1
    then
        echo "FAILED: override $failure failure reported success"
        failures=$((failures + 1))
    elif [ "$(sha256sum "$po")" != "$before" ] || grep -q 'ALL DONE!' failure.log
    then
        echo "FAILED: override $failure failure changed the catalog or printed success"
        failures=$((failures + 1))
    else
        echo "ok: override $failure failure preserves the catalog and exits nonzero"
    fi
done

# Run the real refresh orchestration with extraction and Git replaced by
# fixture setup. A failing producer must also suppress the script's footer.
mkdir -p refresh/lang/po refresh/lang/access refresh/lang/machine refresh/tools
cp "$catalog" refresh/lang/access_catalog.py
cp "$lang_dir/../tools/check_po_printf_format.py" refresh/tools/
cp lang/po/template.pot refresh/lang/po/cataclysm-dda.pot
cp lang/po/template.pot refresh/lang/po/upstream-0123456789ab.pot
cp lang/po/xx.po refresh/lang/po/xx.po
cp lang/access/xx.po refresh/lang/access/xx.po
cp lang/machine/xx.po refresh/lang/machine/xx.po
printf '#!/usr/bin/env bash\nexit 0\n' > refresh/lang/update_pot.sh
chmod +x refresh/lang/update_pot.sh
before_access=$(sha256sum refresh/lang/access/xx.po)
before_machine=$(sha256sum refresh/lang/machine/xx.po)
if (
    cd refresh
    git() { printf '0123456789abcdef0123456789abcdef01234567\n'; }
    msgmerge() { return 42; }
    export -f git msgmerge
    bash "$lang_dir/update_access_po.sh" --base fixture --machine xx
) > refresh.log 2>&1
then
    echo "FAILED: refresh script accepted a failed merge"
    failures=$((failures + 1))
elif [ "$(sha256sum refresh/lang/access/xx.po)" != "$before_access" ] ||
    [ "$(sha256sum refresh/lang/machine/xx.po)" != "$before_machine" ] ||
    grep -q 'ALL DONE!' refresh.log
then
    echo "FAILED: failed refresh changed catalogs or printed success"
    failures=$((failures + 1))
else
    echo "ok: failed refresh retains both catalogs and omits the success footer"
fi

if [ $failures -ne 0 ]
then
    echo "$failures translation layer checks failed."
    exit 1
fi
echo "All translation layer checks passed."
