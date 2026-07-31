/**
 * File:   plot3d.h
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


#ifndef TK_PLOT3D_H
#define TK_PLOT3D_H

#include "base/widget.h"
#include "tkc/color.h"
#include "tkc/darray.h"

BEGIN_C_DECLS

/**
 * 网格采样函数：给定 x/y 返回 z，相当于 matlab 里 surf 用的 z=f(x,y)。
 * 返回 RET_OK 表示成功，否则该点被丢弃。
 * （函数指针类型，不可使用 @method，否则会进入 DLL 导出表。）
 */
typedef ret_t (*plot3d_grid_func_t)(void* ctx, float_t x, float_t y, float_t* z);

/**
 * 参数曲线的采样函数：由参数 t 算出一个点的坐标。
 * 返回 RET_OK 表示成功，否则该点被丢弃。
 */
typedef ret_t (*plot3d_curve_func_t)(void* ctx, float_t t, float_t* x, float_t* y, float_t* z);

/**
 * 采样点的配色函数：由点的坐标算出颜色，用来取代默认的配色表。
 * 返回 RET_OK 表示成功，否则该点用配色表取色。
 */
typedef ret_t (*plot3d_color_func_t)(void* ctx, float_t x, float_t y, float_t z, color_t* color);

/**
 * @class plot3d_t
 * @parent widget_t
 * @annotation ["scriptable","design","widget"]
 * Plot3D 三维图表控件。
 *
 * plot3d\_t是[widget\_t](widget_t.md)的子类控件，widget\_t的函数均适用于plot3d\_t控件。
 *
 * 在xml中使用"plot3d"标签创建控件。如：
 *
 * ```xml
 * <!-- ui -->
 * <plot3d x="c" y="50" w="100" h="100"/>
 * ```
 *
 * 可用通过style来设置控件的显示风格，如字体的大小和颜色等等。如：
 *
 * ```xml
 * <!-- style -->
 * <plot3d>
 *   <style name="default" font_size="32">
 *     <normal text_color="black" />
 *   </style>
 * </plot3d>
 * ```
 */
