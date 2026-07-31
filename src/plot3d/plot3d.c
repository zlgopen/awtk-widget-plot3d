/**
 * File:   plot3d.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D
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
 * 2026-07-26 Li XianJing <xianjimli@hotmail.com> created
 *
 */


#include <math.h>
#include <stdio.h>
#include <string.h>

#include "base/vgcanvas.h"
#include "tkc/color_parser.h"
#include "tkc/log.h"
#include "tkc/mem.h"
#include "tkc/utils.h"
#include "plot3d.h"
#include "plot3d_colormap.h"
#include "plot3d_datatip.h"
#include "plot3d_paint.h"
#include "plot3d_scene.h"
#include "colorizer/plot3d_colorizer.h"
#include "source/plot3d_source.h"
#include "type/plot3d_type.h"

#define PLOT3D_MIN_BOX_ASPECT 0.05f
#define PLOT3D_MAX_BOX_ASPECT 20.0f
#define PLOT3D_MIN_GRID_COUNT 1u
#define PLOT3D_MAX_GRID_COUNT 50u
static plot3d_type_t* plot3d_find_type(plot3d_t* plot3d, const char* type);

static ret_t plot3d_mark_cache_dirty(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  plot3d->projection_cache_dirty = TRUE;
  plot3d->draw_cache_dirty = TRUE;
  widget_invalidate(widget, NULL);

  return RET_OK;
}

static ret_t plot3d_data_point_destroy(void* data) {
  TKMEM_FREE(data);
  return RET_OK;
}

static ret_t plot3d_primitive_destroy(void* data) {
  TKMEM_FREE(data);
  return RET_OK;
}

static ret_t plot3d_source_destroy_wrapper(void* data) {
  return plot3d_source_destroy((plot3d_source_t*)data);
}

static ret_t plot3d_colorizer_destroy_wrapper(void* data) {
  return plot3d_colorizer_destroy((plot3d_colorizer_t*)data);
}

static ret_t plot3d_type_destroy_wrapper(void* data) {
  return plot3d_type_destroy((plot3d_type_t*)data);
}

static ret_t plot3d_append_data_point_internal(plot3d_t* plot3d, float_t x, float_t y, float_t z,
                                                color_t color, bool_t is_break) {
  plot3d_data_point_t* point = NULL;
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  point = TKMEM_ZALLOC(plot3d_data_point_t);
  return_value_if_fail(point != NULL, RET_OOM);

  point->x = x;
  point->y = y;
  point->z = z;
  point->color = color;
  point->is_break = is_break;
  point->node_index = 0;

  return darray_push(&(plot3d->data_points), point);
}

ret_t plot3d_sample_pos_init(plot3d_sample_pos_t* pos, float_t x, float_t y, float_t z) {
  return_value_if_fail(pos != NULL, RET_BAD_PARAMS);

  memset(pos, 0x00, sizeof(*pos));
  pos->x = x;
  pos->y = y;
  pos->z = z;

  return RET_OK;
}

ret_t plot3d_parse_range(const char* text, float_t* out_min, float_t* out_max) {
  const char* comma = NULL;
  float_t a = 0;
  float_t b = 0;
  return_value_if_fail(out_min != NULL && out_max != NULL, RET_BAD_PARAMS);
  return_value_if_fail(text != NULL && *text != '\0', RET_BAD_PARAMS);

  comma = strchr(text, ',');
  return_value_if_fail(comma != NULL, RET_BAD_PARAMS);

  a = (float_t)tk_atof(text);
  b = (float_t)tk_atof(comma + 1);
  *out_min = tk_min(a, b);
  *out_max = tk_max(a, b);

  return RET_OK;
}

static uint32_t plot3d_fix_sample_steps(int32_t steps) {
  if (steps < (int32_t)PLOT3D_MIN_SAMPLE_STEPS) {
    return PLOT3D_MIN_SAMPLE_STEPS;
  }

  return tk_min((uint32_t)steps, PLOT3D_MAX_SAMPLE_STEPS);
}

ret_t plot3d_parse_steps(const char* text, uint32_t* out_cols, uint32_t* out_rows) {
  const char* comma = NULL;
  return_value_if_fail(out_cols != NULL && out_rows != NULL, RET_BAD_PARAMS);
  return_value_if_fail(text != NULL && *text != '\0', RET_BAD_PARAMS);

  comma = strchr(text, ',');
  *out_cols = plot3d_fix_sample_steps(tk_atoi(text));
  *out_rows = comma != NULL ? plot3d_fix_sample_steps(tk_atoi(comma + 1)) : *out_cols;

  return RET_OK;
}

static float_t plot3d_sample_axis_value(float_t v0, float_t v1, uint32_t index, uint32_t count) {
  return count > 1 ? v0 + (v1 - v0) * index / (count - 1) : v0;
}

/* 交给第一个生效的配色插件，没有插件生效时保持按 z 在配色表上取色。 */
static ret_t plot3d_eval_color_value(plot3d_t* plot3d, const plot3d_sample_pos_t* pos,
                                      plot3d_color_value_t* out) {
  uint32_t i = 0;
  plot3d_colorizer_t* colorizer = NULL;
  return_value_if_fail(plot3d != NULL && pos != NULL && out != NULL, RET_BAD_PARAMS);

  out->is_color = FALSE;
  out->scalar = pos->z;

  for (i = 0; i < plot3d->colorizers.size; i++) {
    colorizer = (plot3d_colorizer_t*)darray_get(&(plot3d->colorizers), i);
    if (plot3d_colorizer_is_active(colorizer)) {
      return plot3d_colorizer_eval(colorizer, pos, out);
    }
  }

  return RET_OK;
}

static color_t plot3d_color_value_to_color(plot3d_t* plot3d, const plot3d_color_value_t* cv,
                                           float_t min_v, float_t max_v) {
  color_t color = color_init(255, 255, 255, 255);

  if (cv->is_color) {
    return cv->color;
  }

  {
    float_t t = max_v > min_v ? (cv->scalar - min_v) / (max_v - min_v) : 0;
    plot3d_colormap_get_color(plot3d->colormap, t, &color);
  }

  return color;
}

/* 归一化用的区间只统计标量点：已经定了颜色的点不参与。 */
static ret_t plot3d_track_color_range(const plot3d_color_value_t* cv, uint32_t* scalar_nr,
                                       float_t* min_v, float_t* max_v) {
  if (cv->is_color) {
    return RET_OK;
  }

  if (*scalar_nr == 0) {
    *min_v = *max_v = cv->scalar;
  } else {
    *min_v = tk_min(*min_v, cv->scalar);
    *max_v = tk_max(*max_v, cv->scalar);
  }
  (*scalar_nr)++;

  return RET_OK;
}

static ret_t plot3d_fill_grid_point_colors(plot3d_t* plot3d, const plot3d_color_value_t* cvs,
                                            uint32_t cv_nr, float_t min_v, float_t max_v) {
  uint32_t i = 0;
  return_value_if_fail(plot3d != NULL && cvs != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->data_points.size; i++) {
    plot3d_data_point_t* point = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), i);
    if (point->is_break) {
      continue;
    }

    return_value_if_fail(point->node_index < cv_nr, RET_FAIL);
    point->color = plot3d_color_value_to_color(plot3d, cvs + point->node_index, min_v, max_v);
  }

  return RET_OK;
}

/* 把 cols x rows 的 z 网格按 plottype 铺成数据点，网格采样与矩阵输入共用。 */
static ret_t plot3d_emit_grid(plot3d_t* plot3d, float_t x0, float_t x1, float_t y0, float_t y1,
                               uint32_t cols, uint32_t rows, const float_t* zs) {
  float_t min_v = 0;
  float_t max_v = 0;
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t scalar_nr = 0;
  plot3d_color_value_t* cvs = NULL;
  plot3d_type_t* type = NULL;
  return_value_if_fail(plot3d != NULL && zs != NULL && cols > 0 && rows > 0, RET_BAD_PARAMS);

  cvs = TKMEM_ZALLOCN(plot3d_color_value_t, cols * rows);
  return_value_if_fail(cvs != NULL, RET_OOM);

  for (j = 0; j < rows; j++) {
    float_t y = plot3d_sample_axis_value(y0, y1, j, rows);

    for (i = 0; i < cols; i++) {
      float_t x = plot3d_sample_axis_value(x0, x1, i, cols);
      uint32_t index = j * cols + i;
      plot3d_sample_pos_t pos;

      /* 网格上没有曲线参数 t，has_t 保持 FALSE。 */
      plot3d_sample_pos_init(&pos, x, y, zs[index]);
      plot3d_eval_color_value(plot3d, &pos, cvs + index);
      plot3d_track_color_range(cvs + index, &scalar_nr, &min_v, &max_v);
    }
  }

  type = plot3d_find_type(plot3d, plot3d->plottype);
  if (type == NULL) {
    TKMEM_FREE(cvs);
    return RET_FAIL;
  }

  {
    plot3d_paint_ctx_t ctx;
    plot3d_source_result_t result;
    memset(&ctx, 0x00, sizeof(ctx));
    plot3d_source_result_init(&result, NULL);
    result.type = PLOT3D_SOURCE_RESULT_GRID;
    result.zs = zs;
    result.cols = cols;
    result.rows = rows;
    result.x0 = x0;
    result.x1 = x1;
    result.y0 = y0;
    result.y1 = y1;

    if (plot3d_type_layout_grid(type, &ctx, &result, &(plot3d->data_points)) != RET_OK ||
        plot3d_fill_grid_point_colors(plot3d, cvs, cols * rows, min_v, max_v) != RET_OK) {
      TKMEM_FREE(cvs);
      return RET_FAIL;
    }
  }

  TKMEM_FREE(cvs);

  return RET_OK;
}

