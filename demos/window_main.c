#include <float.h>

#include "awtk.h"
#include "tkc/data_reader.h"
#include "base/data_reader_asset.h"
#include "plot3d/plot3d.h"
#include "plot3d_register.h"

/*yaw/z_offset 不限制范围，用浮点极值代替*/
#define PLOT3D_DEMO_MIN_UNLIMITED (-FLT_MAX)
#define PLOT3D_DEMO_MAX_UNLIMITED FLT_MAX

/*data 资源名保留扩展名，界面上只显示不含扩展名的样例名*/
#define PLOT3D_DEMO_DATASET_EXTNAME ".csv"

typedef ret_t (*plot3d_demo_set_float_t)(widget_t* widget, float_t value);

typedef struct _plot3d_demo_step_t {
  const char* name;
  const char* prop;
  plot3d_demo_set_float_t set;
  float_t delta;
  float_t min;
  float_t max;
} plot3d_demo_step_t;

/*函数采样的配置：范围、密度与配色表*/
typedef struct _plot3d_demo_sample_t {
  /*名字须与 main.xml 中 cb_func 的 options 一致*/
  const char* name;
  plot3d_grid_func_t func;
  const char* x_range;
  const char* y_range;
  const char* steps;
  const char* colormap;
  /*非空表示 z 由表达式提供，此时忽略 func*/
  const char* expr;
  /*非空表示配色由表达式提供，否则按 z 在配色表上取色*/
  const char* color_expr;
  /*非空表示 matrix 模式，z 直接来自这个矩阵*/
  const char* matrix;
  /*非零表示 matrix 模式，矩阵由代码按该边长生成*/
  uint32_t matrix_size;
} plot3d_demo_sample_t;

/*曲线采样的配置：t 范围、点数与三个坐标的来源*/
typedef struct _plot3d_demo_curve_t {
  /*名字须与 main.xml 中 cb_func 的 options 一致*/
  const char* name;
  plot3d_curve_func_t func;
  /*func 为空时用这三个表达式，变量为 t*/
  const char* x_expr;
  const char* y_expr;
  const char* z_expr;
  const char* t_range;
  uint32_t t_steps;
  const char* colormap;
  /*非空表示配色由表达式提供，否则按 z 在配色表上取色*/
  const char* color_expr;
} plot3d_demo_curve_t;

/*一个预设把数据集、图形类型、轴名、网格数与盒子比例一次配好*/
typedef struct _plot3d_demo_preset_t {
  const char* name;
  const char* dataset;
  const char* plottype;
  const char* xlabel;
  const char* ylabel;
  const char* zlabel;
  uint32_t x_grid_count;
  uint32_t y_grid_count;
  uint32_t z_grid_count;
  float_t box_aspect_x;
  float_t box_aspect_y;
  float_t box_aspect_z;
  float_t point_size;
  float_t line_width;
  /*为空表示数据来自 dataset 的 csv，否则忽略 dataset 用函数现场采样*/
  const plot3d_demo_sample_t* sample;
  /*非空表示按参数曲线采样，此时忽略 dataset 与 sample*/
  const plot3d_demo_curve_t* curve;
} plot3d_demo_preset_t;

/*min/max 与 plot3d_set_* 内部钳位一致*/
static const plot3d_demo_step_t s_steps[] = {
    {"btn_point_size_dec", PLOT3D_PROP_POINT_SIZE, plot3d_set_point_size, -1.0f, 1.0f, 20.0f},
    {"btn_point_size_inc", PLOT3D_PROP_POINT_SIZE, plot3d_set_point_size, 1.0f, 1.0f, 20.0f},
    {"btn_line_width_dec", PLOT3D_PROP_LINE_WIDTH, plot3d_set_line_width, -0.5f, 1.0f, 10.0f},
    {"btn_line_width_inc", PLOT3D_PROP_LINE_WIDTH, plot3d_set_line_width, 0.5f, 1.0f, 10.0f},
    {"btn_axis_neg_bright_dec", PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS,
     plot3d_set_axis_negative_brightness, -0.05f, 0.0f, 1.0f},
    {"btn_axis_neg_bright_inc", PLOT3D_PROP_AXIS_NEGATIVE_BRIGHTNESS,
     plot3d_set_axis_negative_brightness, 0.05f, 0.0f, 1.0f},
    {"btn_box_aspect_x_dec", PLOT3D_PROP_BOX_ASPECT_X, plot3d_set_box_aspect_x, -0.1f, 0.1f,
     2.0f},
    {"btn_box_aspect_x_inc", PLOT3D_PROP_BOX_ASPECT_X, plot3d_set_box_aspect_x, 0.1f, 0.1f, 2.0f},
    {"btn_box_aspect_y_dec", PLOT3D_PROP_BOX_ASPECT_Y, plot3d_set_box_aspect_y, -0.1f, 0.1f,
     2.0f},
    {"btn_box_aspect_y_inc", PLOT3D_PROP_BOX_ASPECT_Y, plot3d_set_box_aspect_y, 0.1f, 0.1f, 2.0f},
    {"btn_box_aspect_z_dec", PLOT3D_PROP_BOX_ASPECT_Z, plot3d_set_box_aspect_z, -0.1f, 0.1f,
     2.0f},
    {"btn_box_aspect_z_inc", PLOT3D_PROP_BOX_ASPECT_Z, plot3d_set_box_aspect_z, 0.1f, 0.1f, 2.0f},
    {"btn_yaw_dec", PLOT3D_PROP_CAMERA_YAW, plot3d_set_camera_yaw, -0.25f,
     PLOT3D_DEMO_MIN_UNLIMITED, PLOT3D_DEMO_MAX_UNLIMITED},
    {"btn_yaw_inc", PLOT3D_PROP_CAMERA_YAW, plot3d_set_camera_yaw, 0.25f,
     PLOT3D_DEMO_MIN_UNLIMITED, PLOT3D_DEMO_MAX_UNLIMITED},
    {"btn_pitch_dec", PLOT3D_PROP_CAMERA_PITCH, plot3d_set_camera_pitch, -0.1f, -1.4f, 1.4f},
    {"btn_pitch_inc", PLOT3D_PROP_CAMERA_PITCH, plot3d_set_camera_pitch, 0.1f, -1.4f, 1.4f},
    {"btn_dist_dec", PLOT3D_PROP_CAMERA_DISTANCE, plot3d_set_camera_distance, -0.3f, 1.8f, 12.0f},
    {"btn_dist_inc", PLOT3D_PROP_CAMERA_DISTANCE, plot3d_set_camera_distance, 0.3f, 1.8f, 12.0f},
    {"btn_zoff_dec", PLOT3D_PROP_CAMERA_Z_OFFSET, plot3d_set_camera_z_offset, -0.2f,
     PLOT3D_DEMO_MIN_UNLIMITED, PLOT3D_DEMO_MAX_UNLIMITED},
    {"btn_zoff_inc", PLOT3D_PROP_CAMERA_Z_OFFSET, plot3d_set_camera_z_offset, 0.2f,
     PLOT3D_DEMO_MIN_UNLIMITED, PLOT3D_DEMO_MAX_UNLIMITED}};