typedef struct _plot3d_t {
  widget_t widget;

  /**
   * @property {char*} plottype
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * chart type: dot,line,surface,cylinder。
   */
  char* plottype;

  /**
   * @property {char*} sample_mode
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 函数采样模式：none 用 dataset 的数据，grid 用 z=f(x,y) 采样，curve 用参数 t 采样曲线，
   * matrix 用现成的 z 矩阵。
   */
  char* sample_mode;

  /**
   * @property {char*} colormap
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 函数采样点的配色表：viridis,jet,gray,parula,hot,cool,hsv,bone,copper,pink,turbo。
   */
  char* colormap;

  /**
   * @property {char*} xlabel
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * X 轴名称。
   */
  char* xlabel;

  /**
   * @property {char*} ylabel
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Y 轴名称。
   */
  char* ylabel;

  /**
   * @property {char*} zlabel
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Z 轴名称。
   */
  char* zlabel;

  /**
   * @property {char*} grid_color_str
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 网格颜色字符串（对应属性名 grid_color）。
   */
  char* grid_color_str;

  /**
   * @property {char*} xaxis_color_str
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * X 轴颜色字符串（对应属性名 xaxis-color）。
   */
  char* xaxis_color_str;

  /**
   * @property {char*} yaxis_color_str
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Y 轴颜色字符串（对应属性名 yaxis-color）。
   */
  char* yaxis_color_str;

  /**
   * @property {char*} zaxis_color_str
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Z 轴颜色字符串（对应属性名 zaxis-color）。
   */
  char* zaxis_color_str;

  /**
   * @property {char*} tick_color_str
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 刻度文字颜色字符串（对应属性名 tick-color）。
   */
  char* tick_color_str;

  /**
   * @property {bool_t} show_grid
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否显示网格。
   */
  bool_t show_grid;

  /**
   * @property {bool_t} show_datatip
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否显示数据提示。
   */
  bool_t show_datatip;

  /**
   * @property {bool_t} show_axis_tick
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否显示轴刻度文字。
   */
  bool_t show_axis_tick;

  /**
   * @property {bool_t} enable_cache
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否启用投影/绘制缓存。
   */
  bool_t enable_cache;

  /**
   * @property {bool_t} equal_axis
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 是否三轴等比例（TRUE 时忽略 box-aspect）。
   */
  bool_t equal_axis;

  /**
   * @property {float_t} camera_yaw
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 相机方位角（弧度）。
   */
  float_t camera_yaw;

  /**
   * @property {float_t} camera_pitch
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 相机俯仰角（弧度）。
   */
  float_t camera_pitch;

  /**
   * @property {float_t} camera_distance
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 相机距离。
   */
  float_t camera_distance;

  /**
   * @property {float_t} camera_z_offset
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 相机沿 Z 方向偏移。
   */
  float_t camera_z_offset;

  /**
   * @property {float_t} xy_grid_position
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * XY 网格平面在 Z 轴上的位置。
   */
  float_t xy_grid_position;

  /**
   * @property {float_t} xz_grid_position
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * XZ 网格平面在 Y 轴上的位置。
   */
  float_t xz_grid_position;

  /**
   * @property {float_t} yz_grid_position
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * YZ 网格平面在 X 轴上的位置。
   */
  float_t yz_grid_position;

  /**
   * @property {float_t} box_aspect_x
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 包围盒 X 边相对长度系数。
   */
  float_t box_aspect_x;

  /**
   * @property {float_t} box_aspect_y
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 包围盒 Y 边相对长度系数。
   */
  float_t box_aspect_y;

  /**
   * @property {float_t} box_aspect_z
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 包围盒 Z 边相对长度系数。
   */
  float_t box_aspect_z;

  /**
   * @property {uint32_t} x_grid_count
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * X 轴网格格数上限。
   */
  uint32_t x_grid_count;

  /**
   * @property {uint32_t} y_grid_count
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Y 轴网格格数上限。
   */
  uint32_t y_grid_count;

  /**
   * @property {uint32_t} z_grid_count
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * Z 轴网格格数上限。
   */
  uint32_t z_grid_count;

  /**
   * @property {color_t} grid_color
   * @annotation ["readable"]
   * 网格颜色（由 grid_color 字符串解析得到）。
   */
  color_t grid_color;

  /**
   * @property {color_t} xaxis_color
   * @annotation ["readable"]
   * X 轴颜色（由 xaxis-color 字符串解析得到）。
   */
  color_t xaxis_color;

  /**
   * @property {color_t} yaxis_color
   * @annotation ["readable"]
   * Y 轴颜色（由 yaxis-color 字符串解析得到）。
   */
  color_t yaxis_color;

  /**
   * @property {color_t} zaxis_color
   * @annotation ["readable"]
   * Z 轴颜色（由 zaxis-color 字符串解析得到）。
   */
  color_t zaxis_color;

  /**
   * @property {color_t} tick_color
   * @annotation ["readable"]
   * 刻度文字颜色（由 tick-color 字符串解析得到）。
   */
  color_t tick_color;

  /**
   * @property {float_t} axis_negative_brightness
   * @annotation ["set_prop","get_prop","readable","persitent","design","scriptable"]
   * 负半轴相对正半轴的亮度系数。
   */
  float_t axis_negative_brightness;

  /*private*/
  bool_t pointer_down;
  int32_t pointer_button;
  int32_t hover_index;
  xy_t last_pointer_x;
  xy_t last_pointer_y;

  bool_t projection_cache_dirty;
  bool_t draw_cache_dirty;

  /* 双重用途：(1) 当前数据点是否由函数源（grid/curve/matrix）持续生成——为 TRUE 时才允许被
   * 下一次重采样覆盖；(2) 一次性强制跑采样的门闩——plot3d_set_dataset / 切到 none 时置 TRUE，
   * 采样末尾按函数源是否活动重算。csv 活动时恒为 FALSE。 */
  bool_t sample_from_func;

  darray_t data_points;
  darray_t draw_primitives;

  /* 每种已注册数据源一个实例，plot3d_create 时建全，on_destroy 时释放。 */
  darray_t sources;

  /* 每种已注册配色插件一个实例，与 sources 同样在 plot3d_create 时建全。 */
  darray_t colorizers;

  /* 每种已注册图型插件一个实例，与 sources 同样在 plot3d_create 时建全。 */
  darray_t types;

  void* projected_points;
  uint32_t projected_points_nr;

  /*for test*/
  /* plot3d_resample 被触发的次数，只服务于测试（钉住「一次设置只重采样一次」），
   * 业务逻辑不读它。 */
  uint32_t resample_nr_for_test;

} plot3d_t;

/**
 * @class plot3d_data_point_t
 * @annotation ["scriptable"]
 * 三维数据点。
 */
