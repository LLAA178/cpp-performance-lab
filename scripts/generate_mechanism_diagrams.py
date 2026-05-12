#!/usr/bin/env python3
from __future__ import annotations

import inspect
import re
from dataclasses import dataclass
from pathlib import Path
from textwrap import dedent


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "plots"
SUMMARY = ROOT / "results-summary.md"

BG = "#f7f4ec"
PANEL = "#fffdf8"
PANEL_STROKE = "#d7cfbf"
TEXT = "#26323a"
SUBTEXT = "#5b6770"
BLUE = "#4c87c6"
TEAL = "#2a9d8f"
RED = "#c14d4d"
AMBER = "#c08a28"
PURPLE = "#7a5c9e"
GRAY = "#7d8790"
GREEN = "#4f9b5d"


def esc(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


class Svg:
    def __init__(self, width: int, height: int, title: str, desc: str):
        self.width = width
        self.height = height
        self.title = title
        self.desc = desc
        self.parts: list[str] = []

    def add(self, s: str) -> None:
        self.parts.append(s)

    def rect(self, x, y, w, h, fill=PANEL, stroke=PANEL_STROKE, sw=2, rx=16, dash=None):
        extra = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}"{extra}/>'
        )

    def line(self, x1, y1, x2, y2, stroke=TEXT, sw=2.4, dash=None, arrow=False):
        extra = f' stroke-dasharray="{dash}"' if dash else ""
        marker = ' marker-end="url(#arrow)"' if arrow else ""
        self.add(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{stroke}" '
            f'stroke-width="{sw}" fill="none"{extra}{marker}/>'
        )

    def path(self, d, stroke=TEXT, sw=2.4, fill="none", dash=None, arrow=False):
        extra = f' stroke-dasharray="{dash}"' if dash else ""
        marker = ' marker-end="url(#arrow)"' if arrow else ""
        self.add(
            f'<path d="{d}" stroke="{stroke}" stroke-width="{sw}" fill="{fill}"{extra}{marker}/>'
        )

    def circle(self, cx, cy, r, fill=PANEL, stroke=PANEL_STROKE, sw=2):
        self.add(
            f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'
        )

    def text(self, x, y, text, size=15, weight=400, color=TEXT, anchor="start", italic=False):
        font_style = "italic" if italic else "normal"
        self.add(
            f'<text x="{x}" y="{y}" text-anchor="{anchor}" '
            f'font-family="Georgia, serif" font-size="{size}" font-weight="{weight}" '
            f'font-style="{font_style}" fill="{color}">{esc(text)}</text>'
        )

    def group(self, content: str, transform: str = ""):
        if transform:
            self.add(f'<g transform="{transform}">{content}</g>')
        else:
            self.add(f"<g>{content}</g>")

    def render(self) -> str:
        style = dedent(
            f"""
            <style>
              .bg {{ fill: {BG}; }}
              .title {{ font: 700 23px Georgia, serif; fill: {TEXT}; }}
              .subtitle {{ font: 500 14px Georgia, serif; fill: {SUBTEXT}; }}
              .label {{ font: 700 16px Georgia, serif; fill: {TEXT}; }}
              .text {{ font: 13px Georgia, serif; fill: {TEXT}; }}
              .small {{ font: 12px Georgia, serif; fill: {SUBTEXT}; }}
            </style>
            """
        ).strip()
        defs = dedent(
            """
            <defs>
              <marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="#26323a"/>
              </marker>
            </defs>
            """
        ).strip()
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.width}" height="{self.height}" '
            f'viewBox="0 0 {self.width} {self.height}" role="img" '
            f'aria-labelledby="title desc">\n'
            f'  <title id="title">{esc(self.title)}</title>\n'
            f'  <desc id="desc">{esc(self.desc)}</desc>\n'
            f"  {style}\n"
            f"  {defs}\n"
            f'  <rect class="bg" x="0" y="0" width="{self.width}" height="{self.height}"/>\n'
            + "\n".join(f"  {p}" for p in self.parts)
            + "\n</svg>\n"
        )


def save_svg(name: str, svg: str) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / name).write_text(svg, encoding="utf-8")


def panel_title(doc: Svg, x: int, y: int, title: str, subtitle: str = "") -> None:
    doc.text(x, y, title, size=19, weight=700)
    if subtitle:
        doc.text(x, y + 23, subtitle, size=13, color=SUBTEXT)


def box(doc: Svg, x: int, y: int, w: int, h: int, title: str, sub: str = "", fill="#f6fbff", stroke=BLUE) -> None:
    doc.rect(x, y, w, h, fill=fill, stroke=stroke, rx=14)
    doc.text(x + w / 2, y + 24, title, size=14, weight=700, anchor="middle")
    if sub:
        doc.text(x + w / 2, y + 43, sub, size=11, color=SUBTEXT, anchor="middle")


def arrow_label(doc: Svg, x: int, y: int, txt: str, color=GRAY) -> None:
    doc.text(x, y, txt, size=11, color=color, italic=True)


def panel(doc: Svg, x: int, y: int, w: int, h: int, title: str, subtitle: str = "") -> None:
    doc.rect(x, y, w, h, fill="#fffdf8", stroke=PANEL_STROKE, rx=18)
    doc.text(x + 22, y + 32, title, size=17, weight=700)
    if subtitle:
        doc.text(x + 22, y + 52, subtitle, size=12, color=SUBTEXT)


def vbox(doc: Svg, cx: int, y: int, w: int, h: int, label: str, sub: str = "", fill: str = "#eef6ff", stroke: str = BLUE) -> None:
    doc.rect(cx - w / 2, y, w, h, fill=fill, stroke=stroke, rx=12)
    doc.text(cx, y + 22, label, size=13, weight=700, anchor="middle")
    if sub:
        doc.text(cx, y + 40, sub, size=11, color=SUBTEXT, anchor="middle")


def hbox(doc: Svg, x: int, cy: int, w: int, h: int, label: str, sub: str = "", fill: str = "#eef6ff", stroke: str = BLUE) -> None:
    doc.rect(x, cy - h / 2, w, h, fill=fill, stroke=stroke, rx=12)
    doc.text(x + w / 2, cy - 4, label, size=13, weight=700, anchor="middle")
    if sub:
        doc.text(x + w / 2, cy + 13, sub, size=11, color=SUBTEXT, anchor="middle")


def bullet_note(doc: Svg, x: int, y: int, lines: list[str]) -> None:
    yy = y
    for line in lines:
        doc.text(x, yy, line, size=12, color=SUBTEXT)
        yy += 17


def arrow(doc: Svg, x1: int, y1: int, x2: int, y2: int, color: str = TEXT, sw: float = 2.1, dashed: bool = False) -> None:
    doc.line(x1, y1, x2, y2, stroke=color, sw=sw, dash="5 4" if dashed else None, arrow=True)


def chip_line(doc: Svg, x: int, y: int, label: str, width: int = 92, fill: str = "#dff0ff", stroke: str = BLUE) -> None:
    doc.rect(x, y, width, 27, fill=fill, stroke=stroke, rx=8)
    doc.text(x + width / 2, y + 18, label, size=11, weight=700, anchor="middle")


def call_body(fn: callable, *args) -> None:
    params = len(inspect.signature(fn).parameters)
    fn(*args[:params])


def make_two_panel_figure(
    title: str,
    subtitle: str,
    left_title: str,
    left_subtitle: str,
    right_title: str,
    right_subtitle: str,
    left_body: callable,
    right_body: callable,
    footer: list[str],
    width: int = 1200,
    height: int = 430,
) -> str:
    doc = Svg(width, height, title, subtitle)
    panel_title(doc, 40, 42, title, subtitle)
    panel(doc, 30, 78, 550, 280, left_title, left_subtitle)
    panel(doc, 620, 78, 550, 280, right_title, right_subtitle)
    call_body(left_body, doc, 30, 78, 550, 280, BLUE)
    call_body(right_body, doc, 620, 78, 550, 280, RED)
    bullet_note(doc, 50, 386, footer)
    return doc.render()


def make_three_panel_figure(
    title: str,
    subtitle: str,
    panels: list[tuple[str, str, callable, str]],
    footer: list[str],
    width: int = 1200,
    height: int = 430,
    panel_y: int = 78,
    panel_w: int = 356,
    panel_h: int = 280,
    panel_gap: int = 30,
    footer_y: int | None = None,
) -> str:
    doc = Svg(width, height, title, subtitle)
    panel_title(doc, 40, 42, title, subtitle)
    x = 30
    for header, sub, body, accent in panels:
        panel(doc, x, panel_y, panel_w, panel_h, header, sub)
        call_body(body, doc, x, panel_y, panel_w, panel_h, accent)
        x += panel_w + panel_gap
    bullet_note(doc, 50, footer_y if footer_y is not None else panel_y + panel_h + 28, footer)
    return doc.render()


def make_four_panel_figure(
    title: str,
    subtitle: str,
    panels: list[tuple[str, str, callable, str]],
    footer: list[str],
    width: int = 1200,
    height: int = 480,
) -> str:
    doc = Svg(width, height, title, subtitle)
    panel_title(doc, 40, 42, title, subtitle)
    x = 30
    y = 78
    for header, sub, body, accent in panels:
        panel(doc, x, y, 270, 248, header, sub)
        call_body(body, doc, x, y, 270, 248, accent)
        x += 290
    bullet_note(doc, 50, 438, footer)
    return doc.render()


def figure_aos_soa() -> str:
    doc = Svg(1200, 420, "AoS vs SoA access pattern", "Array-of-Structs keeps fields together, while Structure-of-Arrays lets the benchmark skip unused fields.")
    panel_title(doc, 40, 42, "AoS vs SoA", "Use only the fields the loop actually touches.")
    doc.rect(30, 78, 550, 270)
    doc.rect(620, 78, 550, 270)
    doc.text(58, 114, "Array of Structs", size=18, weight=700)
    doc.text(648, 114, "Structure of Arrays", size=18, weight=700)
    doc.text(58, 138, "every element carries all fields", size=13, color=SUBTEXT)
    doc.text(648, 138, "each field can be streamed independently", size=13, color=SUBTEXT)
    # AoS rows
    for i, y in enumerate([170, 214, 258]):
        doc.rect(78, y, 420, 40, fill="#fdf4e8", stroke=AMBER, rx=10)
        doc.text(96, y + 25, f"item {i}", size=13, weight=700)
        doc.rect(170, y + 6, 84, 28, fill="#d8ebff", stroke=BLUE, rx=8)
        doc.rect(262, y + 6, 84, 28, fill="#e6f4d8", stroke=GREEN, rx=8)
        doc.rect(354, y + 6, 84, 28, fill="#efe1fb", stroke=PURPLE, rx=8)
        doc.text(212, y + 25, "A", size=13, anchor="middle")
        doc.text(304, y + 25, "B", size=13, anchor="middle")
        doc.text(396, y + 25, "C", size=13, anchor="middle")
        doc.line(180, y + 46, 180, y + 64, stroke=TEAL, arrow=True)
        doc.line(364, y + 46, 364, y + 64, stroke=TEAL, arrow=True)
    doc.text(100, 338, "The loop still pulls B into cache even when it is unused.", size=13, color=SUBTEXT)
    # SoA columns
    for idx, (x, color, label) in enumerate([(678, BLUE, "A[]"), (800, GRAY, "B[]"), (922, GREEN, "C[]")]):
        doc.rect(x, 176, 92, 110, fill="#fff", stroke=color, rx=12)
        doc.text(x + 46, 198, label, size=14, weight=700, anchor="middle")
        for j, yy in enumerate([222, 246, 270]):
            doc.rect(x + 18, yy, 56, 18, fill="#f6fbff" if idx != 1 else "#f3f4f6", stroke=color, rx=6)
    doc.text(660, 338, "A and C can be streamed without dragging B through the cache.", size=13, color=SUBTEXT)
    return doc.render()


