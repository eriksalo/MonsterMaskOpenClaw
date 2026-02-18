#!/usr/bin/env python3
"""
Generate mood-specific eyelid BMPs for Monster M4SK animated eyes.

Each eyelid is a 240x240 1-bit monochrome BMP where white pixels indicate
the "open" region of the eye. The M4SK firmware scans each column to find
the min/max white pixel, creating the eyelid contour.

The screen uses ROTATION 3 (90 deg CCW), so:
  - BMP X axis = screen columns (left to right as viewed)
  - BMP Y axis = screen rows (top = 0 in BMP = bottom of eye as viewed)

Upper eyelid: white region below the lid line (eye is open below the lid)
Lower eyelid: white region above the lid line (eye is open above the lid)

Usage:
    python generate_eyelids.py [output_dir]

Output dir defaults to ../M4_Eyes/eyes/moods/
"""

import os
import sys
import struct
import math

SIZE = 240  # Display size
CENTER = SIZE // 2


def create_1bit_bmp(pixels, width, height):
    """Create a 1-bit BMP file from a 2D pixel array.
    pixels[y][x] = 0 (black) or 1 (white).
    BMP stores rows bottom-to-top.
    """
    row_bytes = (width + 31) // 32 * 4  # Rows padded to 4-byte boundary
    pixel_data_size = row_bytes * height
    header_size = 14 + 40 + 8  # File header + info header + 2-color palette

    data = bytearray()

    # BMP File Header (14 bytes)
    data += struct.pack('<2sIHHI', b'BM', header_size + pixel_data_size, 0, 0, header_size)

    # BMP Info Header (40 bytes)
    data += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 1, 0,
                        pixel_data_size, 2835, 2835, 2, 2)

    # Color palette: index 0 = black, index 1 = white
    data += struct.pack('<BBBB', 0, 0, 0, 0)      # Black
    data += struct.pack('<BBBB', 255, 255, 255, 0)  # White

    # Pixel data (bottom-to-top row order)
    for y in range(height - 1, -1, -1):
        row = bytearray(row_bytes)
        for x in range(width):
            if pixels[y][x]:
                row[x // 8] |= (0x80 >> (x % 8))
        data += row

    return bytes(data)


def make_upper_eyelid(shape_func):
    """Generate upper eyelid. White below the lid curve = eye open region."""
    pixels = [[0] * SIZE for _ in range(SIZE)]
    for x in range(SIZE):
        lid_y = shape_func(x)
        lid_y = max(0, min(SIZE - 1, int(lid_y)))
        # White from lid_y down to bottom (bottom = y=0 in screen)
        for y in range(lid_y, SIZE):
            pixels[y][x] = 1
    return pixels


def make_lower_eyelid(shape_func):
    """Generate lower eyelid. White above the lid curve = eye open region."""
    pixels = [[0] * SIZE for _ in range(SIZE)]
    for x in range(SIZE):
        lid_y = shape_func(x)
        lid_y = max(0, min(SIZE - 1, int(lid_y)))
        # White from top (y=0) down to lid_y
        for y in range(0, lid_y + 1):
            pixels[y][x] = 1
    return pixels


# --- Eyelid shape functions ---
# Each returns the Y coordinate of the lid edge for a given X.
# Y=0 is top of BMP image, Y=239 is bottom.
# For upper eyelids: lower Y = more closed (lid descends from top)
# For lower eyelids: higher Y = more closed (lid rises from bottom)

def happy_upper(x):
    """Wide arc, slightly raised center — friendly open look."""
    t = (x - CENTER) / CENTER  # -1 to 1
    return 30 + 20 * (t * t)  # Shallow U-shape, mostly open


def happy_lower(x):
    """Slight upward curve (smile line)."""
    t = (x - CENTER) / CENTER
    return 210 + 15 * (1 - t * t)  # Gentle upward arc at bottom


def angry_upper(x):
    """V-shape pointing down, steep from corners — menacing."""
    t = (x - CENTER) / CENTER
    # Asymmetric V: inner corner dips lower
    return 80 + 50 * abs(t) - 20 * t  # V shape, shifted for asymmetry


def angry_lower(x):
    """Pushed up, narrow slit."""
    t = (x - CENTER) / CENTER
    return 170 - 30 * (1 - t * t)  # Pushed up in center


def sad_upper(x):
    """Drooping outer corners."""
    t = (x - CENTER) / CENTER
    # Higher in center, droops at edges
    return 50 + 40 * (t * t) + 15 * abs(t)


def sad_lower(x):
    """Slightly raised center."""
    t = (x - CENTER) / CENTER
    return 210 - 10 * (1 - t * t)


def scared_upper(x):
    """Very high, nearly full circle open — wide with fear."""
    t = (x - CENTER) / CENTER
    return 15 + 10 * (t * t)  # Very high, almost fully open


def scared_lower(x):
    """Very low, nearly full circle open."""
    t = (x - CENTER) / CENTER
    return 225 - 10 * (t * t)  # Very low


def sleepy_upper(x):
    """Heavy droop, covers top 55%."""
    t = (x - CENTER) / CENTER
    return 130 + 15 * (t * t)  # Droops way down


def sleepy_lower(x):
    """Slightly raised."""
    t = (x - CENTER) / CENTER
    return 200 + 10 * (1 - t * t)


def suspicious_upper(x):
    """Flat, half-closed from top."""
    t = (x - CENTER) / CENTER
    return 110 + 10 * (t * t)  # Flat and low


def suspicious_lower(x):
    """Flat, half-closed from bottom."""
    t = (x - CENTER) / CENTER
    return 150 - 10 * (1 - t * t)  # Flat and high


def surprised_upper(x):
    """Very wide open, rounder than scared."""
    t = (x - CENTER) / CENTER
    return 10 + 15 * (t * t)  # Very high


def surprised_lower(x):
    """Very wide open."""
    t = (x - CENTER) / CENTER
    return 230 - 15 * (t * t)  # Very low


def love_upper(x):
    """Wide happy shape."""
    return happy_upper(x)


def love_lower(x):
    """Slight upward curve."""
    return happy_lower(x)


# --- Mood definitions ---

MOODS = {
    'happy': {
        'upper': happy_upper,
        'lower': happy_lower,
    },
    'angry': {
        'upper': angry_upper,
        'lower': angry_lower,
    },
    'sad': {
        'upper': sad_upper,
        'lower': sad_lower,
    },
    'scared': {
        'upper': scared_upper,
        'lower': scared_lower,
    },
    'sleepy': {
        'upper': sleepy_upper,
        'lower': sleepy_lower,
    },
    'suspicious': {
        'upper': suspicious_upper,
        'lower': suspicious_lower,
    },
    'surprised': {
        'upper': surprised_upper,
        'lower': surprised_lower,
    },
    'love': {
        'upper': love_upper,
        'lower': love_lower,
    },
    # crazy reuses doom-spiral eyelids, no generation needed
}


def main():
    output_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        '..', 'M4_Eyes', 'eyes', 'moods')

    for mood_name, shapes in MOODS.items():
        mood_dir = os.path.join(output_dir, mood_name)
        os.makedirs(mood_dir, exist_ok=True)

        # Generate upper eyelid
        upper_pixels = make_upper_eyelid(shapes['upper'])
        upper_bmp = create_1bit_bmp(upper_pixels, SIZE, SIZE)
        upper_path = os.path.join(mood_dir, 'upper.bmp')
        with open(upper_path, 'wb') as f:
            f.write(upper_bmp)
        print(f"  {upper_path} ({len(upper_bmp)} bytes)")

        # Generate lower eyelid
        lower_pixels = make_lower_eyelid(shapes['lower'])
        lower_bmp = create_1bit_bmp(lower_pixels, SIZE, SIZE)
        lower_path = os.path.join(mood_dir, 'lower.bmp')
        with open(lower_path, 'wb') as f:
            f.write(lower_bmp)
        print(f"  {lower_path} ({len(lower_bmp)} bytes)")

    print(f"\nGenerated eyelids for {len(MOODS)} moods.")
    print("Note: 'crazy' mood reuses doom-spiral eyelids (no generation needed).")


if __name__ == '__main__':
    main()