typedef ret_t (*plot3d_demo_set_uint_t)(widget_t* widget, uint32_t value);

typedef struct _plot3d_demo_count_step_t {
  const char* name;
  const char* prop;
  plot3d_demo_set_uint_t set;
  int32_t delta;
} plot3d_demo_count_step_t;

static const plot3d_demo_count_step_t s_count_steps[] = {
    {"btn_x_grid_count_dec", PLOT3D_PROP_X_GRID_COUNT, plot3d_set_x_grid_count, -1},
    {"btn_x_grid_count_inc", PLOT3D_PROP_X_GRID_COUNT, plot3d_set_x_grid_count, 1},
    {"btn_y_grid_count_dec", PLOT3D_PROP_Y_GRID_COUNT, plot3d_set_y_grid_count, -1},
    {"btn_y_grid_count_inc", PLOT3D_PROP_Y_GRID_COUNT, plot3d_set_y_grid_count, 1},
    {"btn_z_grid_count_dec", PLOT3D_PROP_Z_GRID_COUNT, plot3d_set_z_grid_count, -1},
    {"btn_z_grid_count_inc", PLOT3D_PROP_Z_GRID_COUNT, plot3d_set_z_grid_count, 1},
};

/*控件名到属性名的对应关系，一个回调按控件名分发*/
typedef struct _plot3d_demo_prop_widget_t {
  const char* name;
  const char* prop;
} plot3d_demo_prop_widget_t;

static const plot3d_demo_prop_widget_t s_color_combos[] = {
    {"cb_grid_color", PLOT3D_PROP_GRID_COLOR},   {"cb_tick_color", PLOT3D_PROP_TICK_COLOR},
    {"cb_xaxis_color", PLOT3D_PROP_XAXIS_COLOR}, {"cb_yaxis_color", PLOT3D_PROP_YAXIS_COLOR},
    {"cb_zaxis_color", PLOT3D_PROP_ZAXIS_COLOR},
};

typedef struct _plot3d_demo_switch_t {
  const char* name;
  const char* prop;
  /*取不到勾选状态时的兜底值，与控件默认值一致*/
  bool_t default_value;
} plot3d_demo_switch_t;

static const plot3d_demo_switch_t s_switches[] = {
    {"chk_show_grid", PLOT3D_PROP_SHOW_GRID, TRUE},
    {"chk_show_axis_tick", PLOT3D_PROP_SHOW_AXIS_TICK, TRUE},
    {"chk_enable_cache", PLOT3D_PROP_ENABLE_CACHE, TRUE},
    {"chk_equal_axis", PLOT3D_PROP_EQUAL_AXIS, FALSE},
};

static const char* s_plottype_buttons[] = {"btn_dot", "btn_line", "btn_surface", "btn_cylinder"};

static const char* s_grid_plane_buttons[] = {"btn_xy_grid_dec", "btn_xy_grid_inc",
                                             "btn_xz_grid_dec", "btn_xz_grid_inc",
                                             "btn_yz_grid_dec", "btn_yz_grid_inc"};

/*涟漪面：z = sin(3r)/(1+r)，r 是到原点的距离*/
static ret_t plot3d_demo_func_ripple(void* ctx, float_t x, float_t y, float_t* z) {
  float_t r = sqrtf(x * x + y * y);

  *z = sinf(r * 3.0f) / (1.0f + r);

  return RET_OK;
}

/*行进波：与 sample_sin_line 同一个公式，用来对照函数采样与 csv 的效果*/
static ret_t plot3d_demo_func_sin_wave(void* ctx, float_t x, float_t y, float_t* z) {
  *z = sinf(TK_D2R(x) + y * 0.5f);

  return RET_OK;
}

/*matlab peaks，与 sample_peaks 同一个公式*/
static ret_t plot3d_demo_func_peaks(void* ctx, float_t x, float_t y, float_t* z) {
  float_t a = 3 * (1 - x) * (1 - x) * expf(-x * x - (y + 1) * (y + 1));
  float_t b = 10 * (x / 5 - x * x * x - powf(y, 5)) * expf(-x * x - y * y);
  float_t c = expf(-(x + 1) * (x + 1) - y * y) / 3;

  *z = a - b - c;

  return RET_OK;
}

/*墨西哥帽 sin(r)/r，与 sample_sombrero 同一个公式*/
static ret_t plot3d_demo_func_sombrero(void* ctx, float_t x, float_t y, float_t* z) {
  float_t r = sqrtf(x * x + y * y);

  *z = r > 0.0001f ? sinf(r) / r : 1.0f;

  return RET_OK;
}

/*马鞍面：两个方向弯曲相反，用来看曲面朝向明暗*/
static ret_t plot3d_demo_func_saddle(void* ctx, float_t x, float_t y, float_t* z) {
  *z = x * x - y * y;

  return RET_OK;
}

/*高斯钟形：单峰，用来看柱状与点云的高度分布*/
static ret_t plot3d_demo_func_gauss(void* ctx, float_t x, float_t y, float_t* z) {
  *z = expf(-(x * x + y * y) / 2);

  return RET_OK;
}

