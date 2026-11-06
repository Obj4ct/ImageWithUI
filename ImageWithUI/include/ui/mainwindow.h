/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"qt_headers.h"
#include <QListWidgetItem>
#include <QListWidget>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QTabBar>
#include <QPolygon>
#include "bmp_file.h"
#include "image_ops.h"
#include "eye_zoom.h"
#include "retouch.h"
#include "color_map.h"

// PNG 导出参数（仅在保存/导出为 PNG 时使用）
struct PngExportOptions {
    int compression = 6;   // 压缩级别 0~9，越大文件越小、编码越慢
    int dpi = 96;          // 写入 pHYs 的 DPI；0 表示不写分辨率信息
};

struct ReturnValue{
    bool isNull = true;
    bool isNumeric = false;
    double_t value = 0.0;
};
// 算子执行上下文：统一以【像素】为单位描述尺寸与分段
struct OpContext {
    int32_t width = 0;
    int32_t height = 0;
    size_t pixelCount = 0;    // 总像素数
    size_t segmentSize = 0;   // 每个线程负责的像素数
    int numThreads = 1;
    ContentQuad quad;         // 内容区域(旋转会更新它)
};

// 选区掩码：0~255 表示覆盖度(0 = 完全不在选区内，255 = 完全在选区内)
// 与图像同尺寸，按【视觉坐标】行优先存储(y 向下)；
// data 为空表示"没有选区"(此时调整类算子作用于整幅图)
struct SelectionMask {
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint8_t> data;

    bool isEmpty() const { return width <= 0 || height <= 0 || data.empty(); }
    void clear() {
        width = 0;
        height = 0;
        std::vector<uint8_t>().swap(data);
    }
};

// 选区通道：把一份选区掩码命名保存，之后可重新载入
struct SelectionChannel {
    QString name;
    SelectionMask mask;
};

// 文字图层参数：保存可重新渲染的文本元数据(只有文字图层带这份数据)
struct LayerText {
    QString text;
    QFont font;
    QColor color = QColor(0, 0, 0);
    int alignment = 0;      // 0 左 / 1 中 / 2 右
    QPoint anchor;          // 文字块左上角(图像像素坐标，视觉 y 向下)
    bool valid = false;
};

// 图层：与画布同尺寸的 BGR 像素 + 叠放属性
// 约定：所有图层尺寸都与文档画布一致；imageData 始终是【当前活动图层】的像素
struct Layer {
    QString name;
    std::vector<uint8_t> data;
    bool visible = true;
    double opacity = 1.0;   // 0.0 ~ 1.0
    int blend = 0;          // 0 正常 / 1 正片叠底 / 2 滤色
    // 图层蒙版：与画布同尺寸的覆盖度(0 = 该像素完全隐藏，255 = 完全显示)
    // 空蒙版表示该图层没有蒙版；maskEnabled=false 时临时停用(保留数据)
    SelectionMask mask;
    bool maskEnabled = true;
    // 图层 alpha：与画布同尺寸的覆盖度(0 = 完全透明，255 = 完全不透明)
    // 空 alpha 表示该层整层不透明(默认)；橡皮擦除就是在减少这份 alpha
    SelectionMask alpha;
    LayerText text;         // 文字图层可再次编辑所需的元数据
};

// 历史快照：必须连同尺寸一起保存，
// 因为旋转等算子会改变画布宽高，只存像素会导致撤销/保存时尺寸对不上。
// data 保存【当前活动图层】的像素；layers 保存整份图层栈(活动图层的像素留空，
// 恢复时用 data 填回，避免同一份像素存两遍)
struct ImageSnapshot {
    std::vector<uint8_t> data;
    int32_t width = 0;
    int32_t height = 0;
    ContentQuad quad;         // 内容区域(撤销/重做要一起恢复)
    std::vector<Layer> layers;
    int activeLayer = 0;
};

// 全局历史记录：撤销栈存“操作前状态”，重做栈存“操作后状态”
struct HistoryEntry {
    int docId = -1;
    QString label;
    ImageSnapshot snap;
};

// 算子函数：输入像素与执行上下文(可修改画布尺寸)
using OpFn = std::function<void(std::vector<uint8_t> &, OpContext &)>;