typedef struct _plot3d_data_point_t {
  /**
   * @property {float_t} x
   * @annotation ["readable","scriptable"]
   * X 坐标。
   */
  float_t x;
  /**
   * @property {float_t} y
   * @annotation ["readable","scriptable"]
   * Y 坐标。
   */
  float_t y;
  /**
   * @property {float_t} z
   * @annotation ["readable","scriptable"]
   * Z 坐标。
   */
  float_t z;
  /**
   * @property {color_t} color
   * @annotation ["readable"]
   * 点颜色。
   */
  color_t color;
  /**
   * @property {bool_t} is_break
   * @annotation ["readable","scriptable"]
   * 是否为折线分段点。
   */
  bool_t is_break;

  /* GRID/MATRIX 铺点时记录对应的网格节点索引（j*cols+i），供图型插件路径回填颜色。 */
  uint32_t node_index;
} plot3d_data_point_t;

/**
 * @class plot3d_color_value_t
 * 一个采样点的配色来源：要么已经是颜色，要么是待归一化的标量。
 */
typedef struct _plot3d_color_value_t {
  /**
   * @property {bool_t} is_color
   * @annotation ["readable"]
   * 为TRUE表示直接用 color，为FALSE表示按 scalar 在配色表上取色。
   */
  bool_t is_color;

  /**
   * @property {color_t} color
   * @annotation ["readable"]
   * is_color 为TRUE时的颜色。
   */
  color_t color;

  /**
   * @property {float_t} scalar
   * @annotation ["readable"]
   * is_color 为FALSE时的标量，全部采样点算完后统一归一化再查配色表。
   */
  float_t scalar;
} plot3d_color_value_t;

/**
 * @class plot3d_sample_pos_t
 * 一个采样点的位置，与 plot3d_color_value_t 一起构成配色求值的入参与出参。
 */
typedef struct _plot3d_sample_pos_t {
  /**
   * @property {float_t} x
   * @annotation ["readable"]
   * X 坐标。
   */
  float_t x;

  /**
   * @property {float_t} y
   * @annotation ["readable"]
   * Y 坐标。
   */
  float_t y;

  /**
   * @property {float_t} z
   * @annotation ["readable"]
   * Z 坐标。
   */
  float_t z;

  /**
   * @property {float_t} t
   * @annotation ["readable"]
   * 曲线参数 t，has_t 为FALSE时无意义。
   */
  float_t t;

  /**
   * @property {bool_t} has_t
   * @annotation ["readable"]
   * t 是否有效。为FALSE时插件须把 t 当作未定义处理（如从变量表里移除），
   * 不得沿用上一次求值留下的值。
   */
  bool_t has_t;
} plot3d_sample_pos_t;

/**
 * @method plot3d_create
 * @annotation ["constructor", "scriptable"]
 * 创建plot3d对象
 * @param {widget_t*} parent 父控件
 * @param {xy_t} x x坐标
 * @param {xy_t} y y坐标
 * @param {wh_t} w 宽度
 * @param {wh_t} h 高度
 *
 * @return {widget_t*} plot3d对象。
 */
widget_t* plot3d_create(widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h);

/**
 * @method plot3d_cast
 * 转换为plot3d对象(供脚本语言使用)。
 * @annotation ["cast", "scriptable"]
 * @param {widget_t*} widget plot3d对象。
 *
 * @return {widget_t*} plot3d对象。
 */
widget_t* plot3d_cast(widget_t* widget);

/**
 * @method plot3d_set_dataset
 * 设置 csv format data set. 每行一个点。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} dataset csv format data set. 每行一个点。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_dataset(widget_t* widget, const char* dataset);

/**
 * @method plot3d_set_plottype
 * 设置 chart type: dot,line,surface,cylinder。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} plottype chart type: dot,line,surface,cylinder。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_plottype(widget_t* widget, const char* plottype);

/**
 * @method plot3d_reset_data
 * 清空数据点。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_reset_data(widget_t* widget);

/**
 * @method plot3d_append_data_point
 * 追加一个数据点。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} x x坐标。
 * @param {float_t} y y坐标。
 * @param {float_t} z z坐标。
 * @param {const char*} color 颜色字符串(支持color_parse格式)。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_append_data_point(widget_t* widget, float_t x, float_t y, float_t z, const char* color);

/**
 * @method plot3d_append_break_data_point
 * 追加一个分段点。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_append_break_data_point(widget_t* widget);

/**
 * @method plot3d_get_data_points_nr
 * 获取数据点数量（含break点）。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {uint32_t} 数据点数量。
 */
uint32_t plot3d_get_data_points_nr(widget_t* widget);

