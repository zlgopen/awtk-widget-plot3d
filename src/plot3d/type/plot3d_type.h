/**
 * File:   plot3d_type.h
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

#ifndef TK_PLOT3D_TYPE_H
#define TK_PLOT3D_TYPE_H

/* 与 colorizer 一样只能 include plot3d.h：on_paint 入参是 widget_t / canvas_t，投影点里的
 * color_t 也来自那里。layout_grid 还要用到 plot3d_source_result_t。 */
#include "plot3d/plot3d.h"
#include "plot3d/source/plot3d_source.h"

BEGIN_C_DECLS

struct _plot3d_type_t;
typedef struct _plot3d_type_t plot3d_type_t;

/**
 * @class plot3d_projected_point_t
 * @prefix plot3d_projected_point
 * 投影后的三维点。build 的输入必须是这类点，而不是原始数据点。
 */
typedef struct _plot3d_projected_point_t {
  /**
   * @property {float_t} x
   * @annotation ["readable"]
   * 原始 X 坐标。
   */
  float_t x;
  /**
   * @property {float_t} y
   * @annotation ["readable"]
   * 原始 Y 坐标。
   */
  float_t y;
  /**
   * @property {float_t} z
   * @annotation ["readable"]
   * 原始 Z 坐标。
   */
  float_t z;
  /**
   * @property {float_t} sx
   * @annotation ["readable"]
   * 屏幕 X 坐标。
   */
  float_t sx;
  /**
   * @property {float_t} sy
   * @annotation ["readable"]
   * 屏幕 Y 坐标。
   */
  float_t sy;
  /**
   * @property {float_t} rx
   * @annotation ["readable"]
   * 旋转后的 X，供法线与光照计算。
   */
  float_t rx;
  /**
   * @property {float_t} ry
   * @annotation ["readable"]
   * 旋转后的 Y，供法线与光照计算。
   */
  float_t ry;
  /**
   * @property {float_t} rz
   * @annotation ["readable"]
   * 旋转后的 Z，供法线与光照计算。
   */
  float_t rz;
  /**
   * @property {float_t} depth
   * @annotation ["readable"]
   * 深度，供图元排序。
   */
  float_t depth;
  /**
   * @property {color_t} color
   * @annotation ["readable"]
   * 点颜色。
   */
  color_t color;
  /**
   * @property {bool_t} is_break
   * @annotation ["readable"]
   * 是否为折线分段点。
   */
  bool_t is_break;
  /**
   * @property {bool_t} valid
   * @annotation ["readable"]
   * 是否参与绘制；为 FALSE 时被剔除。
   */
  bool_t valid;
} plot3d_projected_point_t;

/**
 * @enum plot3d_primitive_type_t
 * @prefix PLOT3D_PRIMITIVE
 * 图元类型。
 */
typedef enum _plot3d_primitive_type_t {
  PLOT3D_PRIMITIVE_DOT = 0,
  PLOT3D_PRIMITIVE_LINE,
  PLOT3D_PRIMITIVE_TRIANGLE,
  PLOT3D_PRIMITIVE_CYLINDER
} plot3d_primitive_type_t;

/**
 * @class plot3d_primitive_t
 * @prefix plot3d_primitive
 * 投影图元，i0/i1/i2 为投影点数组下标。
 */
typedef struct _plot3d_primitive_t {
  plot3d_primitive_type_t type;
  uint32_t i0;
  uint32_t i1;
  uint32_t i2;
  float_t depth;
  color_t color;
} plot3d_primitive_t;

/**
 * @class plot3d_paint_ctx_t
 * @prefix plot3d_paint_ctx
 * 图型插件绘制时的上下文。
 *
 * 约束（下期迁移实现时必须遵守）：
 * 1. build 的输入是**投影后**的点（rx/ry/rz/depth/valid），不是原始数据点——核心用它们
 *    算法线、排序与剔除。
 * 2. layout_grid 只产坐标与拓扑，颜色由核心按网格节点下标回填（surface 展开的 6 个顶点必须
 *    复用同一节点颜色）。
 * 3. on_paint 为 NULL 时走核心标准流程，非 NULL 时插件整体接管绘制。
 */
