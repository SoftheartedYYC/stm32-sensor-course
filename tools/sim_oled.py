# -*- coding: utf-8 -*-
"""
sim_oled.py -- 不用硬件，先把 OLED 画面"跑"出来看一眼

做三件事：
  1. 直接解析 APP/oled_font.c，把两个字模表读进来
     （所以验的是真正烧进芯片的那份字节，不是另画一份）；
  2. 按 APP/oled_app.c 的逻辑实现 set_cursor / show_char / show_string /
     show_num / show_cn，写进 8 页 x 128 列的显存；
  3. 按 USER/main.c 的排版画几帧典型画面（正常 / 报警 / 设置阈值 / 清屏静音），
     输出 PNG（放大 4 倍）、GIF 滚动动画和终端字符画。

这样能在没有板子的情况下确认：字模对不对、列号有没有算错、
滚动时文字有没有被裁掉、中文和 ASCII 能不能对齐。

用法（在工程根目录）：
    python tools/sim_oled.py
"""

import io
import os
import re
import sys

from PIL import Image

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

ROOT = os.getcwd()
FONT_C = os.path.join(ROOT, "APP", "oled_font.c")
OUT_PNG = os.path.join(ROOT, "tools", "oled_preview.png")
OUT_GIF = os.path.join(ROOT, "tools", "oled_scroll.gif")

WIDTH, PAGES = 128, 8
HEIGHT = PAGES * 8

SCALE = 4  # 输出图片放大倍数，方便肉眼看


