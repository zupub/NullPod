---
kind: frontend_style
name: ESP32-S3 SD卡文件管理器 Web 前端样式体系
category: frontend_style
scope:
    - '**'
source_files:
    - data/style.css
    - data/index.html
    - data/app.js
---

## 1. 系统/方法

仓库中唯一的前端 UI 位于 `data/` 目录，是一个由 ESP32 LittleFS 静态资源托管的纯 HTML + CSS + JavaScript 单页应用（SD 卡文件管理器）。样式方案为**原生 CSS**，未引入任何框架或预处理器：
- 无 Tailwind、Bootstrap、Sass/SCSS、CSS Modules、PostCSS 等工具链。
- 无设计令牌（design tokens）文件或主题变量；颜色、字号、间距等值直接以硬编码字面量写在 `style.css` 中。
- 布局采用 Flexbox 与 CSS Grid 组合，通过一个 `.container` 包裹 header + sidebar + main-content 三段式布局。

## 2. 关键文件

- `data/style.css`（390 行）—— 全部样式定义，包含组件类、配色、响应式断点。
- `data/index.html`（92 行）—— 页面结构，仅引用 `/style.css` 和 `/app.js`。
- `data/app.js` —— 交互逻辑（拖拽上传、队列、进度条、文件列表渲染），不承载样式。

## 3. 架构与约定

### 命名约定
- 所有样式使用 **BEM 风格的 class 名**（如 `.upload-area`、`.file-header`、`.file-item`、`.progress-bar`、`.queue-item`、`.status-indicator`），未见 ID 选择器用于样式。
- 语义化标签优先（`header`、`h1`、`h3`），但视觉样式仍通过 class 覆盖。

### 色彩体系（硬编码色板）
| 用途 | 色值 |
|---|---|
| 主色 / 按钮 / 链接 / 进度条 | `#667eea`（悬停 `#5a6fd8`） |
| 头部背景 | `#4a5568` |
| 成功态 | `#38a169`（悬停 `#2f855a`） |
| 危险/删除 | `#e53e3e`（悬停 `#c53030`） |
| 次要按钮 | `#a0aec0`（悬停 `#718096`） |
| 文本/边框 | `#333`、`#718096`、`#cbd5e0`、`#e2e8f0` |
| 背景渐变 | `linear-gradient(135deg, #667eea 0%, #764ba2 100%)` |
| 文件夹图标 | `#ecc94b` |

### 字体与排版
- 全局字体栈：`'Segoe UI', Tahoma, Geneva, Verdana, sans-serif`。
- 标题字号集中在 `24px`（`h1`），正文 `14px`，辅助信息 `12px`。
- 统一 `box-sizing: border-box`，全局 margin/padding 归零。

### 组件样式清单
- 按钮族：`.btn`、`.btn-danger`、`.btn-success`、`.btn-secondary`，均带圆角 `5px`、内边距 `10px 20px`、`transition: background 0.3s`。
- 上传区：`.upload-section` + `.upload-area`（虚线边框 `2px dashed #cbd5e0`，hover/dragover 时变为主色边框 + `#f0f4ff` 背景）。
- 文件列表：`.file-list` + `.file-header` + `.file-item`，使用 `grid-template-columns: 3fr 1fr 1fr 1fr` 四列布局。
- 进度条：`.progress-container` + `.progress-bar` + `.progress`，宽度通过 JS 动态更新并带 `transition: width 0.3s`。
- 状态消息：`.success`（浅绿底）、`.error`（浅红底）。
- 上传队列：`.upload-queue` + `.queue-item`，按 `.status-queued` / `.status-uploading` / `.status-completed` / `.status-error` 着色。
- 面包屑：`.breadcrumb` + `.breadcrumb-separator` + `.current-folder`。

### 响应式策略
- 单一断点 `@media (max-width: 768px)`：
  - `.content` 由 flex-row 切换为 column。
  - `.sidebar` 宽度变为 `100%`，移除右边框改为底部边框。
  - 文件列表 grid 从 4 列缩减为 3 列（隐藏“修改时间”列）。
  - `.toolbar` 纵向排列。
  - `.folder-section` 在移动端上边距从 `30px` 降为 `20px`。

### 动画与过渡
- 按钮 hover、上传区域 hover/dragover、进度条宽度变化均使用 `transition`（时长 `0.3s`）。
- 无 CSS keyframes 动画。

## 4. 约定与约束

- **无构建步骤**：`style.css` 直接作为静态资源由 ESP32 的 HTTP 服务器提供，浏览器直接加载，未经过编译/压缩/前缀处理。
- **无 CSS 变量**：颜色、尺寸、间距全部以字面量硬编码，不存在 `:root { --xxx }` 形式的主题变量。
- **无外部依赖**：未引入任何 CDN 或第三方 CSS 包；图标使用 emoji（如 `📁`）而非字体图标或 SVG sprite。
- **BEM 风格 class 命名**是实际观察到的组织约定，但未通过 lint 规则强制。
- 响应式仅在 `768px` 一处断点处理，未实现更细粒度的多断点适配。