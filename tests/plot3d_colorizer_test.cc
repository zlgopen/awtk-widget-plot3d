#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d/plot3d.h"
#include "plot3d/colorizer/plot3d_colorizer.h"
#include "plot3d/source/plot3d_source.h"
#include "plot3d/type/plot3d_type.h"
#include "plot3d_register.h"

namespace {

static ret_t counting_color_func(void* ctx, float_t x, float_t y, float_t z, color_t* color) {
  uint32_t* count = (uint32_t*)ctx;
  (*count)++;
  *color = color_init(0x11, 0x22, 0x33, 0xff);

  return RET_OK;
}

/* 故意写出参再返回失败：回调失败时插件不得采纳它写的值，否则核心的默认配色就被绕过了。 */
static ret_t failing_color_func(void* ctx, float_t x, float_t y, float_t z, color_t* color) {
  *color = color_init(0xff, 0x00, 0x00, 0xff);

  return RET_FAIL;
}

typedef struct _fake_colorizer_t {
  plot3d_colorizer_t colorizer;
  bool_t active;
  color_t color;
  int32_t fake_prop;
} fake_colorizer_t;

static plot3d_colorizer_t* fake_colorizer_create(void);
static plot3d_colorizer_t* fake_colorizer_create_v2(void);
static plot3d_colorizer_t* fake2_colorizer_create(void);
static plot3d_colorizer_t* fake_bare_colorizer_create(void);

#define FAKE_COLORIZER_V2_MARK 12345
/* 两个 fake 各出一种颜色，用例才能分辨核心用的是哪一个。 */
#define FAKE_COLORIZER_R 0x11
#define FAKE2_COLORIZER_R 0x22

static bool_t fake_colorizer_is_active(plot3d_colorizer_t* colorizer) {
  return ((fake_colorizer_t*)colorizer)->active;
}

static ret_t fake_colorizer_eval(plot3d_colorizer_t* colorizer, const plot3d_sample_pos_t* pos,
                                 plot3d_color_value_t* out) {
  fake_colorizer_t* fake = (fake_colorizer_t*)colorizer;

  out->is_color = TRUE;
  out->color = fake->color;

  return RET_OK;
}

static ret_t fake_colorizer_set_prop(plot3d_colorizer_t* colorizer, const char* name,
                                     const value_t* v) {
  fake_colorizer_t* fake = (fake_colorizer_t*)colorizer;

  if (tk_str_eq(name, "fake-prop")) {
    int32_t value = value_int(v);
    /* 先校验后落值：负数拒绝，原值不变。 */
    return_value_if_fail(value >= 0, RET_BAD_PARAMS);
    fake->fake_prop = value;

    return RET_OK;
  } else if (tk_str_eq(name, "fake-active")) {
    fake->active = value_bool(v);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t fake_colorizer_get_prop(plot3d_colorizer_t* colorizer, const char* name, value_t* v) {
  fake_colorizer_t* fake = (fake_colorizer_t*)colorizer;

  if (tk_str_eq(name, "fake-prop")) {
    value_set_int(v, fake->fake_prop);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

/* fake2 只认领自己那份开关，用例才能单独让某一个失效。 */
static ret_t fake2_colorizer_set_prop(plot3d_colorizer_t* colorizer, const char* name,
                                      const value_t* v) {
  fake_colorizer_t* fake = (fake_colorizer_t*)colorizer;

  if (tk_str_eq(name, "fake2-active")) {
    fake->active = value_bool(v);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t fake_colorizer_destroy(plot3d_colorizer_t* colorizer) {
  TKMEM_FREE(colorizer);

  return RET_OK;
}

/* 不用指定初始化器：MSVC 在 C++20 之前不支持。
 * set_data 特意留空，用来验证转发函数在成员缺失时的返回值。 */
static plot3d_colorizer_vtable_t s_fake_colorizer_vtable;

/* type 与上面相同、create 不同，用来验证同名注册是「替换」而不是「忽略」。 */
static plot3d_colorizer_vtable_t s_fake_colorizer_vtable_v2;

/* type 与上面不同（同 type 是替换而不是新增），与 s_fake_colorizer_vtable 同时注册可凑出两个
 * 同时生效的配色插件。 */
static plot3d_colorizer_vtable_t s_fake2_colorizer_vtable;

/* 只填必填的三个成员，用来验证全部可选成员缺失时转发函数的返回值。 */
static plot3d_colorizer_vtable_t s_fake_bare_colorizer_vtable;

static void fake_colorizer_vtable_init(void) {
  memset(&s_fake_colorizer_vtable, 0x00, sizeof(s_fake_colorizer_vtable));
  s_fake_colorizer_vtable.type = "fake";
  s_fake_colorizer_vtable.create = fake_colorizer_create;
  s_fake_colorizer_vtable.is_active = fake_colorizer_is_active;
  s_fake_colorizer_vtable.eval = fake_colorizer_eval;
  s_fake_colorizer_vtable.set_prop = fake_colorizer_set_prop;
  s_fake_colorizer_vtable.get_prop = fake_colorizer_get_prop;
  s_fake_colorizer_vtable.destroy = fake_colorizer_destroy;

  memcpy(&s_fake_colorizer_vtable_v2, &s_fake_colorizer_vtable, sizeof(s_fake_colorizer_vtable_v2));
  s_fake_colorizer_vtable_v2.create = fake_colorizer_create_v2;

  memcpy(&s_fake2_colorizer_vtable, &s_fake_colorizer_vtable, sizeof(s_fake2_colorizer_vtable));
  s_fake2_colorizer_vtable.type = "fake2";
  s_fake2_colorizer_vtable.create = fake2_colorizer_create;
  s_fake2_colorizer_vtable.set_prop = fake2_colorizer_set_prop;
  s_fake2_colorizer_vtable.get_prop = NULL;

  memset(&s_fake_bare_colorizer_vtable, 0x00, sizeof(s_fake_bare_colorizer_vtable));
  s_fake_bare_colorizer_vtable.type = "fake-bare";
  s_fake_bare_colorizer_vtable.create = fake_bare_colorizer_create;
  s_fake_bare_colorizer_vtable.destroy = fake_colorizer_destroy;
}

static plot3d_colorizer_t* fake_colorizer_create(void) {
  fake_colorizer_t* fake = TKMEM_ZALLOC(fake_colorizer_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->colorizer.vt = &s_fake_colorizer_vtable;
  fake->active = TRUE;
  fake->color = color_init(FAKE_COLORIZER_R, 0, 0, 0xff);

  return (plot3d_colorizer_t*)fake;
}

static plot3d_colorizer_t* fake_colorizer_create_v2(void) {
  fake_colorizer_t* fake = TKMEM_ZALLOC(fake_colorizer_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->colorizer.vt = &s_fake_colorizer_vtable_v2;
  fake->fake_prop = FAKE_COLORIZER_V2_MARK;

  return (plot3d_colorizer_t*)fake;
}

static plot3d_colorizer_t* fake2_colorizer_create(void) {
  fake_colorizer_t* fake = TKMEM_ZALLOC(fake_colorizer_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->colorizer.vt = &s_fake2_colorizer_vtable;
  fake->active = TRUE;
  fake->color = color_init(FAKE2_COLORIZER_R, 0, 0, 0xff);

  return (plot3d_colorizer_t*)fake;
}

static plot3d_colorizer_t* fake_bare_colorizer_create(void) {
  fake_colorizer_t* fake = TKMEM_ZALLOC(fake_colorizer_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->colorizer.vt = &s_fake_bare_colorizer_vtable;

  return (plot3d_colorizer_t*)fake;
}

/* 注册表是进程内单例，用例改动后必须复原，否则会污染其它 TU 的用例。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dColorizerTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    plot3d_register_all();
  }
  void TearDown() {
    plot3d_unregister();
    plot3d_register_all();
  }
};

/* 接口层用例用的是空注册表加自己的 fake，与上面端到端用例的「内置插件已注册」前提相反，
 * 所以单开一个 fixture。 */
class Plot3dColorizerFactoryTest : public testing::Test {
 protected:
  void SetUp() {
    fake_colorizer_vtable_init();
    plot3d_unregister();
  }
  void TearDown() {
    ASSERT_EQ(RET_OK, plot3d_unregister());
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }
};

}  // namespace

TEST_F(Plot3dColorizerTest, expr_works_in_matrix_mode) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);
  plot3d_data_point_t p;

  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "\"#ff0000\""));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(0xff, p.color.rgba.r);

  widget_destroy(w);
}

TEST_F(Plot3dColorizerTest, color_func_evaluated_once_per_grid_node) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);
  uint32_t count = 0;

  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "surface"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2,3,4;5,6,7,8"));
  ASSERT_EQ(RET_OK, plot3d_set_color_func(w, counting_color_func, &count));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  /* 4x2 网格 8 个节点，surface 展开的顶点复用同一节点结果。 */
  ASSERT_EQ(8u, count);

  widget_destroy(w);
}

/* has_t 为 FALSE 时 t 按未定义处理：曲线留在插件变量表里的 t 不得被网格类模式读到。 */
TEST_F(Plot3dColorizerTest, t_is_undefined_without_has_t) {
  plot3d_data_point_t p;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 先跑一遍曲线，把远大于 5 的 t 留在插件的变量表里。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_range(w, "6,10"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_t_steps(w, 4));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "t"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "t"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_CURVE));

  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "if(t > 5, \"#ff0000\", \"#00ff00\")"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));

  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(0x00, p.color.rgba.r);
  ASSERT_EQ(0xff, p.color.rgba.g);

