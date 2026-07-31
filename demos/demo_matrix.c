#include "common/demo_common.h"

ret_t application_init(void) {
  return plot3d_demo_init_window("demo_matrix", PLOT3D_DEMO_KIND_MATRIX);
}

ret_t application_exit(void) {
  log_debug("application_exit\n");
  return RET_OK;
}
