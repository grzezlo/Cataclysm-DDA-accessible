#!/usr/bin/env bash
#
# Update the access fork's own translation catalogs in lang/access and,
# with --machine, the machine translation layer in lang/machine.
#
# Usage: lang/update_access_po.sh [--base <revision>] [--machine] [language...]
#
# The fork template contains every string extracted from this tree that the
# upstream base lacks.  The base is the merge base of HEAD and upstream-base
# (or origin/upstream-base in a clone) unless --base names another revision.  Its template is cached in lang/po
# by commit.  Without languages, every existing lang/access and lang/machine
# catalog is updated.  Upstream catalogs in lang/po are left unchanged.
#
# --machine also merges lang/machine/<language>.po with the upstream strings
# that neither lang/po nor lang/access translates (written to
# lang/po/gaps-<language>.pot), so an upstream translation retires its machine
# entry.  It prunes lang/machine/<language>-overrides.po and clears machine
# entries that fail validation.  Run it rarely, for example once per release.

set -eu

script="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"

if [ ! -d lang/po ]
then
    if [ -d ../lang/po ]
    then
        cd ..
    else
        echo "Error: Could not find lang/po subdirectory."
        exit 1
    fi
fi

all_args=("$@")
base_ref=""
machine=""
while [ $# -gt 0 ]
do
    case "$1" in
        --base)
            if [ $# -lt 2 ]
            then
                echo "Error: --base requires a revision."
                exit 1
            fi
            base_ref="$2"
            shift 2
            ;;
        --machine)
            machine=1
            shift
            ;;
        *)
            break
            ;;
    esac
done
if [ -z "$base_ref" ]
then
    for candidate in upstream-base origin/upstream-base
    do
        if git rev-parse --verify --quiet "$candidate^{commit}" >/dev/null
        then
            base_ref="$candidate"
            break
        fi
    done
    if [ -z "$base_ref" ]
    then
        echo "Error: No upstream-base branch found; fetch it or pass --base."
        exit 1
    fi
fi

# The extraction scripts need Python 3 with polib.  Use uv for a temporary
# environment when the installed Python lacks polib.
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
        exec uv run --no-project --with polib bash "$script" "${all_args[@]}"
    fi
    echo "Error: Python 3 with polib is required."
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# extract_json_strings.py is started through "env python3".
if ! command -v python3 >/dev/null 2>&1
then
    mkdir "$work/bin"
    printf '#!/bin/sh\nexec "%s" "$@"\n' "$(command -v "$python")" > "$work/bin/python3"
    chmod +x "$work/bin/python3"
    PATH="$work/bin:$PATH"
fi

if [ $# -eq 0 ]
then
    for f in lang/access/*.po lang/machine/*.po
    do
        n="$(basename "$f" .po)"
        case "$n" in
            *-overrides) n="${n%-overrides}" ;;
        esac
        if [ -f "$f" ] && [[ " $* " != *" $n "* ]]
        then
            set -- "$@" "$n"
        fi
    done
    if [ $# -eq 0 ]
    then
        echo "Error: No catalogs in lang/access or lang/machine; name the languages to create."
        exit 1
    fi
fi
for n in "$@"
do
    if [ ! -f "lang/po/${n}.po" ]
    then
        echo "Error: lang/po/${n}.po does not exist."
        exit 1
    fi
done

base=$(git merge-base HEAD "$base_ref")
base_pot="lang/po/upstream-${base:0:12}.pot"

if [ ! -f "$base_pot" ]
then
    echo "> Extracting the upstream base template at ${base:0:12}"
    # Use this tree's extraction scripts on both sides so the difference
    # reflects the sources only.
    mkdir -p "$work/base/lang/po"
    git archive "$base" Makefile src data | tar -x -C "$work/base"
    cp lang/*.sh lang/*.py "$work/base/lang/"
    cp -r lang/string_extractor "$work/base/lang/"
    (cd "$work/base" && lang/update_pot.sh)
    cp "$work/base/lang/po/cataclysm-dda.pot" "$base_pot"
fi

echo "> Extracting the access template"
lang/update_pot.sh

access_pot="lang/po/cataclysm-dda-access.pot"
"$python" lang/access_catalog.py diff lang/po/cataclysm-dda.pot "$base_pot" "$access_pot"

failed=""
mkdir -p lang/access
for n in "$@"
do
    po="lang/access/${n}.po"
    if [ ! -f "$po" ]
    then
        echo "> Creating $po"
        "$python" lang/access_catalog.py init "$n" "lang/po/${n}.po" "$po"
    fi
    echo "> Updating $po"
    # Fuzzy matches are suggestions for the translator; compile_access_mo.sh
    # leaves them out of the game.
    msgmerge --quiet --previous "$po" "$access_pot" | msgattrib --no-obsolete -o "$po"
    msgfmt -c -o "$work/check.mo" "$po"
    "$python" lang/access_catalog.py validate "$po" || failed=1
done

if [ -n "$machine" ]
then
    mkdir -p lang/machine
    for n in "$@"
    do
        gaps_pot="lang/po/gaps-${n}.pot"
        machine_po="lang/machine/${n}.po"
        overrides_po="lang/machine/${n}-overrides.po"
        "$python" lang/access_catalog.py gaps lang/po/cataclysm-dda.pot "$access_pot"             "lang/po/${n}.po" "$gaps_pot"
        if [ ! -f "$machine_po" ]
        then
            echo "> Creating $machine_po"
            "$python" lang/access_catalog.py init --layer machine "$n" "lang/po/${n}.po" "$machine_po"
        fi
        echo "> Updating $machine_po"
        msgmerge --quiet --previous "$machine_po" "$gaps_pot" | msgattrib --no-obsolete -o "$machine_po"
        "$python" lang/access_catalog.py validate --clear "$machine_po"
        msgfmt -c -o "$work/check.mo" "$machine_po"
        if [ -f "$overrides_po" ]
        then
            echo "> Updating $overrides_po"
            "$python" lang/access_catalog.py overrides "$overrides_po" lang/po/cataclysm-dda.pot                 "$access_pot" "lang/po/${n}.po"
            msgfmt -c -o "$work/check.mo" "$overrides_po"
            "$python" lang/access_catalog.py validate "$overrides_po" || failed=1
        fi
    done
fi

for n in "$@"
do
    for po in "lang/access/${n}.po" "lang/machine/${n}.po" "lang/machine/${n}-overrides.po"
    do
        if [ -f "$po" ]
        then
            "$python" lang/access_catalog.py status "$po"
        fi
    done
done

if [ -n "$failed" ]
then
    echo "Error: Fix the translations reported above."
    exit 1
fi

echo "ALL DONE!"
