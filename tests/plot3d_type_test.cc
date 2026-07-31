#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "plot3d/plot3d.h"
#include "plot3d/type/plot3d_type.h"

namespace {

typedef struct _fake_type_t {
  plot3d_type_t type;
  int32_t fake_prop;
} fake_type_t;

static plot3d_type_t* fake_type_create(void);
static plot3d_type_t* fake_type_create_v2(void);

#define FAKE_TYPE_V2_MARK 12345

static ret_t fake_type_destroy(plot3d_type_t* type) {
  TKMEM_FREE(type);

  return RET_OK;
}

/* 不用指定初始化器：MSVC 在 C++20 之前不支持。
 * layout_grid / build / on_paint / set_prop / get_prop 特意留空，用来验证转发函数在成员缺失时
 * 的返回值。 */
static plot3d_type_vtable_t s_fake_type_vtable;

/* type 与上面相同、create 不同，用来验证同名注册是「替换」而不是「忽略」。 */
static plot3d_type_vtable_t s_fake_type_vtable_v2;

static void fake_type_vtable_init(void) {
  memset(&s_fake_type_vtable, 0x00, sizeof(s_fake_type_vtable));
  s_fake_type_vtable.type = "fake";
  s_fake_type_vtable.create = fake_type_create;
  s_fake_type_vtable.destroy = fake_type_destroy;

  memcpy(&s_fake_type_vtable_v2, &s_fake_type_vtable, sizeof(s_fake_type_vtable_v2));
  s_fake_type_vtable_v2.create = fake_type_create_v2;
}

static plot3d_type_t* fake_type_create(void) {
  fake_type_t* fake = TKMEM_ZALLOC(fake_type_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->type.vt = &s_fake_type_vtable;

  return (plot3d_type_t*)fake;
}

static plot3d_type_t* fake_type_create_v2(void) {
  fake_type_t* fake = TKMEM_ZALLOC(fake_type_t);
  return_value_if_fail(fake != NULL, NULL);
  fake->type.vt = &s_fake_type_vtable_v2;
  fake->fake_prop = FAKE_TYPE_V2_MARK;

  return (plot3d_type_t*)fake;
}

/* 注册表是进程内单例，用例清空后必须复原成全量内置插件已注册的状态，否则会污染其它 TU。
 * gtest 的 ASSERT_* 失败会立即 return，所以复原只能放 TearDown。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dTypeFactoryTest : public testing::Test {
 protected:
  void SetUp() {
    fake_type_vtable_init();
    plot3d_unregister();
  }
  void TearDown() {
    ASSERT_EQ(RET_OK, plot3d_unregister());
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }
};

}  // namespace

TEST_F(Plot3dTypeFactoryTest, register_all_registers_builtin_types) {
  plot3d_type_t* line = NULL;
  plot3d_type_t* dot = NULL;
  plot3d_type_t* surface = NULL;
  plot3d_type_t* cylinder = NULL;

  ASSERT_EQ(RET_OK, plot3d_register_all());

  line = plot3d_type_factory_create("line");
  dot = plot3d_type_factory_create("dot");
  surface = plot3d_type_factory_create("surface");
  cylinder = plot3d_type_factory_create("cylinder");

  ASSERT_NE((plot3d_type_t*)NULL, line);
  ASSERT_NE((plot3d_type_t*)NULL, dot);
  ASSERT_NE((plot3d_type_t*)NULL, surface);
  ASSERT_NE((plot3d_type_t*)NULL, cylinder);

  ASSERT_EQ(RET_OK, plot3d_type_destroy(line));
  ASSERT_EQ(RET_OK, plot3d_type_destroy(dot));
  ASSERT_EQ(RET_OK, plot3d_type_destroy(surface));
  ASSERT_EQ(RET_OK, plot3d_type_destroy(cylinder));
}


TEST_F(Plot3dTypeFactoryTest, instances_created_for_each_registered_type) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(1u, plot3d_get_type_nr_for_test(w));

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, set_plottype_registered_but_missing_instance_returns_not_found) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register());
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(0u, plot3d_get_type_nr_for_test(w));

  ASSERT_EQ(RET_OK, plot3d_type_line_register());
  ASSERT_EQ(RET_OK, plot3d_type_dot_register());
  ASSERT_EQ(RET_OK, plot3d_type_surface_register());
  ASSERT_EQ(RET_OK, plot3d_type_cylinder_register());

  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_plottype(w, "dot"));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_plottype(w, "surface"));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_plottype(w, "cylinder"));

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, set_plottype_unknown_returns_not_found) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register_all());
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  ASSERT_EQ(RET_NOT_FOUND, plot3d_set_plottype(w, "no-such"));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_plottype(w, ""));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_plottype(w, NULL));

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, set_plottype_accepts_registered_custom_type) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register());
  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));

  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(1u, plot3d_get_type_nr_for_test(w));

  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "fake"));

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, point_size_and_line_width_follow_type_plugins) {
  value_t v;
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register_all());
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);

  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_NEAR(4.0f, value_float(&v), 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_NEAR(1.5f, value_float(&v), 0.001f);

  ASSERT_EQ(RET_OK, plot3d_set_point_size(w, 6.0f));
  ASSERT_EQ(RET_OK, plot3d_set_line_width(w, 2.0f));
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_NEAR(6.0f, value_float(&v), 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_NEAR(2.0f, value_float(&v), 0.001f);

  /* 切换图型后值仍在：四个内置插件都认领并广播同步。 */
  ASSERT_EQ(RET_OK, plot3d_set_plottype(w, "line"));
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_NEAR(6.0f, value_float(&v), 0.001f);
  ASSERT_EQ(RET_OK, plot3d_get_prop_for_test(w, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_NEAR(2.0f, value_float(&v), 0.001f);

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, point_size_without_type_plugin_returns_bad_params) {
  widget_t* w = NULL;

  ASSERT_EQ(RET_OK, plot3d_register());
  w = plot3d_create(NULL, 0, 0, 320, 240);
  ASSERT_NE((widget_t*)NULL, w);
  ASSERT_EQ(0u, plot3d_get_type_nr_for_test(w));

  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_point_size(w, 6.0f));
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_set_line_width(w, 2.0f));

  widget_destroy(w);
}

TEST_F(Plot3dTypeFactoryTest, shared_style_prop_helpers) {
  value_t v;
  plot3d_type_t* type = NULL;

  ASSERT_EQ(RET_OK, plot3d_type_line_register());
  type = plot3d_type_factory_create("line");
  ASSERT_NE((plot3d_type_t*)NULL, type);

  plot3d_type_init_style(type);
  ASSERT_EQ(RET_OK, plot3d_type_get_style_prop(type, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_NEAR(4.0f, value_float(&v), 0.001f);
  ASSERT_EQ(RET_OK, plot3d_type_get_style_prop(type, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_NEAR(1.5f, value_float(&v), 0.001f);

  value_set_float(&v, 0.5f);
  ASSERT_EQ(RET_OK, plot3d_type_set_style_prop(type, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_EQ(RET_OK, plot3d_type_get_style_prop(type, PLOT3D_PROP_POINT_SIZE, &v));
  ASSERT_NEAR(1.0f, value_float(&v), 0.001f);

  value_set_float(&v, 3.0f);
  ASSERT_EQ(RET_OK, plot3d_type_set_style_prop(type, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_EQ(RET_OK, plot3d_type_get_style_prop(type, PLOT3D_PROP_LINE_WIDTH, &v));
  ASSERT_NEAR(3.0f, value_float(&v), 0.001f);

  value_set_int(&v, 1);
  ASSERT_EQ(RET_NOT_FOUND, plot3d_type_set_style_prop(type, "other", &v));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_type_get_style_prop(type, "other", &v));

  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}

TEST_F(Plot3dTypeFactoryTest, register_and_create) {
  plot3d_type_t* type = NULL;

  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));

  type = plot3d_type_factory_create("fake");
  ASSERT_NE((plot3d_type_t*)NULL, type);
  ASSERT_STREQ("fake", type->vt->type);
  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}

TEST_F(Plot3dTypeFactoryTest, create_unregistered_returns_null) {
  ASSERT_EQ((plot3d_type_t*)NULL, plot3d_type_factory_create("fake"));
}

TEST_F(Plot3dTypeFactoryTest, register_twice_keeps_one_entry) {
  plot3d_type_t* type = NULL;

  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));
  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable_v2));
  ASSERT_EQ(1u, plot3d_type_factory_count());

  /* 同名注册必须替换成后注册的实现，用户才能用自己的插件覆盖内置插件。 */
  ASSERT_EQ(&s_fake_type_vtable_v2, plot3d_type_factory_get(0));

  type = plot3d_type_factory_create("fake");
  ASSERT_NE((plot3d_type_t*)NULL, type);
  ASSERT_EQ(&s_fake_type_vtable_v2, type->vt);
  ASSERT_EQ(FAKE_TYPE_V2_MARK, ((fake_type_t*)type)->fake_prop);
  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}

TEST_F(Plot3dTypeFactoryTest, deinit_clears_registry) {
  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));
  ASSERT_EQ(RET_OK, plot3d_type_factory_deinit());
  ASSERT_EQ(0u, plot3d_type_factory_count());
  /* 重复 deinit 是安全的：plot3d_unregister 可能被连着调用两次。 */
  ASSERT_EQ(RET_OK, plot3d_type_factory_deinit());
}

