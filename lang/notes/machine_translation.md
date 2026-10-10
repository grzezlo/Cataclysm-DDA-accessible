# Machine translation workflow

This procedure fills `lang/machine/<language>.po`, the AI translations of
upstream strings that `lang/po` leaves untranslated (Stage 23A in
`ACCESS_TODO.md`).  Any AI tool can follow it: Claude Code, Codex, or
another agent.  The examples use Polish (`pl`); replace it with the language
code you work on.

## Roles

Each batch is translated by one model and then reviewed by a different one,
so the reviewer does not share the translator's blind spots.  For Polish,
the `langbench` comparison chose Opus (medium effort) or GPT 6.1 Sol
(medium) to translate and Sonnet (high effort) to review.  Never let the
translating model review its own batch.

## Setup

- Python with `uv`, and gettext (`msgfmt`).  On Windows, the tools also find
  `C:\gettext\bin\msgfmt.exe`.
- Run every command from the repository root.  `catalog` below stands for
  `uv run --no-project --with polib python lang/access_catalog.py`; type the
  full command.
- Keep temporary files outside the repository: `/tmp` on Linux and in Git
  Bash, `$env:TEMP` in PowerShell.
- Batches live in `lang/machine/work/<language>/NNNN.po` and come from
  `catalog split`.  Lower numbers are seen earlier in play.

## Translator

Translate one batch, for example `lang/machine/work/pl/0007.po`.

1. Read `lang/notes/<language>.txt` (for Polish, `lang/notes/pl.txt`)
   completely and follow it.  It names the conventions, the grammar traps
   around tags and format arguments, and the established terms.  Read
   `lang/notes/README_all_translators.md` for format strings, tags, and
   plural forms.
2. Print the batch in ranges: `catalog show <batch> 1 60`.  Each entry has a
   number, extracted comments (`#.`), source references (`#:`), msgctxt,
   msgid, msgid_plural, and its current translation.
3. Translate every entry whose translation is empty.  An entry marked fuzzy
   has an outdated translation of a changed source string; translate it
   again.  Leave empty any entry that `show` marks with "Invalid original
   string": its English text has a format error that no translation can
   fix, and the tools never use its translation.
   - Search the translation memory for every name and game term:
     `catalog lookup -i "<regex>" lang/po/pl.po "lang/machine/work/pl/*.po"`.
     Add `--translation` to search the translated text instead.
   - When the comments and the memory leave the meaning unclear, read the
     game data or source file in the `#:` reference.
   - Keep every format argument and tag exactly; never translate or invent
     a tag.  Keep leading and trailing spaces and `\n`.
4. Write the translations to a JSON file outside the repository, keyed by
   entry number; a plural entry takes a list of all plural forms:
   `{"1": "tekst", "2": ["forma 1", "forma 2-4", "forma 5+", "ułamki"]}`.
   Use several files for a long batch; each one adds to the batch.
5. Apply each file with `catalog fill <batch> <json file>`.  If `fill`
   reports an error, it changed nothing; fix the JSON and run it again.
6. Run `catalog check <batch>` and fix every error until it reports
   "complete and valid".  Warnings about an invalid original string are
   expected.

## Reviewer

Review one translated batch.  Find and fix errors; do not restyle correct
translations, even if you would have worded them differently.

1. Read `lang/notes/<language>.txt` completely; it is your checklist.  Read
   `lang/notes/README_all_translators.md`.
2. Print the batch in ranges with `catalog show <batch> 1 60` and compare
   each translation with its source, comments, and msgctxt.  Fix:
   - meaning: mistranslations, omissions, additions, and the wrong
     referent.  Read the game data in the `#:` reference whenever a name
     could mean several things;
   - terminology that differs from `lang/po/<language>.po` or earlier
     batches (check with `catalog lookup`);
   - grammar around substitutions: the form a tag or `%s` inserts, gender
     assumptions about `<npcname>` and item names, the gender selected by
     msgctxt, and every plural form;
   - grammar, spelling, typography, and interface punctuation;
   - voice: dialogue that does not sound like the speaker, intentional
     mistakes removed from children's notes, English left in running text.
   Leave alone entries marked "Invalid original string".
3. Write the corrections to a JSON file outside the repository, with the
   full corrected translation and a short English reason:
   `{"12": {"translation": "tekst", "reason": "wrong item: leather pouch"}}`.
4. Apply them and log the changes next to the batch (`0007.po` logs to
   `0007.review.md`):
   `catalog fill --log lang/machine/work/pl/0007.review.md <batch> <json file>`.
5. Run `catalog check <batch>` and fix every error.
6. Append a summary line to the review log, even when nothing changed,
   because the log marks the batch as reviewed:
   `Reviewed: <entries checked> entries, <entries changed> changed.`

## Running a range of batches

For each batch in ascending order:

1. Skip it if `NNNN.review.md` exists.
2. If `catalog check` reports untranslated or fuzzy entries, translate it
   (Translator), then commit only the batch file:
   `Translate Polish machine batch NNNN`.
3. Review it with a different model (Reviewer), then commit the batch file
   and its review log: `Review Polish machine batch NNNN`.

A tool that can run only one model translates its range and commits each
batch; another tool reviews the range afterwards.  In Claude Code,
`/translate-pl <first>-<last>` runs both steps with the `pl-translator`
(Opus) and `pl-reviewer` (Sonnet) subagents.

Rules for every tool:

- Change translations only, and only through `fill`; commit only the batch
  file and its review log.
- Do not write a model name or vendor into any file.
- Work on disjoint ranges.  Two tools that run at the same time need
  separate working trees (for example `git worktree add`), because they
  must not share one index.

## Finishing

When all batches are reviewed, merge them locally:

    catalog merge lang/machine/pl.po lang/machine/work/pl

Then run `update_translations_win_x64.bat --machine pl`, which validates the
machine catalog, and remove `lang/machine/work/pl`.
