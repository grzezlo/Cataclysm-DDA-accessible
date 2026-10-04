#!/usr/bin/env bash
#
# Compile game catalogs from the upstream catalogs in lang/po, the access
# fork's catalogs in lang/access, and the machine translation layer in
# lang/machine.
#
# Usage: lang/compile_access_mo.sh [--no-machine] [language... | all]
#
# Translated, non-fuzzy entries are taken in this order, the first one
# winning: lang/access/<language>.po, lang/machine/<language>-overrides.po,
# lang/po/<language>.po (whose fuzzy entries stay in use as upstream intends),
# and lang/machine/<language>.po.  --no-machine skips both lang/machine
# catalogs.  A language without fork or machine catalogs is compiled from
# lang/po alone.

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

machine=1
if [ "${1:-}" = "--no-machine" ]
then
    machine=""
    shift
fi

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

# Write the translated, non-fuzzy entries of a fork or machine catalog, so an
# untranslated or stale entry cannot hide a lower layer.
translated_layer()
{
    msgfmt -c -o "$work/check.mo" "$1"
    msgattrib --force-po --translated --no-fuzzy --no-obsolete -o "$2" "$1"
}

for n in "$@"
do
    po="lang/po/${n}.po"
    mo_dir="$LOCALE_DIR/${n}/LC_MESSAGES"
    if [ ! -f "$po" ]
    then
        echo "Error: $po does not exist."
        exit 1
    fi
    mkdir -p "$mo_dir"

    layers=()
    names=()
    if [ -f "lang/access/${n}.po" ]
    then
        translated_layer "lang/access/${n}.po" "$work/access.po"
        layers+=("$work/access.po")
        names+=("lang/access/${n}.po")
    fi
    if [ -n "$machine" ] && [ -f "lang/machine/${n}-overrides.po" ]
    then
        translated_layer "lang/machine/${n}-overrides.po" "$work/overrides.po"
        layers+=("$work/overrides.po")
        names+=("lang/machine/${n}-overrides.po")
    fi
    upstream_index=${#layers[@]}
    layers+=("$po")
    if [ -n "$machine" ] && [ -f "lang/machine/${n}.po" ]
    then
        translated_layer "lang/machine/${n}.po" "$work/machine.po"
        layers+=("$work/machine.po")
        names+=("lang/machine/${n}.po")
    fi

    if [ ${#names[@]} -eq 0 ]
    then
        echo "> Compiling $n"
        msgfmt -f -o "$mo_dir/cataclysm-dda.mo" "$po"
        continue
    fi
    echo "> Compiling $n with ${names[*]}"
    # Untranslated upstream entries must not hide a machine translation.
    # msgcat prefers a later non-fuzzy translation to an earlier fuzzy one,
    # so clear the flag of upstream fuzzy entries, which msgfmt -f uses anyway.
    msgattrib --force-po --translated --no-obsolete --clear-fuzzy -o "$work/upstream.po" "$po"
    layers[$upstream_index]="$work/upstream.po"
    msgcat --use-first -o "$work/merged.po" "${layers[@]}"
    msgfmt -f -o "$mo_dir/cataclysm-dda.mo" "$work/merged.po"
done
