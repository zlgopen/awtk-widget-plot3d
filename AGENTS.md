# awtk-widget-plot3d AI Agent 指引

本文件为在本仓库内工作的 AI 助手提供统一约束，目标是：**快速改动、可复现构建、可验证结果**。

## 项目定位

- 本仓库是 AWTK 自定义控件项目，核心控件为 `plot3d`。
- 主要代码目录：
  - `src/plot3d/`：控件实现
  - `demos/`：示例入口
  - `tests/`：单元测试
  - `design/` 与 `res/`：设计资源与打包结果

## 常用命令

- 生成资源：`python scripts/update_res.py all`
- 编译（PC）：`scons`
- 运行示例：`./bin/demo_csv`、`./bin/demo_grid`、`./bin/demo_expr`、`./bin/demo_matrix`、`./bin/demo_curve`
- 运行测试（若已构建测试目标）：`./bin/runTest`（单元测试程序名固定为 `runTest`）

## AI 修改规则

- 只改与当前任务相关的文件，避免顺手重构无关代码。
- **代码风格必须遵循 AWTK**（不是“接近”而是“保持一致”）。
- 保持与现有代码风格一致（命名、缩进、宏/函数组织方式）。
- 默认采用 **TDD（Red-Green-Refactor）**：先写失败测试，再写最小实现，最后重构。
- C 代码优先小步修改，避免一次性大规模迁移。
- 涉及行为变更时，优先补充或更新 `tests/` 中对应测试。
- 不要提交或暴露本地环境/密钥类信息。

## AWTK 风格重点

- 命名延续 AWTK 习惯：类型 `xxx_t`、函数 `module_action`、常量/宏全大写下划线。
- 参数校验优先使用 AWTK 常用宏（如 `return_value_if_fail(...)`）。
- 返回值/状态码使用 `ret_t` 与 `RET_OK/RET_BAD_PARAMS/...` 语义。
- 代码排版遵循仓库现有样式与 `.clang-format`，不要引入其他风格体系。
- 头文件公开属性/函数须按 AWTK API 注释格式编写（参考 `../awtk/docs/api_doc.md`）；测试用符号注明 `/*for test*/`，不写标准注释。详见 `.cursor/rules/awtk-api-doc.mdc`。

## 提交前最小检查

- 能通过 `scons` 编译。
- 若改动公开头文件，执行 `../awtk/bin/api_doc_lint src`。
- 若改动影响资源加载或界面，重新执行资源生成并验证运行。
- 若改动影响逻辑，至少执行一次对应测试或提供未执行原因。
- 对于功能/修复类改动，说明对应 Red（失败）与 Green（通过）的测试证据。

