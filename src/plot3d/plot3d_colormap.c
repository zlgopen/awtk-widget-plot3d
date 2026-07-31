/**
 * File:   plot3d_colormap.c
 * Author: AWTK Develop Team
 * Brief:  Plot3D 配色表查表
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

#include "tkc/utils.h"
#include "plot3d_colormap.h"

typedef struct _plot3d_colormap_t {
  const char* name;
  uint32_t stops_nr;
  const uint32_t* stops;
} plot3d_colormap_t;

static const uint32_t s_colormap_viridis[] = {0x440154, 0x3b528b, 0x21918c, 0x5ec962, 0xfde725};
static const uint32_t s_colormap_jet[] = {0x000080, 0x0000ff, 0x00ffff, 0xffff00, 0xff0000,
                                          0x800000};
static const uint32_t s_colormap_gray[] = {0x000000, 0xffffff};
static const uint32_t s_colormap_parula[] = {0x352a87, 0x0f5cde, 0x06928f, 0x65bf4a, 0xf9e846};
static const uint32_t s_colormap_hot[] = {0x000000, 0xff0000, 0xffff00, 0xffffff};
static const uint32_t s_colormap_cool[] = {0x00ffff, 0xff00ff};
static const uint32_t s_colormap_hsv[] = {0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff,
                                          0xff00ff, 0xff0000};
static const uint32_t s_colormap_bone[] = {0x000000, 0x545463, 0xa8a8c1, 0xd9d9e6, 0xffffff};
static const uint32_t s_colormap_copper[] = {0x000000, 0x7f4014, 0xff8040, 0xffc77f};
static const uint32_t s_colormap_pink[] = {0x000000, 0x7f3f3f, 0xffbfbf, 0xffffff};
static const uint32_t s_colormap_turbo[] = {0x30123b, 0x3e9bfe, 0x46f884, 0xe1dd37, 0xf94106,
                                            0x7a0403};

static const plot3d_colormap_t s_colormaps[] = {
    {"viridis", ARRAY_SIZE(s_colormap_viridis), s_colormap_viridis},
    {"jet", ARRAY_SIZE(s_colormap_jet), s_colormap_jet},
    {"gray", ARRAY_SIZE(s_colormap_gray), s_colormap_gray},
    {"parula", ARRAY_SIZE(s_colormap_parula), s_colormap_parula},
    {"hot", ARRAY_SIZE(s_colormap_hot), s_colormap_hot},
    {"cool", ARRAY_SIZE(s_colormap_cool), s_colormap_cool},
    {"hsv", ARRAY_SIZE(s_colormap_hsv), s_colormap_hsv},
    {"bone", ARRAY_SIZE(s_colormap_bone), s_colormap_bone},
    {"copper", ARRAY_SIZE(s_colormap_copper), s_colormap_copper},
    {"pink", ARRAY_SIZE(s_colormap_pink), s_colormap_pink},
    {"turbo", ARRAY_SIZE(s_colormap_turbo), s_colormap_turbo}};

static const plot3d_colormap_t* plot3d_colormap_find(const char* name) {
  uint32_t i = 0;

  if (name != NULL) {
    for (i = 0; i < ARRAY_SIZE(s_colormaps); i++) {
      if (tk_str_eq(s_colormaps[i].name, name)) {
        return s_colormaps + i;
      }
    }
  }

  return s_colormaps;
}

static uint8_t plot3d_lerp_channel(uint32_t c0, uint32_t c1, uint32_t shift, float_t frac) {
  float_t a = (float_t)((c0 >> shift) & 0xff);
  float_t b = (float_t)((c1 >> shift) & 0xff);

  return (uint8_t)(a + (b - a) * frac + 0.5f);
}

ret_t plot3d_colormap_get_color(const char* name, float_t t, color_t* out_color) {
  const plot3d_colormap_t* map = NULL;
  float_t pos = 0;
  float_t frac = 0;
  uint32_t index = 0;
  return_value_if_fail(out_color != NULL, RET_BAD_PARAMS);

  map = plot3d_colormap_find(name);
  t = tk_min(tk_max(t, 0.0f), 1.0f);
  pos = t * (map->stops_nr - 1);
  index = (uint32_t)pos;
  if (index + 1 >= map->stops_nr) {
    index = map->stops_nr - 2;
    frac = 1.0f;
  } else {
    frac = pos - index;
  }

  *out_color = color_init(plot3d_lerp_channel(map->stops[index], map->stops[index + 1], 16, frac),
                          plot3d_lerp_channel(map->stops[index], map->stops[index + 1], 8, frac),
                          plot3d_lerp_channel(map->stops[index], map->stops[index + 1], 0, frac),
                          0xff);

  return RET_OK;
}