/*环面纽结：坐标由 t 一次算出，用来演示曲线回调*/
static ret_t plot3d_demo_curve_knot(void* ctx, float_t t, float_t* x, float_t* y, float_t* z) {
  float_t r = 2 + cosf(3 * t);

  *x = r * cosf(2 * t);
  *y = r * sinf(2 * t);
  *z = sinf(3 * t);

  return RET_OK;
}

/*前三条用表达式，CurveKnot 用 C 回调，CurveByT 让颜色沿曲线走而不是按高度*/
static const plot3d_demo_curve_t s_curves[] = {
    {"CurveHelix", NULL, "cos(t)", "sin(t)", "t / 8", "0,25", 500, "viridis", NULL},
    {"CurveTrefoil", NULL, "sin(t) + 2 * sin(2 * t)", "cos(t) - 2 * cos(2 * t)", "0 - sin(3 * t)",
     "0,6.283", 400, "jet", NULL},
    {"CurveLissajous", NULL, "sin(3 * t)", "sin(4 * t)", "sin(5 * t)", "0,6.283", 600, "viridis",
     NULL},
    {"CurveKnot", plot3d_demo_curve_knot, NULL, NULL, NULL, "0,6.283", 400, "jet", NULL},
    {"CurveByT", NULL, "sin(3 * t)", "sin(4 * t)", "sin(5 * t)", "0,6.283", 600, "jet", "t"},
    /*matlab fplot3 文档里的阻尼螺旋*/
    {"CurveDamped", NULL, "exp(0 - t / 10) * sin(5 * t)", "exp(0 - t / 10) * cos(5 * t)", "t",
     "-10,10", 800, "viridis", NULL},
    /*球面螺旋：绕着单位球从南极缠到北极*/
    {"CurveSphere", NULL, "sin(t) * cos(20 * t)", "sin(t) * sin(20 * t)", "cos(t)", "0,3.1416",
     1200, "jet", NULL},
    /*viviani 曲线：球面与柱面的交线，投影是一条 8 字*/
    {"CurveViviani", NULL, "1 + cos(t)", "sin(t)", "2 * sin(t / 2)", "0,12.566", 600, "viridis",
     NULL},
    /*matlab 内置的 humps 函数：只给 z，x 取 t、y 取 0，得到 xz 平面上的曲线*/
    {"CurveHumps", NULL, NULL, NULL,
     "1 / ((t - 0.3) * (t - 0.3) + 0.01) + 1 / ((t - 0.9) * (t - 0.9) + 0.04) - 6", "0,2", 400,
     "jet", NULL}};

