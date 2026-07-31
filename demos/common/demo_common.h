#ifndef PLOT3D_DEMO_COMMON_H
#define PLOT3D_DEMO_COMMON_H

#include "awtk.h"

BEGIN_C_DECLS

typedef enum _plot3d_demo_kind_t {
  PLOT3D_DEMO_KIND_CSV = 0,
  PLOT3D_DEMO_KIND_GRID,
  PLOT3D_DEMO_KIND_EXPR,
  PLOT3D_DEMO_KIND_MATRIX,
  PLOT3D_DEMO_KIND_CURVE
} plot3d_demo_kind_t;

ret_t plot3d_demo_init_window(const char* window_name, plot3d_demo_kind_t kind);

END_C_DECLS

#endif /*PLOT3D_DEMO_COMMON_H*/
