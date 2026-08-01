/**
 * File:   plot3d_paint.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 上屏绘制
 *
 * Copyright (c) 2026 - 2026 Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2026-07-31 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "base/vgcanvas.h"
#include "tkc/utils.h"
#include "plot3d_paint.h"
#include "type/plot3d_type.h"

#define PLOT3D_AXIS_EDGE_EPS 1e-6f

static plot3d_type_t* plot3d_paint_find_type(plot3d_t* plot3d, const char* type) {
  uint32_t i = 0;
  plot3d_type_t* item = NULL;
  return_value_if_fail(plot3d != NULL && type != NULL, NULL);

  for (i = 0; i < plot3d->types.size; i++) {
    item = (plot3d_type_t*)darray_get(&(plot3d->types), i);
    if (tk_str_eq(item->vt->type, type)) {
      return item;
    }
  }

  return NULL;
}

static float_t plot3d_paint_get_active_type_float(plot3d_t* plot3d, const char* name,
                                                   float_t def_value) {
  value_t v;
  plot3d_type_t* type = NULL;
  return_value_if_fail(plot3d != NULL && name != NULL, def_value);

  type = plot3d_paint_find_type(plot3d, plot3d->plottype);
  if (type == NULL || plot3d_type_get_prop(type, name, &v) != RET_OK) {
    return def_value;
  }

  return value_float(&v);
}

static color_t plot3d_modulate_color(color_t c, float_t factor) {
  color_t out = c;
  float_t f = tk_min(tk_max(factor, 0.15f), 1.0f);
  out.rgba.r = (uint8_t)(out.rgba.r * f);
  out.rgba.g = (uint8_t)(out.rgba.g * f);
  out.rgba.b = (uint8_t)(out.rgba.b * f);
  return out;
}

static ret_t plot3d_draw_line(vgcanvas_t* vg, float_t x0, float_t y0, float_t x1, float_t y1, color_t color,
                               float_t width) {
  return_value_if_fail(vg != NULL, RET_BAD_PARAMS);
  vgcanvas_set_line_width(vg, width);
  vgcanvas_set_stroke_color(vg, color);
  vgcanvas_begin_path(vg);
  vgcanvas_move_to(vg, x0, y0);
  vgcanvas_line_to(vg, x1, y1);
  return vgcanvas_stroke(vg);
}

static ret_t plot3d_vg_prepare_text(vgcanvas_t* vg, canvas_t* c) {
  return_value_if_fail(vg != NULL && c != NULL, RET_BAD_PARAMS);

  if (c->font_name != NULL) {
    vgcanvas_set_font(vg, c->font_name);
  }
  vgcanvas_set_font_size(vg, (float_t)c->font_size);
  vgcanvas_set_text_align(vg, "center");
  vgcanvas_set_text_baseline(vg, "middle");

  return RET_OK;
}

static ret_t plot3d_vg_draw_axis_text(vgcanvas_t* vg, color_t color, const char* text, float_t sx,
                                       float_t sy, float_t dir_sx, float_t dir_sy, float_t cx,
                                       float_t cy, float_t offset) {
  float_t nx = 0;
  float_t ny = 0;
  return_value_if_fail(vg != NULL && text != NULL, RET_BAD_PARAMS);

  plot3d_pick_axis_text_normal(dir_sx - sx, dir_sy - sy, sx, sy, cx, cy, &nx, &ny);

  vgcanvas_set_fill_color(vg, color);
  vgcanvas_fill_text(vg, text, sx + nx * offset, sy + ny * offset, 1000);

  return RET_OK;
}

/* 两端都用 x/y/z 三分量数组表示，画线时不用再区分是哪个轴在变。 */
static ret_t plot3d_draw_world_line(widget_t* widget, vgcanvas_t* vg,
                                     const plot3d_bounds_t* bounds, const float_t p0[3],
                                     const float_t p1[3], color_t color, float_t width) {
  plot3d_projected_point_t a;
  plot3d_projected_point_t b;

  if (plot3d_project_world_point(widget, bounds, p0[0], p0[1], p0[2], &a) &&
      plot3d_project_world_point(widget, bounds, p1[0], p1[1], p1[2], &b)) {
    return plot3d_draw_line(vg, a.sx, a.sy, b.sx, b.sy, color, width);
  }

  return RET_OK;
}

