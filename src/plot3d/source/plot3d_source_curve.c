/**
 * File:   plot3d_source_curve.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 曲线数据源插件
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

/* 本插件支持的 set_data 数据名只有 PLOT3D_SOURCE_DATA_CURVE_FUNC，其 p1/p2 含义与所有权约定写在
 * plot3d_source.h 中该宏的注释上——调用方在核心里，读不到本文件。 */

#include "tkc/fscript.h"
#include "tkc/mem.h"
#include "tkc/object_default.h"
#include "tkc/utils.h"

#include "plot3d/plot3d.h"
#include "plot3d_source.h"

typedef struct _plot3d_source_curve_t {
  plot3d_source_t source;

  /* 配置属性：reset 时不清（t 范围/点数）；sample-z-expr 与 grid 共享，同样不清。 */
  char* sample_t_range;
  uint32_t sample_t_steps;
  char* sample_x_expr;
  char* sample_y_expr;
  char* sample_z_expr;

  plot3d_curve_func_t curve_func;
  void* curve_func_ctx;

  /* 三个表达式共用一个变量表：fscript 自己持有它的引用，插件销毁时再释放自己那份。 */
  tk_object_t* expr_obj;
  fscript_t* expr_x_fscript;
  fscript_t* expr_y_fscript;
  fscript_t* expr_z_fscript;

  /* 与 points 等长的参数 t，由本插件持有并复用，经 result.ts 借给核心配色。 */
  float_t* ts;
  uint32_t ts_capacity;
} plot3d_source_curve_t;

/* 虚函数表里要填 create，而 create 又要取虚函数表的地址，这里先给出原型，
 * 定义放在虚函数表之后。 */
static plot3d_source_t* plot3d_source_curve_create(void);

static float_t plot3d_source_curve_axis_value(float_t v0, float_t v1, uint32_t index,
                                               uint32_t count) {
  return count > 1 ? v0 + (v1 - v0) * index / (count - 1) : v0;
}

static ret_t plot3d_source_curve_prepare_expr_obj(plot3d_source_curve_t* curve) {
  if (curve->expr_obj == NULL) {
    curve->expr_obj = object_default_create();
  }

  return curve->expr_obj != NULL ? RET_OK : RET_OOM;
}

static ret_t plot3d_source_curve_set_one_expr(plot3d_source_curve_t* curve, char** text,
                                               fscript_t** out_fscript, const char* expr) {
  bool_t has_expr = !TK_STR_IS_EMPTY(expr);
  fscript_t* fscript = NULL;

  if (has_expr) {
    fscript_parser_error_t error;
    ret_t ret = RET_OK;

    return_value_if_fail(plot3d_source_curve_prepare_expr_obj(curve) == RET_OK, RET_OOM);

    /* 先校验语法：非法表达式不覆盖现有的，避免图突然变空。 */
    ret = fscript_syntax_check(curve->expr_obj, expr, &error);
    fscript_parser_error_deinit(&error);
    if (ret == RET_OK) {
      fscript = fscript_create(curve->expr_obj, expr);
    }

    return_value_if_fail(fscript != NULL, RET_BAD_PARAMS);
  }

  if (*out_fscript != NULL) {
    fscript_destroy(*out_fscript);
  }
  *out_fscript = fscript;
  if (has_expr) {
    *text = tk_str_copy(*text, expr);
  } else {
    /* 清空就彻底清掉：tk_str_copy 传 NULL 只会把字符串截空，属性值还留着。 */
    TKMEM_FREE(*text);
  }

  return RET_OK;
}

static ret_t plot3d_source_curve_exec_expr(plot3d_source_curve_t* curve, fscript_t* fscript,
                                            float_t* out) {
  value_t result;
  return_value_if_fail(fscript != NULL, RET_BAD_PARAMS);
  return_value_if_fail(fscript_exec(fscript, &result) == RET_OK, RET_FAIL);

  *out = value_float(&result);
  value_reset(&result);

  return RET_OK;
}

