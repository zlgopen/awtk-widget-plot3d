#include <math.h>

#include "plot3d/plot3d.h"
#include "gtest/gtest.h"
#include "tkc/color_parser.h"
#include "awtk.h"
#include "tkc/fs.h"

/* 样例数据体检：声明每份数据的点数、包围盒与最小跨度，防止数据文件写坏。 */
typedef struct _plot3d_sample_spec_t {
  const char* name;
  uint32_t points_nr;
  float_t min_x;
  float_t max_x;
  float_t min_y;
  float_t max_y;
  float_t min_z;
  float_t max_z;
  float_t least_extent_x;
  float_t least_extent_y;
  float_t least_extent_z;
} plot3d_sample_spec_t;

static const plot3d_sample_spec_t s_sample_specs[] = {
    /* sin 行进波：x 每 15 度一个点共 25 个，8 条 y，z=sin(x+0.5y) */
    {"sample_sin_dot", 200, 0, 360, 0, 7, -1, 1, 360, 7, 1.98f},
    {"sample_sin_line", 200, 0, 360, 0, 7, -1, 1, 360, 7, 1.98f},
    {"sample_sin_cylinder", 200, 0, 360, 0, 7, -1, 1, 360, 7, 1.98f},
    /* surface 是三角形列表：6 * (8-1) * (25-1) 个顶点 */
    {"sample_sin_surface", 1008, 0, 360, 0, 7, -1, 1, 360, 7, 1.98f},
    /* peaks：19x19 网格的三角形列表 */
    {"sample_peaks", 1944, -3, 3, -3, 3, -9, 9, 6, 6, 12},
    /* 墨西哥帽 sinc：19x19 网格的三角形列表 */
    {"sample_sombrero", 1944, -8, 8, -8, 8, -0.3f, 1.01f, 16, 16, 1.1f},
    /* 洛伦兹：去掉瞬态后的轨迹，z 不再从 0 起 */
    {"sample_lorenz", 2000, -30, 30, -35, 35, 0, 60, 30, 35, 30},
    /* 双螺旋：两条 121 点的链 + 13 根横档 */
    {"sample_helix", 268, -1, 1, -1, 1, 0, 3, 1.9f, 1.9f, 2.9f},
    {"sample_sphere", 300, -1, 1, -1, 1, -1, 1, 1.9f, 1.9f, 1.9f},
    /* 5x5 柱阵 + 4 个零高度锚点 */
    {"sample_bars", 29, 1, 5, 1, 5, 0, 8, 4, 4, 7.9f},
    {"sample_trefoil", 241, -3, 3, -3, 3, -1, 1, 5, 5, 1.9f}};

/* 直接读设计目录下的源文件：它既是打包资源的输入，也是生成脚本的输出。 */
static char* plot3d_test_read_sample(const char* name) {
  char path[MAX_PATH + 1];
  uint32_t size = 0;

  tk_snprintf(path, sizeof(path), "design/default/data/%s.csv", name);

  return (char*)file_read(path, &size);
}

TEST(plot3d, sample_datasets) {
  uint32_t i = 0;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  for (i = 0; i < ARRAY_SIZE(s_sample_specs); i++) {
    const plot3d_sample_spec_t* spec = s_sample_specs + i;
    plot3d_data_point_t point;
    uint32_t nr = 0;
    uint32_t k = 0;
    uint32_t valid_nr = 0;
    float_t min_x = 0;
    float_t max_x = 0;
    float_t min_y = 0;
    float_t max_y = 0;
    float_t min_z = 0;
    float_t max_z = 0;
    char* data = plot3d_test_read_sample(spec->name);

    ASSERT_TRUE(data != NULL) << spec->name;
    ASSERT_EQ(RET_OK, plot3d_set_dataset(w, data)) << spec->name;
    TKMEM_FREE(data);

    nr = plot3d_get_data_points_nr(w);
    for (k = 0; k < nr; k++) {
      ASSERT_EQ(RET_OK, plot3d_get_data_point(w, k, &point)) << spec->name;
      if (point.is_break) {
        continue;
      }

      if (valid_nr == 0) {
        min_x = max_x = point.x;
        min_y = max_y = point.y;
        min_z = max_z = point.z;
      } else {
        min_x = tk_min(min_x, point.x);
        max_x = tk_max(max_x, point.x);
        min_y = tk_min(min_y, point.y);
        max_y = tk_max(max_y, point.y);
        min_z = tk_min(min_z, point.z);
        max_z = tk_max(max_z, point.z);
      }
      valid_nr++;
    }

    EXPECT_EQ(spec->points_nr, valid_nr) << spec->name;
    EXPECT_GE(min_x, spec->min_x - 0.001f) << spec->name;
    EXPECT_LE(max_x, spec->max_x + 0.001f) << spec->name;
    EXPECT_GE(min_y, spec->min_y - 0.001f) << spec->name;
    EXPECT_LE(max_y, spec->max_y + 0.001f) << spec->name;
    EXPECT_GE(min_z, spec->min_z - 0.001f) << spec->name;
    EXPECT_LE(max_z, spec->max_z + 0.001f) << spec->name;
    EXPECT_GE(max_x - min_x, spec->least_extent_x) << spec->name;
    EXPECT_GE(max_y - min_y, spec->least_extent_y) << spec->name;
    EXPECT_GE(max_z - min_z, spec->least_extent_z) << spec->name;
  }

  widget_destroy(w);
}

TEST(plot3d, basic) {
  widget_t* w = plot3d_create(NULL, 10, 20, 30, 40);

  widget_destroy(w);
}

TEST(plot3d, append_and_get_data_points) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 10, 20, 300, 240);

  ASSERT_EQ(RET_OK, plot3d_reset_data(w));
  ASSERT_EQ(RET_OK, plot3d_append_data_point(w, 1.5f, 2.5f, 3.5f, "#112233"));
  ASSERT_EQ(RET_OK, plot3d_append_break_data_point(w));
  ASSERT_EQ(RET_OK, plot3d_append_data_point(w, -1.0f, -2.0f, 4.0f, "red"));
  ASSERT_EQ(3u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(1.5f, point.x, 0.001f);
  ASSERT_NEAR(2.5f, point.y, 0.001f);
  ASSERT_NEAR(3.5f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_TRUE(point.is_break);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(-1.0f, point.x, 0.001f);
  ASSERT_NEAR(-2.0f, point.y, 0.001f);
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, dataset_import_with_break_line) {
  plot3d_data_point_t point;
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);
  const char* dataset = "1,2,3,#112233\n\n4,5,6,rgba(10,20,30,0.5)\n";

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, dataset));
  ASSERT_EQ(3u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_TRUE(point.is_break);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_FALSE(point.is_break);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_DATASET, &v));
  ASSERT_STREQ(dataset, value_str(&v));
  value_reset(&v);

  widget_destroy(w);
}

/* csv 的点一旦落地就归用户所有：不相干的重采样触发（改配色表、改图型）不得覆盖它们。
 * 同时直接钉住布尔量本身——只看点数会被「TRUE + 重灌同一段 csv」假阳性蒙混。 */
TEST(plot3d, dataset_points_survive_unrelated_resample) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(FALSE, PLOT3D(w)->sample_from_func);

  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "jet"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(FALSE, PLOT3D(w)->sample_from_func);
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(FALSE, PLOT3D(w)->sample_from_func);

  /* 坐标与 csv 第四段显式给的颜色都不变。 */
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_NEAR(4.0f, point.x, 0.001f);
  ASSERT_NEAR(5.0f, point.y, 0.001f);
  ASSERT_NEAR(6.0f, point.z, 0.001f);
  ASSERT_EQ(0x44, point.color.rgba.r);
  ASSERT_EQ(0x55, point.color.rgba.g);
  ASSERT_EQ(0x66, point.color.rgba.b);

  widget_destroy(w);
}

/* 清空数据之后再触发重采样，csv 的点不会被重新灌回来。 */
TEST(plot3d, reset_data_then_resample_keeps_empty) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_reset_data(w));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "jet"));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