def figure_mutex_atomic() -> str:
    doc = Svg(1200, 430, "Mutex vs Atomic contention path", "Atomic fetch_add and mutex-protected increments both bounce cache-line ownership; mutex adds lock management on top.")
    panel_title(doc, 40, 42, "Mutex vs Atomic", "Shared-write contention dominates both paths.")
    doc.rect(30, 78, 550, 290)
    doc.rect(620, 78, 550, 290)
    doc.text(60, 114, "Atomic fetch_add", size=18, weight=700)
    doc.text(650, 114, "Mutex lock + critical section", size=18, weight=700)
    # atomic path
    doc.rect(74, 166, 132, 72, fill="#eaf5ff", stroke=BLUE, rx=14)
    doc.text(140, 195, "core 0", size=14, weight=700, anchor="middle")
    doc.text(140, 217, "atomic RMW", size=13, anchor="middle")
    doc.rect(324, 166, 132, 72, fill="#eaf5ff", stroke=BLUE, rx=14)
    doc.text(390, 195, "core 1", size=14, weight=700, anchor="middle")
    doc.text(390, 217, "atomic RMW", size=13, anchor="middle")
    doc.rect(192, 266, 168, 44, fill="#fff3df", stroke=AMBER, rx=10)
    doc.text(276, 293, "shared counter cache line", size=13, anchor="middle")
    doc.line(140, 238, 246, 266, stroke=RED, arrow=True)
    doc.line(390, 238, 306, 266, stroke=RED, arrow=True)
    doc.text(160, 332, "cache line ownership bounces between cores", size=12, color=SUBTEXT)
    # mutex path
    doc.rect(660, 166, 128, 56, fill="#f3eefb", stroke=PURPLE, rx=12)
    doc.text(724, 199, "lock()", size=14, weight=700, anchor="middle")
    doc.rect(826, 166, 128, 56, fill="#f3eefb", stroke=PURPLE, rx=12)
    doc.text(890, 199, "critical", size=14, weight=700, anchor="middle")
    doc.text(890, 219, "section", size=12, anchor="middle", color=SUBTEXT)
    doc.rect(992, 166, 128, 56, fill="#f3eefb", stroke=PURPLE, rx=12)
    doc.text(1056, 199, "unlock()", size=14, weight=700, anchor="middle")
    doc.line(788, 194, 826, 194, stroke=PURPLE, arrow=True)
    doc.line(954, 194, 992, 194, stroke=PURPLE, arrow=True)
    doc.rect(694, 262, 408, 48, fill="#fff3df", stroke=AMBER, rx=12)
    doc.text(898, 292, "lock word and counter still contend on the same cache line", size=13, anchor="middle")
    doc.text(664, 336, "The mutex path pays the same coherence cost plus lock bookkeeping.", size=13, color=SUBTEXT)
    return doc.render()


def figure_cache_levels() -> str:
    doc = Svg(1200, 460, "Cache hierarchy working-set curve", "Throughput falls as the working set leaves L1, then L2, then the last-level cache and eventually DRAM.")
    panel_title(doc, 40, 42, "Cache Levels", "Crossing a cache boundary changes the cost model.")
    doc.rect(30, 78, 570, 300, fill="#fffdf8", stroke=PANEL_STROKE, rx=18)
    doc.rect(620, 78, 540, 300, fill="#fffdf8", stroke=PANEL_STROKE, rx=18)
    doc.text(54, 112, "Hierarchy", size=18, weight=700)
    for y, label, w, color in [(140, "L1D", 180, "#dff0ff"), (194, "L2", 260, "#e7f6de"), (248, "L3", 350, "#fff0d7"), (302, "DRAM", 440, "#f8e2e2")]:
        doc.rect(78, y, w, 34, fill=color, stroke=GRAY, rx=12)
        doc.text(98, y + 22, label, size=14, weight=700)
    doc.text(78, 374, "each bigger step adds latency and often more misses", size=13, color=SUBTEXT)
    doc.text(650, 112, "Mechanism", size=18, weight=700)
    doc.path("M 686 330 C 748 270, 796 260, 840 220 S 960 152, 1076 128", stroke=TEXT, sw=3.2, arrow=True)
    doc.rect(690, 286, 130, 32, fill="#dff0ff", stroke=BLUE, rx=10)
    doc.text(755, 307, "L1 hits", size=13, weight=700, anchor="middle")
    doc.rect(822, 232, 130, 32, fill="#e7f6de", stroke=GREEN, rx=10)
    doc.text(887, 253, "L2 hits", size=13, weight=700, anchor="middle")
    doc.rect(954, 178, 130, 32, fill="#fff0d7", stroke=AMBER, rx=10)
    doc.text(1019, 199, "L3 hits", size=13, weight=700, anchor="middle")
    doc.rect(1086, 124, 48, 32, fill="#f8e2e2", stroke=RED, rx=10)
    doc.text(1030, 140, "DRAM", size=13, weight=700, anchor="end")
    doc.text(650, 374, "once the footprint exceeds a tier, the access stream falls through to the next one", size=13, color=SUBTEXT)
    return doc.render()


def figure_stride_access() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        for ix, label in [(78, "0"), (164, "1"), (250, "2"), (336, "3")]:
            chip_line(doc, x + ix, 172, label, width=72, fill="#dff0ff", stroke=BLUE)
        arrow(doc, x + 114, 188, x + 164, 188, color=TEAL)
        arrow(doc, x + 200, 188, x + 250, 188, color=TEAL)
        arrow(doc, x + 286, 188, x + 336, 188, color=TEAL)
        doc.text(x + 42, 226, "stride 1", size=13, weight=700)
        doc.text(x + 42, 246, "touch every cache line in order", size=12, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        for ix, label in [(78, "0"), (164, "4"), (250, "16"), (336, "64")]:
            chip_line(doc, x + ix, 172, label, width=72, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 114, 188, x + 164, 188, color=RED)
        arrow(doc, x + 200, 188, x + 250, 188, color=RED)
        arrow(doc, x + 286, 188, x + 336, 188, color=RED)
        doc.text(x + 42, 226, "large stride", size=13, weight=700)
        doc.text(x + 42, 246, "skip most cache lines, so useful work per fetch drops", size=12, color=SUBTEXT)

    return make_two_panel_figure(
        "Stride access locality",
        "Accessing every Nth item converts a streaming pattern into a sparse one with worse spatial locality.",
        "Sequential stream",
        "best locality",
        "Sparse stream",
        "more wasted fetches",
        left,
        right,
        ["The key variable is how much useful data each fetched cache line contributes."],
    )


def figure_pointer_chasing() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        xs = [80, 166, 252, 338]
        for i, px in enumerate(xs):
            chip_line(doc, x + px, 168, f"{i}", width=56, fill="#dff0ff", stroke=BLUE)
        arrow(doc, x + 136, 183, x + 166, 183, color=TEAL)
        arrow(doc, x + 222, 183, x + 252, 183, color=TEAL)
        arrow(doc, x + 308, 183, x + 338, 183, color=TEAL)
        doc.text(x + 52, 228, "next pointer is predictable", size=13, weight=700)
        doc.text(x + 52, 248, "prefetcher can stay ahead", size=12, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        pts = [(86, 170), (194, 140), (302, 208), (200, 246), (104, 224)]
        for idx, (px, py) in enumerate(pts):
            doc.rect(x + px, py, 54, 28, fill="#fff0d7", stroke=AMBER, rx=9)
            doc.text(x + px + 27, py + 18, str(idx), size=12, weight=700, anchor="middle")
        arrow(doc, x + 140, 184, x + 194, 154, color=RED)
        arrow(doc, x + 248, 154, x + 302, 222, color=RED)
        arrow(doc, x + 328, 222, x + 200, 260, color=RED)
        arrow(doc, x + 164, 246, x + 104, 238, color=RED)
        doc.text(x + 52, 286, "each step depends on the previous load", size=13, weight=700)
        doc.text(x + 52, 306, "hardware cannot prefetch an unknown next address", size=12, color=SUBTEXT)

    return make_two_panel_figure(
        "Pointer chasing",
        "The load address itself comes from the previous load, so latency cannot be hidden by regular streaming.",
        "Sequential list",
        "regular next step",
        "Pointer chase",
        "address dependency chain",
        left,
        right,
        ["This is a latency story: the chain blocks overlap, not a bandwidth story."],
    )


def figure_false_sharing() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.rect(x + 62, 146, 180, 54, fill="#dff0ff", stroke=BLUE, rx=12)
        doc.text(x + 152, 178, "counter a", size=14, weight=700, anchor="middle")
        doc.rect(x + 62, 204, 180, 54, fill="#dff0ff", stroke=BLUE, rx=12)
        doc.text(x + 152, 236, "counter b", size=14, weight=700, anchor="middle")
        doc.rect(x + 246, 146, 52, 112, fill="#fff0d7", stroke=AMBER, rx=10)
        doc.text(x + 272, 206, "same", size=12, weight=700, anchor="middle")
        doc.text(x + 272, 222, "cache", size=12, weight=700, anchor="middle")
        doc.text(x + 272, 238, "line", size=12, weight=700, anchor="middle")
        doc.line(x + 242, 173, x + 246, 173, stroke=RED, arrow=True)
        doc.line(x + 242, 231, x + 246, 231, stroke=RED, arrow=True)
        doc.text(x + 52, 272, "independent variables share one coherence unit", size=13, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.rect(x + 48, 146, 114, 54, fill="#dff0ff", stroke=BLUE, rx=12)
        doc.rect(x + 188, 146, 114, 54, fill="#dff0ff", stroke=BLUE, rx=12)
        doc.text(x + 105, 178, "counter a", size=14, weight=700, anchor="middle")
        doc.text(x + 245, 178, "counter b", size=14, weight=700, anchor="middle")
        doc.rect(x + 48, 214, 114, 42, fill="#f3faf3", stroke=GREEN, rx=12)
        doc.rect(x + 188, 214, 114, 42, fill="#f3faf3", stroke=GREEN, rx=12)
        doc.text(x + 105, 240, "line 0", size=12, anchor="middle")
        doc.text(x + 245, 240, "line 1", size=12, anchor="middle")
        doc.line(x + 160, 173, x + 188, 173, stroke=TEAL, arrow=True)
        doc.text(x + 52, 272, "padding moves the writes onto separate cache lines", size=13, color=SUBTEXT)

    return make_two_panel_figure(
        "False sharing",
        "The benchmark writes two different atomics, but performance is determined by whether those atomics live on the same cache line.",
        "Adjacent atomics",
        "one cache line",
        "Padded atomics",
        "separate lines",
        left,
        right,
        ["The unit of ownership is the cache line, not the C++ variable."],
    )


def figure_associativity() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 54, 150, "idx 0", width=72, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 154, 150, "+64B", width=72, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 254, 150, "+64B", width=72, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 354, 150, "+64B", width=72, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 90, 165, x + 154, 165, color=TEAL)
        arrow(doc, x + 190, 165, x + 254, 165, color=TEAL)
        arrow(doc, x + 290, 165, x + 354, 165, color=TEAL)
        doc.text(x + 56, 208, "stride = 64B, each touch goes to the next line", size=13, weight=700)
        doc.text(x + 56, 228, "set index rotates instead of piling up in one set", size=12, color=SUBTEXT)
        for i, fill in enumerate(["#e8f6e8", "#e8f6e8", "#e8f6e8", "#e8f6e8"]):
            chip_line(doc, x + 90 + i * 92, 270, f"set {i}", width=72, fill=fill, stroke=GREEN)
            doc.text(x + 126 + i * 92, 318, f"tag {i}", size=12, anchor="middle", color=SUBTEXT)
        doc.text(x + 56, 338, "active lines spread across sets, so ways are not the bottleneck", size=12, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 54, 150, "idx 0", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 154, 150, "+8KiB", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 254, 150, "+8KiB", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 354, 150, "+8KiB", width=72, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 90, 165, x + 154, 165, color=RED)
        arrow(doc, x + 190, 165, x + 254, 165, color=RED)
        arrow(doc, x + 290, 165, x + 354, 165, color=RED)
        doc.text(x + 56, 208, "stride = 8KiB, tag changes but set index repeats", size=13, weight=700)
        doc.text(x + 56, 228, "the walk keeps hammering one cache set", size=12, color=SUBTEXT)
        for i, lab in enumerate(["tag A", "tag B", "tag C", "tag D"]):
            fill = "#fff0d7" if i < 3 else "#f8e2e2"
            stroke = AMBER if i < 3 else RED
            chip_line(doc, x + 178, 286 - i * 34, lab, width=92, fill=fill, stroke=stroke)
        doc.text(x + 316, 252, "same set", size=12, weight=700, color=RED)
        doc.text(x + 316, 272, "limited ways", size=12, color=SUBTEXT)
        doc.text(x + 316, 304, "new tag arrives", size=12, color=SUBTEXT)
        doc.text(x + 316, 324, "old line gets evicted", size=12, color=SUBTEXT)

    return make_two_panel_figure(
        "Cache associativity",
        "Capacity can still be available while one cache set overflows and starts self-evicting.",
        "Friendly stride",
        "addresses walk different sets",
        "Conflict stride",
        "addresses alias one set",
        left,
        right,
        ["Associativity failures are mapping failures: enough total cache, but not enough ways in one set."],
    )


def figure_queue() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        vbox(doc, x + 120, 146, 150, 56, "producer", "lock -> push", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 270, 216, 150, 56, "std::queue", "shared head/tail", fill="#fff0d7", stroke=AMBER)
        vbox(doc, x + 420, 146, 150, 56, "consumer", "lock -> pop", fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 195, 174, x + 270, 216, color=PURPLE)
        arrow(doc, x + 420, 174, x + 345, 216, color=PURPLE)
        doc.text(x + 60, 294, "each transfer enters the same critical section", size=13, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        vbox(doc, x + 116, 154, 150, 56, "producer", "write slot + head", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 274, 216, 170, 70, "ring buffer", "producer owns head\nconsumer owns tail".replace("\n", " "), fill="#f3faf3", stroke=GREEN)
        vbox(doc, x + 432, 154, 150, 56, "consumer", "read slot + tail", fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 191, 182, x + 274, 216, color=TEAL)
        arrow(doc, x + 432, 182, x + 344, 216, color=TEAL)
        doc.text(x + 68, 302, "shared metadata shrinks to head/tail publication", size=13, color=SUBTEXT)

    return make_two_panel_figure(
        "1P1C queue transfer",
        "The tuned SPSC ring removes the central lock and reduces per-message coordination to bounded metadata exchange.",
        "Mutex queue",
        "serialized access",
        "SPSC ring",
        "split producer / consumer ownership",
        left,
        right,
        ["Batching changes how often the expensive coordination path is paid."],
    )


def figure_memory_pool() -> str:
    panels = [
        ("new/delete", "general allocator", lambda d, x, y, w, h, a: (
            vbox(d, x + 178, 154, 170, 56, "allocate", "global allocator", fill="#eef6ff", stroke=BLUE),
            vbox(d, x + 178, 232, 170, 56, "free", "global allocator", fill="#eef6ff", stroke=BLUE),
            arrow(d, x + 178, 210, x + 178, 232, color=GRAY)
        ), BLUE),
        ("locked pool", "shared free list", lambda d, x, y, w, h, a: (
            vbox(d, x + 178, 146, 170, 56, "threads", "all hit one mutex", fill="#fff0d7", stroke=AMBER),
            vbox(d, x + 178, 236, 170, 56, "free list", "one shared pool", fill="#f8e2e2", stroke=RED),
            arrow(d, x + 178, 202, x + 178, 236, color=RED)
        ), RED),
        ("thread-local pool", "mostly local reuse", lambda d, x, y, w, h, a: (
            vbox(d, x + 106, 176, 112, 56, "T0 pool", "local", fill="#f3faf3", stroke=GREEN),
            vbox(d, x + 250, 176, 112, 56, "T1 pool", "local", fill="#f3faf3", stroke=GREEN),
            doc.text if False else None
        ), GREEN),
    ]
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 178, 154, 170, 56, "allocate", "global allocator", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 178, 232, 170, 56, "free", "global allocator", fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 178, 210, x + 178, 232, color=GRAY)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 178, 146, 170, 56, "threads", "all hit one mutex", fill="#fff0d7", stroke=AMBER)
        vbox(doc, x + 178, 236, 170, 56, "free list", "one shared pool", fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 178, 202, x + 178, 236, color=RED)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 106, 176, 112, 56, "T0 pool", "local", fill="#f3faf3", stroke=GREEN)
        vbox(doc, x + 250, 176, 112, 56, "T1 pool", "local", fill="#f3faf3", stroke=GREEN)
        doc.text(x + 178, 256, "most alloc/free traffic stays off the shared path", size=12, color=SUBTEXT, anchor="middle")
    return make_three_panel_figure(
        "Memory pool topology",
        "Pooling helps only when the free path matches the ownership pattern of the benchmark.",
        [
            ("new/delete", "general allocator", body0, BLUE),
            ("locked pool", "shared free list", body1, RED),
            ("thread-local pool", "mostly local reuse", body2, GREEN),
        ],
        ["A pool is not automatically faster; the synchronization topology decides whether reuse pays off."],
    )