/* 一个轴上的每个刻度都拉出两条网格线，分别贴在另外两个方向的网格面上。 */
static ret_t plot3d_draw_grid_lines(widget_t* widget, vgcanvas_t* vg,
                                     const plot3d_bounds_t* bounds, plot3d_axis_t axis,
                                     const float_t origin[3], color_t color) {
  uint32_t i = 0;
  uint32_t k = 0;
  plot3d_axis_range_t range;
  plot3d_axis_t others[2] = {(plot3d_axis_t)((axis + 1) % 3), (plot3d_axis_t)((axis + 2) % 3)};
  return_value_if_fail(plot3d_bounds_get_axis(bounds, axis, &range) == RET_OK, RET_BAD_PARAMS);

  for (i = 0; i <= range.count; i++) {
    float_t v = plot3d_axis_tick_value(&range, i);

    for (k = 0; k < ARRAY_SIZE(others); k++) {
      plot3d_axis_t along = others[k];
      plot3d_axis_t fixed = others[1 - k];
      plot3d_axis_range_t along_range;
      float_t p0[3];
      float_t p1[3];

      return_value_if_fail(plot3d_bounds_get_axis(bounds, along, &along_range) == RET_OK,
                           RET_BAD_PARAMS);
      p0[axis] = p1[axis] = v;
      p0[fixed] = p1[fixed] = origin[fixed];
      p0[along] = along_range.min_v;
      p1[along] = along_range.max_v;

      plot3d_draw_world_line(widget, vg, bounds, p0, p1, color, 1);
    }
  }

  return RET_OK;
}

/* 轴线穿过网格面的交点，负半轴按亮度调暗后分段画。 */
static ret_t plot3d_draw_axis_line(widget_t* widget, vgcanvas_t* vg,
                                    const plot3d_bounds_t* bounds, plot3d_axis_t axis,
                                    const float_t origin[3], color_t color, float_t brightness) {
  uint32_t i = 0;
  uint32_t nr = 0;
  plot3d_axis_range_t range;
  plot3d_axis_color_segment_t segs[2];
  return_value_if_fail(plot3d_bounds_get_axis(bounds, axis, &range) == RET_OK, RET_BAD_PARAMS);

  nr = plot3d_build_axis_color_segments(range.min_v, range.max_v, color, brightness, segs);
  for (i = 0; i < nr; i++) {
    float_t p0[3];
    float_t p1[3];

    memcpy(p0, origin, sizeof(p0));
    memcpy(p1, origin, sizeof(p1));
    p0[axis] = segs[i].t0;
    p1[axis] = segs[i].t1;

    plot3d_draw_world_line(widget, vg, bounds, p0, p1, segs[i].color, 2);
  }

  return RET_OK;
}

ret_t plot3d_draw_grid_and_axis(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds) {
  plot3d_t* plot3d = PLOT3D(widget);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  uint32_t i = 0;
  float_t origin[3];
  color_t axis_colors[3];
  return_value_if_fail(plot3d != NULL && bounds != NULL, RET_BAD_PARAMS);
  return_value_if_fail(vg != NULL, RET_OK);

  /* 轴范围按整齐刻度规整后可能不再包含 0，网格面按网格序号对齐到等分位置。 */
  origin[PLOT3D_AXIS_X] =
      plot3d_axis_value_at_grid_index(bounds->min_x, bounds->max_x, bounds->count_x,
                                      plot3d->yz_grid_position);
  origin[PLOT3D_AXIS_Y] =
      plot3d_axis_value_at_grid_index(bounds->min_y, bounds->max_y, bounds->count_y,
                                      plot3d->xz_grid_position);
  origin[PLOT3D_AXIS_Z] =
      plot3d_axis_value_at_grid_index(bounds->min_z, bounds->max_z, bounds->count_z,
                                      plot3d->xy_grid_position);

  axis_colors[PLOT3D_AXIS_X] = plot3d->xaxis_color;
  axis_colors[PLOT3D_AXIS_Y] = plot3d->yaxis_color;
  axis_colors[PLOT3D_AXIS_Z] = plot3d->zaxis_color;

  if (plot3d->show_grid) {
    for (i = 0; i < ARRAY_SIZE(axis_colors); i++) {
      plot3d_draw_grid_lines(widget, vg, bounds, (plot3d_axis_t)i, origin, plot3d->grid_color);
    }
  }

  for (i = 0; i < ARRAY_SIZE(axis_colors); i++) {
    plot3d_draw_axis_line(widget, vg, bounds, (plot3d_axis_t)i, origin, axis_colors[i],
                           plot3d->axis_negative_brightness);
  }

  return RET_OK;
}

