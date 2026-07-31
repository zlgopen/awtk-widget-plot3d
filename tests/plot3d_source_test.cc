#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "base/widget_factory.h"
#include "plot3d/plot3d.h"
#include "plot3d/colorizer/plot3d_colorizer.h"
#include "plot3d/source/plot3d_source.h"

namespace {

typedef struct _fake_source_t {
  plot3d_source_t source;
  int32_t fake_prop;
} fake_source_t;

static plot3d_source_t* fake_source_create(void);
static plot3d_source_t* fake_source_create_v2(void);
static plot3d_source_t* fake2_source_create(void);
static plot3d_source_t* fake_color_source_create(void);

/* fake2 把收到的值加上这个偏移再存，两个认领者的状态才可区分。 */
#define FAKE2_PROP_MARK 1000
/* 比 fake 更严的上限，用来构造「一个认领者接受、另一个拒绝」。 */
#define FAKE2_MAX_PROP 100

static ret_t fake_source_sample(plot3d_source_t* source, plot3d_source_result_t* result) {
  result->type = PLOT3D_SOURCE_RESULT_POINTS;

  return RET_OK;
}

static ret_t fake_source_set_prop(plot3d_source_t* source, const char* name, const value_t* v) {
  fake_source_t* fake = (fake_source_t*)source;

  if (tk_str_eq(name, "fake-prop")) {
    int32_t value = value_int(v);
    /* 先校验后落值：负数拒绝，原值不变。 */
    return_value_if_fail(value >= 0, RET_BAD_PARAMS);
    fake->fake_prop = value;

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t fake_source_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  fake_source_t* fake = (fake_source_t*)source;

  if (tk_str_eq(name, "fake-prop")) {
    value_set_int(v, fake->fake_prop);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

/* fake2 与 fake 认领同一个 "fake-prop"，用来钉住广播「遍历完全部实例」与「拒绝优先」两条语义。 */
static ret_t fake2_source_set_prop(plot3d_source_t* source, const char* name, const value_t* v) {
  fake_source_t* fake = (fake_source_t*)source;

  if (tk_str_eq(name, "fake-prop")) {
    int32_t value = value_int(v);
    return_value_if_fail(value >= 0 && value <= FAKE2_MAX_PROP, RET_BAD_PARAMS);
    fake->fake_prop = value + FAKE2_PROP_MARK;

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t fake2_source_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  fake_source_t* fake = (fake_source_t*)source;

  /* 同时认领 "fake-prop"（供用例验证 get 取的是第一个认领者）与只有自己认领的 "fake2-prop"
   * （供用例旁路观察自己那份值）。 */
  if (tk_str_eq(name, "fake-prop") || tk_str_eq(name, "fake2-prop")) {
    value_set_int(v, fake->fake_prop);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

/* 认领 sample-color-expr——它同时也被 colorizer 认领，于是可以凑出「跨类共同认领」的场景。
 * 这里不校验表达式：让「数据源接受、colorizer 拒绝」成为可构造的组合。 */
static ret_t fake_color_source_set_prop(plot3d_source_t* source, const char* name,
                                        const value_t* v) {
  fake_source_t* fake = (fake_source_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_COLOR_EXPR)) {
    fake->fake_prop++;

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t fake_source_destroy(plot3d_source_t* source) {
  TKMEM_FREE(source);

  return RET_OK;
}

/* 不用指定初始化器：MSVC 在 C++20 之前不支持。
 * set_data / has_data / reset 特意留空，用来验证转发函数在成员缺失时的返回值。 */
static plot3d_source_vtable_t s_fake_source_vtable;

/* type 与上面相同、create 不同，用来验证同名注册是「替换」而不是「忽略」。 */
static plot3d_source_vtable_t s_fake_source_vtable_v2;

/* type 与上面不同（同 type 是替换而不是新增），与 s_fake_source_vtable 同时注册可凑出两个认领者。 */
static plot3d_source_vtable_t s_fake2_source_vtable;

/* 与 colorizer 认领同一个属性，用来验证跨类分发的重采样次数。 */
static plot3d_source_vtable_t s_fake_color_source_vtable;

static void fake_source_vtable_init(void) {
  memset(&s_fake_source_vtable, 0x00, sizeof(s_fake_source_vtable));
  s_fake_source_vtable.type = "fake";
  s_fake_source_vtable.create = fake_source_create;
  s_fake_source_vtable.sample = fake_source_sample;
  s_fake_source_vtable.set_prop = fake_source_set_prop;
  s_fake_source_vtable.get_prop = fake_source_get_prop;
  s_fake_source_vtable.destroy = fake_source_destroy;

  memcpy(&s_fake_source_vtable_v2, &s_fake_source_vtable, sizeof(s_fake_source_vtable_v2));
  s_fake_source_vtable_v2.create = fake_source_create_v2;

  memcpy(&s_fake2_source_vtable, &s_fake_source_vtable, sizeof(s_fake2_source_vtable));
  s_fake2_source_vtable.type = "fake2";
  s_fake2_source_vtable.create = fake2_source_create;
  s_fake2_source_vtable.set_prop = fake2_source_set_prop;
  s_fake2_source_vtable.get_prop = fake2_source_get_prop;

  memcpy(&s_fake_color_source_vtable, &s_fake_source_vtable, sizeof(s_fake_color_source_vtable));
  s_fake_color_source_vtable.type = "fake-color";
  s_fake_color_source_vtable.create = fake_color_source_create;
  s_fake_color_source_vtable.set_prop = fake_color_source_set_prop;
  s_fake_color_source_vtable.get_prop = NULL;
}

#define FAKE_SOURCE_V2_MARK 12345

static plot3d_source_t* fake_source_create(void) {
  fake_source_t* fake = TKMEM_ZALLOC(fake_source_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->source.vt = &s_fake_source_vtable;

  return (plot3d_source_t*)fake;
}

static plot3d_source_t* fake_source_create_v2(void) {
  fake_source_t* fake = TKMEM_ZALLOC(fake_source_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->source.vt = &s_fake_source_vtable_v2;
  fake->fake_prop = FAKE_SOURCE_V2_MARK;

  return (plot3d_source_t*)fake;
}

static plot3d_source_t* fake2_source_create(void) {
  fake_source_t* fake = TKMEM_ZALLOC(fake_source_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->source.vt = &s_fake2_source_vtable;

  return (plot3d_source_t*)fake;
}

static plot3d_source_t* fake_color_source_create(void) {
  fake_source_t* fake = TKMEM_ZALLOC(fake_source_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->source.vt = &s_fake_color_source_vtable;

  return (plot3d_source_t*)fake;
}

/* 注册表是进程内单例，用例清空后必须复原成全量内置插件已注册的状态，否则会污染
 * plot3d_test.cc 里的采样用例（tests/SConscript 用 Glob 收集，跨 TU 执行顺序不可控）。
 * gtest 的 ASSERT_* 失败会立即 return，所以复原只能放 TearDown。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dSourceFactoryTest : public testing::Test {
 protected:
  void SetUp() {
    fake_source_vtable_init();
    plot3d_unregister();
  }
  void TearDown() {
    ASSERT_EQ(RET_OK, plot3d_unregister());
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }
};

}  // namespace

TEST_F(Plot3dSourceFactoryTest, register_only_registers_widget) {
  /* SetUp 刚做过 plot3d_unregister()，控件此刻应当已不可创建。 */
  ASSERT_EQ((tk_create_t)NULL, general_factory_find(widget_factory(), WIDGET_TYPE_PLOT3D));

  /* plot3d_register 是给裁剪场景用的，只能注册控件，不能顺带注册插件，
   * 否则按需注册就落空了。 */
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_NE((tk_create_t)NULL, general_factory_find(widget_factory(), WIDGET_TYPE_PLOT3D));
  ASSERT_EQ(0u, plot3d_source_factory_count());
}

TEST_F(Plot3dSourceFactoryTest, register_and_create) {
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));

  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);
  ASSERT_STREQ("fake", source->vt->type);
  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, create_unregistered_returns_null) {
  ASSERT_EQ((plot3d_source_t*)NULL, plot3d_source_factory_create("fake"));
}

TEST_F(Plot3dSourceFactoryTest, register_twice_keeps_one_entry) {
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable_v2));
  ASSERT_EQ(1u, plot3d_source_factory_count());

  /* 同名注册必须替换成后注册的实现，用户才能用自己的插件覆盖内置插件。 */
  ASSERT_EQ(&s_fake_source_vtable_v2, plot3d_source_factory_get(0));

  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);
  ASSERT_EQ(&s_fake_source_vtable_v2, source->vt);
  ASSERT_EQ(FAKE_SOURCE_V2_MARK, ((fake_source_t*)source)->fake_prop);
  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, deinit_clears_registry) {
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_deinit());
  ASSERT_EQ(0u, plot3d_source_factory_count());
}

TEST_F(Plot3dSourceFactoryTest, register_requires_create_and_destroy) {
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_factory_register(NULL));

  s_fake_source_vtable.create = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_factory_register(&s_fake_source_vtable));

  fake_source_vtable_init();
  s_fake_source_vtable.destroy = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_factory_register(&s_fake_source_vtable));

  fake_source_vtable_init();
  s_fake_source_vtable.type = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_factory_register(&s_fake_source_vtable));

