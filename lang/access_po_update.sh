#!/usr/bin/env bash
# Shared catalog replacement used by the refresh script and its failure fixtures.

# merge_access_po <catalog> <template> <python> <access|machine>
merge_access_po()
(
    set -euo pipefail
    po="$1"
    template="$2"
    python="$3"
    layer="$4"
    candidate=$(mktemp "${po}.XXXXXX") || exit 1
    check_mo="${candidate}.mo"
    trap 'rm -f -- "$candidate" "$check_mo"' EXIT

    # Explicit failure checks also work when the caller tests this function's
    # status, which disables Bash's implicit errexit inside functions.
    if ! msgmerge --quiet --previous "$po" "$template" |
        msgattrib --force-po --no-obsolete -o "$candidate"
    then
        exit 1
    fi
    if [ "$layer" = machine ]
    then
        "$python" "${access_po_tools}/access_catalog.py" validate --clear "$candidate" || exit 1
    fi
    msgfmt -c -o "$check_mo" "$candidate" || exit 1
    if [ "$layer" != machine ]
    then
        "$python" "${access_po_tools}/access_catalog.py" validate "$candidate" || exit 1
    fi
    mv -f -- "$candidate" "$po" || exit 1
)

# update_access_overrides <catalog> <full template> <fork template> <upstream> <python>
update_access_overrides()
(
    set -euo pipefail
    po="$1"
    python="$5"
    candidate=$(mktemp "${po}.XXXXXX") || exit 1
    check_mo="${candidate}.mo"
    trap 'rm -f -- "$candidate" "$check_mo"' EXIT
    cp -- "$po" "$candidate" || exit 1
    "$python" "${access_po_tools}/access_catalog.py" overrides "$candidate" "$2" "$3" "$4" || exit 1
    msgfmt -c -o "$check_mo" "$candidate" || exit 1
    "$python" "${access_po_tools}/access_catalog.py" validate "$candidate" || exit 1
    mv -f -- "$candidate" "$po" || exit 1
)
