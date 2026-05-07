from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "logos" / "source"
OUT_C = ROOT / "src" / "generated" / "logos.c"
SIZE = 48

LOGOS = [
    ("logo_claude", SOURCE_DIR / "claude.ico", 0xD97757, None),
    ("logo_gemini", SOURCE_DIR / "gemini.png", 0x6EA8FE, None),
    ("logo_gpt", SOURCE_DIR / "openai_simpleicons_white.png", 0xF4F7FA, "drop_white_bg"),
    ("logo_deepseek", SOURCE_DIR / "deepseek.ico", 0x4F8CFF, None),
]


def load_rgba(path: Path, fallback_hex: int) -> Image.Image:
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    try:
        image = Image.open(path)
        image.load()
        image = image.convert("RGBA")
    except Exception:
        r = (fallback_hex >> 16) & 0xFF
        g = (fallback_hex >> 8) & 0xFF
        b = fallback_hex & 0xFF
        image = Image.new("RGBA", (SIZE, SIZE), (r, g, b, 255))

    image.thumbnail((SIZE, SIZE), Image.Resampling.LANCZOS)
    offset = ((SIZE - image.width) // 2, (SIZE - image.height) // 2)
    canvas.alpha_composite(image, offset)
    return canvas


def recolor(image: Image.Image, mode: str | None) -> Image.Image:
    if mode != "white":
        if mode != "drop_white_bg":
            return image

        out = Image.new("RGBA", image.size, (0, 0, 0, 0))
        pixels = []
        for r, g, b, a in image.getdata():
            if r > 250 and g > 250 and b > 250:
                pixels.append((0, 0, 0, 0))
            else:
                pixels.append((244, 247, 250, a))
        out.putdata(pixels)
        return out

    out = Image.new("RGBA", image.size, (0, 0, 0, 0))
    pixels = []
    for r, g, b, a in image.getdata():
        if a == 0:
            pixels.append((0, 0, 0, 0))
        else:
            luminance = int((r * 30 + g * 59 + b * 11) / 100)
            alpha = max(a, min(255, 255 - luminance))
            pixels.append((244, 247, 250, alpha))
    out.putdata(pixels)
    return out


def emit_pixels(name: str, image: Image.Image) -> str:
    pixels = list(image.getdata())
    lines = [
        f"static const logo_pixels_t {name}_map = {{",
        "    .pixels = {",
    ]
    row = []
    for index, (r, g, b, a) in enumerate(pixels):
        row.append(f"PX({r}, {g}, {b}, {a})")
        if len(row) == 4:
            lines.append("        " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("        " + ", ".join(row) + ",")
    lines.extend(["    },", "};", ""])
    return "\n".join(lines)


def emit_descriptor(name: str) -> str:
    return f"""const lv_image_dsc_t {name} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_ARGB8888,
    .header.w = {SIZE},
    .header.h = {SIZE},
    .header.stride = {SIZE} * 4,
    .data_size = sizeof({name}_map),
    .data = (const uint8_t *)&{name}_map,
}};
"""


def main() -> None:
    parts = [
        '#include "generated/logos.h"',
        "",
        "typedef struct {",
        f"    lv_color32_t pixels[{SIZE} * {SIZE}];",
        "} logo_pixels_t;",
        "",
        "#define PX(r, g, b, a) { .blue = (uint8_t)(b), .green = (uint8_t)(g), .red = (uint8_t)(r), .alpha = (uint8_t)(a) }",
        "",
    ]

    images = []
    for name, path, fallback_hex, color_mode in LOGOS:
        image = load_rgba(path, fallback_hex)
        image = recolor(image, color_mode)
        images.append((name, image))
        parts.append(emit_pixels(name, image))

    for name, _image in images:
        parts.append(emit_descriptor(name))

    OUT_C.write_text("\n".join(parts), encoding="utf-8")


if __name__ == "__main__":
    main()