/* 刻度文字与轴名统一贴在包围盒的外侧棱上，不再压在轴线上。 */
static ret_t plot3d_draw_one_axis_text(widget_t* widget, canvas_t* c,
                                        const plot3d_bounds_t* bounds, plot3d_axis_t axis,
                                        const plot3d_projected_point_t* center,
                                        float_t font_size, const char* axis_name) {
  plot3d_t* plot3d = PLOT3D(widget);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  plot3d_axis_text_edge_t edge;
  plot3d_projected_point_t p0;
  plot3d_projected_point_t p1;
  plot3d_projected_point_t mid;
  plot3d_projected_point_t a;
  plot3d_axis_range_t range;
  float_t dir_sx = 0;
  float_t dir_sy = 0;
  float_t nx = 0;
  float_t ny = 0;
  float_t tick_text_w = 0;
  float_t name_offset = 0;
  uint32_t begin = 0;
  uint32_t end = 0;
  uint32_t i = 0;
  char format[8];
  char text[64];
  return_value_if_fail(plot3d != NULL && vg != NULL && bounds != NULL && center != NULL,
                       RET_BAD_PARAMS);
  /* canvas_get_vgcanvas() 会把对齐方式重置为 left/top，取到 vg 之后必须重设。 */
  plot3d_vg_prepare_text(vg, c);
  return_value_if_fail(plot3d_pick_axis_text_edge(axis, plot3d->camera_yaw, bounds->min_x,
                                                   bounds->max_x, bounds->min_y, bounds->max_y,
                                                   bounds->min_z, bounds->max_z, &edge) == RET_OK,
                       RET_BAD_PARAMS);

  return_value_if_fail(plot3d_bounds_get_axis(bounds, axis, &range) == RET_OK, RET_BAD_PARAMS);

  if (!plot3d_project_world_point(widget, bounds, edge.x0, edge.y0, edge.z0, &p0) ||
      !plot3d_project_world_point(widget, bounds, edge.x1, edge.y1, edge.z1, &p1) ||
      !plot3d_project_world_point(widget, bounds, (edge.x0 + edge.x1) * 0.5f,
                                   (edge.y0 + edge.y1) * 0.5f, (edge.z0 + edge.z1) * 0.5f, &mid)) {
    return RET_OK;
  }

  /* 棱是直线，方向恒定，末端刻度也能算出正确的外法线。 */
  dir_sx = p1.sx - p0.sx;
  dir_sy = p1.sy - p0.sy;
  plot3d_pick_axis_text_normal(dir_sx, dir_sy, mid.sx, mid.sy, center->sx, center->sy, &nx, &ny);

  tk_snprintf(format, sizeof(format), "%%.%uf", range.decimals);
  if (plot3d->show_axis_tick &&
      plot3d_calc_axis_tick_range(axis, plot3d->camera_yaw, range.count, &begin, &end) ==
          RET_OK) {
    float_t edge_len = sqrtf(dir_sx * dir_sx + dir_sy * dir_sy);
    uint32_t stride = 1;

    /* 先量出最宽的刻度文字，才能判断这条棱在屏幕上放得下多少个标签。 */
    for (i = begin; i <= end; i++) {
      tk_snprintf(text, sizeof(text), format, plot3d_axis_tick_value(&range, i));
      tick_text_w = tk_max(tick_text_w, vgcanvas_measure_text(vg, text));
    }

    if (edge_len > PLOT3D_AXIS_EDGE_EPS) {
      plot3d_calc_axis_tick_stride(edge_len, range.count, dir_sx / edge_len, dir_sy / edge_len,
                                    tick_text_w, font_size, &stride);
    }

    /* 间隔按格数起算，X 与 Y 的标签才会落在同样的刻度上，跳过的共角刻度也仍然生效。 */
    for (i = 0; i <= range.count; i += stride) {
      float_t t = plot3d_axis_tick_value(&range, i);
      float_t x = axis == PLOT3D_AXIS_X ? t : edge.x0;
      float_t y = axis == PLOT3D_AXIS_Y ? t : edge.y0;
      float_t z = axis == PLOT3D_AXIS_Z ? t : edge.z0;
      if (i < begin || i > end) {
        continue;
      }
      if (plot3d_project_world_point(widget, bounds, x, y, z, &a)) {
        float_t text_w = 0;
        float_t tick_offset = 0;
        tk_snprintf(text, sizeof(text), format, t);
        text_w = vgcanvas_measure_text(vg, text);
        /* 按各自宽度推开，靠棱那一侧的文字边缘就自然对齐了。 */
        plot3d_calc_axis_tick_offset(font_size, nx, ny, text_w, &tick_offset);
        plot3d_vg_draw_axis_text(vg, plot3d->tick_color, text, a.sx, a.sy, a.sx + dir_sx,
                                  a.sy + dir_sy, center->sx, center->sy, tick_offset);
      }
    }
  }

  if (axis_name != NULL) {
    plot3d_calc_axis_name_offset(font_size, nx, ny, tick_text_w,
                                  vgcanvas_measure_text(vg, axis_name), &name_offset);
    plot3d_vg_draw_axis_text(vg, plot3d->tick_color, axis_name, mid.sx, mid.sy, mid.sx + dir_sx,
                              mid.sy + dir_sy, center->sx, center->sy, name_offset);
  }

  return RET_OK;
}