/* 数据源实例在 plot3d_create 时按注册表建全，这里按类型名取出其中一个。
 * 返回 NULL 表示该插件没注册（裁剪场景），由调用方决定如何处置。 */
static plot3d_source_t* plot3d_find_source(plot3d_t* plot3d, const char* type) {
  uint32_t i = 0;
  plot3d_source_t* source = NULL;

  for (i = 0; i < plot3d->sources.size; i++) {
    source = (plot3d_source_t*)darray_get(&(plot3d->sources), i);
    if (tk_str_eq(source->vt->type, type)) {
      return source;
    }
  }

  return NULL;
}

static plot3d_type_t* plot3d_find_type(plot3d_t* plot3d, const char* type) {
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

/* 取当前该出数据的那个数据源：数据源的类型名与采样模式同名，模式对不上、插件没注册（裁剪场景）
 * 或还没喂数据时都返回 NULL，调用方据此跳过。
 *
 * 先比模式再查实例：数据源全部迁完后每次重采样只需要问一个插件，而不是把四个都查一遍。
 * 也不能把没查到的 NULL 直接交给 plot3d_source_has_data——那会在每次重采样时刷一条参数校验
 * 日志，而裁剪掉某个插件是正常配置。 */
static plot3d_source_t* plot3d_find_active_source(plot3d_t* plot3d, const char* mode) {
  plot3d_source_t* source = NULL;

  if (!tk_str_eq(plot3d->sample_mode, mode)) {
    return NULL;
  }

  source = plot3d_find_source(plot3d, mode);

  return (source != NULL && plot3d_source_has_data(source)) ? source : NULL;
}

/* GRID 数据源只负责产出 z 网格；按 plottype 铺点由当前图型插件 layout_grid 完成。 */
static ret_t plot3d_sample_grid_result(plot3d_t* plot3d, plot3d_source_t* source) {
  plot3d_source_result_t result;
  return_value_if_fail(plot3d != NULL && source != NULL, RET_BAD_PARAMS);

  plot3d_source_result_init(&result, NULL);
  return_value_if_fail(plot3d_source_sample(source, &result) == RET_OK, RET_FAIL);
  return_value_if_fail(result.type == PLOT3D_SOURCE_RESULT_GRID, RET_FAIL);

  return plot3d_emit_grid(plot3d, result.x0, result.x1, result.y0, result.y1, result.cols,
                           result.rows, result.zs);
}

/* 给已落入 data_points 的 POINTS 补色：has_color 为 FALSE 时走这里。有 ts 时带上 t。 */
static ret_t plot3d_colorize_points(plot3d_t* plot3d, const plot3d_source_result_t* result) {
  float_t min_v = 0;
  float_t max_v = 0;
  uint32_t i = 0;
  uint32_t nr = 0;
  uint32_t scalar_nr = 0;
  plot3d_color_value_t* cvs = NULL;
  return_value_if_fail(plot3d != NULL && result != NULL, RET_BAD_PARAMS);

  nr = plot3d->data_points.size;
  if (nr == 0) {
    return RET_OK;
  }

  cvs = TKMEM_ZALLOCN(plot3d_color_value_t, nr);
  return_value_if_fail(cvs != NULL, RET_OOM);

  for (i = 0; i < nr; i++) {
    plot3d_data_point_t* p = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), i);
    plot3d_sample_pos_t pos;

    if (p == NULL || p->is_break) {
      continue;
    }

    plot3d_sample_pos_init(&pos, p->x, p->y, p->z);
    if (result->ts != NULL) {
      pos.t = result->ts[i];
      pos.has_t = TRUE;
    }
    plot3d_eval_color_value(plot3d, &pos, cvs + i);
    plot3d_track_color_range(cvs + i, &scalar_nr, &min_v, &max_v);
  }

  for (i = 0; i < nr; i++) {
    plot3d_data_point_t* p = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), i);
    const plot3d_color_value_t* cv = cvs + i;

    if (p == NULL || p->is_break) {
      continue;
    }

    if (cv->is_color) {
      p->color = cv->color;
    } else {
      float_t t = max_v > min_v ? (cv->scalar - min_v) / (max_v - min_v) : 0;

      plot3d_colormap_get_color(plot3d->colormap, t, &(p->color));
    }
  }

  TKMEM_FREE(cvs);

  return RET_OK;
}

/* POINTS 接入：csv（自带颜色）与 curve（需配色，可带 ts）共用。点由插件直接 push 进
 * plot3d->data_points，元素的销毁由本数组的 destroy 负责。 */
static ret_t plot3d_sample_points(plot3d_t* plot3d, plot3d_source_t* source) {
  plot3d_source_result_t result;
  return_value_if_fail(plot3d != NULL && source != NULL, RET_BAD_PARAMS);

  plot3d_source_result_init(&result, &(plot3d->data_points));
  return_value_if_fail(plot3d_source_sample(source, &result) == RET_OK, RET_FAIL);
  return_value_if_fail(result.type == PLOT3D_SOURCE_RESULT_POINTS, RET_FAIL);

  if (result.has_color) {
    return RET_OK;
  }

  return plot3d_colorize_points(plot3d, &result);
}

/* 采样条件变化后重新生成数据点。 */
static ret_t plot3d_resample(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_source_t* matrix_source = NULL;
  plot3d_source_t* csv_source = NULL;
  plot3d_source_t* curve_source = NULL;
  plot3d_source_t* grid_source = NULL;
  bool_t grid_now = FALSE;
  bool_t curve_now = FALSE;
  bool_t matrix_now = FALSE;
  bool_t none_now = FALSE;
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  /* 计数放在提前返回之前：要钉住的是「重采样被触发了几次」，与本次是否真有数据可采无关。 */
  plot3d->resample_nr_for_test++;

  /* 必须按 sample_mode 门控：设 z-expr 后 curve/grid 的 has_data 可同时为 TRUE，
   * 不能写成互斥 reset，也不能不问模式就采第一个有数据的源。 */
  matrix_source = plot3d_find_active_source(plot3d, PLOT3D_SAMPLE_MODE_MATRIX);
  csv_source = plot3d_find_active_source(plot3d, PLOT3D_SAMPLE_MODE_NONE);
  curve_source = plot3d_find_active_source(plot3d, PLOT3D_SAMPLE_MODE_CURVE);
  grid_source = plot3d_find_active_source(plot3d, PLOT3D_SAMPLE_MODE_GRID);
  grid_now = grid_source != NULL;
  curve_now = curve_source != NULL;
  matrix_now = matrix_source != NULL;
  none_now = csv_source != NULL;

  /* none_now 刻意不进这个条件：csv 的点是一次性落地的静态数据，落地后 sample_from_func 就是
   * FALSE，数据点归调用方所有——plot3d_append_data_point 追加的点、plot3d_reset_data 清空的
   * 结果，都不该被下一次不相干的重采样（改配色表、改图型）冲掉。csv 真正需要被采样的时机由调用方
   * 显式把 sample_from_func 置 TRUE：plot3d_set_dataset，以及 plot3d_set_sample_mode 切到 none。 */
  if (!grid_now && !curve_now && !matrix_now && !plot3d->sample_from_func) {
    return RET_OK;
  }

  darray_clear(&(plot3d->data_points));
  /* csv 不算「函数源」：函数源会随采样参数变化持续重算，csv 不会，理由同上。 */
  plot3d->sample_from_func = grid_now || curve_now || matrix_now;
  if (grid_now) {
    plot3d_sample_grid_result(plot3d, grid_source);
  } else if (curve_now) {
    plot3d_sample_points(plot3d, curve_source);
  } else if (matrix_now) {
    plot3d_sample_grid_result(plot3d, matrix_source);
  } else if (none_now) {
    plot3d_sample_points(plot3d, csv_source);
  }

  return plot3d_mark_cache_dirty(widget);
}

/* dataset 已迁进 csv 数据源，核心侧的互斥只能表达成让 csv 让位；插件未注册（裁剪场景）时
 * 没有对象可让，直接当成功。 */
static ret_t plot3d_reset_csv_source(plot3d_t* plot3d) {
  plot3d_source_t* source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_NONE);

  return source != NULL ? plot3d_source_reset(source) : RET_OK;
}

/* curve / grid 同理：dataset 让位时清回调；插件未注册时直接当成功。 */
static ret_t plot3d_reset_curve_source(plot3d_t* plot3d) {
  plot3d_source_t* source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_CURVE);

  return source != NULL ? plot3d_source_reset(source) : RET_OK;
}

