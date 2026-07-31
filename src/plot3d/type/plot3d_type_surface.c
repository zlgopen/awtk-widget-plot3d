/**
 * File:   plot3d_type_surface.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D surface 图型插件
 *
 * Copyright (c) 2026 - 2026 Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

#include <math.h>

#include "tkc/mem.h"
#include "plot3d_type.h"

typedef struct _plot3d_type_surface_t {
  plot3d_type_t type;
} plot3d_type_surface_t;

static plot3d_type_t* plot3d_type_surface_create(void);

static color_t plot3d_type_surface_modulate_color(color_t color, float_t factor) {
  float_t f = tk_max(factor, 0.15f);
  return color_init((uint8_t)tk_min(255.0f, color.rgba.r * f),
                    (uint8_t)tk_min(255.0f, color.rgba.g * f),
                    (uint8_t)tk_min(255.0f, color.rgba.b * f), color.rgba.a);
}

static float_t plot3d_type_surface_axis_value(float_t v0, float_t v1, uint32_t index, uint32_t count) {
  return count > 1 ? v0 + (v1 - v0) * index / (count - 1) : v0;
}

static ret_t plot3d_type_surface_append_point(darray_t* points, float_t x, float_t y, float_t z,
                                               uint32_t node_index) {
  plot3d_data_point_t* p = NULL;
  return_value_if_fail(points != NULL, RET_BAD_PARAMS);

  p = TKMEM_ZALLOC(plot3d_data_point_t);
  return_value_if_fail(p != NULL, RET_OOM);

  p->x = x;
  p->y = y;
  p->z = z;
  p->node_index = node_index;
  p->color = color_init(0xff, 0xff, 0xff, 0xff);
  p->is_break = FALSE;

  return darray_push(points, p);
}

static ret_t plot3d_type_surface_layout_grid(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                                              const plot3d_source_result_t* result,
                                              darray_t* points) {
  uint32_t i = 0;
  uint32_t j = 0;
  return_value_if_fail(type != NULL && ctx != NULL && result != NULL && points != NULL,
                       RET_BAD_PARAMS);
  return_value_if_fail(result->type == PLOT3D_SOURCE_RESULT_GRID && result->zs != NULL,
                       RET_BAD_PARAMS);

  for (j = 0; j + 1 < result->rows; j++) {
    for (i = 0; i + 1 < result->cols; i++) {
      uint32_t ia = j * result->cols + i;
      uint32_t ib = ia + 1;
      uint32_t ic = (j + 1) * result->cols + i + 1;
      uint32_t id = (j + 1) * result->cols + i;
      float_t xa = plot3d_type_surface_axis_value(result->x0, result->x1, i, result->cols);
      float_t xb = plot3d_type_surface_axis_value(result->x0, result->x1, i + 1, result->cols);
      float_t ya = plot3d_type_surface_axis_value(result->y0, result->y1, j, result->rows);
      float_t yb = plot3d_type_surface_axis_value(result->y0, result->y1, j + 1, result->rows);

      return_value_if_fail(plot3d_type_surface_append_point(points, xa, ya, result->zs[ia], ia) ==
                               RET_OK,
                           RET_FAIL);
      return_value_if_fail(plot3d_type_surface_append_point(points, xb, ya, result->zs[ib], ib) ==
                               RET_OK,
                           RET_FAIL);
      return_value_if_fail(plot3d_type_surface_append_point(points, xb, yb, result->zs[ic], ic) ==
                               RET_OK,
                           RET_FAIL);
      return_value_if_fail(plot3d_type_surface_append_point(points, xa, ya, result->zs[ia], ia) ==
                               RET_OK,
                           RET_FAIL);
      return_value_if_fail(plot3d_type_surface_append_point(points, xb, yb, result->zs[ic], ic) ==
                               RET_OK,
                           RET_FAIL);
      return_value_if_fail(plot3d_type_surface_append_point(points, xa, yb, result->zs[id], id) ==
                               RET_OK,
                           RET_FAIL);
    }
  }

  return RET_OK;
}

static ret_t plot3d_type_surface_build(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                                        darray_t* primitives) {
  uint32_t segment[3];
  uint32_t segment_nr = 0;
  uint32_t i = 0;
  return_value_if_fail(type != NULL && ctx != NULL && primitives != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ctx->points_nr; i++) {
    const plot3d_projected_point_t* p = ctx->points + i;
    if (p->is_break || !p->valid) {
      segment_nr = 0;
      continue;
    }

    segment[segment_nr++] = i;
    if (segment_nr == 3) {
      const plot3d_projected_point_t* p0 = ctx->points + segment[0];
      const plot3d_projected_point_t* p1 = ctx->points + segment[1];
      const plot3d_projected_point_t* p2 = ctx->points + segment[2];
      float_t ux = p1->rx - p0->rx;
      float_t uy = p1->ry - p0->ry;
      float_t uz = p1->rz - p0->rz;
      float_t vx = p2->rx - p0->rx;
      float_t vy = p2->ry - p0->ry;
      float_t vz = p2->rz - p0->rz;
      float_t nx = uy * vz - uz * vy;
      float_t ny = uz * vx - ux * vz;
      float_t nz = ux * vy - uy * vx;
      float_t norm = sqrtf(nx * nx + ny * ny + nz * nz);
      float_t light = 0.55f;
      color_t avg = color_init((uint8_t)((p0->color.rgba.r + p1->color.rgba.r + p2->color.rgba.r) / 3),
                               (uint8_t)((p0->color.rgba.g + p1->color.rgba.g + p2->color.rgba.g) / 3),
                               (uint8_t)((p0->color.rgba.b + p1->color.rgba.b + p2->color.rgba.b) / 3),
                               (uint8_t)((p0->color.rgba.a + p1->color.rgba.a + p2->color.rgba.a) / 3));
      if (norm > 0.0001f) {
        ny /= norm;
        light = tk_min(tk_max(-ny, 0.1f), 1.0f);
      }

      return_value_if_fail(
          plot3d_type_push_primitive(primitives, PLOT3D_PRIMITIVE_TRIANGLE, segment[0],
                                      segment[1], segment[2],
                                      (p0->depth + p1->depth + p2->depth) / 3.0f,
                                      plot3d_type_surface_modulate_color(avg, light)) == RET_OK,
          RET_FAIL);
      segment_nr = 0;
    }
  }

  return RET_OK;
}

static ret_t plot3d_type_surface_set_prop(plot3d_type_t* type, const char* name,
                                           const value_t* v) {
  return plot3d_type_set_style_prop(type, name, v);
}

static ret_t plot3d_type_surface_get_prop(plot3d_type_t* type, const char* name, value_t* v) {
  return plot3d_type_get_style_prop(type, name, v);
}

static ret_t plot3d_type_surface_destroy(plot3d_type_t* type) {
  return_value_if_fail(type != NULL, RET_BAD_PARAMS);
  TKMEM_FREE(type);

  return RET_OK;
}

static const plot3d_type_vtable_t s_plot3d_type_surface_vtable = {
    .type = "surface",
    .create = plot3d_type_surface_create,
    .layout_grid = plot3d_type_surface_layout_grid,
    .build = plot3d_type_surface_build,
    .on_paint = NULL,
    .set_prop = plot3d_type_surface_set_prop,
    .get_prop = plot3d_type_surface_get_prop,
    .destroy = plot3d_type_surface_destroy};

static plot3d_type_t* plot3d_type_surface_create(void) {
  plot3d_type_surface_t* surface = TKMEM_ZALLOC(plot3d_type_surface_t);
  return_value_if_fail(surface != NULL, NULL);

  surface->type.vt = &s_plot3d_type_surface_vtable;
  plot3d_type_init_style(&(surface->type));

  return (plot3d_type_t*)surface;
}

ret_t plot3d_type_surface_register(void) {
  return plot3d_type_factory_register(&s_plot3d_type_surface_vtable);
}
