# plot3d 使用指南

用最简示例说明五种数据接入方式。每个示例对应一个独立程序：`demo_*_min`（仅标题 + 图表，无属性面板）。完整调参界面见 `demo_csv` / `demo_grid` / `demo_expr` / `demo_matrix` / `demo_curve`。

数据格式细节见 [数据格式](data-format.md)。

## 准备

```bash
python scripts/update_res.py all
scons
```

运行最简示例：

```bash
./bin/demo_csv_min
./bin/demo_grid_min
./bin/demo_expr_min
./bin/demo_matrix_min
./bin/demo_curve_min
```

拖拽图表可旋转；默认开启悬停读点（DataTip）。

---

## 1. CSV：`demo_csv_min`

把 CSV 文本喂给 `plot3d_set_dataset`。示例加载资源 `sample_sin_surface.csv`，`plottype=surface`。

![CSV / SinSurface](images/usage-csv.png)

```c
plot3d_register_all();
win = window_open("demo_csv_min");
chart = widget_lookup(win, "chart", TRUE);

plot3d_set_plottype(chart, "surface");
/* …外观与相机参数略，见 demos/demo_csv_min.c… */

tk_snprintf(asset_name, sizeof(asset_name), "%s.csv", "sample_sin_surface");
data_reader_asset_build_url(asset_name, ASSET_TYPE_DATA, url);
data = (char*)data_reader_read_all(url, &size);
plot3d_set_dataset(chart, data);
TKMEM_FREE(data);
```

要点：

- `dataset` 是**CSV 文本内容**，不是文件名
- 行格式 `x,y,z,color`；与 `plottype` 的排布关系见 [数据格式](data-format.md)

完整源码：[`demos/demo_csv_min.c`](../demos/demo_csv_min.c)

---

## 2. 网格函数：`demo_grid_min`

`sample_mode=grid`，用 C 回调 `z = f(x, y)` 在矩形网格上采样。示例为 ripple：`z = sin(3r) / (1 + r)`，`plottype=surface`。

![Grid / FuncRipple](images/usage-grid.png)

```c
static ret_t demo_grid_min_ripple(void* ctx, float_t x, float_t y, float_t* z) {
  float_t r = sqrtf(x * x + y * y);
  (void)ctx;
  *z = sinf(r * 3.0f) / (1.0f + r);
  return RET_OK;
}

plot3d_set_plottype(chart, "surface");
plot3d_set_sample_x_range(chart, "-6,6");
plot3d_set_sample_y_range(chart, "-6,6");
plot3d_set_sample_steps(chart, "40,40");
plot3d_set_colormap(chart, "viridis");
plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_GRID);
plot3d_set_sample_z_expr(chart, NULL); /* 用回调，不用表达式 */
plot3d_set_grid_func(chart, demo_grid_min_ripple, NULL);
```

要点：

- `sample-steps` 形如 `"列数,行数"`
- 回调与 `sample_z_expr` 二选一：设回调前把表达式清成 `NULL`

完整源码：[`demos/demo_grid_min.c`](../demos/demo_grid_min.c)

---

## 3. 表达式：`demo_expr_min`

同样是网格采样，但 `z` 用表达式字符串。示例：`sin(x) * cos(y)`。

![Expr / ExprWave](images/usage-expr.png)

```c
plot3d_set_plottype(chart, "surface");
plot3d_set_sample_x_range(chart, "-6,6");
plot3d_set_sample_y_range(chart, "-6,6");
plot3d_set_sample_steps(chart, "40,40");
plot3d_set_colormap(chart, "viridis");
plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_GRID);
plot3d_set_sample_z_expr(chart, "sin(x) * cos(y)");
plot3d_set_grid_func(chart, NULL, NULL); /* 用表达式，清掉回调 */
```

要点：

- 表达式可用 `x` `y`（以及采样上下文中的其它变量，见 [数据格式](data-format.md)）
- 也可用 `sample_color_expr` 单独控制配色

完整源码：[`demos/demo_expr_min.c`](../demos/demo_expr_min.c)

---

## 4. 矩阵：`demo_matrix_min`

`sample_mode=matrix`，直接给二维 `z` 矩阵。示例为 6×6 文本矩阵 + `plottype=cylinder`（柱状）。

![Matrix / MatrixBars](images/usage-matrix.png)

```c
plot3d_set_plottype(chart, "cylinder");
plot3d_set_xlabel(chart, "COL");
plot3d_set_ylabel(chart, "ROW");
plot3d_set_zlabel(chart, "V");
plot3d_set_line_width(chart, 14.0f);
plot3d_set_colormap(chart, "jet");
plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_MATRIX);
plot3d_set_sample_z_matrix(
    chart, "0,1,2,3,2,1;1,3,5,6,5,3;2,5,8,9,8,5;3,6,9,9,6,3;2,5,8,9,8,5;1,3,5,6,5,3");
```

要点：

- 文本矩阵：行用 `;` 分隔，列用 `,` 分隔
- 未指定范围时，x/y 取列号 / 行号
- 大矩阵也可用 `plot3d_set_z_matrix(chart, zs, cols, rows)` 直接喂 `float` 数组

完整源码：[`demos/demo_matrix_min.c`](../demos/demo_matrix_min.c)

---

## 5. 参数曲线：`demo_curve_min`

`sample_mode=curve`，按参数 `t` 走出 `(x(t), y(t), z(t))`。示例螺旋：`cos(t)`, `sin(t)`, `t/8`。

![Curve / CurveHelix](images/usage-curve.png)

```c
plot3d_set_plottype(chart, "line");
plot3d_set_sample_t_range(chart, "0,25");
plot3d_set_sample_t_steps(chart, 500);
plot3d_set_colormap(chart, "viridis");
plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_CURVE);
plot3d_set_sample_x_expr(chart, "cos(t)");
plot3d_set_sample_y_expr(chart, "sin(t)");
plot3d_set_sample_z_expr(chart, "t / 8");
plot3d_set_sample_color_expr(chart, NULL);
plot3d_set_curve_func(chart, NULL, NULL); /* 用表达式；也可用 C 回调 */
```

要点：

- `t` 范围与步数决定曲线密度
- 表达式与 `plot3d_set_curve_func` 回调二选一

完整源码：[`demos/demo_curve_min.c`](../demos/demo_curve_min.c)

---

## 通用初始化

五个最简 demo 都会先注册控件并打开同名窗口，再配置外观（网格、轴色、相机等）。最小骨架：

```c
plot3d_register_all();
win = window_open("demo_xxx_min"); /* 与 design/default/ui/demo_xxx_min.xml 同名 */
chart = widget_lookup(win, "chart", TRUE);
```

常用外观 API（节选）：

| API | 作用 |
|-----|------|
| `plot3d_set_plottype` | `dot` / `line` / `surface` / `cylinder` |
| `plot3d_set_colormap` | 如 `viridis`、`jet` |
| `plot3d_set_show_grid` / `plot3d_set_show_axis_tick` | 网格与刻度 |
| `plot3d_set_camera_yaw` / `pitch` / `distance` | 相机 |
| `plot3d_set_box_aspect_x/y/z` | 包围盒比例 |
| `plot3d_set_point_size` / `plot3d_set_line_width` | 点径 / 线宽（柱粗） |

属性也可写在 XML 的 `<plot3d …>` 上，或通过 `widget_set_prop_*` 设置。完整属性列表见控件头文件 `src/plot3d/plot3d.h`。

---

## 下一步

- 对照改最简 demo，或打开带面板的完整 demo 调参
- 深入 CSV 行格式、三角面展开、表达式配色：[数据格式](data-format.md)
