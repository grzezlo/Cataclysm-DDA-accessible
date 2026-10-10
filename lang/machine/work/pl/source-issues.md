# Source-string problems found during Polish translation

Problems in English msgids that `validate` reports and no translation can
fix.  Check and fix them outside the translation sessions.

- Batch 0001, entry 76: "%1s failed to perform the %2$s activity because no
  suitable locations were found." mixes non-positional `%1s` with
  positional `%2$s`; it should read `%1$s`.  Reported source:
  `src/activity_item_handling.cpp`.
