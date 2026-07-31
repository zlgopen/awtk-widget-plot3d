/**
 * File:   plot3d_source_csv.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D csv 数据源插件
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
 * 2026-07-31 Li XianJing <xianjimli@hotmail.com> created
 *
 */

/* 本插件不支持 set_data：dataset 是一段文本，value_t 就能表达。 */

#include <string.h>

#include "tkc/color_parser.h"
#include "tkc/mem.h"
#include "tkc/utils.h"

#include "plot3d/plot3d.h"
#include "plot3d_source.h"

typedef struct _plot3d_source_csv_t {
  plot3d_source_t source;

  /* csv 文本本身就是本数据源的全部状态：点在 sample 时现解析，不再留一份解析结果，
   * 免得文本与点两份数据要同步。 */
  char* dataset;
} plot3d_source_csv_t;

/* 虚函数表里要填 create，而 create 又要取虚函数表的地址，这里先给出原型，
 * 定义放在虚函数表之后。 */
static plot3d_source_t* plot3d_source_csv_create(void);

static char* plot3d_source_csv_trim_inplace(char* str) {
  char* end = NULL;
  return_value_if_fail(str != NULL, str);

  while (*str != '\0' && tk_isspace(*str)) {
    str++;
  }

  end = str + tk_strlen(str);
  while (end > str) {
    end--;
    if (!tk_isspace(*end)) {
      break;
    }
    *end = '\0';
  }

  return str;
}

static ret_t plot3d_source_csv_push_point(darray_t* points, float_t x, float_t y, float_t z,
                                           color_t color, bool_t is_break) {
  plot3d_data_point_t* point = TKMEM_ZALLOC(plot3d_data_point_t);
  return_value_if_fail(point != NULL, RET_OOM);

  point->x = x;
  point->y = y;
  point->z = z;
  point->color = color;
  point->is_break = is_break;

  if (darray_push(points, point) != RET_OK) {
    TKMEM_FREE(point);

    return RET_OOM;
  }

  return RET_OK;
}

/* 按 '\n' 分行：空行产生一个 is_break 点，非空行必须凑齐 x,y,z,color 四段才建点，不足四段的
 * 行静默丢弃（也不产生断点）。颜色只切前三个逗号，rgba(...) 里的逗号因此不受影响。 */
static ret_t plot3d_source_csv_parse(const char* dataset, darray_t* points) {
  char* data = NULL;
  char* line = NULL;
  char* p = NULL;
  return_value_if_fail(!TK_STR_IS_EMPTY(dataset) && points != NULL, RET_BAD_PARAMS);

  /* 就地切分省掉逐段拷贝，代价是要先复制一份可写的文本。 */
  data = tk_str_copy(NULL, dataset);
  return_value_if_fail(data != NULL, RET_OOM);

  line = data;
  p = data;

  while (TRUE) {
    if (*p == '\n' || *p == '\0') {
      bool_t is_end = (*p == '\0');
      bool_t has_content = FALSE;
      char* token = NULL;
      char* c1 = NULL;
      char* c2 = NULL;
      char* c3 = NULL;

      *p = '\0';
      token = plot3d_source_csv_trim_inplace(line);
      has_content = (token != NULL && *token != '\0');

      if (has_content) {
        float_t x = 0;
        float_t y = 0;
        float_t z = 0;
        color_t color = color_init(255, 255, 255, 255);
        c1 = strchr(token, ',');
        c2 = c1 != NULL ? strchr(c1 + 1, ',') : NULL;
        c3 = c2 != NULL ? strchr(c2 + 1, ',') : NULL;
        if (c1 != NULL && c2 != NULL && c3 != NULL) {
          *c1 = '\0';
          *c2 = '\0';
          *c3 = '\0';
          x = (float_t)tk_atof(plot3d_source_csv_trim_inplace(token));
          y = (float_t)tk_atof(plot3d_source_csv_trim_inplace(c1 + 1));
          z = (float_t)tk_atof(plot3d_source_csv_trim_inplace(c2 + 1));
          color = color_parse(plot3d_source_csv_trim_inplace(c3 + 1));
          plot3d_source_csv_push_point(points, x, y, z, color, FALSE);
        }
      } else if (!is_end) {
        /* 末尾的换行只是行结束符，不当成空行。 */
        plot3d_source_csv_push_point(points, 0, 0, 0, color_init(0, 0, 0, 0), TRUE);
      }

      if (is_end) {
        break;
      }

      line = p + 1;
    }

    p++;
  }

  TKMEM_FREE(data);

  return RET_OK;
}

