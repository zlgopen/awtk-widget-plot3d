#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""生成 design/default/data 下的 plot3d 样例数据。

用法：python scripts/gen_sample_data.py

数据格式为每行 `x,y,z,#rrggbb`，空行表示断点（line 类型据此断开曲线）。
不同图形类型需要不同的点排布：

* dot/line/cylinder：按曲线顺序排列的点序列
* surface：三角形列表，每 3 个点组成一个独立三角形（控件不做条带复用）
"""

import math
import os

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'design', 'default',
                        'data')

# 按 z 高度取色用的渐变（viridis 风格，白底上层次清晰）。
HEAT_STOPS = ['#440154', '#3b528b', '#21918c', '#5ec962', '#fde725']

# line 类型按曲线分色。
ROW_COLORS = [
    '#f5222d', '#fa8c16', '#fadb14', '#52c41a', '#13c2c2', '#1890ff', '#722ed1', '#eb2f96'
]

TRANSPARENT = '#00000000'


def hex_to_rgb(text):
    return tuple(int(text[i:i + 2], 16) for i in (1, 3, 5))


def heat_color(t):
    """t 在 0~1 之间，返回渐变上对应的颜色。"""
    t = min(max(t, 0.0), 1.0)
    span = 1.0 / (len(HEAT_STOPS) - 1)
    idx = min(int(t / span), len(HEAT_STOPS) - 2)
    k = (t - idx * span) / span
    c0 = hex_to_rgb(HEAT_STOPS[idx])
    c1 = hex_to_rgb(HEAT_STOPS[idx + 1])
    rgb = [int(round(c0[i] + (c1[i] - c0[i]) * k)) for i in range(3)]
    return '#%02x%02x%02x' % tuple(rgb)


def fmt(v):
    text = '%.4f' % v
    text = text.rstrip('0').rstrip('.')
    return text if text not in ('', '-0') else '0'


def write_rows(name, rows):
    path = os.path.join(DATA_DIR, name + '.csv')
    lines = []
    for row in rows:
        if row is None:
            lines.append('')
        else:
            x, y, z, color = row
            lines.append('%s,%s,%s,%s' % (fmt(x), fmt(y), fmt(z), color))

    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    return [r for r in rows if r is not None]


def triangles(grid):
    """把 grid[j][i] 的四边形网格展开成三角形列表。"""
    rows = []
    for j in range(len(grid) - 1):
        for i in range(len(grid[0]) - 1):
            a = grid[j][i]
            b = grid[j][i + 1]
            c = grid[j + 1][i + 1]
            d = grid[j + 1][i]
            rows.extend([a, b, c, a, c, d])
    return rows


def sin_wave_grid():
    """x/z 平面上的 sin 曲线，多条不同 y，相位随 y 递增形成行进波。"""
    grid = []
    for j in range(8):
        row = []
        for i in range(25):
            x = i * 15.0
            y = float(j)
            z = math.sin(math.radians(x) + 0.5 * y)
            row.append((x, y, z))
        grid.append(row)
    return grid


def gen_sin_family():
    grid = sin_wave_grid()
    summary = []

    for name in ('sample_sin_dot', 'sample_sin_cylinder'):
        rows = []
        for j, row in enumerate(grid):
            if j > 0:
                rows.append(None)
            for x, y, z in row:
                rows.append((x, y, z, heat_color((z + 1.0) * 0.5)))
        summary.append((name, write_rows(name, rows)))

    rows = []
    for j, row in enumerate(grid):
        if j > 0:
            rows.append(None)
        color = ROW_COLORS[j % len(ROW_COLORS)]
        for x, y, z in row:
            rows.append((x, y, z, color))
    summary.append(('sample_sin_line', write_rows('sample_sin_line', rows)))

    rows = [(x, y, z, heat_color((z + 1.0) * 0.5)) for x, y, z in triangles(grid)]
    summary.append(('sample_sin_surface', write_rows('sample_sin_surface', rows)))

    return summary


def surface_from_func(name, func, x_range, y_range, steps, z_range):
    """按解析式采样成网格，再输出成 surface 需要的三角形列表。"""
    grid = []
    for j in range(steps):
        row = []
        for i in range(steps):
            x = x_range[0] + (x_range[1] - x_range[0]) * i / (steps - 1.0)
            y = y_range[0] + (y_range[1] - y_range[0]) * j / (steps - 1.0)
            row.append((x, y, func(x, y)))
        grid.append(row)

    span = z_range[1] - z_range[0]
    rows = [(x, y, z, heat_color((z - z_range[0]) / span)) for x, y, z in triangles(grid)]
    return (name, write_rows(name, rows))


def peaks(x, y):
    return (3.0 * (1.0 - x)**2 * math.exp(-(x**2 + (y + 1.0)**2)) - 10.0 *
            (x / 5.0 - x**3 - y**5) * math.exp(-(x**2 + y**2)) - 1.0 / 3.0 * math.exp(-(
                (x + 1.0)**2 + y**2)))


def sombrero(x, y):
    r = math.sqrt(x * x + y * y)
    return 1.0 if r < 1e-6 else math.sin(r) / r


def gen_lorenz():
    """洛伦兹吸引子：RK4 积分出蝴蝶形轨迹，颜色沿时间渐变。"""
    sigma, rho, beta = 10.0, 28.0, 8.0 / 3.0
    dt = 0.01
    nr = 2000

    def deriv(s):
        x, y, z = s
        return (sigma * (y - x), x * (rho - z) - y, x * y - beta * z)

    def add(s, d, k):
        return tuple(s[i] + d[i] * k for i in range(3))

    def step(s):
        k1 = deriv(s)
        k2 = deriv(add(s, k1, dt * 0.5))
        k3 = deriv(add(s, k2, dt * 0.5))
        k4 = deriv(add(s, k3, dt))
        return tuple(s[i] + dt / 6.0 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]) for i in range(3))

    state = (0.1, 0.0, 0.0)
    # 先跑掉从原点螺旋出去的那段瞬态，只保留已经落在吸引子上的轨迹。
    for _ in range(400):
        state = step(state)

    rows = []
    for i in range(nr):
        rows.append(state + (heat_color(i / (nr - 1.0)),))
        state = step(state)

    return ('sample_lorenz', write_rows('sample_lorenz', rows))


def gen_helix():
    """双螺旋 DNA：两条反相的螺旋链 + 若干横档。"""
    turns = 3
    steps = 120
    strand_a = []
    strand_b = []
    for i in range(steps + 1):
        t = turns * 2.0 * math.pi * i / steps
        z = turns * float(i) / steps
        strand_a.append((math.cos(t), math.sin(t), z))
        strand_b.append((math.cos(t + math.pi), math.sin(t + math.pi), z))

    rows = [p + ('#40a9ff',) for p in strand_a]
    rows.append(None)
    rows.extend(p + ('#ff7a45',) for p in strand_b)
    for i in range(0, steps + 1, 10):
        rows.append(None)
        rows.append(strand_a[i] + ('#b7eb8f',))
        rows.append(strand_b[i] + ('#b7eb8f',))

    return ('sample_helix', write_rows('sample_helix', rows))


def gen_sphere():
    """球面斐波那契点云：均匀包裹一个单位球。"""
    nr = 300
    golden = math.pi * (3.0 - math.sqrt(5.0))
    rows = []
    for i in range(nr):
        z = 1.0 - 2.0 * i / (nr - 1.0)
        r = math.sqrt(max(0.0, 1.0 - z * z))
        theta = golden * i
        rows.append((r * math.cos(theta), r * math.sin(theta), z, heat_color((z + 1.0) * 0.5)))

    return ('sample_sphere', write_rows('sample_sphere', rows))


def gen_bars():
    """5x5 柱阵：中间高四周低，四角补零高度锚点把次数轴锚在 0。"""
    rows = []
    for x in range(1, 6):
        for y in range(1, 6):
            z = 2.0 + 6.0 * math.exp(-((x - 3.0)**2 + (y - 3.0)**2) / 4.0)
            z = round(z, 1)
            rows.append((float(x), float(y), z, heat_color((z - 2.0) / 6.0)))

    for x in (1, 5):
        for y in (1, 5):
            rows.append((float(x), float(y), 0.0, TRANSPARENT))

    return ('sample_bars', write_rows('sample_bars', rows))


def gen_trefoil():
    """三叶结：闭合的环面结曲线，自交处能看出深度排序。"""
    nr = 240
    rows = []
    for i in range(nr + 1):
        t = 2.0 * math.pi * i / nr
        x = (2.0 + math.cos(3.0 * t)) * math.cos(2.0 * t)
        y = (2.0 + math.cos(3.0 * t)) * math.sin(2.0 * t)
        z = math.sin(3.0 * t)
        rows.append((x, y, z, heat_color(float(i) / nr)))

    return ('sample_trefoil', write_rows('sample_trefoil', rows))


def main():
    summary = []
    summary.extend(gen_sin_family())
    summary.append(surface_from_func('sample_peaks', peaks, (-3, 3), (-3, 3), 19, (-7, 9)))
    summary.append(surface_from_func('sample_sombrero', sombrero, (-8, 8), (-8, 8), 19, (-0.3, 1)))
    summary.append(gen_lorenz())
    summary.append(gen_helix())
    summary.append(gen_sphere())
    summary.append(gen_bars())
    summary.append(gen_trefoil())

    print('%-22s %6s %18s %18s %18s' % ('name', 'points', 'x', 'y', 'z'))
    for name, rows in summary:
        ranges = []
        for axis in range(3):
            values = [r[axis] for r in rows]
            ranges.append('%8.3f~%8.3f' % (min(values), max(values)))
        print('%-22s %6d %18s %18s %18s' % (name, len(rows), ranges[0], ranges[1], ranges[2]))


if __name__ == '__main__':
    main()
