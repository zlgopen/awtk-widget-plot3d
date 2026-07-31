#include "awtk.h"
#include "plot3d/plot3d.h"
#include "plot3d_register.h"

ret_t application_init(void) {
  widget_t* win = NULL;
  widget_t* chart = NULL;

  plot3d_register_all();
  win = window_open("demo_matrix_min");
  return_value_if_fail(win != NULL, RET_FAIL);
  chart = widget_lookup(win, "chart", TRUE);
  return_value_if_fail(chart != NULL, RET_FAIL);

  plot3d_set_show_grid(chart, TRUE);
  plot3d_set_show_axis_tick(chart, TRUE);
  plot3d_set_grid_color(chart, "#66666688");
  plot3d_set_xaxis_color(chart, "#e74c3cff");
  plot3d_set_yaxis_color(chart, "#27ae60ff");
  plot3d_set_zaxis_color(chart, "#2980b9ff");
  plot3d_set_tick_color(chart, "#888888ff");
  plot3d_set_axis_negative_brightness(chart, 0.5f);
  plot3d_set_enable_cache(chart, TRUE);
  plot3d_set_camera_yaw(chart, -0.75f);
  plot3d_set_camera_pitch(chart, 0.35f);
  plot3d_set_camera_distance(chart, 4.5f);
  plot3d_set_camera_z_offset(chart, 0.0f);

  plot3d_set_plottype(chart, "cylinder");
  plot3d_set_xlabel(chart, "COL");
  plot3d_set_ylabel(chart, "ROW");
  plot3d_set_zlabel(chart, "V");
  plot3d_set_x_grid_count(chart, 6);
  plot3d_set_y_grid_count(chart, 6);
  plot3d_set_z_grid_count(chart, 5);
  plot3d_set_box_aspect_x(chart, 1.0f);
  plot3d_set_box_aspect_y(chart, 1.0f);
  plot3d_set_box_aspect_z(chart, 0.6f);
  plot3d_set_point_size(chart, 4.0f);
  plot3d_set_line_width(chart, 14.0f);

  plot3d_set_colormap(chart, "jet");
  plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_MATRIX);

  return plot3d_set_sample_z_matrix(
      chart, "0,1,2,3,2,1;1,3,5,6,5,3;2,5,8,9,8,5;3,6,9,9,6,3;2,5,8,9,8,5;1,3,5,6,5,3");
}

ret_t application_exit(void) {
  log_debug("application_exit\n");
  return RET_OK;
}
