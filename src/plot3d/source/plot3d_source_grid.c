/**
 * File:   plot3d_source_grid.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 网格数据源插件
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

/* 本插件支持的 set_data 数据名只有 PLOT3D_SOURCE_DATA_GRID_FUNC，其 p1/p2 含义与所有权约定写在
 * plot3d_source.h 中该宏的注释上——调用方在核心里，读不到本文件。 */

#include "tkc/fscript.h"
#include "tkc/mem.h"
#include "tkc/object_default.h"
#include "tkc/utils.h"

#include "plot3d/plot3d.h"
#include "plot3d_source.h"

typedef struct _plot3d_source_grid_t {
  plot3d_source_t source;

  /* 配置属性：reset 时不清。x/y 范围与 matrix 共享，sample-z-expr 与 curve 共享，
   * sample-steps 虽是 grid 独用，同样是配置而非「数据源身份」。 */
  char* sample_x_range;
  char* sample_y_range;
  char* sample_steps;
  char* sample_z_expr;

  plot3d_grid_func_t grid_func;
  void* grid_func_ctx;

  /* z 表达式的变量表：fscript 自己持有它的引用，插件销毁时再释放自己那份。 */
  tk_object_t* expr_obj;
  fscript_t* expr_z_fscript;

  /* sample 产出的 z 网格，由本插件持有并复用，经 result.zs 借给核心。 */
  float_t* zs;
  uint32_t zs_capacity;
} plot3d_source_grid_t;

/* 虚函数表里要填 create，而 create 又要取虚函数表的地址，这里先给出原型，
 * 定义放在虚函数表之后。 */
static plot3d_source_t* plot3d_source_grid_create(void);

static float_t plot3d_source_grid_axis_value(float_t v0, float_t v1, uint32_t index,
                                              uint32_t count) {
  return count > 1 ? v0 + (v1 - v0) * index / (count - 1) : v0;
}

static ret_t plot3d_source_grid_prepare_expr_obj(plot3d_source_grid_t* grid) {
  if (grid->expr_obj == NULL) {
    grid->expr_obj = object_default_create();
  }

  return grid->expr_obj != NULL ? RET_OK : RET_OOM;
}

static ret_t plot3d_source_grid_set_one_expr(plot3d_source_grid_t* grid, char** text,
                                              fscript_t** out_fscript, const char* expr) {
  bool_t has_expr = !TK_STR_IS_EMPTY(expr);
  fscript_t* fscript = NULL;

  if (has_expr) {
    fscript_parser_error_t error;
    ret_t ret = RET_OK;

    return_value_if_fail(plot3d_source_grid_prepare_expr_obj(grid) == RET_OK, RET_OOM);

    /* 先校验语法：非法表达式不覆盖现有的，避免图突然变空。 */
    ret = fscript_syntax_check(grid->expr_obj, expr, &error);
    fscript_parser_error_deinit(&error);
    if (ret == RET_OK) {
      fscript = fscript_create(grid->expr_obj, expr);
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

static ret_t plot3d_source_grid_exec_expr(plot3d_source_grid_t* grid, fscript_t* fscript,
                                           float_t* out) {
  value_t result;
  return_value_if_fail(fscript != NULL, RET_BAD_PARAMS);
  return_value_if_fail(fscript_exec(fscript, &result) == RET_OK, RET_FAIL);

  *out = value_float(&result);
  value_reset(&result);

  return RET_OK;
}

/* 采样函数优先于表达式：函数是代码里显式设置的，比存在属性里的表达式意图更明确。 */
static ret_t plot3d_source_grid_sample_z(plot3d_source_grid_t* grid, float_t x, float_t y,
                                          float_t* z) {
  if (grid->grid_func != NULL) {
    return grid->grid_func(grid->grid_func_ctx, x, y, z);
  }

  tk_object_set_prop_float(grid->expr_obj, "x", x);
  tk_object_set_prop_float(grid->expr_obj, "y", y);

  return plot3d_source_grid_exec_expr(grid, grid->expr_z_fscript, z);
}

static bool_t plot3d_source_grid_has_data(plot3d_source_t* source) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  return grid->grid_func != NULL || grid->expr_z_fscript != NULL;
}

static ret_t plot3d_source_grid_ensure_zs(plot3d_source_grid_t* grid, uint32_t nr) {
  float_t* zs = NULL;

  if (grid->zs_capacity >= nr) {
    return RET_OK;
  }

  zs = TKMEM_REALLOCT(float_t, grid->zs, nr);
  return_value_if_fail(zs != NULL, RET_OOM);
  grid->zs = zs;
  grid->zs_capacity = nr;

  return RET_OK;
}

static ret_t plot3d_source_grid_sample(plot3d_source_t* source, plot3d_source_result_t* result) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;
  float_t x0 = 0;
  float_t x1 = 1;
  float_t y0 = 0;
  float_t y1 = 1;
  uint32_t cols = 0;
  uint32_t rows = 0;
  uint32_t i = 0;
  uint32_t j = 0;
  return_value_if_fail(plot3d_source_grid_has_data(source), RET_BAD_PARAMS);

  /* 范围为空表示不指定：按 0~1 铺开（与迁走前核心 grid 采样一致）。 */
  plot3d_parse_range(grid->sample_x_range, &x0, &x1);
  plot3d_parse_range(grid->sample_y_range, &y0, &y1);
  return_value_if_fail(plot3d_parse_steps(grid->sample_steps, &cols, &rows) == RET_OK,
                       RET_BAD_PARAMS);
  return_value_if_fail(plot3d_source_grid_ensure_zs(grid, cols * rows) == RET_OK, RET_OOM);

  for (j = 0; j < rows; j++) {
    float_t y = plot3d_source_grid_axis_value(y0, y1, j, rows);

    for (i = 0; i < cols; i++) {
      float_t x = plot3d_source_grid_axis_value(x0, x1, i, cols);
      float_t z = 0;

      if (plot3d_source_grid_sample_z(grid, x, y, &z) != RET_OK) {
        z = 0;
      }

      grid->zs[j * cols + i] = z;
    }
  }

  result->type = PLOT3D_SOURCE_RESULT_GRID;
  result->zs = grid->zs;
  result->cols = cols;
  result->rows = rows;
  result->x0 = x0;
  result->x1 = x1;
  result->y0 = y0;
  result->y1 = y1;

  return RET_OK;
}

static ret_t plot3d_source_grid_set_prop(plot3d_source_t* source, const char* name,
                                          const value_t* v) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_RANGE)) {
    if (plot3d_source_set_range_prop(&(grid->sample_x_range), value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_RANGE)) {
    if (plot3d_source_set_range_prop(&(grid->sample_y_range), value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_STEPS)) {
    uint32_t cols = 0;
    uint32_t rows = 0;

    /* 先校验后落值：非法格点数不覆盖现有的。 */
    if (plot3d_parse_steps(value_str(v), &cols, &rows) != RET_OK) {
      return RET_BAD_PARAMS;
    }
    grid->sample_steps = tk_str_copy(grid->sample_steps, value_str(v));

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_EXPR)) {
    if (plot3d_source_grid_set_one_expr(grid, &(grid->sample_z_expr), &(grid->expr_z_fscript),
                                         value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_grid_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_RANGE)) {
    value_set_str(v, grid->sample_x_range);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_RANGE)) {
    value_set_str(v, grid->sample_y_range);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_STEPS)) {
    value_set_str(v, grid->sample_steps);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_EXPR)) {
    value_set_str(v, grid->sample_z_expr);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_grid_set_data(plot3d_source_t* source, const char* name, void* p1,
                                         void* p2) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  if (!tk_str_eq(name, PLOT3D_SOURCE_DATA_GRID_FUNC)) {
    return RET_NOT_FOUND;
  }

  grid->grid_func = (plot3d_grid_func_t)p1;
  grid->grid_func_ctx = p2;

  return RET_OK;
}