static ret_t plot3d_reset_grid_source(plot3d_t* plot3d) {
  plot3d_source_t* source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_GRID);

  return source != NULL ? plot3d_source_reset(source) : RET_OK;
}

/* 定义与其它分发函数放在一起（本文件末尾、plot3d_set_prop 之前），这里只留原型。 */
static ret_t plot3d_forward_prop_to_sources(widget_t* widget, const char* name, const value_t* v);
static ret_t plot3d_forward_prop_to_types(widget_t* widget, const char* name, const value_t* v);

ret_t plot3d_set_grid_func(widget_t* widget, plot3d_grid_func_t func, void* ctx) {
  plot3d_source_t* source = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_GRID);
  /* 与 plot3d_set_curve_func / plot3d_set_z_matrix 一致：回调不是属性，未注册时原样返回
   * RET_NOT_FOUND。 */
  if (source == NULL) {
    return RET_NOT_FOUND;
  }

  return_value_if_fail(
      plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_GRID_FUNC, (void*)func, ctx) == RET_OK,
      RET_BAD_PARAMS);

  if (func != NULL) {
    /* 采样函数与 dataset 互斥，后设置的生效。 */
    plot3d_reset_csv_source(plot3d);
    plot3d->sample_from_func = TRUE;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_mode(widget_t* widget, const char* sample_mode) {
  plot3d_t* plot3d = PLOT3D(widget);
  bool_t mode_changed = FALSE;
  return_value_if_fail(plot3d != NULL && sample_mode != NULL, RET_BAD_PARAMS);
  /* 非法模式名仍 RET_BAD_PARAMS（sample_props 等用例依赖）。 */
  return_value_if_fail(tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_NONE) ||
                           tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_GRID) ||
                           tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_CURVE) ||
                           tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_MATRIX),
                       RET_BAD_PARAMS);

  /* none 豁免注册表检查：对应 csv 插件，裁掉 csv 的配置下必须仍能切回 none。 */
  if (!tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_NONE) &&
      plot3d_find_source(plot3d, sample_mode) == NULL) {
    return RET_NOT_FOUND;
  }

  mode_changed = !tk_str_eq(plot3d->sample_mode, sample_mode);
  plot3d->sample_mode = tk_str_copy(plot3d->sample_mode, sample_mode);

  /* 切到 none 时显式请求一次采样。不能依赖上一个函数源留下的 sample_from_func：中间若出现
   * 「进入重采样但没有活动函数源」（清空矩阵、切到无数据的 grid/curve），该标志会被清成 FALSE，
   * 再依赖遗留值就采不出仍持有数据的 csv。 */
  if (mode_changed && tk_str_eq(sample_mode, PLOT3D_SAMPLE_MODE_NONE)) {
    plot3d->sample_from_func = TRUE;
  }

  return plot3d_resample(widget);
}

/* 范围为空表示不指定：grid 采样据此按 0~1 铺开；认领同一属性的数据源插件如何解释「不指定」由
 * 插件自己决定（如 matrix 按列号与行号铺开）。 */
ret_t plot3d_set_sample_x_range(widget_t* widget, const char* sample_x_range) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_x_range);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_X_RANGE, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_y_range(widget_t* widget, const char* sample_y_range) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_y_range);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_Y_RANGE, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_steps(widget_t* widget, const char* sample_steps) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_steps);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_STEPS, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_z_expr(widget_t* widget, const char* sample_z_expr) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_z_expr);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_Z_EXPR, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_x_expr(widget_t* widget, const char* sample_x_expr) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_x_expr);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_X_EXPR, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_y_expr(widget_t* widget, const char* sample_y_expr) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_y_expr);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_Y_EXPR, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_z_matrix(widget_t* widget, const char* sample_z_matrix) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_z_matrix);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v);
  if (ret != RET_OK) {
    /* 该属性在总表中：无人认领说明 matrix 数据源没注册（裁剪场景），与 plot3d_set_prop 的裁决
     * 保持一致。 */
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  /* 矩阵与 dataset 各自属于一个数据源，由 sample-mode 决定谁出数据，互不清除：本函数只管把矩阵
   * 交给插件。这样属性路径（XML/clone/脚本，走广播 + plot3d_resample）与本函数完全等价，不会
   * 因为绕开了 C API 就有两种行为。 */
  return plot3d_resample(widget);
}

ret_t plot3d_set_z_matrix(widget_t* widget, const float_t* zs, uint32_t cols, uint32_t rows) {
  plot3d_source_t* source = NULL;
  plot3d_source_matrix_size_t size;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  return_value_if_fail(zs == NULL || (cols > 0 && rows > 0), RET_BAD_PARAMS);

  source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_MATRIX);
  /* 与 plot3d_set_color_func 一致：矩阵数据不是属性、不经 plot3d_is_known_prop 裁决，
   * 插件未注册（裁剪场景）时原样返回 RET_NOT_FOUND，也不刷日志。 */
  if (source == NULL) {
    return RET_NOT_FOUND;
  }

  size.cols = cols;
  size.rows = rows;
  return_value_if_fail(
      plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_Z_MATRIX, (void*)zs, &size) == RET_OK,
      RET_BAD_PARAMS);

  /* 与 plot3d_set_sample_z_matrix 同理：矩阵不清 dataset。 */
  return plot3d_resample(widget);
}

/* 定义与其它分发函数放在一起（本文件末尾、plot3d_set_prop 之前），这里只留原型。 */
static ret_t plot3d_forward_prop_to_colorizers(widget_t* widget, const char* name,
                                                const value_t* v);

ret_t plot3d_set_sample_color_expr(widget_t* widget, const char* sample_color_expr) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_color_expr);
  ret = plot3d_forward_prop_to_colorizers(widget, PLOT3D_PROP_SAMPLE_COLOR_EXPR, &v);
  if (ret == RET_OK) {
    return plot3d_resample(widget);
  }

  /* 该属性在总表中：无人认领说明配色插件没注册（裁剪场景），与 plot3d_set_prop 的裁决一致。 */
  return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
}

ret_t plot3d_set_color_func(widget_t* widget, plot3d_color_func_t func, void* ctx) {
  uint32_t i = 0;
  bool_t accepted = FALSE;
  plot3d_colorizer_t* colorizer = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->colorizers.size; i++) {
    colorizer = (plot3d_colorizer_t*)darray_get(&(plot3d->colorizers), i);
    if (plot3d_colorizer_set_data(colorizer, PLOT3D_COLORIZER_DATA_COLOR_FUNC, (void*)func,
                                   ctx) == RET_OK) {
      accepted = TRUE;
    }
  }

  /* 配色插件未注册是裁剪场景下可预期的结果，不刷日志。
   *
   * 与 plot3d_set_sample_color_expr 不同，这里原样返回 RET_NOT_FOUND：配色回调不是属性，
   * 不进 s_plot3d_properties[]、不经 plot3d_is_known_prop 裁决，也就没有「被 custom_props
   * 兜成 RET_OK」的风险，无须把「没人认领」翻译成 RET_BAD_PARAMS。set_data 类接口都照此办理。 */
  if (!accepted) {
    return RET_NOT_FOUND;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_t_range(widget_t* widget, const char* sample_t_range) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, sample_t_range);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_T_RANGE, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_sample_t_steps(widget_t* widget, uint32_t sample_t_steps) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_uint32(&v, sample_t_steps);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_SAMPLE_T_STEPS, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_curve_func(widget_t* widget, plot3d_curve_func_t func, void* ctx) {
  plot3d_source_t* source = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  source = plot3d_find_source(plot3d, PLOT3D_SAMPLE_MODE_CURVE);
  /* 与 plot3d_set_z_matrix / plot3d_set_color_func 一致：回调不是属性，未注册时原样返回
   * RET_NOT_FOUND。 */
  if (source == NULL) {
    return RET_NOT_FOUND;
  }

  return_value_if_fail(
      plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_CURVE_FUNC, (void*)func, ctx) == RET_OK,
      RET_BAD_PARAMS);

  if (func != NULL) {
    /* 采样函数与 dataset 互斥，后设置的生效。 */
    plot3d_reset_csv_source(plot3d);
    plot3d->sample_from_func = TRUE;
  }

  return plot3d_resample(widget);
}

ret_t plot3d_set_colormap(widget_t* widget, const char* colormap) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && colormap != NULL, RET_BAD_PARAMS);

  plot3d->colormap = tk_str_copy(plot3d->colormap, colormap);

  return plot3d_resample(widget);
}

