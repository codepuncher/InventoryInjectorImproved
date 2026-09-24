#!/usr/bin/env python3
"""
banner.py - Generate docs/assets/banner.png, or with --thumbnail, the Nexus
Mods thumbnail (docs/assets/thumbnail.png): the mark alone, centred on black.

Requires: Pillow (python-pillow / pip install Pillow)
Fonts:    Fira Sans Condensed Light and Light Italic (side text and "Improved"),
          Tinos Bold (the "I5" mark). Downloaded automatically from the
          google/fonts repo on first run and cached in
          $XDG_CACHE_HOME/<project>/ (default: ~/.cache/<project>/).

Usage:
    python3 scripts/banner.py [options]
    python3 scripts/banner.py --thumbnail [options]

Options:
    --left TEXT          Text left of the mark    (default: "Inventory Interface")
                          Ignored with --thumbnail.
    --right TEXT         Text right of the mark    (default: "Information Injector")
                          Ignored with --thumbnail.
    --under TEXT         Text under the mark       (default: "Improved")
                          Ignored with --thumbnail.
    --mark TEXT          Mark text; first character is drawn large, any
                          remaining characters trail smaller and lower-right
                          of it                    (default: "I5")
    --text-font PATH     Path to a .ttf for --left/--right (skips auto-download;
                          ignored with --thumbnail)
    --italic-font PATH   Path to a .ttf for --under (skips auto-download;
                          ignored with --thumbnail)
    --mark-font PATH     Path to a .ttf for --mark (skips auto-download)
    --thumbnail          Render only the mark, sized so its ink height is 2/3
                          of the canvas height, centred on black.
    --output PATH, -o    Output file (default: docs/assets/banner.png, or
                          docs/assets/thumbnail.png with --thumbnail)
    --width INT          Canvas width in pixels (default: 1400, or 1920 with
                          --thumbnail)
    --height INT         Canvas height in pixels (default: 400, or 1080 with
                          --thumbnail)
"""

import argparse
import hashlib
import os
import urllib.request
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

DEFAULT_LEFT = "Inventory Interface"
DEFAULT_RIGHT = "Information Injector"
DEFAULT_UNDER = "Improved"
DEFAULT_MARK = "I5"
DEFAULT_WIDTH = 1400
DEFAULT_HEIGHT = 400
DEFAULT_OUTPUT = Path(__file__).parents[1] / "docs" / "assets" / "banner.png"

THUMBNAIL_WIDTH = 1920
THUMBNAIL_HEIGHT = 1080
DEFAULT_THUMBNAIL_OUTPUT = Path(__file__).parents[1] / "docs" / "assets" / "thumbnail.png"
# Matches I4's Nexus thumbnail: mark ink spans y=180..900 of a 1920x1080 canvas.
THUMBNAIL_INK_HEIGHT_RATIO = 2 / 3

# Text must stay this far from the left/right canvas edges.
SIDE_MARGIN = 40
# Same, but for the top/bottom edges (the canvas is much shorter than it is wide).
VERTICAL_MARGIN = 10
MIN_SIDE_FONT_SIZE = 20

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

_project = Path(__file__).parents[1].name.lower()
_xdg_cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
_font_cache_dir = _xdg_cache / _project

# Fonts pinned to specific google/fonts commits for reproducibility.
TEXT_FONT_URL = "https://github.com/google/fonts/raw/a3d70f5d895f125abc00e3d25ab991526969f910/ofl/firasanscondensed/FiraSansCondensed-Light.ttf"
TEXT_FONT_SHA256 = "1fdefd76fb773e95d1120db6bc20c576feac1e964da28e28eb4342c8e7730cda"
TEXT_FONT_CACHE = _font_cache_dir / "FiraSansCondensed-Light.ttf"

