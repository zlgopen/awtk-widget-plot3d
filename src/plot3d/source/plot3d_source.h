/**
 * File:   plot3d_source.h
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

#ifndef TK_PLOT3D_SOURCE_H
#define TK_PLOT3D_SOURCE_H

/* 刻意只依赖 tkc：插件接口若引入 plot3d.h，会把 base/widget.h 与 tkc/fscript.h
 * 拖进每个插件的编译单元。需要 plot3d_data_point_t 的插件自己 include plot3d/plot3d.h。
 *
 * 这条约束只对数据源这一层成立：colorizer / type 的接口签名里直接出现定义在 plot3d.h 中的
 * 类型（如 plot3d_sample_pos_t、plot3d_color_value_t），无法只靠前向声明表达，故已放弃。 */
#include "tkc/darray.h"

BEGIN_C_DECLS

struct _plot3d_source_t;
typedef struct _plot3d_source_t plot3d_source_t;

/**
 * @enum plot3d_source_result_type_t
 * @prefix PLOT3D_SOURCE_RESULT_
 * 数据源采样结果的类型。
 */
typedef enum _plot3d_source_result_type_t {
  /**
   * @const PLOT3D_SOURCE_RESULT_POINTS
   * 散点序列，可以包含断点。
   */
  PLOT3D_SOURCE_RESULT_POINTS = 0,
  /**
   * @const PLOT3D_SOURCE_RESULT_GRID
   * cols x rows 的 z 网格。
   */
  PLOT3D_SOURCE_RESULT_GRID
} plot3d_source_result_type_t;

/**
 * @class plot3d_source_result_t
 * 数据源的采样结果。
 */
typedef struct _plot3d_source_result_t {
  /**
   * @property {plot3d_source_result_type_t} type
   * @annotation ["readable"]
   * 结果类型，决定其它字段中哪些有效。
   */
  plot3d_source_result_type_t type;

  /**
   * @property {darray_t*} points
   * @annotation ["readable"]
   * POINTS 时的数据点。由核心提供且已清空，插件往里 push TKMEM_ZALLOC 出来的
   * plot3d_data_point_t*，元素的销毁由核心负责。
   */
  darray_t* points;

  /**
   * @property {const float_t*} ts
   * @annotation ["readable"]
   * POINTS 且为曲线时，与 points 等长的参数 t 数组，供配色使用；不适用时为 NULL。
   * 由插件持有并复用，核心只读、不得释放，有效期至该插件的下一次
   * sample / set_prop / set_data / reset 或 destroy；核心只应在本次 sample 返回后同步使用，
   * 不得跨调用缓存。
   */
  const float_t* ts;

  /**
   * @property {const float_t*} zs
   * @annotation ["readable"]
   * GRID 时的 cols*rows 个 z 值，按行存放。由插件持有并复用，核心只读、不得释放，
   * 有效期至该插件的下一次 sample / set_prop / set_data / reset 或 destroy；核心只应在本次
   * sample 返回后同步使用，不得跨调用缓存。
   */
  const float_t* zs;

  /**
   * @property {uint32_t} cols
   * @annotation ["readable"]
   * GRID 的列数。
   */
  uint32_t cols;

  /**
   * @property {uint32_t} rows
   * @annotation ["readable"]
   * GRID 的行数。
   */
  uint32_t rows;

  /**
   * @property {float_t} x0
   * @annotation ["readable"]
   * GRID 的 x 范围的起点。
   */
  float_t x0;

  /**
   * @property {float_t} x1
   * @annotation ["readable"]
   * GRID 的 x 范围的终点。
   */
  float_t x1;

  /**
   * @property {float_t} y0
   * @annotation ["readable"]
   * GRID 的 y 范围的起点。
   */
  float_t y0;

  /**
   * @property {float_t} y1
   * @annotation ["readable"]
   * GRID 的 y 范围的终点。
   */
  float_t y1;

  /**
   * @property {bool_t} has_color
   * @annotation ["readable"]
   * 只对 POINTS 有意义：为 TRUE 表示点已自带颜色（如 csv 数据源），核心不再计算配色。
   */
  bool_t has_color;
} plot3d_source_result_t;

/* type / create / destroy 必填（注册时校验），其余成员可为 NULL，此时对应的转发函数返回
 * RET_NOT_IMPL（has_data 返回 FALSE）。 */
