//
//  Copyright (C) 2026  Andreas Hanft
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

// The app icon: the logo of data/vertigo.svg on black, 1024 pixels without alpha, as iOS wants it.
// swift ios/make-icon.swift data/vertigo.svg ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png

import AppKit
let args = CommandLine.arguments
guard let logo = NSImage(contentsOfFile: args[1]) else { fatalError("cannot read \(args[1])") }
let size = 1024
let space = CGColorSpace(name: CGColorSpace.sRGB)!
let context = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0, space: space,
                        bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)!
context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
context.fill(CGRect(x: 0, y: 0, width: size, height: size))
NSGraphicsContext.current = NSGraphicsContext(cgContext: context, flipped: false)
let side = CGFloat(size)*0.86
logo.draw(in: NSRect(x: (CGFloat(size) - side)/2, y: (CGFloat(size) - side)/2, width: side, height: side))
let image = context.makeImage()!
let rep = NSBitmapImageRep(cgImage: image)
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: args[2]))
