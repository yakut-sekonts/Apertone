# Changelog

## Unreleased — 0.1.0 / M0

- Initial C++20 / Qt 6 / QML macOS Apple Silicon application.
- Real integer Bayer RAW decode and independent Float32 CPU pipeline.
- Exposure, As Shot white balance, camera color matrix and ColorSync/LittleCMS display output.
- SQLite catalog, durable autosave, selected-photo and exposure restoration.
- CLI, generated public DNG tests, optional Nikon D7500 acceptance and macOS CI.

Known limitations: this milestone is not the first daily-use version. Bayer only;
bilinear demosaic; Fit viewer; one current photo; no export, manual WB, undo, masks,
preset filters, library grid or Metal pixel processing. Missing and modified
originals retain their edits but need manual source restoration.
