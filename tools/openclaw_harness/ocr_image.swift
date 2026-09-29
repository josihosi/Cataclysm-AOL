#!/usr/bin/env swift

import AppKit
import Foundation
import Vision

struct UsageError: Error {
    let message: String
}

func parseArgs() throws -> (URL, CGRect?) {
    let args = Array(CommandLine.arguments.dropFirst())
    guard (args.count == 2 || args.count == 7), args[0] == "--image" else {
        throw UsageError(message: "usage: ocr_image.swift --image <png-path> [--region-of-interest x y width height]")
    }
    if args.count == 2 {
        return (URL(fileURLWithPath: args[1]), nil)
    }
    guard args[2] == "--region-of-interest",
          let x = Double(args[3]), let y = Double(args[4]),
          let width = Double(args[5]), let height = Double(args[6]),
          x.isFinite, y.isFinite, width.isFinite, height.isFinite,
          x >= 0, y >= 0, width > 0, height > 0,
          x + width <= 1, y + height <= 1 else {
        throw UsageError(message: "region of interest must fit inside normalized image bounds")
    }
    return (URL(fileURLWithPath: args[1]), CGRect(x: x, y: y, width: width, height: height))
}

func cgImage(from url: URL) throws -> CGImage {
    guard let image = NSImage(contentsOf: url) else {
        throw UsageError(message: "failed to load image: \(url.path)")
    }
    var rect = CGRect(origin: .zero, size: image.size)
    guard let cgImage = image.cgImage(forProposedRect: &rect, context: nil, hints: nil) else {
        throw UsageError(message: "failed to resolve CGImage: \(url.path)")
    }
    return cgImage
}

func recognizeText(in cgImage: CGImage, region: CGRect?) throws -> [[String: Any]] {
    let request = VNRecognizeTextRequest()
    request.recognitionLevel = .accurate
    if let region = region {
        request.regionOfInterest = region
    }
    request.usesLanguageCorrection = false
    request.recognitionLanguages = ["en_US"]
    let handler = VNImageRequestHandler(cgImage: cgImage, options: [:])
    try handler.perform([request])

    let observations = request.results ?? []
    return observations.compactMap { observation in
        guard let candidate = observation.topCandidates(1).first else {
            return nil
        }
        let box = observation.boundingBox
        return [
            "text": candidate.string,
            "x": Double(box.origin.x),
            "y": Double(box.origin.y),
            "w": Double(box.size.width),
            "h": Double(box.size.height),
        ]
    }.sorted { lhs, rhs in
        let ly = lhs["y"] as? Double ?? 0
        let ry = rhs["y"] as? Double ?? 0
        if abs(ly - ry) > 0.02 {
            return ly > ry
        }
        let lx = lhs["x"] as? Double ?? 0
        let rx = rhs["x"] as? Double ?? 0
        return lx < rx
    }
}

do {
    let (imageURL, region) = try parseArgs()
    let cgImage = try cgImage(from: imageURL)
    let observations = try recognizeText(in: cgImage, region: region)
    let lines = observations.compactMap { $0["text"] as? String }.filter { !$0.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }
    let payload: [String: Any] = [
        "ok": true,
        "image_path": imageURL.path,
        "line_count": lines.count,
        "lines": lines,
        "text": lines.joined(separator: "\n"),
        "observations": observations,
    ]
    let data = try JSONSerialization.data(withJSONObject: payload, options: [.prettyPrinted, .sortedKeys])
    FileHandle.standardOutput.write(data)
    FileHandle.standardOutput.write(Data("\n".utf8))
} catch let usage as UsageError {
    fputs(usage.message + "\n", stderr)
    exit(2)
} catch {
    fputs("ocr_image.swift failed: \(error)\n", stderr)
    exit(1)
}