ret_t plot3d_reset_data(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  plot3d->sample_from_func = FALSE;
  darray_clear(&(plot3d->data_points));
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_append_data_point(widget_t* widget, float_t x, float_t y, float_t z, const char* color) {
  plot3d_t* plot3d = PLOT3D(widget);
  color_t c = color_init(255, 255, 255, 255);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  return_value_if_fail(color != NULL, RET_BAD_PARAMS);

  c = color_parse(color);
  return_value_if_fail(plot3d_append_data_point_internal(plot3d, x, y, z, c, FALSE) == RET_OK,
                       RET_FAIL);

  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_append_break_data_point(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  return_value_if_fail(
      plot3d_append_data_point_internal(plot3d, 0, 0, 0, color_init(0, 0, 0, 0), TRUE) == RET_OK,
      RET_FAIL);

  return plot3d_mark_cache_dirty(widget);
}

uint32_t plot3d_get_data_points_nr(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, 0);

  return plot3d->data_points.size;
}

ret_t plot3d_get_data_point(widget_t* widget, uint32_t index, plot3d_data_point_t* point) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_data_point_t* p = NULL;
  return_value_if_fail(plot3d != NULL && point != NULL, RET_BAD_PARAMS);
  return_value_if_fail(index < plot3d->data_points.size, RET_BAD_PARAMS);

  p = (plot3d_data_point_t*)(darray_get(&(plot3d->data_points), index));
  return_value_if_fail(p != NULL, RET_FAIL);
  *point = *p;

  return RET_OK;
}

/* dataset 本身归 csv 数据源，但「谁让位给谁」是数据源之间的协调，留在核心。 */
ret_t plot3d_set_dataset(widget_t* widget, const char* dataset) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_str(&v, dataset);
  ret = plot3d_forward_prop_to_sources(widget, PLOT3D_PROP_DATASET, &v);
  if (ret != RET_OK) {
    /* 该属性在总表中：无人认领说明 csv 数据源没注册（裁剪场景），与 plot3d_set_prop 的裁决
     * 保持一致。 */
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  /* dataset 与采样函数互斥，后设置的生效。向 grid / curve 插件 reset（不碰 matrix：矩阵与
   * dataset 由 sample-mode 区分，各自独立）。 */
  plot3d_reset_grid_source(plot3d);
  plot3d_reset_curve_source(plot3d);
  /* 直接改字段而不经 plot3d_set_sample_mode：后者有注册表检查，会在裁掉 csv 的配置下把它拦下。 */
  plot3d->sample_mode = tk_str_copy(plot3d->sample_mode, PLOT3D_SAMPLE_MODE_NONE);
  /* 置位是为了让下面这次重采样真的跑起来：此刻三个函数源都不活动，不置位会被提前返回挡掉，
   * csv 的 sample() 永远不会被调用。重采样内部会立刻把它重算成 FALSE。 */
  plot3d->sample_from_func = TRUE;

  return plot3d_resample(widget);
}

ret_t plot3d_set_plottype(widget_t* widget, const char* plottype) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && plottype != NULL && *plottype != '\0', RET_BAD_PARAMS);

  /* 以控件上的图型实例为准：未注册或创建前无实例统一 RET_NOT_FOUND，不再维护硬编码白名单。 */
  if (plot3d_find_type(plot3d, plottype) == NULL) {
    return RET_NOT_FOUND;
  }

  plot3d->plottype = tk_str_copy(plot3d->plottype, plottype);
  /* 采样点的排布随类型变化，需要重新采样。 */
  plot3d_resample(widget);

  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_xlabel(widget_t* widget, const char* xlabel) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->xlabel = tk_str_copy(plot3d->xlabel, xlabel);
  widget_invalidate(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_ylabel(widget_t* widget, const char* ylabel) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->ylabel = tk_str_copy(plot3d->ylabel, ylabel);
  widget_invalidate(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_zlabel(widget_t* widget, const char* zlabel) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->zlabel = tk_str_copy(plot3d->zlabel, zlabel);
  widget_invalidate(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_show_grid(widget_t* widget, bool_t show_grid) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->show_grid = show_grid;
  widget_invalidate(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_show_datatip(widget_t* widget, bool_t show_datatip) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->show_datatip = show_datatip;
  if (!show_datatip) {
    plot3d->hover_index = -1;
  }
  widget_invalidate(widget, NULL);
  return RET_OK;
}

/* 颜色属性都要同时留下原文与解析结果，原文用于回读与保存。 */
static ret_t plot3d_set_color_field(widget_t* widget, char** text, color_t* color,
                                     const char* value) {
  return_value_if_fail(widget != NULL && value != NULL, RET_BAD_PARAMS);
  *text = tk_str_copy(*text, value);
  *color = color_parse(value);
  widget_invalidate(widget, NULL);

  return RET_OK;
}

ret_t plot3d_set_grid_color(widget_t* widget, const char* grid_color) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_color_field(widget, &(plot3d->grid_color_str), &(plot3d->grid_color),
                                 grid_color);
}

ret_t plot3d_set_xaxis_color(widget_t* widget, const char* xaxis_color) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_color_field(widget, &(plot3d->xaxis_color_str), &(plot3d->xaxis_color),
                                 xaxis_color);
}

ret_t plot3d_set_yaxis_color(widget_t* widget, const char* yaxis_color) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_color_field(widget, &(plot3d->yaxis_color_str), &(plot3d->yaxis_color),
                                 yaxis_color);
}

ret_t plot3d_set_zaxis_color(widget_t* widget, const char* zaxis_color) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_color_field(widget, &(plot3d->zaxis_color_str), &(plot3d->zaxis_color),
                                 zaxis_color);
}

ret_t plot3d_set_tick_color(widget_t* widget, const char* tick_color) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_color_field(widget, &(plot3d->tick_color_str), &(plot3d->tick_color),
                                 tick_color);
}

ret_t plot3d_set_axis_negative_brightness(widget_t* widget, float_t axis_negative_brightness) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->axis_negative_brightness =
      tk_min(tk_max(axis_negative_brightness, 0.0f), 1.0f);
  widget_invalidate_force(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_show_axis_tick(widget_t* widget, bool_t show_axis_tick) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->show_axis_tick = show_axis_tick;
  widget_invalidate(widget, NULL);
  return RET_OK;
}

ret_t plot3d_set_point_size(widget_t* widget, float_t point_size) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_float(&v, point_size);
  ret = plot3d_forward_prop_to_types(widget, PLOT3D_PROP_POINT_SIZE, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_line_width(widget_t* widget, float_t line_width) {
  value_t v;
  ret_t ret = RET_OK;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  value_set_float(&v, line_width);
  ret = plot3d_forward_prop_to_types(widget, PLOT3D_PROP_LINE_WIDTH, &v);
  if (ret != RET_OK) {
    return ret == RET_NOT_FOUND ? RET_BAD_PARAMS : ret;
  }

  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_enable_cache(widget_t* widget, bool_t enable_cache) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->enable_cache = enable_cache;
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_equal_axis(widget_t* widget, bool_t equal_axis) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->equal_axis = equal_axis;
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_camera_yaw(widget_t* widget, float_t camera_yaw) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->camera_yaw = camera_yaw;
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_camera_pitch(widget_t* widget, float_t camera_pitch) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->camera_pitch = tk_min(tk_max(camera_pitch, -1.4f), 1.4f);
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_camera_distance(widget_t* widget, float_t camera_distance) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->camera_distance = tk_min(tk_max(camera_distance, 1.8f), 12.0f);
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_camera_z_offset(widget_t* widget, float_t camera_z_offset) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->camera_z_offset = camera_z_offset;
  return plot3d_mark_cache_dirty(widget);
}

static ret_t plot3d_set_grid_position_field(widget_t* widget, float_t* field, float_t position) {
  *field = position;
  widget_invalidate(widget, NULL);

  return RET_OK;
}

ret_t plot3d_set_xy_grid_position(widget_t* widget, float_t xy_grid_position) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_position_field(widget, &(plot3d->xy_grid_position), xy_grid_position);
}

ret_t plot3d_set_xz_grid_position(widget_t* widget, float_t xz_grid_position) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_position_field(widget, &(plot3d->xz_grid_position), xz_grid_position);
}

ret_t plot3d_set_yz_grid_position(widget_t* widget, float_t yz_grid_position) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_position_field(widget, &(plot3d->yz_grid_position), yz_grid_position);
}

static float_t plot3d_clamp_box_aspect(float_t aspect) {
  return tk_max(PLOT3D_MIN_BOX_ASPECT, tk_min(PLOT3D_MAX_BOX_ASPECT, aspect));
}

ret_t plot3d_set_box_aspect_x(widget_t* widget, float_t box_aspect_x) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->box_aspect_x = plot3d_clamp_box_aspect(box_aspect_x);
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_box_aspect_y(widget_t* widget, float_t box_aspect_y) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->box_aspect_y = plot3d_clamp_box_aspect(box_aspect_y);
  return plot3d_mark_cache_dirty(widget);
}

ret_t plot3d_set_box_aspect_z(widget_t* widget, float_t box_aspect_z) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  plot3d->box_aspect_z = plot3d_clamp_box_aspect(box_aspect_z);
  return plot3d_mark_cache_dirty(widget);
}

static ret_t plot3d_set_grid_count_field(widget_t* widget, uint32_t* field, uint32_t count) {
  *field = tk_max(PLOT3D_MIN_GRID_COUNT, tk_min(PLOT3D_MAX_GRID_COUNT, count));
  widget_invalidate(widget, NULL);

  return RET_OK;
}