  widget_destroy(w);
}

TEST_F(Plot3dColorizerTest, instance_created_for_each_registered_colorizer) {
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  /* register_all 只注册了内置的 expr 一种。 */
  ASSERT_EQ(1u, plot3d_colorizer_factory_count());
  ASSERT_EQ(plot3d_colorizer_factory_count(), plot3d_get_colorizer_nr_for_test(w));
  widget_destroy(w);

  /* 裁剪场景：没注册配色插件时一个实例都不建。 */
  plot3d_unregister();
  ASSERT_EQ(RET_OK, plot3d_register());
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_EQ(0u, plot3d_get_colorizer_nr_for_test(w));

  widget_destroy(w);
}

TEST_F(Plot3dColorizerTest, expr_not_registered_rejects_color_props) {
  widget_t* w = NULL;
  plot3d_unregister();
  ASSERT_EQ(RET_OK, plot3d_register());

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_sample_color_expr(w, "z"));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_color_func(w, counting_color_func, NULL));

  widget_destroy(w);
}

/* 回调失败时插件必须一个字节都不改 out：两个调用点都不看 plot3d_eval_color_value 的返回值，
 * 「不改写 out」是回调失败唯一的表达方式。 */
TEST_F(Plot3dColorizerTest, color_func_failure_falls_back_to_colormap) {
  color_t expected;
  plot3d_data_point_t p;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "viridis"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(RET_OK, plot3d_set_color_func(w, failing_color_func, NULL));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));

  /* 4 个点的 z 是 1..4，第 0 个是最小值，归一化后落在配色表的起点。 */
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(expected.rgba.r, p.color.rgba.r);
  ASSERT_EQ(expected.rgba.g, p.color.rgba.g);
  ASSERT_EQ(expected.rgba.b, p.color.rgba.b);
  ASSERT_EQ(expected.rgba.a, p.color.rgba.a);

  widget_destroy(w);
}