/*第一项 None 表示不采样，回到 preset 选中的 csv 数据；Expr* 用表达式，其余用 C 函数*/
static const plot3d_demo_sample_t s_samples[] = {
    {"None", NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    {"Ripple", plot3d_demo_func_ripple, "-6,6", "-6,6", "40,40", "viridis", NULL, NULL},
    {"SinWave", plot3d_demo_func_sin_wave, "0,360", "0,7", "25,8", "jet", NULL, NULL},
    {"Peaks", plot3d_demo_func_peaks, "-3,3", "-3,3", "30,30", "viridis", NULL, NULL},
    {"Sombrero", plot3d_demo_func_sombrero, "-8,8", "-8,8", "36,36", "viridis", NULL, NULL},
    {"Saddle", plot3d_demo_func_saddle, "-2,2", "-2,2", "20,20", "jet", NULL, NULL},
    {"Gauss", plot3d_demo_func_gauss, "-3,3", "-3,3", "24,24", "viridis", NULL, NULL},
    {"ExprWave", NULL, "-6,6", "-6,6", "40,40", "viridis", "sin(x) * cos(y)", NULL},
    {"ExprCone", NULL, "-6,6", "-6,6", "36,36", "jet", "3 - sqrt(x * x + y * y)", NULL},
    {"ExprRose", NULL, "-6,6", "-6,6", "48,48", "viridis",
     "cos(3 * atan2(y, x)) * exp(0 - sqrt(x * x + y * y) / 4)", NULL},
    {"ExprBars", NULL, "-3,3", "-3,3", "7,7", "jet", "exp(0 - (x * x + y * y) / 2)", NULL},
    /*按半径染色：颜色不再跟着高度走，同高度的波峰颜色也不同*/
    {"ColorRadial", NULL, "-6,6", "-6,6", "40,40", "jet", "sin(x) * cos(y)", "sqrt(x * x + y * y)"},
    /*分段染色：正负半边各一色*/
    {"ColorSplit", NULL, "-6,6", "-6,6", "36,36", "viridis", "sin(x) * cos(y)",
     "if(z > 0, \"#e74c3c\", \"#2980b9\")"},
    /*矩阵输入：直接写出 6x6 的 z，没给范围时 x 与 y 就是列号与行号*/
    {"MatrixBars", NULL, NULL, NULL, NULL, "jet", NULL, NULL,
     "0,1,2,3,2,1;1,3,5,6,5,3;2,5,8,9,8,5;3,6,9,9,6,3;2,5,8,9,8,5;1,3,5,6,5,3"},
    /*矩阵由代码生成，数据量大的场合不适合写在属性里*/
    {"MatrixHeat", NULL, NULL, NULL, NULL, "viridis", NULL, NULL, NULL, 24}};

/*名字须与 main.xml 中 cb_preset 的 options 一致*/
static const plot3d_demo_preset_t s_presets[] = {
    {"FuncRipple", NULL, "surface", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f,
     s_samples + 1},
    {"FuncSinLine", NULL, "line", "X", "Y", "Z", 8, 7, 4, 1.0f, 1.0f, 0.7f, 4.0f, 2.0f,
     s_samples + 2},
    {"ExprWave", NULL, "surface", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f,
     s_samples + 7},
    {"ExprCone", NULL, "surface", "X", "Y", "Z", 6, 6, 5, 1.0f, 1.0f, 0.7f, 4.0f, 1.0f,
     s_samples + 8},
    {"ExprRose", NULL, "surface", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.5f, 4.0f, 1.0f,
     s_samples + 9},
    {"ExprBars", NULL, "cylinder", "X", "Y", "Z", 6, 6, 5, 1.0f, 1.0f, 0.7f, 4.0f, 8.0f,
     s_samples + 10},
    {"CurveHelix", NULL, "line", "X", "Y", "Z", 4, 4, 6, 0.6f, 0.6f, 1.0f, 4.0f, 2.0f, NULL,
     s_curves + 0},
    {"CurveTrefoil", NULL, "line", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.5f, 4.0f, 3.0f, NULL,
     s_curves + 1},
    {"CurveLissajous", NULL, "line", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 4.0f, 2.0f, NULL,
     s_curves + 2},
    {"CurveKnot", NULL, "line", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.5f, 4.0f, 3.0f, NULL,
     s_curves + 3},
    {"CurveByT", NULL, "line", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 4.0f, 2.0f, NULL,
     s_curves + 4},
    {"CurveDamped", NULL, "line", "X", "Y", "Z", 4, 4, 6, 0.6f, 0.6f, 1.2f, 4.0f, 2.0f, NULL,
     s_curves + 5},
    {"CurveSphere", NULL, "line", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 4.0f, 2.0f, NULL,
     s_curves + 6},
    {"CurveViviani", NULL, "line", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 4.0f, 3.0f, NULL,
     s_curves + 7},
    {"CurveHumps", NULL, "line", "T", "Y", "Z", 6, 3, 6, 1.4f, 0.4f, 1.0f, 4.0f, 3.0f, NULL,
     s_curves + 8},
    {"ColorRadial", NULL, "surface", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f,
     s_samples + 11},
    {"ColorSplit", NULL, "surface", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f,
     s_samples + 12},
    {"MatrixBars", NULL, "cylinder", "COL", "ROW", "V", 6, 6, 5, 1.0f, 1.0f, 0.6f, 4.0f, 14.0f,
     s_samples + 13},
    {"MatrixHeat", NULL, "surface", "COL", "ROW", "V", 6, 6, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f,
     s_samples + 14},
    {"SinDot", "sample_sin_dot", "dot", "X", "Y", "Z", 8, 7, 4, 1.0f, 1.0f, 0.7f, 4.0f, 2.0f},
    {"SinLine", "sample_sin_line", "line", "X", "Y", "Z", 8, 7, 4, 1.0f, 1.0f, 0.7f, 4.0f, 2.0f},
    {"SinSurface", "sample_sin_surface", "surface", "X", "Y", "Z", 8, 7, 4, 1.0f, 1.0f, 0.7f, 4.0f,
     1.0f},
    {"SinBars", "sample_sin_cylinder", "cylinder", "X", "Y", "Z", 8, 7, 4, 1.0f, 1.0f, 0.7f, 4.0f,
     3.0f},
    {"Peaks", "sample_peaks", "surface", "X", "Y", "Z", 6, 6, 5, 1.0f, 1.0f, 1.0f, 4.0f, 1.0f},
    {"Sombrero", "sample_sombrero", "surface", "X", "Y", "Z", 8, 8, 4, 1.0f, 1.0f, 0.6f, 4.0f, 1.0f},
    {"Lorenz", "sample_lorenz", "line", "X", "Y", "Z", 6, 6, 6, 1.0f, 1.0f, 1.0f, 4.0f, 2.0f},
    {"DNA", "sample_helix", "line", "X", "Y", "Turn", 4, 4, 6, 0.55f, 0.55f, 1.0f, 4.0f, 2.5f},
    {"Sphere", "sample_sphere", "dot", "X", "Y", "Z", 4, 4, 4, 1.0f, 1.0f, 1.0f, 3.0f, 2.0f},
    {"BarGrid", "sample_bars", "cylinder", "Row", "Col", "Value", 5, 5, 8, 1.0f, 1.0f, 0.7f, 4.0f,
     6.0f},
    {"Trefoil", "sample_trefoil", "line", "X", "Y", "Z", 6, 6, 4, 1.0f, 1.0f, 0.5f, 4.0f, 3.0f},
    {"PD", "sample_pd", "cylinder", "Phase", "Amp", "Count", 8, 3, 6, 1.0f, 1.0f, 0.6f, 4.0f, 4.0f},
    {"Dot", "sample_dot", "dot", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 6.0f, 2.0f},
    {"Line", "sample_line", "line", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 6.0f, 2.0f},
    {"Surface", "sample_surface", "surface", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 6.0f, 2.0f},
    {"Cylinder", "sample_cylinder", "cylinder", "X", "Y", "Z", 5, 5, 5, 1.0f, 1.0f, 1.0f, 6.0f,
     4.0f}};

static ret_t plot3d_demo_load_dataset(widget_t* chart, const char* name) {
  char url[MAX_PATH + 1];
  char asset_name[TK_NAME_LEN + 1];
  uint32_t size = 0;
  char* data = NULL;
  ret_t ret = RET_FAIL;

  return_value_if_fail(chart != NULL && name != NULL, RET_BAD_PARAMS);

  tk_snprintf(asset_name, sizeof(asset_name), "%s%s", name, PLOT3D_DEMO_DATASET_EXTNAME);
  return_value_if_fail(data_reader_asset_build_url(asset_name, ASSET_TYPE_DATA, url) != NULL,
                       RET_FAIL);

  data = (char*)data_reader_read_all(url, &size);
  return_value_if_fail(data != NULL, RET_FAIL);

  ret = plot3d_set_dataset(chart, data);
  TKMEM_FREE(data);

  return ret;
}

/*界面上显示真实的数据来源与规模，便于对照各项设置的效果*/
/*配色被表达式接管时就不走配色表了，状态栏里直接标出来*/
static const char* plot3d_demo_color_source(widget_t* chart) {
  const char* expr = widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_COLOR_EXPR, NULL);

  if (!TK_STR_IS_EMPTY(expr)) {
    return "colorexpr";
  }

  return widget_get_prop_str(chart, PLOT3D_PROP_COLORMAP, "");
}