typedef struct _plot3d_source_vtable_t {
  /* 数据源的类型名，如 "matrix"，注册表以它为键。 */
  const char* type;
  plot3d_source_t* (*create)(void);
  /* 核心保证调用前已对 result 执行 plot3d_source_result_init，插件只需填写与自己产出形态
   * （result->type）相关的字段，其余字段保持清零值即可。 */
  ret_t (*sample)(plot3d_source_t* source, plot3d_source_result_t* result);
  /* set_prop 必须严格按三值协议返回，核心的属性广播完全依赖它区分三种情形：
   *   RET_OK          认领该属性且值合法（已落值）；
   *   RET_BAD_PARAMS  认领该属性但值非法（不得改动原值）；
   *   RET_NOT_FOUND   不认识该属性，核心据此继续按自己的属性处理。
   * 值非法时返回 RET_FAIL/RET_OOM 等其它错误码会被广播当成「未认领」静默吞掉。
   * get_prop 同样以 RET_NOT_FOUND 表示不认识该属性。 */
  ret_t (*set_prop)(plot3d_source_t* source, const char* name, const value_t* v);
  ret_t (*get_prop)(plot3d_source_t* source, const char* name, value_t* v);
  /* 传递回调函数、ctx、C 数组等 value_t 不便表达的数据，所有权约定见
   * plot3d_source_set_data。 */
  ret_t (*set_data)(plot3d_source_t* source, const char* name, void* p1, void* p2);
  bool_t (*has_data)(plot3d_source_t* source);
  /* 让位：被其它数据源或 dataset 接管时清空自身数据。 */
  ret_t (*reset)(plot3d_source_t* source);
  ret_t (*destroy)(plot3d_source_t* source);
} plot3d_source_vtable_t;

/**
 * @class plot3d_source_t
 * 数据源插件的实例，各插件把它作为自己结构体的第一个成员。
 */
struct _plot3d_source_t {
  const plot3d_source_vtable_t* vt;
};

/**
 * @method plot3d_source_result_init
 * 初始化采样结果：全部字段清零，再把 points 挂上去。
 *
 * 核心必须在每次调用 plot3d_source_sample 之前调用本函数，插件因此不必清理与自己产出形态
 * 无关的字段，也不会读到上一次采样（可能来自另一个形态）残留的 zs/has_color 等值。
 *
 * @param {plot3d_source_result_t*} result 采样结果对象。
 * @param {darray_t*} points 用于装数据点的数组，由核心提供且需已清空；
 *                           只产出 GRID 的数据源可以传 NULL。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_result_init(plot3d_source_result_t* result, darray_t* points);

/**
 * @method plot3d_source_sample
 * 采样数据源的数据。调用之前 result 须已由 plot3d_source_result_init 初始化。
 * @param {plot3d_source_t*} source 数据源对象。
 * @param {plot3d_source_result_t*} result 返回采样结果。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_sample(plot3d_source_t* source, plot3d_source_result_t* result);

/**
 * @method plot3d_source_set_prop
 * 设置数据源的属性。
 * @param {plot3d_source_t*} source 数据源对象。
 * @param {const char*} name 属性名。
 * @param {const value_t*} v 属性值。
 *
 * @return {ret_t} 返回RET_OK表示数据源认领该属性且值合法，RET_BAD_PARAMS表示认领该属性但值
 *                 非法（原值不变），RET_NOT_FOUND表示数据源不认识该属性。核心的属性广播依赖这
 *                 三个返回值区分「认领并接受」「认领但拒绝」「未认领」。
 */
ret_t plot3d_source_set_prop(plot3d_source_t* source, const char* name, const value_t* v);

/**
 * @method plot3d_source_get_prop
 * 获取数据源的属性。
 * @param {plot3d_source_t*} source 数据源对象。
 * @param {const char*} name 属性名。
 * @param {value_t*} v 返回属性值。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示数据源不认识该属性。
 */
ret_t plot3d_source_get_prop(plot3d_source_t* source, const char* name, value_t* v);

/**
 * @method plot3d_source_set_data
 * 给数据源设置回调函数、ctx、C 数组等 value_t 不便表达的数据。
 *
 * name 沿用 kebab-case，与属性名保持同一套命名。
 *
 * 所有权总则：数据由调用方提供，**由插件决定是拷贝还是借用**，因此每个 name 都要有一个
 * PLOT3D_SOURCE_DATA_* 宏，并在该宏的注释里写明自己的 p1/p2 含义与所有权（拷贝还是借用；
 * 借用时还要写明调用方需要维持数据有效的期限）。调用方只能依据该宏的注释来管理这块数据的生命
 * 周期。约定只在宏注释里写一份，本总则不再逐个复述，免得两处描述分叉。
 *
 * @param {plot3d_source_t*} source 数据源对象。
 * @param {const char*} name 数据名。
 * @param {void*} p1 第一个参数，含义与所有权由该数据源声明。
 * @param {void*} p2 第二个参数，含义与所有权由该数据源声明。
 *
 * @return {ret_t} 返回RET_OK表示成功，RET_NOT_FOUND表示数据源不认识该数据名。
 */
ret_t plot3d_source_set_data(plot3d_source_t* source, const char* name, void* p1, void* p2);

/**
 * @method plot3d_source_has_data
 * 判断数据源是否已被喂入可用的数据。
 * @param {plot3d_source_t*} source 数据源对象。
 *
 * @return {bool_t} 返回TRUE表示有数据，否则表示没有。
 */
bool_t plot3d_source_has_data(plot3d_source_t* source);