/* is_active 由 TRUE 翻回 FALSE 后，配色要真的退回按 z 查 colormap，而不只是属性读回 NULL。 */
TEST_F(Plot3dColorizerTest, inactive_colorizer_falls_back_to_colormap) {
  color_t expected;
  plot3d_data_point_t p;
  widget_t* w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_OK, plot3d_set_colormap(w, "viridis"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, "\"#ff0000\""));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(0xff, p.color.rgba.r);

  ASSERT_EQ(RET_OK, plot3d_set_sample_color_expr(w, NULL));
  ASSERT_EQ(RET_OK, plot3d_colormap_get_color("viridis", 0.0f, &expected));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(expected.rgba.r, p.color.rgba.r);
  ASSERT_EQ(expected.rgba.g, p.color.rgba.g);
  ASSERT_EQ(expected.rgba.b, p.color.rgba.b);

  widget_destroy(w);
}

TEST_F(Plot3dColorizerFactoryTest, register_and_create) {
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));

  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);
  ASSERT_STREQ("fake", colorizer->vt->type);
  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, create_unregistered_returns_null) {
  ASSERT_EQ((plot3d_colorizer_t*)NULL, plot3d_colorizer_factory_create("fake"));
}

TEST_F(Plot3dColorizerFactoryTest, register_twice_keeps_one_entry) {
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable_v2));
  ASSERT_EQ(1u, plot3d_colorizer_factory_count());

  /* 同名注册必须替换成后注册的实现，用户才能用自己的插件覆盖内置插件。 */
  ASSERT_EQ(&s_fake_colorizer_vtable_v2, plot3d_colorizer_factory_get(0));

  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);
  ASSERT_EQ(&s_fake_colorizer_vtable_v2, colorizer->vt);
  ASSERT_EQ(FAKE_COLORIZER_V2_MARK, ((fake_colorizer_t*)colorizer)->fake_prop);
  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, deinit_clears_registry) {
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_deinit());
  ASSERT_EQ(0u, plot3d_colorizer_factory_count());
  /* 重复 deinit 是安全的：plot3d_unregister 可能被连着调用两次。 */
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_deinit());
}