TEST_F(Plot3dTypeFactoryTest, unregister_clears_type_factory) {
  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));
  ASSERT_EQ(1u, plot3d_type_factory_count());
  ASSERT_EQ(RET_OK, plot3d_unregister());
  ASSERT_EQ(0u, plot3d_type_factory_count());
}

TEST_F(Plot3dTypeFactoryTest, register_requires_create_and_destroy) {
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_type_factory_register(NULL));

  s_fake_type_vtable.create = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_type_factory_register(&s_fake_type_vtable));

  fake_type_vtable_init();
  s_fake_type_vtable.destroy = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_type_factory_register(&s_fake_type_vtable));

  fake_type_vtable_init();
  s_fake_type_vtable.type = NULL;
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_type_factory_register(&s_fake_type_vtable));

  ASSERT_EQ(0u, plot3d_type_factory_count());
}

TEST_F(Plot3dTypeFactoryTest, factory_get_out_of_range_returns_null) {
  ASSERT_EQ((const plot3d_type_vtable_t*)NULL, plot3d_type_factory_get(0));

  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));
  ASSERT_EQ(&s_fake_type_vtable, plot3d_type_factory_get(0));
  ASSERT_EQ((const plot3d_type_vtable_t*)NULL, plot3d_type_factory_get(1));
}

