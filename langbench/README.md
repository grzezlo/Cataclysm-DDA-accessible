# Langbench: choosing a model for machine translation

This folder compares AI models for the machine translation layer in
`lang/machine` (Stage 23A in `ACCESS_TODO.md`).  Each model translates the
same sample on its own branch; the results are then compared blind.

## Instructions for the translating model

You are translating part of the game Cataclysm: Dark Days Ahead from English
into Polish.

### Task

1. Find the current git branch name, for example `model1`.  This is your
   output folder name.  Write your results to `langbench/<branch name>/`.
2. For every file in `langbench/source/`, write a file with the same name
   to your output folder.  Keep every entry, its comments, `msgctxt`,
   `msgid`, `msgid_plural`, and flags unchanged; only fill in `msgstr` and
   `msgstr[N]`.  Do not add or remove entries and do not add the `fuzzy`
   flag.
3. Translate every entry.  `overrides.po` is different; see below.
4. Validate every output file and fix every reported problem (see
   Validation).
5. Commit only your output folder on your branch.  Do not change
   `langbench/source/`, other files in the repository, or other folders in
   `langbench/`, and do not read other folders in `langbench/` apart from
   `source/`.
6. Do not write your model name or vendor into the output files; the
   comparison is blind.

### Context you may use

- `lang/po/pl.po`: the existing human Polish translation of the game.  Use
  its terminology, item and monster names, style, and conventions.  Search it
  for terms that appear in your strings, for example the item a variant name
  refers to, so that your translation is consistent with the rest of the
  game.
- `lang/notes/README_all_translators.md`: format strings, tags, and plural
  forms.
- The game data and source files named in the `#:` reference comments, to
  understand where and how a string is used.  `#.` comments explain the
  string's role, for example "Item name" or "NPC dialogue line".

### Translation rules

- Follow the conventions of `lang/po/pl.po`.  It addresses the player in the
  second person singular ("Czujesz, że..."), uses the masculine form when
  the player's gender is not known, and keeps the established names of
  items, monsters, factions, and game terms.
- Plural forms: `msgstr[0]` is used for 1, `msgstr[1]` for 2–4, 22–24,
  32–34, and so on, `msgstr[2]` for 0, 5–21, 25–31, and so on, and
  `msgstr[3]` for fractions.  For item names this gives, for example,
  "latarka", "latarki", "latarek", "latarki".
- Keep every format argument (`%s`, `%d`, `%1$s`, `%2$d`, `%%`) with the
  same type.  To reorder non-positional arguments, convert all of them to
  positional ones (`%1$s`, `%2$d`).
- Keep tags exactly as written: color tags such as `<color_red>` and
  `</color>`, `<info>`, `<good>`, `<bad>`, and substitution tags such as
  `<npcname>`, `<the_cataclysm>`, `<name_b>`, or `<u_val:name>`.  Never
  translate or invent a tag.  You may leave out a substitution tag only when
  Polish grammar does not need it, as with `<mypronoun>`.
- Keep leading and trailing spaces, line breaks (`\n`), and punctuation that
  the interface relies on, such as a trailing colon.
- Single letters in angle brackets, such as `<E>nable`, mark a hotkey
  letter.  Translators may move the brackets to a letter of the translated
  word.
- Translate meaning, not word order, and write natural Polish.  Dialogue
  should sound like the speaking character; notes and signs like the
  writer; interface text should be short and clear.

### overrides.po

These are existing upstream Polish translations that an automatic check
flagged.  The translator comment `# Problem: ...` names the detected problem.

- If the translation is wrong, correct it in `msgstr` and keep the rest of
  the translation where it is good.
- If the check is a false positive, for example because the tag is literal
  display text that should be translated, keep or improve the translation and
  replace the translator comment with `# False positive: <reason>`.

### Validation

Run these commands from the repository root for each output file.  gettext
is in `C:\gettext\bin` if it is not on `PATH`; write the `msgfmt` output to
a temporary file outside the repository.

```
uv run --no-project --with polib python lang/access_catalog.py validate langbench/<branch name>/<file>.po
msgfmt -c -o <temporary file> langbench/<branch name>/<file>.po
```

In `overrides.po`, an entry kept as a false positive may still be reported;
that is expected.

## Comparison

`make_sample.py` builds `source/` deterministically from
`lang/po/gaps-pl.pot` (written by `update_translations_win_x64.bat --machine
pl`) and `lang/po/pl.po`:

- `ui.po`: 30 interface strings from `src/`.
- `items.po`: 40 item names and descriptions.
- `world.po`: 35 strings from terrain, furniture, monsters, mutations,
  effects, recipes, skills, professions, and other core data.
- `dialogue.po`: 30 NPC dialogue and mission strings.
- `snippets.po`: 15 notes, signs, and other flavor texts.
- `mods.po`: 15 strings from bundled mods.
- `overrides.po`: the upstream Polish translations that the validator
  flags.

Each sample reserves a share for plural forms, tags, format arguments, and
`msgctxt`.  After the branches are merged, each model's results are in
`langbench/model1/`, `langbench/model2/`, and so on.  The evaluator does not
know which model produced which folder until the comparison is finished.

Criteria:

1. Validation errors and untranslated entries.
2. Meaning: mistranslations, omissions, and additions.
3. Consistency with the terminology of `lang/po/pl.po`.
4. Grammar: inflection, gender, and the four plural forms.
5. Style and register: natural Polish, character voice, concise interface
   text.
6. Judgment on `overrides.po`: real problems fixed, false positives
   recognized.
7. Cost and time per run, recorded by the maintainer.