/**
 * @method plot3d_get_data_point
 * 获取指定索引的数据点（含break点）。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {uint32_t} index 索引。
 * @param {plot3d_data_point_t*} point 返回数据点。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_get_data_point(widget_t* widget, uint32_t index, plot3d_data_point_t* point);

/**
 * @method plot3d_set_grid_func
 * 设置网格采样函数，控件按 sample-x-range/sample-y-range/sample-steps 采样并按 plottype 排布：
 * surface 自动三角化，line 每行一条折线，dot/cylinder 直接铺点。
 * 设置采样函数会让位于它的 dataset 数据失效。
 * @param {widget_t*} widget widget对象。
 * @param {plot3d_grid_func_t} func 采样函数，为NULL表示取消。
 * @param {void*} ctx 回调上下文。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_grid_func(widget_t* widget, plot3d_grid_func_t func, void* ctx);

/**
 * @method plot3d_set_sample_mode
 * 设置函数采样模式。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_mode none、grid 或 curve。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_mode(widget_t* widget, const char* sample_mode);

/**
 * @method plot3d_set_sample_x_range
 * 设置网格采样的 x 范围。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_x_range 形如 "0,360"。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_x_range(widget_t* widget, const char* sample_x_range);

/**
 * @method plot3d_set_sample_y_range
 * 设置网格采样的 y 范围。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_y_range 形如 "0,7"。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_y_range(widget_t* widget, const char* sample_y_range);

/**
 * @method plot3d_set_sample_steps
 * 设置网格采样的格点数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_steps 形如 "25,8"，只写一个数时两个方向相同。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_steps(widget_t* widget, const char* sample_steps);

/**
 * @method plot3d_set_colormap
 * 设置函数采样点的配色表。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} colormap viridis、jet、gray、parula、hot、cool、hsv、bone、copper、pink 或 turbo。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_colormap(widget_t* widget, const char* colormap);

/**
 * @method plot3d_set_sample_z_expr
 * 设置网格采样的 z 表达式，变量为 x 与 y，可用 sin/cos/pow 等函数。
 * 与采样函数同时存在时以采样函数为准；表达式为空表示不用表达式。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_z_expr 表达式，如 "sin(x) + cos(y)"。
 *
 * @return {ret_t} 返回RET_OK表示成功，语法不合法时返回RET_BAD_PARAMS且保持原表达式。
 */
ret_t plot3d_set_sample_z_expr(widget_t* widget, const char* sample_z_expr);

/**
 * @method plot3d_set_sample_z_matrix
 * 设置 matrix 模式的 z 矩阵，形如 "1,2,3;4,5,6"，分号或换行分行。
 * x 与 y 默认取列号与行号，设置了 sample_x_range 或 sample_y_range 则映射到该范围。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_z_matrix 矩阵，为空表示清空矩阵。
 *
 * @return {ret_t} 返回RET_OK表示成功；各行列数不同等格式错误时返回RET_BAD_PARAMS且保持原矩阵；
 *                 矩阵数据源未注册（裁剪场景）时同样返回RET_BAD_PARAMS。
 */
ret_t plot3d_set_sample_z_matrix(widget_t* widget, const char* sample_z_matrix);

/**
 * @method plot3d_set_z_matrix
 * 直接用内存中的 z 矩阵，数据会拷贝一份，与 sample_z_matrix 属性共用，后设置的生效。
 * @param {widget_t*} widget widget对象。
 * @param {const float_t*} zs 按行存放的 cols*rows 个 z 值，为NULL表示清空矩阵。
 * @param {uint32_t} cols 列数。
 * @param {uint32_t} rows 行数。
 *
 * @return {ret_t} 返回RET_OK表示成功；矩阵数据源未注册（裁剪场景）时返回RET_NOT_FOUND；
 *                 其它情况表示失败。
 */
ret_t plot3d_set_z_matrix(widget_t* widget, const float_t* zs, uint32_t cols, uint32_t rows);

/**
 * @method plot3d_set_sample_x_expr
 * 设置 curve 模式下 x 的表达式，变量为 t，为空时 x 取 t。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_x_expr 表达式，如 "cos(t)"。
 *
 * @return {ret_t} 返回RET_OK表示成功，语法不合法时返回RET_BAD_PARAMS且保持原表达式。
 */
ret_t plot3d_set_sample_x_expr(widget_t* widget, const char* sample_x_expr);

/**
 * @method plot3d_set_sample_y_expr
 * 设置 curve 模式下 y 的表达式，变量为 t，为空时 y 取 0。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_y_expr 表达式，如 "sin(t)"。
 *
 * @return {ret_t} 返回RET_OK表示成功，语法不合法时返回RET_BAD_PARAMS且保持原表达式。
 */
ret_t plot3d_set_sample_y_expr(widget_t* widget, const char* sample_y_expr);

