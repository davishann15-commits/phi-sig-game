import AppKit
import Foundation

guard CommandLine.arguments.count == 3 else {
    fatalError("Usage: make_runner_video_contact_sheet.swift frames-directory output.png")
}
let framesURL = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])
let cellWidth = 270
let cellHeight = 480
guard let bitmap = NSBitmapImageRep(
    bitmapDataPlanes: nil, pixelsWide: cellWidth * 4, pixelsHigh: cellHeight * 4,
    bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
    colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
    let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
    fatalError("Could not make contact sheet")
}
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = context
NSColor.black.setFill()
NSRect(x: 0, y: 0, width: cellWidth * 4, height: cellHeight * 4).fill()
for index in 0..<16 {
    let file = framesURL.appendingPathComponent(String(format: "frame-%02d.png", index))
    guard let image = NSImage(contentsOf: file) else { fatalError("Missing \(file.path)") }
    let x = (index % 4) * cellWidth
    let y = (3 - index / 4) * cellHeight
    image.draw(in: NSRect(x: x, y: y, width: cellWidth, height: cellHeight),
               from: .zero, operation: .copy, fraction: 1)
    let label = "\(index)s" as NSString
    label.draw(at: NSPoint(x: x + 8, y: y + cellHeight - 34), withAttributes: [
        .font: NSFont.boldSystemFont(ofSize: 23), .foregroundColor: NSColor.white,
        .backgroundColor: NSColor.black.withAlphaComponent(0.6)])
}
context.flushGraphics()
NSGraphicsContext.restoreGraphicsState()
guard let png = bitmap.representation(using: .png, properties: [:]) else {
    fatalError("Could not encode contact sheet")
}
try png.write(to: outputURL)
print(outputURL.path)
