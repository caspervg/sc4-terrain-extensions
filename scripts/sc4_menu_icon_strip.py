#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "Pillow>=10.0.0",
# ]
# ///

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageOps


FRAME_SIZE = 44
ICON_SIZE = 28
STATE_COUNT = 4


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a four-state SC4 menu icon strip from a transparent PNG."
    )
    parser.add_argument("source", type=Path, help="Source icon PNG, usually 32x32 RGBA.")
    parser.add_argument("output", type=Path, help="Output 176x44 PNG.")
    parser.add_argument("--frame-size", type=int, default=FRAME_SIZE)
    parser.add_argument("--icon-size", type=int, default=ICON_SIZE)
    parser.add_argument(
        "--hotspot-marker-size",
        type=int,
        default=9,
        help="Clear an upper-left triangular hotspot marker of this size before composing the strip. Use 0 to disable.",
    )
    parser.add_argument("--quiet", action="store_true")
    return parser.parse_args()


def load_icon(path: Path) -> Image.Image:
    with Image.open(path) as image:
        return image.convert("RGBA")


def fit_icon(icon: Image.Image, size: int) -> Image.Image:
    fitted = ImageOps.contain(icon, (size, size), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(fitted, ((size - fitted.width) // 2, (size - fitted.height) // 2))
    return center_visible_content(canvas)


def center_visible_content(icon: Image.Image) -> Image.Image:
    bbox = icon.getchannel("A").getbbox()
    if bbox is None:
        return icon

    left, top, right, bottom = bbox
    content_cx = (left + right) / 2.0
    content_cy = (top + bottom) / 2.0
    target_cx = icon.width / 2.0
    target_cy = icon.height / 2.0
    dx = round(target_cx - content_cx)
    dy = round(target_cy - content_cy)
    if dx == 0 and dy == 0:
        return icon

    shifted = Image.new("RGBA", icon.size, (0, 0, 0, 0))
    shifted.alpha_composite(icon, (dx, dy))
    return shifted


def remove_hotspot_marker(icon: Image.Image, marker_size: int) -> Image.Image:
    if marker_size <= 0:
        return icon

    cleaned = icon.copy()
    pixels = cleaned.load()
    limit = min(marker_size, cleaned.width, cleaned.height)
    for y in range(limit):
        for x in range(limit - y):
            pixels[x, y] = (0, 0, 0, 0)
    return cleaned


def rgba_adjust(icon: Image.Image, brightness: float, contrast: float, saturation: float) -> Image.Image:
    rgb = icon.convert("RGB")
    alpha = icon.getchannel("A")
    rgb = ImageEnhance.Brightness(rgb).enhance(brightness)
    rgb = ImageEnhance.Contrast(rgb).enhance(contrast)
    rgb = ImageEnhance.Color(rgb).enhance(saturation)
    out = rgb.convert("RGBA")
    out.putalpha(alpha)
    return out


def disabled_icon(icon: Image.Image) -> Image.Image:
    alpha = icon.getchannel("A")
    gray = ImageOps.grayscale(icon.convert("RGB"))
    gray = ImageEnhance.Brightness(gray).enhance(1.55)
    gray = ImageEnhance.Contrast(gray).enhance(0.72)
    out = Image.merge("RGBA", (gray, gray, gray, alpha.point(lambda value: round(value * 0.45))))
    return out


def draw_vertical_gradient(draw: ImageDraw.ImageDraw, rect: tuple[int, int, int, int], top: tuple[int, int, int], bottom: tuple[int, int, int]) -> None:
    x0, y0, x1, y1 = rect
    height = max(1, y1 - y0)
    for y in range(y0, y1):
        t = (y - y0) / height
        color = tuple(round(top[i] * (1.0 - t) + bottom[i] * t) for i in range(3))
        draw.line((x0, y, x1 - 1, y), fill=color)


def draw_frame(frame_size: int, state: int) -> Image.Image:
    frame = Image.new("RGBA", (frame_size, frame_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(frame)

    palettes = [
        ((222, 222, 216), (187, 187, 179), (244, 244, 239), (130, 130, 122)),
        ((224, 207, 151), (184, 154, 86), (255, 246, 205), (105, 86, 47)),
        ((239, 219, 151), (210, 173, 92), (255, 252, 215), (118, 93, 47)),
        ((195, 168, 103), (151, 121, 60), (233, 209, 151), (76, 58, 29)),
    ]
    top, bottom, light, dark = palettes[state]
    pressed = state == 3
    hover = state == 2

    draw_vertical_gradient(draw, (1, 1, frame_size - 1, frame_size - 1), top, bottom)

    draw.rectangle((0, 0, frame_size - 1, frame_size - 1), outline=dark)
    draw.rectangle((2, 2, frame_size - 3, frame_size - 3), outline=(120, 103, 65))

    if pressed:
        draw.line((1, 1, frame_size - 2, 1), fill=(70, 54, 29))
        draw.line((1, 1, 1, frame_size - 2), fill=(70, 54, 29))
        draw.line((1, frame_size - 2, frame_size - 2, frame_size - 2), fill=light)
        draw.line((frame_size - 2, 1, frame_size - 2, frame_size - 2), fill=light)
    else:
        draw.line((1, 1, frame_size - 2, 1), fill=light)
        draw.line((1, 1, 1, frame_size - 2), fill=light)
        draw.line((1, frame_size - 2, frame_size - 2, frame_size - 2), fill=dark)
        draw.line((frame_size - 2, 1, frame_size - 2, frame_size - 2), fill=dark)

    # Small pixel texture keeps active states from looking flat without becoming noisy.
    if state != 0:
        for y in range(4, frame_size - 4, 4):
            for x in range(4, frame_size - 4, 4):
                if (x + y + state) % 8 == 0:
                    draw.point((x, y), fill=(255, 255, 235, 34))

    if hover:
        draw.rectangle((0, 0, frame_size - 1, frame_size - 1), outline=(255, 247, 198))
        draw.rectangle((3, 3, frame_size - 4, frame_size - 4), outline=(255, 249, 205))

    return frame


def icon_shadow(icon: Image.Image) -> Image.Image:
    alpha = icon.getchannel("A").filter(ImageFilter.GaussianBlur(1.0))
    shadow = Image.new("RGBA", icon.size, (35, 25, 10, 0))
    shadow.putalpha(alpha.point(lambda value: round(value * 0.48)))
    return shadow


def icon_highlight(icon: Image.Image) -> Image.Image:
    alpha = icon.getchannel("A")
    shifted = ImageChops.offset(alpha, -1, -1).filter(ImageFilter.GaussianBlur(0.4))
    highlight = Image.new("RGBA", icon.size, (255, 246, 195, 0))
    highlight.putalpha(shifted.point(lambda value: round(value * 0.20)))
    return highlight


def compose_state(base_icon: Image.Image, frame_size: int, icon_size: int, state: int) -> Image.Image:
    frame = draw_frame(frame_size, state)

    if state == 0:
        icon = disabled_icon(base_icon)
    elif state == 1:
        icon = rgba_adjust(base_icon, 1.05, 1.05, 1.08)
    elif state == 2:
        icon = rgba_adjust(base_icon, 1.18, 1.08, 1.16)
    else:
        icon = rgba_adjust(base_icon, 0.90, 1.12, 1.08)

    x = (frame_size - icon_size) // 2
    y = x - 1
    if state == 3:
        x += 1
        y += 1

    frame.alpha_composite(icon_shadow(icon), (x + 1, y + 2))
    frame.alpha_composite(icon_highlight(icon), (x, y))
    frame.alpha_composite(icon, (x, y))
    return frame


def generate(source: Path, output: Path, frame_size: int, icon_size: int, hotspot_marker_size: int) -> None:
    icon = remove_hotspot_marker(load_icon(source), hotspot_marker_size)
    icon = fit_icon(icon, icon_size)
    strip = Image.new("RGBA", (frame_size * STATE_COUNT, frame_size), (0, 0, 0, 0))

    for state in range(STATE_COUNT):
        strip.alpha_composite(compose_state(icon, frame_size, icon_size, state), (state * frame_size, 0))

    output.parent.mkdir(parents=True, exist_ok=True)
    strip.save(output)


def main() -> int:
    args = parse_args()
    generate(args.source, args.output, args.frame_size, args.icon_size, args.hotspot_marker_size)
    if not args.quiet:
        print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
