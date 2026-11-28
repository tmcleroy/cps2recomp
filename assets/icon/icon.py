#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# The CPS-2 Recomp icon, written as icon.svg: a B board shell (green, Japan's) stacked on an A board shell (blue,
# USA's), seen from above at an isometric angle, vents on the B shell, the A board's edge-connector fingers, and the
# in-game menu's dot-font 2 on a blank label. Drawn from scratch: no maker's logo, lettering or label art.
#
#   python3 assets/icon/icon.py && recomp-common/tools/make_icons.sh assets/icon/icon.svg assets/icon android/app/src/main/res
#
# The design language every port's icon follows: recomp-common/docs/ICONS.md. The SVG has only plain shapes (each
# detail put on its face here, through the face's affine map), as make_icons.sh's renderer needs.
import os

DOWN = 42  # (the art a little low of centre)


def face(a, b, c, d, e, f):
    """The affine map of a face: (x, y) in 0..1000 on it to the tile's points."""
    return lambda x, y: (a * x + c * y + e, b * x + d * y + f + DOWN)


def quad(on, x, y, w, h, fill, extra=''):
    pts = [on(x, y), on(x + w, y), on(x + w, y + h), on(x, y + h)]
    return '<polygon points="%s" fill="%s"%s/>' % (' '.join('%.1f,%.1f' % p for p in pts), fill, extra)


def poly(pts, fill):
    return '<polygon points="%s" fill="%s"/>' % (' '.join('%d,%d' % (x, y + DOWN) for x, y in pts), fill)


# The menu's 5x7 dot-font 2 (recomp-common src/text.h), as (column, row)
TWO = [(1, 0), (2, 0), (3, 0), (0, 1), (4, 1), (4, 2), (3, 3), (2, 4), (1, 5), (0, 6), (1, 6), (2, 6), (3, 6), (4, 6)]

out = ['<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">',
       '<!-- Made by icon.py: edit that, not this. -->',
       '<rect width="1024" height="1024" rx="224" fill="#0d1016"/>']
# The A board's shell: its top (mostly under the B's), its left and right faces, the fingers along the left
out.append(poly([(512, 330), (832, 490), (512, 650), (192, 490)], '#3d73c9'))
out.append(poly([(192, 490), (512, 650), (512, 740), (192, 580)], '#2a58a3'))
out.append(poly([(512, 650), (832, 490), (832, 580), (512, 740)], '#1f4682'))
a_left = face(0.32, 0.16, 0, 1, 192, 490)
out += [quad(a_left, 120 + 60 * i, 48, 28, 34, '#d9b45a') for i in range(13)]
# The B board's shell on it: the vents on its right face, the label on its top
out.append(poly([(512, 200), (832, 360), (512, 520), (192, 360)], '#4cb26e'))
out.append(poly([(192, 360), (512, 520), (512, 610), (192, 450)], '#2e8a50'))
out.append(poly([(512, 520), (832, 360), (832, 450), (512, 610)], '#236b3e'))
b_right = face(0.32, -0.16, 0, 1, 512, 520)
out += [quad(b_right, 120 + 80 * i, 24, 34, 42, '#000000', ' fill-opacity="0.32"') for i in range(9)]
b_top = face(0.32, -0.16, 0.32, 0.16, 192, 360)
out.append(quad(b_top, 220, 220, 560, 560, '#e8ecf2'))
out += [quad(b_top, 330 + 70 * c, 260 + 70 * r, 60, 60, '#0d1016') for c, r in TWO]
out.append('</svg>')

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'icon.svg'), 'w') as f:
    f.write('\n'.join(out) + '\n')