static ret_t plot3d_demo_update_sample_info(widget_t* chart) {
  widget_t* label = NULL;
  const char* mode = NULL;
  char text[TK_NAME_LEN * 2 + 1];
  return_value_if_fail(chart != NULL, RET_BAD_PARAMS);

  label = widget_lookup(widget_get_window(chart), "lbl_sample_info", TRUE);
  return_value_if_fail(label != NULL, RET_NOT_FOUND);

  mode = widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_MODE, PLOT3D_SAMPLE_MODE_NONE);
  if (tk_str_eq(mode, PLOT3D_SAMPLE_MODE_GRID)) {
    /*demo 里表达式与内置函数互斥，属性非空即表示当前用的是表达式*/
    const char* expr = widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_Z_EXPR, NULL);

    tk_snprintf(text, sizeof(text), "%s %s %s %upt", TK_STR_IS_EMPTY(expr) ? "func" : "expr",
                widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_STEPS, ""),
                plot3d_demo_color_source(chart), plot3d_get_data_points_nr(chart));
  } else if (tk_str_eq(mode, PLOT3D_SAMPLE_MODE_MATRIX)) {
    /*矩阵要么写在属性里，要么由代码直接给*/
    const char* matrix = widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_Z_MATRIX, NULL);

    tk_snprintf(text, sizeof(text), "matrix %s %s %upt", TK_STR_IS_EMPTY(matrix) ? "data" : "text",
                plot3d_demo_color_source(chart), plot3d_get_data_points_nr(chart));
  } else if (tk_str_eq(mode, PLOT3D_SAMPLE_MODE_CURVE)) {
    const char* expr = widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_Z_EXPR, NULL);

    tk_snprintf(text, sizeof(text), "curve %s t=%s %s %upt", TK_STR_IS_EMPTY(expr) ? "func" : "expr",
                widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_T_RANGE, ""),
                plot3d_demo_color_source(chart), plot3d_get_data_points_nr(chart));
  } else {
    tk_snprintf(text, sizeof(text), "csv %upt", plot3d_get_data_points_nr(chart));
  }

  return widget_set_text_utf8(label, text);
}

/*公式回填到输入框，方便接着改；没有公式的预设把输入框清空；无 edit 时跳过*/
static ret_t plot3d_demo_fill_expr(widget_t* chart, const char* name, const char* expr) {
  widget_t* edit = widget_lookup(widget_get_window(chart), name, TRUE);
  if (edit == NULL) {
    return RET_OK;
  }

  return widget_set_text_utf8(edit, expr != NULL ? expr : "");
}

/*矩阵通常来自设备或文件，这里用一个二维正弦包填出来，演示 plot3d_set_z_matrix*/
static ret_t plot3d_demo_apply_c_matrix(widget_t* chart, uint32_t size) {
  ret_t ret = RET_OK;
  uint32_t i = 0;
  uint32_t j = 0;
  float_t* zs = TKMEM_ZALLOCN(float_t, size * size);
  return_value_if_fail(zs != NULL, RET_OOM);

  for (j = 0; j < size; j++) {
    for (i = 0; i < size; i++) {
      float_t x = (float_t)i / (size - 1) * 6 - 3;
      float_t y = (float_t)j / (size - 1) * 6 - 3;

      zs[j * size + i] = sinf(x) * cosf(y) * expf(-(x * x + y * y) / 12);
    }
  }

  ret = plot3d_set_z_matrix(chart, zs, size, size);
  TKMEM_FREE(zs);

  return ret;
}

static ret_t plot3d_demo_apply_sample(widget_t* chart, const plot3d_demo_sample_t* sample) {
  return_value_if_fail(chart != NULL && sample != NULL, RET_BAD_PARAMS);

  plot3d_set_sample_x_range(chart, sample->x_range);
  plot3d_set_sample_y_range(chart, sample->y_range);
  plot3d_set_sample_steps(chart, sample->steps);
  plot3d_set_colormap(chart, sample->colormap);
  plot3d_set_sample_color_expr(chart, sample->color_expr);
  plot3d_demo_fill_expr(chart, "edit_color_expr", sample->color_expr);

  if (sample->matrix != NULL || sample->matrix_size > 0) {
    plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_MATRIX);
    plot3d_demo_fill_expr(chart, "edit_z_expr", NULL);

    if (sample->matrix != NULL) {
      return plot3d_set_sample_z_matrix(chart, sample->matrix);
    }

    return plot3d_demo_apply_c_matrix(chart, sample->matrix_size);
  }

  return_value_if_fail(sample->func != NULL || sample->expr != NULL, RET_BAD_PARAMS);
  plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_GRID);

  /*内置函数与表达式在界面上二选一，谁提供 z 就清掉另一个*/
  plot3d_set_sample_z_expr(chart, sample->expr);
  plot3d_demo_fill_expr(chart, "edit_z_expr", sample->expr);

  return plot3d_set_grid_func(chart, sample->expr != NULL ? NULL : sample->func, NULL);
}

static ret_t plot3d_demo_apply_curve(widget_t* chart, const plot3d_demo_curve_t* curve) {
  return_value_if_fail(chart != NULL && curve != NULL, RET_BAD_PARAMS);

  plot3d_set_sample_t_range(chart, curve->t_range);
  plot3d_set_sample_t_steps(chart, curve->t_steps);
  plot3d_set_colormap(chart, curve->colormap);
  plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_CURVE);

  /*回调与表达式二选一，规则同网格采样*/
  plot3d_set_sample_x_expr(chart, curve->x_expr);
  plot3d_set_sample_y_expr(chart, curve->y_expr);
  plot3d_set_sample_z_expr(chart, curve->z_expr);
  plot3d_demo_fill_expr(chart, "edit_z_expr", curve->z_expr);
  plot3d_set_sample_color_expr(chart, curve->color_expr);
  plot3d_demo_fill_expr(chart, "edit_color_expr", curve->color_expr);

  return plot3d_set_curve_func(chart, curve->func, NULL);
}

