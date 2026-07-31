/**
 * File:   plot3d_paint.h
 * Author: AWTK Develop Team
 * Brief:  Plot3D 上屏绘制
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

#ifndef TK_PLOT3D_PAINT_H
#define TK_PLOT3D_PAINT_H

#include "plot3d.h"
#include "plot3d_scene.h"

BEGIN_C_DECLS

ret_t plot3d_draw_grid_and_axis(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds);
ret_t plot3d_draw_data(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds);
ret_t plot3d_draw_axis_text(widget_t* widget, canvas_t* c, const plot3d_bounds_t* bounds);

END_C_DECLS

#endif /*TK_PLOT3D_PAINT_H*/