  ASSERT_EQ(0u, plot3d_source_factory_count());
}

TEST_F(Plot3dSourceFactoryTest, factory_get_out_of_range_returns_null) {
  ASSERT_EQ((const plot3d_source_vtable_t*)NULL, plot3d_source_factory_get(0));

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(&s_fake_source_vtable, plot3d_source_factory_get(0));
  ASSERT_EQ((const plot3d_source_vtable_t*)NULL, plot3d_source_factory_get(1));
}

TEST_F(Plot3dSourceFactoryTest, factory_create_null_type_returns_null) {
  ASSERT_EQ((plot3d_source_t*)NULL, plot3d_source_factory_create(NULL));
}

TEST_F(Plot3dSourceFactoryTest, result_init_clears_residue) {
  plot3d_source_result_t result;
  darray_t points;

  darray_init(&points, 0, NULL, NULL);
  /* 用 0xff 模拟上一次采样（可能是另一种形态）留下的残留值。 */
  memset(&result, 0xff, sizeof(result));

  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, &points));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_POINTS, result.type);
  ASSERT_EQ(&points, result.points);
  ASSERT_EQ((const float_t*)NULL, result.ts);
  ASSERT_EQ((const float_t*)NULL, result.zs);
  ASSERT_EQ(0u, result.cols);
  ASSERT_EQ(0u, result.rows);
  ASSERT_EQ(0.0f, result.x0);
  ASSERT_EQ(0.0f, result.x1);
  ASSERT_EQ(0.0f, result.y0);
  ASSERT_EQ(0.0f, result.y1);
  ASSERT_EQ(FALSE, result.has_color);

  darray_deinit(&points);
}

