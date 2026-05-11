"""Generate qt_finance.ico — a candlestick-chart icon for the finance app."""
from PIL import Image, ImageDraw
import math, os

def draw_icon(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d   = ImageDraw.Draw(img)
    s   = size

    # Background: rounded rect, dark navy
    r = max(2, s // 8)
    d.rounded_rectangle([0, 0, s - 1, s - 1], radius=r, fill=(18, 30, 55, 255))

    # Grid lines (subtle)
    grid_color = (255, 255, 255, 20)
    rows = 4
    for i in range(1, rows):
        y = int(s * i / rows)
        d.line([(int(s * 0.08), y), (int(s * 0.92), y)], fill=grid_color, width=max(1, s // 64))

    # Candlestick data: (open, close, low, high) normalised 0–1, low→top of chart
    candles = [
        (0.70, 0.55, 0.72, 0.50),  # red
        (0.55, 0.60, 0.57, 0.48),  # green
        (0.60, 0.45, 0.62, 0.42),  # green  ← tallest candle
        (0.45, 0.50, 0.47, 0.38),  # green
        (0.50, 0.35, 0.52, 0.30),  # green  ← top
    ]

    n       = len(candles)
    margin  = s * 0.10
    total_w = s - 2 * margin
    slot_w  = total_w / n
    body_w  = max(2, slot_w * 0.50)
    wick_w  = max(1, s // 48)

    def ypos(v):
        # v=0 → top of chart area, v=1 → bottom
        top    = s * 0.10
        bottom = s * 0.88
        return top + v * (bottom - top)

    for i, (op, cl, hi, lo) in enumerate(candles):
        cx     = margin + slot_w * i + slot_w / 2
        x0     = cx - body_w / 2
        x1     = cx + body_w / 2
        bull   = cl <= op          # price went up (close ≤ open in our y-scheme)
        color  = (52, 211, 153, 255) if bull else (239, 68, 68, 255)

        top_body = ypos(min(op, cl))
        bot_body = ypos(max(op, cl))
        if bot_body - top_body < max(1, s // 64):
            bot_body = top_body + max(1, s // 64)

        # Wick
        d.line([(int(cx), int(ypos(hi))), (int(cx), int(ypos(lo)))],
               fill=color, width=wick_w)
        # Body
        d.rectangle([int(x0), int(top_body), int(x1), int(bot_body)], fill=color)

    # Trend arrow (top-right, small upward arrow)
    aw = max(4, s // 10)
    ax, ay = int(s * 0.80), int(s * 0.14)
    arrow_color = (52, 211, 153, 255)
    pts = [
        (ax, ay + aw),
        (ax + aw // 2, ay),
        (ax + aw, ay + aw),
    ]
    d.polygon(pts, fill=arrow_color)

    return img


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "resources")
    os.makedirs(out_dir, exist_ok=True)
    ico_path = os.path.join(out_dir, "qt_finance.ico")

    sizes   = [16, 32, 48, 64, 128, 256]
    images  = [draw_icon(sz) for sz in sizes]

    images[0].save(
        ico_path,
        format="ICO",
        append_images=images[1:],
        sizes=[(sz, sz) for sz in sizes],
    )
    print(f"Saved {ico_path}")


if __name__ == "__main__":
    main()