/* 采样函数优先于表达式：函数是代码里显式设置的，比存在属性里的表达式意图更明确。 */
static ret_t plot3d_source_curve_sample_point(plot3d_source_curve_t* curve, float_t t, float_t* x,
                                               float_t* y, float_t* z) {
  if (curve->curve_func != NULL) {
    return curve->curve_func(curve->curve_func_ctx, t, x, y, z);
  }

  *x = t;
  *y = 0;
  *z = 0;

  tk_object_set_prop_float(curve->expr_obj, "t", t);
  if (curve->expr_x_fscript != NULL) {
    plot3d_source_curve_exec_expr(curve, curve->expr_x_fscript, x);
  }
  tk_object_set_prop_float(curve->expr_obj, "x", *x);

  if (curve->expr_y_fscript != NULL) {
    plot3d_source_curve_exec_expr(curve, curve->expr_y_fscript, y);
  }
  tk_object_set_prop_float(curve->expr_obj, "y", *y);

  if (curve->expr_z_fscript != NULL) {
    plot3d_source_curve_exec_expr(curve, curve->expr_z_fscript, z);
  }

  return RET_OK;
}

static ret_t plot3d_source_curve_push_point(darray_t* points, float_t x, float_t y, float_t z) {
  plot3d_data_point_t* point = TKMEM_ZALLOC(plot3d_data_point_t);
  return_value_if_fail(point != NULL, RET_OOM);

  point->x = x;
  point->y = y;
  point->z = z;
  point->is_break = FALSE;

  if (darray_push(points, point) != RET_OK) {
    TKMEM_FREE(point);

    return RET_OOM;
  }

  return RET_OK;
}

static bool_t plot3d_source_curve_has_data(plot3d_source_t* source) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  return curve->curve_func != NULL || curve->expr_x_fscript != NULL ||
         curve->expr_y_fscript != NULL || curve->expr_z_fscript != NULL;
}

static ret_t plot3d_source_curve_ensure_ts(plot3d_source_curve_t* curve, uint32_t steps) {
  float_t* ts = NULL;

  if (curve->ts_capacity >= steps) {
    return RET_OK;
  }

  ts = TKMEM_REALLOCT(float_t, curve->ts, steps);
  return_value_if_fail(ts != NULL, RET_OOM);
  curve->ts = ts;
  curve->ts_capacity = steps;

  return RET_OK;
}

static ret_t plot3d_source_curve_sample(plot3d_source_t* source, plot3d_source_result_t* result) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;
  float_t t0 = 0;
  float_t t1 = 1;
  uint32_t steps = 0;
  uint32_t i = 0;
  return_value_if_fail(plot3d_source_curve_has_data(source), RET_BAD_PARAMS);
  return_value_if_fail(result->points != NULL, RET_BAD_PARAMS);

  plot3d_parse_range(curve->sample_t_range, &t0, &t1);
  steps = curve->sample_t_steps;
  return_value_if_fail(steps >= PLOT3D_MIN_SAMPLE_STEPS, RET_BAD_PARAMS);
  return_value_if_fail(plot3d_source_curve_ensure_ts(curve, steps) == RET_OK, RET_OOM);

  for (i = 0; i < steps; i++) {
    float_t t = plot3d_source_curve_axis_value(t0, t1, i, steps);
    float_t x = 0;
    float_t y = 0;
    float_t z = 0;

    if (plot3d_source_curve_sample_point(curve, t, &x, &y, &z) != RET_OK) {
      x = y = z = 0;
    }

    curve->ts[i] = t;
    if (plot3d_source_curve_push_point(result->points, x, y, z) != RET_OK) {
      return RET_OOM;
    }
  }

  result->type = PLOT3D_SOURCE_RESULT_POINTS;
  /* 曲线点不自带颜色，核心按 colormap / 配色插件补色，并靠 ts 填 has_t。 */
  result->has_color = FALSE;
  result->ts = curve->ts;

  return RET_OK;
}