static bool_t plot3d_source_csv_has_data(plot3d_source_t* source) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;

  return !TK_STR_IS_EMPTY(csv->dataset);
}

static ret_t plot3d_source_csv_sample(plot3d_source_t* source, plot3d_source_result_t* result) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;
  return_value_if_fail(plot3d_source_csv_has_data(source), RET_BAD_PARAMS);
  return_value_if_fail(result->points != NULL, RET_BAD_PARAMS);

  result->type = PLOT3D_SOURCE_RESULT_POINTS;
  /* csv 每行第四段就是该点的颜色，核心不必再配色。 */
  result->has_color = TRUE;

  return plot3d_source_csv_parse(csv->dataset, result->points);
}

static ret_t plot3d_source_csv_set_prop(plot3d_source_t* source, const char* name,
                                         const value_t* v) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;
  const char* dataset = NULL;

  if (tk_str_eq(name, PLOT3D_PROP_DATASET)) {
    dataset = value_str(v);
    if (TK_STR_IS_EMPTY(dataset)) {
      /* 清空就彻底清掉：tk_str_copy 传 NULL 只会把字符串截空，属性值还留着。 */
      TKMEM_FREE(csv->dataset);
    } else {
      csv->dataset = tk_str_copy(csv->dataset, dataset);
    }

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_source_csv_get_prop(plot3d_source_t* source, const char* name, value_t* v) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;

  if (tk_str_eq(name, PLOT3D_PROP_DATASET)) {
    value_set_str(v, csv->dataset);

    return RET_OK;
  }

  return RET_NOT_FOUND;
}

/* dataset 文本就是本插件的「数据源身份」，让位时清掉它即可；本插件不持有任何共享配置属性。 */
static ret_t plot3d_source_csv_reset(plot3d_source_t* source) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;

  TKMEM_FREE(csv->dataset);

  return RET_OK;
}

static ret_t plot3d_source_csv_destroy(plot3d_source_t* source) {
  plot3d_source_csv_t* csv = (plot3d_source_csv_t*)source;

  TKMEM_FREE(csv->dataset);
  TKMEM_FREE(csv);

  return RET_OK;
}

/* 类型名与采样模式同名，核心据此从实例表里找到本插件。 */
static const plot3d_source_vtable_t s_plot3d_source_csv_vtable = {
    .type = PLOT3D_SAMPLE_MODE_NONE,
    .create = plot3d_source_csv_create,
    .sample = plot3d_source_csv_sample,
    .set_prop = plot3d_source_csv_set_prop,
    .get_prop = plot3d_source_csv_get_prop,
    .set_data = NULL,
    .has_data = plot3d_source_csv_has_data,
    .reset = plot3d_source_csv_reset,
    .destroy = plot3d_source_csv_destroy};

/* csv 只有 dataset 一个属性，没有数据就是没有数据，清零即为默认状态。 */
static plot3d_source_t* plot3d_source_csv_create(void) {
  plot3d_source_csv_t* csv = TKMEM_ZALLOC(plot3d_source_csv_t);
  return_value_if_fail(csv != NULL, NULL);

  csv->source.vt = &s_plot3d_source_csv_vtable;

  return (plot3d_source_t*)csv;
}

ret_t plot3d_source_csv_register(void) {
  return plot3d_source_factory_register(&s_plot3d_source_csv_vtable);
}