ITALIC_FONT_URL = "https://github.com/google/fonts/raw/a3d70f5d895f125abc00e3d25ab991526969f910/ofl/firasanscondensed/FiraSansCondensed-LightItalic.ttf"
ITALIC_FONT_SHA256 = "602d1e0b961b2efc0da84d00ea334c0f6fe501912e7965b6ed482c62a6406d3f"
ITALIC_FONT_CACHE = _font_cache_dir / "FiraSansCondensed-LightItalic.ttf"

MARK_FONT_URL = "https://github.com/google/fonts/raw/ba95515f1333efe9342c2ad988b9c2f6bef6dbad/ofl/tinos/Tinos-Bold.ttf"
MARK_FONT_SHA256 = "393269dbab8899f938db19783eca5eac92eb431f7ae0ab45b8349ca895f1a06b"
MARK_FONT_CACHE = _font_cache_dir / "Tinos-Bold.ttf"


def ensure_font(custom: str | None, url: str, sha256: str, cache: Path) -> Path:
    if custom and not Path(custom).exists():
        raise FileNotFoundError(f"Font not found: {custom}")
    if custom:
        return Path(custom)
    if cache.exists() and hashlib.sha256(cache.read_bytes()).hexdigest() == sha256:
        return cache
    print(f"Downloading {cache.name} to {cache}")
    cache.parent.mkdir(parents=True, exist_ok=True)
    tmp = cache.with_suffix(".tmp")
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            tmp.write_bytes(response.read())
        digest = hashlib.sha256(tmp.read_bytes()).hexdigest()
        if digest != sha256:
            raise ValueError(f"Font checksum mismatch: expected {sha256}, got {digest}")
        tmp.replace(cache)
    except Exception:
        tmp.unlink(missing_ok=True)
        raise
    return cache


def ink_extent(font: ImageFont.FreeTypeFont, text: str) -> tuple[float, float]:
    """Horizontal ink bounds of text, relative to where anchor="la" would place its pen origin."""
    mask, (ox, _) = font.getmask2(text)
    bbox = mask.getbbox()
    if bbox is None:
        return 0.0, 0.0
    return float(ox + bbox[0]), float(ox + bbox[2])


def mark_geometry(
    draw: ImageDraw.ImageDraw,
    mark_font_path: Path,
    mark: str,
    big_size: int,
) -> dict:
    """Fonts and local layout geometry for the mark, big glyph centred at (0, 0).

    Shared by the banner layout (which fits the mark alongside side text)
    and the thumbnail layout (which centres the mark alone). w_big/h_big and
    mark_left/mark_right come from draw.textbbox(), which for a lone glyph is
    tight vertically but loose horizontally (it includes side bearing), so
    they're good enough for spacing the mark against side text but not for
    precise centring; see glyph_ink_extent() for that.
    """
    big_glyph, small_glyph = mark[0], mark[1:]
    small_size = round(big_size * 0.4333)
    big_font = ImageFont.truetype(str(mark_font_path), big_size)
    small_font = ImageFont.truetype(str(mark_font_path), small_size) if small_glyph else None

    bbox_big = draw.textbbox((0, 0), big_glyph, font=big_font)
    w_big = bbox_big[2] - bbox_big[0]
    h_big = bbox_big[3] - bbox_big[1]
    # The trailing glyph tucks slightly into the big glyph and drops below its baseline.
    small_y_offset = small_size * 0.08

    mark_left = -w_big / 2
    small_xy_local = None
    if small_glyph:
        small_x_local = w_big / 2 - small_size * 0.04
        small_xy_local = (small_x_local, small_y_offset)
        bbox_small = draw.textbbox(small_xy_local, small_glyph, font=small_font, anchor="lm")
        mark_right = bbox_small[2]
    else:
        mark_right = w_big / 2

    return {
        "big_glyph": big_glyph,
        "small_glyph": small_glyph,
        "big_font": big_font,
        "small_font": small_font,
        "small_size": small_size,
        "small_y_offset": small_y_offset,
        "small_xy_local": small_xy_local,
        "bbox_big": bbox_big,
        "w_big": w_big,
        "h_big": h_big,
        "mark_left": mark_left,
        "mark_right": mark_right,
    }


