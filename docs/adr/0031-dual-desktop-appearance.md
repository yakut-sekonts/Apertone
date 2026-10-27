# ADR 0031: Dual desktop appearance

## Status

Accepted for Luma 1.1.0.

## Context

The 1.0 interface used large, loosely related Qt Basic controls and applied the
same visual hierarchy to navigation, editing and dialogs. It read more like a
touch interface than a macOS desktop editor and consumed too much working space.

## Decision

Luma provides three appearance settings: `system`, `day` and `night`. `system`
follows the macOS color scheme. `day` uses the Framed Sheet direction and
`night` uses the Data Terminal direction. The selection is stored atomically in
`appearance.json` next to the catalog preferences and defaults to `system`
without writing a file during startup.

All neutral colors, semantic colors, spacing, control heights, radii and primary
panel widths come from `Theme.qml`. Shared controls derive from Qt Quick Controls
and retain their native input, focus and accessibility behavior. They only
replace presentation and density.

The photo canvas remains dark in both appearances. This keeps the perceived
brightness of a photograph stable when the surrounding interface changes. Day
and Night change navigation, panels, dialogs and controls around that canvas.

## Consequences

New UI must use theme tokens rather than add neutral hexadecimal colors directly.
Functional colors inside histograms, masks, labels and image overlays may remain
local when they encode data rather than interface chrome.

Appearance persistence is covered together with runtime localization. QML shell,
common image workflow and native language checks must remain warning-free before
the 1.1.0 release candidate is published.