static ret_t plot3d_demo_sync_plottype_ui(widget_t* chart) {
  uint32_t i = 0;
  widget_t* win = NULL;
  const char* plottype = NULL;
  return_value_if_fail(chart != NULL, RET_BAD_PARAMS);

  win = widget_get_window(chart);
  plottype = widget_get_prop_str(chart, PLOT3D_PROP_PLOTTYPE, "surface");
  for (i = 0; i < ARRAY_SIZE(s_plottype_buttons); i++) {
    widget_t* radio = widget_lookup(win, s_plottype_buttons[i], TRUE);
    const char* name = s_plottype_buttons[i];
    bool_t checked = FALSE;

    if (radio == NULL) {
      continue;
    }

    if (tk_str_eq(name, "btn_dot")) {
      checked = tk_str_eq(plottype, "dot");
    } else if (tk_str_eq(name, "btn_line")) {
      checked = tk_str_eq(plottype, "line");
    } else if (tk_str_eq(name, "btn_surface")) {
      checked = tk_str_eq(plottype, "surface");
    } else if (tk_str_eq(name, "btn_cylinder")) {
      checked = tk_str_eq(plottype, "cylinder");
    }

    check_button_set_value(radio, checked);
  }

  return RET_OK;
}

static const plot3d_demo_preset_t* plot3d_demo_find_preset(const char* name) {
  uint32_t i = 0;

  if (name != NULL) {
    for (i = 0; i < ARRAY_SIZE(s_presets); i++) {
      if (tk_str_eq(s_presets[i].name, name)) {
        return s_presets + i;
      }
    }
  }

  return NULL;
}

static const plot3d_demo_preset_t* plot3d_demo_first_csv_preset(void) {
  uint32_t i = 0;

  for (i = 0; i < ARRAY_SIZE(s_presets); i++) {
    if (s_presets[i].sample == NULL && s_presets[i].curve == NULL) {
      return s_presets + i;
    }
  }

  return s_presets;
}

static ret_t plot3d_demo_apply_preset(widget_t* chart, const char* name) {
  ret_t ret = RET_FAIL;
  const plot3d_demo_preset_t* preset = plot3d_demo_find_preset(name);
  return_value_if_fail(chart != NULL && preset != NULL, RET_BAD_PARAMS);

  plot3d_set_plottype(chart, preset->plottype);
  plot3d_set_xlabel(chart, preset->xlabel);
  plot3d_set_ylabel(chart, preset->ylabel);
  plot3d_set_zlabel(chart, preset->zlabel);
  plot3d_set_x_grid_count(chart, preset->x_grid_count);
  plot3d_set_y_grid_count(chart, preset->y_grid_count);
  plot3d_set_z_grid_count(chart, preset->z_grid_count);
  plot3d_set_box_aspect_x(chart, preset->box_aspect_x);
  plot3d_set_box_aspect_y(chart, preset->box_aspect_y);
  plot3d_set_box_aspect_z(chart, preset->box_aspect_z);
  plot3d_set_point_size(chart, preset->point_size);
  plot3d_set_line_width(chart, preset->line_width);

  if (preset->curve != NULL) {
    ret = plot3d_demo_apply_curve(chart, preset->curve);
  } else if (preset->sample != NULL) {
    ret = plot3d_demo_apply_sample(chart, preset->sample);
  } else {
    ret = plot3d_demo_load_dataset(chart, preset->dataset);
  }
  plot3d_demo_sync_plottype_ui(chart);
  plot3d_demo_update_sample_info(chart);

  return ret;
}

static ret_t plot3d_demo_apply_defaults(widget_t* chart) {
  return_value_if_fail(chart != NULL, RET_BAD_PARAMS);

  plot3d_set_show_grid(chart, TRUE);
  plot3d_set_show_axis_tick(chart, TRUE);
  plot3d_set_grid_color(chart, "#66666688");
  plot3d_set_xaxis_color(chart, "#e74c3cff");
  plot3d_set_yaxis_color(chart, "#27ae60ff");
  plot3d_set_zaxis_color(chart, "#2980b9ff");
  plot3d_set_tick_color(chart, "#888888ff");
  plot3d_set_axis_negative_brightness(chart, 0.5f);
  plot3d_set_enable_cache(chart, TRUE);
  plot3d_set_camera_yaw(chart, -0.75f);
  plot3d_set_camera_pitch(chart, 0.35f);
  plot3d_set_camera_distance(chart, 4.5f);
  plot3d_set_camera_z_offset(chart, 0.0f);

  return plot3d_demo_apply_preset(chart, s_presets[0].name);
}

static ret_t on_plottype(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  const char* plottype = NULL;
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  /*radio 取消勾选时忽略，只处理选中*/
  if (!widget_get_prop_bool(btn, WIDGET_PROP_VALUE, FALSE)) {
    return RET_OK;
  }

  if (tk_str_eq(btn->name, "btn_dot")) {
    plottype = "dot";
  } else if (tk_str_eq(btn->name, "btn_line")) {
    plottype = "line";
  } else if (tk_str_eq(btn->name, "btn_surface")) {
    plottype = "surface";
  } else if (tk_str_eq(btn->name, "btn_cylinder")) {
    plottype = "cylinder";
  }
  return_value_if_fail(plottype != NULL, RET_NOT_FOUND);

  plot3d_set_plottype(chart, plottype);
  /*函数采样时切类型会重新排布采样点，点数随之变化*/
  plot3d_demo_update_sample_info(chart);

  return RET_OK;
}

static ret_t on_func_changed(void* ctx, event_t* e) {
  uint32_t i = 0;
  widget_t* chart = WIDGET(ctx);
  const char* name = combo_box_get_text_of_selected(WIDGET(e->target));
  return_value_if_fail(chart != NULL && name != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_curves); i++) {
    if (tk_str_eq(s_curves[i].name, name)) {
      plot3d_demo_apply_curve(chart, s_curves + i);

      return plot3d_demo_update_sample_info(chart);
    }
  }

  for (i = 0; i < ARRAY_SIZE(s_samples); i++) {
    const plot3d_demo_sample_t* sample = s_samples + i;

    if (!tk_str_eq(sample->name, name)) {
      continue;
    }

    if (sample->func != NULL || sample->expr != NULL) {
      plot3d_demo_apply_sample(chart, sample);
    } else {
      /*None：回到 preset 选中的 csv 数据；preset 本身用函数时退回第一份 csv 数据*/
      widget_t* cb_preset = widget_lookup(widget_get_window(chart), "cb_preset", TRUE);
      const char* preset_name = combo_box_get_text_of_selected(cb_preset);
      const plot3d_demo_preset_t* preset = plot3d_demo_find_preset(preset_name);

      if (preset == NULL || preset->sample != NULL || preset->curve != NULL) {
        preset_name = plot3d_demo_first_csv_preset()->name;
      }

      plot3d_demo_apply_preset(chart, preset_name);
    }

    return plot3d_demo_update_sample_info(chart);
  }

  return RET_NOT_FOUND;
}