# ------------------------------------------------------------ 解析字库
def parse_arrays(path):
    with open(path, "r", encoding="utf-8") as fp:
        text = fp.read()

    def grab(name):
        start = text.index(name)
        start = text.index("{", start) + 1
        end = text.index("};", start)
        return [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", text[start:end])]

    ascii_flat = grab("OLED_F8x16")
    cn_flat = grab("OLED_CF16x16")
    assert len(ascii_flat) % 16 == 0, len(ascii_flat)
    assert len(cn_flat) % 32 == 0, len(cn_flat)
    ascii_tab = [ascii_flat[i:i + 16] for i in range(0, len(ascii_flat), 16)]
    cn_tab = [cn_flat[i:i + 32] for i in range(0, len(cn_flat), 32)]
    print("字库: ASCII %d 字(16 字节/字)，中文 %d 字(32 字节/字)" % (len(ascii_tab), len(cn_tab)))
    return ascii_tab, cn_tab


ASCII_TAB, CN_TAB = parse_arrays(FONT_C)

# 和 oled_font.h 里的枚举顺序一致
(CN_GUANG, CN_MIN, CN_DIAN, CN_ZU, CN_ZHI, CN_LIANG, CN_AN, CN_WEN, CN_DU,
 CN_MO, CN_NI, CN_LIANG2, CN_KAI, CN_GUAN, CN_FU, CN_YA, CN_ZHAO, CN_SHE,
 CN_SHI, CN_DEG, CN_YE, CN_YU, CN_CHEN, CN_FEN, CN_YU2, CN_CHAO, CN_BIAO,
 CN_SHI2) = range(28)
CN_NUM = len(CN_TAB)

ASCII_FIRST, ASCII_LAST = 0x20, 0x7E

# main.c 里的横幅参数，保持同步
BANNER_TEXT_W = 136
BANNER_GAP = 32
BANNER_W = BANNER_TEXT_W + BANNER_GAP
BANNER_BUF_W = BANNER_W + WIDTH


# ------------------------------------------------------------ 显存 + 驱动
class Oled:
    """行为对齐 oled_app.c：页寻址，写一字节 = 一列 8 像素"""

    def __init__(self):
        self.gram = [[0x00] * WIDTH for _ in range(PAGES)]
        self.page = 0
        self.col = 0

    # --- 底层 ---
    def set_cursor(self, page, col):
        if page >= PAGES or col >= WIDTH:
            return  # 和 C 里的越界保护一致
        self.page, self.col = page, col

    def write_data(self, data):
        self.gram[self.page][self.col] = data & 0xFF
        self.col += 1  # SSD1306 写一个字节后列地址自动 +1
        if self.col >= WIDTH:
            self.col = 0  # 到行尾就绕回（真实芯片也是这个行为）

    def write_run(self, page, col, data):
        """对齐 oled_write_run()：一页内连写多个字节"""
        if page >= PAGES or col >= WIDTH or not data:
            return
        data = list(data)[:WIDTH - col]
        self.gram[page][col:col + len(data)] = [b & 0xFF for b in data]

    # --- 上层 ---
    def show_char(self, line, col, ch):
        code = ord(ch)
        if code < ASCII_FIRST or code > ASCII_LAST:
            ch = " "
            code = ASCII_FIRST
        glyph = ASCII_TAB[code - ASCII_FIRST]
        self.set_cursor((line - 1) * 2, (col - 1) * 8)
        for i in range(8):
            self.write_data(glyph[i])
        self.set_cursor((line - 1) * 2 + 1, (col - 1) * 8)
        for i in range(8):
            self.write_data(glyph[i + 8])

    def show_string(self, line, col, s):
        for i, ch in enumerate(s):
            self.show_char(line, col + i, ch)

    def show_num(self, line, col, num, length):
        for i in range(length):
            digit = (num // (10 ** (length - i - 1))) % 10
            self.show_char(line, col + i, chr(digit + ord("0")))

    def show_cn(self, line, col, index):
        if index >= CN_NUM:
            return
        glyph = CN_TAB[index]
        x = (col - 1) * 8
        self.set_cursor((line - 1) * 2, x)
        for i in range(16):
            self.write_data(glyph[i])
        self.set_cursor((line - 1) * 2 + 1, x)
        for i in range(16):
            self.write_data(glyph[i + 16])

    # --- 输出 ---
    def to_image(self):
        img = Image.new("L", (WIDTH, HEIGHT), 0)
        px = img.load()
        for page in range(PAGES):
            for x in range(WIDTH):
                byte = self.gram[page][x]
                for j in range(8):
                    px[x, page * 8 + j] = 255 if (byte >> j) & 1 else 0
        return img

    def to_text(self):
        """字符画：保留全部 64 行，空格代表灭，方便和 PNG 对照"""
        rows = []
        for y in range(HEIGHT):
            line = []
            for x in range(WIDTH):
                bit = (self.gram[y // 8][x] >> (y % 8)) & 1
                line.append("#" if bit else " ")
            rows.append("|" + "".join(line) + "|")
        return rows


# ------------------------------------------------------------ 复刻 main.c 的顶部横幅
def build_banner():
    """对齐 main.c 的 banner_build()：姓名 + 空格 + 学号 + 尾部空白"""
    buf = [[0x00] * BANNER_BUF_W for _ in range(2)]

    def put_cn(page, x, index):
        glyph = CN_TAB[index][page * 16:(page + 1) * 16]
        buf[page][x:x + 16] = glyph

    def put_char(page, x, ch):
        code = ord(ch)
        glyph = ASCII_TAB[code - ASCII_FIRST][page * 8:(page + 1) * 8]
        buf[page][x:x + 8] = glyph

    for page in range(2):
        put_cn(page, 0, CN_YE)
        put_cn(page, 16, CN_YU)
        put_cn(page, 32, CN_CHEN)
        for i, ch in enumerate("2026310095"):
            put_char(page, 56 + i * 8, ch)
        buf[page][BANNER_W:BANNER_W + WIDTH] = buf[page][0:WIDTH]
    return buf


BANNER = build_banner()


def show_banner(o, offset):
    """对齐 main.c 的 banner_show()：从 offset 列开始整页搬 128 字节"""
    for page in range(2):
        o.write_run(page, 0, BANNER[page][offset:offset + WIDTH])


# ------------------------------------------------------------ 复刻 main.c 的画面
def draw_static(o):
    """main.c 里 while 之前画一次的静态内容（第 2~4 行标签）"""
    o.show_cn(2, 1, CN_FEN)
    o.show_cn(2, 3, CN_ZHI)
    o.show_cn(2, 10, CN_GUANG)
    o.show_cn(2, 12, CN_YU2)

    # 第 3 行：温阈27℃ 湿阈50%
    o.show_cn(3, 1, CN_WEN)
    o.show_cn(3, 3, CN_YU2)
    o.show_cn(3, 7, CN_DEG)
    o.show_cn(3, 10, CN_SHI2)
    o.show_cn(3, 12, CN_YU2)
    o.show_char(3, 16, "%")


def draw_dynamic(o, score, light_th=30, temp_th=27, humi_th=50, temp_x10=250, humi=45,
                 setting=0, blink_on=True, alarm=0):
    """main.c 里 while 里每一轮刷新的那几个字段"""
    o.show_num(2, 6, score, 3)

    if alarm:
        o.show_cn(2, 10, CN_CHAO)
        o.show_cn(2, 12, CN_BIAO)
        o.show_string(2, 14, "  ")
    elif setting == 1 and not blink_on:
        o.show_string(2, 14, "  ")
    else:
        o.show_num(2, 14, light_th, 2)

    if setting == 2 and not blink_on:
        o.show_string(3, 5, "  ")
    else:
        o.show_num(3, 5, temp_th, 2)

    if setting == 3 and not blink_on:
        o.show_string(3, 14, "  ")
    else:
        o.show_num(3, 14, humi_th, 2)

    # 第 4 行：温度25℃ 湿度45%（和 main.c 的 DHT11 布局一致）
    o.show_cn(4, 1, CN_WEN)
    o.show_cn(4, 3, CN_DU)
    o.show_cn(4, 7, CN_DEG)
    o.show_cn(4, 10, CN_SHI2)
    o.show_cn(4, 12, CN_DU)
    o.show_char(4, 16, "%")
    if temp_x10 is not None:
        o.show_num(4, 5, (temp_x10 // 10) % 100, 2)
        o.show_num(4, 14, humi, 2)
    else:
        o.show_string(4, 5, "--")
        o.show_string(4, 14, "--")


def frame(o, **kw):
    draw_static(o)
    draw_dynamic(o, **kw)


# ------------------------------------------------------------ 主流程
FRAMES = [
    ("正常：分值 25 / 光阈 30，温度 25℃ / 湿度 45%",
     dict(score=25, light_th=30, temp_th=27, humi_th=50, temp_x10=250, humi=45, alarm=0)),
    ("报警：温度 30℃ > 阈值 27℃，湿度 60% > 阈值 50%",
     dict(score=78, light_th=30, temp_th=27, humi_th=50, temp_x10=300, humi=60, alarm=1)),
    ("设置温度阈值：温度阈值数字闪烁（这一帧是灭的）",
     dict(score=25, light_th=30, temp_th=27, humi_th=50, temp_x10=250, humi=45,
          setting=2, blink_on=False, alarm=0)),
    ("按过静音：30℃ / 60% 仍超阈值，但蜂鸣器不再响",
     dict(score=78, light_th=30, temp_th=27, humi_th=50, temp_x10=300, humi=60, alarm=0)),
]
tiles = []
for title, kw in FRAMES:
    o = Oled()
    show_banner(o, 0)
    frame(o, **kw)
    tiles.append(o.to_image())
    print("\n===== %s =====" % title)
    for line in o.to_text():
        print(line)

# 拼成一张对比图：所有预览帧竖着排，帧间留白
PAD = 8
sheet = Image.new("L", (WIDTH, len(tiles) * (HEIGHT + PAD)), 60)
for i, t in enumerate(tiles):
    sheet.paste(t, (0, i * (HEIGHT + PAD)))

sheet = sheet.resize((sheet.width * SCALE, sheet.height * SCALE), Image.NEAREST)
sheet.save(OUT_PNG)
print("\n预览图已写出：%s  (%dx%d)" % (OUT_PNG, sheet.width, sheet.height))


# 再导出一段滚动动画：每帧就是主循环里 offset += 2 的动作，直接看顺不顺
GIF_STEP = 2
GIF_SCALE = 3
gif_frames = []
for off in range(0, BANNER_W, GIF_STEP):
    o = Oled()
    show_banner(o, off)
    draw_static(o)
    draw_dynamic(o, score=25, temp_th=27, humi_th=50, temp_x10=250, humi=45, alarm=0)
    gif_frames.append(o.to_image().convert("P").resize((WIDTH * GIF_SCALE, HEIGHT * GIF_SCALE), Image.NEAREST))

gif_frames[0].save(OUT_GIF, save_all=True, append_images=gif_frames[1:],
                   duration=100, loop=0, optimize=True)
print("滚动动画已写出：%s  (%d 帧, %dms/帧)" % (OUT_GIF, len(gif_frames), 100))
