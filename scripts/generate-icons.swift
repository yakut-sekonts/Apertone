// Original Luma vector artwork: the existing gold half-disc on a macOS icon tile.
// Run: swift scripts/generate-icons.swift /path/to/output.iconset
import Foundation
import CoreGraphics
import ImageIO

let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
let color = CGColorSpace(name: CGColorSpace.sRGB)!
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale
        let context = CGContext(data: nil, width: pixels, height: pixels,
                                bitsPerComponent: 8, bytesPerRow: pixels * 4, space: color,
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.scaleBy(x: CGFloat(pixels) / 1024, y: CGFloat(pixels) / 1024)
        context.setFillColor(CGColor(srgbRed: 25/255, green: 28/255, blue: 34/255, alpha: 1))
        context.addPath(CGPath(roundedRect: CGRect(x: 64, y: 64, width: 896, height: 896),
                               cornerWidth: 196, cornerHeight: 196, transform: nil))
        context.fillPath()
        let gold = CGColor(srgbRed: 223/255, green: 174/255, blue: 119/255, alpha: 1)
        context.setStrokeColor(gold)
        context.setLineWidth(16)
        context.strokeEllipse(in: CGRect(x: 246, y: 246, width: 532, height: 532))
        context.saveGState()
        context.clip(to: CGRect(x: 0, y: 0, width: 512, height: 1024))
        context.setFillColor(gold)
        context.fillEllipse(in: CGRect(x: 246, y: 246, width: 532, height: 532))
        context.restoreGState()
        let suffix = scale == 2 ? "@2x" : ""
        let url = directory.appendingPathComponent("icon_\(size)x\(size)\(suffix).png")
        guard let image = context.makeImage(),
              let output = CGImageDestinationCreateWithURL(url as CFURL, "public.png" as CFString, 1, nil)
        else { fatalError("Cannot create icon representation") }
        CGImageDestinationAddImage(output, image, nil)
        guard CGImageDestinationFinalize(output) else { fatalError("Cannot encode icon") }
    }
}