TEST_F(Plot3dSourceFactoryTest, result_init_accepts_null_points) {
  plot3d_source_result_t result;

  memset(&result, 0xff, sizeof(result));

  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, NULL));
  ASSERT_EQ((darray_t*)NULL, result.points);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_result_init(NULL, NULL));
}

TEST_F(Plot3dSourceFactoryTest, sample_forwards_to_plugin) {
  plot3d_source_result_t result;
  darray_t points;
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);

  darray_init(&points, 0, NULL, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, &points));
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_POINTS, result.type);

  darray_deinit(&points);
  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, missing_vtable_members_are_not_impl) {
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);

  ASSERT_EQ(RET_NOT_IMPL, plot3d_source_set_data(source, "any-data", NULL, NULL));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_source_reset(source));
  ASSERT_EQ(FALSE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, prop_forwarding) {
  value_t v;
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);

  value_set_int(&v, 8);
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, "fake-prop", &v));
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, "fake-prop", &v));
  ASSERT_EQ(8, value_int(&v));

  /* 非法值被拒绝后原值不变。 */
  value_set_int(&v, -1);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, "fake-prop", &v));
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, "fake-prop", &v));
  ASSERT_EQ(8, value_int(&v));

  /* 不认识的属性返回 RET_NOT_FOUND，核心据此继续按自己的属性处理。 */
  value_set_int(&v, 1);
  ASSERT_EQ(RET_NOT_FOUND, plot3d_source_set_prop(source, "no-such-prop", &v));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_source_get_prop(source, "no-such-prop", &v));

  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, null_source_returns_bad_params) {
  value_t v;
  plot3d_source_result_t result;

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, NULL));

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_sample(NULL, &result));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(NULL, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_get_prop(NULL, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_data(NULL, "any-data", NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_reset(NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_destroy(NULL));
  ASSERT_EQ(FALSE, plot3d_source_has_data(NULL));
}

TEST_F(Plot3dSourceFactoryTest, null_vtable_returns_bad_params) {
  value_t v;
  plot3d_source_result_t result;
  plot3d_source_t source = {NULL};

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, NULL));

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_sample(&source, &result));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(&source, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_get_prop(&source, "fake-prop", &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_data(&source, "any-data", NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_reset(&source));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_destroy(&source));
  ASSERT_EQ(FALSE, plot3d_source_has_data(&source));
}