typedef struct _plot3d_paint_ctx_t {
  /**
   * @property {const plot3d_projected_point_t*} points
   * @annotation ["readable"]
   * 投影后的点数组；build 只应读它，不得改写。
   */
  const plot3d_projected_point_t* points;

  /**
   * @property {uint32_t} points_nr
   * @annotation ["readable"]
   * 投影点个数。
   */
  uint32_t points_nr;

  /**
   * @property {float_t} point_size
   * @annotation ["readable"]
   * 点大小。
   */
  float_t point_size;

  /**
   * @property {float_t} line_width
   * @annotation ["readable"]
   * 线宽。
   */
  float_t line_width;

  /**
   * @property {const char*} colormap
   * @annotation ["readable"]
   * 配色表名。
   */
  const char* colormap;
} plot3d_paint_ctx_t;

/* type / create / destroy 必填（注册时校验），其余成员可为 NULL，此时对应的转发函数返回
 * RET_NOT_IMPL。 */
typedef struct _plot3d_type_vtable_t {
  /* 图型的类型名，如 "dot"/"line"/"surface"/"cylinder"，注册表以它为键。 */
  const char* type;
  plot3d_type_t* (*create)(void);
  /* 网格铺排：GRID 结果铺成数据点，surface 三角化、line 每行一条折线。
   * 只产出坐标与拓扑（含断点），颜色由核心按网格节点下标事后回填。 */
  ret_t (*layout_grid)(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                       const plot3d_source_result_t* result, darray_t* points);
  /* 图元构建：从投影后的点数组产出点、线或三角形。输入必须是投影后的点。 */
  ret_t (*build)(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx, darray_t* primitives);
  /* 可选：非 NULL 时插件整体接管绘制，NULL 时走核心标准流程。 */
  ret_t (*on_paint)(plot3d_type_t* type, widget_t* widget, canvas_t* c);
  /* set_prop 必须严格按三值协议返回，核心的属性广播完全依赖它区分三种情形：
   *   RET_OK          认领该属性且值合法（已落值）；
   *   RET_BAD_PARAMS  认领该属性但值非法（不得改动原值）；
   *   RET_NOT_FOUND   不认识该属性，核心据此继续按自己的属性处理。
   * 值非法时返回 RET_FAIL/RET_OOM 等其它错误码会被广播当成「未认领」静默吞掉。
   * get_prop 同样以 RET_NOT_FOUND 表示不认识该属性。 */
  ret_t (*set_prop)(plot3d_type_t* type, const char* name, const value_t* v);
  ret_t (*get_prop)(plot3d_type_t* type, const char* name, value_t* v);
  ret_t (*destroy)(plot3d_type_t* type);
} plot3d_type_vtable_t;

/**
 * @class plot3d_type_t
 * 图型插件的实例，各插件把它作为自己结构体的第一个成员。
 *
 * point_size / line_width 放在基类：内置图型共用同一套视觉属性，避免每个插件复制一份。
 */
struct _plot3d_type_t {
  const plot3d_type_vtable_t* vt;
  float_t point_size;
  float_t line_width;
};

/**
 * @method plot3d_type_layout_grid
 * 把 GRID 采样结果铺成数据点（含拓扑）。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const plot3d_paint_ctx_t*} ctx 绘制上下文。
 * @param {const plot3d_source_result_t*} result GRID 采样结果。
 * @param {darray_t*} points 返回数据点，由核心提供且需已清空。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_layout_grid(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                               const plot3d_source_result_t* result, darray_t* points);

/**
 * @method plot3d_type_build
 * 从投影后的点数组构建图元。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const plot3d_paint_ctx_t*} ctx 绘制上下文（含投影点）。
 * @param {darray_t*} primitives 返回图元，由核心提供且需已清空。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_build(plot3d_type_t* type, const plot3d_paint_ctx_t* ctx,
                         darray_t* primitives);

/**
 * @method plot3d_type_push_primitive
 * 向图元数组追加一个图元。供图型插件 build 复用，数组元素由 darray 的 destroy 回调释放。
 * @param {darray_t*} primitives 图元数组。
 * @param {plot3d_primitive_type_t} type 图元类型。
 * @param {uint32_t} i0 第一个顶点下标。
 * @param {uint32_t} i1 第二个顶点下标。
 * @param {uint32_t} i2 第三个顶点下标。
 * @param {float_t} depth 排序深度。
 * @param {color_t} color 图元颜色。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_push_primitive(darray_t* primitives, plot3d_primitive_type_t type,
                                  uint32_t i0, uint32_t i1, uint32_t i2, float_t depth,
                                  color_t color);

/**
 * @method plot3d_type_on_paint
 * 插件整体接管绘制。vtable 中 on_paint 为 NULL 时不应调用本函数，应走核心标准流程。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {widget_t*} widget 控件。
 * @param {canvas_t*} c 画布。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_on_paint(plot3d_type_t* type, widget_t* widget, canvas_t* c);

/**
 * @method plot3d_type_set_prop
 * 设置图型插件的属性。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const char*} name 属性名。
 * @param {const value_t*} v 属性值。
 *
 * @return {ret_t} 返回RET_OK表示插件认领该属性且值合法，RET_BAD_PARAMS表示认领该属性但值
 *                 非法（原值不变），RET_NOT_FOUND表示插件不认识该属性。核心的属性广播依赖这
 *                 三个返回值区分「认领并接受」「认领但拒绝」「未认领」。
 */
