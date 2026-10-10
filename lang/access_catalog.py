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
split:     write the entries that need translation to numbered batch files
           in player-visible order, so translators can work in batches.
merge:     copy the valid, non-fuzzy translations of batch files back into
           their catalog.
lookup:    print existing translations whose source or translation matches
           a pattern, as translation memory.
show:      print the numbered entries of a batch with their context.
fill:      set translations of a batch from a JSON object keyed by entry
           number; --log records changed translations with their reasons.
"""

import argparse
import glob
import json
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


def source_problem(entry):
    """A format error in the original, which no translation can fix."""
    for source in (entry.msgid, entry.msgid_plural):
        if not source:
            continue
        ok, _seg_msgid, _seg_msgstr, reason = check_message(
            polib.POEntry(msgid=source, msgstr=source))
        if not ok:
            return reason
    return None


def entry_problem(entry):
    reason = source_problem(entry)
    if reason:
        return f"Invalid original string: {reason}"
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
        if source_problem(entry) and not args.clear:
            # The translator cannot fix the original; merge and --clear
            # drop the translation, so it never reaches the game.
            print(f"{args.catalog}: warning: {reason}: {entry.msgid!r}")
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
    for path in args.catalogs:
        catalog = polib.pofile(path)
        # An invalid original cannot be translated, so it is not counted.
        untranslated = sum(1 for entry in catalog.untranslated_entries()
                           if not source_problem(entry))
        fuzzy = len(catalog.fuzzy_entries())
        drafts = sum(1 for entry in catalog.translated_entries()
                     if AI_DRAFT in entry.tcomment.splitlines())
        message = (f"{path}: {len(catalog)} entries, {untranslated} "
                   f"untranslated, {fuzzy} fuzzy")
        if drafts:
            message += f", {drafts} AI drafts awaiting review"
        print(message)


# Players meet interface strings (rank 0) first, then items and monsters,
# other core data, flavor texts, dialogue, and finally the bundled mods.
# The first matching prefix wins.
SOURCE_ORDER = [
    ("data/json/items/", 1),
    ("data/json/monsters/", 2),
    ("data/json/snippets/", 4),
    ("data/json/npcs/", 5),
    ("data/json/", 3),
    ("data/", 6),
]


def source_rank(entry):
    path = entry.occurrences[0][0] if entry.occurrences else ""
    for prefix, rank in SOURCE_ORDER:
        if path.startswith(prefix):
            return rank
    return 0


def needs_translation(entry):
    return (not entry.obsolete and not source_problem(entry) and
            ("fuzzy" in entry.flags or not has_translation(entry)))


def batch_paths(directory):
    return sorted(glob.glob(os.path.join(directory, "[0-9]*.po")))


def split(args):
    if batch_paths(args.directory):
        print(f"Error: {args.directory} already holds batches; merge or "
              f"remove them first.")
        return 1
    catalog = polib.pofile(args.catalog, wrapwidth=79)
    for entry in catalog:
        reason = source_problem(entry)
        if reason and not entry.obsolete:
            print(f"Skipping an invalid original string: {reason}: "
                  f"{entry.msgid!r}")
    pending = [entry for entry in catalog if needs_translation(entry)]
    pending.sort(key=source_rank)
    batches = []
    for entry in pending:
        # A batch never mixes source groups, so each covers related strings.
        if (not batches or len(batches[-1]) >= args.size or
                source_rank(batches[-1][0]) != source_rank(entry)):
            batches.append([])
        batches[-1].append(entry)
    os.makedirs(args.directory, exist_ok=True)
    for number, entries in enumerate(batches, start=1):
        batch = polib.POFile(wrapwidth=79)
        batch.metadata = catalog.metadata
        batch.header = (f"Batch {number} of {len(batches)} from "
                        f"{args.catalog}.\nMerge it back with "
                        f"lang/access_catalog.py merge.")
        batch.extend(entries)
        batch.save(os.path.join(args.directory, f"{number:04d}.po"))
    print(f"{len(pending)} entries written to {len(batches)} batches in "
          f"{args.directory}")
    return 0


def merge(args):
    catalog = polib.pofile(args.catalog, wrapwidth=79)
    targets = {entry_key(entry): entry for entry in catalog
               if not entry.obsolete}
    merged = 0
    skipped = 0
    for path in batch_paths(args.directory):
        for entry in polib.pofile(path):
            if (entry.obsolete or "fuzzy" in entry.flags or
                    not has_translation(entry)):
                continue
            target = targets.get(entry_key(entry))
            if target is None or target.msgid_plural != entry.msgid_plural:
                print(f"{path}: skipped, source string changed: "
                      f"{entry.msgid!r}")
                skipped += 1
                continue
            reason = entry_problem(entry)
            if reason:
                print(f"{path}: skipped: {reason}: {entry.msgid!r}")
                skipped += 1
                continue
            if target.msgid_plural:
                target.msgstr_plural = dict(entry.msgstr_plural)
            else:
                target.msgstr = entry.msgstr
            if "fuzzy" in target.flags:
                target.flags.remove("fuzzy")
            target.previous_msgctxt = None
            target.previous_msgid = None
            target.previous_msgid_plural = None
            merged += 1
    catalog.save(args.catalog)
    print(f"{merged} translations merged into {args.catalog}, {skipped} "
          f"skipped")
    return 0


def lookup(args):
    flags = re.IGNORECASE if args.ignore_case else 0
    pattern = re.compile(args.pattern, flags)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    shown = 0
    for path in args.catalogs:
        if shown >= args.limit:
            break
        for entry in polib.pofile(path):
            if (shown >= args.limit or entry.obsolete or
                    "fuzzy" in entry.flags or not has_translation(entry)):
                continue
            if entry.msgid_plural:
                msgstr = " | ".join(entry.msgstr_plural[index]
                                    for index in sorted(entry.msgstr_plural))
            else:
                msgstr = entry.msgstr
            texts = [msgstr] if args.translation else [entry.msgid,
                                                       entry.msgid_plural]
            if not any(pattern.search(text) for text in texts if text):
                continue
            context = f"[{entry.msgctxt}] " if entry.msgctxt else ""
            print(f"{path}: {context}{entry.msgid!r}\n    -> {msgstr!r}")
            shown += 1
    return 0


def numbered_entries(catalog):
    return [entry for entry in catalog if not entry.obsolete]


def plural_count(catalog):
    match = re.search(r"nplurals\s*=\s*(\d+)",
                      catalog.metadata.get("Plural-Forms", ""))
    return int(match.group(1)) if match else 2


def translation_text(entry):
    if entry.msgid_plural:
        return [entry.msgstr_plural.get(index, "")
                for index in sorted(entry.msgstr_plural)]
    return entry.msgstr


def show(args):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    entries = numbered_entries(polib.pofile(args.catalog))
    last = min(args.last or len(entries), len(entries))
    for number in range(max(args.first, 1), last + 1):
        entry = entries[number - 1]
        print(f"=== {number}")
        reason = source_problem(entry)
        if reason:
            print(f"!!! Invalid original string ({reason}); leave the "
                  f"translation empty.")
        for line in entry.comment.splitlines():
            print(f"#. {line}")
        for line in entry.tcomment.splitlines():
            print(f"# {line}")
        for path, _line in entry.occurrences:
            print(f"#: {path}")
        if entry.flags:
            print(f"#, {', '.join(entry.flags)}")
        if entry.previous_msgid:
            print(f"previous msgid: "
                  f"{json.dumps(entry.previous_msgid, ensure_ascii=False)}")
        if entry.msgctxt is not None:
            print(f"msgctxt: {json.dumps(entry.msgctxt, ensure_ascii=False)}")
        print(f"msgid: {json.dumps(entry.msgid, ensure_ascii=False)}")
        if entry.msgid_plural:
            print(f"msgid_plural: "
                  f"{json.dumps(entry.msgid_plural, ensure_ascii=False)}")
        print(f"translation: "
              f"{json.dumps(translation_text(entry), ensure_ascii=False)}")
    return 0


def fill(args):
    catalog = polib.pofile(args.catalog, wrapwidth=79)
    entries = numbered_entries(catalog)
    nplurals = plural_count(catalog)
    with open(args.translations, encoding="utf-8") as stream:
        values = json.load(stream)
    errors = []
    changes = []
    for key, value in values.items():
        reason = ""
        if isinstance(value, dict):
            reason = value.get("reason", "")
            value = value.get("translation")
        if not key.isdigit() or not 1 <= int(key) <= len(entries):
            errors.append(f"{key}: no such entry")
            continue
        entry = entries[int(key) - 1]
        if entry.msgid_plural:
            if (not isinstance(value, list) or len(value) != nplurals or
                    not all(isinstance(text, str) for text in value)):
                errors.append(f"{key}: expected a list of {nplurals} plural "
                              f"forms")
                continue
        elif not isinstance(value, str):
            errors.append(f"{key}: expected a string")
            continue
        changes.append((int(key), entry, value, reason))
    if errors:
        for error in errors:
            print(f"{args.translations}: error: {error}")
        print("Nothing was changed.")
        return 1
    log = []
    for number, entry, value, reason in sorted(changes, key=lambda c: c[0]):
        old = translation_text(entry)
        if old == value:
            continue
        if any(old if isinstance(old, list) else [old]):
            log.append(f"- {number} `{entry.msgid}`\n"
                       f"  - before: {json.dumps(old, ensure_ascii=False)}\n"
                       f"  - after: {json.dumps(value, ensure_ascii=False)}\n"
                       f"  - reason: {reason or 'not given'}\n")
        if entry.msgid_plural:
            entry.msgstr_plural = dict(enumerate(value))
        else:
            entry.msgstr = value
        # A translation set here is current, so it is no longer fuzzy.
        if "fuzzy" in entry.flags and has_translation(entry):
            entry.flags.remove("fuzzy")
            entry.previous_msgctxt = None
            entry.previous_msgid = None
            entry.previous_msgid_plural = None
    catalog.save(args.catalog)
    print(f"{len(changes)} translations set in {args.catalog}, "
          f"{len(log)} changed")
    if args.log:
        with open(args.log, "a", encoding="utf-8") as stream:
            stream.writelines(log)
    return 0


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
    status_parser.add_argument("catalogs", nargs="+")
    status_parser.set_defaults(run=status)
    split_parser = commands.add_parser("split")
    split_parser.add_argument("--size", type=int, default=300)
    split_parser.add_argument("catalog")
    split_parser.add_argument("directory")
    split_parser.set_defaults(run=split)
    merge_parser = commands.add_parser("merge")
    merge_parser.add_argument("catalog")
    merge_parser.add_argument("directory")
    merge_parser.set_defaults(run=merge)
    lookup_parser = commands.add_parser("lookup")
    lookup_parser.add_argument("-i", "--ignore-case", action="store_true")
    lookup_parser.add_argument("--translation", action="store_true",
                               help="match the translation, not the source")
    lookup_parser.add_argument("--limit", type=int, default=20,
                               help="matches shown in total, from the "
                               "first catalog onward")
    lookup_parser.add_argument("pattern")
    lookup_parser.add_argument("catalogs", nargs="+")
    lookup_parser.set_defaults(run=lookup)
    show_parser = commands.add_parser("show")
    show_parser.add_argument("catalog")
    show_parser.add_argument("first", type=int, nargs="?", default=1)
    show_parser.add_argument("last", type=int, nargs="?")
    show_parser.set_defaults(run=show)
    fill_parser = commands.add_parser("fill")
    fill_parser.add_argument("--log", help="append changed translations, "
                             "with reasons, to this Markdown file")
    fill_parser.add_argument("catalog")
    fill_parser.add_argument("translations")
    fill_parser.set_defaults(run=fill)
    args = parser.parse_args()
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
