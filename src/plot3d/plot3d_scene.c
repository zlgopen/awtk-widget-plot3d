/**
 * File:   plot3d_scene.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 场景几何与投影
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
#include <string.h>

#include "tkc/mem.h"
#include "tkc/utils.h"
#include "plot3d.h"
#include "plot3d_datatip.h"
#include "plot3d_scene.h"

#define PLOT3D_MIN_EXTENT 0.001f
/* 坐标先归一化到单位立方体，深度与透视计算才能与数据量级无关。 */
#define PLOT3D_DEPTH_BIAS 0.8f
/* 边长为 1 的立方体的对角线长度，用作盒子变形后自动缩放的基准。 */
#define PLOT3D_CUBE_DIAGONAL 1.7320508f
#define PLOT3D_NICE_STEP_EPS 1e-4f
#define PLOT3D_NICE_STEP_MAX_TRY 8
#define PLOT3D_NICE_MAX_DECIMALS 6
#define PLOT3D_AXIS_TEXT_DEFAULT_FONT_SIZE 14.0f
#define PLOT3D_AXIS_TEXT_MIN_GAP 4.0f
#define PLOT3D_AXIS_EDGE_EPS 1e-6f

ret_t plot3d_bounds_get_axis(const plot3d_bounds_t* bounds, plot3d_axis_t axis,
                              plot3d_axis_range_t* out) {
  return_value_if_fail(bounds != NULL && out != NULL, RET_BAD_PARAMS);

  if (axis == PLOT3D_AXIS_X) {
    out->min_v = bounds->min_x;
    out->max_v = bounds->max_x;
    out->extent = bounds->extent_x;
    out->count = bounds->count_x;
    out->decimals = bounds->decimals_x;
  } else if (axis == PLOT3D_AXIS_Y) {
    out->min_v = bounds->min_y;
    out->max_v = bounds->max_y;
    out->extent = bounds->extent_y;
    out->count = bounds->count_y;
    out->decimals = bounds->decimals_y;
  } else {
    out->min_v = bounds->min_z;
    out->max_v = bounds->max_z;
    out->extent = bounds->extent_z;
    out->count = bounds->count_z;
    out->decimals = bounds->decimals_z;
  }

  return RET_OK;
}

float_t plot3d_axis_tick_value(const plot3d_axis_range_t* range, uint32_t index) {
  return range->min_v + range->extent * ((float_t)index / range->count);
}

static float_t plot3d_fix_box_aspect(float_t aspect) {
  return aspect > 0 ? aspect : 1.0f;
}

ret_t plot3d_calc_axis_scales(bool_t equal_axis, float_t extent_x, float_t extent_y,
                               float_t extent_z, float_t aspect_x, float_t aspect_y,
                               float_t aspect_z, float_t* out_scale_x, float_t* out_scale_y,
                               float_t* out_scale_z) {
  float_t ex = tk_max(extent_x, PLOT3D_MIN_EXTENT);
  float_t ey = tk_max(extent_y, PLOT3D_MIN_EXTENT);
  float_t ez = tk_max(extent_z, PLOT3D_MIN_EXTENT);
  float_t ax = 1.0f;
  float_t ay = 1.0f;
  float_t az = 1.0f;
  return_value_if_fail(out_scale_x != NULL && out_scale_y != NULL && out_scale_z != NULL,
                       RET_BAD_PARAMS);

  if (equal_axis) {
    ex = ey = ez = tk_max(ex, tk_max(ey, ez));
  } else {
    float_t max_aspect = 0;
    float_t fit = 0;

    ax = plot3d_fix_box_aspect(aspect_x);
    ay = plot3d_fix_box_aspect(aspect_y);
    az = plot3d_fix_box_aspect(aspect_z);
    max_aspect = tk_max(ax, tk_max(ay, az));
    ax /= max_aspect;
    ay /= max_aspect;
    az /= max_aspect;

    fit = PLOT3D_CUBE_DIAGONAL / sqrtf(ax * ax + ay * ay + az * az);
    ax *= fit;
    ay *= fit;
    az *= fit;
  }

  *out_scale_x = ax / ex;
  *out_scale_y = ay / ey;
  *out_scale_z = az / ez;

  return RET_OK;
}

