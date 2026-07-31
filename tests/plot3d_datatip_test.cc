#include "gtest/gtest.h"
#include "awtk.h"
#include "plot3d/plot3d.h"
#include "plot3d/plot3d_datatip.h"
#include "plot3d/plot3d_scene.h"
#include "plot3d/type/plot3d_type.h"
#include <string.h>

static plot3d_projected_point_t make_pt(float sx, float sy, bool_t valid) {
  plot3d_projected_point_t p;
  memset(&p, 0, sizeof(p));
  p.sx = sx;
  p.sy = sy;
  p.valid = valid;
  return p;
}

TEST(Plot3dDatatip, PickNearestIndex) {
  plot3d_projected_point_t pts[3];
  pts[0] = make_pt(10, 10, TRUE);
  pts[1] = make_pt(100, 100, TRUE);
  pts[2] = make_pt(50, 50, TRUE);
  ASSERT_EQ(2, plot3d_datatip_pick_nearest(pts, 3, 52, 48, PLOT3D_DATATIP_HIT_THRESHOLD_PX));
}

TEST(Plot3dDatatip, PickBeyondThresholdReturnsMinusOne) {
  plot3d_projected_point_t pts[1];
  pts[0] = make_pt(0, 0, TRUE);
  ASSERT_EQ(-1, plot3d_datatip_pick_nearest(pts, 1, 100, 100, 20));
}

TEST(Plot3dDatatip, PickIgnoresInvalid) {
  plot3d_projected_point_t pts[2];
  pts[0] = make_pt(10, 10, FALSE);
  pts[1] = make_pt(20, 20, TRUE);
  ASSERT_EQ(1, plot3d_datatip_pick_nearest(pts, 2, 12, 12, 20));
}

TEST(Plot3dDatatip, PickNullOrEmptyReturnsMinusOne) {
  ASSERT_EQ(-1, plot3d_datatip_pick_nearest(NULL, 0, 0, 0, 20));
}

TEST(Plot3dDatatip, ShowDatatipPropDefaultTrue) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  ASSERT_EQ(TRUE, widget_get_prop_bool(w, PLOT3D_PROP_SHOW_DATATIP, FALSE));
  ASSERT_EQ(RET_OK, plot3d_set_show_datatip(w, FALSE));
  ASSERT_EQ(FALSE, widget_get_prop_bool(w, PLOT3D_PROP_SHOW_DATATIP, TRUE));
  widget_destroy(w);
}

TEST(Plot3dDatatip, UpdateHoverPicksNearest) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  plot3d_t* plot3d = PLOT3D(w);
  plot3d_projected_point_t* pts =
      (plot3d_projected_point_t*)TKMEM_ALLOC(sizeof(plot3d_projected_point_t) * 2);
  memset(pts, 0, sizeof(plot3d_projected_point_t) * 2);
  pts[0].sx = 10;
  pts[0].sy = 10;
  pts[0].valid = TRUE;
  pts[1].sx = 100;
  pts[1].sy = 100;
  pts[1].valid = TRUE;
  TKMEM_FREE(plot3d->projected_points);
  plot3d->projected_points = pts;
  plot3d->projected_points_nr = 2;
  ASSERT_EQ(RET_OK, plot3d_datatip_update_hover(w, 12, 12));
  ASSERT_EQ(0, plot3d->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, UpdateHoverClearsOutOfRangeIndex) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  plot3d_t* plot3d = PLOT3D(w);
  plot3d_projected_point_t* pts =
      (plot3d_projected_point_t*)TKMEM_ALLOC(sizeof(plot3d_projected_point_t));
  pts[0] = make_pt(10, 10, TRUE);
  TKMEM_FREE(plot3d->projected_points);
  plot3d->projected_points = pts;
  plot3d->projected_points_nr = 1;
  plot3d->hover_index = 3;

  ASSERT_EQ(RET_OK, plot3d_datatip_update_hover(w, 100, 100));
  ASSERT_EQ(-1, plot3d->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, ClearHoverResetsIndex) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  PLOT3D(w)->hover_index = 3;
  plot3d_datatip_clear_hover(w);
  ASSERT_EQ(-1, PLOT3D(w)->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, DisabledSkipsHover) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  plot3d_t* plot3d = PLOT3D(w);
  plot3d_projected_point_t* pts =
      (plot3d_projected_point_t*)TKMEM_ALLOC(sizeof(plot3d_projected_point_t));
  memset(pts, 0, sizeof(*pts));
  pts[0].sx = 10;
  pts[0].sy = 10;
  pts[0].valid = TRUE;
  TKMEM_FREE(plot3d->projected_points);
  plot3d->projected_points = pts;
  plot3d->projected_points_nr = 1;
  ASSERT_EQ(RET_OK, plot3d_set_show_datatip(w, FALSE));
  ASSERT_EQ(RET_OK, plot3d_datatip_update_hover(w, 10, 10));
  ASSERT_EQ(-1, plot3d->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, SanitizeHoverClearsInvalidProjectedPoint) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  plot3d_t* plot3d = PLOT3D(w);
  plot3d_projected_point_t* pts =
      (plot3d_projected_point_t*)TKMEM_ALLOC(sizeof(plot3d_projected_point_t));
  pts[0] = make_pt(10, 10, TRUE);
  TKMEM_FREE(plot3d->projected_points);
  plot3d->projected_points = pts;
  plot3d->projected_points_nr = 1;
  plot3d->hover_index = 0;

  pts[0].valid = FALSE;
  plot3d_datatip_sanitize_hover(w);
  ASSERT_EQ(-1, plot3d->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, ProjectionRebuildClearsOutOfRangeHover) {
  widget_t* w = plot3d_create(NULL, 0, 0, 200, 200);
  plot3d_t* plot3d = PLOT3D(w);
  plot3d_bounds_t bounds;
  plot3d->hover_index = 3;

  ASSERT_EQ(RET_OK, plot3d_calc_bounds(plot3d, &bounds));
  ASSERT_EQ(RET_OK, plot3d_update_projection_cache(w, &bounds));
  ASSERT_EQ(0, plot3d->projected_points_nr);
  ASSERT_EQ(-1, plot3d->hover_index);
  widget_destroy(w);
}

TEST(Plot3dDatatip, LabelPosCenterUsesUpperRight) {
  float_t x = 0;
  float_t y = 0;

  plot3d_datatip_compute_label_pos(100, 100, 80, 60, 0, 0, 300, 300, &x, &y);

  ASSERT_FLOAT_EQ(112, x);
  ASSERT_FLOAT_EQ(32, y);
}

TEST(Plot3dDatatip, LabelPosNearRightEdgeFlipsLeft) {
  float_t x = 0;
  float_t y = 0;

  plot3d_datatip_compute_label_pos(280, 100, 80, 60, 0, 0, 300, 300, &x, &y);

  ASSERT_FLOAT_EQ(188, x);
  ASSERT_LE(x + 80, 300);
}

TEST(Plot3dDatatip, LabelPosNearTopEdgeFlipsBelow) {
  float_t x = 0;
  float_t y = 0;

  plot3d_datatip_compute_label_pos(100, 20, 80, 60, 0, 0, 300, 300, &x, &y);

  ASSERT_FLOAT_EQ(28, y);
  ASSERT_GE(y, 0);
}