/**
 * @method plot3d_set_sample_t_range
 * 设置曲线采样的参数 t 范围。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_t_range 形如 "0,6.28"。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_t_range(widget_t* widget, const char* sample_t_range);

/**
 * @method plot3d_set_sample_t_steps
 * 设置曲线采样的点数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {uint32_t} sample_t_steps 点数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_sample_t_steps(widget_t* widget, uint32_t sample_t_steps);

/**
 * @method plot3d_set_curve_func
 * 设置曲线采样函数，控件按 sample-t-range/sample-t-steps 采样，得到一条连续的点序列。
 * 设置采样函数会让位于它的 dataset 数据失效。
 * @param {widget_t*} widget widget对象。
 * @param {plot3d_curve_func_t} func 采样函数，为NULL表示取消。
 * @param {void*} ctx 回调上下文。
 *
 * @return {ret_t} 返回RET_OK表示成功；曲线数据源未注册（裁剪场景）时返回RET_NOT_FOUND；
 *                 其它情况表示失败。
 */
ret_t plot3d_set_curve_func(widget_t* widget, plot3d_curve_func_t func, void* ctx);

/**
 * @method plot3d_set_sample_color_expr
 * 设置采样点的配色表达式，变量为 x、y、z（curve 模式还有 t）。
 * 结果为颜色字符串时直接用该颜色，为数值时按该值在配色表上取色。
 * 与配色函数同时存在时以配色函数为准；表达式为空表示按 z 取色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} sample_color_expr 表达式，如 "sqrt(x * x + y * y)"。
 *
 * @return {ret_t} 返回RET_OK表示成功；语法不合法时返回RET_BAD_PARAMS且保持原表达式；
 *                 表达式配色插件未注册（裁剪场景）时同样返回RET_BAD_PARAMS。
 */
ret_t plot3d_set_sample_color_expr(widget_t* widget, const char* sample_color_expr);

/**
 * @method plot3d_set_color_func
 * 设置采样点的配色函数，取代默认的按 z 在配色表上取色。
 * @param {widget_t*} widget widget对象。
 * @param {plot3d_color_func_t} func 配色函数，为NULL表示取消。
 * @param {void*} ctx 回调上下文。
 *
 * @return {ret_t} 返回RET_OK表示成功；表达式配色插件未注册（裁剪场景）时返回RET_NOT_FOUND；
 *                 其它情况表示失败。
 */
ret_t plot3d_set_color_func(widget_t* widget, plot3d_color_func_t func, void* ctx);

