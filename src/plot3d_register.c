/**
 * File:   plot3d.c
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


#include "tkc/mem.h"
#include "tkc/utils.h"
#include "plot3d_register.h"
#include "base/widget_factory.h"
#include "plot3d/plot3d.h"
#include "plot3d/colorizer/plot3d_colorizer.h"
#include "plot3d/source/plot3d_source.h"
#include "plot3d/type/plot3d_type.h"

ret_t plot3d_register(void) {
  return widget_factory_register(widget_factory(), WIDGET_TYPE_PLOT3D, plot3d_create);
}

ret_t plot3d_register_all(void) {
  return_value_if_fail(plot3d_register() == RET_OK, RET_FAIL);

  /* 内置插件在此逐个注册，每个写成
   *   return_value_if_fail(plot3d_source_xxx_register() == RET_OK, RET_FAIL);
   * 失败即返回，不做回滚，由调用方整体放弃初始化（与 AWTK tk_init_internal 一致）。
   * 裁剪时改用 plot3d_register() 加手工注册，并同步删减 src/SConscript 中的插件清单。 */
  return_value_if_fail(plot3d_source_csv_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_source_matrix_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_source_curve_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_source_grid_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_colorizer_expr_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_type_line_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_type_dot_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_type_surface_register() == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_type_cylinder_register() == RET_OK, RET_FAIL);

  return RET_OK;
}

ret_t plot3d_unregister(void) {
  ret_t ret = RET_OK;
  ret_t tmp = RET_OK;

  /* 反注册要求尽力做完：不能用 return_value_if_fail 中途返回，否则会漏掉后面的注册表，
   * 因此只记下第一个错误码，末尾统一返回。 */
  tmp = plot3d_source_factory_deinit();
  if (ret == RET_OK) {
    ret = tmp;
  }

  tmp = plot3d_colorizer_factory_deinit();
  if (ret == RET_OK) {
    ret = tmp;
  }

  tmp = plot3d_type_factory_deinit();
  if (ret == RET_OK) {
    ret = tmp;
  }

  /* widget_factory_t 是 general_factory_t 的 typedef，后者才有 unregister。 */
  tmp = general_factory_unregister(widget_factory(), WIDGET_TYPE_PLOT3D);
  if (ret == RET_OK) {
    ret = tmp;
  }

  return ret;
}

const char* plot3d_supported_render_mode(void) {
  return "OpenGL|AGGE-BGR565|AGGE-BGRA8888|AGGE-MONO";
}
