# -*- coding: utf-8 -*-
"""
gen_oled_font.py -- 生成 APP/oled_font.c 与 APP/oled_font.h

做两件事：
  1. 从 0.示例 的 OLED_Font.h 里抽出 8x16 ASCII 字模（0x20~0x7E，共 95 个），
     原样重排进本工程的 oled_font.c —— 保证数字/字母的字形和课程示例完全一致。
  2. 用系统黑体(simhei.ttf) 超采样渲染 16x16 中文字模。

中文字模的对齐做法：所有字都在同一张大画布上用同一个基线绘制，
再用"所有字的并集墨迹框"作为统一裁切框缩放到 16x16。
这样各字之间的相对位置（高低、大小）保持不变 —— 逐个按各自墨迹框居中会导致
"电"这种满格字和"阻"这种偏旁字上下错位。

用法（在工程根目录）：
    python tools/gen_oled_font.py
"""

import io
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# 工程根目录 = 当前工作目录。
# 故意不用 __file__：本工程路径含中文，若通过命令行传脚本路径进来，
# Windows 控制台代码页会让 __file__ 变成乱码（argv 解码用的不是 UTF-8）。
ROOT = os.getcwd()
if not os.path.isdir(os.path.join(ROOT, "APP")):
    raise SystemExit("请在工程根目录（含 APP 子目录）下运行：python tools/gen_oled_font.py")

# 课程示例在上一级目录（项目\0.示例），本工程在 项目\OLED显示光敏值\
PARENT = os.path.dirname(ROOT)
SRC_FONT = os.path.join(
    PARENT, "0.示例", "江科协大", "程序源码", "STM32Project-有注释版",
    "4-1 OLED显示屏", "Hardware", "OLED_Font.h")
if not os.path.isfile(SRC_FONT):
    raise SystemExit("找不到课程示例字库，请确认目录结构：\n  %s" % SRC_FONT)
OUT_C = os.path.join(ROOT, "APP", "oled_font.c")
OUT_H = os.path.join(ROOT, "APP", "oled_font.h")

TTF = r"C:\Windows\Fonts\simhei.ttf"
CN_CHARS = "光敏电阻值亮暗温度模拟量开关伏压照摄氏℃叶雨晨分阈超标湿"
CN_NAMES = ["GUANG", "MIN", "DIAN", "ZU", "ZHI", "LIANG", "AN", "WEN", "DU", "MO", "NI", "LIANG2", "KAI", "GUAN", "FU", "YA", "ZHAO", "SHE", "SHI", "DEG", "YE", "YU", "CHEN", "FEN", "YU2", "CHAO", "BIAO", "SHI2"]

SS = 8                      # 超采样倍数：8 倍下渲染再缩小，等于 8x8 面积平均
CELL = 16                   # 目标字模边长
THRESH = 110                # 二值化阈值


# ---------------------------------------------------------------- ASCII 部分
def extract_ascii():
    """从课程示例里抽出 ASCII 字模的字节，返回 [[16 字节], ...]。"""
    with open(SRC_FONT, "r", encoding="utf-8") as fp:
        text = fp.read()

    start = text.index("OLED_F8x16")
    start = text.index("{", start) + 1
    end = text.index("};", start)
    body = text[start:end]

    values = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
    if len(values) % 16 != 0:
        raise SystemExit("ASCII 字模字节数 %d 不是 16 的整数倍" % len(values))

    glyphs = [values[i:i + 16] for i in range(0, len(values), 16)]
    print("ASCII: 抽出 %d 个字模 (期望 95 = 0x20~0x7E)" % len(glyphs))
    if len(glyphs) != 95:
        raise SystemExit("ASCII 字模数量不对，请检查源文件")
    return glyphs


