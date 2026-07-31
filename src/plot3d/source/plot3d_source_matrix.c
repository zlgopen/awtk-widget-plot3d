/**
 * File:   plot3d_source_matrix.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 矩阵数据源插件
 *
 * Copyright (c) 2026 - 2026 Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2026-07-30 Li XianJing <xianjimli@hotmail.com> created
 *
 */

/* 本插件支持的 set_data 数据名只有 PLOT3D_SOURCE_DATA_Z_MATRIX，其 p1/p2 含义与所有权约定写在
 * plot3d_source.h 中该宏的注释上——调用方在核心里，读不到本文件。 */

#include <string.h>

#include "tkc/mem.h"
#include "tkc/tokenizer.h"
#include "tkc/utils.h"

#include "plot3d/plot3d.h"
#include "plot3d_source.h"

typedef struct _plot3d_source_matrix_t {
  plot3d_source_t source;

  /* 矩阵的 z 数据，按行存放。文本属性与 set_data 共用这一份，后设置的生效。 */
  float_t* zs;
  uint32_t cols;
  uint32_t rows;
  /* 用 set_data 直接给数据时为空：属性读回 NULL 才不会与实际数据不一致。 */
  char* sample_z_matrix;

  /* 与 grid 数据源共享的配置属性，reset 时不清。 */
  char* sample_x_range;
  char* sample_y_range;
} plot3d_source_matrix_t;

/* 虚函数表里要填 create，而 create 又要取虚函数表的地址，这里先给出原型，
 * 定义放在虚函数表之后。 */
static plot3d_source_t* plot3d_source_matrix_create(void);

/* "1,2,3;4,5,6"：分号或换行分行，逗号或空格分列，各行列数须相同。 */
static ret_t plot3d_source_matrix_parse(const char* text, float_t** out_zs, uint32_t* out_cols,
                                         uint32_t* out_rows) {
  ret_t ret = RET_OK;
  uint32_t cols = 0;
  uint32_t rows = 0;
  uint32_t nr = 0;
  uint32_t capacity = 0;
  float_t* zs = NULL;
  tokenizer_t lines;
  return_value_if_fail(!TK_STR_IS_EMPTY(text) && out_zs != NULL, RET_BAD_PARAMS);

  tokenizer_init(&lines, text, tk_strlen(text), ";\r\n");
  while (ret == RET_OK && tokenizer_has_more(&lines)) {
    uint32_t row_cols = 0;
    tokenizer_t items;
    const char* line = tokenizer_next(&lines);

    if (line == NULL) {
      break;
    }

    tokenizer_init(&items, line, tk_strlen(line), ", \t");
    while (tokenizer_has_more(&items)) {
      const char* item = tokenizer_next(&items);

      if (item == NULL) {
        break;
      }

      if (nr == capacity) {
        float_t* p = NULL;

        capacity = capacity > 0 ? capacity * 2 : 16;
        p = TKMEM_REALLOCT(float_t, zs, capacity);
        if (p == NULL) {
          ret = RET_OOM;
          break;
        }
        zs = p;
      }
      zs[nr++] = tk_atof(item);
      row_cols++;
    }
    tokenizer_deinit(&items);

    if (rows == 0) {
      cols = row_cols;
    } else if (row_cols != cols) {
      /* 行列数不齐说明写错了，不猜用户的意思。 */
      ret = RET_BAD_PARAMS;
    }
    rows++;
  }
  tokenizer_deinit(&lines);

  if (ret == RET_OK && (cols == 0 || rows == 0)) {
    ret = RET_BAD_PARAMS;
  }

  if (ret != RET_OK) {
    TKMEM_FREE(zs);

    return ret;
  }

  *out_zs = zs;
  *out_cols = cols;
  *out_rows = rows;

  return RET_OK;
}

/* 只清「数据源身份」：矩阵数据与它的文本形式。 */
static ret_t plot3d_source_matrix_clear(plot3d_source_matrix_t* matrix) {
  TKMEM_FREE(matrix->zs);
  TKMEM_FREE(matrix->sample_z_matrix);
  matrix->cols = 0;
  matrix->rows = 0;

  return RET_OK;
}

static ret_t plot3d_source_matrix_set_text(plot3d_source_matrix_t* matrix, const char* text) {
  float_t* zs = NULL;
  uint32_t cols = 0;
  uint32_t rows = 0;

  if (TK_STR_IS_EMPTY(text)) {
    return plot3d_source_matrix_clear(matrix);
  }

  /* 先解析成功再替换：格式错误时保持原矩阵，图不会突然变空。 */
  return_value_if_fail(plot3d_source_matrix_parse(text, &zs, &cols, &rows) == RET_OK,
                       RET_BAD_PARAMS);

  plot3d_source_matrix_clear(matrix);
  matrix->zs = zs;
  matrix->cols = cols;
  matrix->rows = rows;
  matrix->sample_z_matrix = tk_str_copy(NULL, text);

  return RET_OK;
}

/* 没给范围时按列号与行号铺开，给了范围就把下标映射到该范围。 */
static ret_t plot3d_source_matrix_axis_range(const char* range, uint32_t count, float_t* v0,
                                              float_t* v1) {
  *v0 = 0;
  *v1 = count > 1 ? (float_t)(count - 1) : 0;

  if (!TK_STR_IS_EMPTY(range)) {
    plot3d_parse_range(range, v0, v1);
  }

  return RET_OK;
}

static bool_t plot3d_source_matrix_has_data(plot3d_source_t* source) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;

  return matrix->zs != NULL && matrix->cols > 0 && matrix->rows > 0;
}

