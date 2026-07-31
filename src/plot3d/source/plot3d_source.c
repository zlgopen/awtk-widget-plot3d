/**
 * File:   plot3d_source.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 数据源插件接口
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

#include "tkc/mem.h"
#include "tkc/utils.h"

/* 只有接口头文件受「不引入 plot3d.h」的约束（它会被每个插件包含），本编译单元不受此限：
 * plot3d_source_set_range_prop 要用到 plot3d_parse_range。 */
#include "plot3d/plot3d.h"
#include "plot3d_source.h"

/* 注册表只存 vtable 指针，vtable 由插件以静态变量的形式持有，无需析构函数。 */
static darray_t* s_source_factory = NULL;

static int32_t plot3d_source_vtable_cmp(const plot3d_source_vtable_t* iter, const char* type) {
  return strcmp(iter->type, type);
}

ret_t plot3d_source_result_init(plot3d_source_result_t* result, darray_t* points) {
  return_value_if_fail(result != NULL, RET_BAD_PARAMS);

  memset(result, 0x00, sizeof(*result));
  result->points = points;

  return RET_OK;
}

ret_t plot3d_source_sample(plot3d_source_t* source, plot3d_source_result_t* result) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(result != NULL, RET_BAD_PARAMS);

  if (source->vt->sample == NULL) {
    return RET_NOT_IMPL;
  }

  return source->vt->sample(source, result);
}

ret_t plot3d_source_set_prop(plot3d_source_t* source, const char* name, const value_t* v) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (source->vt->set_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return source->vt->set_prop(source, name, v);
}

ret_t plot3d_source_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL && v != NULL, RET_BAD_PARAMS);

  if (source->vt->get_prop == NULL) {
    return RET_NOT_IMPL;
  }

  return source->vt->get_prop(source, name, v);
}

ret_t plot3d_source_set_data(plot3d_source_t* source, const char* name, void* p1, void* p2) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);
  return_value_if_fail(name != NULL, RET_BAD_PARAMS);

  if (source->vt->set_data == NULL) {
    return RET_NOT_IMPL;
  }

  return source->vt->set_data(source, name, p1, p2);
}

bool_t plot3d_source_has_data(plot3d_source_t* source) {
  return_value_if_fail(source != NULL && source->vt != NULL, FALSE);

  /* has_data 会落在绘制路径上，未实现是合法配置，不能每帧刷一条 warning。 */
  if (source->vt->has_data == NULL) {
    return FALSE;
  }

  return source->vt->has_data(source);
}

ret_t plot3d_source_reset(plot3d_source_t* source) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);

  if (source->vt->reset == NULL) {
    return RET_NOT_IMPL;
  }

  return source->vt->reset(source);
}

ret_t plot3d_source_destroy(plot3d_source_t* source) {
  return_value_if_fail(source != NULL && source->vt != NULL, RET_BAD_PARAMS);
  /* 与上面几个可选成员不同：destroy 是必填的，为 NULL 意味着对象会泄漏，值得留下日志。 */
  return_value_if_fail(source->vt->destroy != NULL, RET_NOT_IMPL);

  return source->vt->destroy(source);
}

ret_t plot3d_source_set_range_prop(char** text, const char* value) {
  float_t min_v = 0;
  float_t max_v = 0;
  return_value_if_fail(text != NULL, RET_BAD_PARAMS);

  if (TK_STR_IS_EMPTY(value)) {
    /* 清空就彻底清掉：tk_str_copy 传 NULL 只会把字符串截空，属性值还留着。 */
    TKMEM_FREE(*text);

    return RET_OK;
  }

  /* 先校验后落值：格式非法时保持原值。 */
  return_value_if_fail(plot3d_parse_range(value, &min_v, &max_v) == RET_OK, RET_BAD_PARAMS);
  *text = tk_str_copy(*text, value);

  return RET_OK;
}

ret_t plot3d_source_factory_register(const plot3d_source_vtable_t* vt) {
  int32_t index = 0;
  /* create 为 NULL 的插件创建不出实例，destroy 为 NULL 会漏掉实例，都在注册时挡掉。 */
  return_value_if_fail(vt != NULL && vt->type != NULL && vt->create != NULL && vt->destroy != NULL,
                       RET_BAD_PARAMS);

  if (s_source_factory == NULL) {
    s_source_factory = darray_create(8, NULL, (tk_compare_t)plot3d_source_vtable_cmp);
    return_value_if_fail(s_source_factory != NULL, RET_OOM);
  }

  index = darray_find_index(s_source_factory, (void*)(vt->type));
  if (index >= 0) {
    return darray_set(s_source_factory, index, (void*)vt);
  }

  return darray_push(s_source_factory, (void*)vt);
}

plot3d_source_t* plot3d_source_factory_create(const char* type) {
  const plot3d_source_vtable_t* vt = NULL;
  return_value_if_fail(type != NULL, NULL);

  /* 类型未注册是调用者可预期的结果，不作为错误上报，避免刷日志。 */
  if (s_source_factory == NULL) {
    return NULL;
  }

  vt = (const plot3d_source_vtable_t*)darray_find(s_source_factory, (void*)type);
  if (vt == NULL) {
    return NULL;
  }

  return vt->create();
}

uint32_t plot3d_source_factory_count(void) {
  return s_source_factory != NULL ? s_source_factory->size : 0;
}

const plot3d_source_vtable_t* plot3d_source_factory_get(uint32_t index) {
  /* 序号无效是可查询的正常返回，同样不打 warning。 */
  if (s_source_factory == NULL || index >= s_source_factory->size) {
    return NULL;
  }

  return (const plot3d_source_vtable_t*)darray_get(s_source_factory, index);
}

ret_t plot3d_source_factory_deinit(void) {
  if (s_source_factory != NULL) {
    darray_destroy(s_source_factory);
    s_source_factory = NULL;
  }

  return RET_OK;
}