# ---------------------------------------------------------------- 中文部分
def render_cn():
    """渲染中文字模，返回 {char: [16 字节]}。"""
    font = ImageFont.truetype(TTF, CELL * SS)
    canvas = CELL * SS * 4

    big = {}
    for ch in CN_CHARS:
        img = Image.new("L", (canvas, canvas), 0)
        # 统一基线 + 统一画布原点：各字的相对高低就固定下来了
        ImageDraw.Draw(img).text((canvas // 2, canvas // 2 + CELL * SS // 2),
                                 ch, font=font, fill=255, anchor="ls")
        big[ch] = img

    # 所有字的并集墨迹框 -> 一个共用裁切框（取方形，避免缩放时比例失真）
    boxes = [im.getbbox() for im in big.values()]
    x0 = min(b[0] for b in boxes)
    y0 = min(b[1] for b in boxes)
    x1 = max(b[2] for b in boxes)
    y1 = max(b[3] for b in boxes)
    side = max(x1 - x0, y1 - y0)
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    box = (cx - side // 2, cy - side // 2, cx - side // 2 + side, cy - side // 2 + side)
    print("CN: 并集墨迹框 %s -> 共用裁切框 %s" % ((x0, y0, x1, y1), box))

    out = {}
    for ch in CN_CHARS:
        cell = big[ch].crop(box).resize((CELL, CELL), Image.LANCZOS)
        px = cell.load()
        # 列行式：一个 16x16 字 = 上半页 16 字节 + 下半页 16 字节，共 32 字节。
        # 每字节对应 1 个像素列，bit0 是该列最上面那个像素。
        data = []
        for page in (0, 1):
            for x in range(CELL):  # 一个 16 像素宽的字，每页有 16 列
                byte = 0
                for j in range(8):
                    y = page * 8 + j
                    if px[x, y] > THRESH:
                        byte |= 1 << j
                data.append(byte)
        assert len(data) == 32, len(data)
        out[ch] = data
    return out


# ---------------------------------------------------------------- 输出
HEADER = """/**
 * @file    {name}
 * @brief   {brief}
 *
 * 本文件由 tools/gen_oled_font.py 自动生成，请不要手工修改；
 * 要加字/换字体请改脚本顶部的 CN_CHARS 与 TTF，然后重新运行脚本。
 *
 * 字模格式（SSD1306 页寻址，列行式）：
 *   8x16  ASCII 字模 = 16 字节：上半页 8 字节 + 下半页 8 字节；
 *   16x16 中文字模  = 32 字节：上半页 16 字节 + 下半页 16 字节；
 *   每字节对应 1 个像素列，bit0 是该列最上面那个像素，bit7 是最下面那个。
 */

#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include "stm32f10x.h" /* 提供 uint8_t */

/* ---- 8x16 ASCII 字库：覆盖 0x20(' ') ~ 0x7E('~') ---- */
#define OLED_ASCII_FIRST 0x20 /* 字库第一个字符的 ASCII 码 */
#define OLED_ASCII_LAST  0x7E /* 字库最后一个字符的 ASCII 码 */
#define OLED_ASCII_NUM   (OLED_ASCII_LAST - OLED_ASCII_FIRST + 1) /* 字模个数 = 95 */

/* ---- 16x16 中文字库：索引即 OLED_CF16x16[] 的行号 ---- */
enum
{{
{enum_body}
    OLED_CN_NUM /* 中文字模总数，仅用于边界检查 */
}};

extern const uint8_t OLED_F8x16[OLED_ASCII_NUM][16]; /* 8 宽 16 高 ASCII 字模，16 字节/字 */
extern const uint8_t OLED_CF16x16[OLED_CN_NUM][32];   /* 16 宽 16 高中文字模，32 字节/字 */

#endif /* __OLED_FONT_H */
"""


def fmt_glyph(data, comment):
    """把字模排成每行 8 字节；8x16 是 2 行，16x16 是 4 行（前两行上半页、后两行下半页）。"""
    lines = []
    for i in range(0, len(data), 8):
        lines.append("    " + ",".join("0x%02X" % b for b in data[i:i + 8]) + ",")
    lines[-1] = lines[-1] + comment
    return "\n".join(lines) + "\n"


def main():
    ascii_glyphs = extract_ascii()
    cn_glyphs = render_cn()

    # ---- oled_font.c ----
    parts = []
    parts.append("""/**
 * @file    oled_font.c
 * @brief   OLED 字库：8x16 ASCII（取自课程示例，字形与示例一致）+ 16x16 中文（黑体生成）
 *
 * 本文件由 tools/gen_oled_font.py 自动生成，请不要手工修改。
 */

#include "oled_font.h"

/* 8x16 ASCII 字模，下标 = 字符 - ' '（0x20），共 95 个 */
const uint8_t OLED_F8x16[OLED_ASCII_NUM][16] =
{
""")
    for i, g in enumerate(ascii_glyphs):
        ch = chr(0x20 + i)
        label = "/* '%s' %d */" % (ch if ch != "\\" else "\\\\", i)
        parts.append(fmt_glyph(g, label))
    parts.append("};\n\n")

    parts.append("/* 16x16 中文字模，顺序见 oled_font.h 的 OLED_CN_* 索引；32 字节/字 */\n")
    parts.append("const uint8_t OLED_CF16x16[OLED_CN_NUM][32] =\n{\n")
    for ch, name in zip(CN_CHARS, CN_NAMES):
        parts.append(fmt_glyph(cn_glyphs[ch], "/* %s  OLED_CN_%s */" % (ch, name)))
    parts.append("};\n")

    with open(OUT_C, "w", encoding="utf-8", newline="\n") as fp:
        fp.write("".join(parts))

    # ---- oled_font.h ----
    enum_body = "".join(
        "    OLED_CN_%-6s = %d, /* %s */\n" % (name, i, ch)
        for i, (ch, name) in enumerate(zip(CN_CHARS, CN_NAMES)))
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as fp:
        fp.write(HEADER.format(
            name="oled_font.h",
            brief="OLED 字库声明：8x16 ASCII + 16x16 中文（自动生成，勿手改）",
            enum_body=enum_body))

    print("写出 %s" % OUT_C)
    print("写出 %s" % OUT_H)


if __name__ == "__main__":
    main()