ret_t plot3d_set_x_grid_count(widget_t* widget, uint32_t x_grid_count) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_count_field(widget, &(plot3d->x_grid_count), x_grid_count);
}

ret_t plot3d_set_y_grid_count(widget_t* widget, uint32_t y_grid_count) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_count_field(widget, &(plot3d->y_grid_count), y_grid_count);
}

ret_t plot3d_set_z_grid_count(widget_t* widget, uint32_t z_grid_count) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_set_grid_count_field(widget, &(plot3d->z_grid_count), z_grid_count);
}

static float_t plot3d_pick_step_position(float_t pos, float_t min_v, float_t extent,
                                          uint32_t count, int32_t dir) {
  uint32_t i = 0;
  int32_t on_index = -1;
  float_t step = extent / (float_t)count;
  float_t eps = tk_max(extent * 1e-4f, 1e-6f);

  for (i = 0; i <= count; i++) {
    float_t v = min_v + step * (float_t)i;
    if (tk_abs(pos - v) <= eps) {
      on_index = (int32_t)i;
      break;
    }
  }

  if (on_index >= 0) {
    int32_t next_index = on_index + (dir > 0 ? 1 : -1);
    next_index = tk_max(0, tk_min((int32_t)count, next_index));
    return min_v + step * (float_t)next_index;
  }

  if (dir > 0) {
    for (i = 0; i <= count; i++) {
      float_t v = min_v + step * (float_t)i;
      if (v > pos + eps) {
        return v;
      }
    }
    return min_v + extent;
  }

  for (i = count + 1; i > 0; i--) {
    float_t v = min_v + step * (float_t)(i - 1);
    if (v < pos - eps) {
      return v;
    }
  }

  return min_v;
}

/* 网格面用它的法线轴标识：xy 面沿 z 挪，xz 面沿 y 挪，yz 面沿 x 挪。 */
static ret_t plot3d_step_grid_position(widget_t* widget, plot3d_axis_t axis, float_t* field,
                                        int32_t dir) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_bounds_t bounds;
  plot3d_axis_range_t range;
  float_t position = 0;
  return_value_if_fail(plot3d != NULL && dir != 0, RET_BAD_PARAMS);
  return_value_if_fail(plot3d_calc_bounds(plot3d, &bounds) == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_bounds_get_axis(&bounds, axis, &range) == RET_OK, RET_FAIL);

  position = plot3d_pick_step_position(*field, range.min_v, range.extent, range.count, dir);

  return plot3d_set_grid_position_field(widget, field, position);
}

ret_t plot3d_step_xy_grid_position(widget_t* widget, int32_t dir) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_step_grid_position(widget, PLOT3D_AXIS_Z, &(plot3d->xy_grid_position), dir);
}

ret_t plot3d_step_xz_grid_position(widget_t* widget, int32_t dir) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_step_grid_position(widget, PLOT3D_AXIS_Y, &(plot3d->xz_grid_position), dir);
}

ret_t plot3d_step_yz_grid_position(widget_t* widget, int32_t dir) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  return plot3d_step_grid_position(widget, PLOT3D_AXIS_X, &(plot3d->yz_grid_position), dir);
}

static color_t plot3d_modulate_color(color_t c, float_t factor) {
  color_t out = c;
  float_t f = tk_min(tk_max(factor, 0.15f), 1.0f);
  out.rgba.r = (uint8_t)(out.rgba.r * f);
  out.rgba.g = (uint8_t)(out.rgba.g * f);
  out.rgba.b = (uint8_t)(out.rgba.b * f);
  return out;
}

uint32_t plot3d_build_axis_color_segments(float_t min_v, float_t max_v, color_t positive_color,
                                           float_t brightness, plot3d_axis_color_segment_t out[2]) {
  color_t neg;
  return_value_if_fail(out != NULL, 0);
  if (min_v > max_v) {
    float_t tmp = min_v;
    min_v = max_v;
    max_v = tmp;
  }
  neg = plot3d_modulate_color(positive_color, brightness);
  if (min_v < 0 && max_v > 0) {
    out[0].t0 = min_v;
    out[0].t1 = 0;
    out[0].color = neg;
    out[1].t0 = 0;
    out[1].t1 = max_v;
    out[1].color = positive_color;
    return 2;
  }
  out[0].t0 = min_v;
  out[0].t1 = max_v;
  out[0].color = (max_v <= 0) ? neg : positive_color;
  return 1;
}

static float_t plot3d_get_active_type_float(plot3d_t* plot3d, const char* name,
                                             float_t def_value) {
  value_t v;
  plot3d_type_t* type = NULL;
  return_value_if_fail(plot3d != NULL && name != NULL, def_value);

  type = plot3d_find_type(plot3d, plot3d->plottype);
  if (type == NULL || plot3d_type_get_prop(type, name, &v) != RET_OK) {
    return def_value;
  }

  return value_float(&v);
}

static ret_t plot3d_rebuild_draw_cache(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_projected_point_t* projected = (plot3d_projected_point_t*)(plot3d->projected_points);
  plot3d_type_t* type = NULL;
  plot3d_paint_ctx_t ctx;

  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);
  if (plot3d->enable_cache && !plot3d->draw_cache_dirty) {
    return RET_OK;
  }

  darray_clear(&(plot3d->draw_primitives));

  type = plot3d_find_type(plot3d, plot3d->plottype);
  if (type == NULL) {
    return RET_FAIL;
  }

  memset(&ctx, 0x00, sizeof(ctx));
  ctx.points = projected;
  ctx.points_nr = plot3d->projected_points_nr;
  ctx.point_size = plot3d_get_active_type_float(plot3d, PLOT3D_PROP_POINT_SIZE, 4);
  ctx.line_width = plot3d_get_active_type_float(plot3d, PLOT3D_PROP_LINE_WIDTH, 1.5f);
  ctx.colormap = plot3d->colormap;
  return_value_if_fail(plot3d_type_build(type, &ctx, &(plot3d->draw_primitives)) == RET_OK,
                       RET_FAIL);

  if (plot3d->draw_primitives.size > 1) {
    qsort(plot3d->draw_primitives.elms, plot3d->draw_primitives.size, sizeof(void*),
          plot3d_primitive_compare);
  }

  plot3d->draw_cache_dirty = FALSE;
  return RET_OK;
}

/* 定义与 s_plot3d_properties[] 放在一起（该数组在本文件末尾、vtable 之前），这里只留原型。 */
static bool_t plot3d_is_known_prop(const char* name);

/* 三值协议在各类插件上一致，收敛返回码的逻辑也就只写一份。 */
static ret_t plot3d_merge_set_prop_ret(ret_t ret, bool_t* accepted, bool_t* rejected) {
  if (ret == RET_OK) {
    *accepted = TRUE;
  } else if (ret == RET_BAD_PARAMS) {
    *rejected = TRUE;
  }

  return RET_OK;
}

/* 认领者之间对合法性的判断应当一致，同时出现接受与拒绝时以拒绝为准。此时先接受的插件已经改了
 * 自己的状态，而调用方拿到 RET_BAD_PARAMS 后不会 resample，控件会停在「插件有新值、数据是
 * 旧的」状态——这里假设认领者的校验逻辑一致，假设不成立时没有回滚。 */
static ret_t plot3d_conclude_set_prop_ret(bool_t accepted, bool_t rejected) {
  if (rejected) {
    return RET_BAD_PARAMS;
  }

  return accepted ? RET_OK : RET_NOT_FOUND;
}

/* 纯分发原语：把属性挨个转发给全部数据源实例，自身不触发 resample、也不做任何裁决。
 *
 * 调用方两类：(1) plot3d_set_prop 尾部——合并各插件结果后再统一 resample / 裁决；
 * (2) 公开 C API setter（如 plot3d_set_sample_x_range）——按本函数返回值决定成败，
 * RET_NOT_FOUND 翻成 RET_BAD_PARAMS（属性在总表中却无人认领=裁剪），RET_BAD_PARAMS 原样上抛，
 * 成功后再 resample。数据源属性已全部进插件，核心不再「先落值再转发」。 */
static ret_t plot3d_forward_prop_to_sources(widget_t* widget, const char* name, const value_t* v) {
  uint32_t i = 0;
  ret_t ret = RET_OK;
  plot3d_source_t* source = NULL;
  bool_t accepted = FALSE;
  bool_t rejected = FALSE;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  /* 必须遍历完全部实例：同一属性可能有多个认领者（如 grid 与 curve 共用 sample-z-expr），
   * 命中一个就提前返回会让其余认领者收不到值。 */
  for (i = 0; i < plot3d->sources.size; i++) {
    source = (plot3d_source_t*)darray_get(&(plot3d->sources), i);
    ret = plot3d_source_set_prop(source, name, v);
    plot3d_merge_set_prop_ret(ret, &accepted, &rejected);
  }

  return plot3d_conclude_set_prop_ret(accepted, rejected);
}

