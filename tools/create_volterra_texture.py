"""Create a UV-safe fictional rally livery from the supplied texture atlas.

Unlike a generative image edit, this script never moves UV islands: it keeps
the original pixels and replaces only selected wordmarks with VOLTERRA RACING.
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
source = Path(r"C:\Users\ADMIN\AppData\Local\Temp\lancia_delta_HF_alta.png")
png_output = root / "assets" / "lancia_delta_volterra.png"
bmp_output = root / "assets" / "lancia_delta_volterra.bmp"

image = Image.open(source).convert("RGB")
draw = ImageDraw.Draw(image)
font_path = r"C:\Windows\Fonts\arialbd.ttf"


def font(size: int):
    return ImageFont.truetype(font_path, size)


def centered_label(bounds, text, fill, text_fill, size):
    draw.rectangle(bounds, fill=fill)
    text_font = font(size)
    text_box = draw.textbbox((0, 0), text, font=text_font)
    x = (bounds[0] + bounds[2] - (text_box[2] - text_box[0])) // 2
    y = (bounds[1] + bounds[3] - (text_box[3] - text_box[1])) // 2
    draw.text((x, y), text, font=text_font, fill=text_fill)


def vertical_label(bounds, text):
    layer = Image.new("RGBA", (bounds[3] - bounds[1], bounds[2] - bounds[0]), (0, 0, 0, 255))
    layer_draw = ImageDraw.Draw(layer)
    text_font = font(8)
    text_box = layer_draw.textbbox((0, 0), text, font=text_font)
    layer_draw.text(((layer.width - (text_box[2] - text_box[0])) // 2, 2), text, font=text_font, fill="white")
    image.paste(layer.rotate(90, expand=True), (bounds[0], bounds[1]))


# Preserve the atlas, stripes, body panels and wheels. Only existing decal zones
# are painted over with original fictional labels.
centered_label((336, 482, 511, 511), "VOLTERRA RACING", "white", "black", 18)
centered_label((332, 301, 385, 319), "VOLTERRA", "#111111", "white", 8)
centered_label((111, 146, 131, 190), "VR", "#0b1f4d", "white", 10)
centered_label((366, 146, 386, 190), "VR", "#0b1f4d", "white", 10)
vertical_label((207, 122, 256, 244), "VOLTERRA RACING")
vertical_label((371, 126, 421, 244), "VOLTERRA RACING")

# Mask tiny sponsor panels with clean fictional blue rally blocks rather than
# leaving inherited real-world wordmarks in those visible locations.
for rect in ((22, 62, 66, 80), (105, 62, 150, 80), (150, 19, 178, 50),
             (143, 307, 186, 333), (256, 19, 284, 79), (348, 18, 405, 49)):
    draw.rectangle(rect, fill="#0b1f4d")
    draw.line((rect[0] + 3, rect[1] + 3, rect[2] - 3, rect[3] - 3), fill="#22aee4", width=2)

png_output.parent.mkdir(parents=True, exist_ok=True)
image.save(png_output)
image.save(bmp_output)
print(f"Wrote {png_output} and {bmp_output} at {image.size[0]}x{image.size[1]}")
