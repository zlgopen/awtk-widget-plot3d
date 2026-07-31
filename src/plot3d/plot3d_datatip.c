/**
 * File:   plot3d_datatip.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 悬停 DataTip 拾取
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

#include "base/vgcanvas.h"
#include "tkc/utils.h"
#include "plot3d.h"
#include "plot3d_datatip.h"

#define PLOT3D_DATATIP_MARKER_RADIUS 5.0f
#define PLOT3D_DATATIP_BOX_PADDING_X 8.0f
#define PLOT3D_DATATIP_BOX_PADDING_Y 6.0f
#define PLOT3D_DATATIP_LINE_GAP 4.0f
#define PLOT3D_DATATIP_BOX_RADIUS 4.0f

int32_t plot3d_datatip_pick_nearest(const plot3d_projected_point_t* points, uint32_t points_nr,
                                    float_t screen_x, float_t screen_y, float_t threshold_px) {
  uint32_t i = 0;
  int32_t best_index = -1;
  float_t best_dist2 = 0.0f;
  float_t threshold2 = threshold_px * threshold_px;
  bool_t has_best = FALSE;

  if (points == NULL || points_nr == 0) {
    return -1;
  }

  for (i = 0; i < points_nr; i++) {
    const plot3d_projected_point_t* p = points + i;
    float_t dx = 0.0f;
    float_t dy = 0.0f;
    float_t dist2 = 0.0f;

    if (!p->valid) {
      continue;
    }

    dx = p->sx - screen_x;
    dy = p->sy - screen_y;
    dist2 = dx * dx + dy * dy;

    if (!has_best || dist2 < best_dist2) {
      best_dist2 = dist2;
      best_index = (int32_t)i;
      has_best = TRUE;
    }
  }

  if (!has_best || best_dist2 > threshold2) {
    return -1;
  }

  return best_index;
}

void plot3d_datatip_compute_label_pos(float_t marker_sx, float_t marker_sy, float_t box_w,
                                      float_t box_h, float_t clip_l, float_t clip_t,
                                      float_t clip_r, float_t clip_b, float_t* out_x,
                                      float_t* out_y) {
  float_t x = marker_sx + 12;
  float_t y = marker_sy - box_h - 8;

  return_if_fail(out_x != NULL && out_y != NULL);

  if (x + box_w > clip_r) {
    x = marker_sx - box_w - 12;
  }
  if (y < clip_t) {
    y = marker_sy + 8;
  }

  *out_x = tk_max(clip_l, tk_min(x, clip_r - box_w));
  *out_y = tk_max(clip_t, tk_min(y, clip_b - box_h));
}

ret_t plot3d_datatip_paint(widget_t* widget, canvas_t* c) {
  uint32_t i = 0;
  float_t x = 0;
  float_t y = 0;
  float_t box_w = 0;
  float_t box_h = 0;
  float_t line_h = 0;
  float_t values[3];
  char text[3][32];
  vgcanvas_t* vg = NULL;
  plot3d_data_point_t* data_point = NULL;
  plot3d_projected_point_t* projected = NULL;
  plot3d_projected_point_t* marker = NULL;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL && c != NULL, RET_BAD_PARAMS);

  if (!plot3d->show_datatip || plot3d->hover_index < 0 ||
      (uint32_t)plot3d->hover_index >= plot3d->projected_points_nr ||
      plot3d->projected_points == NULL) {
    return RET_OK;
  }

  projected = (plot3d_projected_point_t*)plot3d->projected_points;
  marker = projected + plot3d->hover_index;
  if (!marker->valid) {
    return RET_OK;
  }

  values[0] = marker->x;
  values[1] = marker->y;
  values[2] = marker->z;
  if ((uint32_t)plot3d->hover_index < plot3d->data_points.size) {
    data_point = (plot3d_data_point_t*)darray_get(&(plot3d->data_points), plot3d->hover_index);
    if (data_point != NULL) {
      values[0] = data_point->x;
      values[1] = data_point->y;
      values[2] = data_point->z;
    }
  }

  widget_prepare_text_style(widget, c);
  vg = canvas_get_vgcanvas(c);
  return_value_if_fail(vg != NULL, RET_OK);

  vgcanvas_save(vg);
  if (c->font_name != NULL) {
    vgcanvas_set_font(vg, c->font_name);
  }
  vgcanvas_set_font_size(vg, (float_t)c->font_size);
  vgcanvas_set_text_align(vg, "left");
  vgcanvas_set_text_baseline(vg, "top");

  tk_snprintf(text[0], sizeof(text[0]), "X:  %.3f", values[0]);
  tk_snprintf(text[1], sizeof(text[1]), "Y:  %.3f", values[1]);
  tk_snprintf(text[2], sizeof(text[2]), "Z:  %.3f", values[2]);
  line_h = (float_t)c->font_size + PLOT3D_DATATIP_LINE_GAP;
  for (i = 0; i < ARRAY_SIZE(text); i++) {
    box_w = tk_max(box_w, vgcanvas_measure_text(vg, text[i]));
  }
  box_w += PLOT3D_DATATIP_BOX_PADDING_X * 2;
  box_h = line_h * ARRAY_SIZE(text) + PLOT3D_DATATIP_BOX_PADDING_Y * 2;

  plot3d_datatip_compute_label_pos(
      marker->sx, marker->sy, box_w, box_h, widget->x, widget->y, widget->x + widget->w,
      widget->y + widget->h, &x, &y);

  vgcanvas_set_fill_color(vg, color_init(32, 36, 44, 220));
  vgcanvas_begin_path(vg);
  vgcanvas_rounded_rect(vg, x, y, box_w, box_h, PLOT3D_DATATIP_BOX_RADIUS);
  vgcanvas_fill(vg);

  vgcanvas_set_fill_color(vg, color_init(245, 247, 250, 255));
  for (i = 0; i < ARRAY_SIZE(text); i++) {
    vgcanvas_fill_text(vg, text[i], x + PLOT3D_DATATIP_BOX_PADDING_X,
                       y + PLOT3D_DATATIP_BOX_PADDING_Y + line_h * i, box_w);
  }

  vgcanvas_set_fill_color(vg, color_init(255, 221, 64, 255));
  vgcanvas_begin_path(vg);
  vgcanvas_ellipse(vg, marker->sx, marker->sy, PLOT3D_DATATIP_MARKER_RADIUS,
                   PLOT3D_DATATIP_MARKER_RADIUS);
  vgcanvas_fill(vg);
  vgcanvas_set_stroke_color(vg, color_init(255, 255, 255, 255));
  vgcanvas_set_line_width(vg, 2);
  vgcanvas_begin_path(vg);
  vgcanvas_ellipse(vg, marker->sx, marker->sy, PLOT3D_DATATIP_MARKER_RADIUS,
                   PLOT3D_DATATIP_MARKER_RADIUS);
  vgcanvas_stroke(vg);
  vgcanvas_restore(vg);

  return RET_OK;
}

void plot3d_datatip_clear_hover(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  return_if_fail(plot3d != NULL);

  if (plot3d->hover_index != -1) {
    plot3d->hover_index = -1;
    widget_invalidate(widget, NULL);
  }
}

void plot3d_datatip_sanitize_hover(widget_t* widget) {
  plot3d_t* plot3d = PLOT3D(widget);
  const plot3d_projected_point_t* projected = NULL;
  return_if_fail(plot3d != NULL);

  if (plot3d->hover_index < 0) {
    return;
  }

  if ((uint32_t)plot3d->hover_index >= plot3d->projected_points_nr ||
      plot3d->projected_points == NULL) {
    plot3d_datatip_clear_hover(widget);
    return;
  }

  projected = (const plot3d_projected_point_t*)plot3d->projected_points;
  if (!projected[plot3d->hover_index].valid) {
    plot3d_datatip_clear_hover(widget);
  }
}

ret_t plot3d_datatip_update_hover(widget_t* widget, float_t screen_x, float_t screen_y) {
  int32_t hover_index = -1;
  plot3d_t* plot3d = PLOT3D(widget);
  return_value_if_fail(plot3d != NULL, RET_BAD_PARAMS);

  plot3d_datatip_sanitize_hover(widget);

  if (plot3d->show_datatip && plot3d->projected_points != NULL &&
      plot3d->projected_points_nr > 0) {
    hover_index = plot3d_datatip_pick_nearest(
        (const plot3d_projected_point_t*)plot3d->projected_points,
        plot3d->projected_points_nr, screen_x, screen_y, PLOT3D_DATATIP_HIT_THRESHOLD_PX);
  }

  if (hover_index != plot3d->hover_index) {
    plot3d->hover_index = hover_index;
    widget_invalidate(widget, NULL);
  }

  return RET_OK;
}
