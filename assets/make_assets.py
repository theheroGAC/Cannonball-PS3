import math
import os
import struct
import zlib


def write_png(path, w, h, rgb_buf):
    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    stride = w * 3
    raw = bytearray((stride + 1) * h)
    for y in range(h):
        off = y * (stride + 1)
        raw[off] = 0
        s = y * stride
        raw[off + 1 : off + 1 + stride] = rgb_buf[s : s + stride]

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 6)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def lerp_c(c0, c1, t):
    if t < 0.0:
        t = 0.0
    elif t > 1.0:
        t = 1.0
    r0, g0, b0 = (c0 >> 16) & 0xFF, (c0 >> 8) & 0xFF, c0 & 0xFF
    r1, g1, b1 = (c1 >> 16) & 0xFF, (c1 >> 8) & 0xFF, c1 & 0xFF
    return (
        int(r0 + (r1 - r0) * t),
        int(g0 + (g1 - g0) * t),
        int(b0 + (b1 - b0) * t),
    )


def blend_px(buf, w, h, x, y, r, g, b, a):
    if x < 0 or y < 0 or x >= w or y >= h or a <= 0.0:
        return
    if a > 1.0:
        a = 1.0
    i = (y * w + x) * 3
    ia = 1.0 - a
    buf[i] = int(buf[i] * ia + r * a)
    buf[i + 1] = int(buf[i + 1] * ia + g * a)
    buf[i + 2] = int(buf[i + 2] * ia + b * a)


def fill_rect(buf, w, h, x0, y0, x1, y1, r, g, b, a=1.0):
    ix0 = max(0, int(x0))
    iy0 = max(0, int(y0))
    ix1 = min(w, int(x1))
    iy1 = min(h, int(y1))
    for y in range(iy0, iy1):
        for x in range(ix0, ix1):
            blend_px(buf, w, h, x, y, r, g, b, a)


def draw_line_aa(buf, w, h, x0, y0, x1, y1, r, g, b, a=1.0, thick=1.0):
    steps = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
    rad = int(math.ceil(thick))
    for s in range(steps + 1):
        t = s / float(steps) if steps > 0 else 0.0
        px = x0 + (x1 - x0) * t
        py = y0 + (y1 - y0) * t
        cx, cy = int(round(px)), int(round(py))
        for oy in range(-rad, rad + 1):
            for ox in range(-rad, rad + 1):
                d = math.hypot((cx + ox) - px, (cy + oy) - py)
                if d <= thick:
                    blend_px(buf, w, h, cx + ox, cy + oy, r, g, b, a * max(0.0, 1.0 - d / (thick + 0.3)))


def draw_palm(buf, w, h, base_x, base_y, trunk_h, curve_dir, scale):
    segs = 24
    tip_x, tip_y = base_x, base_y
    for i in range(segs):
        t0 = i / float(segs)
        t1 = (i + 1) / float(segs)
        x0 = base_x + curve_dir * (t0 * t0) * 28.0 * scale
        y0 = base_y - t0 * trunk_h
        x1 = base_x + curve_dir * (t1 * t1) * 28.0 * scale
        y1 = base_y - t1 * trunk_h
        th = (1.0 - t0 * 0.65) * 3.8 * scale
        draw_line_aa(buf, w, h, x0, y0, x1, y1, 14, 7, 26, 0.96, th)
        tip_x, tip_y = x1, y1

    angles = [-2.6, -2.0, -1.35, -0.5, 0.25, 0.95]
    for ang in angles:
        flen = 46.0 * scale
        for step in range(18):
            f = step / 18.0
            ang_cur = ang + 0.45 * f * f * (1 if math.cos(ang) >= 0 else -1)
            fx0 = tip_x + math.cos(ang_cur) * flen * f
            fy0 = tip_y + math.sin(ang_cur) * flen * f * 0.65 + (f * f) * 14.0 * scale
            draw_line_aa(buf, w, h, fx0, fy0, fx0 + math.cos(ang_cur) * 3.0 * scale,
                         fy0 + math.sin(ang_cur) * 2.0 * scale + 3.0 * scale,
                         14, 7, 26, 0.92, (1.0 - f * 0.7) * 2.2 * scale)


