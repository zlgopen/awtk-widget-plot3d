/**
 * File:   plot3d_register.h
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


#ifndef TK_PLOT3D_REGISTER_H
#define TK_PLOT3D_REGISTER_H

#include "base/widget.h"

BEGIN_C_DECLS

/**
 * @method plot3d_register
 * 注册控件。
 *
 * 只注册控件，刻意不注册任何内置插件，以便资源受限的场景手工注册用到的插件；
 * 需要全部内置插件请用 plot3d_register_all。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_register(void);

/**
 * @method plot3d_register_all
 * 注册控件及全部内置插件。
 *
 * 资源受限的场景可以改用 plot3d_register 只注册控件，再手工注册用到的插件。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_register_all(void);

/**
 * @method plot3d_unregister
 * 反注册控件及全部插件。
 *
 * 必须在 tk_deinit/tk_deinit_internal 之前调用：tk_deinit_internal 会销毁控件工厂并把它
 * 置为 NULL，之后再调用只会返回RET_BAD_PARAMS并刷日志。调用之后不应再创建或操作 plot3d
 * 控件。重复调用是安全的。
 *
 * @annotation ["global"]
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_unregister(void);

/**
 * @method plot3d_supported_render_mode
 * 获取支持的渲染模式。
 *
 * @annotation ["global"]
 *
 * @return {const char*} 返回渲染模式。
 */
const char* plot3d_supported_render_mode(void);

END_C_DECLS

#endif /*TK_PLOT3D_REGISTER_H*/
