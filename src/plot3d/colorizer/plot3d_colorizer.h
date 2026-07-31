/**
 * File:   plot3d_colorizer.h
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

#ifndef TK_PLOT3D_COLORIZER_H
#define TK_PLOT3D_COLORIZER_H

/* 与只依赖 tkc 的数据源接口不同，这里只能 include plot3d.h：eval 的入参 plot3d_sample_pos_t、
 * 出参 plot3d_color_value_t 与配色回调 plot3d_color_func_t 都定义在那里。 */
#include "plot3d/plot3d.h"

BEGIN_C_DECLS

struct _plot3d_colorizer_t;
typedef struct _plot3d_colorizer_t plot3d_colorizer_t;

/* type / create / destroy 必填（注册时校验），其余成员可为 NULL，此时对应的转发函数返回
 * RET_NOT_IMPL（is_active 返回 FALSE）。 */
typedef struct _plot3d_colorizer_vtable_t {
  /* 配色插件的类型名，如 "expr"，注册表以它为键。 */
  const char* type;
  plot3d_colorizer_t* (*create)(void);
  /* 是否已被喂入可用的配色数据：为 FALSE 时核心跳过该插件，按 z 在配色表上取色。 */
  bool_t (*is_active)(plot3d_colorizer_t* colorizer);
  /* 核心保证调用前已把 out 填成默认值（is_color 为 FALSE、scalar 为 pos->z），插件求值失败时
   * 不要改写 out，让核心的默认配色兜底。 */
  ret_t (*eval)(plot3d_colorizer_t* colorizer, const plot3d_sample_pos_t* pos,
                plot3d_color_value_t* out);
  /* set_prop 必须严格按三值协议返回，核心的属性广播完全依赖它区分三种情形：
   *   RET_OK          认领该属性且值合法（已落值）；
   *   RET_BAD_PARAMS  认领该属性但值非法（不得改动原值）；
   *   RET_NOT_FOUND   不认识该属性，核心据此继续按自己的属性处理。
   * 值非法时返回 RET_FAIL/RET_OOM 等其它错误码会被广播当成「未认领」静默吞掉。
   * get_prop 同样以 RET_NOT_FOUND 表示不认识该属性。 */
  ret_t (*set_prop)(plot3d_colorizer_t* colorizer, const char* name, const value_t* v);
  ret_t (*get_prop)(plot3d_colorizer_t* colorizer, const char* name, value_t* v);
  /* 传递回调函数、ctx 等 value_t 不便表达的数据，所有权约定见 plot3d_colorizer_set_data。 */
  ret_t (*set_data)(plot3d_colorizer_t* colorizer, const char* name, void* p1, void* p2);
  ret_t (*destroy)(plot3d_colorizer_t* colorizer);
} plot3d_colorizer_vtable_t;

/**
 * @class plot3d_colorizer_t
 * 配色插件的实例，各插件把它作为自己结构体的第一个成员。
 */
struct _plot3d_colorizer_t {
  const plot3d_colorizer_vtable_t* vt;
};

/**
 * @method plot3d_colorizer_is_active
 * 判断配色插件是否已被喂入可用的配色数据。
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 *
 * @return {bool_t} 返回TRUE表示生效，否则表示不生效。
 */
bool_t plot3d_colorizer_is_active(plot3d_colorizer_t* colorizer);

/**
 * @method plot3d_colorizer_eval
 * 求一个采样点的配色。调用之前 out 须已填好默认值。
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 * @param {const plot3d_sample_pos_t*} pos 采样点位置。
 * @param {plot3d_color_value_t*} out 返回配色，求值失败时保持调用方填好的默认值。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colorizer_eval(plot3d_colorizer_t* colorizer, const plot3d_sample_pos_t* pos,
                             plot3d_color_value_t* out);

/**
 * @method plot3d_colorizer_set_prop
 * 设置配色插件的属性。
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 * @param {const char*} name 属性名。
 * @param {const value_t*} v 属性值。
 *
 * @return {ret_t} 返回RET_OK表示插件认领该属性且值合法，RET_BAD_PARAMS表示认领该属性但值
 *                 非法（原值不变），RET_NOT_FOUND表示插件不认识该属性。核心的属性广播依赖这
 *                 三个返回值区分「认领并接受」「认领但拒绝」「未认领」。
 */