/* 在 csv 数据之上手工追加的点，不会被后续重采样冲掉。 */
TEST(plot3d, manual_point_after_dataset_survives_resample) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(RET_OK, plot3d_append_data_point(w, 7.0f, 8.0f, 9.0f, "#778899"));
  ASSERT_EQ(RET_OK, plot3d_append_break_data_point(w));
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "jet"));
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(7.0f, point.x, 0.001f);
  ASSERT_NEAR(8.0f, point.y, 0.001f);
  ASSERT_NEAR(9.0f, point.z, 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_TRUE(point.is_break);

  widget_destroy(w);
}

TEST(plot3d, set_extended_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "surface"));
  ASSERT_EQ(RET_OK, plot3d_set_xlabel(w, "X"));
  ASSERT_EQ(RET_OK, plot3d_set_ylabel(w, "Y"));
  ASSERT_EQ(RET_OK, plot3d_set_zlabel(w, "Z"));
  ASSERT_EQ(RET_OK, plot3d_set_show_grid(w, TRUE));
  ASSERT_EQ(RET_OK, plot3d_set_show_axis_tick(w, TRUE));
  ASSERT_EQ(RET_OK, plot3d_set_grid_color(w, "#123456"));
  ASSERT_EQ(RET_OK, plot3d_set_xaxis_color(w, "#654321ff"));
  ASSERT_EQ(RET_OK, plot3d_set_tick_color(w, "#654321ff"));
  ASSERT_EQ(RET_OK, plot3d_set_point_size(w, 6.0f));
  ASSERT_EQ(RET_OK, plot3d_set_line_width(w, 2.0f));
  ASSERT_EQ(RET_OK, plot3d_set_enable_cache(w, TRUE));
  ASSERT_EQ(RET_OK, plot3d_set_camera_yaw(w, 0.3f));
  ASSERT_EQ(RET_OK, plot3d_set_camera_pitch(w, 0.2f));
  ASSERT_EQ(RET_OK, plot3d_set_camera_distance(w, 5.5f));
  ASSERT_EQ(RET_OK, plot3d_set_camera_z_offset(w, 1.2f));

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_PLOTTYPE, &v));
  ASSERT_STREQ("surface", value_str(&v));
  value_reset(&v);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XLABEL, &v));
  ASSERT_STREQ("X", value_str(&v));
  value_reset(&v);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_ENABLE_CACHE, &v));
  ASSERT_EQ(TRUE, value_bool(&v));
  value_reset(&v);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_CAMERA_DISTANCE, &v));
  ASSERT_NEAR(5.5f, value_float32(&v), 0.001f);
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, grid_position_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(0.0f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XZ_GRID_POSITION, &v));
  ASSERT_NEAR(0.0f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_YZ_GRID_POSITION, &v));
  ASSERT_NEAR(0.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_xy_grid_position(w, 1.5f));
  ASSERT_EQ(RET_OK, plot3d_set_xz_grid_position(w, -0.5f));
  ASSERT_EQ(RET_OK, plot3d_set_yz_grid_position(w, 2.0f));

  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(1.5f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XZ_GRID_POSITION, &v));
  ASSERT_NEAR(-0.5f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_YZ_GRID_POSITION, &v));
  ASSERT_NEAR(2.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_float(w, PLOT3D_PROP_XY_GRID_POSITION, 3.0f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(3.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_float(w, PLOT3D_PROP_XZ_GRID_POSITION, 4.0f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XZ_GRID_POSITION, &v));
  ASSERT_NEAR(4.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_float(w, PLOT3D_PROP_YZ_GRID_POSITION, 5.0f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_YZ_GRID_POSITION, &v));
  ASSERT_NEAR(5.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, grid_count_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_X_GRID_COUNT, &v));
  ASSERT_EQ(5u, (uint32_t)value_uint32(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_Y_GRID_COUNT, &v));
  ASSERT_EQ(5u, (uint32_t)value_uint32(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_Z_GRID_COUNT, &v));
  ASSERT_EQ(5u, (uint32_t)value_uint32(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_x_grid_count(w, 10));
  ASSERT_EQ(RET_OK, plot3d_set_y_grid_count(w, 8));
  ASSERT_EQ(RET_OK, plot3d_set_z_grid_count(w, 3));

  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_X_GRID_COUNT, &v));
  ASSERT_EQ(10u, (uint32_t)value_uint32(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_x_grid_count(w, 0));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_X_GRID_COUNT, &v));
  ASSERT_EQ(1u, (uint32_t)value_uint32(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_y_grid_count(w, 100));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_Y_GRID_COUNT, &v));
  ASSERT_EQ(50u, (uint32_t)value_uint32(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_int(w, PLOT3D_PROP_Z_GRID_COUNT, 7));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_Z_GRID_COUNT, &v));
  ASSERT_EQ(7u, (uint32_t)value_uint32(&v));
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, step_grid_position) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_reset_data(w));
  ASSERT_EQ(RET_OK, plot3d_append_data_point(w, 0, 0, 0, "#ffffff"));
  ASSERT_EQ(RET_OK, plot3d_append_data_point(w, 10, 10, 10, "#ffffff"));
  ASSERT_EQ(RET_OK, plot3d_set_z_grid_count(w, 5));
  ASSERT_EQ(RET_OK, plot3d_set_xy_grid_position(w, 0.0f));

  ASSERT_EQ(RET_OK, plot3d_step_xy_grid_position(w, 1));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(2.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_xy_grid_position(w, 3.0f)); /* off-grid */
  ASSERT_EQ(RET_OK, plot3d_step_xy_grid_position(w, 1));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(4.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_xy_grid_position(w, 3.0f));
  ASSERT_EQ(RET_OK, plot3d_step_xy_grid_position(w, -1));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(2.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_xy_grid_position(w, 10.0f));
  ASSERT_EQ(RET_OK, plot3d_step_xy_grid_position(w, 1));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XY_GRID_POSITION, &v));
  ASSERT_NEAR(10.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_step_xy_grid_position(w, 0));

  /* smoke xz/yz once */
  ASSERT_EQ(RET_OK, plot3d_set_y_grid_count(w, 5));
  ASSERT_EQ(RET_OK, plot3d_set_xz_grid_position(w, 0.0f));
  ASSERT_EQ(RET_OK, plot3d_step_xz_grid_position(w, 1));
  ASSERT_EQ(RET_OK, plot3d_set_x_grid_count(w, 5));
  ASSERT_EQ(RET_OK, plot3d_set_yz_grid_position(w, 0.0f));
  ASSERT_EQ(RET_OK, plot3d_step_yz_grid_position(w, 1));

  widget_destroy(w);
}

TEST(plot3d, pick_axis_text_normal) {
  float_t nx = 0, ny = 0;
  /* axis dir (1,0), equal dots → pick n1 */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_normal(1, 0, 20, 10, 0, 10, &nx, &ny));
  /* r=(20,0); n1=(0,1) n2=(0,-1); both dot r = 0 → pick n1=(0,1) */
  ASSERT_NEAR(0.0f, nx, 0.001f);
  ASSERT_NEAR(1.0f, ny, 0.001f);

  /* |r| large, r points up → outward n1=(0,1) wins */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_normal(1, 0, 0, 30, 0, 10, &nx, &ny));
  ASSERT_NEAR(0.0f, nx, 0.001f);
  ASSERT_NEAR(1.0f, ny, 0.001f);

  /* |r| large, r points down → outward n2=(0,-1) wins */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_normal(1, 0, 0, -10, 0, 10, &nx, &ny));
  ASSERT_NEAR(0.0f, nx, 0.001f);
  ASSERT_NEAR(-1.0f, ny, 0.001f);

  /* |r| < 1 → fixed n1 for dir (1,0) */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_normal(1, 0, 0.1f, 0.1f, 0, 0, &nx, &ny));
  ASSERT_NEAR(0.0f, nx, 0.001f);
  ASSERT_NEAR(1.0f, ny, 0.001f);
}