TEST_F(Plot3dTypeFactoryTest, factory_create_null_type_returns_null) {
  ASSERT_EQ((plot3d_type_t*)NULL, plot3d_type_factory_create(NULL));
}

TEST_F(Plot3dTypeFactoryTest, missing_vtable_members_are_not_impl) {
  value_t v;
  plot3d_type_t* type = NULL;
  plot3d_paint_ctx_t ctx;
  plot3d_source_result_t result;
  darray_t points;
  darray_t primitives;

  memset(&ctx, 0x00, sizeof(ctx));
  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, NULL));
  darray_init(&points, 0, NULL, NULL);
  darray_init(&primitives, 0, NULL, NULL);

  ASSERT_EQ(RET_OK, plot3d_type_factory_register(&s_fake_type_vtable));
  type = plot3d_type_factory_create("fake");
  ASSERT_NE((plot3d_type_t*)NULL, type);

  value_set_int(&v, 0);
  ASSERT_EQ(RET_NOT_IMPL, plot3d_type_layout_grid(type, &ctx, &result, &points));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_type_build(type, &ctx, &primitives));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_type_on_paint(type, NULL, NULL));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_type_set_prop(type, "fake-prop", &v));
  ASSERT_EQ(RET_NOT_IMPL, plot3d_type_get_prop(type, "fake-prop", &v));

  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
  darray_deinit(&points);
  darray_deinit(&primitives);
}


TEST_F(Plot3dTypeFactoryTest, primitive_type_exposed_in_public_header) {
  plot3d_primitive_t primitive;

  memset(&primitive, 0x00, sizeof(primitive));
  primitive.type = PLOT3D_PRIMITIVE_LINE;
  primitive.i0 = 1;
  primitive.i1 = 2;

  ASSERT_EQ(PLOT3D_PRIMITIVE_LINE, primitive.type);
  ASSERT_EQ(1u, primitive.i0);
  ASSERT_EQ(2u, primitive.i1);
}
