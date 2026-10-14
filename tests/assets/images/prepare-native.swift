// Original Luma pixel fixture; codec bytes can differ between macOS encoder versions.
import Foundation
import ImageIO
import CoreGraphics
let folder = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let width = 64, height = 48
var rgba = [UInt8](repeating: 255, count: width * height * 4)
let colors: [[UInt8]] = [[255,0,0,255], [0,255,0,255], [0,0,255,255], [128,128,128,255]]
for y in 0..<height { for x in 0..<width {
 let c = colors[(y >= height/2 ? 2 : 0) + (x >= width/2 ? 1 : 0)]
 for i in 0..<4 { rgba[(y*width+x)*4+i] = c[i] }
} }
let data = Data(rgba)
let provider = CGDataProvider(data: data as CFData)!
let color = CGColorSpace(name: CGColorSpace.sRGB)!
let image = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width*4, space: color, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue), provider: provider, decode: nil, shouldInterpolate: false, intent: .relativeColorimetric)!
for (name, type) in [("opaque.heic","public.heic")] {
 let url = folder.appendingPathComponent(name)
 guard let output = CGImageDestinationCreateWithURL(url as CFURL, type as CFString, 1, nil) else { fatalError("Missing native encoder") }
 CGImageDestinationAddImage(output, image, [kCGImageDestinationLossyCompressionQuality: 1, kCGImagePropertyOrientation: 1] as CFDictionary)
 assert(CGImageDestinationFinalize(output))
 print(name, (try! Data(contentsOf: url)).count)
}