TEST(plot3d, axis_color_segments) {
  plot3d_axis_color_segment_t segs[2];
  color_t pos = color_parse("#ff0000ff");
  uint32_t n = 0;

  /* 跨 0：两段 */
  n = plot3d_build_axis_color_segments(-2.0f, 3.0f, pos, 0.5f, segs);
  ASSERT_EQ(2u, n);
  ASSERT_NEAR(-2.0f, segs[0].t0, 0.001f);
  ASSERT_NEAR(0.0f, segs[0].t1, 0.001f);
  ASSERT_EQ((uint8_t)(0xff * 0.5f), segs[0].color.rgba.r); /* modulate 下限未触发 */
  ASSERT_NEAR(0.0f, segs[1].t0, 0.001f);
  ASSERT_NEAR(3.0f, segs[1].t1, 0.001f);
  ASSERT_EQ(0xff, segs[1].color.rgba.r);

  /* 全正 */
  n = plot3d_build_axis_color_segments(1.0f, 4.0f, pos, 0.5f, segs);
  ASSERT_EQ(1u, n);
  ASSERT_NEAR(1.0f, segs[0].t0, 0.001f);
  ASSERT_NEAR(4.0f, segs[0].t1, 0.001f);
  ASSERT_EQ(0xff, segs[0].color.rgba.r);

  /* 全负 */
  n = plot3d_build_axis_color_segments(-5.0f, -1.0f, pos, 0.5f, segs);
  ASSERT_EQ(1u, n);
  ASSERT_NEAR(-5.0f, segs[0].t0, 0.001f);
  ASSERT_NEAR(-1.0f, segs[0].t1, 0.001f);
  ASSERT_EQ((uint8_t)(0xff * 0.5f), segs[0].color.rgba.r);
}

