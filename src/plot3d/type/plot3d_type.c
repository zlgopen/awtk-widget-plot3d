/**
 * File:   plot3d_type.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 图型插件接口
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

#include <string.h>

#include "plot3d_type.h"

/* 注册表只存 vtable 指针，vtable 由插件以静态变量的形式持有，无需析构函数。 */
static darray_t* s_type_factory = NULL;

static int32_t plot3d_type_vtable_cmp(const plot3d_type_vtable_t* iter, const char* type) {
  return strcmp(iter->type, type);
}

ret_t plot3d_type_layout_grid(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                               const plot3d_source_result_t* result, darray_t* points) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(ctx != NULL && result != NULL && points != NULL, RET_BAD_PARAMS);

  if (type->vt->layout_grid == NULL) {
    return RET_NOT_IMPL;
  }

  return type->vt->layout_grid(type, ctx, result, points);
}

ret_t plot3d_type_build(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                         darray_t* primitives) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(ctx != NULL && primitives != NULL, RET_BAD_PARAMS);

  if (type->vt->build == NULL) {
    return RET_NOT_IMPL;
  }

  return type->vt->build(type, ctx, primitives);
}

ret_t plot3d_type_push_primitive(darray_t* primitives, plot3d_primitive_type_t type, uint32_t i0,
                                  uint32_t i1, uint32_t i2, float_t depth, color_t color) {
  plot3d_primitive_t* primitive = NULL;
  return_value_if_fail(primitives != NULL, RET_BAD_PARAMS);

  primitive = TKMEM_ZALLOC(plot3d_primitive_t);
  return_value_if_fail(primitive != NULL, RET_OOM);

  primitive->type = type;
  primitive->i0 = i0;
  primitive->i1 = i1;
  primitive->i2 = i2;
  primitive->depth = depth;
  primitive->color = color;

  return darray_push(primitives, primitive);
}

ret_t plot3d_type_on_paint(plot3d_type_t* type, widget_t* widget, canvas_t* c) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);

  if (type->vt->on_paint == NULL) {
    return RET_NOT_IMPL;
  }

  return type->vt->on_paint(type, widget, c);
}

ret_t plot3d_type_set_prop(plot3d_type_t* type, const char* name, const value_t* v) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (type->vt->set_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return type->vt->set_prop(type, name, v);
}

ret_t plot3d_type_get_prop(plot3d_type_t* type, const char* name, value_t* v) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (type->vt->get_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return type->vt->get_prop(type, name, v);
}

ret_t plot3d_type_init_style(plot3d_type_t* type) {
  return_value_if_fail(type != NULL, RET_BAD_PARAMS);

  type->point_size = 4.0f;
  type->line_width = 1.5f;

  return RET_OK;
}

ret_t plot3d_type_set_style_prop(plot3d_type_t* type, const char* name, const value_t* v) {
  return_value_if_fail(type != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, PLOT3D_PROP_POINT_SIZE)) {
    type->point_size = tk_max(1.0f, value_float(v));
    return RET_OK;
  }
  if (tk_str_eq(name, PLOT3D_PROP_LINE_WIDTH)) {
    type->line_width = tk_max(1.0f, value_float(v));
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

ret_t plot3d_type_get_style_prop(plot3d_type_t* type, const char* name, value_t* v) {
  return_value_if_fail(type != NULL && name != NULL && v != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(name, PLOT3D_PROP_POINT_SIZE)) {
    value_set_float(v, type->point_size);
    return RET_OK;
  }
  if (tk_str_eq(name, PLOT3D_PROP_LINE_WIDTH)) {
    value_set_float(v, type->line_width);
    return RET_OK;
  }

  return RET_NOT_FOUND;
}

ret_t plot3d_type_destroy(plot3d_type_t* type) {
  return_value_if_fail(type != NULL && type->vt != NULL, RET_BAD_PARAMS);
  /* 与上面几个可选成员不同：destroy 是必填的，为 NULL 意味着对象会泄漏，值得留下日志。 */
  return_value_if_fail(type->vt->destroy != NULL, RET_NOT_IMPL);

  return type->vt->destroy(type);
}

ret_t plot3d_type_factory_register(const plot3d_type_vtable_t* vt) {
  int32_t index = 0;
  /* create 为 NULL 的插件创建不出实例，destroy 为 NULL 会漏掉实例，都在注册时挡掉。 */
  return_value_if_fail(vt != NULL && vt->type != NULL && vt->create != NULL && vt->destroy != NULL,
                       RET_BAD_PARAMS);

  if (s_type_factory == NULL) {
    s_type_factory = darray_create(4, NULL, (tk_compare_t)plot3d_type_vtable_cmp);
    return_value_if_fail(s_type_factory != NULL, RET_OOM);
  }

  index = darray_find_index(s_type_factory, (void*)(vt->type));
  if (index >= 0) {
    return darray_set(s_type_factory, index, (void*)vt);
  }

  return darray_push(s_type_factory, (void*)vt);
}

plot3d_type_t* plot3d_type_factory_create(const char* type) {
  const plot3d_type_vtable_t* vt = NULL;
  return_value_if_fail(type != NULL, NULL);

  /* 类型未注册是调用者可预期的结果，不作为错误上报，避免刷日志。 */
  if (s_type_factory == NULL) {
    return NULL;
  }

  vt = (const plot3d_type_vtable_t*)darray_find(s_type_factory, (void*)type);
  if (vt == NULL) {
    return NULL;
  }

  return vt->create();
}

uint32_t plot3d_type_factory_count(void) {
  return s_type_factory != NULL ? s_type_factory->size : 0;
}

const plot3d_type_vtable_t* plot3d_type_factory_get(uint32_t index) {
  /* 序号无效是可查询的正常返回，同样不打 warning。 */
  if (s_type_factory == NULL || index >= s_type_factory->size) {
    return NULL;
  }

  return (const plot3d_type_vtable_t*)darray_get(s_type_factory, index);
}

ret_t plot3d_type_factory_deinit(void) {
  if (s_type_factory != NULL) {
    darray_destroy(s_type_factory);
    s_type_factory = NULL;
  }

  return RET_OK;
}
