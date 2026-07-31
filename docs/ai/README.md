# AI 协作说明

本目录用于维护本仓库的 AI 协作规范与流程，减少重复沟通成本。

## 当前已启用

- 仓库级指引：`AGENTS.md`
- Cursor 规则：
  - `.cursor/rules/project-baseline.mdc`
  - `.cursor/rules/custom-widget-change-checklist.mdc`
  - `.cursor/rules/awtk-code-style.mdc`
  - `.cursor/rules/tdd-workflow.mdc`
  - `.cursor/rules/tdd-exemption-reason.mdc`
  - `.cursor/rules/unit-test-binary-name.mdc`

## 风格约束（重要）

- 本仓库 AI 生成/修改代码必须遵循 AWTK 风格。
- 任何新改动都应优先与现有 AWTK 风格保持一致，而不是引入个人偏好。

## 推荐工作流

1. 先阅读 `AGENTS.md` 与规则文件，再开始修改。
2. 功能/修复改动先走 TDD（Red-Green-Refactor）。
3. 以“最小必要改动”完成任务，避免无关改动。
4. 按改动类型执行资源生成、编译与测试验证。
5. 交付时附带验证步骤和结果（或说明未执行原因）。

## 后续可扩展

- 在本目录新增专题文档（如渲染规范、测试样例、发布流程）。
- 在 `.cursor/rules/` 新增场景规则（如仅针对测试、仅针对资源）。

