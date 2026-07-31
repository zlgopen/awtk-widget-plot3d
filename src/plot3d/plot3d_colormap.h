/**
 * File:   plot3d_colormap.h
 * Author: AWTK Develop Team
 * Brief:  Plot3D 配色表查表
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

#ifndef TK_PLOT3D_COLORMAP_H
#define TK_PLOT3D_COLORMAP_H

#include "tkc/types_def.h"
#include "tkc/color.h"

BEGIN_C_DECLS

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

END_C_DECLS

#endif /*TK_PLOT3D_COLORMAP_H*/
