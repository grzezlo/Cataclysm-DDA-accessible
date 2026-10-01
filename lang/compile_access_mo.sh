#!/usr/bin/env bash
#
# Compile game catalogs from the upstream catalogs in lang/po and the access
# fork's catalogs in lang/access.
#
# Usage: lang/compile_access_mo.sh [language... | all]
#
# Translated, non-fuzzy fork entries take precedence over upstream entries.
# A language without a fork catalog is compiled from lang/po alone.

set -eu

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

LOCALE_DIR="${LOCALE_DIR:-lang/mo}"

if [ $# -eq 0 ] || [ "$1" = "all" ]
then
    set --
    for f in lang/po/*.po
    do
        set -- "$@" "$(basename "$f" .po)"
    done
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for n in "$@"
do
    po="lang/po/${n}.po"
    access_po="lang/access/${n}.po"
    mo_dir="$LOCALE_DIR/${n}/LC_MESSAGES"
    if [ ! -f "$po" ]
    then
        echo "Error: $po does not exist."
        exit 1
    fi
    mkdir -p "$mo_dir"
    if [ -f "$access_po" ]
    then
        echo "> Compiling $n with $access_po"
        msgfmt -c -o "$work/check.mo" "$access_po"
        # Untranslated fork entries must not hide an upstream translation.
        msgattrib --force-po --translated --no-fuzzy --no-obsolete -o "$work/access.po" "$access_po"
        msgcat --use-first -o "$work/merged.po" "$work/access.po" "$po"
        msgfmt -f -o "$mo_dir/cataclysm-dda.mo" "$work/merged.po"
    else
        echo "> Compiling $n"
        msgfmt -f -o "$mo_dir/cataclysm-dda.mo" "$po"
    fi
done