TEST_F(Plot3dSourceFactoryTest, bad_params_of_prop_forwarding) {
  value_t v;
  plot3d_source_t* source = NULL;

  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  source = plot3d_source_factory_create("fake");
  ASSERT_NE((plot3d_source_t*)NULL, source);

  value_set_int(&v, 0);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, NULL, &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, "fake-prop", NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_get_prop(source, NULL, &v));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_get_prop(source, "fake-prop", NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_data(source, NULL, NULL, NULL));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_sample(source, NULL));

  ASSERT_EQ(RET_OK, plot3d_source_destroy(source));
}

TEST_F(Plot3dSourceFactoryTest, instances_created_for_each_registered_source) {
  widget_t* w = NULL;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake2_source_vtable));

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(2u, plot3d_get_source_nr_for_test(w));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, set_prop_dispatched_to_plugin) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  value_set_int(&v, 5);
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, "fake-prop", &v));

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake-prop", &v));
  ASSERT_EQ(5, value_int(&v));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, set_prop_dispatched_to_every_claimer) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake2_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  value_set_int(&v, 5);
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, "fake-prop", &v));

  /* 第一个认领者收到了值。 */
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake-prop", &v));
  ASSERT_EQ(5, value_int(&v));

  /* 第二个认领者也必须收到：广播不能命中一个就提前返回。 */
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake2-prop", &v));
  ASSERT_EQ(5 + FAKE2_PROP_MARK, value_int(&v));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, get_prop_returns_first_claimer) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake2_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  value_set_int(&v, 5);
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, "fake-prop", &v));

  /* 两个插件都认领 "fake-prop"，get 取先注册的那个（fake），而不是 fake2 的带偏移值。 */
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake-prop", &v));
  ASSERT_EQ(5, value_int(&v));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, plugin_rejects_invalid_value_and_keeps_old) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  value_set_int(&v, 5);
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, "fake-prop", &v));
  value_set_int(&v, -1);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_prop_for_test(w, "fake-prop", &v));

  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake-prop", &v));
  ASSERT_EQ(5, value_int(&v));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, rejection_wins_over_acceptance) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake2_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  /* fake 接受、fake2 因超上限拒绝：只要有一个认领者拒绝，广播就必须返回 RET_BAD_PARAMS。 */
  value_set_int(&v, FAKE2_MAX_PROP + 1);
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_prop_for_test(w, "fake-prop", &v));

  /* 记录当前的无回滚语义：先接受的 fake 已经落了新值，广播并不会把它撤回。 */
  value_set_int(&v, 0);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, "fake-prop", &v));
  ASSERT_EQ(FAKE2_MAX_PROP + 1, value_int(&v));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, unknown_prop_returns_not_found) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  /* 注册插件后再测，才能覆盖「插件返回 RET_NOT_FOUND 后核心继续返回 RET_NOT_FOUND」这条链路，
   * 而不是只跑一遍空循环。 */
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  value_set_int(&v, 1);
  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_prop_for_test(w, "no-such-prop", &v));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_get_prop_for_test(w, "no-such-prop", &v));

  widget_destroy(w);
}

