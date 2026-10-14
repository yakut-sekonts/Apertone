# 0019 — Embedded language packs and English-first UI

Accepted for the human-requested pre-phase-0.5 extension, version 0.4.11.

Use the existing Qt translation mechanism with an embedded QTranslator-backed,
versioned UTF-8 JSON catalog for each requested language: en, ru, zh_CN (Simplified
Chinese), hy, de, es, it and fr. English is the source language and the default,
independent of the macOS locale. Store an explicitly selected language atomically;
custom --catalog sessions use isolated preferences next to that catalog.

Translate presentation strings, accessibility labels and application statuses;
never translate serialized IDs, parameter keys, user filenames, collection names,
user preset names, or values passed to processing APIs. Source/controller messages
remain stable English; translation bindings also handle formatted %1/%2 messages,
retaining values verbatim. Native OS dialogs/errors can retain macOS-owned text.

A language change installs a validated translator, updates QLocale and retranslates
QQmlEngine without rebuilding the editor, changing edits, cancelling jobs or
resetting selection. Persist only a valid language. Invalid/corrupt preferences
fall back to English with a visible error, without overwriting the invalid file.
Validate all catalogs, identical source-key coverage and placeholder integrity;
exercise all languages, actual QML retranslation/accessibility, settings restart,
write failure and existing photo workflows. Packages ship in the app; no network
translation dependency or new build-time translation service.

This changes presentation/preferences only. Ordinary image import is a separate
0.4.12 step. Phase 0.5 remains on hold.