def draw_car(buf, w, h, cx, cy, scale):
    bw = 48.0 * scale
    bh = 18.0 * scale
    fill_rect(buf, w, h, cx - bw * 1.08, cy + bh * 0.45, cx + bw * 1.08, cy + bh * 0.95, 6, 3, 14, 0.85)
    fill_rect(buf, w, h, cx - bw, cy - bh * 0.05, cx + bw, cy + bh * 0.55, 225, 24, 42, 1.0)
    fill_rect(buf, w, h, cx - bw * 0.92, cy - bh * 0.25, cx + bw * 0.92, cy - bh * 0.02, 250, 54, 62, 1.0)
    fill_rect(buf, w, h, cx - bw * 0.62, cy - bh * 0.72, cx + bw * 0.62, cy - bh * 0.22, 175, 20, 38, 1.0)
    fill_rect(buf, w, h, cx - bw * 0.52, cy - bh * 0.64, cx + bw * 0.52, cy - bh * 0.30, 34, 18, 52, 0.95)
    fill_rect(buf, w, h, cx - bw * 0.86, cy + bh * 0.02, cx - bw * 0.48, cy + bh * 0.28, 255, 195, 70, 0.98)
    fill_rect(buf, w, h, cx + bw * 0.48, cy + bh * 0.02, cx + bw * 0.86, cy + bh * 0.28, 255, 195, 70, 0.98)
    fill_rect(buf, w, h, cx - bw * 0.96, cy + bh * 0.04, cx - bw * 0.86, cy + bh * 0.24, 255, 45, 75, 1.0)
    fill_rect(buf, w, h, cx + bw * 0.86, cy + bh * 0.04, cx + bw * 0.96, cy + bh * 0.24, 255, 45, 75, 1.0)
    fill_rect(buf, w, h, cx - bw * 0.42, cy + bh * 0.10, cx + bw * 0.42, cy + bh * 0.30, 32, 14, 32, 0.95)
    fill_rect(buf, w, h, cx - bw * 0.88, cy + bh * 0.50, cx - bw * 0.54, cy + bh * 0.84, 15, 10, 20, 1.0)
    fill_rect(buf, w, h, cx + bw * 0.54, cy + bh * 0.50, cx + bw * 0.88, cy + bh * 0.84, 15, 10, 20, 1.0)


FONT_5X7 = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "G": ["01111", "10000", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "/": ["00001", "00010", "00010", "00100", "01000", "01000", "10000"],
    " ": ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
}


def draw_text(buf, w, h, text, x, y, scale, r, g, b, shadow=True):
    cx = int(x)
    for ch in text:
        pat = FONT_5X7.get(ch.upper(), FONT_5X7[" "])
        for ry in range(7):
            row = pat[ry]
            for rx in range(5):
                if row[rx] == "1":
                    if shadow:
                        fill_rect(buf, w, h, cx + rx * scale + scale, y + ry * scale + scale,
                                  cx + (rx + 1) * scale + scale, y + (ry + 1) * scale + scale,
                                  12, 4, 24, 0.85)
                    fill_rect(buf, w, h, cx + rx * scale, y + ry * scale,
                              cx + (rx + 1) * scale, y + (ry + 1) * scale, r, g, b, 1.0)
        cx += 6 * scale


