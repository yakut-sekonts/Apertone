# Apertone icon

Original Apache-2.0 artwork: a six-blade aperture split between the cool Day
palette and warm Night palette on a graphite macOS tile. The canonical vector
drawing is `scripts/generate-icons.swift`; it produces the ten standard macOS
16–1024 px icon representations. No fonts, external artwork, AI-generated
assets or third-party design files are used.

Regenerate on macOS:

```sh
swift scripts/generate-icons.swift /tmp/Apertone.iconset
iconutil -c icns /tmp/Apertone.iconset -o resources/branding/Apertone.icns
cp /tmp/Apertone.iconset/icon_512x512@2x.png resources/branding/Apertone-1024.png
```

The app embeds Apertone.icns through CFBundleIconFile. DMG packaging also embeds the
same artwork as .VolumeIcon.icns, with the custom volume-icon Finder flag.