TEST_F(Plot3dColorizerFactoryTest, register_requires_create_and_destroy) {
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_factory_register(NULL));

  s_fake_colorizer_vtable.create = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));

  fake_colorizer_vtable_init();
  s_fake_colorizer_vtable.destroy = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));

  fake_colorizer_vtable_init();
  s_fake_colorizer_vtable.type = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));

  ASSERT_EQ(0u, plot3d_colorizer_factory_count());
}

TEST_F(Plot3dColorizerFactoryTest, factory_get_out_of_range_returns_null) {
  ASSERT_EQ((const plot3d_colorizer_vtable_t*)NULL, plot3d_colorizer_factory_get(0));

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  ASSERT_EQ(&s_fake_colorizer_vtable, plot3d_colorizer_factory_get(0));
  ASSERT_EQ((const plot3d_colorizer_vtable_t*)NULL, plot3d_colorizer_factory_get(1));
}

TEST_F(Plot3dColorizerFactoryTest, factory_create_null_type_returns_null) {
  ASSERT_EQ((plot3d_colorizer_t*)NULL, plot3d_colorizer_factory_create(NULL));
}

TEST_F(Plot3dColorizerFactoryTest, sample_pos_init_clears_residue) {
  plot3d_sample_pos_t pos;

  /* 用 0xff 模拟上一次求值（可能来自曲线）留下的残留值。 */
  memset(&pos, 0xff, sizeof(pos));

  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));
  ASSERT_EQ(1.0f, pos.x);
  ASSERT_EQ(2.0f, pos.y);
  ASSERT_EQ(3.0f, pos.z);
  ASSERT_EQ(0.0f, pos.t);
  /* has_t 必须被清成 FALSE：漏清会让网格模式读到曲线残留的 t。 */
  ASSERT_EQ(FALSE, pos.has_t);

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_sample_pos_init(NULL, 1, 2, 3));
}