TEST(plot3d, pick_axis_text_edge) {
  plot3d_axis_text_edge_t edge;
  const float_t pi = 3.1415927f;

  /* yaw=0：X 刻度文字贴底面上离观察者最近的棱（y=min_y, z=min_z） */
  ASSERT_EQ(RET_OK,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_X, 0.0f, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(-1.0f, edge.x0, 0.001f);
  ASSERT_NEAR(2.0f, edge.x1, 0.001f);
  ASSERT_NEAR(-3.0f, edge.y0, 0.001f);
  ASSERT_NEAR(-3.0f, edge.y1, 0.001f);
  ASSERT_NEAR(-5.0f, edge.z0, 0.001f);
  ASSERT_NEAR(-5.0f, edge.z1, 0.001f);

  /* yaw=pi：观察者转到另一侧，X 棱翻到 y=max_y */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_edge(PLOT3D_AXIS_X, pi, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(4.0f, edge.y0, 0.001f);
  ASSERT_NEAR(4.0f, edge.y1, 0.001f);

  /* yaw=pi/2：Y 刻度文字贴 x=min_x 的底面棱 */
  ASSERT_EQ(RET_OK,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_Y, pi / 2, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(-1.0f, edge.x0, 0.001f);
  ASSERT_NEAR(-1.0f, edge.x1, 0.001f);
  ASSERT_NEAR(-3.0f, edge.y0, 0.001f);
  ASSERT_NEAR(4.0f, edge.y1, 0.001f);
  ASSERT_NEAR(-5.0f, edge.z0, 0.001f);

  /* yaw=-pi/2：Y 棱翻到 x=max_x */
  ASSERT_EQ(RET_OK,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_Y, -pi / 2, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(2.0f, edge.x0, 0.001f);

  /* 深度相同时固定取 min 端，保证旋转到 90 度边界不抖动 */
  ASSERT_EQ(RET_OK,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_Y, 0.0f, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(-1.0f, edge.x0, 0.001f);

  /* Z 刻度文字贴屏幕最左侧的竖棱：yaw=pi/4 → (min_x, max_y) */
  ASSERT_EQ(RET_OK,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_Z, pi / 4, -1, 2, -3, 4, -5, 6, &edge));
  ASSERT_NEAR(-1.0f, edge.x0, 0.001f);
  ASSERT_NEAR(4.0f, edge.y0, 0.001f);
  ASSERT_NEAR(-5.0f, edge.z0, 0.001f);
  ASSERT_NEAR(-1.0f, edge.x1, 0.001f);
  ASSERT_NEAR(4.0f, edge.y1, 0.001f);
  ASSERT_NEAR(6.0f, edge.z1, 0.001f);

  /* yaw=-3pi/4 → (max_x, min_y) */
  ASSERT_EQ(RET_OK, plot3d_pick_axis_text_edge(PLOT3D_AXIS_Z, -3 * pi / 4, -1, 2, -3, 4, -5, 6,
                                                &edge));
  ASSERT_NEAR(2.0f, edge.x0, 0.001f);
  ASSERT_NEAR(-3.0f, edge.y0, 0.001f);

  ASSERT_EQ(RET_BAD_PARAMS,
            plot3d_pick_axis_text_edge(PLOT3D_AXIS_X, 0.0f, -1, 2, -3, 4, -5, 6, NULL));
}

TEST(plot3d, calc_axis_tick_offset) {
  float_t offset = 0;

  /* 竖棱：法线水平，偏移要让开文字宽度的一半，否则文字会压到棱和网格上 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(14.0f, -1.0f, 0.0f, 30.0f, &offset));
  ASSERT_NEAR(20.6f, offset, 0.001f);

  /* 底面棱：法线竖直，只需让开半行高度 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(14.0f, 0.0f, 1.0f, 30.0f, &offset));
  ASSERT_NEAR(12.6f, offset, 0.001f);

  /* 斜棱：按文字包围盒在法线方向的投影取半径 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(14.0f, 0.6f, -0.8f, 30.0f, &offset));
  ASSERT_NEAR(20.2f, offset, 0.001f);

  /* 小字号的间隙有下限 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(6.0f, 0.0f, 1.0f, 0.0f, &offset));
  ASSERT_NEAR(7.0f, offset, 0.001f);

  /* 字号非法时回退到默认字号 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(0.0f, 0.0f, 1.0f, 30.0f, &offset));
  ASSERT_NEAR(12.6f, offset, 0.001f);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_calc_axis_tick_offset(14.0f, -1.0f, 0.0f, 30.0f, NULL));
}

TEST(plot3d, calc_axis_name_offset) {
  float_t offset = 0;

  /* 竖棱：轴名要整体让开刻度文字的宽度 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_name_offset(14.0f, -1.0f, 0.0f, 30.0f, 10.0f, &offset));
  ASSERT_NEAR(46.2f, offset, 0.001f);

  /* 底面棱：让开一整行刻度文字 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_name_offset(14.0f, 0.0f, 1.0f, 30.0f, 10.0f, &offset));
  ASSERT_NEAR(32.2f, offset, 0.001f);

  /* 轴名始终比同方向的刻度文字更靠外 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_name_offset(14.0f, 0.6f, -0.8f, 30.0f, 10.0f, &offset));
  float_t tick_offset = 0;
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_offset(14.0f, 0.6f, -0.8f, 30.0f, &tick_offset));
  ASSERT_TRUE(offset > tick_offset);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_calc_axis_name_offset(14.0f, -1.0f, 0.0f, 30.0f, 10.0f, NULL));
}

TEST(plot3d, calc_axis_tick_range) {
  uint32_t begin = 0;
  uint32_t end = 0;
  const float_t pi = 3.1415927f;

  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_range(PLOT3D_AXIS_X, 0.0f, 5, &begin, &end));
  ASSERT_EQ(0u, begin);
  ASSERT_EQ(5u, end);

  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_range(PLOT3D_AXIS_Z, 0.0f, 5, &begin, &end));
  ASSERT_EQ(0u, begin);
  ASSERT_EQ(5u, end);

  /* yaw=0 时 X 棱在 y=min_y，Y 棱要跳过和它共角的首个刻度 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_range(PLOT3D_AXIS_Y, 0.0f, 5, &begin, &end));
  ASSERT_EQ(1u, begin);
  ASSERT_EQ(5u, end);

  /* yaw=pi 时 X 棱在 y=max_y，改跳最后一个刻度 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_range(PLOT3D_AXIS_Y, pi, 5, &begin, &end));
  ASSERT_EQ(0u, begin);
  ASSERT_EQ(4u, end);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_calc_axis_tick_range(PLOT3D_AXIS_Y, 0.0f, 5, NULL, &end));
}

TEST(plot3d, calc_axis_scales) {
  float_t fx = 0;
  float_t fy = 0;
  float_t fz = 0;

  /* 默认各轴独立填满立方体：范围小的轴也被拉满，网格不会挤成一团 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(FALSE, 10.0f, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f, &fx, &fy, &fz));
  ASSERT_NEAR(0.1f, fx, 0.0001f);
  ASSERT_NEAR(0.2f, fy, 0.0001f);
  ASSERT_NEAR(1.0f, fz, 0.0001f);

  /* 盒子边长系数只改形状：按最大值归一化，2,2,1 等价于 1,1,0.5 */
  float_t fit = sqrtf(3.0f) / sqrtf(1.0f + 1.0f + 0.25f);
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(FALSE, 10.0f, 5.0f, 1.0f, 2.0f, 2.0f, 1.0f, &fx, &fy, &fz));
  ASSERT_NEAR(0.1f * fit, fx, 0.0001f);
  ASSERT_NEAR(0.2f * fit, fy, 0.0001f);
  ASSERT_NEAR(0.5f * fit, fz, 0.0001f);

  /* 自动缩放：变形后的盒子包围球与立方体一致，仍然占满可视区 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(FALSE, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.25f, &fx, &fy, &fz));
  ASSERT_NEAR(3.0f, fx * fx + fy * fy + fz * fz, 0.001f);
  ASSERT_TRUE(fx > fy && fy > fz);

  /* 非法系数按 1 处理 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(FALSE, 10.0f, 5.0f, 1.0f, 0.0f, -1.0f, 1.0f, &fx, &fy, &fz));
  ASSERT_NEAR(0.1f, fx, 0.0001f);
  ASSERT_NEAR(0.2f, fy, 0.0001f);
  ASSERT_NEAR(1.0f, fz, 0.0001f);

  /* equal_axis：三轴共用最长范围，保持几何比例，忽略盒子系数 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(TRUE, 10.0f, 5.0f, 1.0f, 1.0f, 1.0f, 0.5f, &fx, &fy, &fz));
  ASSERT_NEAR(0.1f, fx, 0.0001f);
  ASSERT_NEAR(0.1f, fy, 0.0001f);
  ASSERT_NEAR(0.1f, fz, 0.0001f);

  /* 退化范围不能除零 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_axis_scales(FALSE, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, &fx, &fy, &fz));
  ASSERT_TRUE(fx > 0.0f && fy > 0.0f && fz > 0.0f);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_calc_axis_scales(FALSE, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                                     NULL, &fy, &fz));
}

TEST(plot3d, calc_axis_tick_stride) {
  uint32_t stride = 0;

  /* 横棱足够长：每个刻度都画得下 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(400.0f, 5, 1.0f, 0.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(1u, stride);

  /* 横棱变短：按字宽抽稀 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(100.0f, 5, 1.0f, 0.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(2u, stride);

  /* 竖棱被压扁：按行高抽稀，并优先取能整除的步长，保证两端都有标签 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(60.0f, 5, 0.0f, 1.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(5u, stride);

  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(60.0f, 10, 0.0f, 1.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(5u, stride);

  /* 极端拥挤时最多退到只画两端 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(10.0f, 5, 1.0f, 0.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(5u, stride);

  /* 退化输入不抽稀 */
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(0.0f, 5, 1.0f, 0.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(1u, stride);
  ASSERT_EQ(RET_OK, plot3d_calc_axis_tick_stride(100.0f, 0, 1.0f, 0.0f, 30.0f, 18.0f, &stride));
  ASSERT_EQ(1u, stride);

  ASSERT_EQ(RET_BAD_PARAMS,
            plot3d_calc_axis_tick_stride(100.0f, 5, 1.0f, 0.0f, 30.0f, 18.0f, NULL));
}

TEST(plot3d, calc_nice_axis) {
  float_t nice_min = 0;
  float_t nice_max = 0;
  uint32_t count = 0;
  uint32_t decimals = 0;

  /* 步长取 2，范围本身已经整齐 */
  ASSERT_EQ(RET_OK, plot3d_calc_nice_axis(0.0f, 10.0f, 5, &nice_min, &nice_max, &count, &decimals));
  ASSERT_NEAR(0.0f, nice_min, 0.001f);
  ASSERT_NEAR(10.0f, nice_max, 0.001f);
  ASSERT_EQ(5u, count);
  ASSERT_EQ(0u, decimals);

  /* 小范围：步长取 0.5，范围向外扩到步长的整数倍，小数位由步长决定 */
  ASSERT_EQ(RET_OK, plot3d_calc_nice_axis(0.4f, 1.6f, 5, &nice_min, &nice_max, &count, &decimals));
  ASSERT_NEAR(0.0f, nice_min, 0.001f);
  ASSERT_NEAR(2.0f, nice_max, 0.001f);
  ASSERT_EQ(4u, count);
  ASSERT_EQ(1u, decimals);

  /* 扩范围之后格数仍不超过上限 */
  ASSERT_EQ(RET_OK, plot3d_calc_nice_axis(0.2f, 9.9f, 5, &nice_min, &nice_max, &count, &decimals));
  ASSERT_NEAR(0.0f, nice_min, 0.001f);
  ASSERT_NEAR(10.0f, nice_max, 0.001f);
  ASSERT_EQ(5u, count);
  ASSERT_EQ(0u, decimals);

  /* 跨 0 的范围两端都向外取整 */
  ASSERT_EQ(RET_OK,
            plot3d_calc_nice_axis(-1.0f, 1.2f, 5, &nice_min, &nice_max, &count, &decimals));
  ASSERT_NEAR(-1.0f, nice_min, 0.001f);
  ASSERT_NEAR(1.5f, nice_max, 0.001f);
  ASSERT_EQ(5u, count);
  ASSERT_EQ(1u, decimals);

  /* 退化范围也要给出可用的刻度 */
  ASSERT_EQ(RET_OK, plot3d_calc_nice_axis(5.0f, 5.0f, 5, &nice_min, &nice_max, &count, &decimals));
  ASSERT_TRUE(nice_max > nice_min);
  ASSERT_TRUE(count >= 1u);
  ASSERT_TRUE(nice_min <= 5.0f && nice_max >= 5.0f);

  /* 上限为 0 按 1 处理 */
  ASSERT_EQ(RET_OK, plot3d_calc_nice_axis(0.0f, 10.0f, 0, &nice_min, &nice_max, &count, &decimals));
  ASSERT_EQ(1u, count);

  ASSERT_EQ(RET_BAD_PARAMS,
            plot3d_calc_nice_axis(0.0f, 1.0f, 5, NULL, &nice_max, &count, &decimals));
}

TEST(plot3d, box_aspect_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_X, &v));
  ASSERT_NEAR(1.0f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_Y, &v));
  ASSERT_NEAR(1.0f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_Z, &v));
  ASSERT_NEAR(1.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_box_aspect_z(w, 0.5f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_Z, &v));
  ASSERT_NEAR(0.5f, value_float32(&v), 0.001f);
  value_reset(&v);

  /* 超出范围钳位，避免盒子退化成一条线或撑爆控件 */
  ASSERT_EQ(RET_OK, plot3d_set_box_aspect_x(w, 0.0f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_X, &v));
  ASSERT_NEAR(0.05f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_box_aspect_y(w, 100.0f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_Y, &v));
  ASSERT_NEAR(20.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_float(w, PLOT3D_PROP_BOX_ASPECT_Z, 0.8f));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_BOX_ASPECT_Z, &v));
  ASSERT_NEAR(0.8f, value_float32(&v), 0.001f);
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, equal_axis_prop) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_EQUAL_AXIS, &v));
  ASSERT_EQ(FALSE, value_bool(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_equal_axis(w, TRUE));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_EQUAL_AXIS, &v));
  ASSERT_EQ(TRUE, value_bool(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_bool(w, PLOT3D_PROP_EQUAL_AXIS, FALSE));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_EQUAL_AXIS, &v));
  ASSERT_EQ(FALSE, value_bool(&v));
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, per_axis_color_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XAXIS_COLOR, &v));
  ASSERT_STREQ("#e74c3cff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_YAXIS_COLOR, &v));
  ASSERT_STREQ("#27ae60ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_ZAXIS_COLOR, &v));
  ASSERT_STREQ("#2980b9ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_TICK_COLOR, &v));
  ASSERT_STREQ("#888888ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS, &v));
  ASSERT_NEAR(0.5f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, plot3d_set_xaxis_color(w, "#112233ff"));
  ASSERT_EQ(RET_OK, plot3d_set_yaxis_color(w, "#445566ff"));
  ASSERT_EQ(RET_OK, plot3d_set_zaxis_color(w, "#778899ff"));
  ASSERT_EQ(RET_OK, plot3d_set_tick_color(w, "#aabbccff"));
  ASSERT_EQ(RET_OK, plot3d_set_axis_negative_brightness(w, 0.25f));
  ASSERT_EQ(RET_OK, plot3d_set_axis_negative_brightness(w, 2.0f)); /* clamp → 1 */
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS, &v));
  ASSERT_NEAR(1.0f, value_float32(&v), 0.001f);
  value_reset(&v);
  ASSERT_EQ(RET_OK, plot3d_set_axis_negative_brightness(w, -1.0f)); /* clamp → 0 */
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS, &v));
  ASSERT_NEAR(0.0f, value_float32(&v), 0.001f);
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_XAXIS_COLOR, &v));
  ASSERT_STREQ("#112233ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_YAXIS_COLOR, &v));
  ASSERT_STREQ("#445566ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_ZAXIS_COLOR, &v));
  ASSERT_STREQ("#778899ff", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_TICK_COLOR, &v));
  ASSERT_STREQ("#aabbccff", value_str(&v));
  value_reset(&v);

  widget_destroy(w);
}

