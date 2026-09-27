import AppKit
import AVFoundation
import Foundation

guard CommandLine.arguments.count == 3 else {
    fatalError("Usage: extract_runner_video_frames.swift movie output-directory")
}
let movieURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
try FileManager.default.createDirectory(at: outputURL, withIntermediateDirectories: true)

let asset = AVURLAsset(url: movieURL)
let generator = AVAssetImageGenerator(asset: asset)
generator.appliesPreferredTrackTransform = true
generator.requestedTimeToleranceBefore = .zero
generator.requestedTimeToleranceAfter = .zero

for frameIndex in 0...15 {
    let seconds = Double(frameIndex) + 0.1
    let time = CMTime(seconds: seconds, preferredTimescale: 600)
    let cgImage = try generator.copyCGImage(at: time, actualTime: nil)
    let bitmap = NSBitmapImageRep(cgImage: cgImage)
    guard let png = bitmap.representation(using: .png, properties: [:]) else {
        fatalError("Could not encode frame \(frameIndex)")
    }
    let file = outputURL.appendingPathComponent(String(format: "frame-%02d.png", frameIndex))
    try png.write(to: file)
    print("\(seconds)s \(cgImage.width)x\(cgImage.height) \(file.path)")
}
