# ImageWithUI

ImageWithUI 是一个使用 **C++17 + Qt 5 Widgets** 编写的桌面图像处理程序，采用类 Photoshop 的深色三栏界面，内置选区、图层、全局撤销/重做、实时参数预览与交互式自由变换等能力。

> 当前版本：`0.1` · 定位：教学 / 实验向的图像处理工具

## 项目简介

- 图像数据在内存中统一为 **24 位 BGR 紧凑布局、自底向上**（第 0 行对应画面最底行）。
- 图像算法（灰度、模糊、锐化、形态学、卷积、几何变换等）全部为项目自有实现，不依赖 OpenCV。
- BMP 使用项目自带的读写器；PNG / JPEG / TIFF / WebP 等格式由 Qt 图像插件解码。
- 所有耗时算子按像素分段多线程执行，参数调整支持实时预览，提交时才写入历史。

## 功能

| 分类    | 功能                                                        |
| ----- | --------------------------------------------------------- |
| 文件与文档 | 多文档标签页、拖入打开、打开/保存/另存、还原成原图、未保存修改提示                        |
| 历史记录  | 应用级全局撤销 / 重做（最多 40 步）、跨文档撤销自动切换标签、历史面板点击跳转                |
| 图像调整  | 亮度、对比度、饱和度、提亮暗部、压住过曝、让边缘更清晰、黑白分界                          |
| 模糊    | 均值模糊、中值滤波、高斯模糊                                            |
| 颜色    | 转为黑白、反相、互补色、自动对比度、校正偏色、自动拉大明暗、调色曲线                        |
| 几何变换  | 水平/垂直翻转、顺时针/逆时针 90°、对角线镜像、裁剪、缩放图像、任意角度旋转                  |
| 滤镜    | 鱼眼、马赛克、Sobel 边缘、智能锐化、自定义 3×3 卷积、放大眼睛、图片微调（液化）             |
| 形态学   | 腐蚀、膨胀、开运算、闭运算（3×3 结构元）                                    |
| 选区    | 矩形 / 手绘 / 多边形 / 自动贴边四种工具；羽化、扩大、缩小、磨平、反选、加选/减选             |
| 选区编辑  | 复制 / 粘贴到选区、填色、描边、清空内容、裁到选区、自由变换（拖拽手柄或输入数值）                |
| 图层    | 新建 / 复制 / 删除 / 上移 / 下移 / 合并、可见性、不透明度、混合模式（正常 / 正片叠底 / 滤色） |
| 界面    | 深色 / 浅色主题、左侧工具条、右侧调整与图层面板、状态栏提示、窗口自适应                     |

详细的功能与交互说明见 [ROADMAP.md](ROADMAP.md)；后续规划与可选依赖同样记录在其中。

## 环境要求

| 依赖    | 版本 / 说明                                  |
| ----- | ---------------------------------------- |
| 操作系统  | Windows 10 / 11 x64                      |
| 编译器   | Visual Studio 2022，需安装「使用 C++ 的桌面开发」工作负载 |
| Qt    | Qt 5.15（Widgets、Gui、Concurrent 模块）       |
| CMake | 3.21 及以上                                 |

项目源码除 Qt 外不依赖任何第三方库。ROADMAP 中的可选能力（OpenCV、LibRaw、Exiv2、ONNX Runtime 等）默认关闭，不启用时仍可正常编译运行。

## 构建

### 方式一：使用脚本（推荐）

先按本机环境修改 [ImageWithUI/scripts/build.ps1](ImageWithUI/scripts/build.ps1) 顶部的工具链路径：

```powershell
$VsTools    = "D:\Microsoft Visual Studio\VC\Tools\MSVC\14.44.35207"
$WinSdk     = "D:\Windows Kits\10"
$WinSdkVer  = "10.0.19041.0"
$QtPrefix   = "D:\Qt\5.15.2\msvc2019_64"
$CMakeExe   = "D:\cmake-gui\bin\cmake.exe"
$MSBuildExe = "D:\Microsoft Visual Studio\MSBuild\Current\Bin\amd64\MSBuild.exe"
```

然后在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File ImageWithUI\scripts\build.ps1 -VSBuild
```

- 生成目录：`ImageWithUI/build-vs/`
- 程序产物：`ImageWithUI/build-vs/Release/ImageWithUI.exe`
- 常用参数：`-Config Debug`（调试构建）、`-Clean`（先清理再构建）、`-NMake`（改用 NMake）

### 方式二：直接使用 CMake

```powershell
cmake -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="D:\Qt\5.15.2\msvc2019_64" `
      -S ImageWithUI -B ImageWithUI/build-vs
cmake --build ImageWithUI/build-vs --config Release
```

> 必须使用动态 CRT（`/MD`），与 Qt 官方 DLL 保持一致，否则跨 DLL 传递 `QString` / `std::string` 可能导致堆损坏。