/* colorizer 版的纯分发原语，语义与 plot3d_forward_prop_to_sources 完全一致。 */
static ret_t plot3d_forward_prop_to_colorizers(widget_t* widget, const char* name,
                                                const value_t* v) {
  uint32_t i = 0;
  ret_t ret = RET_OK;
  plot3d_colorizer_t* colorizer = NULL;
  bool_t accepted = FALSE;
  bool_t rejected = FALSE;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->colorizers.size; i++) {
    colorizer = (plot3d_colorizer_t*)darray_get(&(plot3d->colorizers), i);
    ret = plot3d_colorizer_set_prop(colorizer, name, v);
    plot3d_merge_set_prop_ret(ret, &accepted, &rejected);
  }

  return plot3d_conclude_set_prop_ret(accepted, rejected);
}

static ret_t plot3d_broadcast_get_prop_from_sources(widget_t* widget, const char* name,
                                                     value_t* v) {
  uint32_t i = 0;
  plot3d_source_t* source = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  /* 与 set 侧不同，这里命中即返回：值只能返回一个，取第一个认领者的。多认领者（如 grid 与 curve
   * 共用 sample-z-expr）必须保存等价的值，否则 get 的结果取决于插件的注册顺序。 */
  for (i = 0; i < plot3d->sources.size; i++) {
    source = (plot3d_source_t*)darray_get(&(plot3d->sources), i);
    if (plot3d_source_get_prop(source, name, v) == RET_OK) {
      return RET_OK;
    }
  }

  return RET_NOT_FOUND;
}

/* colorizer 版的 get 广播，语义与 plot3d_broadcast_get_prop_from_sources 完全一致。 */
static ret_t plot3d_broadcast_get_prop_from_colorizers(widget_t* widget, const char* name,
                                                        value_t* v) {
  uint32_t i = 0;
  plot3d_colorizer_t* colorizer = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->colorizers.size; i++) {
    colorizer = (plot3d_colorizer_t*)darray_get(&(plot3d->colorizers), i);
    if (plot3d_colorizer_get_prop(colorizer, name, v) == RET_OK) {
      return RET_OK;
    }
  }

  return RET_NOT_FOUND;
}

/* type 版的纯分发原语，语义与 plot3d_forward_prop_to_sources 完全一致。 */
static ret_t plot3d_forward_prop_to_types(widget_t* widget, const char* name, const value_t* v) {
  uint32_t i = 0;
  ret_t ret = RET_OK;
  plot3d_type_t* type = NULL;
  bool_t accepted = FALSE;
  bool_t rejected = FALSE;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->types.size; i++) {
    type = (plot3d_type_t*)darray_get(&(plot3d->types), i);
    ret = plot3d_type_set_prop(type, name, v);
    plot3d_merge_set_prop_ret(ret, &accepted, &rejected);
  }

  return plot3d_conclude_set_prop_ret(accepted, rejected);
}

static ret_t plot3d_broadcast_get_prop_from_types(widget_t* widget, const char* name, value_t* v) {
  uint32_t i = 0;
  plot3d_type_t* type = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  for (i = 0; i < plot3d->types.size; i++) {
    type = (plot3d_type_t*)darray_get(&(plot3d->types), i);
    if (plot3d_type_get_prop(type, name, v) == RET_OK) {
      return RET_OK;
    }
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_get_prop(widget_t* widget, const char* name, value_t* v) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(PLOT3D_PROP_PLOTTYPE, name)) {
    value_set_str(v, plot3d->plottype);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_SAMPLE_MODE, name)) {
    value_set_str(v, plot3d->sample_mode);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_COLORMAP, name)) {
    value_set_str(v, plot3d->colormap);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_XLABEL, name)) {
    value_set_str(v, plot3d->xlabel);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_YLABEL, name)) {
    value_set_str(v, plot3d->ylabel);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_ZLABEL, name)) {
    value_set_str(v, plot3d->zlabel);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_GRID, name)) {
    value_set_bool(v, plot3d->show_grid);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_DATATIP, name)) {
    value_set_bool(v, plot3d->show_datatip);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_GRID_COLOR, name)) {
    value_set_str(v, plot3d->grid_color_str);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_XAXIS_COLOR, name)) {
    value_set_str(v, plot3d->xaxis_color_str);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_YAXIS_COLOR, name)) {
    value_set_str(v, plot3d->yaxis_color_str);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_ZAXIS_COLOR, name)) {
    value_set_str(v, plot3d->zaxis_color_str);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_TICK_COLOR, name)) {
    value_set_str(v, plot3d->tick_color_str);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS, name)) {
    value_set_float(v, plot3d->axis_negative_brightness);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_AXIS_TICK, name)) {
    value_set_bool(v, plot3d->show_axis_tick);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_ENABLE_CACHE, name)) {
    value_set_bool(v, plot3d->enable_cache);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_EQUAL_AXIS, name)) {
    value_set_bool(v, plot3d->equal_axis);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_YAW, name)) {
    value_set_float(v, plot3d->camera_yaw);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_PITCH, name)) {
    value_set_float(v, plot3d->camera_pitch);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_DISTANCE, name)) {
    value_set_float(v, plot3d->camera_distance);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_Z_OFFSET, name)) {
    value_set_float(v, plot3d->camera_z_offset);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_XY_GRID_POSITION, name)) {
    value_set_float(v, plot3d->xy_grid_position);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_XZ_GRID_POSITION, name)) {
    value_set_float(v, plot3d->xz_grid_position);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_YZ_GRID_POSITION, name)) {
    value_set_float(v, plot3d->yz_grid_position);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_X, name)) {
    value_set_float(v, plot3d->box_aspect_x);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_Y, name)) {
    value_set_float(v, plot3d->box_aspect_y);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_Z, name)) {
    value_set_float(v, plot3d->box_aspect_z);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_X_GRID_COUNT, name)) {
    value_set_uint32(v, plot3d->x_grid_count);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_Y_GRID_COUNT, name)) {
    value_set_uint32(v, plot3d->y_grid_count);
    return RET_OK;
  } else if (tk_str_eq(PLOT3D_PROP_Z_GRID_COUNT, name)) {
    value_set_uint32(v, plot3d->z_grid_count);
    return RET_OK;
  }

  if (plot3d_broadcast_get_prop_from_sources(widget, name, v) == RET_OK) {
    return RET_OK;
  }
  if (plot3d_broadcast_get_prop_from_colorizers(widget, name, v) == RET_OK) {
    return RET_OK;
  }

  return plot3d_broadcast_get_prop_from_types(widget, name, v);
}