static float_t plot3d_next_nice_step(float_t step) {
  float_t mag = powf(10.0f, floorf(log10f(step) + PLOT3D_NICE_STEP_EPS));
  float_t norm = step / mag;

  if (norm < 1.5f) {
    return mag * 2.0f;
  } else if (norm < 3.5f) {
    return mag * 5.0f;
  }

  return mag * 10.0f;
}

static uint32_t plot3d_calc_nice_decimals(float_t step) {
  float_t decimals = 0;

  if (step >= 1.0f) {
    return 0;
  }

  decimals = ceilf(-log10f(step) - PLOT3D_NICE_STEP_EPS);

  return (uint32_t)tk_min(tk_max(decimals, 0.0f), (float_t)PLOT3D_NICE_MAX_DECIMALS);
}

ret_t plot3d_calc_nice_axis(float_t min_v, float_t max_v, uint32_t max_count, float_t* out_min,
                             float_t* out_max, uint32_t* out_count, uint32_t* out_decimals) {
  float_t step = 0;
  float_t nice_min = 0;
  float_t nice_max = 0;
  uint32_t count = 0;
  uint32_t i = 0;
  return_value_if_fail(out_min != NULL && out_max != NULL && out_count != NULL &&
                           out_decimals != NULL,
                       RET_BAD_PARAMS);

  max_count = tk_max(1u, max_count);
  if (max_v < min_v) {
    float_t t = min_v;
    min_v = max_v;
    max_v = t;
  }

  if ((max_v - min_v) < PLOT3D_MIN_EXTENT) {
    min_v -= 0.5f;
    max_v += 0.5f;
  }

  step = (max_v - min_v) / (float_t)max_count;
  step = powf(10.0f, floorf(log10f(step)));

  for (i = 0; i < PLOT3D_NICE_STEP_MAX_TRY; i++) {
    nice_min = floorf(min_v / step) * step;
    nice_max = ceilf(max_v / step) * step;
    count = (uint32_t)((nice_max - nice_min) / step + 0.5f);
    if (count >= 1 && count <= max_count) {
      break;
    }
    step = plot3d_next_nice_step(step);
  }

  if (count < 1) {
    nice_max = nice_min + step;
    count = 1;
  }

  *out_min = nice_min;
  *out_max = nice_max;
  *out_count = count;
  *out_decimals = plot3d_calc_nice_decimals(step);

  return RET_OK;
}

ret_t plot3d_pick_axis_text_normal(float_t dx, float_t dy, float_t sx, float_t sy, float_t cx,
                                    float_t cy, float_t* out_nx, float_t* out_ny) {
  float_t len = 0;
  float_t nx1 = 0;
  float_t ny1 = 0;
  float_t nx2 = 0;
  float_t ny2 = 0;
  float_t rx = 0;
  float_t ry = 0;
  float_t rlen = 0;
  float_t d1 = 0;
  float_t d2 = 0;
  return_value_if_fail(out_nx != NULL && out_ny != NULL, RET_BAD_PARAMS);

  len = sqrtf(dx * dx + dy * dy);
  if (len < 1e-6f) {
    dx = 1.0f;
    dy = 0.0f;
    len = 1.0f;
  }
  dx /= len;
  dy /= len;

  nx1 = -dy;
  ny1 = dx;
  nx2 = dy;
  ny2 = -dx;

  rx = sx - cx;
  ry = sy - cy;
  rlen = sqrtf(rx * rx + ry * ry);
  if (rlen < 1.0f) {
    *out_nx = nx1;
    *out_ny = ny1;
    return RET_OK;
  }

  d1 = nx1 * rx + ny1 * ry;
  d2 = nx2 * rx + ny2 * ry;
  if (d2 > d1) {
    *out_nx = nx2;
    *out_ny = ny2;
  } else {
    *out_nx = nx1;
    *out_ny = ny1;
  }

  return RET_OK;
}

static float_t plot3d_pick_edge_end(float_t min_v, float_t max_v, float_t weight) {
  return weight < -PLOT3D_AXIS_EDGE_EPS ? max_v : min_v;
}