## 使用

直接运行 `ImageWithUI.exe` 打开图形界面，通过工具栏「打开图片」或 `Ctrl+N` 载入图像。

命令行参数：

| 参数                                                | 说明                                                   |
| ------------------------------------------------- | ---------------------------------------------------- |
| `--selftest [图片路径]`                               | 运行内置自检，结果输出到控制台并写入 `%TEMP%\imagewithui_selftest.txt` |
| `--screenshot <输出.png> [图片路径] [--select=x,y,w,h]` | 启动后截取窗口画面并退出，用于界面回归检查                                |

常用快捷键：

| 快捷键                                              | 功能                             |
| ------------------------------------------------ | ------------------------------ |
| `Ctrl+N`                                         | 打开图片                           |
| `Ctrl+S`                                         | 保存 / 导出                        |
| `Ctrl+Z` / `Ctrl+Y`                              | 撤销 / 重做                        |
| `Ctrl+R`                                         | 还原成原图                          |
| `Ctrl+T`                                         | 自由变换（拖拽手柄），`Enter` 提交、`Esc` 取消 |
| `Ctrl+Shift+T`                                   | 切换深色 / 浅色主题                    |
| `Ctrl+Shift+A` / `Ctrl+Shift+I` / `Ctrl+Shift+D` | 全选 / 反选 / 取消选区                 |
| `Ctrl+Shift+C` / `Ctrl+Shift+V`                  | 复制选区 / 粘贴到选区                   |
| `Ctrl+Shift+X`                                   | 裁到选区                           |
| `Ctrl+Shift+Delete`                              | 清空选区内容                         |
| `Enter` / `Esc` / `Backspace`                    | 多边形选区：闭合 / 取消 / 撤回上一点          |

鼠标修饰键：`Shift` 加选，`Alt` 减选；在选区内拖动可移动选区，按住 `Ctrl` 拖动则连同像素一起移动。

> 当前版本不含测试图片目录 `ImageWithUI/resources/image/`；历史提交中仍可能存在其中的文件（见 [NOTICE](NOTICE)）。运行自检时请通过参数传入本地图片，或自行把测试图放回该目录。

## 目录结构

```
ImageWithUI/
├─ LICENSE                      MIT 许可证全文
├─ NOTICE                       第三方素材与依赖的授权说明
├─ README.md                    项目说明
├─ ROADMAP.md                   功能规划、可选依赖与选型
├─ .gitignore
└─ ImageWithUI/                 工程目录
   ├─ CMakeLists.txt            构建入口
   ├─ CMakePresets.json         CMake 预设
   ├─ include/
   │  ├─ core/                  BMP 读写、图像算子、调试输出、公共头文件
   │  ├─ ui/                    主窗口、主题、滑块控件、对话框
   │  ├─ tools/                 图片微调、放大眼睛、色彩映射
   │  └─ tests/                 自检接口
   ├─ src/
   │  ├─ app/                   main.cpp（入口与命令行参数）
   │  ├─ core/                  图像底层实现
   │  ├─ ui/                    主窗口与对话框实现
   │  ├─ tools/                 工具窗口实现
   │  └─ tests/                 自检实现
   ├─ ui/                       Qt Designer 界面文件（.ui）
   ├─ resources/
   │  ├─ icon.qrc               图标资源清单
   │  ├─ icon/                  界面图标
   │  └─ image/                 本地测试图片（已 gitignore，不随仓库分发）
   └─ scripts/
      └─ build.ps1              一键构建脚本
```

## 贡献指南

欢迎提交 Issue 与 Pull Request。

如果对你有用，请点一个Star！

## License

本项目采用 **MIT License**，版权人：素曜。许可证全文见 [LICENSE](LICENSE)。

MIT 许可的覆盖范围仅限素曜拥有著作权的源代码、文档与 `ImageWithUI/resources/icon/` 下的自产图标；仓库历史提交中的上述自有内容同样按 MIT 授权。

- `ImageWithUI/resources/image/` 已从当前版本移除并加入 `.gitignore`。Git 历史提交中仍可能包含 `Lenna.bmp`、`back.bmp`、`image.bmp`、`lrh.bmp` 等文件；其中 `Lenna.bmp` 为第三方测试图，其余文件来源无法确认，均不受 MIT 覆盖，使用前请自行确认授权。
- Qt 5 采用 LGPLv3 / GPLv3 / 商业授权，不属于本项目，与 MIT 许可相互独立；分发二进制时需遵守其许可要求。
- 第三方依赖与素材的完整说明见 [NOTICE](NOTICE)。

## 给作者加鸡腿

| 支付宝 | 微信支付 |
| :---: | :---: |
| <img src="ImageWithUI/resources/donation/alipay.jpg" alt="支付宝收款码" width="240"> | <img src="ImageWithUI/resources/donation/wechat.png" alt="微信收款码" width="240"> |