// 参数实时预览的一个输入项：
// 提交路径与预览路径共用同一个算子工厂，保证两边结果完全一致
struct PreviewField {
    QLineEdit *edit = nullptr;
    QString title;
    double min = 0.0;
    double max = 0.0;
    OpFn (*makeOp)(double) = nullptr;   // 把输入值变成一次算子调用
};

// 一个打开的文档(一个标签页)：
// 只有"当前活动文档"的状态会放在 MainWindow 的成员里直接使用，
// 其余文档以这份结构保存，切换标签时把两边状态整体交换(移动语义，不拷贝像素)
struct Document {
    int id = 0;                               // 文档唯一标识(全局历史按它定位文档)
    QString path;                             // 文件路径(空表示未命名)
    BitmapImage value;                            // 文件头信息 + 原始像素
    std::vector<uint8_t> imageData;           // 当前像素
    std::vector<uint8_t> originalImageData;   // 打开时的原始像素(重置用)
    SelectionMask originalAlpha;              // 打开时图片自带的 alpha(重置用)
    int32_t originalWidth = 0;
    int32_t originalHeight = 0;
    ContentQuad quad;
    bool rotBaseValid = false;
    std::vector<uint8_t> rotBase;
    int32_t rotBaseW = 0;
    int32_t rotBaseH = 0;
    ContentQuad rotBaseQuad;
    double rotTotal = 0.0;
    bool canSave = false;
    SelectionMask mask;                       // 选区掩码(图像像素坐标，视觉，y 向下)
    std::vector<SelectionChannel> channels;   // 已保存的选区通道(随文档切换)
    std::vector<Layer> layers;                // 图层栈(自底向上)
    int activeLayer = 0;
};