/**
 * @method plot3d_parse_range
 * 解析采样范围，顺序写反时按小到大返回。
 * @param {const char*} text 形如 "min,max"。
 * @param {float_t*} out_min 返回下界。
 * @param {float_t*} out_max 返回上界。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_parse_range(const char* text, float_t* out_min, float_t* out_max);

/**
 * @method plot3d_parse_steps
 * 解析采样格点数，结果钳到 PLOT3D_MIN_SAMPLE_STEPS 与 PLOT3D_MAX_SAMPLE_STEPS 之间。
 * @param {const char*} text 形如 "cols,rows"，只写一个数时两个方向相同。
 * @param {uint32_t*} out_cols 返回 x 方向格点数。
 * @param {uint32_t*} out_rows 返回 y 方向格点数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_parse_steps(const char* text, uint32_t* out_cols, uint32_t* out_rows);

/**
 * @method plot3d_sample_pos_init
 * 初始化采样点位置：全部字段清零后填入 x/y/z，t 按未定义处理。
 *
 * 曲线等有参数 t 的场景在本函数之后再填 t 与 has_t，其它场景直接用即可，不必操心 has_t。
 *
 * @param {plot3d_sample_pos_t*} pos 采样点位置。
 * @param {float_t} x x坐标。
 * @param {float_t} y y坐标。
 * @param {float_t} z z坐标。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_sample_pos_init(plot3d_sample_pos_t* pos, float_t x, float_t y, float_t z);

/**
 * @method plot3d_colormap_get_color
 * 按配色表取色：色标之间线性插值，名字不认识时用默认配色表。
 * @param {const char*} name 配色表名称。
 * @param {float_t} t 归一化位置，超出 [0,1] 时钳到两端。
 * @param {color_t*} out_color 返回颜色。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_colormap_get_color(const char* name, float_t t, color_t* out_color);

/**
 * @method plot3d_set_xlabel
 * 设置 X 轴名称。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} xlabel X 轴名称。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_xlabel(widget_t* widget, const char* xlabel);

/**
 * @method plot3d_set_ylabel
 * 设置 Y 轴名称。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} ylabel Y 轴名称。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_ylabel(widget_t* widget, const char* ylabel);

/**
 * @method plot3d_set_zlabel
 * 设置 Z 轴名称。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} zlabel Z 轴名称。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_zlabel(widget_t* widget, const char* zlabel);

/**
 * @method plot3d_set_show_grid
 * 设置是否显示网格。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} show_grid 是否显示网格。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_show_grid(widget_t* widget, bool_t show_grid);

/**
 * @method plot3d_set_show_datatip
 * 设置是否显示数据提示。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} show_datatip 是否显示数据提示。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_show_datatip(widget_t* widget, bool_t show_datatip);

/**
 * @method plot3d_set_grid_color
 * 设置网格颜色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} grid_color 颜色字符串。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_grid_color(widget_t* widget, const char* grid_color);

/**
 * @method plot3d_set_xaxis_color
 * 设置 X 轴颜色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} xaxis_color 颜色字符串。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_xaxis_color(widget_t* widget, const char* xaxis_color);

/**
 * @method plot3d_set_yaxis_color
 * 设置 Y 轴颜色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} yaxis_color 颜色字符串。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_yaxis_color(widget_t* widget, const char* yaxis_color);

/**
 * @method plot3d_set_zaxis_color
 * 设置 Z 轴颜色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} zaxis_color 颜色字符串。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_zaxis_color(widget_t* widget, const char* zaxis_color);

/**
 * @method plot3d_set_tick_color
 * 设置刻度文字颜色。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {const char*} tick_color 颜色字符串。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_tick_color(widget_t* widget, const char* tick_color);

/**
 * @method plot3d_set_axis_negative_brightness
 * 设置负半轴相对正半轴的亮度系数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} axis_negative_brightness 亮度系数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_axis_negative_brightness(widget_t* widget, float_t axis_negative_brightness);

/**
 * @method plot3d_set_show_axis_tick
 * 设置是否显示轴刻度文字。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} show_axis_tick 是否显示轴刻度文字。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_show_axis_tick(widget_t* widget, bool_t show_axis_tick);

/**
 * @method plot3d_set_point_size
 * 设置点图点大小。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} point_size 点大小。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_point_size(widget_t* widget, float_t point_size);

/**
 * @method plot3d_set_line_width
 * 设置折线线宽。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} line_width 线宽。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_line_width(widget_t* widget, float_t line_width);

/**
 * @method plot3d_set_enable_cache
 * 设置是否启用投影/绘制缓存。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} enable_cache 是否启用缓存。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_enable_cache(widget_t* widget, bool_t enable_cache);

/**
 * @method plot3d_set_equal_axis
 * 设置是否三轴等比例。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {bool_t} equal_axis 是否等比例。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_equal_axis(widget_t* widget, bool_t equal_axis);

/**
 * @method plot3d_set_camera_yaw
 * 设置相机方位角。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} camera_yaw 方位角（弧度）。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_camera_yaw(widget_t* widget, float_t camera_yaw);

/**
 * @method plot3d_set_camera_pitch
 * 设置相机俯仰角。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} camera_pitch 俯仰角（弧度）。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_camera_pitch(widget_t* widget, float_t camera_pitch);

/**
 * @method plot3d_set_camera_distance
 * 设置相机距离。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} camera_distance 距离。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_camera_distance(widget_t* widget, float_t camera_distance);

/**
 * @method plot3d_set_camera_z_offset
 * 设置相机沿 Z 方向偏移。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} camera_z_offset 偏移量。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_camera_z_offset(widget_t* widget, float_t camera_z_offset);

/**
 * @method plot3d_set_xy_grid_position
 * 设置 XY 网格平面在 Z 轴上的位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} xy_grid_position 位置。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_xy_grid_position(widget_t* widget, float_t xy_grid_position);

/**
 * @method plot3d_set_xz_grid_position
 * 设置 XZ 网格平面在 Y 轴上的位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} xz_grid_position 位置。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_xz_grid_position(widget_t* widget, float_t xz_grid_position);

/**
 * @method plot3d_set_yz_grid_position
 * 设置 YZ 网格平面在 X 轴上的位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} yz_grid_position 位置。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_yz_grid_position(widget_t* widget, float_t yz_grid_position);

/**
 * @method plot3d_step_xy_grid_position
 * 按刻度步进调整 XY 网格平面位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {int32_t} dir 方向：正数向前，负数向后。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_step_xy_grid_position(widget_t* widget, int32_t dir);

/**
 * @method plot3d_step_xz_grid_position
 * 按刻度步进调整 XZ 网格平面位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {int32_t} dir 方向：正数向前，负数向后。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_step_xz_grid_position(widget_t* widget, int32_t dir);

/**
 * @method plot3d_step_yz_grid_position
 * 按刻度步进调整 YZ 网格平面位置。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {int32_t} dir 方向：正数向前，负数向后。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_step_yz_grid_position(widget_t* widget, int32_t dir);

/**
 * @method plot3d_set_box_aspect_x
 * 设置包围盒 X 边相对长度系数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} box_aspect_x 相对长度系数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_box_aspect_x(widget_t* widget, float_t box_aspect_x);

/**
 * @method plot3d_set_box_aspect_y
 * 设置包围盒 Y 边相对长度系数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} box_aspect_y 相对长度系数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_box_aspect_y(widget_t* widget, float_t box_aspect_y);

/**
 * @method plot3d_set_box_aspect_z
 * 设置包围盒 Z 边相对长度系数。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} box_aspect_z 相对长度系数。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_box_aspect_z(widget_t* widget, float_t box_aspect_z);

/**
 * @method plot3d_set_x_grid_count
 * 设置 X 轴网格格数上限。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {uint32_t} x_grid_count 格数上限。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_x_grid_count(widget_t* widget, uint32_t x_grid_count);

/**
 * @method plot3d_set_y_grid_count
 * 设置 Y 轴网格格数上限。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {uint32_t} y_grid_count 格数上限。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_y_grid_count(widget_t* widget, uint32_t y_grid_count);

/**
 * @method plot3d_set_z_grid_count
 * 设置 Z 轴网格格数上限。
 * @annotation ["scriptable"]
 * @param {widget_t*} widget widget对象。
 * @param {uint32_t} z_grid_count 格数上限。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_set_z_grid_count(widget_t* widget, uint32_t z_grid_count);

/*for test*/
typedef struct _plot3d_axis_color_segment_t {
  float_t t0;
  float_t t1;
  color_t color;
} plot3d_axis_color_segment_t;