/*表达式取代回调：curve 模式下只换 z，其余情况切到 grid，语法非法时在状态栏提示*/
static ret_t on_expr_apply(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  widget_t* win = widget_get_window(chart);
  char expr[TK_NAME_LEN * 4 + 1];
  bool_t is_curve = FALSE;
  return_value_if_fail(chart != NULL && win != NULL, RET_BAD_PARAMS);

  is_curve = tk_str_eq(widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_MODE, ""),
                       PLOT3D_SAMPLE_MODE_CURVE);
  widget_get_text_utf8(widget_lookup(win, "edit_z_expr", TRUE), expr, sizeof(expr));
  if (TK_STR_IS_EMPTY(expr)) {
    /*公式为空就没有数据源了，保持原图并提示*/
    return widget_set_text_utf8(widget_lookup(win, "lbl_sample_info", TRUE), "empty expr");
  }

  if (plot3d_set_sample_z_expr(chart, expr) != RET_OK) {
    return widget_set_text_utf8(widget_lookup(win, "lbl_sample_info", TRUE), "bad expr");
  }

  widget_get_text_utf8(widget_lookup(win, "edit_color_expr", TRUE), expr, sizeof(expr));
  if (plot3d_set_sample_color_expr(chart, expr) != RET_OK) {
    return widget_set_text_utf8(widget_lookup(win, "lbl_sample_info", TRUE), "bad color expr");
  }

  if (is_curve) {
    plot3d_set_curve_func(chart, NULL, NULL);
  } else {
    plot3d_set_grid_func(chart, NULL, NULL);
    plot3d_set_sample_mode(chart, PLOT3D_SAMPLE_MODE_GRID);
  }

  return plot3d_demo_update_sample_info(chart);
}

static ret_t on_colormap_changed(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  const char* name = combo_box_get_text_of_selected(WIDGET(e->target));
  return_value_if_fail(chart != NULL && name != NULL, RET_BAD_PARAMS);

  plot3d_set_colormap(chart, name);

  return plot3d_demo_update_sample_info(chart);
}

static ret_t on_sample_steps_step(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  int32_t delta = 0;
  uint32_t cols = 0;
  uint32_t rows = 0;
  char steps[TK_NAME_LEN + 1];
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  delta = tk_str_eq(btn->name, "btn_sample_steps_inc") ? 4 : -4;
  if (tk_str_eq(widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_MODE, ""),
                PLOT3D_SAMPLE_MODE_CURVE)) {
    /*曲线只有一个方向，一次多加几十个点才看得出变化*/
    int32_t t_steps = widget_get_prop_int(chart, PLOT3D_PROP_SAMPLE_T_STEPS, 0) + delta * 10;

    plot3d_set_sample_t_steps(chart, (uint32_t)tk_max(t_steps, 0));

    return plot3d_demo_update_sample_info(chart);
  }

  plot3d_parse_steps(widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_STEPS, "20,20"), &cols, &rows);
  tk_snprintf(steps, sizeof(steps), "%d,%d", (int32_t)cols + delta, (int32_t)rows + delta);
  plot3d_set_sample_steps(chart, steps);

  return plot3d_demo_update_sample_info(chart);
}

/*以范围中心为轴整体缩放，看采样窗口变化对图形的影响*/
static ret_t plot3d_demo_zoom_range(widget_t* chart, const char* prop, float_t scale) {
  float_t min_v = 0;
  float_t max_v = 0;
  float_t center = 0;
  float_t half = 0;
  char range[TK_NAME_LEN + 1];
  return_value_if_fail(
      plot3d_parse_range(widget_get_prop_str(chart, prop, "0,1"), &min_v, &max_v) == RET_OK,
      RET_BAD_PARAMS);

  center = (min_v + max_v) / 2;
  half = (max_v - min_v) * scale / 2;
  tk_snprintf(range, sizeof(range), "%.3f,%.3f", center - half, center + half);

  return widget_set_prop_str(chart, prop, range);
}

static ret_t on_sample_range_step(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  float_t scale = 0;
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  scale = tk_str_eq(btn->name, "btn_sample_range_inc") ? 1.25f : 0.8f;
  if (tk_str_eq(widget_get_prop_str(chart, PLOT3D_PROP_SAMPLE_MODE, ""),
                PLOT3D_SAMPLE_MODE_CURVE)) {
    plot3d_demo_zoom_range(chart, PLOT3D_PROP_SAMPLE_T_RANGE, scale);

    return plot3d_demo_update_sample_info(chart);
  }

  plot3d_demo_zoom_range(chart, PLOT3D_PROP_SAMPLE_X_RANGE, scale);
  plot3d_demo_zoom_range(chart, PLOT3D_PROP_SAMPLE_Y_RANGE, scale);

  return plot3d_demo_update_sample_info(chart);
}

static ret_t on_step(void* ctx, event_t* e) {
  uint32_t i = 0;
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_steps); i++) {
    const plot3d_demo_step_t* iter = s_steps + i;

    if (tk_str_eq(iter->name, btn->name)) {
      float_t value = widget_get_prop_float(chart, iter->prop, 0.0f) + iter->delta;

      value = tk_max(iter->min, tk_min(iter->max, value));

      return iter->set(chart, value);
    }
  }

  return RET_NOT_FOUND;
}


static ret_t on_count_step(void* ctx, event_t* e) {
  uint32_t i = 0;
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_count_steps); i++) {
    if (tk_str_eq(s_count_steps[i].name, btn->name)) {
      int32_t value = (int32_t)widget_get_prop_int(chart, s_count_steps[i].prop, 5) +
                      s_count_steps[i].delta;
      return s_count_steps[i].set(chart, (uint32_t)value);
    }
  }
  return RET_NOT_FOUND;
}