ret_t plot3d_pick_axis_text_edge(plot3d_axis_t axis, float_t camera_yaw, float_t min_x,
                                  float_t max_x, float_t min_y, float_t max_y, float_t min_z,
                                  float_t max_z, plot3d_axis_text_edge_t* out) {
  float_t cy = cosf(camera_yaw);
  float_t sy = sinf(camera_yaw);
  return_value_if_fail(out != NULL, RET_BAD_PARAMS);

  memset(out, 0x00, sizeof(*out));
  if (axis == PLOT3D_AXIS_X) {
    out->x0 = min_x;
    out->x1 = max_x;
    out->y0 = out->y1 = plot3d_pick_edge_end(min_y, max_y, cy);
    out->z0 = out->z1 = min_z;
  } else if (axis == PLOT3D_AXIS_Y) {
    out->x0 = out->x1 = plot3d_pick_edge_end(min_x, max_x, sy);
    out->y0 = min_y;
    out->y1 = max_y;
    out->z0 = out->z1 = min_z;
  } else if (axis == PLOT3D_AXIS_Z) {
    out->x0 = out->x1 = plot3d_pick_edge_end(min_x, max_x, cy);
    out->y0 = out->y1 = plot3d_pick_edge_end(min_y, max_y, -sy);
    out->z0 = min_z;
    out->z1 = max_z;
  } else {
    return RET_BAD_PARAMS;
  }

  return RET_OK;
}

static float_t plot3d_fix_font_size(float_t font_size) {
  return font_size < 1.0f ? PLOT3D_AXIS_TEXT_DEFAULT_FONT_SIZE : font_size;
}

static float_t plot3d_calc_axis_text_gap(float_t font_size) {
  return tk_max(font_size * 0.4f, PLOT3D_AXIS_TEXT_MIN_GAP);
}

static float_t plot3d_calc_axis_text_radius(float_t nx, float_t ny, float_t text_w,
                                             float_t font_size) {
  return (tk_abs(nx) * text_w + tk_abs(ny) * font_size) * 0.5f;
}

ret_t plot3d_calc_axis_tick_offset(float_t font_size, float_t nx, float_t ny, float_t text_w,
                                    float_t* out_offset) {
  return_value_if_fail(out_offset != NULL, RET_BAD_PARAMS);

  font_size = plot3d_fix_font_size(font_size);
  *out_offset = plot3d_calc_axis_text_gap(font_size) +
                plot3d_calc_axis_text_radius(nx, ny, text_w, font_size);

  return RET_OK;
}

ret_t plot3d_calc_axis_name_offset(float_t font_size, float_t nx, float_t ny, float_t tick_text_w,
                                    float_t name_text_w, float_t* out_offset) {
  float_t gap = 0;
  return_value_if_fail(out_offset != NULL, RET_BAD_PARAMS);

  font_size = plot3d_fix_font_size(font_size);
  gap = plot3d_calc_axis_text_gap(font_size);
  *out_offset = gap + 2.0f * plot3d_calc_axis_text_radius(nx, ny, tick_text_w, font_size) + gap +
                plot3d_calc_axis_text_radius(nx, ny, name_text_w, font_size);

  return RET_OK;
}

static uint32_t plot3d_fit_tick_stride(uint32_t stride, uint32_t count) {
  uint32_t i = 0;

  for (i = stride; i <= count && i <= stride * 2; i++) {
    if ((count % i) == 0) {
      return i;
    }
  }

  return stride;
}

ret_t plot3d_calc_axis_tick_stride(float_t edge_len, uint32_t count, float_t ux, float_t uy,
                                    float_t text_w, float_t font_size, uint32_t* out_stride) {
  float_t need = 0;
  float_t available = 0;
  uint32_t stride = 1;
  return_value_if_fail(out_stride != NULL, RET_BAD_PARAMS);

  *out_stride = 1;
  if (count == 0 || edge_len <= 0) {
    return RET_OK;
  }

  font_size = plot3d_fix_font_size(font_size);
  need = 2.0f * plot3d_calc_axis_text_radius(ux, uy, text_w, font_size) +
         plot3d_calc_axis_text_gap(font_size);
  available = edge_len / (float_t)count;
  if (available >= need) {
    return RET_OK;
  }

  stride = (uint32_t)ceilf(need / available);
  stride = tk_min(stride, count);
  *out_stride = plot3d_fit_tick_stride(stride, count);

  return RET_OK;
}