typedef enum _plot3d_axis_t {
  PLOT3D_AXIS_X = 0,
  PLOT3D_AXIS_Y,
  PLOT3D_AXIS_Z
} plot3d_axis_t;

typedef struct _plot3d_axis_text_edge_t {
  float_t x0;
  float_t y0;
  float_t z0;
  float_t x1;
  float_t y1;
  float_t z1;
} plot3d_axis_text_edge_t;

ret_t plot3d_pick_axis_text_normal(float_t dx, float_t dy, float_t sx, float_t sy, float_t cx,
                                    float_t cy, float_t* out_nx, float_t* out_ny);
ret_t plot3d_calc_axis_scales(bool_t equal_axis, float_t extent_x, float_t extent_y,
                               float_t extent_z, float_t aspect_x, float_t aspect_y,
                               float_t aspect_z, float_t* out_scale_x, float_t* out_scale_y,
                               float_t* out_scale_z);
ret_t plot3d_calc_nice_axis(float_t min_v, float_t max_v, uint32_t max_count, float_t* out_min,
                             float_t* out_max, uint32_t* out_count, uint32_t* out_decimals);
uint32_t plot3d_build_axis_color_segments(float_t min_v, float_t max_v, color_t positive_color,
                                           float_t brightness, plot3d_axis_color_segment_t out[2]);
ret_t plot3d_pick_axis_text_edge(plot3d_axis_t axis, float_t camera_yaw, float_t min_x,
                                  float_t max_x, float_t min_y, float_t max_y, float_t min_z,
                                  float_t max_z, plot3d_axis_text_edge_t* out);
ret_t plot3d_calc_axis_tick_offset(float_t font_size, float_t nx, float_t ny, float_t text_w,
                                    float_t* out_offset);
ret_t plot3d_calc_axis_name_offset(float_t font_size, float_t nx, float_t ny, float_t tick_text_w,
                                    float_t name_text_w, float_t* out_offset);
ret_t plot3d_calc_axis_tick_stride(float_t edge_len, uint32_t count, float_t ux, float_t uy,
                                    float_t text_w, float_t font_size, uint32_t* out_stride);
ret_t plot3d_calc_axis_tick_range(plot3d_axis_t axis, float_t camera_yaw, uint32_t count,
                                   uint32_t* out_begin, uint32_t* out_end);

/*for test*/
/* widget_set_prop 在 vtable 返回 RET_NOT_FOUND 时会把值写进 custom_props 并返回 RET_OK，
 * 这个兜底会把「插件没认领该属性」伪装成成功，所以测试只能直连 vtable 里的 static 函数。 */
ret_t plot3d_set_prop_for_test(widget_t* widget, const char* name, const value_t* v);
ret_t plot3d_get_prop_for_test(widget_t* widget, const char* name, value_t* v);
uint32_t plot3d_get_source_nr_for_test(widget_t* widget);
uint32_t plot3d_get_colorizer_nr_for_test(widget_t* widget);
uint32_t plot3d_get_type_nr_for_test(widget_t* widget);
uint32_t plot3d_get_resample_nr_for_test(widget_t* widget);

