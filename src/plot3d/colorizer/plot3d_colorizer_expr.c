/**
 * File:   plot3d_colorizer_expr.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 表达式配色插件
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
 * 2026-07-30 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "tkc/color_parser.h"
#include "tkc/fscript.h"
#include "tkc/mem.h"
#include "tkc/object_default.h"
#include "tkc/utils.h"

#include "plot3d_colorizer.h"

typedef struct _plot3d_colorizer_expr_t {
  plot3d_colorizer_t colorizer;

  char* sample_color_expr;
  /* 插件自建的变量表：不与数据源插件共用，免得采样残留的变量被配色表达式读到。 */
  tk_object_t* expr_obj;
  fscript_t* fscript;

  plot3d_color_func_t color_func;
  void* color_func_ctx;
} plot3d_colorizer_expr_t;

/* 虚函数表里要填 create，而 create 又要取虚函数表的地址，这里先给出原型，
 * 定义放在虚函数表之后。 */
static plot3d_colorizer_t* plot3d_colorizer_expr_create(void);

static ret_t plot3d_colorizer_expr_prepare_obj(plot3d_colorizer_expr_t* expr) {
  if (expr->expr_obj == NULL) {
    expr->expr_obj = object_default_create();
  }

  return expr->expr_obj != NULL ? RET_OK : RET_OOM;
}

static ret_t plot3d_colorizer_expr_set_expr(plot3d_colorizer_expr_t* expr, const char* text) {
  bool_t has_expr = !TK_STR_IS_EMPTY(text);
  fscript_t* fscript = NULL;

  if (has_expr) {
    fscript_parser_error_t error;
    ret_t ret = RET_OK;

    return_value_if_fail(plot3d_colorizer_expr_prepare_obj(expr) == RET_OK, RET_OOM);

    /* 先校验语法：非法表达式不覆盖现有的，避免图突然变空。 */
    ret = fscript_syntax_check(expr->expr_obj, text, &error);
    fscript_parser_error_deinit(&error);
    if (ret == RET_OK) {
      fscript = fscript_create(expr->expr_obj, text);
    }

    return_value_if_fail(fscript != NULL, RET_BAD_PARAMS);
  }

  if (expr->fscript != NULL) {
    fscript_destroy(expr->fscript);
  }
  expr->fscript = fscript;

  if (has_expr) {
    expr->sample_color_expr = tk_str_copy(expr->sample_color_expr, text);
  } else {
    /* 清空就彻底清掉：tk_str_copy 传 NULL 只会把字符串截空，属性值还留着。 */
    TKMEM_FREE(expr->sample_color_expr);
  }

  return RET_OK;
}

/* 配色函数与配色表达式都没有时不生效，核心退回按 z 在配色表上取色。 */
static bool_t plot3d_colorizer_expr_is_active(plot3d_colorizer_t* colorizer) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  return expr->color_func != NULL || expr->fscript != NULL;
}

/* 配色函数优先于配色表达式：函数是代码里显式设置的，比存在属性里的表达式意图更明确。 */
static ret_t plot3d_colorizer_expr_eval(plot3d_colorizer_t* colorizer,
                                         const plot3d_sample_pos_t* pos,
                                         plot3d_color_value_t* out) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  if (expr->color_func != NULL) {
    color_t color;

    /* 先算到局部变量：回调失败时 out 一个字节都不能动，否则核心填好的默认配色就被绕过了。 */
    if (expr->color_func(expr->color_func_ctx, pos->x, pos->y, pos->z, &color) == RET_OK) {
      out->is_color = TRUE;
      out->color = color;
    }

    return RET_OK;
  }

  if (expr->fscript != NULL) {
    value_t result;

    tk_object_set_prop_float(expr->expr_obj, "x", pos->x);
    tk_object_set_prop_float(expr->expr_obj, "y", pos->y);
    tk_object_set_prop_float(expr->expr_obj, "z", pos->z);
    if (pos->has_t) {
      tk_object_set_prop_float(expr->expr_obj, "t", pos->t);
    } else {
      /* 变量表由本插件长期复用，不写 t 会读到上一次曲线采样留下的值，只能显式移除。 */
      tk_object_remove_prop(expr->expr_obj, "t");
    }

    /* 求值失败时不改写 out，由核心填好的默认值兜底。 */
    if (fscript_exec(expr->fscript, &result) == RET_OK) {
      if (result.type == VALUE_TYPE_STRING) {
        out->is_color = TRUE;
        out->color = color_parse(value_str(&result));
      } else {
        out->scalar = value_float(&result);
      }
      value_reset(&result);
    }
  }

  return RET_OK;
}

static ret_t plot3d_colorizer_expr_set_prop(plot3d_colorizer_t* colorizer, const char* name,
                                             const value_t* v) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_COLOR_EXPR)) {
    /* 三值协议只认 RET_BAD_PARAMS 这一种「认领但拒绝」，OOM 之类也一并归到它。 */
    if (plot3d_colorizer_expr_set_expr(expr, value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_colorizer_expr_get_prop(plot3d_colorizer_t* colorizer, const char* name,
                                             value_t* v) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_COLOR_EXPR)) {
    value_set_str(v, expr->sample_color_expr);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_colorizer_expr_set_data(plot3d_colorizer_t* colorizer, const char* name,
                                             void* p1, void* p2) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  if (tk_str_eq(name, PLOT3D_COLORIZER_DATA_COLOR_FUNC)) {
    /* 只借用：ctx 的生命周期由调用方负责。 */
    expr->color_func = (plot3d_color_func_t)p1;
    expr->color_func_ctx = p2;

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_colorizer_expr_destroy(plot3d_colorizer_t* colorizer) {
  plot3d_colorizer_expr_t* expr = (plot3d_colorizer_expr_t*)colorizer;

  if (expr->fscript != NULL) {
    fscript_destroy(expr->fscript);
  }
  /* fscript 自己持有 expr_obj 的引用，这里只释放本插件那一份。 */
  if (expr->expr_obj != NULL) {
    TK_OBJECT_UNREF(expr->expr_obj);
  }
  TKMEM_FREE(expr->sample_color_expr);
  TKMEM_FREE(expr);

  return RET_OK;
}

static const plot3d_colorizer_vtable_t s_plot3d_colorizer_expr_vtable = {
    .type = PLOT3D_COLORIZER_TYPE_EXPR,
    .create = plot3d_colorizer_expr_create,
    .is_active = plot3d_colorizer_expr_is_active,
    .eval = plot3d_colorizer_expr_eval,
    .set_prop = plot3d_colorizer_expr_set_prop,
    .get_prop = plot3d_colorizer_expr_get_prop,
    .set_data = plot3d_colorizer_expr_set_data,
    .destroy = plot3d_colorizer_expr_destroy};

/* 配色的几个属性目前都没有非零默认值，清零即为默认状态。 */
static plot3d_colorizer_t* plot3d_colorizer_expr_create(void) {
  plot3d_colorizer_expr_t* expr = TKMEM_ZALLOC(plot3d_colorizer_expr_t);
  return_value_if_fail(expr != NULL, NULL);

  expr->colorizer.vt = &s_plot3d_colorizer_expr_vtable;

  return (plot3d_colorizer_t*)expr;
}

ret_t plot3d_colorizer_expr_register(void) {
  return plot3d_colorizer_factory_register(&s_plot3d_colorizer_expr_vtable);
}
