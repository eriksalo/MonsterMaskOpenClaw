#!/usr/bin/env python3
"""
Generate the custom love iris texture for Monster M4SK.

Creates a 16-bit 565 BMP iris texture with pink-to-deep-red radial gradient
and subtle heart-shaped highlight pattern. The texture is polar-mapped:
  X axis = angle (0 to width = 0 to 360 degrees)
  Y axis = radius (0 = outer edge of iris, height-1 = near pupil)

Usage:
    python generate_love_iris.py [output_path]

Output defaults to ../M4_Eyes/eyes/moods/love/iris.bmp
"""

import os
import sys
import struct
import math


def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit 565 format."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def create_16bit_bmp(pixels, width, height):
    """Create a 16-bit 565 BMP file.
    pixels[y][x] = (r, g, b) tuples with 8-bit values.
    BMP stores rows bottom-to-top.
    """
    row_bytes = width * 2  # 2 bytes per pixel
    # Pad to 4-byte boundary
    row_padding = (4 - (row_bytes % 4)) % 4
    padded_row_bytes = row_bytes + row_padding
    pixel_data_size = padded_row_bytes * height

    # Using BITMAPINFOHEADER with BI_BITFIELDS for 565
    header_size = 14 + 40 + 12  # File header + info header + 3 color masks

    data = bytearray()

    # BMP File Header (14 bytes)
    data += struct.pack('<2sIHHI', b'BM', header_size + pixel_data_size, 0, 0, header_size)

    # BMP Info Header (40 bytes) with BI_BITFIELDS compression
    data += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 16, 3,
                        pixel_data_size, 2835, 2835, 0, 0)

    # Color masks for RGB565
    data += struct.pack('<III', 0xF800, 0x07E0, 0x001F)

    # Pixel data (bottom-to-top)
    for y in range(height - 1, -1, -1):
        for x in range(width):
            r, g, b = pixels[y][x]
            rgb565 = rgb888_to_rgb565(r, g, b)
            data += struct.pack('<H', rgb565)
        data += b'\x00' * row_padding

    return bytes(data)


def heart_shape(angle, scale=1.0):
    """Return the radius of a heart shape at given angle (radians).
    Used for subtle highlight pattern."""
    # Parametric heart: r = 1 - sin(t)
    t = angle
    r = (1 - math.sin(t)) * scale
    return r


def generate_love_iris(width=128, height=128):
    """Generate a pink/red gradient iris with heart highlights."""
    pixels = [[None] * width for _ in range(height)]

    for y in range(height):
        # Y=0 is outer iris edge, Y=height-1 is near pupil
        radius_frac = y / (height - 1)  # 0 = outer, 1 = inner (near pupil)

        for x in range(width):
            angle_frac = x / width  # 0 to 1 around the iris
            angle_rad = angle_frac * 2 * math.pi

            # Base color: pink outer to deep red inner
            if radius_frac < 0.5:
                # Outer half: pink to medium red
                t = radius_frac * 2  # 0 to 1
                r = int(255 - 40 * t)
                g = int(120 - 100 * t)
                b = int(150 - 120 * t)
            else:
                # Inner half: medium red to deep crimson
                t = (radius_frac - 0.5) * 2  # 0 to 1
                r = int(215 - 80 * t)
                g = int(20 - 15 * t)
                b = int(30 - 20 * t)

            # Heart-shaped highlight pattern
            heart_r = heart_shape(angle_rad, 0.3)
            heart_dist = abs(radius_frac - 0.3 - heart_r * 0.2)
            if heart_dist < 0.08:
                highlight = (0.08 - heart_dist) / 0.08
                r = min(255, int(r + 40 * highlight))
                g = min(255, int(g + 20 * highlight))
                b = min(255, int(b + 30 * highlight))

            # Radial streaks for organic texture
            streak = math.sin(angle_rad * 12) * 0.5 + 0.5  # 0 to 1
            streak *= math.sin(angle_rad * 7 + 1.5) * 0.3 + 0.7
            r = max(0, min(255, int(r + (streak - 0.5) * 30)))
            g = max(0, min(255, int(g + (streak - 0.5) * 10)))

            # Subtle sparkle at certain angles
            sparkle = max(0, math.sin(angle_rad * 5) * math.sin(radius_frac * math.pi * 3))
            r = min(255, int(r + sparkle * 25))
            g = min(255, int(g + sparkle * 15))
            b = min(255, int(b + sparkle * 20))

            pixels[y][x] = (max(0, min(255, r)),
                            max(0, min(255, g)),
                            max(0, min(255, b)))

    return pixels


def main():
    output_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        '..', 'M4_Eyes', 'eyes', 'moods', 'love', 'iris.bmp')

    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    print("Generating love iris texture (128x128 16-bit 565 BMP)...")
    pixels = generate_love_iris(128, 128)
    bmp_data = create_16bit_bmp(pixels, 128, 128)

    with open(output_path, 'wb') as f:
        f.write(bmp_data)

    print(f"Saved: {output_path} ({len(bmp_data)} bytes)")


if __name__ == '__main__':
    main()
