# Accessibility work in progress

Keep this file short. It contains only active and near-term work. Move completed
items to `ACCESS_DONE.md` together with their verification notes.

## Next playable slice

- [ ] Announce new game messages without repeating message-history redraws.
- [ ] Announce movement results, the current tile, and nearby threats.
- [ ] Add a concise command for reviewing the current location and surroundings.

## Platform follow-up

- [ ] After the native playable slice is stable, add an Emscripten backend that
  exposes the same semantic events through browser accessibility APIs and ARIA.
