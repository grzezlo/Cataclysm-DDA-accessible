#!/usr/bin/env python3
"""Helpers for the access fork's translation layers.

lang/access/<language>.po translates strings added by the fork.
lang/machine/<language>.po translates upstream strings that neither the
upstream catalog nor the fork catalog translates.
lang/machine/<language>-overrides.po corrects wrong upstream translations.

diff:      write the entries of the fork template that the upstream base
           template does not contain.
init:      create an empty catalog for a language, using the plural forms of
           that language's upstream catalog.
gaps:      write the entries of the full template that are neither fork
           strings nor translated by the upstream catalog.
overrides: drop overrides that upstream now translates identically or whose
           source string is gone, and report overrides of fork strings.
validate:  check printf arguments and tags of translated entries; --clear
           empties the entries that fail.
status:    report untranslated entries and AI drafts awaiting review.
"""

import argparse
import os
import re
import sys
from datetime import datetime

import polib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
from check_po_printf_format import check_message  # noqa: E402

LINE_NUMBER = re.compile(r":\d+$")
# Color tags, <info>, <npcname>, <u_val:name>, and similar markup that the
# game substitutes or interprets.  Single letters such as <E>nable mark
# hotkeys, which translators may move.
TAG = re.compile(r"</?[A-Za-z_][A-Za-z0-9_]+(?:[:=][^<>\s]*)?>")
AI_DRAFT = "AI draft"

HEADERS = {
    "access": "Translations of the access fork's own strings for {0}.\n"
              "Upstream strings are translated in lang/po.",
    "machine": "Machine translations of upstream strings for {0} that "
               "lang/po leaves untranslated.\n"
               "Never submit these entries to Transifex.",
    "overrides": "Corrections of wrong upstream translations for {0}.\n"
                 "Never submit these entries to Transifex.",
}


def entry_key(entry):
    return (entry.msgctxt, entry.msgid)


def has_translation(entry):
    # Upstream fuzzy entries are compiled with msgfmt --use-fuzzy, so they
    # count as translated here.
    if entry.obsolete:
        return False
    if entry.msgid_plural:
        return bool(entry.msgstr_plural) and all(entry.msgstr_plural.values())
    return bool(entry.msgstr)


def same_translation(a, b):
    if a.msgid_plural:
        return a.msgstr_plural == b.msgstr_plural
    return a.msgstr == b.msgstr


def normalized_occurrences(entry):
    # Line numbers change with every edit and Windows extraction writes
    # backslashes; keep only stable forward-slash file names.
    files = []
    for path, _line in entry.occurrences:
        path = LINE_NUMBER.sub("", path.replace("\\", "/"))
        if path not in files:
            files.append(path)
    return [(path, "") for path in files]


def diff(args):
    fork = polib.pofile(args.fork, wrapwidth=79)
    base_keys = {entry_key(entry) for entry in polib.pofile(args.base)}
    result = polib.POFile(wrapwidth=79)
    result.metadata = fork.metadata
    result.header = ("Strings added by the access fork that its upstream "
                     "base lacks.")
    for entry in fork:
        if entry_key(entry) in base_keys:
            continue
        entry.occurrences = normalized_occurrences(entry)
        result.append(entry)
    result.save(args.output)
    print(f"{len(result)} fork strings written to {args.output}")


def init(args):
    upstream = polib.pofile(args.upstream)
    result = polib.POFile(wrapwidth=79)
    result.header = HEADERS[args.layer].format(args.language)
    result.metadata = {
        "Project-Id-Version": "cataclysm-dda-access",
        "PO-Revision-Date":
            datetime.now().astimezone().strftime("%Y-%m-%d %H:%M%z"),
        "Last-Translator": "Cataclysm: DDA Access contributors",
        "Language-Team": args.language,
        "Language": args.language,
        "MIME-Version": "1.0",
        "Content-Type": "text/plain; charset=UTF-8",
        "Content-Transfer-Encoding": "8bit",
    }
    plural_forms = upstream.metadata.get("Plural-Forms")
    if plural_forms:
        result.metadata["Plural-Forms"] = plural_forms
    result.save(args.output)


def gaps(args):
    template = polib.pofile(args.template, wrapwidth=79)
    fork_keys = {entry_key(entry) for entry in polib.pofile(args.fork)}
    translated = {entry_key(entry) for entry in polib.pofile(args.upstream)
                  if has_translation(entry)}
    result = polib.POFile(wrapwidth=79)
    result.metadata = template.metadata
    result.header = ("Upstream strings that lang/po and lang/access leave "
                     "untranslated.")
    for entry in template:
        key = entry_key(entry)
        if key in fork_keys or key in translated:
            continue
        entry.occurrences = normalized_occurrences(entry)
        result.append(entry)
    result.save(args.output)
    print(f"{len(result)} untranslated upstream strings written to "
          f"{args.output}")