ret_t plot3d_colorizer_set_prop(plot3d_colorizer_t* colorizer, const char* name,
                                 const value_t* v);

/**
 * @method plot3d_colorizer_get_prop
 * 获取配色插件的属性。
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 * @param {const char*} name 属性名。
 * @param {value_t*} v 返回属性值。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示插件不认识该属性。
 */
ret_t plot3d_colorizer_get_prop(plot3d_colorizer_t* colorizer, const char* name, value_t* v);

/**
 * @method plot3d_colorizer_set_data
 * 给配色插件设置回调函数、ctx 等 value_t 不便表达的数据。
 *
 * name 沿用 kebab-case，与属性名保持同一套命名。
 *
 * 所有权总则：数据由调用方提供，**由插件决定是拷贝还是借用**，因此每个插件必须逐个声明它支持的
 * name、对应的 p1/p2 含义，以及是拷贝还是借用；借用时还要写明调用方需要维持数据有效的期限。
 * 内置的 expr 插件支持 PLOT3D_COLORIZER_DATA_COLOR_FUNC，见该宏的说明。
 *
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 * @param {const char*} name 数据名。
 * @param {void*} p1 第一个参数，含义与所有权由该插件声明。
 * @param {void*} p2 第二个参数，含义与所有权由该插件声明。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示插件不认识该数据名。
 */
ret_t plot3d_colorizer_set_data(plot3d_colorizer_t* colorizer, const char* name, void* p1,
                                 void* p2);

/**
 * @method plot3d_colorizer_destroy
 * 销毁配色插件对象。
 * @param {plot3d_colorizer_t*} colorizer 配色插件对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colorizer_destroy(plot3d_colorizer_t* colorizer);

/**
 * @method plot3d_colorizer_factory_register
 * 注册配色插件，type 相同时替换原有的插件，以便用户用自己的实现覆盖内置插件。
 * @param {const plot3d_colorizer_vtable_t*} vt 配色插件的虚函数表，需要在注册期间一直有效，
 *                                               其 type/create/destroy 三个成员不能为 NULL。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colorizer_factory_register(const plot3d_colorizer_vtable_t* vt);

/**
 * @method plot3d_colorizer_factory_create
 * 创建指定类型的配色插件对象。
 * @param {const char*} type 配色插件的类型名。
 *
 * @return {plot3d_colorizer_t*} 返回配色插件对象，类型未注册时返回NULL。
 */
plot3d_colorizer_t* plot3d_colorizer_factory_create(const char* type);

/**
 * @method plot3d_colorizer_factory_count
 * 获取已注册的配色插件的个数。
 *
 * @return {uint32_t} 返回已注册的插件个数。
 */
uint32_t plot3d_colorizer_factory_count(void);

/**
 * @method plot3d_colorizer_factory_get
 * 获取指定序号的配色插件的虚函数表。
 * @param {uint32_t} index 序号。
 *
 * @return {const plot3d_colorizer_vtable_t*} 返回虚函数表，序号无效时返回NULL。
 */
const plot3d_colorizer_vtable_t* plot3d_colorizer_factory_get(uint32_t index);

/**
 * @method plot3d_colorizer_factory_deinit
 * 清空配色插件注册表，重复调用是安全的。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colorizer_factory_deinit(void);

/**
 * @method plot3d_colorizer_expr_register
 * 注册表达式配色插件（类型名 "expr"）。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colorizer_expr_register(void);

#define PLOT3D_COLORIZER_TYPE_EXPR "expr"

/* expr 插件的 set_data 数据名：p1 为 plot3d_color_func_t 函数指针（为 NULL 表示取消），
 * p2 为回调上下文。插件只**借用**这两者，ctx 须在插件被销毁或再次设置之前一直有效。 */
#define PLOT3D_COLORIZER_DATA_COLOR_FUNC "color-func"

END_C_DECLS

#endif /*TK_PLOT3D_COLORIZER_H*/