class SelectionOverlay;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    //reset image
    void ResetImage(BitmapImage &myValue);
    void ShowImage(const std::vector<uint8_t> &inImageData,BitmapImage value,int32_t width,int32_t height);
    void ResetAll(BitmapImage &myValue);
    ReturnValue CheckOK(QLineEdit* lineEdit);
    void UndoImageProcessing();
    void RedoImageProcessing();
    //清理每段数据,防止数据冲突引发异常
    void ClearSegmentData();

    // ---- 给子窗口使用的接口 ----
    // 子窗口算子结果写回主窗口：更新数据 + 记录历史 + 刷新显示
    void ApplyResult(std::vector<uint8_t> &result,
                     const QString &label = QStringLiteral("应用处理结果"));
    // 当前正在显示的像素数据
    const std::vector<uint8_t> &CurrentImageData() const { return imageData; }
    // 是否正在处理中(处理中应禁止再次触发算子)
    bool IsBusy() const { return m_busy; }
    // 当前画面是否为尚未提交的预览结果
    bool IsPreviewActive() const { return m_previewActive; }
    // 更新状态栏文字
    void UpdateStatus(const QString &text);
    // 供外部(命令行)载入图片
    bool OpenImage(const QString &path) { return LoadImage(path); }
    // 按扩展名保存当前画面；PNG 在存在图层 alpha 时保留透明，其余格式按白底压平
    // pngOptions 非空且导出 PNG 时，应用压缩级别与 DPI
    bool SaveImageTo(const QString &filePath, QString &error,
                     const PngExportOptions *pngOptions = nullptr);
    // 当前打开的文档数 / 活动文档下标(供自检使用)
    int DocumentCount() const { return static_cast<int>(m_docs.size()); }
    int ActiveDocumentIndex() const { return m_activeDoc; }

    // ---- 选区(图像像素坐标，视觉坐标，y 向下；内部用掩码保存，支持羽化/反选/加选) ----
    // add=true 表示把新矩形并入已有选区(Shift 加选)
    void SetSelection(const QRect &imageRect, bool add = false);
    // 套索：把多边形内的像素并入/替换/减去选区(subtract 优先于 add)
    void SetSelectionPolygon(const QPolygon &points, bool add = false, bool subtract = false);
    // 直接替换整份掩码(尺寸不符或为空时视为取消选区)
    void SetSelectionMask(const SelectionMask &mask);
    const SelectionMask &CurrentSelectionMask() const { return m_mask; }
    QRect SelectionRect() const { return m_selection; }   // 掩码的包围盒
    bool HasSelection() const { return !m_mask.isEmpty(); }
    void SelectAll();
    void InvertSelection();                 // 反选：有选区时取补集
    void FeatherSelection(double radius);   // 羽化：按半径做两次盒式模糊
    void GrowSelection(int radius);         // 扩展选区(向外扩 radius 像素)
    void ShrinkSelection(int radius);       // 收缩选区(向内收 radius 像素)
    void SmoothSelection(int radius);       // 边界平滑(取半径内平均覆盖度后二值化)
    void TranslateSelection(int dx, int dy);
    // 磁性吸附：在 p 周围 radius 内找边缘强度最高的像素(edgeAt 返回梯度，0 视为非边缘)
    static QPoint SnapSelectionEdge(const std::function<int(int, int)> &edgeAt,
                                    const QPoint &p, int radius);
    // 把选区内像素整体搬到新位置(原位置按覆盖度刷白)，作为一步历史提交
    void MoveSelectionContent(int dx, int dy);
    // 把剪贴板图像粘贴到选区(等比缩放到包围盒内并居中)
    void PasteIntoSelection();
    // 同上，但图像由调用方提供(自检复用)
    void PasteImageIntoSelection(const QImage &image);
    void ClearSelection();
    void CopySelection();
    void CropToSelection();
    void ClearSelectionContent();
    // 用纯色填充选区(按掩码覆盖度混合)
    void FillSelection(const QColor &color);
    // 沿选区边界向内描边 width 像素
    void StrokeSelection(const QColor &color, int width);
    // 自由变换：把选区内容按中心缩放(百分比)与旋转(角度)后贴回，原位置刷白
    void FreeTransformSelection(double scalePercent, double angleDegrees);
    // 非等比缩放 + 旋转的自由变换(交互式手柄提交与自检复用)
    void ApplyFreeTransform(double scaleX, double scaleY, double angleDegrees,
                            const QString &label = QStringLiteral("自由变换"));

    // ---- 图层(自底向上，索引 0 为最底层) ----
    int LayerCount() const { return static_cast<int>(m_layers.size()); }
    int ActiveLayerIndex() const { return m_activeLayer; }
    bool SetActiveLayer(int index);
    void AddLayer();                // 在当前图层上方新建(纯白)
    void DuplicateLayer();          // 复制当前图层
    void DeleteActiveLayer();
    void MergeAllLayers();          // 合并所有图层为一张
    void MoveActiveLayerUp();
    void MoveActiveLayerDown();
    void SetLayerVisible(int index, bool visible);
    void SetLayerOpacity(int index, double opacity);   // 0.0 ~ 1.0
    void SetLayerBlend(int index, int blend);          // 0 正常 / 1 正片叠底 / 2 滤色
    // 当前各图层合成后的像素(供保存与自检使用)
    std::vector<uint8_t> CompositePixels();

    // ---- 图层蒙版(作用于当前活动图层；0 = 隐藏，255 = 显示) ----
    bool HasLayerMask(int index) const;         // 该图层是否带蒙版(无论是否停用)
    bool IsLayerMaskEnabled(int index) const;   // 蒙版当前是否生效
    void AddLayerMaskFromSelection();           // 用当前选区生成蒙版(选区内显示)
    void AddLayerMaskFill(bool white);          // 添加全白/全黑蒙版
    void ApplySelectionToLayerMask(bool reveal);// 用当前选区显示/隐藏蒙版内容
    void InvertLayerMask();                     // 蒙版反相
    void ToggleLayerMask();                     // 停用/启用蒙版(仅显示属性，不写历史)
    void RemoveLayerMask();                     // 删除蒙版

    // ---- 选区通道(把选区命名保存，之后可重新载入) ----
    int ChannelCount() const { return static_cast<int>(m_channels.size()); }
    QString ChannelName(int index) const;
    bool StoreSelectionChannel(const QString &name);   // 存储当前选区
    bool LoadSelectionChannel(int index);              // 载入为当前选区
    bool DeleteSelectionChannel(int index);

    // ---- 画笔/橡皮(在活动图层上绘制；遵守选区覆盖度) ----
    // 用当前画笔参数画一条直线笔画；供交互提交与自检复用。
    // 按下时记录快照，松开时作为一步历史提交。
    void PaintBrushStroke(const QPoint &from, const QPoint &to, bool erase = false);
    int BrushSize() const { return m_brushSize; }
    void SetBrushParams(int size, int hardness, double opacity, const QColor &color);

    // ---- 渐变工具(在活动图层上填充；遵守选区覆盖度) ----
    // 按当前参数在 from→to 的范围内填充渐变，作为一步历史提交。
    // radial=false 为线性(沿 from→to 方向)，radial=true 为径向(from 为圆心、到 to 为半径)。
    void ApplyGradientFill(const QPoint &from, const QPoint &to, bool radial = false);
    // 供自检与面板复用：设置类型、反向、不透明度与两端颜色
    void SetGradientParams(bool radial, bool reverse, double opacity,
                           const QColor &startColor, const QColor &endColor);

    // ---- 文字图层(栅格化提交，生成独立图层) ----
    // topLeft 为文字块左上角(图像像素坐标，视觉 y 向下)；alignment: 0 左 / 1 中 / 2 右
    void AddTextLayer(const QPoint &topLeft, const QString &text, const QFont &font,
                      const QColor &color, int alignment = 0);
    // 重新渲染指定文字图层(自检与交互编辑共用)；index 为图层栈下标
    bool UpdateTextLayer(int index, const QString &text, const QFont &font,
                         const QColor &color, int alignment);
    // 把指定文字图层移动到新的左上角(图像像素坐标，含历史记录)
    bool MoveTextLayer(int index, const QPoint &newTopLeft);

    // ---- 形状图层(矩形/椭圆/直线，栅格化为独立图层) ----
    // type: 0 矩形 / 1 椭圆 / 2 直线；strokeWidth 为 0 表示不描边
    void SetShapeParams(int type, bool filled, const QColor &fillColor,
                        const QColor &strokeColor, int strokeWidth, double opacity);
    // from/to 为图像像素坐标(视觉，y 向下)
    void AddShapeLayer(const QPoint &from, const QPoint &to);

