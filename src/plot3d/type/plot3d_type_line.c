/**
 * File:   plot3d_type_line.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D line 图型插件
 *
 * Copyright (c) 2026 - 2026 Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

#include "tkc/mem.h"
#include "plot3d_type.h"

typedef struct _plot3d_type_line_t {
  plot3d_type_t type;
} plot3d_type_line_t;

static plot3d_type_t* plot3d_type_line_create(void);

static ret_t plot3d_type_line_append_point(darray_t* points, float_t x, float_t y, float_t z,
                                            uint32_t node_index, bool_t is_break) {
  plot3d_data_point_t* p = NULL;
  return_value_if_fail(points != NULL, RET_BAD_PARAMS);

  p = TKMEM_ZALLOC(plot3d_data_point_t);
  return_value_if_fail(p != NULL, RET_OOM);

  p->x = x;
  p->y = y;
  p->z = z;
  p->node_index = node_index;
  p->color = color_init(0xff, 0xff, 0xff, 0xff);
  p->is_break = is_break;

  return darray_push(points, p);
}

static ret_t plot3d_type_line_layout_grid(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                                           const plot3d_source_result_t* result, darray_t* points) {
  uint32_t i = 0;
  uint32_t j = 0;
  return_value_if_fail(type != NULL && ctx != NULL && result != NULL && points != NULL,
                       RET_BAD_PARAMS);
  return_value_if_fail(result->type == PLOT3D_SOURCE_RESULT_GRID && result->zs != NULL,
                       RET_BAD_PARAMS);

  for (j = 0; j < result->rows; j++) {
    float_t y = result->rows > 1 ? result->y0 + (result->y1 - result->y0) * j / (result->rows - 1)
                                 : result->y0;

    for (i = 0; i < result->cols; i++) {
      float_t x = result->cols > 1 ? result->x0 + (result->x1 - result->x0) * i / (result->cols - 1)
                                   : result->x0;
      return_value_if_fail(plot3d_type_line_append_point(points, x, y,
                                                          result->zs[j * result->cols + i],
                                                          j * result->cols + i, FALSE) == RET_OK,
                           RET_FAIL);
    }

    if ((j + 1) < result->rows) {
      return_value_if_fail(plot3d_type_line_append_point(points, 0, 0, 0, 0, TRUE) == RET_OK,
                           RET_FAIL);
    }
  }

  return RET_OK;
}

static ret_t plot3d_type_line_build(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                                     darray_t* primitives) {
  int32_t prev = -1;
  uint32_t i = 0;
  return_value_if_fail(type != NULL && ctx != NULL && primitives != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ctx->points_nr; i++) {
    const plot3d_projected_point_t* p = ctx->points + i;
    if (!p->is_break && p->valid) {
      if (prev >= 0) {
        float_t depth = (ctx->points[prev].depth + p->depth) * 0.5f;
        return_value_if_fail(plot3d_type_push_primitive(primitives, PLOT3D_PRIMITIVE_LINE,
                                                         (uint32_t)prev, i, 0, depth,
                                                         ctx->points[prev].color) == RET_OK,
                             RET_FAIL);
      }
      prev = (int32_t)i;
    } else {
      prev = -1;
    }
  }

  return RET_OK;
}

static ret_t plot3d_type_line_set_prop(plot3d_type_t* type, const char* name, const value_t* v) {
  return plot3d_type_set_style_prop(type, name, v);
}

static ret_t plot3d_type_line_get_prop(plot3d_type_t* type, const char* name, value_t* v) {
  return plot3d_type_get_style_prop(type, name, v);
}

static ret_t plot3d_type_line_destroy(plot3d_type_t* type) {
  return_value_if_fail(type != NULL, RET_BAD_PARAMS);
  TKMEM_FREE(type);

  return RET_OK;
}

static const plot3d_type_vtable_t s_plot3d_type_line_vtable = {
    .type = "line",
    .create = plot3d_type_line_create,
    .layout_grid = plot3d_type_line_layout_grid,
    .build = plot3d_type_line_build,
    .on_paint = NULL,
    .set_prop = plot3d_type_line_set_prop,
    .get_prop = plot3d_type_line_get_prop,
    .destroy = plot3d_type_line_destroy};

static plot3d_type_t* plot3d_type_line_create(void) {
  plot3d_type_line_t* line = TKMEM_ZALLOC(plot3d_type_line_t);
  return_value_if_fail(line != NULL, NULL);

  line->type.vt = &s_plot3d_type_line_vtable;
  plot3d_type_init_style(&(line->type));

  return (plot3d_type_t*)line;
}

ret_t plot3d_type_line_register(void) {
  return plot3d_type_factory_register(&s_plot3d_type_line_vtable);
}
