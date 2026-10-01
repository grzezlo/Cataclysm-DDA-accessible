#!/usr/bin/env python3
"""Helpers for the access fork's own translation catalogs in lang/access.

diff:  write the entries of the fork template that the upstream base
       template does not contain.
init:  create an empty catalog for a language, using the plural forms of
       that language's upstream catalog.
"""

import argparse
import re
import sys
from datetime import datetime

import polib

LINE_NUMBER = re.compile(r":\d+$")


def entry_key(entry):
    return (entry.msgctxt, entry.msgid)


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
    result.header = ("Translations of the access fork's own strings for "
                     f"{args.language}.\n"
                     "Upstream strings are translated in lang/po.")
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    diff_parser = commands.add_parser("diff")
    diff_parser.add_argument("fork")
    diff_parser.add_argument("base")
    diff_parser.add_argument("output")
    diff_parser.set_defaults(run=diff)
    init_parser = commands.add_parser("init")
    init_parser.add_argument("language")
    init_parser.add_argument("upstream")
    init_parser.add_argument("output")
    init_parser.set_defaults(run=init)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    sys.exit(main())