def overrides(args):
    catalog = polib.pofile(args.overrides, wrapwidth=79)
    template_keys = {entry_key(entry)
                     for entry in polib.pofile(args.template)}
    fork_keys = {entry_key(entry) for entry in polib.pofile(args.fork)}
    upstream = {entry_key(entry): entry
                for entry in polib.pofile(args.upstream)
                if has_translation(entry)}
    kept = []
    for entry in catalog:
        key = entry_key(entry)
        if key in fork_keys:
            print(f"Warning: {args.overrides} overrides the fork string "
                  f"{entry.msgid!r}; move it to lang/access.")
        if key not in template_keys:
            print(f"Dropping obsolete override {entry.msgid!r}")
            continue
        if key in upstream and same_translation(entry, upstream[key]):
            print(f"Dropping override that upstream now matches "
                  f"{entry.msgid!r}")
            continue
        kept.append(entry)
    if len(kept) != len(catalog):
        catalog[:] = kept
        catalog.save(args.overrides)


def translations(entry):
    """Pairs of the original and each non-empty translation."""
    if not entry.msgid_plural:
        return [(entry.msgid, entry.msgstr)] if entry.msgstr else []
    return [(entry.msgid if index == 0 else entry.msgid_plural, msgstr)
            for index, msgstr in sorted(entry.msgstr_plural.items())
            if msgstr]


def paired_tag_counts(tags):
    """Opening minus closing counts of tags that come in pairs."""
    counts = {}
    for tag in tags:
        if tag.startswith("</"):
            name = tag[2:-1]
            counts[name] = counts.get(name, 0) - 1
        else:
            name = "color" if tag.startswith("<color_") else tag[1:-1]
            counts[name] = counts.get(name, 0) + 1
    return counts


def tag_problem(source, msgstr):
    # A translation may leave out a substitution such as <mypronoun> that
    # its grammar does not need, but every tag it uses must exist in the
    # original and paired tags must stay balanced.
    source_tags = TAG.findall(source)
    msgstr_tags = TAG.findall(msgstr)
    unknown = sorted(set(msgstr_tags) - set(source_tags))
    if unknown:
        return "Tags missing from the original: " + " ".join(unknown)
    source_counts = paired_tag_counts(source_tags)
    msgstr_counts = paired_tag_counts(msgstr_tags)
    for name, count in source_counts.items():
        closing = any(tag == f"</{name}>" for tag in source_tags)
        if closing and msgstr_counts.get(name, 0) != count:
            return f"Unbalanced <{name}> tags"
    return None


def entry_problem(entry):
    for source, msgstr in translations(entry):
        ok, _seg_msgid, _seg_msgstr, reason = check_message(
            polib.POEntry(msgid=source, msgstr=msgstr))
        if not ok:
            return reason
        problem = tag_problem(source, msgstr)
        if problem:
            return problem
    return None


def clear(entry):
    if entry.msgid_plural:
        for index in entry.msgstr_plural:
            entry.msgstr_plural[index] = ""
    else:
        entry.msgstr = ""


def validate(args):
    catalog = polib.pofile(args.catalog, wrapwidth=79)
    failures = 0
    for entry in catalog:
        if entry.obsolete or not translations(entry):
            continue
        reason = entry_problem(entry)
        if reason is None:
            continue
        failures += 1
        action = "cleared" if args.clear else "error"
        print(f"{args.catalog}: {action}: {reason}: {entry.msgid!r}")
        if args.clear:
            clear(entry)
            if "fuzzy" in entry.flags:
                entry.flags.remove("fuzzy")
    if args.clear:
        if failures:
            catalog.save(args.catalog)
        return 0
    return 1 if failures else 0


def status(args):
    catalog = polib.pofile(args.catalog)
    untranslated = len(catalog.untranslated_entries())
    fuzzy = len(catalog.fuzzy_entries())
    drafts = sum(1 for entry in catalog.translated_entries()
                 if AI_DRAFT in entry.tcomment.splitlines())
    message = (f"{args.catalog}: {len(catalog)} entries, {untranslated} "
               f"untranslated, {fuzzy} fuzzy")
    if drafts:
        message += f", {drafts} AI drafts awaiting review"
    print(message)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    diff_parser = commands.add_parser("diff")
    diff_parser.add_argument("fork")
    diff_parser.add_argument("base")
    diff_parser.add_argument("output")
    diff_parser.set_defaults(run=diff)
    init_parser = commands.add_parser("init")
    init_parser.add_argument("--layer", choices=sorted(HEADERS),
                             default="access")
    init_parser.add_argument("language")
    init_parser.add_argument("upstream")
    init_parser.add_argument("output")
    init_parser.set_defaults(run=init)
    gaps_parser = commands.add_parser("gaps")
    gaps_parser.add_argument("template")
    gaps_parser.add_argument("fork")
    gaps_parser.add_argument("upstream")
    gaps_parser.add_argument("output")
    gaps_parser.set_defaults(run=gaps)
    overrides_parser = commands.add_parser("overrides")
    overrides_parser.add_argument("overrides")
    overrides_parser.add_argument("template")
    overrides_parser.add_argument("fork")
    overrides_parser.add_argument("upstream")
    overrides_parser.set_defaults(run=overrides)
    validate_parser = commands.add_parser("validate")
    validate_parser.add_argument("--clear", action="store_true")
    validate_parser.add_argument("catalog")
    validate_parser.set_defaults(run=validate)
    status_parser = commands.add_parser("status")
    status_parser.add_argument("catalog")
    status_parser.set_defaults(run=status)
    args = parser.parse_args()
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