static ret_t plot3d_set_prop(widget_t* widget, const char* name, const value_t* v) {
  ret_t ret = RET_OK;
  bool_t accepted = FALSE;
  bool_t rejected = FALSE;
  return_value_if_fail(widget != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  /* dataset 的值已经归 csv 数据源，本分支只为把互斥协调（让函数源让位、把模式设回 none）
   * 接进属性路径——删掉它，XML 加载、clone 与脚本设 dataset 时就只落值、不协调，与 C API 分叉。
   * 落值本身由 plot3d_set_dataset 转发给插件完成。 */
  if (tk_str_eq(PLOT3D_PROP_DATASET, name)) {
    return plot3d_set_dataset(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_PLOTTYPE, name)) {
    return plot3d_set_plottype(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_XLABEL, name)) {
    return plot3d_set_xlabel(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_YLABEL, name)) {
    return plot3d_set_ylabel(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_ZLABEL, name)) {
    return plot3d_set_zlabel(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_GRID, name)) {
    return plot3d_set_show_grid(widget, value_bool(v));
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_DATATIP, name)) {
    return plot3d_set_show_datatip(widget, value_bool(v));
  } else if (tk_str_eq(PLOT3D_PROP_GRID_COLOR, name)) {
    return plot3d_set_grid_color(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_XAXIS_COLOR, name)) {
    return plot3d_set_xaxis_color(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_YAXIS_COLOR, name)) {
    return plot3d_set_yaxis_color(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_ZAXIS_COLOR, name)) {
    return plot3d_set_zaxis_color(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_TICK_COLOR, name)) {
    return plot3d_set_tick_color(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS, name)) {
    return plot3d_set_axis_negative_brightness(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_SHOW_AXIS_TICK, name)) {
    return plot3d_set_show_axis_tick(widget, value_bool(v));
  } else if (tk_str_eq(PLOT3D_PROP_POINT_SIZE, name)) {
    return plot3d_set_point_size(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_LINE_WIDTH, name)) {
    return plot3d_set_line_width(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_ENABLE_CACHE, name)) {
    return plot3d_set_enable_cache(widget, value_bool(v));
  } else if (tk_str_eq(PLOT3D_PROP_EQUAL_AXIS, name)) {
    return plot3d_set_equal_axis(widget, value_bool(v));
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_YAW, name)) {
    return plot3d_set_camera_yaw(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_PITCH, name)) {
    return plot3d_set_camera_pitch(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_DISTANCE, name)) {
    return plot3d_set_camera_distance(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_CAMERA_Z_OFFSET, name)) {
    return plot3d_set_camera_z_offset(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_XY_GRID_POSITION, name)) {
    return plot3d_set_xy_grid_position(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_XZ_GRID_POSITION, name)) {
    return plot3d_set_xz_grid_position(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_YZ_GRID_POSITION, name)) {
    return plot3d_set_yz_grid_position(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_X, name)) {
    return plot3d_set_box_aspect_x(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_Y, name)) {
    return plot3d_set_box_aspect_y(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_BOX_ASPECT_Z, name)) {
    return plot3d_set_box_aspect_z(widget, value_float(v));
  } else if (tk_str_eq(PLOT3D_PROP_SAMPLE_MODE, name)) {
    return plot3d_set_sample_mode(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_COLORMAP, name)) {
    return plot3d_set_colormap(widget, value_str(v));
  } else if (tk_str_eq(PLOT3D_PROP_X_GRID_COUNT, name)) {
    return plot3d_set_x_grid_count(widget, (uint32_t)value_int(v));
  } else if (tk_str_eq(PLOT3D_PROP_Y_GRID_COUNT, name)) {
    return plot3d_set_y_grid_count(widget, (uint32_t)value_int(v));
  } else if (tk_str_eq(PLOT3D_PROP_Z_GRID_COUNT, name)) {
    return plot3d_set_z_grid_count(widget, (uint32_t)value_int(v));
  }

  /* 三类插件都要问一遍再收敛，理由与单类内部「遍历完全部实例」相同：同一属性可能被跨类认领，
   * 先命中就返回会让另一类收不到值。
   *
   * 新增一类插件时要改的就四处：plot3d_create（建实例）、plot3d_on_destroy（销毁实例）、
   * 这里（多一次 forward）、plot3d_get_prop 尾部（多一次 broadcast）。 */
  plot3d_merge_set_prop_ret(plot3d_forward_prop_to_sources(widget, name, v), &accepted,
                             &rejected);
  plot3d_merge_set_prop_ret(plot3d_forward_prop_to_colorizers(widget, name, v), &accepted,
                             &rejected);
  plot3d_merge_set_prop_ret(plot3d_forward_prop_to_types(widget, name, v), &accepted, &rejected);
  ret = plot3d_conclude_set_prop_ret(accepted, rejected);

  /* 重采样与裁决同处一层，分发本身不碰它：认领者再多也只重采样一次，而只要有一个认领者拒绝就
   * 一次都不采——否则「一类接受、另一类拒绝」会留下「已按新值重采过、却返回 RET_BAD_PARAMS」
   * 的矛盾状态。 */
  if (ret == RET_OK) {
    plot3d_resample(widget);
  }

  /* 裁决必须在全部插件集合都问过之后做：属性名在总表中却无人认领，说明是「已知属性但对应插件未
   * 注册」（裁剪场景）。返回 RET_NOT_FOUND 会被 widget_set_prop 兜进 custom_props 并变成
   * RET_OK，让调用方误以为设置成功。
   * 后续增加别的插件集合时，在本判断之前再插一次分发即可，本判断不必再动。 */
  if (ret == RET_NOT_FOUND && plot3d_is_known_prop(name)) {
    ret = RET_BAD_PARAMS;
  }

  return ret;
}

ret_t plot3d_set_prop_for_test(widget_t* widget, const char* name, const value_t* v) {
  return plot3d_set_prop(widget, name, v);
}

ret_t plot3d_get_prop_for_test(widget_t* widget, const char* name, value_t* v) {
  return plot3d_get_prop(widget, name, v);
}

uint32_t plot3d_get_source_nr_for_test(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, 0);

  return plot3d->sources.size;
}

uint32_t plot3d_get_colorizer_nr_for_test(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, 0);

  return plot3d->colorizers.size;
}

uint32_t plot3d_get_type_nr_for_test(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, 0);

  return plot3d->types.size;
}

uint32_t plot3d_get_resample_nr_for_test(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, 0);

  return plot3d->resample_nr_for_test;
}

static ret_t plot3d_on_destroy(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(widget != NULL && plot3d != NULL, RET_BAD_PARAMS);

  TKMEM_FREE(plot3d->plottype);
  TKMEM_FREE(plot3d->sample_mode);
  TKMEM_FREE(plot3d->colormap);
  TKMEM_FREE(plot3d->xlabel);
  TKMEM_FREE(plot3d->ylabel);
  TKMEM_FREE(plot3d->zlabel);
  TKMEM_FREE(plot3d->grid_color_str);
  TKMEM_FREE(plot3d->xaxis_color_str);
  TKMEM_FREE(plot3d->yaxis_color_str);
  TKMEM_FREE(plot3d->zaxis_color_str);
  TKMEM_FREE(plot3d->tick_color_str);
  TKMEM_FREE(plot3d->projected_points);
  darray_deinit(&(plot3d->data_points));
  darray_deinit(&(plot3d->draw_primitives));
  darray_deinit(&(plot3d->sources));
  darray_deinit(&(plot3d->colorizers));
  darray_deinit(&(plot3d->types));

  return RET_OK;
}

static ret_t plot3d_on_paint_self(widget_t* widget, canvas_t* c) {
  plot3d_t* plot3d = PLOT3D(widget);
  plot3d_bounds_t bounds;
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  plot3d_calc_bounds(plot3d, &bounds);
  plot3d_update_projection_cache(widget, &bounds);
  plot3d_rebuild_draw_cache(widget);
  plot3d_draw_grid_and_axis(widget, c, &bounds);
  plot3d_draw_data(widget, c, &bounds);
  /* 文字最后画，保证压在网格线和数据图元之上。 */
  plot3d_draw_axis_text(widget, c, &bounds);
  plot3d_datatip_paint(widget, c);

  if (!plot3d->enable_cache) {
    plot3d->projection_cache_dirty = TRUE;
    plot3d->draw_cache_dirty = TRUE;
  }

  return RET_OK;
}

static ret_t plot3d_on_event(widget_t* widget, event_t* e) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(widget != NULL && plot3d != NULL && e != NULL, RET_BAD_PARAMS);

  if (e->type == EVT_POINTER_DOWN) {
    pointer_event_t* evt = (pointer_event_t*)e;
    plot3d_datatip_clear_hover(widget);
    plot3d->pointer_down = TRUE;
    plot3d->pointer_button = evt->button;
    plot3d->last_pointer_x = evt->x;
    plot3d->last_pointer_y = evt->y;
    return RET_STOP;
  } else if (e->type == EVT_POINTER_MOVE && plot3d->pointer_down) {
    pointer_event_t* evt = (pointer_event_t*)e;
    int32_t dx = evt->x - plot3d->last_pointer_x;
    int32_t dy = evt->y - plot3d->last_pointer_y;

    if (plot3d->pointer_button == 2) {
      plot3d->camera_z_offset += dy * 0.02f;
    } else {
      plot3d->camera_yaw += dx * 0.01f;
      plot3d->camera_pitch = tk_min(tk_max(plot3d->camera_pitch + dy * 0.01f, -1.4f), 1.4f);
    }

    plot3d->last_pointer_x = evt->x;
    plot3d->last_pointer_y = evt->y;
    plot3d_mark_cache_dirty(widget);
    return RET_STOP;
  } else if (e->type == EVT_POINTER_MOVE) {
    pointer_event_t* evt = (pointer_event_t*)e;
    if (plot3d->show_datatip) {
      plot3d_datatip_update_hover(widget, evt->x, evt->y);
    }
    return RET_OK;
  } else if (e->type == EVT_POINTER_UP) {
    plot3d_datatip_clear_hover(widget);
    plot3d->pointer_down = FALSE;
    plot3d->pointer_button = 0;
    return RET_STOP;
  } else if (e->type == EVT_POINTER_LEAVE) {
    plot3d_datatip_clear_hover(widget);
  } else if (e->type == EVT_WHEEL) {
    wheel_event_t* evt = (wheel_event_t*)e;
    plot3d->camera_distance = tk_max(0.2f, plot3d->camera_distance - evt->dy * 0.05f);
    plot3d_mark_cache_dirty(widget);
    return RET_STOP;
  } else if (e->type == EVT_MULTI_GESTURE) {
    multi_gesture_event_t* evt = (multi_gesture_event_t*)e;
    plot3d->camera_distance = tk_max(0.2f, plot3d->camera_distance * (1.0f - evt->distance));
    plot3d_mark_cache_dirty(widget);
    return RET_STOP;
  } else if (e->type == EVT_RESIZE || e->type == EVT_MOVE_RESIZE) {
    plot3d_mark_cache_dirty(widget);
  }

  return RET_OK;
}

