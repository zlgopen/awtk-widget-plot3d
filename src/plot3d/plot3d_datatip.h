#ifndef TK_PLOT3D_DATATIP_H
#define TK_PLOT3D_DATATIP_H

#include "base/widget.h"
#include "plot3d/type/plot3d_type.h"

BEGIN_C_DECLS

#define PLOT3D_DATATIP_HIT_THRESHOLD_PX 20

/**
 * @method plot3d_datatip_pick_nearest
 * 在投影点中选取距屏幕坐标最近且在阈值内的点。
 * @annotation ["global"]
 * @param {const plot3d_projected_point_t*} points 投影点数组。
 * @param {uint32_t} points_nr 点数。
 * @param {float_t} screen_x 屏幕 x。
 * @param {float_t} screen_y 屏幕 y。
 * @param {float_t} threshold_px 命中阈值（像素）。
 *
 * @return {int32_t} 返回最近点索引，未命中返回 -1。
 */
int32_t plot3d_datatip_pick_nearest(const plot3d_projected_point_t* points, uint32_t points_nr,
                                    float_t screen_x, float_t screen_y, float_t threshold_px);

/**
 * @method plot3d_datatip_update_hover
 * 根据屏幕坐标更新 hover 点。
 * @annotation ["global"]
 * @param {widget_t*} widget widget对象。
 * @param {float_t} screen_x 屏幕 x。
 * @param {float_t} screen_y 屏幕 y。
 *
 * @return {ret_t} 返回RET_OK表示成功，否则表示失败。
 */
ret_t plot3d_datatip_update_hover(widget_t* widget, float_t screen_x, float_t screen_y);

/**
 * @method plot3d_datatip_clear_hover
 * 清除 hover 状态。
 * @annotation ["global"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {void} 返回无。
 */
void plot3d_datatip_clear_hover(widget_t* widget);

/**
 * @method plot3d_datatip_sanitize_hover
 * 校验并清理无效的 hover 索引。
 * @annotation ["global"]
 * @param {widget_t*} widget widget对象。
 *
 * @return {void} 返回无。
 */
void plot3d_datatip_sanitize_hover(widget_t* widget);

ret_t plot3d_datatip_paint(widget_t* widget, canvas_t* c);

/**
 * @method plot3d_datatip_compute_label_pos
 * 计算 datatip 标签框位置，尽量不越出裁剪矩形。
 * @annotation ["global"]
 * @param {float_t} marker_sx 标记点屏幕 x。
 * @param {float_t} marker_sy 标记点屏幕 y。
 * @param {float_t} box_w 标签框宽。
 * @param {float_t} box_h 标签框高。
 * @param {float_t} clip_l 裁剪左。
 * @param {float_t} clip_t 裁剪上。
 * @param {float_t} clip_r 裁剪右。
 * @param {float_t} clip_b 裁剪下。
 * @param {float_t*} out_x 返回标签左上角 x。
 * @param {float_t*} out_y 返回标签左上角 y。
 *
 * @return {void} 返回无。
 */
void plot3d_datatip_compute_label_pos(float_t marker_sx, float_t marker_sy, float_t box_w,
                                      float_t box_h, float_t clip_l, float_t clip_t,
                                      float_t clip_r, float_t clip_b, float_t* out_x,
                                      float_t* out_y);

END_C_DECLS

#endif /*TK_PLOT3D_DATATIP_H*/