ret_t plot3d_type_set_prop(plot3d_type_t* type, const char* name, const value_t* v);

/**
 * @method plot3d_type_get_prop
 * 获取图型插件的属性。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const char*} name 属性名。
 * @param {value_t*} v 返回属性值。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示插件不认识该属性。
 */
ret_t plot3d_type_get_prop(plot3d_type_t* type, const char* name, value_t* v);

/**
 * @method plot3d_type_init_style
 * 初始化图型共用的 point_size / line_width 默认值。插件 create 时应调用。
 * @param {plot3d_type_t*} type 图型对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_init_style(plot3d_type_t* type);

/**
 * @method plot3d_type_set_style_prop
 * 设置图型共用的 point_size / line_width。供插件 set_prop 复用；其它属性返回 RET_NOT_FOUND。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const char*} name 属性名。
 * @param {const value_t*} v 属性值。
 *
 * @return {ret_t} 返回RET_OK表示认领并接受，RET_NOT_FOUND表示不是样式属性。
 */
ret_t plot3d_type_set_style_prop(plot3d_type_t* type, const char* name, const value_t* v);

/**
 * @method plot3d_type_get_style_prop
 * 读取图型共用的 point_size / line_width。供插件 get_prop 复用；其它属性返回 RET_NOT_FOUND。
 * @param {plot3d_type_t*} type 图型对象。
 * @param {const char*} name 属性名。
 * @param {value_t*} v 返回属性值。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示不是样式属性。
 */
ret_t plot3d_type_get_style_prop(plot3d_type_t* type, const char* name, value_t* v);

/**
 * @method plot3d_type_destroy
 * 销毁图型插件对象。
 * @param {plot3d_type_t*} type 图型对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_destroy(plot3d_type_t* type);

/**
 * @method plot3d_type_factory_register
 * 注册图型插件，type 相同时替换原有的插件，以便用户用自己的实现覆盖内置插件。
 * @param {const plot3d_type_vtable_t*} vt 图型插件的虚函数表，需要在注册期间一直有效，
 *                                          其 type/create/destroy 三个成员不能为 NULL。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_factory_register(const plot3d_type_vtable_t* vt);

/**
 * @method plot3d_type_factory_create
 * 创建指定类型的图型插件对象。
 * @param {const char*} type 图型的类型名。
 *
 * @return {plot3d_type_t*} 返回图型对象，类型未注册时返回NULL。
 */
plot3d_type_t* plot3d_type_factory_create(const char* type);

/**
 * @method plot3d_type_factory_count
 * 获取已注册的图型插件的个数。
 *
 * @return {uint32_t} 返回已注册的插件个数。
 */
uint32_t plot3d_type_factory_count(void);

/**
 * @method plot3d_type_factory_get
 * 获取指定序号的图型插件的虚函数表。
 * @param {uint32_t} index 序号。
 *
 * @return {const plot3d_type_vtable_t*} 返回虚函数表，序号无效时返回NULL。
 */
const plot3d_type_vtable_t* plot3d_type_factory_get(uint32_t index);

/**
 * @method plot3d_type_factory_deinit
 * 清空图型插件注册表，重复调用是安全的。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_factory_deinit(void);

/**
 * @method plot3d_type_line_register
 * 注册内置的 line 图型插件。
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_line_register(void);

/**
 * @method plot3d_type_dot_register
 * 注册内置的 dot 图型插件。
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_dot_register(void);

/**
 * @method plot3d_type_surface_register
 * 注册内置的 surface 图型插件。
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_surface_register(void);

/**
 * @method plot3d_type_cylinder_register
 * 注册内置的 cylinder 图型插件。
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_type_cylinder_register(void);

END_C_DECLS

#endif /*TK_PLOT3D_TYPE_H*/