static ret_t plot3d_source_matrix_sample(plot3d_source_t* source,
                                          plot3d_source_result_t* result) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;
  return_value_if_fail(plot3d_source_matrix_has_data(source), RET_BAD_PARAMS);

  result->type = PLOT3D_SOURCE_RESULT_GRID;
  /* zs 由插件持有并复用，核心只读、不得释放。 */
  result->zs = matrix->zs;
  result->cols = matrix->cols;
  result->rows = matrix->rows;
  plot3d_source_matrix_axis_range(matrix->sample_x_range, matrix->cols, &(result->x0),
                                   &(result->x1));
  plot3d_source_matrix_axis_range(matrix->sample_y_range, matrix->rows, &(result->y0),
                                   &(result->y1));

  return RET_OK;
}

static ret_t plot3d_source_matrix_set_prop(plot3d_source_t* source, const char* name,
                                            const value_t* v) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_MATRIX)) {
    /* 三值协议只认 RET_BAD_PARAMS 这一种「认领但拒绝」，OOM 之类也一并归到它。 */
    if (plot3d_source_matrix_set_text(matrix, value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_RANGE)) {
    if (plot3d_source_set_range_prop(&(matrix->sample_x_range), value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_RANGE)) {
    if (plot3d_source_set_range_prop(&(matrix->sample_y_range), value_str(v)) != RET_OK) {
      return RET_BAD_PARAMS;
    }

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_matrix_get_prop(plot3d_source_t* source, const char* name,
                                            value_t* v) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Z_MATRIX)) {
    value_set_str(v, matrix->sample_z_matrix);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_X_RANGE)) {
    value_set_str(v, matrix->sample_x_range);

    return RET_OK;
  } else if (tk_str_eq(name, PLOT3D_PROP_SAMPLE_Y_RANGE)) {
    value_set_str(v, matrix->sample_y_range);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_matrix_set_data(plot3d_source_t* source, const char* name, void* p1,
                                            void* p2) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;
  const float_t* zs = (const float_t*)p1;
  const plot3d_source_matrix_size_t* size = (const plot3d_source_matrix_size_t*)p2;
  float_t* data = NULL;

  if (!tk_str_eq(name, PLOT3D_SOURCE_DATA_Z_MATRIX)) {
    return RET_NOT_FOUND;
  }

  if (zs == NULL) {
    return plot3d_source_matrix_clear(matrix);
  }

  return_value_if_fail(size != NULL && size->cols > 0 && size->rows > 0, RET_BAD_PARAMS);
  /* 元素个数按 uint32 相乘，而下面 memcpy 的长度含 size_t 的 sizeof 会提升到 64 位，两者在乘积
   * 溢出时不一致。实际够不着，但挡住比推理便宜。 */
  return_value_if_fail(size->cols <= UINT32_MAX / size->rows, RET_BAD_PARAMS);

  /* 先拷贝再清空原数据：调用方完全可能把插件自己那份 zs 传回来，先清会读到已释放的内存。 */
  data = TKMEM_ZALLOCN(float_t, size->cols * size->rows);
  return_value_if_fail(data != NULL, RET_OOM);
  memcpy(data, zs, sizeof(float_t) * size->cols * size->rows);

  plot3d_source_matrix_clear(matrix);
  matrix->zs = data;
  matrix->cols = size->cols;
  matrix->rows = size->rows;

  return RET_OK;
}

/* 让位时只清矩阵与它的文本：sample-x-range/sample-y-range 是与 grid 共享的配置属性，清掉会让
 * 两个认领者的副本分叉，而 get 侧广播只取第一个认领者，读回哪一份将取决于注册顺序。 */
static ret_t plot3d_source_matrix_reset(plot3d_source_t* source) {
  return plot3d_source_matrix_clear((plot3d_source_matrix_t*)source);
}

static ret_t plot3d_source_matrix_destroy(plot3d_source_t* source) {
  plot3d_source_matrix_t* matrix = (plot3d_source_matrix_t*)source;

  plot3d_source_matrix_clear(matrix);
  TKMEM_FREE(matrix->sample_x_range);
  TKMEM_FREE(matrix->sample_y_range);
  TKMEM_FREE(matrix);

  return RET_OK;
}

/* 类型名与采样模式同名，核心据此从实例表里找到本插件。 */
static const plot3d_source_vtable_t s_plot3d_source_matrix_vtable = {
    .type = PLOT3D_SAMPLE_MODE_MATRIX,
    .create = plot3d_source_matrix_create,
    .sample = plot3d_source_matrix_sample,
    .set_prop = plot3d_source_matrix_set_prop,
    .get_prop = plot3d_source_matrix_get_prop,
    .set_data = plot3d_source_matrix_set_data,
    .has_data = plot3d_source_matrix_has_data,
    .reset = plot3d_source_matrix_reset,
    .destroy = plot3d_source_matrix_destroy};

/* 矩阵的几个属性都没有非零默认值：没给范围时按下标铺开，清零即为默认状态。 */
static plot3d_source_t* plot3d_source_matrix_create(void) {
  plot3d_source_matrix_t* matrix = TKMEM_ZALLOC(plot3d_source_matrix_t);
  return_value_if_fail(matrix != NULL, NULL);

  matrix->source.vt = &s_plot3d_source_matrix_vtable;

  return (plot3d_source_t*)matrix;
}

ret_t plot3d_source_matrix_register(void) {
  return plot3d_source_factory_register(&s_plot3d_source_matrix_vtable);
}