def figure_mmap_read() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 36, 152, "fd", width=48, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 88, 152, "read(4KiB)", width=82, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 176, 152, "kernel copy", width=88, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 270, 152, "user buf", width=52, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 84, 167, x + 88, 167, color=GRAY)
        arrow(doc, x + 170, 167, x + 176, 167, color=GRAY)
        arrow(doc, x + 264, 167, x + 270, 167, color=GRAY)
        chip_line(doc, x + 112, 242, "repeat for every chunk", width=180, fill="#f8e2e2", stroke=RED)
        doc.text(x + 52, 300, "timed loop keeps paying syscall entry plus copy into user memory", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 24, 152, "random page", width=72, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 104, 152, "pread(off)", width=76, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 188, 152, "page cache", width=76, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 272, 152, "user buf", width=58, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 96, 167, x + 104, 167, color=RED)
        arrow(doc, x + 180, 167, x + 188, 167, color=RED)
        arrow(doc, x + 264, 167, x + 272, 167, color=RED)
        chip_line(doc, x + 122, 242, "same syscall path, weaker locality", width=220, fill="#f8e2e2", stroke=RED)
        doc.text(x + 54, 300, "each request names a new offset, so readahead and cache-line reuse help less", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 74, 140, "mmap(fd)", width=92, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 188, 140, "VA range", width=88, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 166, 155, x + 188, 155, color=GRAY)
        chip_line(doc, x + 50, 232, "load bytes[offset]", width=118, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 180, 232, "mapped page", width=100, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 286, 232, "checksum", width=60, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 168, 247, x + 180, 247, color=TEAL)
        arrow(doc, x + 280, 247, x + 286, 247, color=TEAL)
        doc.text(x + 50, 300, "once the mapping exists, the timed loop is just ordinary loads on warm pages", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "mmap vs read / pread",
        "Warm page-cache scanning favors the path with the fewest repeated crossings between user code and the kernel.",
        [
            ("read", "sequential chunks", body0, BLUE),
            ("pread", "random chunks", body1, RED),
            ("mmap", "direct loads after mapping", body2, GREEN),
        ],
        ["The dramatic warm-path gap is mostly about syscall removal, not bypassing the page cache entirely."],
    )


def figure_memory_order() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 134, 148, 170, 52, "writer", "payload=1 ; ready=1", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 134, 236, 170, 52, "reader", "if ready==1, load payload", fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 134, 200, x + 134, 236, color=RED)
        doc.text(x + 40, 314, "relaxed allows the flag to become visible before the payload", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 134, 148, 170, 52, "release", "publish payload then flag", fill="#f3faf3", stroke=GREEN)
        vbox(doc, x + 134, 236, 170, 52, "acquire", "seeing flag pulls payload into order", fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 134, 200, x + 134, 236, color=GREEN)
        doc.text(x + 44, 314, "release/acquire fixes single-variable publication", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 78, 160, "T0: x=1", width=120, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 78, 204, "T0: r1=y", width=120, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 214, 160, "T1: y=1", width=120, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 214, 204, "T1: r2=x", width=120, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 42, 266, "only seq_cst rules out the both-zero outcome here", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "Memory-order semantics",
        "The benchmark mixes throughput tests with litmus tests because the important differences are semantic, not only micro-cost differences.",
        [
            ("relaxed", "no ordering edge", body0, RED),
            ("release/acquire", "publication edge", body1, GREEN),
            ("seq_cst", "one global order", body2, PURPLE),
        ],
        ["Use throughput tests to price the fence strength, but litmus tests to show what weaker orderings permit."],
    )


def figure_thread_placement() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        vbox(doc, x + 152, 156, 180, 54, "placement request", "hint to runtime / OS", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 152, 244, 180, 54, "verification counters", "requested? verified?", fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 152, 210, x + 152, 244, color=GRAY)
        doc.text(x + 56, 314, "benchmark now records whether the request was real", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        vbox(doc, x + 152, 160, 190, 54, "placement request = 0", "no hint accepted", fill="#f8e2e2", stroke=RED)
        vbox(doc, x + 152, 244, 190, 54, "placement verified = 0", "all three runs are baseline", fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 152, 214, x + 152, 244, color=RED)
    return make_two_panel_figure(
        "Thread placement validation",
        "Affinity-style experiments are only meaningful if the runtime actually issues and verifies a placement request.",
        "Instrumented benchmark",
        "request and verify",
        "Current macOS run",
        "request path inactive",
        left,
        right,
        ["Without self-reporting, placement benchmarks can silently measure the default scheduler path."],
    )


def figure_pipe_shm() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 40, 148, "writer", width=68, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 126, 148, "write(8B)", width=82, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 228, 148, "pipe buf", width=80, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 326, 148, "read(8B)", width=78, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 422, 148, "reader", width=66, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 108, 163, x + 126, 163, color=RED)
        arrow(doc, x + 208, 163, x + 228, 163, color=RED)
        arrow(doc, x + 308, 163, x + 326, 163, color=RED)
        arrow(doc, x + 404, 163, x + 422, 163, color=RED)
        chip_line(doc, x + 114, 242, "kernel crossing on both sides", width=214, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 328, 242, "payload copied through pipe buffer", width=176, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 54, 304, "the handoff path is write -> kernel buffer -> read for every 8-byte message", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 56, 138, "while(full!=0)", width=112, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 188, 138, "payload=value", width=112, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 320, 138, "full=1", width=72, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 76, 238, "while(full==0)", width=112, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 208, 238, "load payload", width=108, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 336, 238, "full=0", width=72, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 168, 153, x + 188, 153, color=TEAL)
        arrow(doc, x + 300, 153, x + 320, 153, color=TEAL)
        arrow(doc, x + 188, 253, x + 208, 253, color=TEAL)
        arrow(doc, x + 316, 253, x + 336, 253, color=TEAL)
        doc.line(x + 356, 174, x + 356, 230, stroke=GREEN, sw=2.2, arrow=True)
        doc.text(x + 372, 208, "flag ping-pong", size=12, color=GREEN)
        doc.text(x + 54, 304, "payload and full flag live in one shared page, so the hot path stays in userspace", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Pipe vs shared-memory handoff",
        "For tiny messages in a tight loop, avoiding a kernel round-trip often matters more than the payload copy itself.",
        "Pipe",
        "syscall path",
        "Shared mailbox",
        "userspace polling path",
        left,
        right,
        ["This benchmark isolates coordination cost, not durability or cross-process isolation requirements."],
    )


def figure_dispatch_cost() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 88, 180, "loop", width=80)
        chip_line(doc, x + 190, 180, "templated op", width=120, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 168, 195, x + 190, 195, color=TEAL)
        doc.text(x + 52, 250, "direct target known in generated code", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 90, 180, "loop", width=80)
        chip_line(doc, x + 188, 180, "fn*", width=80, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 274, 180, "target", width=74, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 170, 195, x + 188, 195, color=BLUE)
        arrow(doc, x + 268, 195, x + 274, 195, color=BLUE)
        doc.text(x + 42, 250, "one indirection, but a simple call path", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 76, 172, "obj", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 166, 172, "vptr", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 256, 172, "vtable", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 166, 224, "target fn", width=116, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 148, 187, x + 166, 187, color=AMBER)
        arrow(doc, x + 238, 187, x + 256, 187, color=AMBER)
        arrow(doc, x + 292, 202, x + 224, 224, color=RED)
        doc.text(x + 44, 276, "extra metadata loads matter when work per item is tiny", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "Dispatch cost",
        "As per-item work shrinks, each extra layer on the call path becomes visible in throughput.",
        [
            ("template", "direct code shape", body0, GREEN),
            ("function pointer", "one indirection", body1, BLUE),
            ("virtual", "object -> vptr -> vtable -> target", body2, RED),
        ],
        ["The result is not about OOP in general; it is about hot-loop dispatch overhead when the callee body is small."],
    )


