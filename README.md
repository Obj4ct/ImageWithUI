# ImageWithUI

这是一个带有界面(Qt)的数字图像处理程序

2026年开始此项目转为AI协助开发

在此之前为古法编程

## 构建与运行

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -VSBuild
```

- 程序：`build-vs/Release/ImageWithUI.exe`
- 自检：`build-vs/Release/ImageWithUI.exe --selftest`（当前 47 项，全部通过）

## 依赖

- **必需**：Qt 5.15（Widgets / Gui）、MSVC 2022 x64、CMake。当前代码除 Qt 外**不依赖任何第三方库**。
- **可选**（允许引入，全部通过 CMake 开关启用、默认关闭）：OpenCV（透视校正 / 内容识别填充 / CLAHE）、
  LibRaw（RAW 解码）、Exiv2（EXIF）、ONNX Runtime 或 ncnn（AI 推理）、Tesseract（OCR）、ZXing-cpp（二维码）。
- 选型、许可证（只接受 MIT / BSD / Apache-2.0，LGPL 仅动态链接）与 vcpkg 集成方式见
  [ROADMAP.md](ROADMAP.md) 第十二节；任何开关关闭时程序仍可编译并降级运行。

## 功能一览

| 分类 | 功能 |
|---|---|
| 文件 | 打开 BMP / PNG / JPG / JPEG / GIF / TIFF / TGA / WebP / ICO / ICNS / SVG（透明按白底合成、按 EXIF 方向自动旋正）；保存 / 导出 BMP、PNG、JPEG、TIFF、WebP（JPG 质量 92）；撤销 / 重做 / 重置 |
| 色彩 | 灰度化、颜色反转、补色、自动对比度、色彩平衡、亮度 / 对比度 / 饱和度、阴影 / 高光 |
| 模糊 | 均值模糊、中值滤波、高斯模糊 |
| 变换 | 水平翻转、垂直翻转、顺/逆时针 90°、沿主/副对角线镜像、拖拽框选裁剪、任意角度旋转 |
| 增强 | 直方图均衡化、USM 锐化（半径 / 强度 / 阈值）、高反差保留 |
| 形态学 | 腐蚀、膨胀、开运算、闭运算（3×3 结构元） |
| 卷积 | 3×3 自定义卷积核（恒等 / 高斯 / 锐化 / 浮雕 / 描边 / 拉普拉斯 预设） |
| 高级 | 图层混合、马赛克、Sobel 边缘检测、阈值处理、鱼眼、眼部放大（径向放大，强度 0~1）、图像微调（瘦脸）、色彩映射、插值缩放 |
| 界面 | 深色 / 浅色主题（科技风配色）、右侧「图像调整 / 几何变换」分组快捷面板、**历史记录面板（点击可跳转步骤）**、图层面板占位、状态栏图像信息、图像适应窗口 |

后续功能规划（图层系统、选区、画笔修复、批量处理、AI 增强、色彩专业调整等）见 [ROADMAP.md](ROADMAP.md)，
其中第十节为扩展功能清单，第十一节为参考开源项目（含许可证差异说明）。

## 约定

- 输入支持上表列出的常见格式，统一转成内部 24 位 BGR；其中 BMP 仍走项目自有解析器
  （只接受 24 位未压缩，自顶向下存储的文件会自动翻转为自底向上），其余格式由 Qt 图像插件解码。
- 内部像素为 `std::vector<uint8_t>`（BGR 连续），内存第 0 行对应画面**最底行**（BMP 自底向上），显示时统一翻转。
- 几何算子（旋转 / 翻转 / 裁剪）同时更新内容四边形 `ContentQuad`，保证反复变换不丢边、不变黑。
- 快捷键：`Ctrl+N` 打开、`Ctrl+S` 保存、`Ctrl+Z` 撤销、`Ctrl+Y` 重做、`Ctrl+R` 重置、`Ctrl+T` 切换主题。
