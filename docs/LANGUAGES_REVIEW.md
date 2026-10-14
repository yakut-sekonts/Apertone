# English-first language packages with runtime switching

The UI previously mixed Russian and English without a language preference. Settings
now switches between English (new-install default), Russian, Simplified Chinese,
Armenian, German, Spanish, Italian and French without restarting or changing edits.
Embedded versioned catalogs cover UI captions, accessibility, standard buttons and
application messages. Preferences persist atomically and fall back safely to English.

Presentation strings use English source keys. Status templates retain their values
separately so language changes also update existing messages while keeping filenames
and '%' characters intact. White-balance controls display translated labels but pass
stable English processing values; user presets/collections and model IDs remain data.
Core lens status text is normalized to English and localized in the desktop adapter.

Validation includes matching catalog coverage/placeholders/file tokens, all eight
language choices through actual QML, native Main/Settings retranslation, English
default independent of OS locale, restart, invalid settings and write failures.
Existing photo workflows, private Nikon RAW acceptance, format/tidy and standalone
packaging gate integration, followed by exact-head macOS CI.

0.4.11 is a human-requested extension before phase 0.5. Ordinary image import is a
separate 0.4.12 change. No Local Adjustments work, new image algorithms or public
signed release is included. See ADR 0019 and docs/LOCALIZATION.md.