#define PLOT3D_PROP_DATASET "dataset"
#define PLOT3D_PROP_PLOTTYPE "plottype"
#define PLOT3D_PROP_XLABEL "xlabel"
#define PLOT3D_PROP_YLABEL "ylabel"
#define PLOT3D_PROP_ZLABEL "zlabel"
#define PLOT3D_PROP_SHOW_GRID "show_grid"
#define PLOT3D_PROP_SHOW_DATATIP "show-datatip"
#define PLOT3D_PROP_GRID_COLOR "grid_color"
#define PLOT3D_PROP_XAXIS_COLOR "xaxis-color"
#define PLOT3D_PROP_YAXIS_COLOR "yaxis-color"
#define PLOT3D_PROP_ZAXIS_COLOR "zaxis-color"
#define PLOT3D_PROP_TICK_COLOR "tick-color"
#define PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS "axis-negative-brightness"
#define PLOT3D_PROP_SHOW_AXIS_TICK "show_axis_tick"
#define PLOT3D_PROP_POINT_SIZE "point_size"
#define PLOT3D_PROP_LINE_WIDTH "line_width"
#define PLOT3D_PROP_ENABLE_CACHE "enable_cache"
#define PLOT3D_PROP_EQUAL_AXIS "equal-axis"
#define PLOT3D_PROP_CAMERA_YAW "camera_yaw"
#define PLOT3D_PROP_CAMERA_PITCH "camera_pitch"
#define PLOT3D_PROP_CAMERA_DISTANCE "camera_distance"
#define PLOT3D_PROP_CAMERA_Z_OFFSET "camera_z_offset"
#define PLOT3D_PROP_XY_GRID_POSITION "xy-grid-position"
#define PLOT3D_PROP_XZ_GRID_POSITION "xz-grid-position"
#define PLOT3D_PROP_YZ_GRID_POSITION "yz-grid-position"
#define PLOT3D_PROP_BOX_ASPECT_X "box-aspect-x"
#define PLOT3D_PROP_BOX_ASPECT_Y "box-aspect-y"
#define PLOT3D_PROP_BOX_ASPECT_Z "box-aspect-z"
#define PLOT3D_PROP_SAMPLE_MODE "sample-mode"
#define PLOT3D_PROP_SAMPLE_X_RANGE "sample-x-range"
#define PLOT3D_PROP_SAMPLE_Y_RANGE "sample-y-range"
#define PLOT3D_PROP_SAMPLE_STEPS "sample-steps"
#define PLOT3D_PROP_SAMPLE_Z_EXPR "sample-z-expr"
#define PLOT3D_PROP_SAMPLE_X_EXPR "sample-x-expr"
#define PLOT3D_PROP_SAMPLE_Y_EXPR "sample-y-expr"
#define PLOT3D_PROP_SAMPLE_T_RANGE "sample-t-range"
#define PLOT3D_PROP_SAMPLE_T_STEPS "sample-t-steps"
#define PLOT3D_PROP_SAMPLE_COLOR_EXPR "sample-color-expr"
#define PLOT3D_PROP_SAMPLE_Z_MATRIX "sample-z-matrix"
#define PLOT3D_PROP_COLORMAP "colormap"
#define PLOT3D_PROP_X_GRID_COUNT "x-grid-count"
#define PLOT3D_PROP_Y_GRID_COUNT "y-grid-count"
#define PLOT3D_PROP_Z_GRID_COUNT "z-grid-count"

#define PLOT3D_SAMPLE_MODE_NONE "none"
#define PLOT3D_SAMPLE_MODE_GRID "grid"
#define PLOT3D_SAMPLE_MODE_CURVE "curve"
#define PLOT3D_SAMPLE_MODE_MATRIX "matrix"
#define PLOT3D_MIN_SAMPLE_STEPS 2u
#define PLOT3D_MAX_SAMPLE_STEPS 100u
/* 曲线只有一个方向，点数上限比网格宽松得多。 */
#define PLOT3D_MAX_CURVE_STEPS 10000u
#define PLOT3D_DEFAULT_CURVE_STEPS 200u

#define WIDGET_TYPE_PLOT3D "plot3d"

#define PLOT3D(widget) ((plot3d_t*)(plot3d_cast(WIDGET(widget))))

/*public for subclass and runtime type check*/
TK_EXTERN_VTABLE(plot3d);

END_C_DECLS

#endif /*TK_PLOT3D_H*/
