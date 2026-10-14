# 0021 — Branded macOS installer and one application version

The human requested 0.4.13 as a public release containing all completed pre-0.5
work. Keep CMake PROJECT_VERSION as the only source for binary, bundle, UI and
release versions; remove the separately hard-coded development string. Show the
version in Settings and the workspace status bar, including --version and Finder.

Use Luma's existing gold half-disc logo for a native multi-resolution ICNS icon.
Original vector drawing code and its generated artwork are owned by Luma. Embed
CFBundleIconFile and the resource in every app; embed .VolumeIcon.icns and custom
volume flags inside the disk image so the branding survives HTTP downloads.
Do not rely on workstation-only Finder extended attributes for distribution.

The tag workflow publishes verified immutable DMG/ZIP, checksums, source build
information and a signed Sparkle feed as the latest GitHub Release. Validate the
actual mounted DMG's version, icon, Application link, code signature and standalone
startup before publication. Keep the existing ad-hoc/notarization status explicit.
No image algorithms or catalog schemas change. Phase 0.5 remains on hold.
