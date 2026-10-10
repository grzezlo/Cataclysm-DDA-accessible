---
description: Translate and review a range of Polish machine translation batches
argument-hint: <first>-<last>, for example 0001-0007
---

Translate and review the batches in `lang/machine/work/pl/` numbered
$ARGUMENTS (inclusive), in ascending order, as described in "Running a range
of batches" in `lang/notes/machine_translation.md`.  You coordinate; the
`pl-translator` and `pl-reviewer` subagents do all translation work.  Do not
translate or edit translations yourself.

When you invoke a subagent, pass only `subagent_type` and the prompt.  Never
pass `model` or `effort`: each subagent's definition selects them, and an
explicit value would override it.

Run every command from the repository root.  For each batch `NNNN` in the
range:

1. Skip the batch if `lang/machine/work/pl/NNNN.review.md` exists; it has
   been reviewed already.
2. Check it:

       uv run --no-project --with polib python lang/access_catalog.py check lang/machine/work/pl/NNNN.po

   If it reports untranslated or fuzzy entries, invoke `pl-translator` with
   the prompt "Translate lang/machine/work/pl/NNNN.po." and check again.
   If entries are still missing, invoke `pl-translator` once more with the
   prompt "Finish lang/machine/work/pl/NNNN.po; some entries are still
   untranslated or fuzzy."  If `check` still fails, stop and report the
   batch.  When the translator ran, commit and push the batch at once, so
   the translation survives if the session ends:

       git add lang/machine/work/pl/NNNN.po
       git commit -m "Translate Polish machine batch NNNN"
       git push -u origin HEAD

3. Invoke `pl-reviewer` with the prompt "Review
   lang/machine/work/pl/NNNN.po."
4. Run `check` again.  It must report "complete and valid" (warnings about
   an invalid original string are expected), and
   `lang/machine/work/pl/NNNN.review.md` must exist.  Otherwise stop and
   report the batch.
5. Commit and push the review:

       git add lang/machine/work/pl/NNNN.po lang/machine/work/pl/NNNN.review.md
       git commit -m "Review Polish machine batch NNNN"
       git push -u origin HEAD

Push only in a cloud session, where the environment variable
`CLAUDE_CODE_REMOTE` is `true`.  A local session commits without pushing;
the maintainer pushes.

When the range is done or you stop, reply with a short list: each batch, its
entry count, the number of review corrections (the `Reviewed:` line of its
log), and any batch that failed with the reason.
