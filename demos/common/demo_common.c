#include "demo_common.h"
#include "base/ui_loader.h"

/*
 * 复用原 window_main.c 的完整交互逻辑，避免在拆分 demo 时引入行为回归。
 * 这里通过宏重命名其入口函数，把逻辑当作内部实现复用。
 */
#define application_init plot3d_demo_legacy_application_init
#define application_exit plot3d_demo_legacy_application_exit
#include "../window_main.c"
#undef application_init
#undef application_exit

typedef struct _plot3d_demo_filter_t {
  const char* preset_options;
  const char* func_options;
  const char* default_preset;
} plot3d_demo_filter_t;

static const plot3d_demo_filter_t s_filters[] = {
    /* PLOT3D_DEMO_KIND_CSV */
    {"SinDot;SinLine;SinSurface;SinBars;Peaks;Sombrero;Lorenz;DNA;Sphere;BarGrid;Trefoil;PD;Dot;Line;"
     "Surface;Cylinder",
     "None", "SinSurface"},
    /* PLOT3D_DEMO_KIND_GRID */
    {"FuncRipple;FuncSinLine;Peaks;Sombrero", "None;Ripple;SinWave;Peaks;Sombrero;Saddle;Gauss",
     "FuncRipple"},
    /* PLOT3D_DEMO_KIND_EXPR */
    {"ExprWave;ExprCone;ExprRose;ExprBars;ColorRadial;ColorSplit",
     "None;ExprWave;ExprCone;ExprRose;ExprBars;ColorRadial;ColorSplit", "ExprWave"},
    /* PLOT3D_DEMO_KIND_MATRIX */
    {"MatrixBars;MatrixHeat", "None;MatrixBars;MatrixHeat", "MatrixBars"},
    /* PLOT3D_DEMO_KIND_CURVE */
    {"CurveHelix;CurveTrefoil;CurveLissajous;CurveKnot;CurveByT;CurveDamped;CurveSphere;CurveViviani;"
     "CurveHumps",
     "None;CurveHelix;CurveTrefoil;CurveLissajous;CurveKnot;CurveByT;CurveDamped;CurveSphere;"
     "CurveViviani;CurveHumps",
     "CurveHelix"}};

static ret_t plot3d_demo_mount_panel(widget_t* win, const char* slot_name, const char* panel_name) {
  widget_t* slot = widget_lookup(win, slot_name, TRUE);
  return_value_if_fail(slot != NULL, RET_NOT_FOUND);
  return_value_if_fail(ui_loader_load_widget_with_parent(panel_name, slot) != NULL, RET_FAIL);

  return RET_OK;
}

static ret_t plot3d_demo_mount_common_panels(widget_t* win) {
  return_value_if_fail(win != NULL, RET_BAD_PARAMS);
  return_value_if_fail(
      plot3d_demo_mount_panel(win, "slot_plot_type", "common/panel_plot_type") == RET_OK, RET_FAIL);
  return_value_if_fail(
      plot3d_demo_mount_panel(win, "slot_appearance", "common/panel_appearance") == RET_OK, RET_FAIL);
  return_value_if_fail(
      plot3d_demo_mount_panel(win, "slot_grid_axis", "common/panel_grid_axis") == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_demo_mount_panel(win, "slot_camera", "common/panel_camera") == RET_OK,
                       RET_FAIL);

  return RET_OK;
}

static const char* plot3d_demo_default_preset_name(plot3d_demo_kind_t kind) {
  return s_filters[kind].default_preset;
}

static ret_t plot3d_demo_apply_filter(widget_t* win, plot3d_demo_kind_t kind) {
  widget_t* cb_preset = widget_lookup(win, "cb_preset", TRUE);
  widget_t* cb_func = widget_lookup(win, "cb_func", TRUE);
  return_value_if_fail(cb_preset != NULL, RET_NOT_FOUND);

  combo_box_set_options(cb_preset, s_filters[kind].preset_options);
  widget_set_prop_int(cb_preset, WIDGET_PROP_SELECTED_INDEX, 0);
  if (cb_func != NULL) {
    combo_box_set_options(cb_func, s_filters[kind].func_options);
    widget_set_prop_int(cb_func, WIDGET_PROP_SELECTED_INDEX, 0);
  }

  return RET_OK;
}

static ret_t plot3d_demo_apply_defaults_for_kind(widget_t* chart, plot3d_demo_kind_t kind) {
  return_value_if_fail(plot3d_demo_apply_defaults(chart) == RET_OK, RET_FAIL);
  return plot3d_demo_apply_preset(chart, plot3d_demo_default_preset_name(kind));
}

ret_t plot3d_demo_init_window(const char* window_name, plot3d_demo_kind_t kind) {
  widget_t* chart = NULL;
  widget_t* win = NULL;
  return_value_if_fail(window_name != NULL, RET_BAD_PARAMS);
  return_value_if_fail(kind >= PLOT3D_DEMO_KIND_CSV && kind <= PLOT3D_DEMO_KIND_CURVE, RET_BAD_PARAMS);

  plot3d_register_all();
  win = window_open(window_name);
  return_value_if_fail(win != NULL, RET_FAIL);
  chart = widget_lookup(win, "chart", TRUE);
  return_value_if_fail(chart != NULL, RET_FAIL);

  return_value_if_fail(plot3d_demo_mount_common_panels(win) == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_demo_apply_filter(win, kind) == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_demo_bind_events(win, chart) == RET_OK, RET_FAIL);
  return_value_if_fail(plot3d_demo_apply_defaults_for_kind(chart, kind) == RET_OK, RET_FAIL);

  return RET_OK;
}