static ret_t on_grid_plane_step(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  widget_t* btn = WIDGET(e->target);
  return_value_if_fail(chart != NULL && btn != NULL && btn->name != NULL, RET_BAD_PARAMS);

  if (tk_str_eq(btn->name, "btn_xy_grid_inc")) return plot3d_step_xy_grid_position(chart, 1);
  if (tk_str_eq(btn->name, "btn_xy_grid_dec")) return plot3d_step_xy_grid_position(chart, -1);
  if (tk_str_eq(btn->name, "btn_xz_grid_inc")) return plot3d_step_xz_grid_position(chart, 1);
  if (tk_str_eq(btn->name, "btn_xz_grid_dec")) return plot3d_step_xz_grid_position(chart, -1);
  if (tk_str_eq(btn->name, "btn_yz_grid_inc")) return plot3d_step_yz_grid_position(chart, 1);
  if (tk_str_eq(btn->name, "btn_yz_grid_dec")) return plot3d_step_yz_grid_position(chart, -1);
  return RET_NOT_FOUND;
}

static ret_t on_preset_changed(void* ctx, event_t* e) {
  widget_t* chart = WIDGET(ctx);
  const char* name = combo_box_get_text_of_selected(WIDGET(e->target));
  return_value_if_fail(chart != NULL && name != NULL, RET_BAD_PARAMS);

  return plot3d_demo_apply_preset(chart, name);
}

/*下拉框选中的颜色直接写到图表的对应属性上*/
static ret_t on_color_changed(void* ctx, event_t* e) {
  uint32_t i = 0;
  widget_t* chart = WIDGET(ctx);
  widget_t* combo = WIDGET(e->target);
  const char* color = combo_box_get_text_of_selected(combo);
  return_value_if_fail(chart != NULL && combo != NULL && color != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_color_combos); i++) {
    if (tk_str_eq(combo->name, s_color_combos[i].name)) {
      return widget_set_prop_str(chart, s_color_combos[i].prop, color);
    }
  }

  return RET_NOT_FOUND;
}

static ret_t on_switch_changed(void* ctx, event_t* e) {
  uint32_t i = 0;
  widget_t* chart = WIDGET(ctx);
  widget_t* check = WIDGET(e->target);
  return_value_if_fail(chart != NULL && check != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_switches); i++) {
    if (tk_str_eq(check->name, s_switches[i].name)) {
      bool_t value = widget_get_prop_bool(check, WIDGET_PROP_VALUE, s_switches[i].default_value);

      return widget_set_prop_bool(chart, s_switches[i].prop, value);
    }
  }

  return RET_NOT_FOUND;
}

static ret_t plot3d_demo_bind_child(widget_t* win, const char* name, uint32_t type,
                                     event_func_t on_event, void* ctx) {
  widget_t* child = widget_lookup(win, name, TRUE);
  if (child == NULL) {
    return RET_NOT_FOUND;
  }

  widget_on(child, type, on_event, ctx);

  return RET_OK;
}

static ret_t plot3d_demo_bind_events(widget_t* win, widget_t* chart) {
  uint32_t i = 0;
  return_value_if_fail(win != NULL && chart != NULL, RET_BAD_PARAMS);

  for (i = 0; i < ARRAY_SIZE(s_plottype_buttons); i++) {
    plot3d_demo_bind_child(win, s_plottype_buttons[i], EVT_VALUE_CHANGED, on_plottype, chart);
  }

  for (i = 0; i < ARRAY_SIZE(s_steps); i++) {
    plot3d_demo_bind_child(win, s_steps[i].name, EVT_CLICK, on_step, chart);
  }

  for (i = 0; i < ARRAY_SIZE(s_count_steps); i++) {
    plot3d_demo_bind_child(win, s_count_steps[i].name, EVT_CLICK, on_count_step, chart);
  }

  for (i = 0; i < ARRAY_SIZE(s_grid_plane_buttons); i++) {
    plot3d_demo_bind_child(win, s_grid_plane_buttons[i], EVT_CLICK, on_grid_plane_step, chart);
  }

  plot3d_demo_bind_child(win, "btn_expr_apply", EVT_CLICK, on_expr_apply, chart);
  plot3d_demo_bind_child(win, "btn_sample_steps_dec", EVT_CLICK, on_sample_steps_step, chart);
  plot3d_demo_bind_child(win, "btn_sample_steps_inc", EVT_CLICK, on_sample_steps_step, chart);
  plot3d_demo_bind_child(win, "btn_sample_range_dec", EVT_CLICK, on_sample_range_step, chart);
  plot3d_demo_bind_child(win, "btn_sample_range_inc", EVT_CLICK, on_sample_range_step, chart);

  plot3d_demo_bind_child(win, "cb_preset", EVT_VALUE_CHANGED, on_preset_changed, chart);
  plot3d_demo_bind_child(win, "cb_func", EVT_VALUE_CHANGED, on_func_changed, chart);
  plot3d_demo_bind_child(win, "cb_colormap", EVT_VALUE_CHANGED, on_colormap_changed, chart);

  for (i = 0; i < ARRAY_SIZE(s_color_combos); i++) {
    plot3d_demo_bind_child(win, s_color_combos[i].name, EVT_VALUE_CHANGED, on_color_changed, chart);
  }

  for (i = 0; i < ARRAY_SIZE(s_switches); i++) {
    plot3d_demo_bind_child(win, s_switches[i].name, EVT_VALUE_CHANGED, on_switch_changed, chart);
  }

  return RET_OK;
}

/**
 * 初始化
 */
ret_t application_init(void) {
  widget_t* chart = NULL;
  widget_t* win = NULL;

  plot3d_register_all();
  win = window_open("main");
  return_value_if_fail(win != NULL, RET_FAIL);

  chart = widget_lookup(win, "chart", TRUE);
  return_value_if_fail(chart != NULL, RET_FAIL);

  plot3d_demo_bind_events(win, chart);
  plot3d_demo_apply_defaults(chart);

  return RET_OK;
}

/**
 * 退出
 */
ret_t application_exit(void) {
  log_debug("application_exit\n");
  return RET_OK;
}
