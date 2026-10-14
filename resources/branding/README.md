# Luma icon

Original Apache-2.0 artwork based on Luma's existing gold half-disc logo.
The canonical vector drawing is `scripts/generate-icons.swift`; it produces the
ten standard macOS 16–1024 px icon representations. No fonts, external artwork,
AI-generated assets or third-party design files are used.

Regenerate on macOS:

```sh
swift scripts/generate-icons.swift /tmp/Luma.iconset
iconutil -c icns /tmp/Luma.iconset -o resources/branding/Luma.icns
cp /tmp/Luma.iconset/icon_512x512@2x.png resources/branding/Luma-1024.png
```

The app embeds Luma.icns through CFBundleIconFile. DMG packaging also embeds the
same artwork as .VolumeIcon.icns, with the custom volume-icon Finder flag.