/**
 * @method plot3d_source_reset
 * 清空数据源的数据，用于让位于其它数据源或 dataset。
 * @param {plot3d_source_t*} source 数据源对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_reset(plot3d_source_t* source);

/**
 * @method plot3d_source_destroy
 * 销毁数据源对象。
 * @param {plot3d_source_t*} source 数据源对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_destroy(plot3d_source_t* source);

/**
 * @method plot3d_source_set_range_prop
 * 校验并落值一个 "min,max" 形式的范围属性。
 *
 * 属性两阶段迁移期间，同一个范围属性会同时有核心与插件两个持有者，而阶段一的转发只在插件拒绝了
 * 核心已接受的值时告警、并不回滚，其正确性完全建立在「双方用同一套校验」之上。因此两边都必须调
 * 本函数，不要各抄一份。
 *
 * @param {char**} text 存放该属性的字段，值合法时被改写成 value 的副本。
 * @param {const char*} value 形如 "0,360"；为空表示不指定，此时字段被清空。
 *
 * @return {ret_t} 返回RET_OK表示成功，格式非法时返回RET_BAD_PARAMS且字段保持原值。
 */
ret_t plot3d_source_set_range_prop(char** text, const char* value);

/**
 * @method plot3d_source_factory_register
 * 注册数据源插件，type 相同时替换原有的插件，以便用户用自己的实现覆盖内置插件。
 * @param {const plot3d_source_vtable_t*} vt 数据源的虚函数表，需要在注册期间一直有效，
 *                                            其 type/create/destroy 三个成员不能为 NULL。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_factory_register(const plot3d_source_vtable_t* vt);

/**
 * @method plot3d_source_factory_create
 * 创建指定类型的数据源对象。
 * @param {const char*} type 数据源的类型名。
 *
 * @return {plot3d_source_t*} 返回数据源对象，类型未注册时返回NULL。
 */
plot3d_source_t* plot3d_source_factory_create(const char* type);

/**
 * @method plot3d_source_factory_count
 * 获取已注册的数据源插件的个数。
 *
 * @return {uint32_t} 返回已注册的插件个数。
 */
uint32_t plot3d_source_factory_count(void);

/**
 * @method plot3d_source_factory_get
 * 获取指定序号的数据源插件的虚函数表。
 * @param {uint32_t} index 序号。
 *
 * @return {const plot3d_source_vtable_t*} 返回虚函数表，序号无效时返回NULL。
 */
const plot3d_source_vtable_t* plot3d_source_factory_get(uint32_t index);

/**
 * @method plot3d_source_factory_deinit
 * 清空数据源注册表，重复调用是安全的。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_factory_deinit(void);

/**
 * @method plot3d_source_csv_register
 * 注册 csv 数据源插件（类型名 "none"，与采样模式同名）。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_csv_register(void);

/**
 * @method plot3d_source_matrix_register
 * 注册矩阵数据源插件（类型名 "matrix"，与采样模式同名）。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_matrix_register(void);

/**
 * @method plot3d_source_curve_register
 * 注册曲线数据源插件（类型名 "curve"，与采样模式同名）。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_curve_register(void);

/**
 * @method plot3d_source_grid_register
 * 注册网格数据源插件（类型名 "grid"，与采样模式同名）。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_source_grid_register(void);

/**
 * @class plot3d_source_matrix_size_t
 * matrix 数据源的矩阵形状，作为 PLOT3D_SOURCE_DATA_Z_MATRIX 的 p2 传入。
 */
typedef struct _plot3d_source_matrix_size_t {
  /**
   * @property {uint32_t} cols
   * @annotation ["readable"]
   * 列数。
   */
  uint32_t cols;

  /**
   * @property {uint32_t} rows
   * @annotation ["readable"]
   * 行数。
   */
  uint32_t rows;
} plot3d_source_matrix_size_t;

/* matrix 插件的 set_data 数据名：p1 为按行存放的 const float_t* z 数组（为 NULL 表示清空矩阵，
 * 此时 p2 被忽略），p2 为 plot3d_source_matrix_size_t* 形状。插件把 z 数组**拷贝**一份，调用方
 * 在 set_data 返回后即可释放自己那份；p2 也只在调用期间被读取。 */
#define PLOT3D_SOURCE_DATA_Z_MATRIX "sample-z-matrix"

/* curve 插件的 set_data 数据名：p1 为 plot3d_curve_func_t（为 NULL 表示取消回调），
 * p2 为 void* ctx。插件只**借用**函数指针与 ctx，不负责释放；调用方须保证在插件仍持有期间
 * （直到下一次 set_data / reset / destroy）二者有效。 */
#define PLOT3D_SOURCE_DATA_CURVE_FUNC "sample-curve-func"

/* grid 插件的 set_data 数据名：p1 为 plot3d_grid_func_t（为 NULL 表示取消回调），
 * p2 为 void* ctx。插件只**借用**函数指针与 ctx，不负责释放；调用方须保证在插件仍持有期间
 * （直到下一次 set_data / reset / destroy）二者有效。 */
#define PLOT3D_SOURCE_DATA_GRID_FUNC "sample-grid-func"

END_C_DECLS

#endif /*TK_PLOT3D_SOURCE_H*/