def figure_mpsc_mpmc() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        for px, label in [(56, "P0"), (126, "P1"), (196, "P2"), (266, "P3")]:
            chip_line(doc, x + px, 154, label, width=52, fill="#eef6ff", stroke=BLUE)
            arrow(doc, x + px + 26, 184, x + 180, 230, color=GRAY)
        chip_line(doc, x + 144, 230, "1 consumer", width=120, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 42, 296, "4 producers funnel into one dequeue point", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        for px, label in [(56, "P0"), (126, "P1"), (196, "P2"), (266, "P3")]:
            chip_line(doc, x + px, 154, label, width=52, fill="#eef6ff", stroke=BLUE)
        for px, label in [(56, "C0"), (126, "C1"), (196, "C2"), (266, "C3")]:
            chip_line(doc, x + px, 256, label, width=52, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 124, 206, "shared queue metadata", width=156, fill="#f8e2e2", stroke=RED)
        doc.text(x + 38, 314, "multiple producers and consumers fight over the same indices", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Queue scaling topology",
        "Changing from MPSC to MPMC changes how many threads contend for enqueue and dequeue metadata.",
        "4P1C",
        "single dequeue side",
        "4P4C",
        "metadata hot on both ends",
        body0,
        body1,
        ["Topology matters enough that the same algorithm can win in one shape and lose badly in another."],
    )


def figure_blocking_spinning() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 48, 152, "T0: load turn", width=98, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 164, 152, "pause/yield", width=102, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 282, 152, "turn=1", width=64, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 94, 238, "T1 mirrors same loop", width=136, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 250, 238, "turn=0", width=64, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 146, 167, x + 164, 167, color=TEAL)
        arrow(doc, x + 266, 167, x + 282, 167, color=TEAL)
        doc.text(x + 44, 304, "both threads stay runnable and poll the same flag until ownership flips", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 48, 152, "turn!=mine", width=82, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 146, 152, "std::this_thread::yield()", width=136, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 274, 152, "scheduler", width=68, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 130, 167, x + 146, 167, color=RED)
        arrow(doc, x + 282, 167, x + 274, 167, color=RED)
        chip_line(doc, x + 114, 238, "peer runs later and retries", width=156, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 46, 304, "yield removes busy polling but now each failed handoff enters scheduler policy", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 44, 144, "lock(m)", width=66, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 126, 144, "cv.wait(turn)", width=96, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 238, 144, "sleep", width=58, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 284, 144, "notify", width=54, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 110, 159, x + 126, 159, color=RED)
        arrow(doc, x + 222, 159, x + 238, 159, color=RED)
        arrow(doc, x + 296, 159, x + 284, 159, color=RED)
        chip_line(doc, x + 94, 236, "mutex + sleep + wakeup", width=160, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 268, 236, "low CPU burn", width=76, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 44, 304, "the pure handoff loop makes lock, park, and wakeup overhead dominate the payload work", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "Blocking vs spinning",
        "This benchmark is a pure handoff loop, so wakeup path length dominates more than useful work does.",
        [
            ("busy spin", "poll turn flag", body0, BLUE),
            ("yield loop", "scheduler retry", body1, AMBER),
            ("condition variable", "sleep and notify", body2, RED),
        ],
        ["The result is throughput-focused; it does not mean spinning is the right policy for general workloads."],
    )


def figure_tlb() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        for i, px in enumerate([74, 154, 234, 314]):
            chip_line(doc, x + px, 168, f"page {i}", width=68, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 142, 184, x + 154, 184, color=TEAL)
        arrow(doc, x + 222, 184, x + 234, 184, color=TEAL)
        arrow(doc, x + 302, 184, x + 314, 184, color=TEAL)
        doc.text(x + 56, 248, "regular page walk", size=13, weight=700)
        doc.text(x + 56, 268, "TLB translations are easy to reuse", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        pts = [(82, 162), (190, 138), (294, 216), (190, 248), (88, 224)]
        for idx, (px, py) in enumerate(pts):
            chip_line(doc, x + px, py, f"page {idx}", width=72, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 154, 177, x + 190, 153, color=RED)
        arrow(doc, x + 262, 153, x + 294, 231, color=RED)
        arrow(doc, x + 330, 231, x + 190, 263, color=RED)
        arrow(doc, x + 190, 263, x + 160, 236, color=RED)
        doc.text(x + 54, 288, "random page order increases translation churn", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "TLB pressure",
        "The loop touches one value per page, so page-order and translation reuse become the visible bottleneck.",
        "Deterministic page order",
        "translation reuse",
        "Random page order",
        "translation churn",
        body0,
        body1,
        ["The benchmark is page-granular by construction, which is why TLB effects stand out earlier than bandwidth effects."],
    )


def figure_lock_variants() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 44, 144, "std::mutex", width=88, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 150, 144, "OS lock word", width=92, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 256, 144, "critical work", width=88, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 92, 234, "contended path can park a waiter", width=188, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 132, 159, x + 150, 159, color=BLUE)
        arrow(doc, x + 242, 159, x + 256, 159, color=BLUE)
        doc.text(x + 44, 302, "higher entry overhead, but waiting threads need not burn the whole critical section", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 42, 144, "atomic_flag.test_and_set", width=136, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 196, 144, "success?", width=70, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 268, 144, "critical work", width=80, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 84, 234, "losers keep polling the same flag", width=196, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 178, 159, x + 196, 159, color=RED)
        arrow(doc, x + 266, 159, x + 268, 159, color=TEAL)
        doc.text(x + 46, 302, "great for tiny uncontended work, but 4-thread contention turns wait time into wasted cycles", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 42, 134, "ticket=next.fetch_add()", width=136, fill="#efe1fb", stroke=PURPLE)
        chip_line(doc, x + 190, 134, "wait for serving==ticket", width=128, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 96, 224, "FIFO fairness", width=92, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 204, 224, "still spins", width=90, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 178, 149, x + 190, 149, color=PURPLE)
        doc.text(x + 42, 302, "ordering is cleaner than plain spinlock, but the benchmark still pays busy-wait cost as work grows", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "Lock variants",
        "Lock rankings depend not only on the primitive but also on how much time is spent inside the critical section.",
        [
            ("mutex", "heavier entry, can block", body0, BLUE),
            ("spinlock", "test-and-set loop", body1, AMBER),
            ("ticket lock", "FIFO handoff", body2, PURPLE),
        ],
        ["An uncontended microbenchmark is not enough to choose a lock for real contested work."],
    )


def figure_mmap_cow() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 72, 154, "MAP_PRIVATE", width=120, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 226, 154, "first write", width=108, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 368, 154, "COW fault", width=96, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 192, 169, x + 226, 169, color=BLUE)
        arrow(doc, x + 334, 169, x + 368, 169, color=RED)
        doc.text(x + 58, 240, "first touch allocates and copies page ownership", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 84, 154, "already dirty page", width=144, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 258, 154, "rewrite", width=88, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 54, 240, "steady-state private rewrite avoids the first COW fault", size=12, color=SUBTEXT)
        chip_line(doc, x + 112, 256, "MAP_SHARED + msync", width=172, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 316, 256, "flush dirty pages", width=132, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 284, 271, x + 316, 271, color=RED)
    return make_two_panel_figure(
        "Mapped writes",
        "The write path changes sharply depending on whether the page is private-first-touch, already dirty, or explicitly flushed for durability.",
        "Private first touch",
        "copy-on-write fault path",
        "Rewrite / shared flush",
        "different steady-state costs",
        body0,
        body1,
        ["One mapped-write benchmark is too coarse because first-touch and durability are separate costs."],
    )


def figure_cross_thread_free() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        hbox(doc, x + 58, 184, 118, 56, "producer", "allocate")
        hbox(doc, x + 212, 184, 118, 56, "slots[i]", "hand off")
        hbox(doc, x + 366, 184, 118, 56, "consumer", "free")
        arrow(doc, x + 176, 184, x + 212, 184, color=BLUE)
        arrow(doc, x + 330, 184, x + 366, 184, color=BLUE)
        doc.text(x + 52, 258, "ownership crosses threads on every object", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        hbox(doc, x + 92, 164, 156, 56, "shared pool / sync pool", "central synchronization", fill="#fff0d7", stroke=AMBER)
        hbox(doc, x + 92, 246, 156, 56, "free on another thread", "remote recycle path", fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 170, 220, x + 170, 246, color=RED)
        doc.text(x + 54, 322, "remote free turns internal synchronization into the dominant cost", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Cross-thread free",
        "Allocator designs that are fine under single-thread reuse can collapse when allocation and deallocation happen on different threads.",
        "Benchmark shape",
        "allocate on one thread, free on another",
        "Why pools struggle",
        "recycle path is no longer local",
        left,
        right,
        ["Cross-thread ownership transfer is one of the harshest allocator stress patterns."],
    )


def figure_variant_virtual() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 56, 142, "variant[i]", width=88, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 166, 142, "tag load", width=74, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 262, 142, "visit switch", width=100, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 144, 157, x + 166, 157, color=AMBER)
        arrow(doc, x + 240, 157, x + 262, 157, color=AMBER)
        chip_line(doc, x + 104, 234, "Add: acc + a", width=110, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 248, 234, "Mul: acc * m", width=110, fill="#f3faf3", stroke=GREEN)
        doc.line(x + 312, 172, x + 164, 234, stroke=BLUE, sw=2.2, arrow=True)
        doc.line(x + 312, 172, x + 304, 234, stroke=BLUE, sw=2.2, arrow=True)
        doc.text(x + 52, 304, "storage is inline, but each item still branches through the active-alternative path", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 54, 142, "virtuals[i]", width=92, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 168, 142, "ptr -> object", width=98, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 288, 142, "vptr", width=62, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 372, 142, "apply()", width=76, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 146, 157, x + 168, 157, color=BLUE)
        arrow(doc, x + 266, 157, x + 288, 157, color=BLUE)
        arrow(doc, x + 350, 157, x + 372, 157, color=BLUE)
        chip_line(doc, x + 106, 234, "AddOp objects", width=114, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 244, 234, "MulOp objects", width=114, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 56, 304, "this benchmark preallocates stable objects, so the virtual path is mostly pointer + vptr dispatch", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "variant vs virtual hierarchy",
        "After removing per-object allocation bias, the comparison reduces to the real dispatch shape each abstraction imposes.",
        "std::variant visitation",
        "tag + visit path",
        "virtual hierarchy",
        "object + virtual call path",
        left,
        right,
        ["The useful lesson is not that one abstraction always wins, but that their dispatch machinery is different enough to benchmark directly."],
    )


def figure_tcp_loopback() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        hbox(doc, x + 62, 184, 108, 56, "sender", "stream writes")
        hbox(doc, x + 202, 184, 120, 56, "TCP / Unix", "one-way stream", fill="#eef6ff", stroke=BLUE)
        hbox(doc, x + 354, 184, 108, 56, "receiver", "stream reads")
        arrow(doc, x + 170, 184, x + 202, 184, color=BLUE)
        arrow(doc, x + 322, 184, x + 354, 184, color=BLUE)
        doc.text(x + 62, 258, "one direction hides some transport differences", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 94, 154, "request", width=92, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 214, 154, "kernel path", width=112, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 354, 154, "response", width=92, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 186, 169, x + 214, 169, color=RED)
        arrow(doc, x + 326, 169, x + 354, 169, color=RED)
        chip_line(doc, x + 216, 246, "every RTT pays the path twice", width=168, fill="#f8e2e2", stroke=RED)
        doc.text(x + 54, 314, "round-trip latency amplifies transport overhead", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "TCP loopback vs Unix stream",
        "Streaming throughput and request/response latency stress different parts of the local transport path.",
        "Unidirectional stream",
        "send only",
        "Ping-pong",
        "send + wake + reply",
        left,
        right,
        ["If you only benchmark one-way throughput, you can miss the real latency ranking entirely."],
    )


def figure_page_fault_mlock() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 92, 164, "mmap", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 206, 164, "first write", width=108, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 344, 164, "page fault", width=102, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 176, 179, x + 206, 179, color=BLUE)
        arrow(doc, x + 314, 179, x + 344, 179, color=RED)
        doc.text(x + 48, 250, "first touch must back each virtual page with real memory", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 88, 156, "prefault memset", width=136, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 252, 156, "timed walk", width=112, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 224, 171, x + 252, 171, color=GREEN)
        chip_line(doc, x + 142, 246, "mlock keeps pages resident when it succeeds", width=248, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 54, 314, "the timed path no longer pays first-touch faults", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Page faults and mlock",
        "The benchmark separates first-touch fault cost from the later steady-state scan and records whether residency locking really succeeded.",
        "First touch",
        "fault on demand",
        "Prefault / mlock",
        "pay setup outside timed path",
        body0,
        body1,
        ["Always record whether mlock worked; otherwise the benchmark may claim a residency effect it never obtained."],
    )


def figure_allocator_variants() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 62, 170, "new/delete", width=100, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 188, 170, "malloc/free", width=112, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 48, 246, "general-purpose allocators balance flexibility and reuse", size=12, color=SUBTEXT)
    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 82, 156, "monotonic buffer", width=150, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 240, 156, "bump pointer", width=108, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 232, 171, x + 240, 171, color=GREEN)
        chip_line(doc, x + 104, 244, "unsynchronized_pool_resource", width=196, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 56, 314, "different PMR resources can have completely different recycling paths", size=12, color=SUBTEXT)
    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 120, 170, "arena free list", width=132, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 238, 170, "Node[] storage", width=98, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 252, 185, x + 238, 185, color=GREEN)
        doc.text(x + 48, 246, "fixed-size single-owner reuse makes the custom arena very cheap", size=12, color=SUBTEXT)
    return make_three_panel_figure(
        "Allocator variants",
        "Allocation cost is heavily shaped by how much bookkeeping and recycling policy each allocator performs.",
        [
            ("general allocators", "flexible path", body0, BLUE),
            ("PMR resources", "different reuse models", body1, AMBER),
            ("arena pool", "fixed-size local reuse", body2, GREEN),
        ],
        ["The same PMR family can land in very different regimes because the resource policy is the real mechanism."],
    )