/* 跨类认领：sample-color-expr 同时被 fake-color 数据源与内置 colorizer 认领，两类都要收到值，
 * 但重采样只能做一次——重采样属于「问完所有插件之后」的动作，不能挂在每一类的分发里。 */
TEST_F(Plot3dSourceFactoryTest, cross_class_claimers_resample_once) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_color_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_colorizer_expr_register());

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(1u, plot3d_get_source_nr_for_test(w));
  ASSERT_EQ(1u, plot3d_get_colorizer_nr_for_test(w));
  ASSERT_EQ(0u, plot3d_get_resample_nr_for_test(w));

  value_set_str(&v, "z * 2");
  ASSERT_EQ(RET_OK, plot3d_set_prop_for_test(w, PLOT3D_PROP_SAMPLE_COLOR_EXPR, &v));

  /* 数据源那一份也确实收到了值（它只在被认领时自增）。 */
  ASSERT_EQ(1, ((fake_source_t*)darray_get(&(PLOT3D(w)->sources), 0))->fake_prop);
  ASSERT_EQ(1u, plot3d_get_resample_nr_for_test(w));

  widget_destroy(w);
}

/* 一类接受、另一类拒绝时按拒绝论处，且一次都不能重采样：否则控件会停在「已按新值重采过、
 * 调用方却收到 RET_BAD_PARAMS」的矛盾状态。 */
TEST_F(Plot3dSourceFactoryTest, cross_class_rejection_does_not_resample) {
  widget_t* w = NULL;
  value_t v;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_color_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_colorizer_expr_register());

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  /* 语法非法：fake-color 数据源不校验、照单全收，colorizer 拒绝。 */
  value_set_str(&v, "if(");
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_prop_for_test(w, PLOT3D_PROP_SAMPLE_COLOR_EXPR, &v));
  ASSERT_EQ(0u, plot3d_get_resample_nr_for_test(w));

  widget_destroy(w);
}

/* widget_clone 是唯一会把「建全实例 + 逐属性 get/set + 二次 deinit」串起来跑的真实路径。
 * 注意 "fake-prop" 不在 s_plot3d_properties[] 里，属性值本身克隆不过去，「插件属性随 clone
 * 复制」要等属性真正迁进插件之后才能验证。 */
TEST_F(Plot3dSourceFactoryTest, clone_creates_its_own_sources) {
  widget_t* w = NULL;
  widget_t* clone = NULL;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake_source_vtable));
  ASSERT_EQ(RET_OK, plot3d_source_factory_register(&s_fake2_source_vtable));

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  clone = widget_clone(w, NULL);
  ASSERT_NE((widget_t*)NULL, clone);
  ASSERT_EQ(plot3d_get_source_nr_for_test(w), plot3d_get_source_nr_for_test(clone));

  widget_destroy(clone);
  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, prop_order_independent) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register_all());
  w = plot3d_create(NULL, 0, 0, 320, 240);

  /* 先设采样属性、后设模式，属性仍生效。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_x_range(w, "0,10"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_steps(w, "3,3"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_z_expr(w, "x + y"));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(9u, plot3d_get_data_points_nr(w));

  widget_destroy(w);
}

TEST_F(Plot3dSourceFactoryTest, register_all_registers_builtin_sources) {
  ASSERT_EQ(RET_OK, plot3d_register_all());
  ASSERT_EQ(4u, plot3d_source_factory_count());
}

TEST_F(Plot3dSourceFactoryTest, unregistered_source_mode_returns_not_found) {
  widget_t* w = NULL;
  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_source_matrix_register());
  w = plot3d_create(NULL, 0, 0, 320, 240);

  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_GRID));
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_MATRIX));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_sample_mode(w, "no-such-mode"));
  /* none 豁免注册表检查：裁掉 csv 仍能切回 none。 */
  ASSERT_EQ(RET_OK, plot3d_set_sample_mode(w, PLOT3D_SAMPLE_MODE_NONE));

  widget_destroy(w);
}
