#include "gtest/gtest.h"
#include "tkc/mem.h"
#include "plot3d_register.h"
#include "plot3d/type/plot3d_type.h"

namespace {

static ret_t free_item(void* data) {
  TKMEM_FREE(data);
  return RET_OK;
}

class Plot3dTypeLineTest : public testing::Test {
 protected:
  void SetUp() {
    plot3d_unregister();
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }

  void TearDown() {
    ASSERT_EQ(RET_OK, plot3d_unregister());
    ASSERT_EQ(RET_OK, plot3d_register_all());
  }
};

}  // namespace

TEST_F(Plot3dTypeLineTest, line_registered_in_register_all) {
  plot3d_type_t* type = plot3d_type_factory_create("line");
  ASSERT_NE((plot3d_type_t*)NULL, type);
  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}

TEST_F(Plot3dTypeLineTest, line_layout_grid_adds_row_break) {
  plot3d_source_result_t result;
  plot3d_type_t* type = plot3d_type_factory_create("line");
  plot3d_paint_ctx_t ctx;
  darray_t points;
  plot3d_data_point_t* p = NULL;

  ASSERT_NE((plot3d_type_t*)NULL, type);
  memset(&ctx, 0x00, sizeof(ctx));
  ASSERT_EQ(RET_OK, plot3d_source_result_init(&result, NULL));
  result.type = PLOT3D_SOURCE_RESULT_GRID;
  result.cols = 2;
  result.rows = 2;
  result.x0 = 0;
  result.x1 = 1;
  result.y0 = 0;
  result.y1 = 1;
  {
    static const float_t zs[] = {1, 2, 3, 4};
    result.zs = zs;
  }

  darray_init(&points, 8, free_item, NULL);
  ASSERT_EQ(RET_OK, plot3d_type_layout_grid(type, &ctx, &result, &points));
  ASSERT_EQ(5u, points.size);

  p = (plot3d_data_point_t*)darray_get(&points, 2);
  ASSERT_NE((plot3d_data_point_t*)NULL, p);
  ASSERT_TRUE(p->is_break);

  darray_deinit(&points);
  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}

TEST_F(Plot3dTypeLineTest, line_build_creates_segment_primitives) {
  plot3d_type_t* type = plot3d_type_factory_create("line");
  plot3d_paint_ctx_t ctx;
  darray_t primitives;
  plot3d_primitive_t* primitive = NULL;
  plot3d_projected_point_t points[4];

  ASSERT_NE((plot3d_type_t*)NULL, type);
  memset(&ctx, 0x00, sizeof(ctx));
  memset(points, 0x00, sizeof(points));
  points[0].valid = TRUE;
  points[1].valid = TRUE;
  points[2].is_break = TRUE;
  points[3].valid = TRUE;
  points[0].depth = 0.1f;
  points[1].depth = 0.3f;
  points[3].depth = 0.8f;
  points[0].color = color_init(1, 2, 3, 255);

  ctx.points = points;
  ctx.points_nr = 4;

  darray_init(&primitives, 4, free_item, NULL);
  ASSERT_EQ(RET_OK, plot3d_type_build(type, &ctx, &primitives));
  ASSERT_EQ(1u, primitives.size);

  primitive = (plot3d_primitive_t*)darray_get(&primitives, 0);
  ASSERT_NE((plot3d_primitive_t*)NULL, primitive);
  ASSERT_EQ(PLOT3D_PRIMITIVE_LINE, primitive->type);
  ASSERT_EQ(0u, primitive->i0);
  ASSERT_EQ(1u, primitive->i1);

  darray_deinit(&primitives);
  ASSERT_EQ(RET_OK, plot3d_type_destroy(type));
}
