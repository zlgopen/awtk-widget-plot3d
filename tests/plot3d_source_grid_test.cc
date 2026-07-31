#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "plot3d/plot3d.h"
#include "plot3d/source/plot3d_source.h"

namespace {

/* grid 插件自身的用例：SetUp 建实例与装点的数组，TearDown 一并销毁。
 *
 * 只注册 grid 一个插件而不是 plot3d_register_all()：后者一旦有别的插件注册失败，会把本组
 * 用例一起拖红，掩盖真正的失败点。
 *
 * 注册表是进程内单例，用例改动后必须复原成全量内置插件已注册的状态，否则会污染其它 TU 的用例
 * （tests/SConscript 用 Glob 收集，跨 TU 执行顺序不可控）。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dSourceGridTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    ASSERT_EQ(RET_OK, plot3d_source_grid_register());
    source = plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_GRID);
    ASSERT_NE((plot3d_source_t*)NULL, source);
  }

  void TearDown() {
    if (source != NULL) {
      plot3d_source_destroy(source);
      source = NULL;
    }

    ASSERT_EQ(RET_OK, plot3d_unregister());
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }

  ret_t set_prop_str(const char* name, const char* text) {
    value_t v;

    value_set_str(&v, text);

    return plot3d_source_set_prop(source, name, &v);
  }

  ret_t sample(plot3d_source_result_t* result) {
    plot3d_source_result_init(result, NULL);

    return plot3d_source_sample(source, result);
  }

  plot3d_source_t* source;
};

static ret_t plot3d_test_grid_func(void* ctx, float_t x, float_t y, float_t* z) {
  uint32_t* calls = (uint32_t*)ctx;

  if (calls != NULL) {
    (*calls)++;
  }
  *z = x + y;

  return RET_OK;
}

}  // namespace

/* 不用 TEST_F：未注册时连 fixture 的 create 都会失败，这条只钉住工厂本身。 */
TEST(Plot3dSourceGridFactoryTest, create_unregistered_is_null) {
  plot3d_unregister();
  ASSERT_EQ((plot3d_source_t*)NULL, plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(RET_OK, plot3d_register_all());
}

TEST_F(Plot3dSourceGridTest, grid_defaults_readable) {
  value_t v;

  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_STEPS, &v));
  ASSERT_STREQ("20,20", value_str(&v));

  /* x/y 范围默认「不指定」：读回 NULL，采样时按 0~1。 */
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));
}

TEST_F(Plot3dSourceGridTest, grid_sample_size_and_z_values) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_X_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Y_RANGE, "0,1"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_STEPS, "4,2"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "x + y"));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_GRID, result.type);
  ASSERT_EQ(4u, result.cols);
  ASSERT_EQ(2u, result.rows);
  ASSERT_NEAR(0.0f, result.x0, 0.001f);
  ASSERT_NEAR(3.0f, result.x1, 0.001f);
  ASSERT_NEAR(0.0f, result.y0, 0.001f);
  ASSERT_NEAR(1.0f, result.y1, 0.001f);

  /* 行主序：首点 (0,0)->0，末点 (3,1)->4。 */
  ASSERT_NEAR(0.0f, result.zs[0], 0.001f);
  ASSERT_NEAR(4.0f, result.zs[7], 0.001f);
}

TEST_F(Plot3dSourceGridTest, grid_default_steps_twenty_by_twenty) {
  plot3d_source_result_t result;

  /* 未显式设 sample-steps 时仍按 create() 里的 20×20。 */
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "x + y"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(20u, result.cols);
  ASSERT_EQ(20u, result.rows);
  ASSERT_NEAR(0.0f, result.x0, 0.001f);
  ASSERT_NEAR(1.0f, result.x1, 0.001f);
}

TEST_F(Plot3dSourceGridTest, grid_invalid_z_expr_rejected_and_keeps_old) {
  value_t v;

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "x + y"));

  ASSERT_EQ(RET_BAD_PARAMS, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "sin("));

  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_EXPR, &v));
  ASSERT_STREQ("x + y", value_str(&v));
}

TEST_F(Plot3dSourceGridTest, grid_func_overrides_expr) {
  uint32_t calls = 0;
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_X_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Y_RANGE, "0,1"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_STEPS, "4,2"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "100"));
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_GRID_FUNC,
                                            (void*)plot3d_test_grid_func, &calls));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(8u, calls);
  ASSERT_NEAR(0.0f, result.zs[0], 0.001f);
  ASSERT_NEAR(4.0f, result.zs[7], 0.001f);

  /* 取消回调后回到表达式。 */
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_GRID_FUNC, NULL, NULL));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_NEAR(100.0f, result.zs[0], 0.001f);
  ASSERT_NEAR(100.0f, result.zs[7], 0.001f);
}

TEST_F(Plot3dSourceGridTest, grid_reset_keeps_shared_config) {
  value_t v;

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_X_RANGE, "0,10"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Y_RANGE, "-1,1"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_STEPS, "3,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_GRID_FUNC,
                                            (void*)plot3d_test_grid_func, NULL));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, plot3d_source_reset(source));

  /* 回调是「数据源身份」，让位时清掉；范围 / steps / z-expr 是配置（含与 matrix/curve 共享），
   * reset 不得清。 */
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  ASSERT_STREQ("0,10", value_str(&v));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_STEPS, &v));
  ASSERT_STREQ("3,3", value_str(&v));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_EXPR, &v));
  ASSERT_STREQ("x + y", value_str(&v));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));
}

/* widget 级：阶段二后 sample-x-range 广播须同时送达 grid 与 matrix。 */
TEST(Plot3dSourceGridWidgetTest, sample_x_range_reaches_grid_and_matrix) {
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, "0,10"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_y_range(w, "0,5"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "3,2"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));

  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_NEAR(10.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);
  ASSERT_NEAR(10.0f, point.z, 0.001f);

  /* 同一控件上的 matrix 实例也收到了广播后的范围（3 列映射到 0~10）。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3;4,5,6"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(6u, plot3d_get_data_points_nr(w));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 2, &point));
  ASSERT_NEAR(10.0f, point.x, 0.001f);
  ASSERT_NEAR(0.0f, point.y, 0.001f);

  widget_destroy(w);
}