/* 界面上的颜色下拉框与开关都按属性名写入，这条通道要与 setter 等价。 */
TEST(plot3d, colors_and_switches_by_prop_name) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_GRID_COLOR, "#010203ff"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_TICK_COLOR, "#040506ff"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_XAXIS_COLOR, "#070809ff"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_YAXIS_COLOR, "#0a0b0cff"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_ZAXIS_COLOR, "#0d0e0fff"));

  ASSERT_STREQ("#010203ff", widget_get_prop_str(w, PLOT3D_PROP_GRID_COLOR, NULL));
  ASSERT_STREQ("#040506ff", widget_get_prop_str(w, PLOT3D_PROP_TICK_COLOR, NULL));
  ASSERT_STREQ("#070809ff", widget_get_prop_str(w, PLOT3D_PROP_XAXIS_COLOR, NULL));
  ASSERT_STREQ("#0a0b0cff", widget_get_prop_str(w, PLOT3D_PROP_YAXIS_COLOR, NULL));
  ASSERT_STREQ("#0d0e0fff", widget_get_prop_str(w, PLOT3D_PROP_ZAXIS_COLOR, NULL));

  ASSERT_EQ(RET_OK, widget_set_prop_bool(w, PLOT3D_PROP_SHOW_GRID, FALSE));
  ASSERT_EQ(RET_OK, widget_set_prop_bool(w, PLOT3D_PROP_SHOW_AXIS_TICK, FALSE));
  ASSERT_EQ(RET_OK, widget_set_prop_bool(w, PLOT3D_PROP_ENABLE_CACHE, FALSE));
  ASSERT_EQ(RET_OK, widget_set_prop_bool(w, PLOT3D_PROP_EQUAL_AXIS, TRUE));

  ASSERT_EQ(FALSE, widget_get_prop_bool(w, PLOT3D_PROP_SHOW_GRID, TRUE));
  ASSERT_EQ(FALSE, widget_get_prop_bool(w, PLOT3D_PROP_SHOW_AXIS_TICK, TRUE));
  ASSERT_EQ(FALSE, widget_get_prop_bool(w, PLOT3D_PROP_ENABLE_CACHE, TRUE));
  ASSERT_EQ(TRUE, widget_get_prop_bool(w, PLOT3D_PROP_EQUAL_AXIS, FALSE));

  widget_destroy(w);
}

TEST(plot3d, parse_range) {
  float_t min_v = 0;
  float_t max_v = 0;

  ASSERT_EQ(RET_OK, plot3d_parse_range("0,360", &min_v, &max_v));
  ASSERT_NEAR(0.0f, min_v, 0.001f);
  ASSERT_NEAR(360.0f, max_v, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_parse_range(" -3.5 , 3.5 ", &min_v, &max_v));
  ASSERT_NEAR(-3.5f, min_v, 0.001f);
  ASSERT_NEAR(3.5f, max_v, 0.001f);

  /* 顺序写反了按小到大处理，不当成错误。 */
  ASSERT_EQ(RET_OK, plot3d_parse_range("5,1", &min_v, &max_v));
  ASSERT_NEAR(1.0f, min_v, 0.001f);
  ASSERT_NEAR(5.0f, max_v, 0.001f);

  ASSERT_NE(RET_OK, plot3d_parse_range("5", &min_v, &max_v));
  ASSERT_NE(RET_OK, plot3d_parse_range("", &min_v, &max_v));
  ASSERT_NE(RET_OK, plot3d_parse_range(NULL, &min_v, &max_v));
}

TEST(plot3d, parse_steps) {
  uint32_t cols = 0;
  uint32_t rows = 0;

  ASSERT_EQ(RET_OK, plot3d_parse_steps("25,8", &cols, &rows));
  ASSERT_EQ(25u, cols);
  ASSERT_EQ(8u, rows);

  /* 只写一个数时两个方向取同一密度。 */
  ASSERT_EQ(RET_OK, plot3d_parse_steps(" 16 ", &cols, &rows));
  ASSERT_EQ(16u, cols);
  ASSERT_EQ(16u, rows);

  /* 至少 2 个格点才能构成范围，上限防止点数爆炸。 */
  ASSERT_EQ(RET_OK, plot3d_parse_steps("1,0", &cols, &rows));
  ASSERT_EQ(2u, cols);
  ASSERT_EQ(2u, rows);
  ASSERT_EQ(RET_OK, plot3d_parse_steps("1000,1000", &cols, &rows));
  ASSERT_EQ(PLOT3D_MAX_SAMPLE_STEPS, cols);
  ASSERT_EQ(PLOT3D_MAX_SAMPLE_STEPS, rows);

  ASSERT_NE(RET_OK, plot3d_parse_steps("", &cols, &rows));
  ASSERT_NE(RET_OK, plot3d_parse_steps(NULL, &cols, &rows));
}

