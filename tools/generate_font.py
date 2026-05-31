# generate_font.py - LVGL v8 中文位图字体生成器
# 用法: python generate_font.py --font simhei.ttf --size 24 --output lv_font_chinese_24.c
# 依赖: pip install Pillow

import argparse
import os
from PIL import Image, ImageDraw, ImageFont

# M2 最小字符集
M2_CHARS = (
    "声语同行请选择用户新建最近训练次未首页听理解句子构建命名发音"
    "历史记录设置今日当前屏幕亮度音量自动息屏数据导出清理缓存关于"
    "设备开发者模式返回秒分钟确认取消提示模块中暂无一二三四五六日"
    "月份上午下午时间永久正在加载退出系统就绪版本号"
    "0123456789"
    ".,:;!?()（）/、%°℃"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    "  "
)


def render_glyph(font, char, bpp):
    bbox = font.getbbox(char)
    if bbox is None:
        return None, 0, 0, 0, 0
    w = max(bbox[2] - bbox[0], 0)
    h = max(bbox[3] - bbox[1], 0)
    if w <= 0 or h <= 0:
        return None, 0, 0, 0, 0
    pad = 4
    img = Image.new("L", (w + pad * 2, h + pad * 2), 0)
    draw = ImageDraw.Draw(img)
    draw.text((pad - bbox[0], pad - bbox[1]), char, font=font, fill=255)
    img = img.crop((pad, pad, pad + w, pad + h))
    if bpp == 1:
        img = img.point(lambda x: 255 if x > 127 else 0, "1")
    elif bpp == 2:
        img = img.point(lambda x: min(x // 85, 3) * 85)
    elif bpp == 4:
        pass
    return img, w, h, bbox[0] - pad, bbox[1] - pad


def pack_4bpp(img):
    """Pack grayscale image to LVGL 4bpp: 2 pixels per byte, MSB first."""
    w, h = img.size
    row_bytes = (w + 1) // 2
    data = bytearray(row_bytes * h)
    for y in range(h):
        for x in range(w):
            px = img.getpixel((x, y))
            val = min(px // 17, 15)
            byte_idx = y * row_bytes + x // 2
            if x % 2 == 0:
                data[byte_idx] = (val << 4)
            else:
                data[byte_idx] |= val
    return bytes(data)


def pack_2bpp(img):
    """Pack to 2bpp: 4 pixels per byte, MSB first."""
    w, h = img.size
    row_bytes = (w + 3) // 4
    data = bytearray(row_bytes * h)
    for y in range(h):
        for x in range(w):
            px = img.getpixel((x, y))
            val = min(px // 85, 3)
            byte_idx = y * row_bytes + x // 4
            shift = 6 - 2 * (x % 4)
            data[byte_idx] |= (val << shift)
    return bytes(data)


def pack_1bpp(img):
    """Pack to 1bpp: 8 pixels per byte, MSB first."""
    w, h = img.size
    row_bytes = (w + 7) // 8
    data = bytearray(row_bytes * h)
    for y in range(h):
        for x in range(w):
            if img.getpixel((x, y)):
                byte_idx = y * row_bytes + x // 8
                bit_idx = 7 - (x % 8)
                data[byte_idx] |= (1 << bit_idx)
    return bytes(data)


def generate(font_path, size, chars, output_path, font_name, bpp):
    font = ImageFont.truetype(font_path, size)
    ascent, descent = font.getmetrics()

    # Unique sorted chars
    chars = sorted(set(chars), key=lambda c: ord(c))
    if not chars:
        print("ERROR: empty char set")
        return

    # Render
    bitmaps = []
    descriptors = []
    unicode_list = []
    for c in chars:
        img, w, h, ofs_x, ofs_y = render_glyph(font, c, bpp)
        if bpp == 4:
            packed = pack_4bpp(img) if img else b""
        elif bpp == 2:
            packed = pack_2bpp(img) if img else b""
        else:
            packed = pack_1bpp(img) if img else b""
        bitmaps.append(packed)
        descriptors.append((len(packed), w, h, ofs_x, ofs_y - ascent, w + 1))
        unicode_list.append(ord(c))

    # Recalculate: ofs_y relative to baseline (negative = above baseline)
    # For Chinese fonts: ofs_y = -ascent (glyph starts at the top)
    # Actually: LVGL's ofs_y is the y offset from the baseline to the top of the glyph box
    # Positive = below baseline, Negative = above baseline
    # Most glyphs have ofs_y = -(glyph_height) to 0
    for i, c in enumerate(chars):
        img, w, h, _, _ = render_glyph(font, c, bpp)
        if img is None:
            adv = size // 2
            descriptors[i] = (0, 0, 0, 0, 0, adv)
        else:
            adv = w + 1  # simple advance
            # LVGL ofs_y: from baseline to top of glyph. baseline is at ascent from top.
            # We use a simple model: ofs_y = -(h // 2) centers glyphs on baseline
            descriptors[i] = (len(bitmaps[i]), w, h, 0, -h + ascent // 2, adv)

    # Build bitmap array
    all_bitmap = bytearray()
    desc_final = []
    bitmap_offset = 0
    for i, (bmp_size, w, h, ofs_x, ofs_y, adv) in enumerate(descriptors):
        if bmp_size > 0:
            all_bitmap.extend(bitmaps[i])
        desc_final.append({
            "bitmap_index": bitmap_offset if bmp_size > 0 else 0,
            "adv_w": adv,
            "box_w": max(w, 0),
            "box_h": max(h, 0),
            "ofs_x": ofs_x,
            "ofs_y": ofs_y,
        })
        bitmap_offset += bmp_size

    # Reserve glyph_dsc[0] for "missing glyph"
    glyph_dsc_lines = ["    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"]
    for d in desc_final:
        glyph_dsc_lines.append(
            f"    {{.bitmap_index = {d['bitmap_index']}, .adv_w = {d['adv_w']},"
            f" .box_w = {d['box_w']}, .box_h = {d['box_h']},"
            f" .ofs_x = {d['ofs_x']}, .ofs_y = {d['ofs_y']}}},"
        )

    # Unicode list (uint16_t array)
    ul_lines = []
    for i, cp in enumerate(unicode_list):
        ul_lines.append(f"    0x{cp:04X}")  # comma added below

    # Hex dump
    hex_lines = []
    for i in range(0, len(all_bitmap), 16):
        chunk = all_bitmap[i:i + 16]
        hex_lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")

    header_name = os.path.basename(output_path).replace(".c", ".h")
    guard = header_name.upper().replace(".", "_").replace("-", "_")

    c_code = f"""/* Auto-generated by generate_font.py */
/* Font: {os.path.basename(font_path)}, {size}px, {bpp}bpp, {len(chars)} glyphs */
#include "lvgl.h"

/* ---- UNICODE LIST (SPARSE_TINY) ---- */
static const uint16_t unicode_list_0[] = {{
{','.join(ul_lines)}
}};

/* ---- GLYPH BITMAP ---- */
static const uint8_t glyph_bitmap[] = {{
{chr(10).join(hex_lines)}
}};

/* ---- GLYPH DESCRIPTORS (index 0 = missing glyph) ---- */
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{chr(10).join(glyph_dsc_lines)}
}};

/* ---- CMAP ---- */
static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = {unicode_list[0]},
        .range_length = {unicode_list[-1] - unicode_list[0] + 1},
        .glyph_id_start = 1,
        .unicode_list = unicode_list_0,
        .glyph_id_ofs_list = NULL,
        .list_length = {len(chars)},
        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }}
}};

/* ---- FONT DESCRIPTOR ---- */
static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = {bpp},
    .kern_classes = 0,
    .bitmap_format = 0,
    .cache = NULL,
}};

/* ---- PUBLIC FONT ---- */
const lv_font_t {font_name} = {{
    .dsc = &font_dsc,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .line_height = {size},
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = -{descent // 2},
    .underline_thickness = {max(1, size // 12)},
}};
"""

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(c_code)

    h_code = f"""#ifndef {guard}
#define {guard}
#ifdef __cplusplus
extern "C" {{
#endif
#include "lvgl.h"
extern const lv_font_t {font_name};
#ifdef __cplusplus
}}
#endif
#endif
"""
    h_path = os.path.join(os.path.dirname(output_path),
                          os.path.basename(output_path).replace(".c", ".h"))
    with open(h_path, "w", encoding="utf-8") as f:
        f.write(h_code)

    size_kb = len(all_bitmap) / 1024.0
    print(f"OK: {output_path}  |  {len(chars)} chars, {size_kb:.1f} KB bitmap, {bpp}bpp")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--font", required=True)
    p.add_argument("--size", type=int, default=24)
    p.add_argument("--chars", default=M2_CHARS)
    p.add_argument("--output", required=True)
    p.add_argument("--name", default="lv_font_chinese_24")
    p.add_argument("--bpp", type=int, default=4, choices=[1, 2, 4])
    args = p.parse_args()

    if not os.path.exists(args.font):
        alt = os.path.join(r"C:\Windows\Fonts", args.font)
        if os.path.exists(alt):
            args.font = alt
        else:
            print(f"Font not found: {args.font}")
            exit(1)

    generate(args.font, args.size, args.chars, args.output, args.name, args.bpp)
