/**
 * File:   plot3d_colorizer.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 配色插件接口
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

#include <string.h>

#include "plot3d_colorizer.h"

/* 注册表只存 vtable 指针，vtable 由插件以静态变量的形式持有，无需析构函数。 */
static darray_t* s_colorizer_factory = NULL;

static int32_t plot3d_colorizer_vtable_cmp(const plot3d_colorizer_vtable_t* iter,
                                            const char* type) {
  return strcmp(iter->type, type);
}

bool_t plot3d_colorizer_is_active(plot3d_colorizer_t* colorizer) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, FALSE);

  /* is_active 会落在采样路径上，未实现是合法配置，不能每次采样都刷一条 warning。 */
  if (colorizer->vt->is_active == NULL) {
    return FALSE;
  }

  return colorizer->vt->is_active(colorizer);
}

ret_t plot3d_colorizer_eval(plot3d_colorizer_t* colorizer, const plot3d_sample_pos_t* pos,
                             plot3d_color_value_t* out) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(pos != NULL && out != NULL, RET_BAD_PARAMS);

  if (colorizer->vt->eval == NULL) {
    return RET_NOT_IMPL;
  }

  return colorizer->vt->eval(colorizer, pos, out);
}

ret_t plot3d_colorizer_set_prop(plot3d_colorizer_t* colorizer, const char* name,
                                 const value_t* v) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (colorizer->vt->set_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return colorizer->vt->set_prop(colorizer, name, v);
}

ret_t plot3d_colorizer_get_prop(plot3d_colorizer_t* colorizer, const char* name, value_t* v) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (colorizer->vt->get_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return colorizer->vt->get_prop(colorizer, name, v);
}

ret_t plot3d_colorizer_set_data(plot3d_colorizer_t* colorizer, const char* name, void* p1,
                                 void* p2) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL, RET_BAD_PARAMS);

  if (colorizer->vt->set_data == NULL) {
    return RET_NOT_IMPL;
  }

  return colorizer->vt->set_data(colorizer, name, p1, p2);
}

ret_t plot3d_colorizer_destroy(plot3d_colorizer_t* colorizer) {
  return_value_if_fail(colorizer != NULL && colorizer->vt != NULL, RET_BAD_PARAMS);
  /* 与上面几个可选成员不同：destroy 是必填的，为 NULL 意味着对象会泄漏，值得留下日志。 */
  return_value_if_fail(colorizer->vt->destroy != NULL, RET_NOT_IMPL);

  return colorizer->vt->destroy(colorizer);
}

ret_t plot3d_colorizer_factory_register(const plot3d_colorizer_vtable_t* vt) {
  int32_t index = 0;
  /* create 为 NULL 的插件创建不出实例，destroy 为 NULL 会漏掉实例，都在注册时挡掉。 */
  return_value_if_fail(vt != NULL && vt->type != NULL && vt->create != NULL && vt->destroy != NULL,
                       RET_BAD_PARAMS);

  if (s_colorizer_factory == NULL) {
    s_colorizer_factory = darray_create(4, NULL, (tk_compare_t)plot3d_colorizer_vtable_cmp);
    return_value_if_fail(s_colorizer_factory != NULL, RET_OOM);
  }

  index = darray_find_index(s_colorizer_factory, (void*)(vt->type));
  if (index >= 0) {
    return darray_set(s_colorizer_factory, index, (void*)vt);
  }

  return darray_push(s_colorizer_factory, (void*)vt);
}

plot3d_colorizer_t* plot3d_colorizer_factory_create(const char* type) {
  const plot3d_colorizer_vtable_t* vt = NULL;
  return_value_if_fail(type != NULL, NULL);

  /* 类型未注册是调用者可预期的结果，不作为错误上报，避免刷日志。 */
  if (s_colorizer_factory == NULL) {
    return NULL;
  }

  vt = (const plot3d_colorizer_vtable_t*)darray_find(s_colorizer_factory, (void*)type);
  if (vt == NULL) {
    return NULL;
  }

  return vt->create();
}

uint32_t plot3d_colorizer_factory_count(void) {
  return s_colorizer_factory != NULL ? s_colorizer_factory->size : 0;
}

const plot3d_colorizer_vtable_t* plot3d_colorizer_factory_get(uint32_t index) {
  /* 序号无效是可查询的正常返回，同样不打 warning。 */
  if (s_colorizer_factory == NULL || index >= s_colorizer_factory->size) {
    return NULL;
  }

  return (const plot3d_colorizer_vtable_t*)darray_get(s_colorizer_factory, index);
}

ret_t plot3d_colorizer_factory_deinit(void) {
  if (s_colorizer_factory != NULL) {
    darray_destroy(s_colorizer_factory);
    s_colorizer_factory = NULL;
  }

  return RET_OK;
}