ret_t plot3d_draw_axis_text(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds) {
  plot3d_t* plot3d = PLOT3D(widget);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  plot3d_projected_point_t center;
  float_t font_size = 0;
  return_value_if_fail(plot3d != NULL && bounds != NULL, RET_BAD_PARAMS);
  return_value_if_fail(vg != NULL, RET_OK);
  return_value_if_fail(plot3d_project_world_point(widget, bounds, bounds->center_x,
                                                   bounds->center_y, bounds->center_z, &center),
                       RET_FAIL);

  widget_prepare_text_style(widget, c);
  font_size = (float_t)c->font_size;

  plot3d_draw_one_axis_text(widget, c, bounds, PLOT3D_AXIS_X, &center, font_size,
                             plot3d->xlabel);
  plot3d_draw_one_axis_text(widget, c, bounds, PLOT3D_AXIS_Y, &center, font_size,
                             plot3d->ylabel);
  plot3d_draw_one_axis_text(widget, c, bounds, PLOT3D_AXIS_Z, &center, font_size,
                             plot3d->zlabel);

  return RET_OK;
}

ret_t plot3d_draw_data(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds) {
  plot3d_t* plot3d = PLOT3D(widget);
  vgcanvas_t* vg = canvas_get_vgcanvas(c);
  plot3d_projected_point_t* projected = (plot3d_projected_point_t*)(plot3d->projected_points);
  float_t point_size = 0;
  float_t line_width = 0;
  uint32_t i = 0;
  return_value_if_fail(plot3d != NULL && bounds != NULL, RET_BAD_PARAMS);
  return_value_if_fail(vg != NULL, RET_OK);

  point_size = plot3d_paint_get_active_type_float(plot3d, PLOT3D_PROP_POINT_SIZE, 4);
  line_width = plot3d_paint_get_active_type_float(plot3d, PLOT3D_PROP_LINE_WIDTH, 1.5f);

  for (i = 0; i < plot3d->draw_primitives.size; i++) {
    plot3d_primitive_t* primitive = (plot3d_primitive_t*)darray_get(&(plot3d->draw_primitives), i);
    if (primitive == NULL) {
      continue;
    }

    if (primitive->type == PLOT3D_PRIMITIVE_DOT) {
      plot3d_projected_point_t* p = projected + primitive->i0;
      vgcanvas_draw_circle(vg, p->sx, p->sy, point_size * 0.5f, primitive->color, TRUE, FALSE);
    } else if (primitive->type == PLOT3D_PRIMITIVE_LINE) {
      plot3d_projected_point_t* p0 = projected + primitive->i0;
      plot3d_projected_point_t* p1 = projected + primitive->i1;
      plot3d_draw_line(vg, p0->sx, p0->sy, p1->sx, p1->sy, primitive->color, line_width);
    } else if (primitive->type == PLOT3D_PRIMITIVE_TRIANGLE) {
      plot3d_projected_point_t* p0 = projected + primitive->i0;
      plot3d_projected_point_t* p1 = projected + primitive->i1;
      plot3d_projected_point_t* p2 = projected + primitive->i2;
      vgcanvas_set_fill_color(vg, primitive->color);
      vgcanvas_begin_path(vg);
      vgcanvas_move_to(vg, p0->sx, p0->sy);
      vgcanvas_line_to(vg, p1->sx, p1->sy);
      vgcanvas_line_to(vg, p2->sx, p2->sy);
      vgcanvas_close_path(vg);
      vgcanvas_fill(vg);
      vgcanvas_set_stroke_color(vg, plot3d_modulate_color(primitive->color, 0.75f));
      vgcanvas_set_line_width(vg, 1);
      vgcanvas_stroke(vg);
    } else if (primitive->type == PLOT3D_PRIMITIVE_CYLINDER) {
      plot3d_projected_point_t* p = projected + primitive->i0;
      plot3d_projected_point_t base;
      if (plot3d_project_world_point(widget, bounds, p->x, p->y, 0, &base)) {
        plot3d_draw_line(vg, p->sx, p->sy, base.sx, base.sy, primitive->color, line_width);
      }
    }
  }

  return RET_OK;
}