def figure_allocator_mixed() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        for idx, label in enumerate(["32B", "64B", "128B", "256B"]):
            chip_line(doc, x + 82 + idx * 92, 176, label, width=68, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 56, 254, "mixed-size churn", size=13, weight=700)
        doc.text(x + 56, 274, "allocator sees several size classes in one run", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 108, 164, "pool bins by size", width=144, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 108, 246, "reuse within class", width=144, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 180, 220, x + 180, 246, color=GREEN)
        doc.text(x + 54, 322, "here the PMR pool matches the workload better than in the fixed-size test", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Mixed-size allocator behavior",
        "Changing the allocation-size distribution changes which internal reuse strategy matches the workload.",
        "Benchmark mix",
        "alternating size classes",
        "Pool effect",
        "reuse now aligns with those classes",
        left,
        right,
        ["Allocator conclusions are workload-shaped; the same implementation can flip from worst to best across mixes."],
    )


def figure_queue_message_size() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 88, 174, "256B msg", width=116, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 236, 174, "lock/unlock", width=116, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 384, 174, "256B msg", width=116, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 56, 258, "unbatched mutex queue pays the lock for each payload", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 88, 152, "msg x8", width=100, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 216, 152, "one lock", width=92, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 334, 152, "msg x8", width=100, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 154, 246, "SPSC already has a light control path", width=250, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 56, 314, "batching helps most when coordination cost dominates payload movement", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Queue message size and batching",
        "Larger payloads make data movement matter more, while batching changes how often the queue pays its coordination overhead.",
        "Unbatched transfer",
        "coordination per message",
        "Batched transfer",
        "amortized coordination",
        left,
        right,
        ["Batch size is an algorithmic parameter, not just a tuning footnote."],
    )


def figure_aliasing() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 84, 174, "in1[]", width=86, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 196, 174, "in2[]", width=86, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 308, 174, "out[]", width=86, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 52, 254, "potential alias only limits optimization slightly here", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 110, 166, "out == in1", width=132, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 270, 166, "read / write overlap", width=156, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 242, 181, x + 270, 181, color=RED)
        doc.text(x + 54, 246, "now the loop creates a real dependence chain", size=12, color=SUBTEXT)
        doc.text(x + 54, 266, "later iterations must wait on earlier writes", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Aliasing effects",
        "The biggest penalty in this benchmark comes from actual read/write overlap, not merely from conservative compiler assumptions.",
        "Potential alias",
        "compiler caution",
        "Real overlap",
        "data dependence",
        left,
        right,
        ["A true overlap case is essential; otherwise the benchmark mostly measures heuristics."],
    )


def figure_exception_error() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 86, 174, "return code", width=120, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 236, 174, "check path", width=108, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 58, 252, "no-fail path stays in-band", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 86, 154, "throw", width=92, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 208, 154, "stack unwind", width=124, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 364, 154, "catch", width=72, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 178, 169, x + 208, 169, color=RED)
        arrow(doc, x + 332, 169, x + 364, 169, color=RED)
        doc.text(x + 46, 264, "actual throws are expensive because control flow and unwinding become the work", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Exception vs error code",
        "Cold-path signaling is cheap when nothing fails, but actual exception throws trigger a much heavier control-flow path.",
        "Error code",
        "branch on return",
        "Exception",
        "stack unwind on failure",
        left,
        right,
        ["The cost jump is mostly about what happens on failure, not the nominal syntax of the API."],
    )


def figure_lookup() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 54, 134, "256 keys", width=80, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 154, 134, "100% hits", width=82, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 256, 134, "query stream", width=100, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 378, 134, "hot cache", width=84, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 70, 226, "hash -> bucket -> node", width=144, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 236, 226, "tree compares", width=112, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 370, 226, "binary search probes", width=136, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 46, 304, "with a tiny hot keyset, all three stay warm, but hashing still finds the target with the shortest common path", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 62, 132, "64K keys", width=84, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 166, 132, "50% misses", width=96, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 282, 132, "query ^ mask", width=104, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 406, 132, "not found often", width=108, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 72, 224, "unordered_map: hash then miss bucket/chain", width=212, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 306, 224, "sorted vector: lower_bound compares", width=190, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 148, 270, "map: tree walk with many pointer compares", width=218, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 50, 322, "misses make every structure pay more control work, which is why the map and sorted-vector gap narrows here", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "Container lookup",
        "Key-set size and miss rate matter as much as the container class name.",
        "Hot-hit set",
        "cache-friendly lookup",
        "Mixed large set",
        "misses become visible",
        left,
        right,
        ["Choose lookup structure by workload shape, not by abstract container reputation."],
    )


def figure_socket_loopback() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        hbox(doc, x + 56, 184, 118, 56, "process A", "send stream")
        hbox(doc, x + 214, 184, 118, 56, "TCP/Unix", "kernel path", fill="#fff0d7", stroke=AMBER)
        hbox(doc, x + 372, 184, 118, 56, "process B", "recv stream")
        arrow(doc, x + 174, 184, x + 214, 184, color=BLUE)
        arrow(doc, x + 332, 184, x + 372, 184, color=BLUE)
        doc.text(x + 56, 258, "streaming throughput can look similar on loopback", size=12, color=SUBTEXT)
    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 100, 154, "request", width=92, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 220, 154, "reply", width=78, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 322, 154, "round trip", width=96, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 194, 169, x + 220, 169, color=RED)
        arrow(doc, x + 298, 169, x + 322, 169, color=RED)
        doc.text(x + 52, 258, "ping-pong magnifies wakeup and scheduling latency", size=12, color=SUBTEXT)
    return make_two_panel_figure(
        "TCP loopback",
        "A one-way stream and a request/response exchange expose different parts of the same local transport stack.",
        "Streaming",
        "throughput shape",
        "Ping-pong",
        "latency shape",
        left,
        right,
        ["Transport comparisons need both throughput and round-trip views to be meaningful."],
    )


def figure_ilp() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 70, 142, "x0 load", width=78, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 172, 142, "x0=x0+1", width=88, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 284, 142, "x0=x0+1", width=88, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 396, 142, "x0 store", width=84, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 148, 157, x + 172, 157, color=RED)
        arrow(doc, x + 260, 157, x + 284, 157, color=RED)
        arrow(doc, x + 372, 157, x + 396, 157, color=RED)
        chip_line(doc, x + 122, 236, "same register chain", width=156, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 302, 236, "next add waits", width=128, fill="#f8e2e2", stroke=RED)
        doc.text(x + 52, 304, "the benchmark does two increments on the same volatile x0, so each result feeds the next instruction", size=12, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 72, 134, "x0=x0+1", width=88, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 184, 134, "x0 store", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 72, 214, "x1=x1+1", width=88, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 184, 214, "x1 store", width=84, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 160, 149, x + 184, 149, color=TEAL)
        arrow(doc, x + 160, 229, x + 184, 229, color=TEAL)
        chip_line(doc, x + 306, 164, "issue overlap", width=120, fill="#fff0d7", stroke=AMBER)
        doc.line(x + 268, 149, x + 306, 179, stroke=AMBER, sw=2.2, arrow=True)
        doc.line(x + 268, 229, x + 306, 179, stroke=AMBER, sw=2.2, arrow=True)
        doc.text(x + 54, 304, "x0 and x1 are independent, so the core can overlap both increment streams in the same loop body", size=12, color=SUBTEXT)

    return make_two_panel_figure(
        "Instruction-level parallelism",
        "Independent operations let out-of-order execution overlap more work per cycle.",
        "Dependent chain",
        "same volatile register every step",
        "Independent streams",
        "x0 and x1 can overlap",
        left,
        right,
        ["The gain comes from removing true dependencies."],
    )


def figure_branch_prediction() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 50, 140, "values[i]=1", width=88, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 156, 140, "if(values[i])", width=94, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 262, 140, "sum += 3", width=76, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 138, 155, x + 156, 155, color=GREEN)
        arrow(doc, x + 250, 155, x + 262, 155, color=GREEN)
        chip_line(doc, x + 116, 228, "1 1 1 1 1 1", width=150, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 40, 302, "same direction repeats for the whole stream", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 72, 140, "0 1 0 1 0 1", width=150, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 244, 140, "if(values[i])", width=100, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 118, 228, "sum+=1 / sum+=3", width=150, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 222, 155, x + 244, 155, color=AMBER)
        doc.text(x + 44, 302, "alternating control flow changes the path, but throughput stayed close here", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 38, 132, "pseudo-random bits", width=116, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 174, 132, "branchy", width=72, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 260, 132, "sum+=3/1", width=76, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 82, 224, "branchless: sum += 1 + (bit<<1)", width=198, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 154, 147, x + 174, 147, color=RED)
        arrow(doc, x + 246, 147, x + 260, 147, color=RED)
        doc.text(x + 38, 302, "branchless wins only modestly here; the local conclusion is a narrow band", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "Branch predictability",
        "The measured code shape did not expose a large branch-prediction gap on this machine.",
        [
            ("always taken", "steady branch direction", body0, GREEN),
            ("alternating", "direction flips every item", body1, AMBER),
            ("pseudo-random / branchless", "small local gap", body2, RED),
        ],
        ["The result is a narrow local signal, not a universal branch rule."],
    )


def figure_inlining() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 54, 148, "loop body", width=82, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 152, 148, "x = x*1664525 + 1013904223", width=170, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 136, 163, x + 152, 163, color=TEAL)
        chip_line(doc, x + 110, 236, "no separate call boundary", width=150, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 40, 302, "StepInline can be folded directly into the loop's arithmetic chain", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 48, 148, "loop body", width=76, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 142, 148, "call StepNoInline", width=112, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 268, 148, "ret", width=56, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 124, 163, x + 142, 163, color=AMBER)
        arrow(doc, x + 254, 163, x + 268, 163, color=AMBER)
        chip_line(doc, x + 106, 236, "same arithmetic body", width=150, fill="#f3faf3", stroke=GREEN)
        doc.text(x + 42, 302, "the call boundary survives, but the callee still does only one tiny arithmetic step", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 52, 148, "fn = &StepNoInline", width=136, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 208, 148, "indirect call fn(x)", width=136, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 128, 236, "target still same body", width=156, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 188, 163, x + 208, 163, color=AMBER)
        doc.text(x + 42, 302, "even the function-pointer form stays close, which is why this benchmark reports no meaningful inlining gap", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "Inlining effects",
        "Forced inline, noinline, and function-pointer shapes land in the same rough throughput band here.",
        [
            ("forced inline", "boundary disappears", body0, GREEN),
            ("forced noinline", "direct call remains", body1, AMBER),
            ("function pointer", "indirect call remains", body2, BLUE),
        ],
        ["This code shape is not very inlining-sensitive."],
    )


def figure_callable_abstraction() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 46, 148, "RunCallableBenchmark<Fn>", width=150, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 216, 148, "fn(value)", width=82, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 108, 228, "(x*13)^(x>>7)", width=140, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 196, 163, x + 216, 163, color=GREEN)
        doc.line(x + 256, 176, x + 178, 228, stroke=GREEN, sw=2.1, arrow=True)
        doc.text(x + 42, 302, "lambda and functor instantiate the template with a concrete callable type, so the hot path stays direct", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 72, 148, "fn*", width=58, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 150, 148, "call target", width=88, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 108, 228, "ApplyFn body", width=112, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 130, 163, x + 150, 163, color=BLUE)
        doc.line(x + 238, 176, x + 164, 228, stroke=AMBER, sw=2.1, arrow=True)
        chip_line(doc, x + 232, 228, "one indirection", width=104, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 48, 302, "function pointers add an indirect target load, but no type-erased wrapper state", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 42, 132, "std::function obj", width=112, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 170, 132, "erased invoker", width=96, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 282, 132, "target", width=60, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 154, 147, x + 170, 147, color=AMBER)
        arrow(doc, x + 266, 147, x + 282, 147, color=AMBER)
        chip_line(doc, x + 76, 228, "erased storage / callable state", width=172, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 264, 228, "invoke", width=68, fill="#fff0d7", stroke=AMBER)
        doc.text(x + 36, 302, "the benchmark's lambda target is simple, so the extra erased-dispatch machinery dominates the difference", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "Callable abstraction",
        "Type erasure is convenient, but it adds visible overhead in a hot loop.",
        [
            ("lambda / functor", "templated direct path", body0, GREEN),
            ("function pointer", "single indirection", body1, BLUE),
            ("std::function", "erased dispatch path", body2, RED),
        ],
        ["Type erasure is convenient, but it is not free."],
    )