ret_t plot3d_calc_axis_tick_range(plot3d_axis_t axis, float_t camera_yaw, uint32_t count,
                                   uint32_t* out_begin, uint32_t* out_end) {
  return_value_if_fail(out_begin != NULL && out_end != NULL && count > 0, RET_BAD_PARAMS);

  *out_begin = 0;
  *out_end = count;
  if (axis == PLOT3D_AXIS_Y) {
    if (cosf(camera_yaw) < -PLOT3D_AXIS_EDGE_EPS) {
      *out_end = count - 1;
    } else {
      *out_begin = 1;
    }
  }

  return RET_OK;
}

ret_t plot3d_calc_bounds(plot3d_t* plot3d, plot3d_bounds_t* bounds) {
  uint32_t i = 0;
  return_value_if_fail(plot3d != NULL && bounds != NULL, RET_BAD_PARAMS);

  memset(bounds, 0x00, sizeof(*bounds));
  for (i = 0; i < plot3d->data_points.size; i++) {
    plot3d_data_point_t* p = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), i);
    if (p == NULL || p->is_break) {
      continue;
    }

    if (!bounds->valid) {
      bounds->min_x = bounds->max_x = p->x;
      bounds->min_y = bounds->max_y = p->y;
      bounds->min_z = bounds->max_z = p->z;
      bounds->valid = TRUE;
    } else {
      bounds->min_x = tk_min(bounds->min_x, p->x);
      bounds->min_y = tk_min(bounds->min_y, p->y);
      bounds->min_z = tk_min(bounds->min_z, p->z);
      bounds->max_x = tk_max(bounds->max_x, p->x);
      bounds->max_y = tk_max(bounds->max_y, p->y);
      bounds->max_z = tk_max(bounds->max_z, p->z);
    }
  }

  if (!bounds->valid) {
    bounds->min_x = bounds->min_y = bounds->min_z = -1;
    bounds->max_x = bounds->max_y = bounds->max_z = 1;
    bounds->valid = TRUE;
  }

  plot3d_calc_nice_axis(bounds->min_x, bounds->max_x, plot3d->x_grid_count, &(bounds->min_x),
                         &(bounds->max_x), &(bounds->count_x), &(bounds->decimals_x));
  plot3d_calc_nice_axis(bounds->min_y, bounds->max_y, plot3d->y_grid_count, &(bounds->min_y),
                         &(bounds->max_y), &(bounds->count_y), &(bounds->decimals_y));
  plot3d_calc_nice_axis(bounds->min_z, bounds->max_z, plot3d->z_grid_count, &(bounds->min_z),
                         &(bounds->max_z), &(bounds->count_z), &(bounds->decimals_z));

  bounds->center_x = (bounds->min_x + bounds->max_x) * 0.5f;
  bounds->center_y = (bounds->min_y + bounds->max_y) * 0.5f;
  bounds->center_z = (bounds->min_z + bounds->max_z) * 0.5f;
  bounds->extent_x = tk_max(bounds->max_x - bounds->min_x, PLOT3D_MIN_EXTENT);
  bounds->extent_y = tk_max(bounds->max_y - bounds->min_y, PLOT3D_MIN_EXTENT);
  bounds->extent_z = tk_max(bounds->max_z - bounds->min_z, PLOT3D_MIN_EXTENT);
  bounds->max_extent = tk_max(bounds->extent_x, tk_max(bounds->extent_y, bounds->extent_z));

  return plot3d_calc_axis_scales(plot3d->equal_axis, bounds->extent_x, bounds->extent_y,
                                  bounds->extent_z, plot3d->box_aspect_x, plot3d->box_aspect_y,
                                  plot3d->box_aspect_z, &(bounds->scale_x), &(bounds->scale_y),
                                  &(bounds->scale_z));
}

