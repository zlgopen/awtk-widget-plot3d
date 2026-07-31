#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "tkc/utils.h"
#include "plot3d_register.h"
#include "plot3d/plot3d.h"
#include "plot3d/source/plot3d_source.h"

namespace {

/* csv 插件自身的用例：SetUp 建实例与装点的数组，TearDown 一并销毁。
 *
 * 只注册 csv 一个插件而不是 plot3d_register_all()：后者一旦有别的插件注册失败，会把本组
 * 用例一起拖红，掩盖真正的失败点。
 *
 * 注册表是进程内单例，用例改动后必须复原成全量内置插件已注册的状态，否则会污染其它 TU 的用例
 * （tests/SConscript 用 Glob 收集，跨 TU 执行顺序不可控）。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dSourceCsvTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    ASSERT_EQ(RET_OK, plot3d_source_csv_register());
    source = plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_NONE);
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

  ret_t set_dataset(const char* text) {
    value_t v;

    value_set_str(&v, text);

    return plot3d_source_set_prop(source, PLOT3D_PROP_DATASET, &v);
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

}  // namespace

TEST_F(Plot3dSourceCsvTest, csv_sample_outputs_points) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n4,5,6,#445566\n"));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_POINTS, result.type);
  /* csv 每行显式给颜色，核心据此跳过配色。 */
  ASSERT_EQ(TRUE, result.has_color);
  ASSERT_EQ(2u, points.size);

  ASSERT_NEAR(1.0f, point_at(0)->x, 0.001f);
  ASSERT_NEAR(2.0f, point_at(0)->y, 0.001f);
  ASSERT_NEAR(3.0f, point_at(0)->z, 0.001f);
  ASSERT_FALSE(point_at(0)->is_break);

  ASSERT_NEAR(4.0f, point_at(1)->x, 0.001f);
  ASSERT_NEAR(5.0f, point_at(1)->y, 0.001f);
  ASSERT_NEAR(6.0f, point_at(1)->z, 0.001f);
}

/* 空行（不是 matrix 那种 ';'）产生一个 is_break 点，位置就在它所在的行序上。 */
TEST_F(Plot3dSourceCsvTest, csv_blank_line_makes_break_point) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n\n4,5,6,#445566\n"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(3u, points.size);

  ASSERT_FALSE(point_at(0)->is_break);
  ASSERT_TRUE(point_at(1)->is_break);
  ASSERT_FALSE(point_at(2)->is_break);
  ASSERT_NEAR(4.0f, point_at(2)->x, 0.001f);

  /* 末尾的换行只是行结束符，不额外产生断点。 */
  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(1u, points.size);
}

TEST_F(Plot3dSourceCsvTest, csv_point_color_from_fourth_field) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n 4 , 5 , 6 , rgba(10,20,30,0.5) \n"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(2u, points.size);

  ASSERT_EQ(0x11, point_at(0)->color.rgba.r);
  ASSERT_EQ(0x22, point_at(0)->color.rgba.g);
  ASSERT_EQ(0x33, point_at(0)->color.rgba.b);

  /* 各段两侧的空白被去掉，rgba() 里的逗号不影响前三段的切分。 */
  ASSERT_NEAR(4.0f, point_at(1)->x, 0.001f);
  ASSERT_EQ(10, point_at(1)->color.rgba.r);
  ASSERT_EQ(20, point_at(1)->color.rgba.g);
  ASSERT_EQ(30, point_at(1)->color.rgba.b);
}

/* 不足四段的行被静默丢弃，且不产生断点。 */
TEST_F(Plot3dSourceCsvTest, csv_short_line_dropped) {
  plot3d_source_result_t result;

  ASSERT_EQ(RET_OK, set_dataset("1,2\nnot-a-point\n4,5,6\n7,8,9,#010203\n"));
  ASSERT_EQ(RET_OK, sample(&result));
  ASSERT_EQ(1u, points.size);
  ASSERT_NEAR(7.0f, point_at(0)->x, 0.001f);
  ASSERT_NEAR(8.0f, point_at(0)->y, 0.001f);
  ASSERT_NEAR(9.0f, point_at(0)->z, 0.001f);
}

/* reset 的契约：只清本插件的「数据源身份」，这里就是 dataset 文本本身。 */
TEST_F(Plot3dSourceCsvTest, csv_reset_clears_dataset) {
  value_t v;

  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n"));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_DATASET, &v));
  ASSERT_STREQ("1,2,3,#112233\n", value_str(&v));

  ASSERT_EQ(RET_OK, plot3d_source_reset(source));
  ASSERT_EQ(FALSE, plot3d_source_has_data(source));
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_DATASET, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));
}

TEST_F(Plot3dSourceCsvTest, csv_empty_dataset_has_no_data) {
  plot3d_source_result_t result;
  value_t v;

  ASSERT_EQ(FALSE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, set_dataset("1,2,3,#112233\n"));
  ASSERT_EQ(RET_OK, set_dataset(""));
  ASSERT_EQ(FALSE, plot3d_source_has_data(source));

  /* 清空就彻底清掉，属性读回 NULL 而不是空串。 */
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_DATASET, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));

  /* 没有数据时不产出结果，与 matrix 一致。 */
  ASSERT_NE(RET_OK, sample(&result));
}

TEST_F(Plot3dSourceCsvTest, csv_unknown_prop_not_found) {
  value_t v;

  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_NOT_FOUND, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ(RET_NOT_FOUND, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
}