def glyph_ink_extent(font: ImageFont.FreeTypeFont, text: str, anchor: str) -> tuple[float, float, float, float]:
    """True ink bbox of text drawn at pen position (0, 0) with the given anchor.

    font.getmask2(text, anchor=...) returns the same shaping offset
    draw.text(xy, text, anchor=...) uses, so offsetting its mask's own tight
    bbox by that gives the real ink extent. draw.textbbox()'s horizontal
    edges include side bearing instead (see mark_geometry).
    """
    mask, (ox, oy) = font.getmask2(text, anchor=anchor)
    bbox = mask.getbbox()
    if bbox is None:
        raise ValueError(f"{text!r} produced no ink")
    left, top, right, bottom = bbox
    return ox + left, oy + top, ox + right, oy + bottom


def mark_ink_extent(geo: dict) -> tuple[float, float, float, float]:
    """True ink bbox (left, top, right, bottom) of the mark in mark_geometry's (cx, cy) = (0, 0) frame."""
    big_pen = (-geo["w_big"] / 2 - geo["bbox_big"][0], -geo["h_big"] / 2 - geo["bbox_big"][1])
    bl, bt, br, bb = glyph_ink_extent(geo["big_font"], geo["big_glyph"], "la")
    left, top, right, bottom = big_pen[0] + bl, big_pen[1] + bt, big_pen[0] + br, big_pen[1] + bb

    if geo["small_glyph"]:
        small_x_local, small_y_offset = geo["small_xy_local"]
        sl, st, sr, sb = glyph_ink_extent(geo["small_font"], geo["small_glyph"], "lm")
        left = min(left, small_x_local + sl)
        top = min(top, small_y_offset + st)
        right = max(right, small_x_local + sr)
        bottom = max(bottom, small_y_offset + sb)

    return left, top, right, bottom


def solve_mark_size(mark_font_path: Path, mark: str, target_ink_height: float) -> tuple[dict, tuple]:
    """Mark geometry and ink bbox at the big-glyph size whose ink height matches target_ink_height.

    Glyph outlines scale linearly with font size, so one reference
    measurement is enough to solve for it directly.
    """
    probe_draw = ImageDraw.Draw(Image.new("L", (1, 1)))
    reference_size = 1000
    ref_geo = mark_geometry(probe_draw, mark_font_path, mark, reference_size)
    ref_ink = mark_ink_extent(ref_geo)
    ref_height = ref_ink[3] - ref_ink[1]
    if ref_height <= 0:
        raise ValueError("mark produced no ink")

    size = max(1, round(reference_size * target_ink_height / ref_height))
    geo = mark_geometry(probe_draw, mark_font_path, mark, size)
    ink_bbox = mark_ink_extent(geo)
    return geo, ink_bbox


def build_thumbnail_layout(width: int, height: int, mark: str, mark_font_path: Path) -> dict:
    """Lay out the mark alone, centred on its true ink bbox, for the Nexus thumbnail."""
    if not mark:
        raise ValueError("--mark must not be empty")
    target_ink_height = height * THUMBNAIL_INK_HEIGHT_RATIO
    geo, ink_bbox = solve_mark_size(mark_font_path, mark, target_ink_height)

    cx = width / 2 - (ink_bbox[0] + ink_bbox[2]) / 2
    cy = height / 2 - (ink_bbox[1] + ink_bbox[3]) / 2
    big_xy = (cx - geo["w_big"] / 2 - geo["bbox_big"][0], cy - geo["h_big"] / 2 - geo["bbox_big"][1])
    small_xy = None
    if geo["small_glyph"]:
        small_x_local, small_y_offset = geo["small_xy_local"]
        small_xy = (cx + small_x_local, cy + small_y_offset)

    return {
        "big_glyph": geo["big_glyph"],
        "small_glyph": geo["small_glyph"],
        "big_font": geo["big_font"],
        "small_font": geo["small_font"],
        "big_xy": big_xy,
        "small_xy": small_xy,
    }


