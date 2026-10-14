# Language packages — 0.4.11

Open Settings (gear) and choose the interface language. Available packages are
English (default), Russian, Simplified Chinese, Armenian, German, Spanish, Italian
and French. A new installation uses English independently of the macOS language.
An explicit choice is saved in `language.json` next to the catalog and takes effect
without restarting the application. Custom `--catalog` sessions keep isolated
preferences. Language changes do not reload photos or change edits/selection/jobs.

UTF-8, versioned JSON catalogs live in `resources/i18n` and are embedded in the
application. Qt's translator and QML retranslation update existing bindings and
standard buttons. English source strings are stable keys. `LocalizedText` retains
status templates and arguments separately, so visible messages can switch language
without translating filenames or interpreting '%' characters inside user data.
Presets, collection names, camera/lens database identities, format names and
processing parameter IDs are data, not translated keys. White-balance labels map
to stable processing values independently of the displayed language.

Preferences use a lock and atomic replacement. Invalid preferences fall back to
English and expose an error while preserving the original file; an explicit valid
selection can repair them. Unsupported language selection or write failure retains
the current language. Processing/platform diagnostic details keep their original
text when no translated entry exists. macOS-owned dialogs and messages may follow
the operating system language. Numeric processing/serialization remains unchanged.

Catalog changes must preserve source-key coverage, %1/%2 placeholders, filename
tokens and file-filter patterns. `scripts/check-translations.py`, runtime QML tests,
and native Main/Settings checks cover all eight languages, persistence, invalid
settings, write failures and verbatim values. New messages should be added to every
catalog in the same change. No translation-service dependency or network request.

This is a pre-phase-0.5 extension. Ordinary image support follows separately in
0.4.12; Local Adjustments remain on hold.
