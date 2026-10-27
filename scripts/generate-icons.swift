// Original Apertone vector artwork: a split-tone camera aperture on a macOS tile.
// Run: swift scripts/generate-icons.swift /path/to/output.iconset
import Foundation
import CoreGraphics
import ImageIO

let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!

func rgb(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(srgbRed: red / 255, green: green / 255, blue: blue / 255, alpha: alpha)
}

func point(center: CGPoint, radius: CGFloat, angle: CGFloat) -> CGPoint {
    CGPoint(x: center.x + cos(angle) * radius, y: center.y + sin(angle) * radius)
}

for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale
        let context = CGContext(data: nil, width: pixels, height: pixels,
                                bitsPerComponent: 8, bytesPerRow: pixels * 4,
                                space: colorSpace,
                                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.clear(CGRect(x: 0, y: 0, width: pixels, height: pixels))
        context.scaleBy(x: CGFloat(pixels) / 1024, y: CGFloat(pixels) / 1024)

        let tile = CGPath(roundedRect: CGRect(x: 64, y: 64, width: 896, height: 896),
                          cornerWidth: 196, cornerHeight: 196, transform: nil)
        context.addPath(tile)
        context.clip()
        let background = CGGradient(colorsSpace: colorSpace,
                                    colors: [rgb(33, 39, 43), rgb(12, 15, 18)] as CFArray,
                                    locations: [0, 1])!
        context.drawLinearGradient(background,
                                   start: CGPoint(x: 180, y: 900),
                                   end: CGPoint(x: 850, y: 120),
                                   options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])

        context.setStrokeColor(rgb(255, 255, 255, 0.09))
        context.setLineWidth(10)
        context.addPath(CGPath(roundedRect: CGRect(x: 72, y: 72, width: 880, height: 880),
                               cornerWidth: 190, cornerHeight: 190, transform: nil))
        context.strokePath()

        let center = CGPoint(x: 512, y: 512)
        let outerRadius: CGFloat = 286
        let innerRadius: CGFloat = 112
        let teal = rgb(91, 175, 170)
        let copper = rgb(224, 159, 102)

        // Six broad blades form an iris. The cool and warm halves encode the
        // Day/Night appearances and the tonal range edited by Apertone.
        for index in 0..<6 {
            let base = CGFloat(index) * .pi / 3 - .pi / 6
            let path = CGMutablePath()
            path.move(to: point(center: center, radius: outerRadius, angle: base))
            path.addArc(center: center, radius: outerRadius,
                        startAngle: base, endAngle: base + .pi / 3 - 0.035,
                        clockwise: false)
            path.addLine(to: point(center: center, radius: innerRadius,
                                   angle: base + .pi / 3 + 0.34))
            path.addArc(center: center, radius: innerRadius,
                        startAngle: base + .pi / 3 + 0.34,
                        endAngle: base + 0.34, clockwise: true)
            path.closeSubpath()
            context.setFillColor(index < 3 ? teal : copper)
            context.addPath(path)
            context.fillPath()
        }

        context.setStrokeColor(rgb(255, 255, 255, 0.18))
        context.setLineWidth(9)
        context.strokeEllipse(in: CGRect(x: 226, y: 226, width: 572, height: 572))

        let opening = CGMutablePath()
        for index in 0..<6 {
            let p = point(center: center, radius: 112,
                          angle: CGFloat(index) * .pi / 3 + .pi / 6)
            index == 0 ? opening.move(to: p) : opening.addLine(to: p)
        }
        opening.closeSubpath()
        context.setFillColor(rgb(12, 15, 18))
        context.addPath(opening)
        context.fillPath()
        context.setStrokeColor(rgb(255, 255, 255, 0.28))
        context.setLineWidth(8)
        context.addPath(opening)
        context.strokePath()

        let suffix = scale == 2 ? "@2x" : ""
        let url = directory.appendingPathComponent("icon_\(size)x\(size)\(suffix).png")
        guard let image = context.makeImage(),
              let output = CGImageDestinationCreateWithURL(url as CFURL,
                                                            "public.png" as CFString, 1, nil)
        else { fatalError("Cannot create icon representation") }
        CGImageDestinationAddImage(output, image, nil)
        guard CGImageDestinationFinalize(output) else { fatalError("Cannot encode icon") }
    }
}