def draw_mark(draw: ImageDraw.ImageDraw, layout: dict) -> None:
    """Draw the mark's big glyph and, if present, its trailing glyph."""
    draw.text(layout["big_xy"], layout["big_glyph"], font=layout["big_font"], fill=WHITE)
    if layout["small_glyph"]:
        draw.text(layout["small_xy"], layout["small_glyph"], font=layout["small_font"], fill=WHITE, anchor="lm")


def draw_side_text(draw: ImageDraw.ImageDraw, left: str, right: str, under: str, layout: dict) -> None:
    """Draw the banner's left/right side text and, if present, the under text."""
    draw.text(layout["left_xy"], left, font=layout["side_font"], fill=WHITE, anchor="rm")
    draw.text(layout["right_xy"], right, font=layout["side_font"], fill=WHITE, anchor="lm")
    if not under:
        return
    draw.text(layout["under_xy"], under, font=layout["under_font"], fill=WHITE, anchor="mm")


def compute_positions(
    draw: ImageDraw.ImageDraw,
    cy: float,
    *,
    cx: float,
    w_big: float,
    h_big: float,
    bbox_big: tuple[float, float, float, float],
    small_glyph: str,
    small_font: ImageFont.FreeTypeFont | None,
    small_x: float,
    small_y_offset: float,
    x_left_mark: float,
    x_right_big: float,
    gap: float,
    side_font: ImageFont.FreeTypeFont,
    left: str,
    right: str,
    under: str,
    under_font: ImageFont.FreeTypeFont | None,
    under_gap: float,
) -> dict:
    """Lay out every element around a shared vertical centre cy.

    Every y coordinate below is cy plus a fixed offset, so calling this twice
    (once at a guessed cy, once at the shifted cy) is enough to vertically
    centre the whole block without an iterative solver.
    """
    top_mark = cy - h_big / 2
    bottom_mark = cy + h_big / 2
    x_right_mark = x_right_big

    small_xy = None
    if small_glyph:
        small_xy = (small_x, cy + small_y_offset)
        bbox_small = draw.textbbox(small_xy, small_glyph, font=small_font, anchor="lm")
        x_right_mark = bbox_small[2]
        top_mark = min(top_mark, bbox_small[1])
        bottom_mark = max(bottom_mark, bbox_small[3])

    left_xy = (x_left_mark - gap, cy)
    right_xy = (x_right_mark + gap, cy)
    bbox_left = draw.textbbox(left_xy, left, font=side_font, anchor="rm")
    bbox_right = draw.textbbox(right_xy, right, font=side_font, anchor="lm")

    under_xy = None
    bbox_under = None
    if under:
        under_cx = (x_left_mark + x_right_mark) / 2
        under_xy = (under_cx, bottom_mark + under_gap)
        bbox_under = draw.textbbox(under_xy, under, font=under_font, anchor="mm")

    tops = [top_mark, bbox_left[1], bbox_right[1]]
    bottoms = [bottom_mark, bbox_left[3], bbox_right[3]]
    if bbox_under:
        tops.append(bbox_under[1])
        bottoms.append(bbox_under[3])

    return {
        "big_xy": (cx - w_big / 2 - bbox_big[0], cy - h_big / 2 - bbox_big[1]),
        "small_xy": small_xy,
        "left_xy": left_xy,
        "right_xy": right_xy,
        "under_xy": under_xy,
        "block_top": min(tops),
        "block_bottom": max(bottoms),
    }


