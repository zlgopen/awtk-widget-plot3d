#ifndef TK_PLOT3D_DATATIP_H
#define TK_PLOT3D_DATATIP_H

#include "base/widget.h"
#include "plot3d/type/plot3d_type.h"

BEGIN_C_DECLS

#define PLOT3D_DATATIP_HIT_THRESHOLD_PX 20

/*for test*/
int32_t plot3d_datatip_pick_nearest(const plot3d_projected_point_t* points, uint32_t points_nr,
                                    float_t screen_x, float_t screen_y, float_t threshold_px);

/*for test*/
ret_t plot3d_datatip_update_hover(widget_t* widget, float_t screen_x, float_t screen_y);

/*for test*/
void plot3d_datatip_clear_hover(widget_t* widget);

/*for test*/
void plot3d_datatip_sanitize_hover(widget_t* widget);

ret_t plot3d_datatip_paint(widget_t* widget, canvas_t* c);

/*for test*/
void plot3d_datatip_compute_label_pos(float_t marker_sx, float_t marker_sy, float_t box_w,
                                      float_t box_h, float_t clip_l, float_t clip_t,
                                      float_t clip_r, float_t clip_b, float_t* out_x,
                                      float_t* out_y);

END_C_DECLS

#endif /*TK_PLOT3D_DATATIP_H*/