const char* s_plot3d_properties[] = {PLOT3D_PROP_DATASET,
                                      PLOT3D_PROP_PLOTTYPE,
                                      PLOT3D_PROP_XLABEL,
                                      PLOT3D_PROP_YLABEL,
                                      PLOT3D_PROP_ZLABEL,
                                      PLOT3D_PROP_SHOW_GRID,
                                      PLOT3D_PROP_SHOW_DATATIP,
                                      PLOT3D_PROP_GRID_COLOR,
                                      PLOT3D_PROP_XAXIS_COLOR,
                                      PLOT3D_PROP_YAXIS_COLOR,
                                      PLOT3D_PROP_ZAXIS_COLOR,
                                      PLOT3D_PROP_TICK_COLOR,
                                      PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS,
                                      PLOT3D_PROP_SHOW_AXIS_TICK,
                                      PLOT3D_PROP_POINT_SIZE,
                                      PLOT3D_PROP_LINE_WIDTH,
                                      PLOT3D_PROP_ENABLE_CACHE,
                                      PLOT3D_PROP_CAMERA_YAW,
                                      PLOT3D_PROP_CAMERA_PITCH,
                                      PLOT3D_PROP_CAMERA_DISTANCE,
                                      PLOT3D_PROP_CAMERA_Z_OFFSET,
                                      PLOT3D_PROP_XY_GRID_POSITION,
                                      PLOT3D_PROP_XZ_GRID_POSITION,
                                      PLOT3D_PROP_YZ_GRID_POSITION,
                                      PLOT3D_PROP_X_GRID_COUNT,
                                      PLOT3D_PROP_Y_GRID_COUNT,
                                      PLOT3D_PROP_Z_GRID_COUNT,
                                      PLOT3D_PROP_EQUAL_AXIS,
                                      PLOT3D_PROP_BOX_ASPECT_X,
                                      PLOT3D_PROP_BOX_ASPECT_Y,
                                      PLOT3D_PROP_BOX_ASPECT_Z,
                                      PLOT3D_PROP_SAMPLE_MODE,
                                      PLOT3D_PROP_SAMPLE_X_RANGE,
                                      PLOT3D_PROP_SAMPLE_Y_RANGE,
                                      PLOT3D_PROP_SAMPLE_STEPS,
                                      PLOT3D_PROP_SAMPLE_T_RANGE,
                                      PLOT3D_PROP_SAMPLE_T_STEPS,
                                      PLOT3D_PROP_SAMPLE_X_EXPR,
                                      PLOT3D_PROP_SAMPLE_Y_EXPR,
                                      PLOT3D_PROP_SAMPLE_Z_EXPR,
                                      PLOT3D_PROP_SAMPLE_Z_MATRIX,
                                      PLOT3D_PROP_SAMPLE_COLOR_EXPR,
                                      PLOT3D_PROP_COLORMAP,
                                      NULL};

/* 复用上面这张总表而不是另写一份清单，新增属性时不会漏同步。 */
static bool_t plot3d_is_known_prop(const char* name) {
  uint32_t i = 0;

  for (i = 0; s_plot3d_properties[i] != NULL; i++) {
    if (tk_str_eq(s_plot3d_properties[i], name)) {
      return TRUE;
    }
  }

  return FALSE;
}

TK_DECL_VTABLE(plot3d) = {.size = sizeof(plot3d_t),
                           .type = WIDGET_TYPE_PLOT3D,
                           .clone_properties = s_plot3d_properties,
                           .persistent_properties = s_plot3d_properties,
                           .parent = TK_PARENT_VTABLE(widget),
                           .create = plot3d_create,
                           .get_prop = plot3d_get_prop,
                           .set_prop = plot3d_set_prop,
                           .on_paint_self = plot3d_on_paint_self,
                           .on_event = plot3d_on_event,
                           .on_destroy = plot3d_on_destroy};

widget_t* plot3d_create(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h) {
  uint32_t i = 0;
  const plot3d_source_vtable_t* vt = NULL;
  plot3d_source_t* source = NULL;
  const plot3d_colorizer_vtable_t* colorizer_vt = NULL;
  plot3d_colorizer_t* colorizer = NULL;
  const plot3d_type_vtable_t* type_vt = NULL;
  plot3d_type_t* type = NULL;
  widget_t* widget = widget_create(parent, TK_REF_VTABLE(plot3d), x, y, w, h);
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, NULL);

  darray_init(&(plot3d->data_points), 32, plot3d_data_point_destroy, NULL);
  darray_init(&(plot3d->draw_primitives), 32, plot3d_primitive_destroy, NULL);

  plot3d->show_grid = TRUE;
  plot3d->show_datatip = TRUE;
  plot3d->show_axis_tick = TRUE;
  plot3d->enable_cache = TRUE;
  plot3d->equal_axis = FALSE;
  plot3d->hover_index = -1;
  plot3d->camera_yaw = -0.8f;
  plot3d->camera_pitch = 0.35f;
  plot3d->camera_distance = 4.5f;
  plot3d->camera_z_offset = 0;
  plot3d->xy_grid_position = 0;
  plot3d->xz_grid_position = 0;
  plot3d->yz_grid_position = 0;
  plot3d->box_aspect_x = 1.0f;
  plot3d->box_aspect_y = 1.0f;
  plot3d->box_aspect_z = 1.0f;
  plot3d->x_grid_count = 5;
  plot3d->y_grid_count = 5;
  plot3d->z_grid_count = 5;
  plot3d->projection_cache_dirty = TRUE;
  plot3d->draw_cache_dirty = TRUE;
  plot3d->grid_color = color_parse("#66666688");
  plot3d->xaxis_color = color_parse("#e74c3cff");
  plot3d->yaxis_color = color_parse("#27ae60ff");
  plot3d->zaxis_color = color_parse("#2980b9ff");
  plot3d->tick_color = color_parse("#888888ff");
  plot3d->axis_negative_brightness = 0.5f;
  plot3d->grid_color_str = tk_str_copy(NULL, "#66666688");
  plot3d->xaxis_color_str = tk_str_copy(NULL, "#e74c3cff");
  plot3d->yaxis_color_str = tk_str_copy(NULL, "#27ae60ff");
  plot3d->zaxis_color_str = tk_str_copy(NULL, "#2980b9ff");
  plot3d->tick_color_str = tk_str_copy(NULL, "#888888ff");
  plot3d->plottype = tk_str_copy(NULL, "dot");
  plot3d->sample_mode = tk_str_copy(NULL, PLOT3D_SAMPLE_MODE_NONE);
  /* 采样相关默认值已全部迁进数据源插件的 create（如 grid 的 sample-steps="20,20"，
   * curve 的 sample-t-range / sample-t-steps）。 */
  plot3d->colormap = tk_str_copy(NULL, "viridis");
  plot3d->xlabel = tk_str_copy(NULL, "X");
  plot3d->ylabel = tk_str_copy(NULL, "Y");
  plot3d->zlabel = tk_str_copy(NULL, "Z");

  /* 一次性建全而不是懒创建：widget_clone 与 XML 加载都会遍历全部属性，等价于把所有插件触发一遍，
   * 懒创建省不下什么；而插件在 create 里设的默认值必须在控件一建好就可读。
   * 单个插件创建失败只告警不让 plot3d_create 整体失败：一个插件 OOM 就整个控件建不出来代价太重，
   * 且 widget_create 已经成功，整体失败还要额外清理；代价是控件会带着残缺的插件集合运行，届时对应
   * 属性会走到「已知属性但无人认领」，由 plot3d_set_prop 判成 RET_BAD_PARAMS。 */
  darray_init(&(plot3d->sources), 4, plot3d_source_destroy_wrapper, NULL);
  for (i = 0; i < plot3d_source_factory_count(); i++) {
    vt = plot3d_source_factory_get(i);
    source = vt->create();
    if (source == NULL) {
      log_warn("plot3d: create source \"%s\" failed.\n", vt->type);
      continue;
    }

    if (darray_push(&(plot3d->sources), source) != RET_OK) {
      log_warn("plot3d: push source \"%s\" failed.\n", vt->type);
      plot3d_source_destroy(source);
    }
  }

  /* 配色插件与数据源同样一次性建全，理由见上。 */
  darray_init(&(plot3d->colorizers), 2, plot3d_colorizer_destroy_wrapper, NULL);
  for (i = 0; i < plot3d_colorizer_factory_count(); i++) {
    colorizer_vt = plot3d_colorizer_factory_get(i);
    colorizer = colorizer_vt->create();
    if (colorizer == NULL) {
      log_warn("plot3d: create colorizer \"%s\" failed.\n", colorizer_vt->type);
      continue;
    }

    if (darray_push(&(plot3d->colorizers), colorizer) != RET_OK) {
      log_warn("plot3d: push colorizer \"%s\" failed.\n", colorizer_vt->type);
      plot3d_colorizer_destroy(colorizer);
    }
  }

  darray_init(&(plot3d->types), 4, plot3d_type_destroy_wrapper, NULL);
  for (i = 0; i < plot3d_type_factory_count(); i++) {
    type_vt = plot3d_type_factory_get(i);
    type = type_vt->create();
    if (type == NULL) {
      log_warn("plot3d: create type \"%s\" failed.\n", type_vt->type);
      continue;
    }

    if (darray_push(&(plot3d->types), type) != RET_OK) {
      log_warn("plot3d: push type \"%s\" failed.\n", type_vt->type);
      plot3d_type_destroy(type);
    }
  }

  return widget;
}

widget_t* plot3d_cast(widget_t* widget) {
  return_value_if_fail(WIDGET_IS_INSTANCE_OF(widget, plot3d), NULL);

  return widget;
}
