#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "Pillow>=10.0.0",
# ]
# ///

from __future__ import annotations

import argparse
import math
import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


WIDTH = 32
HEIGHT = 32


@dataclass(frozen=True)
class CursorVariant:
    suffix: str
    bits_per_pixel: int
    group_id: int
    palette_kind: str
    grayscale_levels: int | None


VARIANTS = (
    CursorVariant("32bit", 32, 0x20, "rgba", None),
    CursorVariant("8bit", 8, 0x08, "color", None),
    CursorVariant("4bit", 4, 0x04, "gray", 16),
    CursorVariant("1bit", 1, 0x01, "gray", 2),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert one 32x32 RGBA PNG into SC4 cursor .cur variants: "
            "32-bit RGBA, 8-bit color, 4-bit grayscale, and 1-bit grayscale."
        )
    )
    parser.add_argument("png", type=Path, help="Input 32x32 32-bit RGBA PNG.")
    parser.add_argument(
        "-o",
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory. Defaults to the input PNG directory.",
    )
    parser.add_argument(
        "--stem",
        default=None,
        help="Output filename stem. Defaults to the input PNG stem.",
    )
    parser.add_argument(
        "--hotspot-x",
        type=int,
        default=0,
        help="Cursor hotspot X coordinate, 0-31. Defaults to 0.",
    )
    parser.add_argument(
        "--hotspot-y",
        type=int,
        default=0,
        help="Cursor hotspot Y coordinate, 0-31. Defaults to 0.",
    )
    parser.add_argument(
        "--alpha-cutoff",
        type=int,
        default=128,
        help=(
            "Alpha value below which pixels are transparent in the 1/4/8-bit "
            "AND masks. Defaults to 128."
        ),
    )
    parser.add_argument(
        "--brightness",
        type=float,
        default=1.0,
        help="Multiply RGB values before writing cursor bitmaps. Defaults to 1.0.",
    )
    parser.add_argument(
        "--gamma",
        type=float,
        default=1.0,
        help="Apply RGB gamma correction before writing cursor bitmaps. Defaults to 1.0.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Do not print generated file paths.",
    )
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if not 0 <= args.hotspot_x < WIDTH:
        raise SystemExit("--hotspot-x must be in the range 0-31.")
    if not 0 <= args.hotspot_y < HEIGHT:
        raise SystemExit("--hotspot-y must be in the range 0-31.")
    if not 0 <= args.alpha_cutoff <= 255:
        raise SystemExit("--alpha-cutoff must be in the range 0-255.")
    if args.brightness <= 0:
        raise SystemExit("--brightness must be greater than 0.")
    if args.gamma <= 0:
        raise SystemExit("--gamma must be greater than 0.")


def load_rgba_png(path: Path) -> Image.Image:
    with Image.open(path) as image:
        if image.size != (WIDTH, HEIGHT):
            raise SystemExit(f"{path} must be exactly {WIDTH}x{HEIGHT}; got {image.size}.")
        if image.mode != "RGBA":
            raise SystemExit(f"{path} must be a 32-bit RGBA PNG; got mode {image.mode}.")
        return image.copy()


def adjust_rgb(image: Image.Image, brightness: float, gamma: float) -> Image.Image:
    if brightness == 1.0 and gamma == 1.0:
        return image

    adjusted = Image.new("RGBA", image.size)
    src = image.load()
    dst = adjusted.load()

    for y in range(HEIGHT):
        for x in range(WIDTH):
            red, green, blue, alpha = src[x, y]
            dst[x, y] = (
                adjust_channel(red, brightness, gamma),
                adjust_channel(green, brightness, gamma),
                adjust_channel(blue, brightness, gamma),
                alpha,
            )

    return adjusted


def adjust_channel(value: int, brightness: float, gamma: float) -> int:
    normalized = max(0.0, min(1.0, value / 255.0))
    corrected = math.pow(normalized, gamma) * brightness
    return max(0, min(255, round(corrected * 255)))


def rgba_to_luma(red: int, green: int, blue: int) -> int:
    return round((red * 299 + green * 587 + blue * 114) / 1000)


def quantize_gray(value: int, levels: int) -> int:
    if levels == 2:
        return 1 if value >= 128 else 0
    return round(value * (levels - 1) / 255)


def make_gray_palette(levels: int) -> bytes:
    entries = bytearray()
    for index in range(levels):
        value = round(index * 255 / (levels - 1))
        entries.extend((value, value, value, 0))
    return bytes(entries)


def make_8bit_color_palette_and_indices(image: Image.Image, alpha_cutoff: int) -> tuple[bytes, bytes]:
    rgb = Image.new("RGB", image.size)
    src = image.load()
    dst = rgb.load()

    for y in range(HEIGHT):
        for x in range(WIDTH):
            red, green, blue, alpha = src[x, y]
            dst[x, y] = (red, green, blue) if alpha >= alpha_cutoff else (0, 0, 0)

    quantized = rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    raw_palette = quantized.getpalette("RGB") or []
    palette = bytearray()
    for index in range(256):
        offset = index * 3
        if offset + 2 < len(raw_palette):
            red, green, blue = raw_palette[offset : offset + 3]
        else:
            red = green = blue = 0
        palette.extend((blue, green, red, 0))

    return bytes(palette), bytes(quantized.tobytes())


