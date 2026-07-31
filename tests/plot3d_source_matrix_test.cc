#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "plot3d/plot3d.h"
#include "plot3d/source/plot3d_source.h"

namespace {

/* matrix 插件自身的用例：SetUp 建实例、TearDown 销毁，省掉每个用例重复的注册与建实例样板。
 *
 * 只注册 matrix 一个插件而不是 plot3d_register_all()：后者一旦有别的插件注册失败，会把本组
 * 用例一起拖红，掩盖真正的失败点。
 *
 * 注册表是进程内单例，用例改动后必须复原成全量内置插件已注册的状态，否则会污染其它 TU 的用例
 * （tests/SConscript 用 Glob 收集，跨 TU 执行顺序不可控）。
 * SetUp/TearDown 不写 override：本仓库默认按 C++98 编译，override 会触发
 * -Wc++11-extensions 告警。 */
class Plot3dSourceMatrixTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    ASSERT_EQ(RET_OK, plot3d_source_matrix_register());
    source = plot3d_source_factory_create(PLOT3D_SAMPLE_MODE_MATRIX);
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

  plot3d_source_t* source;
};

}  // namespace

TEST_F(Plot3dSourceMatrixTest, matrix_sample_outputs_grid) {
  plot3d_source_result_t result;
  value_t v;

  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_GRID, result.type);
  ASSERT_EQ(3u, result.cols);
  ASSERT_EQ(2u, result.rows);
  ASSERT_EQ(6.0f, result.zs[5]);

  /* 没给范围时按列号与行号铺开：3 列取 0~2，2 行取 0~1。 */
  ASSERT_EQ(0.0f, result.x0);
  ASSERT_EQ(2.0f, result.x1);
  ASSERT_EQ(0.0f, result.y0);
  ASSERT_EQ(1.0f, result.y1);
}

TEST_F(Plot3dSourceMatrixTest, matrix_sample_maps_indices_to_ranges) {
  plot3d_source_result_t result;
  value_t v;

  value_set_str(&v, "0,10");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  value_set_str(&v, "-1,1");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(0.0f, result.x0);
  ASSERT_EQ(10.0f, result.x1);
  ASSERT_EQ(-1.0f, result.y0);
  ASSERT_EQ(1.0f, result.y1);

  /* 清空范围又回到按下标。 */
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(0.0f, result.x0);
  ASSERT_EQ(2.0f, result.x1);

  /* 范围格式非法时认领但拒绝，原值不变。 */
  value_set_str(&v, "not-a-range");
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  ASSERT_STREQ("-1,1", value_str(&v));
}

TEST_F(Plot3dSourceMatrixTest, matrix_invalid_text_rejected_and_keeps_old) {
  plot3d_source_result_t result;
  value_t v;

  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  /* 各行列数不同、以及分不出任何数值：都是「认领但拒绝」，原矩阵与原文本一并保持不变。 */
  value_set_str(&v, "1,2;3");
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  value_set_str(&v, ";");
  ASSERT_EQ(RET_BAD_PARAMS, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_STREQ("1,2,3;4,5,6", value_str(&v));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(3u, result.cols);
  ASSERT_EQ(2u, result.rows);
  ASSERT_EQ(6.0f, result.zs[5]);
}

TEST_F(Plot3dSourceMatrixTest, matrix_set_data_copies_array_and_clears_text) {
  plot3d_source_result_t result;
  plot3d_source_matrix_size_t size;
  float_t zs[6] = {1, 2, 3, 4, 5, 6};
  value_t v;

  value_set_str(&v, "9,9;9,9");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  size.cols = 3;
  size.rows = 2;
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_Z_MATRIX, zs, &size));

  /* 插件必须拷贝一份：调用方的数组在 set_data 返回后就可以失效。 */
  memset(zs, 0x00, sizeof(zs));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(PLOT3D_SOURCE_RESULT_GRID, result.type);
  ASSERT_EQ(3u, result.cols);
  ASSERT_EQ(2u, result.rows);
  ASSERT_EQ(6.0f, result.zs[5]);

  /* 数据换成 C 数组之后属性里的文本一并清掉，避免属性与实际数据不一致。 */
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));
}

/* 别名场景：把上一次 sample 拿到的 result.zs（就是插件自己那份）原样回传。实现必须先拷贝再清空
 * 原数据，反过来会读已释放的内存。注意这条断言的发现能力依赖分配器行为——先清后拷时数据往往还
 * 残留在原地，稳定暴露需要 ASan 一类的堆毒化，这里主要是把契约钉成可执行的文档。 */
TEST_F(Plot3dSourceMatrixTest, matrix_set_data_accepts_its_own_zs) {
  plot3d_source_result_t result;
  plot3d_source_matrix_size_t size;
  value_t v;

  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));

  size.cols = result.cols;
  size.rows = result.rows;
  ASSERT_EQ(RET_OK,
            plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_Z_MATRIX, (void*)result.zs, &size));

  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(3u, result.cols);
  ASSERT_EQ(2u, result.rows);
  ASSERT_EQ(1.0f, result.zs[0]);
  ASSERT_EQ(6.0f, result.zs[5]);
}

TEST_F(Plot3dSourceMatrixTest, matrix_set_data_null_clears_matrix) {
  value_t v;

  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  /* p1 为 NULL 表示清空矩阵，此时 p2 被忽略。 */
  ASSERT_EQ(RET_OK, plot3d_source_set_data(source, PLOT3D_SOURCE_DATA_Z_MATRIX, NULL, NULL));
  ASSERT_EQ(FALSE, plot3d_source_has_data(source));

  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));

  /* 不认识的数据名照三值协议返回 RET_NOT_FOUND。 */
  ASSERT_EQ(RET_NOT_FOUND, plot3d_source_set_data(source, "no-such-data", NULL, NULL));
}

/* reset 的契约：只清本插件的「数据源身份」，共享配置属性留给其它认领者。 */
TEST_F(Plot3dSourceMatrixTest, matrix_reset_keeps_shared_ranges) {
  plot3d_source_result_t result;
  value_t v;

  value_set_str(&v, "0,10");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  value_set_str(&v, "-1,1");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ(TRUE, plot3d_source_has_data(source));

  ASSERT_EQ(RET_OK, plot3d_source_reset(source));

  ASSERT_EQ(FALSE, plot3d_source_has_data(source));
  value_set_str(&v, "not-null");
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  ASSERT_EQ((const char*)NULL, value_str(&v));

  /* x/y 范围与 grid 数据源共享，reset 清掉会让两份副本分叉，而 get 侧广播只取第一个认领者。 */
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_X_RANGE, &v));
  ASSERT_STREQ("0,10", value_str(&v));
  value_set_str(&v, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_get_prop(source, PLOT3D_PROP_SAMPLE_Y_RANGE, &v));
  ASSERT_STREQ("-1,1", value_str(&v));

  /* 光回读属性还不够：范围要真的还能进到采样结果里。 */
  value_set_str(&v, "1,2,3;4,5,6");
  ASSERT_EQ(RET_OK, plot3d_source_set_prop(source, PLOT3D_PROP_SAMPLE_Z_MATRIX, &v));
  plot3d_source_result_init(&result, NULL);
  ASSERT_EQ(RET_OK, plot3d_source_sample(source, &result));
  ASSERT_EQ(0.0f, result.x0);
  ASSERT_EQ(10.0f, result.x1);
  ASSERT_EQ(-1.0f, result.y0);
  ASSERT_EQ(1.0f, result.y1);
}
