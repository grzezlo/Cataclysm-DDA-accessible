#!/usr/bin/env python3
"""Build the translation benchmark sample in langbench/source.

Reads lang/po/gaps-pl.pot (written by update_translations_win_x64.bat
--machine pl) and lang/po/pl.po.  The selection is deterministic.
"""

import os
import random
import sys

import polib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "lang"))
from access_catalog import TAG, entry_problem, has_translation  # noqa: E402

SEED = 20261004
MAX_LENGTH = 1200

# name: (description, size, path predicate)
STRATA = {
    "ui": ("Interface strings from src/", 30,
           lambda f: f.startswith("src/")),
    "items": ("Item names and descriptions", 40,
              lambda f: f.startswith("data/json/items/")),
    "world": ("Terrain, furniture, monsters, mutations, effects, recipes, "
              "skills, professions, and other core data", 35,
              lambda f: f.startswith("data/json/") and not f.startswith(
                  ("data/json/items/", "data/json/npcs/",
                   "data/json/snippets", "data/json/obsoletion"))),
    "dialogue": ("NPC dialogue and missions", 30,
                 lambda f: f.startswith("data/json/npcs/")),
    "snippets": ("Snippets: notes, signs, rumors, and flavor text", 15,
                 lambda f: f.startswith("data/json/snippets")),
    "mods": ("Strings from bundled mods", 15,
             lambda f: f.startswith("data/mods/")),
}

PLURAL_FORMS = ("nplurals=4; plural=(n==1 ? 0 : (n%10>=2 && n%10<=4) && "
                "(n%100<12 || n%100>14) ? 1 : n!=1 && (n%10>=0 && n%10<=1) "
                "|| (n%10>=5 && n%10<=9) || (n%100>=12 && n%100<=14) "
                "? 2 : 3);")


def new_catalog(header):
    catalog = polib.POFile(wrapwidth=79)
    catalog.header = header
    catalog.metadata = {
        "Project-Id-Version": "cataclysm-dda-langbench",
        "PO-Revision-Date": "2026-10-04 00:00+0200",
        "Last-Translator": "langbench",
        "Language-Team": "pl",
        "Language": "pl",
        "MIME-Version": "1.0",
        "Content-Type": "text/plain; charset=UTF-8",
        "Content-Transfer-Encoding": "8bit",
        "Plural-Forms": PLURAL_FORMS,
    }
    return catalog


def first_file(entry):
    return entry.occurrences[0][0] if entry.occurrences else ""


def pick(rng, pool, size):
    """Choose size entries, reserving shares for plurals, tags, and printf
    arguments so every stratum exercises them when it has them."""
    chosen = []
    features = [
        (lambda e: e.msgid_plural, size // 5),
        (lambda e: TAG.search(e.msgid), size // 5),
        (lambda e: "c-format" in e.flags, size // 10),
        (lambda e: e.msgctxt, size // 10),
    ]
    for predicate, quota in features:
        candidates = [e for e in pool if predicate(e) and e not in chosen]
        chosen += rng.sample(candidates, min(quota, len(candidates)))
    rest = [e for e in pool if e not in chosen]
    chosen += rng.sample(rest, min(size - len(chosen), len(rest)))
    return sorted(chosen, key=lambda e: (first_file(e), e.msgid))


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    out = os.path.join(root, "langbench", "source")
    os.makedirs(out, exist_ok=True)
    rng = random.Random(SEED)
    gaps = [e for e in polib.pofile(os.path.join(root, "lang", "po",
                                                 "gaps-pl.pot"))
            if len(e.msgid) <= MAX_LENGTH]
    for name, (description, size, predicate) in STRATA.items():
        pool = [e for e in gaps if predicate(first_file(e))]
        catalog = new_catalog(f"Langbench sample: {description}.")
        for entry in pick(rng, pool, size):
            if entry.msgid_plural:
                entry.msgstr_plural = {i: "" for i in range(4)}
            catalog.append(entry)
        catalog.save(os.path.join(out, f"{name}.po"))
        print(f"{name}: {len(catalog)} of {len(pool)}")

    # Upstream translations that the validator rejects, to be corrected.
    catalog = new_catalog("Langbench sample: upstream Polish translations "
                          "that need correction.  Each translator comment "
                          "names the detected problem; fix the msgstr.")
    seen = set()
    for entry in polib.pofile(os.path.join(root, "lang", "po", "pl.po")):
        if not has_translation(entry) or entry.msgid in seen:
            continue
        problem = entry_problem(entry)
        if problem:
            seen.add(entry.msgid)
            entry.tcomment = f"Problem: {problem}"
            entry.flags = [f for f in entry.flags if f != "fuzzy"]
            entry.previous_msgid = None
            entry.previous_msgid_plural = None
            entry.previous_msgctxt = None
            catalog.append(entry)
    catalog.save(os.path.join(out, "overrides.po"))
    print(f"overrides: {len(catalog)}")


if __name__ == "__main__":
    sys.exit(main())