def mask_stride_bytes(width: int) -> int:
    return ((width + 31) // 32) * 4


def bitmap_stride_bytes(width: int, bits_per_pixel: int) -> int:
    return ((width * bits_per_pixel + 31) // 32) * 4


def pack_and_mask(image: Image.Image, alpha_cutoff: int) -> bytes:
    pixels = image.load()
    stride = mask_stride_bytes(WIDTH)
    rows = bytearray()

    for y in range(HEIGHT - 1, -1, -1):
        row = bytearray(stride)
        for x in range(WIDTH):
            alpha = pixels[x, y][3]
            if alpha < alpha_cutoff:
                row[x // 8] |= 0x80 >> (x % 8)
        rows.extend(row)

    return bytes(rows)


def pack_32bit_xor(image: Image.Image) -> bytes:
    pixels = image.load()
    rows = bytearray()

    for y in range(HEIGHT - 1, -1, -1):
        for x in range(WIDTH):
            red, green, blue, alpha = pixels[x, y]
            rows.extend((blue, green, red, alpha))

    return bytes(rows)


def pack_8bit_color_xor(indices: bytes) -> bytes:
    stride = bitmap_stride_bytes(WIDTH, 8)
    rows = bytearray()

    for y in range(HEIGHT - 1, -1, -1):
        start = y * WIDTH
        row = bytearray(stride)
        row[:WIDTH] = indices[start : start + WIDTH]
        rows.extend(row)

    return bytes(rows)


def pack_gray_xor(image: Image.Image, bits_per_pixel: int, levels: int, alpha_cutoff: int) -> bytes:
    pixels = image.load()
    stride = bitmap_stride_bytes(WIDTH, bits_per_pixel)
    rows = bytearray()

    for y in range(HEIGHT - 1, -1, -1):
        row = bytearray(stride)
        bit_offset = 0
        for x in range(WIDTH):
            red, green, blue, alpha = pixels[x, y]
            if alpha < alpha_cutoff:
                index = 0
            else:
                index = quantize_gray(rgba_to_luma(red, green, blue), levels)

            if bits_per_pixel == 8:
                row[x] = index
            elif bits_per_pixel == 4:
                if x % 2 == 0:
                    row[x // 2] |= index << 4
                else:
                    row[x // 2] |= index
            elif bits_per_pixel == 1:
                if index:
                    row[bit_offset // 8] |= 0x80 >> (bit_offset % 8)
                bit_offset += 1
            else:
                raise ValueError(f"Unsupported indexed bit depth: {bits_per_pixel}")

        rows.extend(row)

    return bytes(rows)


def build_cur(
    image: Image.Image,
    variant: CursorVariant,
    hotspot_x: int,
    hotspot_y: int,
    alpha_cutoff: int,
) -> bytes:
    palette = b""
    if variant.palette_kind == "rgba":
        xor_bitmap = pack_32bit_xor(image)
        color_count = 0
    elif variant.palette_kind == "color":
        palette, indices = make_8bit_color_palette_and_indices(image, alpha_cutoff)
        xor_bitmap = pack_8bit_color_xor(indices)
        color_count = 0
    else:
        assert variant.grayscale_levels is not None
        palette = make_gray_palette(variant.grayscale_levels)
        xor_bitmap = pack_gray_xor(
            image,
            variant.bits_per_pixel,
            variant.grayscale_levels,
            alpha_cutoff,
        )
        color_count = 0 if variant.grayscale_levels >= 256 else variant.grayscale_levels

    and_mask = pack_and_mask(image, alpha_cutoff)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40,  # biSize
        WIDTH,
        HEIGHT * 2,  # cursor DIB includes XOR bitmap plus AND mask
        1,  # biPlanes
        variant.bits_per_pixel,
        0,  # BI_RGB
        len(xor_bitmap) + len(and_mask),
        0,
        0,
        0,
        0,
    )
    image_data = info_header + palette + xor_bitmap + and_mask

    icon_dir = struct.pack("<HHH", 0, 2, 1)
    icon_dir_entry = struct.pack(
        "<BBBBHHII",
        WIDTH,
        HEIGHT,
        color_count,
        0,
        hotspot_x,
        hotspot_y,
        len(image_data),
        len(icon_dir) + 16,
    )
    return icon_dir + icon_dir_entry + image_data


def output_path(out_dir: Path, stem: str, variant: CursorVariant) -> Path:
    return out_dir / f"{stem}.{variant.suffix}.group-{variant.group_id:08x}.cur"


def main() -> int:
    args = parse_args()
    validate_args(args)

    image = load_rgba_png(args.png)
    image = adjust_rgb(image, args.brightness, args.gamma)
    out_dir = args.out_dir if args.out_dir is not None else args.png.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    stem = args.stem if args.stem is not None else args.png.stem
    written: list[Path] = []
    for variant in VARIANTS:
        path = output_path(out_dir, stem, variant)
        path.write_bytes(
            build_cur(
                image=image,
                variant=variant,
                hotspot_x=args.hotspot_x,
                hotspot_y=args.hotspot_y,
                alpha_cutoff=args.alpha_cutoff,
            )
        )
        written.append(path)

    if not args.quiet:
        for path in written:
            print(path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
