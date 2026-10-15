# Luma 0.4.13 — branded installer and current release

The public DMG still contains 0.3.0 while newer Develop, languages and ordinary
image support exist only in source. This release ships the completed implementation
as matching DMG/ZIP assets and makes the installed version visible.

CMake PROJECT_VERSION now drives the binary, bundle, Settings, window title and
status bar. The original Luma half-disc logo is embedded as a multi-resolution
ICNS app icon and a portable custom DMG volume icon. Volume flags are set on a
mounted writable image before compression, rather than relying on staging-folder
attributes that do not survive image creation. Batch controls wrap on narrow
native windows, Library rows shrink to their available width, and retranslation
checks cover minimum/default window sizes after QML layout settles.

Release verification checks the actual read-only mounted installer: version,
icons/custom flag, Applications symlink, notes, provenance, strict signature and
binary startup. Standalone verification covers the current Develop, languages and
ordinary-image workflows with development SDKs unavailable. Existing build/tests,
format/tidy, packaging recovery and signed update/publication validation gate
release. The tag workflow publishes verified assets and the signed Sparkle feed
as latest only after checksums/source/upload integrity checks.

Ad-hoc signing and the absence of Developer ID/notarization remain explicit in
release notes. No image algorithms, catalog schemas or phase-0.5 work are included.
See ADR 0021 and docs/releases/v0.4.13.md.