TEST(plot3d, colormap_get_color) {
  color_t c = color_init(0, 0, 0, 0);

  /* viridis 两端与正中间的三个色标。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &c));
  ASSERT_EQ(0x44, c.rgba.r);
  ASSERT_EQ(0x01, c.rgba.g);
  ASSERT_EQ(0x54, c.rgba.b);
  ASSERT_EQ(0xff, c.rgba.a);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.5f, &c));
  ASSERT_EQ(0x21, c.rgba.r);
  ASSERT_EQ(0x91, c.rgba.g);
  ASSERT_EQ(0x8c, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 1.0f, &c));
  ASSERT_EQ(0xfd, c.rgba.r);
  ASSERT_EQ(0xe7, c.rgba.g);
  ASSERT_EQ(0x25, c.rgba.b);

  /* 超出 [0,1] 钳到两端。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", -1.0f, &c));
  ASSERT_EQ(0x44, c.rgba.r);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 2.0f, &c));
  ASSERT_EQ(0xfd, c.rgba.r);

  /* 色标之间线性插值。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("gray", 0.5f, &c));
  ASSERT_NEAR(128, c.rgba.r, 1);
  ASSERT_NEAR(128, c.rgba.g, 1);
  ASSERT_NEAR(128, c.rgba.b, 1);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("jet", 0.0f, &c));
  ASSERT_TRUE(c.rgba.b > c.rgba.r);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("jet", 1.0f, &c));
  ASSERT_TRUE(c.rgba.r > c.rgba.b);

  /* 名字不认识或没给都回退到默认 colormap。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("no-such-map", 0.0f, &c));
  ASSERT_EQ(0x44, c.rgba.r);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color(NULL, 1.0f, &c));
  ASSERT_EQ(0xfd, c.rgba.r);

  /* 新增 MATLAB 风格色表：端点须命中约定 stop（未注册时会落到 viridis）。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("cool", 0.0f, &c));
  ASSERT_EQ(0x00, c.rgba.r);
  ASSERT_EQ(0xff, c.rgba.g);
  ASSERT_EQ(0xff, c.rgba.b);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("cool", 1.0f, &c));
  ASSERT_EQ(0xff, c.rgba.r);
  ASSERT_EQ(0x00, c.rgba.g);
  ASSERT_EQ(0xff, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("hot", 0.0f, &c));
  ASSERT_EQ(0x00, c.rgba.r);
  ASSERT_EQ(0x00, c.rgba.g);
  ASSERT_EQ(0x00, c.rgba.b);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("hot", 1.0f, &c));
  ASSERT_EQ(0xff, c.rgba.r);
  ASSERT_EQ(0xff, c.rgba.g);
  ASSERT_EQ(0xff, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("parula", 0.0f, &c));
  ASSERT_EQ(0x35, c.rgba.r);
  ASSERT_EQ(0x2a, c.rgba.g);
  ASSERT_EQ(0x87, c.rgba.b);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("parula", 1.0f, &c));
  ASSERT_EQ(0xf9, c.rgba.r);
  ASSERT_EQ(0xe8, c.rgba.g);
  ASSERT_EQ(0x46, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("hsv", 0.0f, &c));
  ASSERT_EQ(0xff, c.rgba.r);
  ASSERT_EQ(0x00, c.rgba.g);
  ASSERT_EQ(0x00, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("bone", 1.0f, &c));
  ASSERT_EQ(0xff, c.rgba.r);
  ASSERT_EQ(0xff, c.rgba.g);
  ASSERT_EQ(0xff, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("copper", 0.0f, &c));
  ASSERT_EQ(0x00, c.rgba.r);
  ASSERT_EQ(0x00, c.rgba.g);
  ASSERT_EQ(0x00, c.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("pink", 0.0f, &c));
  ASSERT_EQ(0x00, c.rgba.r);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("turbo", 0.0f, &c));
  ASSERT_EQ(0x30, c.rgba.r);
  ASSERT_EQ(0x12, c.rgba.g);
  ASSERT_EQ(0x3b, c.rgba.b);
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("turbo", 1.0f, &c));
  ASSERT_EQ(0x7a, c.rgba.r);
  ASSERT_EQ(0x04, c.rgba.g);
  ASSERT_EQ(0x03, c.rgba.b);

  ASSERT_NE(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, NULL));
}

/* z = x + y，格点坐标可直接从 z 反推，便于校验采样顺序。 */
static ret_t plot3d_test_grid_func(void* ctx, float_t x, float_t y, float_t* z) {
  uint32_t* calls = (uint32_t*)ctx;

  if (calls != NULL) {
    (*calls)++;
  }
  *z = x + y;

  return RET_OK;
}

static void plot3d_test_setup_grid_sample(widget_t* w, const char* plottype) {
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, plottype));
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, "0,3"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_range(w, "0,1"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "4,2"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, "grid"));
}

TEST(plot3d, grid_func_sample_dot) {
  plot3d_data_point_t point;
  uint32_t calls = 0;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, &calls));

  /* 4 列 x 2 行，行主序铺点，没有断点。 */
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(8u, calls);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(0.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(3.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 4, &point));
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);
  ASSERT_NEAR(1.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(3.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  /* 颜色按 z 归一化后取 colormap 两端色。 */
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(0x44, point.color.rgba.r);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_EQ(0xfd, point.color.rgba.r);

  widget_destroy(w);
}

TEST(plot3d, grid_func_sample_line) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, NULL));

  /* 每行一条折线，行之间插断点。 */
  ASSERT_EQ(9u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 4, &point));
  ASSERT_TRUE(point.is_break);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, grid_func_sample_surface) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "surface");
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, NULL));

  /* 自动三角化：6 * (rows-1) * (cols-1) 个顶点。 */
  ASSERT_EQ(18u, plot3d_get_data_points_nr(w));

  /* 第一个三角形 (0,0) (1,0) (1,1)。 */
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_NEAR(1.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_NEAR(1.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);

  /* 第二个三角形 (0,0) (1,1) (0,1)。 */
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, grid_func_resample_on_change) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, NULL));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));

  /* 改密度、改类型都要重新采样。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "5,3"));
  ASSERT_EQ(15u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "surface"));
  ASSERT_EQ(48u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(17u, plot3d_get_data_points_nr(w));

  /* 关掉采样模式就清空函数产生的点。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, "none"));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

TEST(plot3d, grid_func_and_dataset_exclusive) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));

  /* 设函数源后 CSV 数据让位。 */
  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, NULL));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_DATASET, &v));
  ASSERT_TRUE(value_str(&v) == NULL || *value_str(&v) == '\0');
  value_reset(&v);

  /* 反过来设 dataset 也要把函数源清掉。 */
  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n"));
  ASSERT_EQ(1u, plot3d_get_data_points_nr(w));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ("none", value_str(&v));
  value_reset(&v);

  widget_destroy(w);
}

TEST(plot3d, sample_z_expr) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));

  /* 表达式与回调采出同样的点：4 列 x 2 行，z = x + y。 */
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_NEAR(0.0f, point.z, 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(3.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  /* 改表达式立即重新采样。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "2 * x"));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(6.0f, point.z, 0.001f);

  /* 清空表达式就没有数据来源了，属性也一并清掉。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, NULL));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));
  ASSERT_TRUE(widget_get_prop_str(w, PLOT3D_PROP_SAMPLE_Z_EXPR, NULL) == NULL);

  widget_destroy(w);
}

TEST(plot3d, sample_z_expr_math_func) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "dot"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, "0,1"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_range(w, "0,1"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "2,2"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, "grid"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "sin(x) + cos(y)"));

  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_NEAR(1.0f, point.z, 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(sinf(1.0f) + cosf(1.0f), point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, sample_z_expr_invalid) {
  value_t v;
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));

  /* 语法不合法时保持原表达式，不让图变空。 */
  ASSERT_NE(RET_OK, plot3d_set_sample_z_expr(w, "sin("));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_Z_EXPR, &v));
  ASSERT_STREQ("x + y", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));

  /* 语法合法但变量不认识时取 0，不崩。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "no_such_var"));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(0.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, grid_func_beats_z_expr) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "100"));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(100.0f, point.z, 0.001f);

  /* 回调比表达式优先，取消回调后又回到表达式。 */
  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, plot3d_test_grid_func, NULL));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_set_grid_func(w, NULL, NULL));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_NEAR(100.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, sample_props_are_persistent) {
  value_t v;
  widget_t* clone = NULL;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "surface");
  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "jet"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x * y"));

  /* 采样设置随控件克隆一起走，克隆体不依赖 C 回调也能出图。 */
  clone = widget_clone(w, NULL);
  ASSERT_TRUE(clone != NULL);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ("grid", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  ASSERT_STREQ("0,3", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_STEPS, &v));
  ASSERT_STREQ("4,2", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_COLORMAP, &v));
  ASSERT_STREQ("jet", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_Z_EXPR, &v));
  ASSERT_STREQ("x * y", value_str(&v));
  value_reset(&v);

  ASSERT_EQ(plot3d_get_data_points_nr(w), plot3d_get_data_points_nr(clone));

  widget_destroy(clone);
  widget_destroy(w);
}