public:
    std::string BMPPath;
    std::list<HistoryEntry> imageDataHistory;      // 全局撤销栈(操作前状态)
    std::list<HistoryEntry> redoImageDataHistory;  // 全局重做栈(操作后状态)
    //分段处理数据
    int num_threads = 1;
    size_t imageSize = 0;      // 像素数据字节数
    size_t pixelCount = 0;     // 总像素数
    std::vector<std::thread> threads;              // 存储线程对象，每个线程对象将处理图像数据的不同部分
    size_t segmentSize = 0; // 每线程负责的【像素】数
    std::vector<size_t> segmentStarts; // 用来存储每个数据段的起始位置
    //QImage
    QImage m_bmpImage;
    BitmapImage myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //store original image data
    std::vector<uint8_t> originalImageData;
    // 原图自带的 alpha(导入带透明图片时保留，重置时恢复)
    SelectionMask m_originalAlpha;
    // PNG 导出参数(记住上次选择)
    int m_pngCompression = 6;
    int m_pngDpi = 96;

    //create a object of object
    ImageOps function;

    double sigma;
    double angle;
private slots:

    //click menubar to open image
    void on_openImage_triggered();
    void on_actionsave_triggered();


    void on_btn_resetAll_clicked();


    void on_btn_brightness_clicked();

    void on_btn_contrast_clicked();

    void on_btn_saturation_clicked();

    void on_btn_rotate_ok_clicked();

    void on_btn_rotate_r_clicked();



    void on_btn_shadow_ok_clicked();

    void on_btn_highlight_ok_clicked();

    void on_btn_sharpen_clicked();

    void on_btn_threshold_ok_clicked();

    void on_btn_undo_clicked();

    // 点击历史面板某一项：自动连续撤销/重做到该步
    void on_historyList_itemClicked(QListWidgetItem *item);

    void on_actionmedian_triggered();

    void on_actionaver_triggered();

    void on_actiongauss_triggered();

    void on_actiongray_triggered();

    void on_actionreverse_triggered();

    void on_actioncomplementary_triggered();

    void on_actionautoContrast_triggered();

    void on_actioncolorBalance_triggered();

    void on_actionmosaic_triggered();

    void on_actionsobel_triggered();

    void on_actioneyeZoom_triggered();

    void on_actioncolorMap_triggered();

    void on_actionfishEye_triggered();

    void on_actionretouch_triggered();

    void on_actionundo_triggered();

    void on_actionreset_triggered();


    void on_actionscale_triggered();

    void on_actionredo_triggered();

    // ---- P2-A 新功能 ----
    void on_actionflipH_triggered();
    void on_actionflipV_triggered();
    void on_actionrotate90_triggered();
    void on_actionrotate90ccw_triggered();
    void on_actiontranspose_triggered();
    void on_actionantitranspose_triggered();
    void on_actiontailor_triggered();
    void on_actionequalize_triggered();
    void on_actionerode_triggered();
    void on_actiondilate_triggered();
    void on_actionopenMorph_triggered();
    void on_actioncloseMorph_triggered();
    void on_actionconvolution_triggered();
    void on_actionusm_triggered();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    // 输入框按 Esc 取消预览
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::MainWindow *ui;
    bool canSave = false;
    void closeEvent(QCloseEvent *event) override;
    void _SetVisible(bool set);
    void _SetShortCut();

    // ---- 算子调度与状态 ----
    bool m_busy = false;          // 是否有算子正在后台执行
    bool m_fitToWindow = true;    // 图像是否自适应窗口显示
    int  m_historyLimit = 40;     // 撤销历史最多保留的步数(控制内存)
    int32_t m_originalWidth = 0;  // 原始图像尺寸(重置时用)
    int32_t m_originalHeight = 0;
    ContentQuad m_contentQuad;    // 当前内容区域(旋转时用于确定新画布)

    // 旋转"无损基准"：任意角度旋转都从这份基准像素按累计角度一次性重采样，
    // 避免在上一次(已被插值糊过)的结果上再插值，导致越转越模糊
    bool m_rotBaseValid = false;
    std::vector<uint8_t> m_rotBase;   // 基准像素(紧凑 width*3)
    int32_t m_rotBaseW = 0;
    int32_t m_rotBaseH = 0;
    ContentQuad m_rotBaseQuad;        // 基准的内容区域
    double m_rotTotal = 0.0;          // 相对基准的累计角度(顺时针，0~360)

    // 在后台线程执行算子；算子可通过 ctx 修改宽高(旋转等会改变画布尺寸)
    // sourceOverride 非空时以该快照作为输入(旋转走"无损基准"，不再用上一次的结果)
    // onDone 在结果应用后回到 UI 线程调用(ok 表示算子确实产生了有效结果)
    void ApplyOperator(const QString &title,
                       std::function<void(std::vector<uint8_t> &, OpContext &)> op,
                       const ImageSnapshot *sourceOverride = nullptr,
                       std::function<void()> onDone = nullptr,
                       bool restrictSelection = true);
    // 旋转：始终从"无损基准"按【累计角度】重新采样一次，
    // 避免每次都在上一次的插值结果上再插值，导致一直旋转越来越模糊
    void ApplyRotation(double deltaDegrees, const QString &title);
    // 丢弃旋转基准(任何非旋转操作/撤销/重做/重置/重新打开都会调用)
    void InvalidateRotateBase();
    // 把算子按线程分段执行(以像素为单位，保证 BGR 通道不被拆散)
    void RunSegmented(const OpContext &ctx,
                      const std::function<void(size_t, size_t)> &fn);
    void SetBusy(bool busy);
    // 打开文件后的统一初始化
    bool LoadImage(const QString &path);
    // 当前活动文档的完整快照(与 MakeSnapshot 的存储约定一致)
    ImageSnapshot CurrentSnapshot() const;
    // 写入一条全局历史：before 为操作前状态，当前状态为操作后
    void PushHistory(const QString &label, const ImageSnapshot &before);
    // 应用一个快照：写历史 + 同步尺寸 + 刷新显示
    void ApplySnapshot(const ImageSnapshot &snap, bool keepRotateBase = false,
                       const QString &label = QStringLiteral("编辑"),
                       const ImageSnapshot *beforeOverride = nullptr);
    // 把快照内容直接写进当前文档并刷新显示(不写历史，撤销/重做共用)
    void ApplyStateSnapshot(const ImageSnapshot &snap, bool keepRotateBase);
    // 按文档 id 切换到目标文档；找不到返回 false
    bool ActivateDocumentById(int docId);
    // 历史面板/状态提示用的文档名
    QString DocumentDisplayName(int docId) const;
    // 文档关闭后清理它名下的撤销/重做记录
    void PurgeHistoryForDocument(int docId);
    // 只刷新画面(不修改已提交的像素数据)，供预览/取消预览使用
    void SetDisplayPixmap(const std::vector<uint8_t> &data, int32_t width, int32_t height);
    void ApplyDisplayPixmap(QPixmap pixmap);
    void RefreshDisplay();
    // 按当前图层状态刷新画面：有 alpha 时铺棋盘格，否则沿用白底合成
    void SetCurrentDisplay();

    // ---- 参数实时预览 ----
    // 右侧"图像调整"的 7 个输入框：停止输入约 280ms 后在【已提交的图像】上
    // 后台执行一次算子并只刷新画面，不写历史；点"应用"才真正提交
    void SetupLivePreview();
    void OnPreviewTextChanged(int fieldIndex);
    void RunPreview();
    void CancelPreview();
    // 预览结果与当前输入完全一致时直接提交(省掉一次重复计算)，否则返回 false
    bool CommitPreview(const QString &title, QLineEdit *edit, double value);
    int PreviewFieldIndex(const QLineEdit *edit) const;
    // 通用算子预览：旋转、弹窗参数用它试算(与面板共用防抖/世代号，不写历史)
    void PreviewCustomOp(const QString &title,
                         std::function<void(std::vector<uint8_t> &, OpContext &)> op);
    // 旋转预览：在(缩小后的)当前图像上直接试转，提交时仍走无损基准
    void PreviewRotation(double deltaDegrees);

    QTimer *m_previewTimer = nullptr;
    bool m_customPreview = false;   // 当前预览来自旋转/对话框，而不是右侧 7 个输入框
    std::function<void(std::vector<uint8_t> &, OpContext &)> m_pendingCustomOp;
    QString m_pendingCustomTitle;
    std::vector<PreviewField> m_previewFields;
    int m_pendingFieldIndex = -1;   // 正在等待/执行的输入项
    int m_previewGeneration = 0;    // 世代号：过期结果直接丢弃
    bool m_previewRunning = false;  // 有预览任务在后台执行
    bool m_previewQueued = false;   // 执行期间又有新输入，完成后重算
    bool m_previewActive = false;   // 当前画面是预览结果(尚未提交)
    ImageSnapshot m_previewResult;  // 最近一次有效的预览结果
    QImage m_previewImage;          // 大图低分辨率预览图(仅用于显示，不参与提交)
    bool m_previewDownsampled = false;  // 本次预览是否在缩小图上计算
    double m_previewValue = 0.0;    // 该结果对应的输入值
    int m_previewFieldIndex = -1;   // 该结果来自哪个输入项
    // 刷新状态栏右侧信息
    void UpdateStatusInfo();
    // 刷新右侧"历史记录"面板：列出每一步的尺寸，并选中当前步
    void UpdateHistoryPanel();

    // ---- 多文档标签页 ----
    // 标签页状态以"移动交换"实现：当前文档用成员变量，其余文档存 m_docs
    void SetupDocumentTabs();
    void ActivateDocument(int index);
    void CloseDocument(int index);
    void UpdateTabBar();
    // 清空当前活动文档的全部成员状态(关闭最后一个文档时用)
    void ResetDocumentState();
    QTabBar *m_tabBar = nullptr;
    std::vector<Document> m_docs;
    int m_activeDoc = -1;
    int m_nextDocId = 1;          // 下一个文档 id(只增不减)

    // ---- 选区 ----
    void SetupSelectionTool();
    // 图像尺寸变化后把选区夹到图像范围内，并同步覆盖层
    void SyncSelectionToImage();
    void UpdateSelectionOverlay();
    // 掩码的包围盒(视觉坐标)；空掩码返回空矩形
    QRect MaskBoundingRect() const;
    // 生成给覆盖层显示的半透明掩码缩略图(长边不超过 512)
    QImage BuildMaskPreview() const;
    // 按整数步长抽样得到缩略图尺寸的掩码(供低分辨率预览使用)
    static SelectionMask DownsampleMask(const SelectionMask &mask, int step);
    // 选区形态学处理：mode 0 扩展 / 1 收缩 / 2 平滑(复用盒式模糊后二值化)
    void RefineSelection(int radius, int mode);
    SelectionOverlay *m_overlay = nullptr;
    // 交互式变换(拖拽手柄)：进入、提交/取消，以及撤销/裁剪时静默退出
    void BeginInteractiveTransform();
    void FinishInteractiveTransform(bool commit, double scaleX, double scaleY, double angleDegrees);
    void CancelInteractiveTransform();
    bool AnySelectionToolChecked() const;

    SelectionMask m_mask;                  // 选区掩码(唯一真值)
    QRect m_selection;                     // 掩码包围盒(供复制/裁剪/绘制使用)
    QAction *m_selectionToolAction = nullptr;
    QAction *m_lassoToolAction = nullptr;
    QAction *m_polyToolAction = nullptr;
    QAction *m_magnetToolAction = nullptr;
    QMenu *m_selectionMenu = nullptr;

    // ---- 图层 ----
    void SetupLayerPanel();      // 右侧图层面板(替换"开发中"占位)
    void UpdateLayerPanel();     // 刷新列表/不透明度/混合模式(不触发信号)
    void RepaintComposite();     // 重新合成并刷新画面(不改历史)
    // 重新合成所有图层；返回可直接显示的像素(单图层且无蒙版时就是 imageData 本身)
    const std::vector<uint8_t> &BuildComposite();
    // 带 alpha 的合成结果(用于 PNG 导出)：颜色与透明度按图层叠加，透明区域真正透明
    QImage BuildCompositeRgba() const;
    // 是否有生效中的图层蒙版(有蒙版时不能走单图层快速路径/实时预览)
    bool AnyLayerMaskEnabled() const;
    // 是否有生效中的图层 alpha(有透明像素时同样不能走单图层快速路径)
    bool AnyLayerAlphaActive() const;
    // 是否需要合成(多图层 / 蒙版 / alpha 任一成立)
    bool NeedsComposite() const;
    // 右侧通道面板：列表 + 存储/载入/删除
    void SetupChannelPanel();
    void UpdateChannelPanel();
    // 弹窗输入名称后存储当前选区(面板按钮与菜单共用)
    void PromptStoreChannel();
    // 画布尺寸变化后丢弃尺寸不再匹配的通道，避免载入时错位
    void PruneChannelsForCanvas();

    // ---- 画笔/橡皮 ----
    void SetupBrushPanel();                 // 右侧画笔参数面板
    void UpdateBrushParams();               // 把当前参数同步给画布覆盖层
    void BeginBrushStroke(const QPoint &point);
    void BrushSegmentTo(const QPoint &from, const QPoint &to);
    void EndBrushStroke();
    // 把一条"胶囊"笔迹按硬度写进笔画掩码(取最大值，避免重复加深)
    void StampBrushCapsule(const QPoint &from, const QPoint &to);
    bool ApplyPendingBrush();
    QAction *m_brushToolAction = nullptr;
    QAction *m_eraserToolAction = nullptr;
    QGroupBox *m_brushGroup = nullptr;
    int m_brushSize = 20;                  // 直径(图像像素)
    int m_brushHardness = 70;              // 0~100
    double m_brushOpacity = 0.8;           // 0.0~1.0
    QColor m_brushColor = QColor(20, 20, 20);
    bool m_brushStrokeActive = false;
    bool m_brushErase = false;
    std::vector<uint8_t> m_brushMask;      // 视觉坐标，0~255
    int32_t m_brushMaskW = 0;
    int32_t m_brushMaskH = 0;
    ImageSnapshot m_brushBefore;           // 笔画开始时的快照(撤销用)

    // ---- 渐变工具 ----
    void SetupGradientPanel();              // 右侧渐变参数面板
    void UpdateGradientControls();          // 刷新面板上的颜色/类型显示
    void SetupShapePanel();                 // 右侧形状参数面板
    void UpdateShapeControls();             // 刷新形状颜色/类型显示
    void EditTextLayer(int index);          // 双击文字图层：预填参数并重渲染
    QAction *m_gradientToolAction = nullptr;
    QGroupBox *m_gradientGroup = nullptr;
    QLabel *m_gradientStartSwatch = nullptr;
    QLabel *m_gradientEndSwatch = nullptr;
    bool m_gradientRadial = false;          // false 线性 / true 径向
    bool m_gradientReverse = false;
    double m_gradientOpacity = 1.0;
    QColor m_gradientStartColor = QColor(0, 0, 0);
    QColor m_gradientEndColor = QColor(255, 255, 255);

    // ---- 文字工具 ----
    void PlaceTextLayer(const QPoint &pos);   // 点击画布后弹对话框并创建文字图层
    int TextLayerIndexAt(const QPoint &pos) const;             // 命中顶层文字图层，未命中 -1
    bool RenderTextLayer(int index, const QString &text, const QFont &font,
                         const QColor &color, int alignment, const QPoint &anchor);
    void BeginTextDrag(int index);                             // 按下已有文字：记录拖动前状态
    void PreviewTextMove(int index, const QPoint &delta);      // 拖动中实时重绘
    void EndTextDrag(int index, bool moved);                   // 松开：提交历史或打开编辑
    QAction *m_textToolAction = nullptr;
    QFont m_textFont = QFont(QStringLiteral("Microsoft YaHei"), 48);
    QColor m_textColor = QColor(0, 0, 0);
    int m_textAlign = 0;
    ImageSnapshot m_textMoveBefore;    // 文字拖动前的快照
    int m_textMoveLayer = -1;
    QPoint m_textMoveAnchor;           // 拖动开始时文字的原始锚点

    // ---- 形状工具 ----
    QAction *m_shapeToolAction = nullptr;
    QGroupBox *m_shapeGroup = nullptr;
    QLabel *m_shapeFillSwatch = nullptr;
    QLabel *m_shapeStrokeSwatch = nullptr;
    int m_shapeType = 0;
    bool m_shapeFilled = true;
    QColor m_shapeFillColor = QColor(34, 211, 238);
    QColor m_shapeStrokeColor = QColor(20, 20, 20);
    int m_shapeStrokeWidth = 2;
    double m_shapeOpacity = 1.0;

    // 保存/导出用像素：多图层时为合成结果
    const std::vector<uint8_t> &DisplayPixels();
    // 用"当前图层栈 + 新像素"构造快照(活动图层像素单独存 data)
    ImageSnapshot MakeSnapshot(std::vector<uint8_t> pixels, int32_t width, int32_t height,
                               const ContentQuad &quad) const;
    // 从快照恢复图层栈与像素(撤销/重做/应用快照共用)
    void RestoreSnapshot(const ImageSnapshot &snap);
    // 图层结构变化(增删/合并/排序)写一步历史
    void CommitLayerStructure(const QString &title, const ImageSnapshot &before);
    // 需要保持"单图层"的几何操作守卫(多图层时提示先合并)
    bool RequireSingleLayer(const QString &opName);

    std::vector<Layer> m_layers;             // 图层栈(自底向上)
    int m_activeLayer = 0;
    std::vector<uint8_t> m_composite;        // 多图层时的合成缓存
    QListWidget *m_layerList = nullptr;
    QSlider *m_layerOpacitySlider = nullptr;
    QLabel *m_layerOpacityLabel = nullptr;
    QComboBox *m_layerBlendCombo = nullptr;
    QMenu *m_layerMenu = nullptr;
    // 蒙版子菜单及其状态相关项(菜单弹出时按当前图层/选区刷新可用性)
    QMenu *m_layerMaskMenu = nullptr;
    QAction *m_maskFromSelectionAction = nullptr;
    QAction *m_maskRevealSelectionAction = nullptr;
    QAction *m_maskHideSelectionAction = nullptr;
    QAction *m_maskToggleAction = nullptr;
    QAction *m_maskInvertAction = nullptr;
    QAction *m_maskRemoveAction = nullptr;
    // 选区通道
    std::vector<SelectionChannel> m_channels;
    QMenu *m_channelMenu = nullptr;
    QGroupBox *m_channelGroup = nullptr;
    QListWidget *m_channelList = nullptr;
};
#endif // MAINWINDOW_H