def build_layout(
    draw: ImageDraw.ImageDraw,
    width: int,
    height: int,
    left: str,
    right: str,
    under: str,
    mark: str,
    text_font_path: Path,
    italic_font_path: Path,
    mark_font_path: Path,
) -> dict:
    if not mark:
        raise ValueError("--mark must not be empty")
    if not left:
        raise ValueError("--left must not be empty")
    if not right:
        raise ValueError("--right must not be empty")

    scale = 1.0
    while True:
        big_size = round(height * 0.75 * scale)
        side_size = round(height * 0.155 * scale)
        under_size = round(height * 0.135 * scale)
        # Only a shrink-to-fit pass (scale < 1) can hit this; at scale 1 a
        # small --height just means a small (and valid) side_size.
        if scale < 1.0 and side_size < MIN_SIDE_FONT_SIZE:
            raise ValueError("text does not fit even at the minimum font size; shorten --left/--right/--under")

        side_font = ImageFont.truetype(str(text_font_path), side_size)
        under_font = ImageFont.truetype(str(italic_font_path), under_size) if under else None

        # Mark ink extent in a frame where the big glyph is centred on 0, so
        # it can be measured before cx (the composition's true centre) is known.
        geo = mark_geometry(draw, mark_font_path, mark, big_size)
        big_glyph, small_glyph = geo["big_glyph"], geo["small_glyph"]
        big_font, small_font, small_size = geo["big_font"], geo["small_font"], geo["small_size"]
        bbox_big, w_big, h_big = geo["bbox_big"], geo["w_big"], geo["h_big"]
        small_y_offset = geo["small_y_offset"]
        mark_left_local, mark_right_local = geo["mark_left"], geo["mark_right"]
        mark_width = mark_right_local - mark_left_local
        gap = side_size * 0.8
        under_gap = under_size * 0.75

        left_width = side_font.getlength(left)
        right_width = side_font.getlength(right)

        # Centre the whole (left text + gap + mark + gap + right text) block on
        # the canvas, rather than centring the mark alone, so outer margins match.
        total_width = left_width + gap + mark_width + gap + right_width
        fits_horizontally = total_width <= width - 2 * SIDE_MARGIN
        block_left = (width - total_width) / 2
        cx = block_left + left_width + gap - mark_left_local

        # getlength() gives each string's advance width, not its ink extent, so
        # the block above is centred on the advance boxes. Their ink can sit a
        # few px off-centre within those boxes (side bearing); measure the real
        # ink margins this would produce and shift cx once to equalise them.
        l_left, _ = ink_extent(side_font, left)
        _, r_right = ink_extent(side_font, right)
        left_margin = (cx + mark_left_local - gap) - left_width + l_left
        right_margin = width - ((cx + mark_left_local + mark_width + gap) + r_right)
        cx += (right_margin - left_margin) / 2

        x_left_mark = cx + mark_left_local
        x_right_big = cx + w_big / 2
        small_x = x_right_big - small_size * 0.04

        kwargs = dict(
            cx=cx,
            w_big=w_big,
            h_big=h_big,
            bbox_big=bbox_big,
            small_glyph=small_glyph,
            small_font=small_font,
            small_x=small_x,
            small_y_offset=small_y_offset,
            x_left_mark=x_left_mark,
            x_right_big=x_right_big,
            gap=gap,
            side_font=side_font,
            left=left,
            right=right,
            under=under,
            under_font=under_font,
            under_gap=under_gap,
        )

        probe = compute_positions(draw, height / 2, **kwargs)
        shift = height / 2 - (probe["block_top"] + probe["block_bottom"]) / 2
        cy = height / 2 + shift
        final = compute_positions(draw, cy, **kwargs)
        fits_vertically = final["block_top"] >= VERTICAL_MARGIN and final["block_bottom"] <= height - VERTICAL_MARGIN

        # The under text is centred independently of the left/right block, so
        # it needs its own margin check: a long --under can outgrow a narrow canvas.
        fits_under = True
        if under:
            l_under, r_under = ink_extent(under_font, under)
            under_pen_x = final["under_xy"][0] - under_font.getlength(under) / 2
            fits_under = (
                under_pen_x + l_under >= SIDE_MARGIN and under_pen_x + r_under <= width - SIDE_MARGIN
            )

        if fits_horizontally and fits_vertically and fits_under:
            break
        scale *= 0.95

    return {
        "big_glyph": big_glyph,
        "small_glyph": small_glyph,
        "big_font": big_font,
        "small_font": small_font,
        "side_font": side_font,
        "under_font": under_font,
        **final,
    }