static ret_t plot3d_test_curve_func(void* ctx, float_t t, float_t* x, float_t* y, float_t* z) {
  uint32_t* calls = (uint32_t*)ctx;

  if (calls != NULL) {
    (*calls)++;
  }
  *x = t;
  *y = 2 * t;
  *z = 3 * t;

  return RET_OK;
}

static void plot3d_test_setup_curve_sample(widget_t* w, const char* plottype) {
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, plottype));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_range(w, "0,3"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 4));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_CURVE));
}

TEST(plot3d, curve_expr) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_expr(w, "cos(t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_expr(w, "sin(t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "t"));

  /* t 取 0、1、2、3，曲线是一条连续折线，没有断点。 */
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(1.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(0.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_FALSE(point.is_break);
  ASSERT_NEAR(cosf(3.0f), point.x, 0.001f);
  ASSERT_NEAR(sinf(3.0f), point.y, 0.001f);
  ASSERT_NEAR(3.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, curve_expr_defaults_x_to_t) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 只给 z：x 取 t，y 取 0，画的是 xz 平面上的曲线。 */
  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "t * t"));

  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_NEAR(2.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, curve_expr_z_uses_xy) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* x 与 y 先算出来，z 里可以直接引用。 */
  plot3d_test_setup_curve_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_expr(w, "t + 1"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_expr(w, "t + 2"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x * y"));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(4.0f, point.x, 0.001f);
  ASSERT_NEAR(5.0f, point.y, 0.001f);
  ASSERT_NEAR(20.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, curve_expr_on_unit_sphere) {
  uint32_t i = 0;
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 球面螺旋：三个表达式必须用同一个 t 求值，否则点会离开单位球面。 */
  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_range(w, "0,3.1416"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 64));
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_expr(w, "sin(t) * cos(20 * t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_expr(w, "sin(t) * sin(20 * t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "cos(t)"));

  ASSERT_EQ(64u, plot3d_get_data_points_nr(w));
  for (i = 0; i < 64; i++) {
    ASSERT_EQ(RET_OK, plot3d_get_data_point(w, i, &point));
    ASSERT_NEAR(1.0f, point.x * point.x + point.y * point.y + point.z * point.z, 0.001f);
  }

  widget_destroy(w);
}

TEST(plot3d, curve_func) {
  uint32_t calls = 0;
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_expr(w, "100"));
  ASSERT_EQ(RET_OK, plot3d_set_curve_func(w, plot3d_test_curve_func, &calls));

  /* 回调比表达式优先。 */
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(4u, calls);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(3.0f, point.x, 0.001f);
  ASSERT_NEAR(6.0f, point.y, 0.001f);
  ASSERT_NEAR(9.0f, point.z, 0.001f);

  /* 取消回调后回到表达式。 */
  ASSERT_EQ(RET_OK, plot3d_set_curve_func(w, NULL, NULL));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(100.0f, point.x, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, curve_and_grid_switch) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* grid 与 curve 的范围、密度各自独立，来回切换都按当前模式重新采样。 */
  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_sample_t_range(w, "0,3"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 4));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_CURVE));
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(3.0f, point.x, 0.001f);
  ASSERT_NEAR(3.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

TEST(plot3d, curve_props) {
  value_t v;
  widget_t* clone = NULL;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 240));
  ASSERT_EQ(240u, (uint32_t)widget_get_prop_int(w, PLOT3D_PROP_SAMPLE_T_STEPS, 0));

  /* 与其它采样属性一样被钳位。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 0));
  ASSERT_EQ(PLOT3D_MIN_SAMPLE_STEPS, (uint32_t)widget_get_prop_int(w, PLOT3D_PROP_SAMPLE_T_STEPS, 0));

  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_expr(w, "cos(t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_expr(w, "sin(t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "t"));

  clone = widget_clone(w, NULL);
  ASSERT_TRUE(clone != NULL);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ(PLOT3D_SAMPLE_MODE_CURVE, value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_T_RANGE, &v));
  ASSERT_STREQ("0,3", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_X_EXPR, &v));
  ASSERT_STREQ("cos(t)", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_Y_EXPR, &v));
  ASSERT_STREQ("sin(t)", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(plot3d_get_data_points_nr(w), plot3d_get_data_points_nr(clone));

  /* 表达式语法非法时不覆盖现有的。 */
  ASSERT_NE(RET_OK, plot3d_set_sample_x_expr(w, "cos("));
  ASSERT_NE(RET_OK, plot3d_set_sample_y_expr(w, "sin("));

  widget_destroy(clone);
  widget_destroy(w);
}

static void plot3d_test_setup_matrix_sample(widget_t* w, const char* plottype) {
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, plottype));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
}

TEST(plot3d, matrix_from_text) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 3 列 2 行，没给范围时 x 与 y 按下标取 0、1、2 与 0、1。 */
  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));

  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_NEAR(0.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(1.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_NEAR(2.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(3.0f, point.z, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_NEAR(2.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);
  ASSERT_NEAR(6.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, matrix_rows_by_newline) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 换行也当作换行分隔，便于在 XML 里分行书写。 */
  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1, 2\n3, 4"));

  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_NEAR(1.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);
  ASSERT_NEAR(4.0f, point.z, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, matrix_text_invalid) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));

  /* 各行列数不同或内容为空时拒绝，原矩阵保持不变。 */
  ASSERT_NE(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3"));
  ASSERT_NE(RET_OK, plot3d_set_sample_z_matrix(w, ";"));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_STREQ("1,2,3;4,5,6", widget_get_prop_str(w, PLOT3D_PROP_SAMPLE_Z_MATRIX, NULL));

  widget_destroy(w);
}

TEST(plot3d, matrix_ranges) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 给了范围就把下标映射到范围上。 */
  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, "0,10"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_range(w, "-1,1"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_NEAR(5.0f, point.x, 0.001f);
  ASSERT_NEAR(-1.0f, point.y, 0.001f);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_NEAR(10.0f, point.x, 0.001f);
  ASSERT_NEAR(1.0f, point.y, 0.001f);

  /* 清空范围又回到按下标。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, NULL));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_NEAR(2.0f, point.x, 0.001f);

  widget_destroy(w);
}

TEST(plot3d, matrix_layout_by_plottype) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* surface 自动三角化：一个四边形两个三角形。 */
  plot3d_test_setup_matrix_sample(w, "surface");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));

  /* line 每行一条折线，行之间断开。 */
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(5u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_TRUE(point.is_break);

  widget_destroy(w);
}

TEST(plot3d, matrix_from_c) {
  const float_t zs[] = {1, 2, 3, 4, 5, 6};
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "9,9;9,9"));
  ASSERT_EQ(RET_OK, plot3d_set_z_matrix(w, zs, 3, 2));

  /* 后设置的生效，属性里的字符串一并清掉，避免与实际数据不一致。 */
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_NEAR(6.0f, point.z, 0.001f);
  ASSERT_TRUE(widget_get_prop_str(w, PLOT3D_PROP_SAMPLE_Z_MATRIX, NULL) == NULL);

  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "9,9;9,9"));
  ASSERT_EQ(4u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

/* 经 C API 与经属性路径（XML 加载、clone、脚本）设置矩阵，对 dataset 的影响必须一致：两者是
 * 同一个逻辑操作，不能因为属性路径绕开了 C API 就有两种行为。 */
TEST(plot3d, matrix_set_by_api_and_prop_agree_on_dataset) {
  const char* dataset = "1,2,3,#112233\n";
  widget_t* by_api = plot3d_create(NULL, 0, 0, 320, 240);
  widget_t* by_prop = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(by_api, dataset));
  ASSERT_EQ(RET_OK, plot3d_set_dataset(by_prop, dataset));

  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(by_api, "1,2;3,4"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(by_prop, PLOT3D_PROP_SAMPLE_Z_MATRIX, "1,2;3,4"));

  ASSERT_STREQ(widget_get_prop_str(by_prop, PLOT3D_PROP_DATASET, ""),
               widget_get_prop_str(by_api, PLOT3D_PROP_DATASET, ""));
  ASSERT_EQ(plot3d_get_data_points_nr(by_prop), plot3d_get_data_points_nr(by_api));

  /* 顺带钉住最终行为本身：矩阵与 dataset 由 sample-mode 区分，此刻模式还是 none，
   * dataset 的文本与数据点都留着。 */
  ASSERT_STREQ(dataset, widget_get_prop_str(by_api, PLOT3D_PROP_DATASET, ""));
  ASSERT_EQ(1u, plot3d_get_data_points_nr(by_api));

  widget_destroy(by_api);
  widget_destroy(by_prop);
}