def figure_clock_overhead() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 26, 142, "steady_clock::now()", width=104, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 146, 142, "chrono wrapper", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 52, 222, "monotonic timestamp source", width=152, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 130, 157, x + 146, 157, color=BLUE)
        doc.text(x + 34, 292, "C++ API wraps a lower-level clock source and then converts to duration count", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 26, 142, "system_clock::now()", width=108, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 148, 142, "chrono wrapper", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 50, 222, "wall-clock timestamp source", width=152, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 134, 157, x + 148, 157, color=BLUE)
        doc.text(x + 34, 292, "similar fixed call path, just a different underlying clock domain", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 36, 142, "clock_gettime", width=98, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 148, 142, "CLOCK_MONOTONIC[_RAW]", width=112, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 82, 222, "timespec ts", width=90, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 134, 157, x + 148, 157, color=GREEN)
        doc.text(x + 36, 292, "more direct POSIX entry point, but still a full timestamp fetch per iteration", size=12, color=SUBTEXT)

    def body3(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        chip_line(doc, x + 40, 142, "gettimeofday", width=92, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 146, 142, "timeval tv", width=76, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 58, 222, "legacy wall-clock path", width=148, fill="#fff0d7", stroke=AMBER)
        arrow(doc, x + 132, 157, x + 146, 157, color=RED)
        doc.text(x + 34, 292, "older API, but on this platform it still lands in the same rough fixed-cost band", size=12, color=SUBTEXT)

    return make_four_panel_figure(
        "Clock call overhead",
        "The benchmark compares several timestamp APIs that all end up paying a similar fixed entry cost.",
        [
            ("steady_clock", "C++ monotonic wrapper", body0, BLUE),
            ("system_clock", "C++ wall-clock wrapper", body1, BLUE),
            ("clock_gettime", "POSIX monotonic path", body2, GREEN),
            ("gettimeofday", "legacy time API", body3, RED),
        ],
        ["Clock choice matters mainly in very tight loops."],
    )


def figure_socketpair_vs_pipe() -> str:
    def left(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 54, 148, "write(fd[1])", width=102, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 178, 148, "pipe buffer", width=98, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 298, 148, "read(fd[0])", width=96, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 156, 163, x + 178, 163, color=GREEN)
        arrow(doc, x + 276, 163, x + 298, 163, color=GREEN)
        chip_line(doc, x + 122, 236, "unidirectional byte stream", width=162, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 310, 236, "narrow IPC path", width=120, fill="#eef6ff", stroke=BLUE)
        doc.text(x + 54, 304, "the benchmark just needs one-way message flow, which matches the simpler pipe abstraction", size=12, color=SUBTEXT)

    def right(doc: Svg, x: int, y: int, w: int, h: int) -> None:
        chip_line(doc, x + 48, 138, "send(sock[1])", width=98, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 164, 138, "socket endpoint", width=100, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 282, 138, "stream socket layer", width=126, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 426, 138, "recv(sock[0])", width=94, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 146, 153, x + 164, 153, color=AMBER)
        arrow(doc, x + 264, 153, x + 282, 153, color=AMBER)
        arrow(doc, x + 408, 153, x + 426, 153, color=AMBER)
        chip_line(doc, x + 154, 236, "two socket endpoints + stream semantics", width=236, fill="#f8e2e2", stroke=RED)
        doc.text(x + 48, 304, "same-host Unix stream sockets carry heavier endpoint and protocol machinery than the pipe path here", size=12, color=SUBTEXT)

    return make_two_panel_figure(
        "Pipe vs socketpair",
        "Both are kernel IPC, but the Unix stream socket pair has a heavier abstraction path here.",
        "Pipe",
        "one-way kernel buffer",
        "Unix stream socketpair",
        "two endpoints plus stream layer",
        left,
        right,
        ["This is about IPC path weight, not network semantics."],
    )


def figure_nrvo() -> str:
    doc = Svg(1200, 460, "NRVO lifecycle comparison", "Copy return materializes two objects and duplicates resources, move return materializes two objects and transfers ownership, and NRVO constructs directly in the caller result slot.")
    doc.add(
        dedent(
            """
            <style>
              .bg { fill: #f7f5ef; }
              .panel { fill: #fffdf8; stroke: #d8cfbd; stroke-width: 2; rx: 18; }
              .title { font: 700 24px Georgia, serif; fill: #24313a; }
              .subtitle { font: 500 15px Georgia, serif; fill: #52606b; }
              .label { font: 700 16px Georgia, serif; fill: #24313a; }
              .text { font: 14px Georgia, serif; fill: #34424d; }
              .small { font: 13px Georgia, serif; fill: #5d6a74; }
              .boxLocal { fill: #d7ebff; stroke: #4c87c6; stroke-width: 2; rx: 12; }
              .boxResult { fill: #dff4de; stroke: #5e9b5c; stroke-width: 2; rx: 12; }
              .boxRes { fill: #ffe5bf; stroke: #cb8b2d; stroke-width: 2; rx: 12; }
              .boxGone { fill: #f3e4e4; stroke: #b77a7a; stroke-width: 2; stroke-dasharray: 6 5; rx: 12; }
              .arrowCopy { stroke: #b04a4a; stroke-width: 3; fill: none; marker-end: url(#arrowCopy); }
              .arrowMove { stroke: #8a5a18; stroke-width: 3; fill: none; marker-end: url(#arrowMove); }
              .arrowNrvo { stroke: #2a7a65; stroke-width: 3; fill: none; marker-end: url(#arrowNrvo); }
              .arrowThin { stroke: #7b8791; stroke-width: 2; fill: none; marker-end: url(#arrowThin); }
              .legendCopy { fill: #b04a4a; }
              .legendMove { fill: #8a5a18; }
              .legendNrvo { fill: #2a7a65; }
            </style>
            <defs>
              <marker id="arrowCopy" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="#b04a4a"/>
              </marker>
              <marker id="arrowMove" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="#8a5a18"/>
              </marker>
              <marker id="arrowNrvo" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="#2a7a65"/>
              </marker>
              <marker id="arrowThin" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="#7b8791"/>
              </marker>
            </defs>
            """
        )
    )
    doc.add('<rect class="bg" x="0" y="0" width="1200" height="460"/>')
    doc.add('<text class="title" x="46" y="44">Return-by-Value Object Lifecycles</text>')
    doc.add('<text class="subtitle" x="46" y="72">Why copy return, move return, and NRVO have different cost models even when source code looks similar.</text>')
    doc.add('<rect class="panel" x="36" y="98" width="356" height="312"/>')
    doc.add('<rect class="panel" x="422" y="98" width="356" height="312"/>')
    doc.add('<rect class="panel" x="808" y="98" width="356" height="312"/>')
    # reuse the existing artwork path to keep this one accurate
    doc.add('<text class="label" x="62" y="132">1. Copy Return</text>')
    doc.add('<text class="small" x="62" y="154">Two full objects exist, and resources are duplicated.</text>')
    doc.add('<rect class="boxLocal" x="62" y="182" width="118" height="62"/>')
    doc.add('<text class="text" x="81" y="207">callee local</text>')
    doc.add('<text class="text" x="105" y="226">x</text>')
    doc.add('<rect class="boxRes" x="74" y="272" width="94" height="46"/>')
    doc.add('<text class="text" x="97" y="300">heap data A</text>')
    doc.add('<rect class="boxResult" x="236" y="182" width="118" height="62"/>')
    doc.add('<text class="text" x="258" y="207">caller result</text>')
    doc.add('<text class="text" x="286" y="226">r</text>')
    doc.add('<rect class="boxRes" x="248" y="272" width="94" height="46"/>')
    doc.add('<text class="text" x="271" y="300">heap data B</text>')
    doc.add('<path class="arrowThin" d="M 121 244 L 121 270"/>')
    doc.add('<path class="arrowThin" d="M 295 244 L 295 270"/>')
    doc.add('<path class="arrowCopy" d="M 180 213 C 202 213, 214 213, 236 213"/>')
    doc.add('<path class="arrowCopy" d="M 168 295 C 196 334, 226 334, 248 295"/>')
    doc.add('<text class="small" x="120" y="352">copy-construct result from local</text>')
    doc.add('<text class="small" x="93" y="373">source and destination own different buffers</text>')
    doc.add('<text class="label" x="448" y="132">2. Move Return</text>')
    doc.add('<text class="small" x="448" y="154">Two objects exist, but the destination steals resources.</text>')
    doc.add('<rect class="boxLocal" x="448" y="182" width="118" height="62"/>')
    doc.add('<text class="text" x="467" y="207">callee local</text>')
    doc.add('<text class="text" x="491" y="226">x</text>')
    doc.add('<rect class="boxGone" x="460" y="272" width="94" height="46"/>')
    doc.add('<text class="text" x="482" y="300">moved-from</text>')
    doc.add('<rect class="boxResult" x="622" y="182" width="118" height="62"/>')
    doc.add('<text class="text" x="644" y="207">caller result</text>')
    doc.add('<text class="text" x="672" y="226">r</text>')
    doc.add('<rect class="boxRes" x="634" y="272" width="94" height="46"/>')
    doc.add('<text class="text" x="657" y="300">heap data A</text>')
    doc.add('<path class="arrowThin" d="M 507 244 L 507 270"/>')
    doc.add('<path class="arrowThin" d="M 681 244 L 681 270"/>')
    doc.add('<path class="arrowMove" d="M 566 213 C 588 213, 600 213, 622 213"/>')
    doc.add('<path class="arrowMove" d="M 554 295 C 584 334, 612 334, 634 295"/>')
    doc.add('<text class="small" x="507" y="352">move-construct result from local</text>')
    doc.add('<text class="small" x="486" y="373">resource ownership is transferred, not duplicated</text>')
    doc.add('<text class="label" x="834" y="132">3. NRVO / Elided Return</text>')
    doc.add('<text class="small" x="834" y="154">Only one object is materialized: the local is the result slot.</text>')
    doc.add('<rect class="boxResult" x="900" y="182" width="170" height="62"/>')
    doc.add('<text class="text" x="923" y="207">caller result slot</text>')
    doc.add('<text class="text" x="922" y="226">also serves as local x</text>')
    doc.add('<rect class="boxRes" x="938" y="272" width="94" height="46"/>')
    doc.add('<text class="text" x="961" y="300">heap data A</text>')
    doc.add('<path class="arrowThin" d="M 985 244 L 985 270"/>')
    doc.add('<path class="arrowNrvo" d="M 854 213 C 882 213, 888 213, 900 213"/>')
    doc.add('<text class="small" x="846" y="202">construct directly into final slot</text>')
    doc.add('<text class="small" x="878" y="352">no copy, no move, no extra destructor for a temp</text>')
    doc.add('<text class="small" x="881" y="373">source object never exists separately</text>')
    doc.add('<rect x="44" y="426" width="14" height="14" class="legendCopy"/>')
    doc.add('<text class="small" x="66" y="438">copy path: duplicate object state and resources</text>')
    doc.add('<rect x="382" y="426" width="14" height="14" class="legendMove"/>')
    doc.add('<text class="small" x="404" y="438">move path: separate objects, transferred ownership</text>')
    doc.add('<rect x="760" y="426" width="14" height="14" class="legendNrvo"/>')
    doc.add('<text class="small" x="782" y="438">NRVO path: single object, direct construction into destination</text>')
    return doc.render()


def figure_vptr_object_lifecycle() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Plain object", size=13, weight=700)
        doc.rect(x + 46, 170, 120, 42, fill="#dff0ff", stroke=BLUE, rx=10)
        doc.text(x + 106, 196, "PlainLeaf", size=13, weight=700, anchor="middle")
        doc.rect(x + 46, 212, 120, 34, fill="#dff0ff", stroke=BLUE, rx=10)
        doc.text(x + 106, 234, "payload @ +0", size=11, weight=700, anchor="middle")
        doc.rect(x + 46, 246, 120, 34, fill="#dff0ff", stroke=BLUE, rx=10)
        doc.text(x + 106, 268, "bias @ +8", size=11, weight=700, anchor="middle")
        doc.text(x + 106, 302, "16 B total", size=12, color=SUBTEXT, anchor="middle")

        doc.text(x + 204, 148, "Polymorphic object", size=13, weight=700)
        doc.rect(x + 208, 170, 126, 42, fill="#fff0d7", stroke=AMBER, rx=10)
        doc.text(x + 271, 196, "PolyLeafA", size=13, weight=700, anchor="middle")
        doc.rect(x + 208, 212, 126, 32, fill="#f8e2e2", stroke=RED, rx=10)
        doc.text(x + 271, 233, "vptr @ +0", size=11, weight=700, anchor="middle")
        doc.rect(x + 208, 244, 126, 32, fill="#fff0d7", stroke=AMBER, rx=10)
        doc.text(x + 271, 265, "payload @ +8", size=11, weight=700, anchor="middle")
        doc.rect(x + 208, 276, 126, 32, fill="#fff0d7", stroke=AMBER, rx=10)
        doc.text(x + 271, 297, "bias @ +16", size=11, weight=700, anchor="middle")
        doc.text(x + 271, 330, "24 B total", size=12, color=SUBTEXT, anchor="middle")

        doc.text(x + 42, 366, "Measured counters:", size=12, weight=700, color=SUBTEXT)
        doc.text(x + 42, 386, "plain_bytes=16, poly_bytes=24, extra_bytes=8", size=12, color=SUBTEXT)
        doc.text(x + 42, 406, "plain_payload_off=0, poly_payload_off=8", size=12, color=SUBTEXT)
        doc.text(x + 42, 426, "Dense scan: plain 5.34G vs polymorphic 3.04G items/s", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Lifecycle probe", size=13, weight=700)
        doc.line(x + 54, 236, x + 314, 236, stroke=GRAY, sw=3)
        for cx, color in [(78, BLUE), (154, GREEN), (230, GREEN), (306, BLUE)]:
            doc.circle(cx, 236, 8, fill=color, stroke=color, sw=1)
        vbox(doc, x + 78, 286, 84, 54, "base ctor", "0xBACE", fill="#dff0ff", stroke=BLUE)
        vbox(doc, x + 154, 194, 84, 54, "derived ctor", "0xD00D", fill="#f3faf3", stroke=GREEN)
        vbox(doc, x + 230, 194, 84, 54, "derived dtor", "0xD00D", fill="#f3faf3", stroke=GREEN)
        vbox(doc, x + 306, 286, 84, 54, "base dtor", "0xBACE", fill="#dff0ff", stroke=BLUE)
        arrow(doc, x + 78, 258, x + 78, 245, color=BLUE)
        arrow(doc, x + 154, 194, x + 154, 160, color=GREEN)
        arrow(doc, x + 230, 194, x + 230, 160, color=GREEN)
        arrow(doc, x + 306, 258, x + 306, 245, color=BLUE)
        doc.text(x + 42, 368, "Measured counters:", size=12, weight=700, color=SUBTEXT)
        doc.text(x + 42, 388, "samples=4, unique_vptrs=2, vptr_switches=2", size=12, color=SUBTEXT)
        doc.text(x + 42, 408, "order: base ctor -> derived ctor -> derived dtor -> base dtor", size=12, color=SUBTEXT)
        doc.text(x + 42, 428, "Construct/destroy throughput: plain 1.216G vs virtual 1.208G items/s", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Hot-path dispatch", size=13, weight=700)
        chip_line(doc, x + 44, 174, "direct call", width=96, fill="#dff0ff", stroke=BLUE)
        chip_line(doc, x + 158, 174, "mono virtual", width=108, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 284, 174, "mixed virtual", width=112, fill="#f8e2e2", stroke=RED)
        doc.text(x + 92, 228, "3.13G items/s", size=12, anchor="middle")
        doc.text(x + 212, 228, "1.22G items/s", size=12, anchor="middle")
        doc.text(x + 340, 228, "670M items/s", size=12, anchor="middle")
        arrow(doc, x + 92, 188, x + 158, 188, color=GRAY)
        arrow(doc, x + 266, 188, x + 284, 188, color=GRAY)

        doc.text(x + 42, 288, "Call-path shape", size=13, weight=700)
        chip_line(doc, x + 44, 310, "obj", width=56, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 116, 310, "vptr", width=60, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 192, 310, "vtable", width=70, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 278, 310, "target fn", width=86, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 100, 324, x + 116, 324, color=AMBER)
        arrow(doc, x + 176, 324, x + 192, 324, color=RED)
        arrow(doc, x + 262, 324, x + 278, 324, color=BLUE)
        doc.text(x + 42, 368, "The object-side experiments are one group: footprint, lifetime rewrites, and call throughput.", size=12, color=SUBTEXT)
        doc.text(x + 42, 388, "They answer how polymorphism changes one object and one hot loop.", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "vptr object layout and lifetime",
        "These experiments stay on the object side: how a polymorphic object grows, when its active vptr changes, and how those choices show up in hot-loop throughput.",
        [
            ("1. Layout tax", "one object in memory", body0, AMBER),
            ("2. Lifetime rewrites", "active vptr changes over time", body1, GREEN),
            ("3. Dispatch cost", "call throughput on hot paths", body2, RED),
        ],
        ["This figure is only about layout, lifetime, and dispatch. It does not try to show the detailed vtable ABI view."],
        height=560,
        panel_h=390,
        footer_y=508,
    )


def figure_vtable_layout_views() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Single inheritance probe", size=13, weight=700)
        chip_line(doc, x + 42, 170, "SingleBase*", width=96, fill="#dff0ff", stroke=BLUE)
        chip_line(doc, x + 156, 170, "vptr", width=58, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 232, 170, "address point", width=104, fill="#eef6ff", stroke=BLUE)
        arrow(doc, x + 138, 184, x + 156, 184, color=BLUE)
        arrow(doc, x + 214, 184, x + 232, 184, color=RED)
        doc.rect(x + 232, 212, 104, 30, fill="#f8e2e2", stroke=RED, rx=8)
        doc.text(x + 284, 232, "offset_to_top=0", size=10, weight=700, anchor="middle")
        doc.rect(x + 232, 242, 104, 30, fill="#eef6ff", stroke=BLUE, rx=8)
        doc.text(x + 284, 262, "typeinfo*", size=10, weight=700, anchor="middle")
        doc.rect(x + 232, 272, 104, 30, fill="#f3faf3", stroke=GREEN, rx=8)
        doc.text(x + 284, 292, "slot0 = 11", size=10, weight=700, anchor="middle")
        doc.rect(x + 232, 302, 104, 30, fill="#f3faf3", stroke=GREEN, rx=8)
        doc.text(x + 284, 322, "slot1 = 22", size=10, weight=700, anchor="middle")
        doc.text(x + 42, 364, "Measured counters:", size=12, weight=700, color=SUBTEXT)
        doc.text(x + 42, 384, "offset_to_top=0, typeinfo_match=1, slot results 11 / 22", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Multiple inheritance object views", size=13, weight=700)
        doc.rect(x + 54, 176, 132, 42, fill="#fff0d7", stroke=AMBER, rx=10)
        doc.text(x + 120, 202, "LeftBase view", size=13, weight=700, anchor="middle")
        doc.text(x + 120, 224, "offset 0", size=11, color=SUBTEXT, anchor="middle")
        doc.rect(x + 206, 176, 132, 42, fill="#dff0ff", stroke=BLUE, rx=10)
        doc.text(x + 272, 202, "RightBase view", size=13, weight=700, anchor="middle")
        doc.text(x + 272, 224, "offset +8", size=11, color=SUBTEXT, anchor="middle")
        doc.rect(x + 112, 274, 168, 42, fill="#f8e2e2", stroke=RED, rx=10)
        doc.text(x + 196, 300, "MultiVtableDerived", size=13, weight=700, anchor="middle")
        arrow(doc, x + 120, 218, x + 170, 262, color=AMBER)
        arrow(doc, x + 272, 218, x + 222, 262, color=BLUE)
        doc.text(x + 42, 364, "Measured counters:", size=12, weight=700, color=SUBTEXT)
        doc.text(x + 42, 384, "left_offset=0, right_offset=-8, shared_typeinfo=1", size=12, color=SUBTEXT)
        doc.text(x + 42, 404, "The two base views share RTTI but do not share the same address point.", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Two vtable views for the same object", size=13, weight=700)
        chip_line(doc, x + 42, 170, "vptrL", width=62, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 122, 170, "offset=0", width=78, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 218, 170, "typeinfo*", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 42, 208, "31 / 32", width=76, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 104, 184, x + 122, 184, color=AMBER)
        arrow(doc, x + 200, 184, x + 218, 184, color=RED)

        chip_line(doc, x + 42, 276, "vptrR", width=62, fill="#dff0ff", stroke=BLUE)
        chip_line(doc, x + 122, 276, "offset=-8", width=82, fill="#f8e2e2", stroke=RED)
        chip_line(doc, x + 222, 276, "typeinfo*", width=84, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 42, 314, "41 / 42", width=76, fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 104, 290, x + 122, 290, color=BLUE)
        arrow(doc, x + 204, 290, x + 222, 290, color=RED)

        doc.text(x + 42, 364, "This second experiment group is pure ABI probing.", size=12, color=SUBTEXT)
        doc.text(x + 42, 384, "It answers what the vtable view looks like for single and multiple inheritance.", size=12, color=SUBTEXT)
        doc.text(x + 42, 404, "These boxes are vtable entries, not object fields.", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "vtable layout views",
        "These experiments stay on the metadata side: what the vtable address point exposes under single inheritance, and how multiple inheritance creates more than one base-subobject view.",
        [
            ("1. Single inheritance", "one vptr, one address point", body0, RED),
            ("2. Multiple inheritance views", "two base-subobject pointers", body1, BLUE),
            ("3. Multiple inheritance vtables", "same RTTI, different offsets", body2, GREEN),
        ],
        ["This figure is only about vtable layout probes. It does not try to explain object-size or lifecycle effects."],
        height=540,
        panel_h=380,
        footer_y=490,
    )


def figure_virtual_base_class() -> str:
    def body0(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        vbox(doc, x + 178, 156, 168, 44, "VirtualSharedBase", "virtual base", fill="#f8e2e2", stroke=RED)
        vbox(doc, x + 112, 246, 132, 44, "VirtualLeft", "virtual VirtualSharedBase", fill="#eef6ff", stroke=BLUE)
        vbox(doc, x + 244, 246, 136, 44, "VirtualRight", "virtual VirtualSharedBase", fill="#fff0d7", stroke=AMBER)
        vbox(doc, x + 178, 336, 178, 44, "VirtualDiamondA", "inherits left and right", fill="#f3faf3", stroke=GREEN)
        arrow(doc, x + 178, 200, x + 112, 224, color=RED)
        arrow(doc, x + 178, 200, x + 244, 224, color=RED)
        arrow(doc, x + 112, 290, x + 166, 314, color=BLUE)
        arrow(doc, x + 244, 290, x + 190, 314, color=AMBER)
        doc.text(x + 42, 404, "Inheritance only: one shared virtual base is referenced by both intermediate bases.", size=12, color=SUBTEXT)
        doc.text(x + 42, 424, "This panel is about class relationships, not byte offsets inside the final object.", size=12, color=SUBTEXT)

    def body1(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.rect(x + 86, 148, 184, 44, fill="#eef6ff", stroke=BLUE, rx=12)
        doc.text(x + 178, 166, "VirtualLeft subobject", size=13, weight=700, anchor="middle")
        doc.text(x + 178, 183, "offset +0", size=11, color=SUBTEXT, anchor="middle")
        doc.rect(x + 86, 192, 184, 44, fill="#fff0d7", stroke=AMBER, rx=12)
        doc.text(x + 178, 210, "VirtualRight subobject", size=13, weight=700, anchor="middle")
        doc.text(x + 178, 227, "offset +16", size=11, color=SUBTEXT, anchor="middle")
        doc.rect(x + 86, 236, 184, 44, fill="#f3faf3", stroke=GREEN, rx=12)
        doc.text(x + 178, 254, "VirtualDiamondA::payload", size=13, weight=700, anchor="middle")
        doc.text(x + 178, 271, "offset +32", size=11, color=SUBTEXT, anchor="middle")
        doc.rect(x + 86, 280, 184, 48, fill="#f8e2e2", stroke=RED, rx=12)
        doc.text(x + 178, 299, "VirtualSharedBase subobject", size=13, weight=700, anchor="middle")
        doc.text(x + 178, 317, "single shared copy @ +40", size=11, color=SUBTEXT, anchor="middle")
        doc.text(x + 42, 364, "Measured on this run:", size=12, weight=700, color=SUBTEXT)
        doc.text(x + 42, 384, "left_offset=0, right_offset=16, shared_from_left=40, shared_from_right=24", size=12, color=SUBTEXT)
        doc.text(x + 42, 404, "shared_alias=1 means both paths reach the same VirtualSharedBase bytes.", size=12, color=SUBTEXT)
        doc.text(x + 42, 424, "Object layout only: this panel is about subobject placement inside VirtualDiamondA.", size=12, color=SUBTEXT)

    def body2(doc: Svg, x: int, y: int, w: int, h: int, accent: str) -> None:
        doc.text(x + 42, 148, "Fast path from primary base view", size=13, weight=700)
        chip_line(doc, x + 42, 170, "VirtualLeft*", width=96, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 156, 170, "vbase delta +40", width=118, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 292, 170, "shared base", width=104, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 138, 184, x + 156, 184, color=BLUE)
        arrow(doc, x + 274, 184, x + 292, 184, color=GREEN)

        doc.text(x + 42, 244, "Recovery from secondary base view", size=13, weight=700)
        chip_line(doc, x + 42, 266, "VirtualRight*", width=108, fill="#fff0d7", stroke=AMBER)
        chip_line(doc, x + 170, 246, "offset_to_top=-16", width=140, fill="#eef6ff", stroke=BLUE)
        chip_line(doc, x + 170, 286, "vbase delta +24", width=126, fill="#f3faf3", stroke=GREEN)
        chip_line(doc, x + 320, 266, "shared base", width=104, fill="#f8e2e2", stroke=RED)
        arrow(doc, x + 150, 281, x + 170, 281, color=AMBER)
        arrow(doc, x + 310, 261, x + 320, 261, color=BLUE)
        arrow(doc, x + 296, 301, x + 320, 281, color=GREEN)
        doc.text(x + 42, 350, "Vtable-side metadata only: these chips are lookup steps, not extra object fields.", size=12, color=SUBTEXT)
        doc.text(x + 42, 370, "The secondary base view first recovers top-of-object, then finds the shared virtual base.", size=12, color=SUBTEXT)
        doc.text(x + 42, 390, "This matches the local Itanium-style model: shared vbase plus vtable-driven offset recovery.", size=12, color=SUBTEXT)

    return make_three_panel_figure(
        "Virtual base class object model",
        "Separate the ideas cleanly: inheritance introduces one shared virtual base, object layout places one copy in the most-derived object, and vtable metadata recovers it from different base views.",
        [
            ("1. Inheritance", "class graph only", body0, BLUE),
            ("2. Object layout", "subobject placement", body1, GREEN),
            ("3. vtable-side recovery", "pointer adjustment path", body2, RED),
        ],
        ["Virtual inheritance changes both layout and addressing rules; the two should not be conflated in one diagram."],
        height=560,
        panel_h=380,
        footer_y=490,
    )


FIGURES: dict[str, tuple[str, str, callable]] = {
    "01": ("stride_access.svg", "Diagram note: stride changes how many useful bytes each fetched cache line contributes.", figure_stride_access),
    "02": ("pointer_chasing.svg", "Diagram note: pointer chasing serializes the next address, so latency cannot be hidden by regular prefetching.", figure_pointer_chasing),
    "03": ("false_sharing.svg", "Diagram note: false sharing is a cache-line ownership problem, not a variable-name problem.", figure_false_sharing),
    "04": ("aos_vs_soa.svg", "Diagram note: AoS keeps unused fields in the same cache line as the hot fields, while SoA lets the loop stream only the fields it touches.", figure_aos_soa),
    "05": ("mutex_vs_atomic.svg", "Diagram note: both paths bounce the shared counter line, but the mutex path also pays lock management and wakeup overhead.", figure_mutex_atomic),
    "06": ("cache_levels.svg", "Diagram note: once the working set crosses a cache tier, the next access path is forced to pay a higher miss cost.", figure_cache_levels),
    "07": ("ilp.svg", "Diagram note: dependency chains block overlap, while independent streams let the core issue more work in parallel.", figure_ilp),
    "08": ("branch_prediction.svg", "Diagram note: this benchmark did not show a strong branch-predictor cliff on the local platform, so the picture is a narrow band rather than a dramatic swing.", figure_branch_prediction),
    "09": ("inlining.svg", "Diagram note: in this benchmark the call shapes stay close, so the main result is that inlining was not the dominant cost lever.", figure_inlining),
    "10": ("cache_associativity.svg", "Diagram note: enough capacity does not help if too many active lines map to the same set.", figure_associativity),
    "11": ("queue.svg", "Diagram note: the SPSC ring reduces coordination to split ownership of head and tail metadata, unlike the mutex queue.", figure_queue),
    "12": ("memory_pool.svg", "Diagram note: pool speed depends on whether reuse stays local or needs shared synchronization.", figure_memory_pool),
    "13": ("mmap_vs_read.svg", "Diagram note: mmap removes the per-chunk syscall path, while read and pread keep crossing the kernel boundary.", figure_mmap_read),
    "14": ("memory_order.svg", "Diagram note: throughput tests price the fence cost, but litmus tests show the actual behaviors weaker orderings permit.", figure_memory_order),
    "15": ("thread_placement.svg", "Diagram note: a placement benchmark must record whether the OS actually accepted the request.", figure_thread_placement),
    "16": ("pipe_vs_shm.svg", "Diagram note: the mailbox path avoids the kernel round-trip that the pipe must pay on each handoff.", figure_pipe_shm),
    "17": ("dispatch_cost.svg", "Diagram note: virtual dispatch adds metadata loads and an extra indirect step relative to templates or function pointers.", figure_dispatch_cost),
    "18": ("callable_abstraction.svg", "Diagram note: lightweight callable forms stay close to direct calls, while std::function adds type-erasure overhead.", figure_callable_abstraction),
    "19": ("clock_overhead.svg", "Diagram note: all clock APIs live in the same rough cost band, so the choice matters mostly in very tight loops.", figure_clock_overhead),
    "20": ("mpsc_mpmc.svg", "Diagram note: the queue topology changes which end of the structure is hot, and how many threads fight there.", figure_mpsc_mpmc),
    "21": ("blocking_vs_spinning.svg", "Diagram note: spinning stays in userspace, while blocking pays scheduler and wakeup overhead.", figure_blocking_spinning),
    "22": ("tlb_pressure.svg", "Diagram note: one value per page makes translation behavior visible before bandwidth saturates.", figure_tlb),
    "23": ("exception_vs_error.svg", "Diagram note: exception throws are expensive because they change control flow and unwind state, not just because of syntax.", figure_exception_error),
    "24": ("lock_variants.svg", "Diagram note: lock ranking depends on how much work sits inside the critical section.", figure_lock_variants),
    "25": ("mmap_cow.svg", "Diagram note: first-touch COW, rewrite, and shared flush are different costs that a single mapped-write number would blur together.", figure_mmap_cow),
    "26": ("socketpair_vs_pipe.svg", "Diagram note: the stream socket pair pays a heavier IPC path than the pipe in this benchmark.", figure_socketpair_vs_pipe),
    "27": ("cross_thread_free.svg", "Diagram note: allocation and free on different threads turn pool synchronization into the main cost.", figure_cross_thread_free),
    "28": ("variant_vs_virtual.svg", "Diagram note: after equalizing allocation bias, the comparison reduces to dispatch machinery itself.", figure_variant_virtual),
    "29": ("container_lookup.svg", "Diagram note: lookup performance depends on keyset size and miss rate, not just the container type name.", figure_lookup),
    "30": ("tcp_loopback.svg", "Diagram note: stream throughput and ping-pong latency stress different parts of the local transport path.", figure_tcp_loopback),
    "31": ("page_fault_mlock.svg", "Diagram note: first-touch, prefault, and mlock separate fault cost from steady-state access cost.", figure_page_fault_mlock),
    "32": ("vector_deque_list.svg", "Diagram note: contiguous iteration wins because it minimizes pointer chasing and cache misses.", lambda: make_three_panel_figure(
        "vector vs deque vs list",
        "Iteration cost tracks locality: contiguous storage wins, segmented storage comes next, and pointer-linked nodes lose most.",
        [
            ("vector", "contiguous", lambda d, x, y, w, h, a: (
                chip_line(d, x + 74, 184, "0 1 2 3", width=130, fill="#f3faf3", stroke=GREEN),
                d.text(x + 46, 264, "single sequential stream", size=12, color=SUBTEXT)
            ), GREEN),
            ("deque", "segmented", lambda d, x, y, w, h, a: (
                chip_line(d, x + 62, 174, "seg A", width=84, fill="#eef6ff", stroke=BLUE),
                chip_line(d, x + 170, 174, "seg B", width=84, fill="#eef6ff", stroke=BLUE),
                chip_line(d, x + 264, 174, "seg C", width=76, fill="#eef6ff", stroke=BLUE),
                d.text(x + 46, 264, "extra jumps between blocks", size=12, color=SUBTEXT)
            ), BLUE),
            ("list", "node chain", lambda d, x, y, w, h, a: (
                chip_line(d, x + 64, 184, "node -> node -> node", width=168, fill="#f8e2e2", stroke=RED),
                d.text(x + 46, 264, "pointer chasing dominates", size=12, color=SUBTEXT)
            ), RED),
        ],
        ["Scan-heavy loops are locality tests first, API tests second."],
    )),
    "33": ("allocator_variants.svg", "Diagram note: allocator policy and recycling model determine whether the hot path is a bump pointer, a free list, or a general allocator.", figure_allocator_variants),
    "34": ("allocator_mixed_size.svg", "Diagram note: changing the size mix can flip which allocator policy lines up with the workload.", figure_allocator_mixed),
    "35": ("dynamic_cast_vs_tag.svg", "Diagram note: RTTI dispatch pays for runtime type checks, while tag dispatch is a lighter branch on a known enum.", lambda: make_two_panel_figure(
        "dynamic_cast vs tag dispatch",
        "Type checks at runtime are much heavier than a simple enum-tag branch in a hot loop.",
        "Tag dispatch",
        "enum already known",
        "dynamic_cast",
        "RTTI walk",
        lambda d, x, y, w, h: (
            chip_line(d, x + 92, 180, "tag", width=80, fill="#f3faf3", stroke=GREEN),
            chip_line(d, x + 196, 180, "switch", width=92, fill="#eef6ff", stroke=BLUE),
            d.text(x + 50, 264, "one branch on a small known set", size=12, color=SUBTEXT)
        ),
        lambda d, x, y, w, h: (
            chip_line(d, x + 74, 170, "base*", width=84, fill="#eef6ff", stroke=BLUE),
            chip_line(d, x + 180, 170, "RTTI", width=74, fill="#fff0d7", stroke=AMBER),
            chip_line(d, x + 276, 170, "derived*", width=92, fill="#f8e2e2", stroke=RED),
            d.text(x + 50, 264, "repeat checks in a tight loop are costly", size=12, color=SUBTEXT)
        ),
        ["Hot dispatch loops should avoid repeated RTTI if the type set is already known."],
    )),
    "36": ("queue_message_size.svg", "Diagram note: batching changes how often the queue pays lock or metadata overhead relative to payload copying.", figure_queue_message_size),
    "37": ("aliasing_effects.svg", "Diagram note: a real output/input overlap creates the largest dependence chain in this benchmark.", figure_aliasing),
    "38": ("nrvo_lifecycle.svg", "Diagram note: prvalue return and NRVO eliminate extra object materialization, while explicit move can block that optimization.", figure_nrvo),
    "39": ("virtual_base_class.svg", "Diagram note: virtual inheritance shares one base subobject, so base-pointer views need ABI metadata to recover its address.", figure_virtual_base_class),
    "40": ("vptr_object_lifecycle.svg", "Diagram note: object-side experiments show layout tax, active-vptr rewrites during lifetime transitions, and hot-loop dispatch throughput.", figure_vptr_object_lifecycle),
    "41": ("vtable_layout_views.svg", "Diagram note: vtable-side experiments probe single-inheritance and multiple-inheritance address-point layouts separately.", figure_vtable_layout_views),
}


def inject_summary(summary_text: str) -> str:
    blocks = {
        "04": ("![AoS vs SoA](assets/plots/aos_vs_soa.svg)", FIGURES["04"][1]),
        "05": ("![Mutex vs Atomic](assets/plots/mutex_vs_atomic.svg)", FIGURES["05"][1]),
        "06": ("![Cache Levels](assets/plots/cache_levels.svg)", FIGURES["06"][1]),
    }
    out: list[str] = []
    lines = summary_text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        m = re.match(r"##\s+(\d+)\)", line)
        if m:
            key = f"{int(m.group(1)):02d}"
            if key in blocks:
                img, note = blocks[key]
                j = i + 1
                if j < len(lines) and lines[j].strip() == "":
                    out.append("")
                    i = j + 1
                else:
                    i += 1
                if i < len(lines) and lines[i].startswith("!["):
                    pass
                else:
                    out.append(img)
                    out.append("")
                    out.append(f"- {note}")
                    out.append("")
                continue
        i += 1
    return "\n".join(out) + "\n"


def main() -> None:
    for key, (name, _, fn) in FIGURES.items():
        save_svg(name, fn())
    SUMMARY.write_text(inject_summary(SUMMARY.read_text(encoding="utf-8")), encoding="utf-8")
    print(f"generated {len(FIGURES)} diagrams")


if __name__ == "__main__":
    main()
