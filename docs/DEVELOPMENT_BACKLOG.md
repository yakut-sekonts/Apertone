# Development backlog toward 1.0.0

Implementation continues without intermediate QA by user direction on
2026-10-20. Open reports are not independently reproduced or qualified.
All deferred checks and fixes must be covered in the final pre-release campaign.
The 23 accepted 0.5.1 qualification gaps remain tracked in
[Issue #48](https://github.com/yakut-sekonts/Luma/issues/48) and
[the release record](releases/v0.5.1-quality.md).

## LUMA-BUG-035 — Library scroll returns to the beginning on photo selection

- GitHub: [Issue #49](https://github.com/yakut-sekonts/Luma/issues/49).
- Status: **open — reported by user; fix pending**.
- Reported: 2026-10-20, after installation of the 0.5.1 release.
  The exact running build has not been independently established.
- Reproduction supplied by the user: import photographs, scroll down through
  the Library, then select a photograph.
- Actual: the list jumps back to the beginning after every selection.
- Expected: selecting a photograph preserves the visible asset anchor and its
  offset in the viewport; the selected photo opens without resetting scroll.
- Evidence supplied: local screen recording
  `Запись экрана — 2026-10-20 в 17.33.02.mov`. The recording has not been
  independently analyzed; the description above records the user's report.
- Likely investigation area, not an established cause: Library selection,
  `photoOpened`, model reload/reset, and viewport restoration.
- Final qualification must cover click selection after deep scrolling, repeated
  selection, paged results, sort/filter combinations and asynchronous photo load.