/* 矩阵与 dataset 各自属于一个数据源，由 sample-mode 决定谁出数据，来回切换互不干扰。 */
TEST(plot3d, dataset_and_matrix_switch_by_sample_mode) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_NONE));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 1, &point));
  ASSERT_NEAR(4.0f, point.x, 0.001f);
  ASSERT_EQ(0x44, point.color.rgba.r);

  /* 中间清空矩阵会把 sample_from_func 清成 FALSE；切回 none 不能靠遗留置位。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, ""));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(FALSE, PLOT3D(w)->sample_from_func);
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_NONE));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_NEAR(1.0f, point.x, 0.001f);
  ASSERT_EQ(0x11, point.color.rgba.r);

  /* 经无数据的 grid 中转同样会清掉置位——必须先从有数据的函数源切过去，
   * 才会真正进入重采样并把 sample_from_func 重算成 FALSE；从 none 直接切 grid
   * 会被提前返回挡掉，点数原样留下，测不到这条洞。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(FALSE, PLOT3D(w)->sample_from_func);
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_NONE));
  ASSERT_EQ(2u, plot3d_get_data_points_nr(w));

  /* 重设 dataset 只把模式拉回 none，不碰矩阵数据。 */
  ASSERT_EQ(RET_OK, plot3d_set_dataset(w, "1,2,3,#112233\n"));
  ASSERT_EQ(1u, plot3d_get_data_points_nr(w));
  ASSERT_STREQ("1,2,3;4,5,6", widget_get_prop_str(w, PLOT3D_PROP_SAMPLE_Z_MATRIX, ""));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

TEST(plot3d, matrix_color_by_z) {
  color_t expected = color_init(0, 0, 0, 0);
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 配色与网格采样一致：按 z 归一化后到配色表取色。 */
  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "viridis"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 1.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 5, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  widget_destroy(w);
}

TEST(plot3d, matrix_props) {
  widget_t* clone = NULL;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_matrix_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));

  clone = widget_clone(w, NULL);
  ASSERT_TRUE(clone != NULL);
  ASSERT_STREQ(PLOT3D_SAMPLE_MODE_MATRIX,
               widget_get_prop_str(clone, PLOT3D_PROP_SAMPLE_MODE, ""));
  ASSERT_STREQ("1,2,3;4,5,6", widget_get_prop_str(clone, PLOT3D_PROP_SAMPLE_Z_MATRIX, NULL));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(clone));

  /* 切到网格再切回来，矩阵数据还在。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "4,2"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(8u, plot3d_get_data_points_nr(w));

  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));

  widget_destroy(clone);
  widget_destroy(w);
}

static ret_t plot3d_test_color_func(void* ctx, float_t x, float_t y, float_t z, color_t* color) {
  uint32_t* calls = (uint32_t*)ctx;

  if (calls != NULL) {
    (*calls)++;
  }
  *color = color_init(0x11, 0x22, 0x33, 0xff);

  return RET_OK;
}

TEST(plot3d, color_expr_scalar) {
  color_t expected = color_init(0, 0, 0, 0);
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 默认按 z 取色，这里改成按 -z 取色，两端的颜色应当对调。 */
  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "viridis"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "0 - z"));

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 1.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.g, point.color.rgba.g);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.g, point.color.rgba.g);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  widget_destroy(w);
}

TEST(plot3d, color_expr_string) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 表达式返回字符串时直接当颜色用，配合 if 可以分段着色。 */
  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "if(z > 2, \"#ff0000\", \"#0000ff\")"));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(0x00, point.color.rgba.r);
  ASSERT_EQ(0xff, point.color.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 7, &point));
  ASSERT_EQ(0xff, point.color.rgba.r);
  ASSERT_EQ(0x00, point.color.rgba.b);

  widget_destroy(w);
}

TEST(plot3d, color_func_beats_color_expr) {
  uint32_t calls = 0;
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "\"#ff0000\""));
  ASSERT_EQ(RET_OK, plot3d_set_color_func(w, plot3d_test_color_func, &calls));

  ASSERT_EQ(8u, calls);
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(0x11, point.color.rgba.r);
  ASSERT_EQ(0x22, point.color.rgba.g);
  ASSERT_EQ(0x33, point.color.rgba.b);

  /* 取消回调后回到表达式。 */
  ASSERT_EQ(RET_OK, plot3d_set_color_func(w, NULL, NULL));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(0xff, point.color.rgba.r);
  ASSERT_EQ(0x00, point.color.rgba.b);

  widget_destroy(w);
}

TEST(plot3d, color_expr_on_curve) {
  color_t expected = color_init(0, 0, 0, 0);
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 曲线上可以按 t 取色，让颜色沿曲线走而不是按高度。 */
  plot3d_test_setup_curve_sample(w, "line");
  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "viridis"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "sin(t)"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "t"));

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 1.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 3, &point));
  ASSERT_EQ(expected.rgba.r, point.color.rgba.r);
  ASSERT_EQ(expected.rgba.b, point.color.rgba.b);

  widget_destroy(w);
}

TEST(plot3d, color_expr_props) {
  value_t v;
  widget_t* clone = NULL;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  plot3d_test_setup_grid_sample(w, "dot");
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "z * 2"));

  /* 语法非法时保持原表达式。 */
  ASSERT_NE(RET_OK, plot3d_set_sample_color_expr(w, "if("));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_COLOR_EXPR, &v));
  ASSERT_STREQ("z * 2", value_str(&v));
  value_reset(&v);

  clone = widget_clone(w, NULL);
  ASSERT_TRUE(clone != NULL);
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(clone, PLOT3D_PROP_SAMPLE_COLOR_EXPR, &v));
  ASSERT_STREQ("z * 2", value_str(&v));
  value_reset(&v);

  /* 清空后回到按 z 取色。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, NULL));
  ASSERT_TRUE(widget_get_prop_str(w, PLOT3D_PROP_SAMPLE_COLOR_EXPR, NULL) == NULL);

  widget_destroy(clone);
  widget_destroy(w);
}

TEST(plot3d, sample_props) {
  value_t v;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ("none", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_COLORMAP, &v));
  ASSERT_STREQ("viridis", value_str(&v));
  value_reset(&v);

  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_SAMPLE_X_RANGE, "0,360"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_SAMPLE_Y_RANGE, "0,7"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_SAMPLE_STEPS, "25,8"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_COLORMAP, "jet"));
  ASSERT_EQ(RET_OK, widget_set_prop_str(w, PLOT3D_PROP_SAMPLE_MODE, "grid"));

  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  ASSERT_STREQ("0,360", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  ASSERT_STREQ("0,7", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_STEPS, &v));
  ASSERT_STREQ("25,8", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_COLORMAP, &v));
  ASSERT_STREQ("jet", value_str(&v));
  value_reset(&v);
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ("grid", value_str(&v));
  value_reset(&v);

  /* 非法模式名不改变现状。 */
  ASSERT_NE(RET_OK, plot3d_set_sample_mode(w, "no-such-mode"));
  ASSERT_EQ(RET_OK, widget_get_prop(w, PLOT3D_PROP_SAMPLE_MODE, &v));
  ASSERT_STREQ("grid", value_str(&v));
  value_reset(&v);

  /* 没有回调时开采样模式也不产生点。 */
  ASSERT_EQ(0u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}