static ret_t plot3d_source_curve_set_prop(plot3d_source_t* source, const char* name,
                                          const value_t* v) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_T_RANGE)) {
    if (plot3d_source_set_range_prop(&(curve->sample_t_range), value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_T_STEPS)) {
    uint32_t steps = (uint32_t)value_int(v);

    curve->sample_t_steps =
        tk_min(tk_max(steps, PLOT3D_MIN_SAMPLE_STEPS), PLOT3D_MAX_CURVE_STEPS);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_EXPR)) {
    if (plot3d_source_curve_set_one_expr(curve, &(curve->sample_x_expr), &(curve->expr_x_fscript),
                                          value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_EXPR)) {
    if (plot3d_source_curve_set_one_expr(curve, &(curve->sample_y_expr), &(curve->expr_y_fscript),
                                          value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_EXPR)) {
    if (plot3d_source_curve_set_one_expr(curve, &(curve->sample_z_expr), &(curve->expr_z_fscript),
                                          value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_curve_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_T_RANGE)) {
    value_set_str(v, curve->sample_t_range);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_T_STEPS)) {
    value_set_uint32(v, curve->sample_t_steps);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_EXPR)) {
    value_set_str(v, curve->sample_x_expr);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_EXPR)) {
    value_set_str(v, curve->sample_y_expr);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_EXPR)) {
    value_set_str(v, curve->sample_z_expr);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_curve_set_data(plot3d_source_t* source, const char* name, void* p1,
                                          void* p2) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  if (!tk_str_eq(name, PLOT3D_SOURCE_DATA_CURVE_FUNC)) {
    return RET_NOT_FOUND;
  }

  curve->curve_func = (plot3d_curve_func_t)p1;
  curve->curve_func_ctx = p2;

  return RET_OK;
}

/* 让位时只清回调：与 plot3d_set_dataset 迁走前只清 curve_func 的历史行为对齐。
 * sample-z-expr 与 grid 共享，清掉会让两份副本分叉；x/y 表达式虽是 curve 独有，但旧核心在
 * set_dataset 时也不清它们；t 范围与点数是配置属性。 */
static ret_t plot3d_source_curve_reset(plot3d_source_t* source) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  curve->curve_func = NULL;
  curve->curve_func_ctx = NULL;

  return RET_OK;
}

static ret_t plot3d_source_curve_destroy(plot3d_source_t* source) {
  plot3d_source_curve_t* curve = (plot3d_source_curve_t*)source;

  TKMEM_FREE(curve->sample_t_range);
  TKMEM_FREE(curve->sample_x_expr);
  TKMEM_FREE(curve->sample_y_expr);
  TKMEM_FREE(curve->sample_z_expr);
  TKMEM_FREE(curve->ts);

  if (curve->expr_x_fscript != NULL) {
    fscript_destroy(curve->expr_x_fscript);
  }
  if (curve->expr_y_fscript != NULL) {
    fscript_destroy(curve->expr_y_fscript);
  }
  if (curve->expr_z_fscript != NULL) {
    fscript_destroy(curve->expr_z_fscript);
  }
  if (curve->expr_obj != NULL) {
    TK_OBJECT_UNREF(curve->expr_obj);
  }

  TKMEM_FREE(curve);

  return RET_OK;
}

/* 类型名与采样模式同名，核心据此从实例表里找到本插件。 */
static const plot3d_source_vtable_t s_plot3d_source_curve_vtable = {
    .type = PLOT3D_SAMPLE_MODE_CURVE,
    .create = plot3d_source_curve_create,
    .sample = plot3d_source_curve_sample,
    .set_prop = plot3d_source_curve_set_prop,
    .get_prop = plot3d_source_curve_get_prop,
    .set_data = plot3d_source_curve_set_data,
    .has_data = plot3d_source_curve_has_data,
    .reset = plot3d_source_curve_reset,
    .destroy = plot3d_source_curve_destroy};

static plot3d_source_t* plot3d_source_curve_create(void) {
  plot3d_source_curve_t* curve = TKMEM_ZALLOC(plot3d_source_curve_t);
  return_value_if_fail(curve != NULL, NULL);

  curve->source.vt = &s_plot3d_source_curve_vtable;
  /* 默认值必须在 create 里设：控件一建好 get_prop 就要读得回来，curve 也靠它们采点。 */
  curve->sample_t_range = tk_str_copy(NULL, "0,1");
  curve->sample_t_steps = PLOT3D_DEFAULT_CURVE_STEPS;

  return (plot3d_source_t*)curve;
}

ret_t plot3d_source_curve_register(void) {
  return plot3d_source_factory_register(&s_plot3d_source_curve_vtable);
}
