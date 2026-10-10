---
description: Translate and review a range of Polish machine translation batches
argument-hint: <first>-<last>, for example 0001-0007
---

Translate and review the batches in `lang/machine/work/pl/` numbered
$ARGUMENTS (inclusive), in ascending order.  You coordinate; the
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

       uv run --no-project --with polib python lang/access_catalog.py status lang/machine/work/pl/NNNN.po

   If it reports untranslated or fuzzy entries, invoke `pl-translator` with
   the prompt "Translate lang/machine/work/pl/NNNN.po." and check again.
   If entries are still missing, invoke `pl-translator` once more with the
   prompt "Finish lang/machine/work/pl/NNNN.po; some entries are still
   untranslated or fuzzy."  If that fails too, stop and report the batch.
3. Invoke `pl-reviewer` with the prompt "Review
   lang/machine/work/pl/NNNN.po."
4. Verify the result; if any check fails, stop and report the batch:

       uv run --no-project --with polib python lang/access_catalog.py validate lang/machine/work/pl/NNNN.po
       msgfmt -c -o /tmp/check.mo lang/machine/work/pl/NNNN.po
       uv run --no-project --with polib python lang/access_catalog.py status lang/machine/work/pl/NNNN.po

   `status` must report 0 untranslated and 0 fuzzy entries, and
   `lang/machine/work/pl/NNNN.review.md` must exist.

   Ignore a `validate` error caused by the English source string itself
   (for example a msgid that mixes positional and non-positional
   arguments): no translation can fix it.  It does not fail the batch.
   Append the batch, entry number, msgid, and problem to
   `lang/machine/work/pl/source-issues.md` for review outside the session,
   unless that entry is already listed there.
5. Commit and push only that batch, so finished work survives if the
   session ends:

       git add lang/machine/work/pl/NNNN.po lang/machine/work/pl/NNNN.review.md
       git add lang/machine/work/pl/source-issues.md   # if it changed
       git commit -m "Translate Polish machine batch NNNN"
       git push -u origin HEAD

When the range is done or you stop, reply with a short list: each batch, its
entry count, the number of review corrections (the `Reviewed:` line of its
log), any source-string problems noted, and any batch that failed with the
reason.
