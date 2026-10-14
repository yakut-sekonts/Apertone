import Foundation
import ImageIO
import CoreGraphics
let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let entries = [("portrait.png", "portrait.png"), ("landscape-original.jpg", "landscape.png"), ("night-original.jpg", "night.png")]
try FileManager.default.createDirectory(at: root.appendingPathComponent("tests/assets/filters"), withIntermediateDirectories: true)
for (input, output) in entries {
    let sourceURL = root.appendingPathComponent(".tools/filter-fixtures/" + input)
    guard let source = CGImageSourceCreateWithURL(sourceURL as CFURL, nil),
          let image = CGImageSourceCreateThumbnailAtIndex(source, 0, [kCGImageSourceCreateThumbnailFromImageAlways: true, kCGImageSourceCreateThumbnailWithTransform: true, kCGImageSourceThumbnailMaxPixelSize: 512] as CFDictionary),
          let target = CGImageDestinationCreateWithURL(root.appendingPathComponent("tests/assets/filters/" + output) as CFURL, "public.png" as CFString, 1, nil) else { fatalError("Fixture conversion failed") }
    CGImageDestinationAddImage(target, image, [:] as CFDictionary)
    guard CGImageDestinationFinalize(target) else { fatalError("Fixture writing failed") }
    print(output, image.width, image.height)
}