TEST_F(Plot3dColorizerFactoryTest, eval_forwards_to_plugin) {
  plot3d_sample_pos_t pos;
  plot3d_color_value_t cv;
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);

  ASSERT_EQ(TRUE, plot3d_colorizer_is_active(colorizer));
  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));
  cv.is_color = FALSE;
  cv.scalar = pos.z;
  ASSERT_EQ(RET_OK, plot3d_colorizer_eval(colorizer, &pos, &cv));
  ASSERT_EQ(TRUE, cv.is_color);
  ASSERT_EQ(FAKE_COLORIZER_R, cv.color.rgba.r);

  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, missing_vtable_members_are_not_impl) {
  value_t v;
  plot3d_sample_pos_t pos;
  plot3d_color_value_t cv;
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);
  /* fake 只缺 set_data。 */
  ASSERT_EQ(RET_NOT_IMPL, plot3d_colorizer_set_data(colorizer, "any-data", NULL, NULL));
  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_bare_colorizer_vtable));
  colorizer = plot3d_colorizer_factory_create("fake-bare");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));
  /* 全部可选成员缺失是合法配置：is_active 为 FALSE，核心据此跳过该插件。 */
  ASSERT_EQ(FALSE, plot3d_colorizer_is_active(colorizer));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_colorizer_eval(colorizer, &pos, &cv));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_colorizer_set_prop(colorizer, "fake-prop", &v));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_colorizer_get_prop(colorizer, "fake-prop", &v));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_colorizer_set_data(colorizer, "any-data", NULL, NULL));

  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, prop_forwarding) {
  value_t v;
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);

  value_set_int(&v, 8);
  ASSERT_EQ(RET_OK, plot3d_colorizer_set_prop(colorizer, "fake-prop", &v));
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_colorizer_get_prop(colorizer, "fake-prop", &v));
  ASSERT_EQ(8, value_int(&v));

  /* 非法值被拒绝后原值不变。 */
  value_set_int(&v, -1);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_prop(colorizer, "fake-prop", &v));
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_colorizer_get_prop(colorizer, "fake-prop", &v));
  ASSERT_EQ(8, value_int(&v));

  /* 不认识的属性返回 RET_NOT_FOUND，核心据此继续按自己的属性处理。 */
  value_set_int(&v, 1);
  ASSERT_EQ(RET_NOT_FOUND, plot3d_colorizer_set_prop(colorizer, "no-such-prop", &v));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_colorizer_get_prop(colorizer, "no-such-prop", &v));

  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, null_colorizer_returns_bad_params) {
  value_t v;
  plot3d_sample_pos_t pos;
  plot3d_color_value_t cv;

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));

  ASSERT_EQ(FALSE, plot3d_colorizer_is_active(NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_eval(NULL, &pos, &cv));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_prop(NULL, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_get_prop(NULL, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_data(NULL, "any-data", NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_destroy(NULL));
}

TEST_F(Plot3dColorizerFactoryTest, null_vtable_returns_bad_params) {
  value_t v;
  plot3d_sample_pos_t pos;
  plot3d_color_value_t cv;
  plot3d_colorizer_t colorizer = {NULL};

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));

  ASSERT_EQ(FALSE, plot3d_colorizer_is_active(&colorizer));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_eval(&colorizer, &pos, &cv));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_prop(&colorizer, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_get_prop(&colorizer, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_data(&colorizer, "any-data", NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_destroy(&colorizer));
}

TEST_F(Plot3dColorizerFactoryTest, bad_params_of_prop_forwarding) {
  value_t v;
  plot3d_sample_pos_t pos;
  plot3d_color_value_t cv;
  plot3d_colorizer_t* colorizer = NULL;

  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  colorizer = plot3d_colorizer_factory_create("fake");
  ASSERT_NE((plot3d_colorizer_t*)NULL, colorizer);

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_sample_pos_init(&pos, 1, 2, 3));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_prop(colorizer, NULL, &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_prop(colorizer, "fake-prop", NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_get_prop(colorizer, NULL, &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_get_prop(colorizer, "fake-prop", NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_set_data(colorizer, NULL, NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_eval(colorizer, NULL, &cv));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_colorizer_eval(colorizer, &pos, NULL));

  ASSERT_EQ(RET_OK, plot3d_colorizer_destroy(colorizer));
}

/* 多个配色插件同时生效时取第一个生效的（顺序即注册顺序）：这条优先级决定了用户注册的插件
 * 与内置插件谁说了算。 */
TEST_F(Plot3dColorizerFactoryTest, first_active_colorizer_wins) {
  value_t v;
  widget_t* w = NULL;
  plot3d_data_point_t p;

  ASSERT_EQ(RET_OK, plot3d_register());
  /* 本 fixture 从空注册表起步，而喂矩阵数据要经 matrix 数据源插件，得自己把它注册上；
   * GRID 铺点还需至少一个图型插件（默认 plottype=dot）。 */
  ASSERT_EQ(RET_OK, plot3d_source_matrix_register());
  ASSERT_EQ(RET_OK, plot3d_type_dot_register());
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake_colorizer_vtable));
  ASSERT_EQ(RET_OK, plot3d_colorizer_factory_register(&s_fake2_colorizer_vtable));

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(2u, plot3d_get_colorizer_nr_for_test(w));

  ASSERT_EQ(RET_OK, plot3d_set_sample_z_matrix(w, "1,2;3,4"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(FAKE_COLORIZER_R, p.color.rgba.r);

  /* 取的是「第一个生效的」而不是「第一个」：先注册的那个失效后，轮到后面的。 */
  value_set_bool(&v, FALSE);
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, "fake-active", &v));
  ASSERT_EQ(RET_OK, plot3d_get_data_point(w, 0, &p));
  ASSERT_EQ(FAKE2_COLORIZER_R, p.color.rgba.r);

  widget_destroy(w);
}