def render_scene(w, h, is_icon):
    buf = bytearray(w * h * 3)
    hy = int(h * 0.56)
    cx = w * (0.50 if is_icon else 0.58)
    sun_r = h * 0.30

    for y in range(h):
        if y < hy:
            t = y / float(hy)
            if t < 0.45:
                r, g, b = lerp_c(0x0B0622, 0x3C0F5E, t / 0.45)
            elif t < 0.78:
                r, g, b = lerp_c(0x3C0F5E, 0xD92B6B, (t - 0.45) / 0.33)
            else:
                r, g, b = lerp_c(0xD92B6B, 0xFF9E44, (t - 0.78) / 0.22)
        else:
            gt = (y - hy) / float(h - hy)
            r, g, b = lerp_c(0x1E0836, 0x070312, gt)
        row_bytes = bytes((r, g, b)) * w
        row_s = y * w * 3
        buf[row_s : row_s + w * 3] = row_bytes

    sun_cy = hy - h * 0.04
    for i in range(45):
        sx = int(((i * 2654435761) % 9973) / 9973.0 * w)
        sy = int(((i * 1597334677) % 7919) / 7919.0 * (hy * 0.58))
        alpha = 0.28 + 0.55 * ((i % 5) / 4.0)
        blend_px(buf, w, h, sx, sy, 235, 220, 255, alpha)
        if not is_icon and i % 3 == 0:
            blend_px(buf, w, h, sx + 1, sy, 235, 220, 255, alpha * 0.6)
            blend_px(buf, w, h, sx, sy + 1, 235, 220, 255, alpha * 0.6)

    for y in range(max(0, int(sun_cy - sun_r - 2)), min(hy, int(hy + 2))):
        dy = sun_cy - y
        for x in range(max(0, int(cx - sun_r - 2)), min(w, int(cx + sun_r + 2))):
            dx = x - cx
            dist = math.hypot(dx, dy)
            if dist <= sun_r:
                st = (y - (sun_cy - sun_r)) / (2.0 * sun_r)
                stripe = False
                if st > 0.48:
                    stripe_phase = (st - 0.48) * 14.0
                    stripe = (stripe_phase - int(stripe_phase)) < (0.14 + (st - 0.48) * 0.55)
                if not stripe:
                    sr, sg, sb = lerp_c(0xFFE66D, 0xFF2E7E, min(1.0, st * 1.15))
                    blend_px(buf, w, h, x, y, sr, sg, sb, 0.96)

    for x in range(w):
        nx = x / float(w)
        mh = (math.sin(nx * 13.0) * 0.5 + math.sin(nx * 29.0 + 1.2) * 0.35 + 1.0) * h * 0.055
        top_y = int(hy - mh)
        for y in range(max(0, top_y), hy):
            i = (y * w + x) * 3
            buf[i], buf[i + 1], buf[i + 2] = 22, 8, 40

    road_w_top = w * 0.05
    road_w_bot = w * 0.42
    prev_band = -1
    for y in range(hy, h):
        gt = (y - hy) / float(max(1, h - hy))
        rw = road_w_top + (road_w_bot - road_w_top) * gt
        lx = int(cx - rw * 0.5)
        rx = int(cx + rw * 0.5)
        clx = max(0, lx)
        crx = min(w, rx)
        if crx > clx:
            row_s = y * w * 3
            buf[row_s + clx * 3 : row_s + crx * 3] = bytes((26, 20, 42)) * (crx - clx)
        if 0 <= lx < w:
            blend_px(buf, w, h, lx, y, 255, 70, 180, 0.95)
        if 0 <= rx < w:
            blend_px(buf, w, h, rx, y, 255, 70, 180, 0.95)
        if int(gt * 28.0) % 2 == 0:
            blend_px(buf, w, h, int(cx), y, 255, 220, 110, 0.9)

        cur_band = int((gt ** 0.65) * 14.0)
        if y > hy and cur_band != prev_band:
            for x in range(w):
                if x < lx or x > rx:
                    blend_px(buf, w, h, x, y, 210, 45, 165, 0.52)
        prev_band = cur_band

    for gx in range(-10, 11):
        if gx == 0:
            continue
        x_top = cx + gx * (w * 0.025)
        x_bot = cx + gx * (w * 0.145)
        draw_line_aa(buf, w, h, x_top, hy, x_bot, h - 1, 185, 45, 165, 0.35, 1.0)

    draw_line_aa(buf, w, h, 0, hy, w - 1, hy, 255, 90, 200, 0.9, 1.2 if is_icon else 2.2)

    if is_icon:
        draw_palm(buf, w, h, w * 0.10, hy + h * 0.12, h * 0.42, -0.8, 0.95)
        draw_palm(buf, w, h, w * 0.90, hy + h * 0.14, h * 0.40, 0.8, 0.90)
        draw_car(buf, w, h, cx, hy + h * 0.25, 0.92)
        draw_line_aa(buf, w, h, 8, 8, w - 9, 8, 0, 235, 255, 0.85, 1.3)
        draw_line_aa(buf, w, h, 8, h - 9, w - 9, h - 9, 255, 65, 175, 0.85, 1.3)
        title = "CANNONBALL"
        ts = 4
        tw = len(title) * 6 * ts
        draw_text(buf, w, h, title, (w - tw) // 2, 16, ts, 255, 240, 120)
        sub = "PS3 EDITION"
        ss = 2
        sw = len(sub) * 6 * ss
        draw_text(buf, w, h, sub, (w - sw) // 2, 48, ss, 0, 240, 255)
        cred = "PORT BY THEHEROGAC"
        cw = len(cred) * 6 * 1
        draw_text(buf, w, h, cred, (w - cw) // 2, h - 22, 1, 245, 210, 255)
    else:
        draw_palm(buf, w, h, w * 0.34, hy + h * 0.12, h * 0.42, -0.85, 4.2)
        draw_palm(buf, w, h, w * 0.86, hy + h * 0.15, h * 0.46, 0.95, 4.5)
        draw_palm(buf, w, h, w * 0.94, hy + h * 0.22, h * 0.35, 0.60, 3.6)
        draw_car(buf, w, h, cx, hy + h * 0.25, 4.2)
        fill_rect(buf, w, h, w - 580, h - 68, w - 36, h - 28, 12, 6, 26, 0.72)
        draw_text(buf, w, h, "CANNONBALL PS3 // THEHEROGAC", w - 560, h - 55, 3, 255, 215, 110)

    return buf


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    icon_buf = render_scene(320, 176, True)
    write_png(os.path.join(here, "ICON0.PNG"), 320, 176, icon_buf)
    pic_buf = render_scene(1920, 1080, False)
    write_png(os.path.join(here, "PIC1.PNG"), 1920, 1080, pic_buf)


if __name__ == "__main__":
    main()
