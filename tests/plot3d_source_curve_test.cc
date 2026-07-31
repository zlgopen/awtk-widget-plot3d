#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "plot3d/plot3d.h"
#include "plot3d/source/plot3d_source.h"

namespace {

/* curve 插件自身的用例：SetUp 建实例与装点的数组，TearDown 一并销毁。
 *
 * 只注册 curve 一个插件而不是 plot3d_register_all()：后者一旦有别的插件注册失败，会把本组
 * 用例一起拖红，掩盖真正的失败点。
 *
 * 注册表是进程内单例，用例改动后必须复原成全量内置插件已注册的状态，否则会污染其它 TU 的用例
 * （tests/SConscript 用 Glob 收集，跨 TU 执行顺序不可控）。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dSourceCurveTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    ASSERT_EQ(RET_OK, plot3d_source_curve_register());
    source = plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_CURVE);
    ASSERT_NE((plot3d_source_t*)NULL, source);

    /* 点由插件 TKMEM_ZALLOC 出来 push 进来，销毁由持有数组的一方负责，这里模拟核心的
     * plot3d->data_points。 */
    darray_init(&points, 8, default_destroy, NULL);
  }

  void TearDown() {
    darray_deinit(&points);
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

  ret_t set_prop_uint32(const char* name, uint32_t n) {
    value_t v;

    value_set_uint32(&v, n);

    return plot3d_source_set_prop(source, name, &v);
  }

  ret_t sample(plot3d_source_result_t* result) {
    darray_clear(&points);
    plot3d_source_result_init(result, &points);

    return plot3d_source_sample(source, result);
  }

  const plot3d_data_point_t* point_at(uint32_t index) {
    return (const plot3d_data_point_t*)darray_get(&points, index);
  }

  plot3d_source_t* source;
  darray_t points;
};

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

}  // namespace

/* 不用 TEST_F：未注册时连 fixture 的 create 都会失败，这条只钉住工厂本身。 */
TEST(Plot3dSourceCurveFactoryTest, create_unregistered_is_null) {
  plot3d_unregister();
  ASSERT_EQ((plot3d_source_t*)NULL, plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_CURVE));
  ASSERT_EQ(RET_OK, plot3d_register_all());
}

TEST_F(Plot3dSourceCurveTest, curve_defaults_readable) {
  value_t v;

  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_T_RANGE, &v));
  ASSERT_STREQ("0,1", value_str(&v));

  value_set_uint32(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_T_STEPS, &v));
  ASSERT_EQ(PLOT3D_DEFAULT_CURVE_STEPS, value_uint32(&v));
}

TEST_F(Plot3dSourceCurveTest, curve_sample_point_count_matches_t_steps) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_uint32(PLOT3D_PROP_SAMPLE_T_STEPS, 4));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_T_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "t"));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_POINTS, result.type);
  ASSERT_EQ(FALSE, result.has_color);
  ASSERT_EQ(4u, points.size);

  ASSERT_NEAR(0.0f, point_at(0)->x, 0.001f);
  ASSERT_NEAR(0.0f, point_at(0)->z, 0.001f);
  ASSERT_NEAR(3.0f, point_at(3)->x, 0.001f);
  ASSERT_NEAR(3.0f, point_at(3)->z, 0.001f);
}

TEST_F(Plot3dSourceCurveTest, curve_sample_t_steps_zero_clamped) {
  value_t v;
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_uint32(PLOT3D_PROP_SAMPLE_T_STEPS, 0));
  value_set_uint32(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_T_STEPS, &v));
  ASSERT_EQ(PLOT3D_MIN_SAMPLE_STEPS, value_uint32(&v));

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "t"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(PLOT3D_MIN_SAMPLE_STEPS, points.size);
}

TEST_F(Plot3dSourceCurveTest, curve_sample_exposes_ts) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_uint32(PLOT3D_PROP_SAMPLE_T_STEPS, 4));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_T_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "t"));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_NE((const float_t*)NULL, result.ts);
  ASSERT_NEAR(0.0f, result.ts[0], 0.001f);
  ASSERT_NEAR(1.0f, result.ts[1], 0.001f);
  ASSERT_NEAR(2.0f, result.ts[2], 0.001f);
  ASSERT_NEAR(3.0f, result.ts[3], 0.001f);
}

TEST_F(Plot3dSourceCurveTest, curve_func_overrides_expr) {
  uint32_t calls = 0;
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_prop_uint32(PLOT3D_PROP_SAMPLE_T_STEPS, 4));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_T_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_X_EXPR, "100"));
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_CURVE_FUNC,
                                            (void*)plot3d_test_curve_func, &calls));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(4u, points.size);
  ASSERT_EQ(4u, calls);
  ASSERT_NEAR(3.0f, point_at(3)->x, 0.001f);
  ASSERT_NEAR(6.0f, point_at(3)->y, 0.001f);
  ASSERT_NEAR(9.0f, point_at(3)->z, 0.001f);

  /* 取消回调后回到表达式。 */
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_CURVE_FUNC, NULL, NULL));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_NEAR(100.0f, point_at(3)->x, 0.001f);
}

TEST_F(Plot3dSourceCurveTest, curve_reset_keeps_t_config) {
  value_t v;

  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_T_RANGE, "0,3"));
  ASSERT_EQ(RET_OK, set_prop_uint32(PLOT3D_PROP_SAMPLE_T_STEPS, 4));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_X_EXPR, "cos(t)"));
  ASSERT_EQ(RET_OK, set_prop_str(PLOT3D_PROP_SAMPLE_Z_EXPR, "t"));
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_CURVE_FUNC,
                                            (void*)plot3d_test_curve_func, NULL));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, plot3d_source_reset(source));

  /* 回调是「数据源身份」，让位时清掉；t 范围与点数是配置属性，reset 不得清。 */
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_T_RANGE, &v));
  ASSERT_STREQ("0,3", value_str(&v));
  value_set_uint32(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_T_STEPS, &v));
  ASSERT_EQ(4u, value_uint32(&v));

  /* sample-z-expr 与 grid 共享，清掉会让两份副本分叉。x 表达式是 curve 独有身份的一部分，
   * 与 set_dataset 今天只清 curve_func、保留表达式的历史行为对齐：reset 只清回调。 */
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_EXPR, &v));
  ASSERT_STREQ("t", value_str(&v));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_X_EXPR, &v));
  ASSERT_STREQ("cos(t)", value_str(&v));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));
}

/* widget 级：curve 产出的 ts 经核心 POINTS 接入后，sample-color-expr="t" 生效。 */
TEST(Plot3dSourceCurveWidgetTest, color_expr_t_on_curve) {
  color_t expected = color_init(0, 0, 0, 0);
  plot3d_data_point_t point;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_range(w, "0,3"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 4));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_CURVE));
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
