/**
 * File:   plot3d_scene.h
 * Author: AWTK Develop Team
 * Brief:  Plot3D 场景几何与投影
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

#ifndef TK_PLOT3D_SCENE_H
#define TK_PLOT3D_SCENE_H

#include "type/plot3d_type.h"

BEGIN_C_DECLS

typedef struct _plot3d_t plot3d_t;

typedef struct _plot3d_bounds_t {
  bool_t valid;
  float_t min_x;
  float_t min_y;
  float_t min_z;
  float_t max_x;
  float_t max_y;
  float_t max_z;
  float_t center_x;
  float_t center_y;
  float_t center_z;
  float_t extent_x;
  float_t extent_y;
  float_t extent_z;
  float_t max_extent;
  float_t scale_x;
  float_t scale_y;
  float_t scale_z;
  uint32_t count_x;
  uint32_t count_y;
  uint32_t count_z;
  uint32_t decimals_x;
  uint32_t decimals_y;
  uint32_t decimals_z;
} plot3d_bounds_t;

typedef struct _plot3d_axis_range_t {
  float_t min_v;
  float_t max_v;
  float_t extent;
  uint32_t count;
  uint32_t decimals;
} plot3d_axis_range_t;

ret_t plot3d_bounds_get_axis(const plot3d_bounds_t* bounds, plot3d_axis_t axis,
                              plot3d_axis_range_t* out);
float_t plot3d_axis_tick_value(const plot3d_axis_range_t* range, uint32_t index);
ret_t plot3d_calc_bounds(plot3d_t* plot3d, plot3d_bounds_t* bounds);
bool_t plot3d_project_world_point(widget_t* widget, const plot3d_bounds_t* bounds, float_t x,
                                   float_t y, float_t z, plot3d_projected_point_t* out);
ret_t plot3d_update_projection_cache(widget_t* widget, const plot3d_bounds_t* bounds);
int plot3d_primitive_compare(const void* a, const void* b);

END_C_DECLS

#endif /*TK_PLOT3D_SCENE_H*/
