#include <math.h>

#include "awtk.h"
#include "tkc/data_reader.h"
#include "base/data_reader_asset.h"
#include "plot3d/plot3d.h"
#include "plot3d_register.h"

static ret_t demo_csv_min_load_dataset(widget_t* chart, const char* name) {
  char url[MAX_PATH + 1];
  char asset_name[TK_NAME_LEN + 1];
  uint32_t size = 0;
  char* data = NULL;
  ret_t ret = RET_FAIL;

  return_value_if_fail(chart != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(asset_name, sizeof(asset_name), "%s.csv", name);
  return_value_if_fail(data_reader_asset_build_url(asset_name, ASSET_TYPE_DATA, url) != NULL,
                       RET_FAIL);

  data = (char*)data_reader_read_all(url, &size);
  return_value_if_fail(data != NULL, RET_FAIL);

  ret = plot3d_set_dataset(chart, data);
  TKMEM_FREE(data);

  return ret;
}

ret_t application_init(void) {
  widget_t* win = NULL;
  widget_t* chart = NULL;

  plot3d_register_all();
  win = window_open("demo_csv_min");
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

  plot3d_set_plottype(chart, "surface");
  plot3d_set_xlabel(chart, "X");
  plot3d_set_ylabel(chart, "Y");
  plot3d_set_zlabel(chart, "Z");
  plot3d_set_x_grid_count(chart, 8);
  plot3d_set_y_grid_count(chart, 7);
  plot3d_set_z_grid_count(chart, 4);
  plot3d_set_box_aspect_x(chart, 1.0f);
  plot3d_set_box_aspect_y(chart, 1.0f);
  plot3d_set_box_aspect_z(chart, 0.7f);
  plot3d_set_point_size(chart, 4.0f);
  plot3d_set_line_width(chart, 1.0f);

  return demo_csv_min_load_dataset(chart, "sample_sin_surface");
}

ret_t application_exit(void) {
  log_debug("application_exit\n");
  return RET_OK;
}