/* 让位时只清回调：与 plot3d_set_dataset 迁走前只清 grid_func 的历史行为对齐。
 * 范围 / steps / z-expr 是配置属性（前两者与 matrix 共享，后者与 curve 共享），清掉会让副本分叉。 */
static ret_t plot3d_source_grid_reset(plot3d_source_t* source) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  grid->grid_func = NULL;
  grid->grid_func_ctx = NULL;

  return RET_OK;
}

static ret_t plot3d_source_grid_destroy(plot3d_source_t* source) {
  plot3d_source_grid_t* grid = (plot3d_source_grid_t*)source;

  TKMEM_FREE(grid->sample_x_range);
  TKMEM_FREE(grid->sample_y_range);
  TKMEM_FREE(grid->sample_steps);
  TKMEM_FREE(grid->sample_z_expr);
  TKMEM_FREE(grid->zs);

  if (grid->expr_z_fscript != NULL) {
    fscript_destroy(grid->expr_z_fscript);
  }
  if (grid->expr_obj != NULL) {
    TK_OBJECT_UNREF(grid->expr_obj);
  }

  TKMEM_FREE(grid);

  return RET_OK;
}

/* 类型名与采样模式同名，核心据此从实例表里找到本插件。 */
static const plot3d_source_vtable_t s_plot3d_source_grid_vtable = {
    .type = PLOT3D_SAMPLE_MODE_GRID,
    .create = plot3d_source_grid_create,
    .sample = plot3d_source_grid_sample,
    .set_prop = plot3d_source_grid_set_prop,
    .get_prop = plot3d_source_grid_get_prop,
    .set_data = plot3d_source_grid_set_data,
    .has_data = plot3d_source_grid_has_data,
    .reset = plot3d_source_grid_reset,
    .destroy = plot3d_source_grid_destroy};

static plot3d_source_t* plot3d_source_grid_create(void) {
  plot3d_source_grid_t* grid = TKMEM_ZALLOC(plot3d_source_grid_t);
  return_value_if_fail(grid != NULL, NULL);

  grid->source.vt = &s_plot3d_source_grid_vtable;
  /* 默认值必须在 create 里设：漏掉则 parse_steps(NULL) 失败，grid 一个点都采不出来。 */
  grid->sample_steps = tk_str_copy(NULL, "20,20");

  return (plot3d_source_t*)grid;
}

ret_t plot3d_source_grid_register(void) {
  return plot3d_source_factory_register(&s_plot3d_source_grid_vtable);
}