def positive_int(value: str) -> int:
    n = int(value)
    if n <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return n


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the Nexus Mods banner image.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--left", default=DEFAULT_LEFT, help="Text left of the mark (ignored with --thumbnail)")
    parser.add_argument("--right", default=DEFAULT_RIGHT, help="Text right of the mark (ignored with --thumbnail)")
    parser.add_argument("--under", default=DEFAULT_UNDER, help="Text under the mark (ignored with --thumbnail)")
    parser.add_argument("--mark", default=DEFAULT_MARK, help="Mark text (first char large, rest trail smaller)")
    parser.add_argument(
        "--text-font", default=None, metavar="PATH",
        help="Path to a .ttf for --left/--right (skips auto-download; ignored with --thumbnail)",
    )
    parser.add_argument(
        "--italic-font", default=None, metavar="PATH",
        help="Path to a .ttf for --under (skips auto-download; ignored with --thumbnail)",
    )
    parser.add_argument("--mark-font", default=None, metavar="PATH", help="Path to a .ttf for --mark (skips auto-download)")
    parser.add_argument(
        "--thumbnail", action="store_true",
        help="Render only the mark, centred on black, sized for a Nexus Mods thumbnail",
    )
    parser.add_argument(
        "--output", "-o", default=argparse.SUPPRESS, metavar="PATH",
        help=f"Output .png path (default: {DEFAULT_OUTPUT}, or {DEFAULT_THUMBNAIL_OUTPUT} with --thumbnail)",
    )
    parser.add_argument(
        "--width", default=argparse.SUPPRESS, type=positive_int,
        help=f"Canvas width in pixels (default: {DEFAULT_WIDTH}, or {THUMBNAIL_WIDTH} with --thumbnail)",
    )
    parser.add_argument(
        "--height", default=argparse.SUPPRESS, type=positive_int,
        help=f"Canvas height in pixels (default: {DEFAULT_HEIGHT}, or {THUMBNAIL_HEIGHT} with --thumbnail)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    mark_font_path = ensure_font(args.mark_font, MARK_FONT_URL, MARK_FONT_SHA256, MARK_FONT_CACHE)

    width = getattr(args, "width", None) or (THUMBNAIL_WIDTH if args.thumbnail else DEFAULT_WIDTH)
    height = getattr(args, "height", None) or (THUMBNAIL_HEIGHT if args.thumbnail else DEFAULT_HEIGHT)
    output = getattr(args, "output", None) or (str(DEFAULT_THUMBNAIL_OUTPUT) if args.thumbnail else str(DEFAULT_OUTPUT))

    img = Image.new("RGB", (width, height), BLACK)
    draw = ImageDraw.Draw(img)

    if args.thumbnail:
        layout = build_thumbnail_layout(width, height, args.mark, mark_font_path)
    else:
        text_font_path = ensure_font(args.text_font, TEXT_FONT_URL, TEXT_FONT_SHA256, TEXT_FONT_CACHE)
        italic_font_path = ensure_font(args.italic_font, ITALIC_FONT_URL, ITALIC_FONT_SHA256, ITALIC_FONT_CACHE)
        layout = build_layout(
            draw,
            width,
            height,
            args.left,
            args.right,
            args.under,
            args.mark,
            text_font_path,
            italic_font_path,
            mark_font_path,
        )

    draw_mark(draw, layout)
    if not args.thumbnail:
        draw_side_text(draw, args.left, args.right, args.under, layout)

    out = Path(output)
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out, format="PNG", optimize=True)
    print(f"Saved to {out}")


if __name__ == "__main__":
    main()