bool_t plot3d_project_world_point(widget_t* widget, const plot3d_bounds_t* bounds, float_t x,
                                   float_t y, float_t z, plot3d_projected_point_t* out) {
  plot3d_t* plot3d = PLOT3D(widget);
  float_t yaw = 0;
  float_t pitch = 0;
  float_t cy = 0;
  float_t sy = 0;
  float_t cp = 0;
  float_t sp = 0;
  float_t nx = 0;
  float_t ny = 0;
  float_t nz = 0;
  float_t rx = 0;
  float_t ry = 0;
  float_t rz = 0;
  float_t depth = 0;
  float_t k = 0;
  float_t radius = 0;
  float_t center_x = 0;
  float_t center_y = 0;
  float_t scale = 0;
  float_t delta = 0;
  float_t depth_bias = 0;
  float_t zoom_factor = 1.0f;
  float_t perspective_strength = 0.35f;

  return_value_if_fail(plot3d != NULL && bounds != NULL && out != NULL, FALSE);

  yaw = plot3d->camera_yaw;
  pitch = plot3d->camera_pitch;
  cy = cosf(yaw);
  sy = sinf(yaw);
  cp = cosf(pitch);
  sp = sinf(pitch);

  nx = (x - bounds->center_x) * bounds->scale_x;
  ny = (y - bounds->center_y) * bounds->scale_y;
  nz = (z - bounds->center_z) * bounds->scale_z;

  rx = nx * cy - ny * sy;
  ry = nx * sy + ny * cy;
  rz = nz;

  ny = ry * cp - rz * sp;
  nz = ry * sp + rz * cp;
  ry = ny;
  rz = nz;

  depth_bias = PLOT3D_DEPTH_BIAS;
  depth = ry + plot3d->camera_distance + depth_bias;
  if (depth < 0.05f) {
    return FALSE;
  }

  radius = tk_min(widget->w, widget->h) * 0.40f;
  zoom_factor = 4.5f / plot3d->camera_distance;
  zoom_factor = tk_min(tk_max(zoom_factor, 0.6f), 2.4f);
  scale = radius * zoom_factor;
  delta = (depth - (plot3d->camera_distance + depth_bias)) /
          tk_max(plot3d->camera_distance + depth_bias, 0.1f);
  k = 1.0f / (1.0f + delta * perspective_strength);
  k = tk_min(tk_max(k, 0.90f), 1.10f);
  center_x = (float_t)widget->x + widget->w * 0.5f;
  center_y = (float_t)widget->y + widget->h * 0.5f;

  out->x = x;
  out->y = y;
  out->z = z;
  out->rx = rx;
  out->ry = ry;
  out->rz = rz;
  out->sx = center_x + rx * scale * k;
  out->sy = center_y - (rz + plot3d->camera_z_offset) * scale * k;
  out->depth = depth;
  out->valid = TRUE;

  return TRUE;
}

ret_t plot3d_update_projection_cache(widget_t* widget, const plot3d_bounds_t* bounds) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_projected_point_t* projected = NULL;
  uint32_t i = 0;

  return_value_if_fail(plot3d != NULL && bounds != NULL, RET_BAD_PARAMS);
  if (plot3d->enable_cache && !plot3d->projection_cache_dirty) {
    return RET_OK;
  }

  if (plot3d->projected_points_nr != plot3d->data_points.size) {
    plot3d->projected_points =
        TKMEM_REALLOC(plot3d->projected_points, sizeof(plot3d_projected_point_t) * plot3d->data_points.size);
    plot3d->projected_points_nr = plot3d->projected_points == NULL ? 0 : plot3d->data_points.size;
  }

  projected = (plot3d_projected_point_t*)(plot3d->projected_points);
  return_value_if_fail(projected != NULL || plot3d->data_points.size == 0, RET_OOM);

  for (i = 0; i < plot3d->data_points.size; i++) {
    plot3d_data_point_t* p = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), i);
    plot3d_projected_point_t* pp = projected + i;
    memset(pp, 0x00, sizeof(*pp));
    if (p == NULL) {
      continue;
    }

    pp->is_break = p->is_break;
    pp->color = p->color;
    if (!p->is_break) {
      plot3d_project_world_point(widget, bounds, p->x, p->y, p->z, pp);
      pp->color = p->color;
    }
  }

  plot3d_datatip_sanitize_hover(widget);

  plot3d->projection_cache_dirty = FALSE;
  return RET_OK;
}

int plot3d_primitive_compare(const void* a, const void* b) {
  const plot3d_primitive_t* pa = *(const plot3d_primitive_t* const*)a;
  const plot3d_primitive_t* pb = *(const plot3d_primitive_t* const*)b;
  if (pa->depth < pb->depth) {
    return 1;
  }
  if (pa->depth > pb->depth) {
    return -1;
  }
  return 0;
}
