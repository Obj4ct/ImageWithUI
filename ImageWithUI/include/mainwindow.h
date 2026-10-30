#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include <QListWidgetItem>
#include <QListWidget>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QTabBar>
#include "BMPFile.h"
#include "Function.h"
#include "blendwindow.h"
#include "eye.h"
#include "interpolation.h"
#include "face.h"
#include "colormap.h"

struct ReturnValue{
    bool isNull = true;
    bool isNumeric = false;
    double_t value = 0.0;
};
enum OperationType
{
    UNDO,
    REDO
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

// 图层：与画布同尺寸的 BGR 像素 + 叠放属性
// 约定：所有图层尺寸都与文档画布一致；imageData 始终是【当前活动图层】的像素
struct Layer {
    QString name;
    std::vector<uint8_t> data;
    bool visible = true;
    double opacity = 1.0;   // 0.0 ~ 1.0
    int blend = 0;          // 0 正常 / 1 正片叠底 / 2 滤色
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
    QString path;                             // 文件路径(空表示未命名)
    MyValue value;                            // 文件头信息 + 原始像素
    std::vector<uint8_t> imageData;           // 当前像素
    std::vector<uint8_t> originalImageData;   // 打开时的原始像素(重置用)
    std::list<ImageSnapshot> history;         // 撤销栈
    std::list<ImageSnapshot> redoHistory;     // 重做栈
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
    void ResetImage(MyValue &myValue);
    void ShowImage(const std::vector<uint8_t> &inImageData,MyValue value,int32_t width,int32_t height);
    void ResetAll(MyValue &myValue);
    ReturnValue CheckOK(QLineEdit* lineEdit);
    OperationType SaveImageDataToHistory(const ImageSnapshot &snap);
    void UndoImageProcessing();
    void RedoImageProcessing();
    //清理每段数据,防止数据冲突引发异常
    void ClearSegmentData();

    // ---- 给子窗口使用的接口 ----
    // 子窗口算子结果写回主窗口：更新数据 + 记录历史 + 刷新显示
    void ApplyResult(std::vector<uint8_t> &result);
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
    // 当前打开的文档数 / 活动文档下标(供自检使用)
    int DocumentCount() const { return static_cast<int>(m_docs.size()); }
    int ActiveDocumentIndex() const { return m_activeDoc; }

    // ---- 选区(图像像素坐标，视觉坐标，y 向下；内部用掩码保存，支持羽化/反选/加选) ----
    // add=true 表示把新矩形并入已有选区(Shift 加选)
    void SetSelection(const QRect &imageRect, bool add = false);
    // 直接替换整份掩码(尺寸不符或为空时视为取消选区)
    void SetSelectionMask(const SelectionMask &mask);
    const SelectionMask &CurrentSelectionMask() const { return m_mask; }
    QRect SelectionRect() const { return m_selection; }   // 掩码的包围盒
    bool HasSelection() const { return !m_mask.isEmpty(); }
    void SelectAll();
    void InvertSelection();                 // 反选：有选区时取补集
    void FeatherSelection(double radius);   // 羽化：按半径做两次盒式模糊
    void TranslateSelection(int dx, int dy);
    void ClearSelection();
    void CopySelection();
    void CropToSelection();
    void ClearSelectionContent();

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

public:
    std::string BMPPath;
    std::list<ImageSnapshot> imageDataHistory;
    std::list<ImageSnapshot> redoImageDataHistory;
    //分段处理数据
    int num_threads = 1;
    size_t imageSize = 0;      // 像素数据字节数
    size_t pixelCount = 0;     // 总像素数
    std::vector<std::thread> threads;              // 存储线程对象，每个线程对象将处理图像数据的不同部分
    size_t segmentSize = 0; // 每线程负责的【像素】数
    std::vector<size_t> segmentStarts; // 用来存储每个数据段的起始位置
    //QImage
    QImage m_bmpImage;
    MyValue myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //store original image data
    std::vector<uint8_t> originalImageData;

    //create a object of object
    Function function;

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

    void on_actionblend_triggered();

    void on_actionmosaic_triggered();

    void on_actionsobel_triggered();

    void on_actionliquefaction_triggered();

    void on_actioncolorMap_triggered();

    void on_actionfishEye_triggered();

    void on_actionface_triggered();

    void on_actionundo_triggered();

    void on_actionreset_triggered();


    void on_actionscale_triggered();

    void on_actionredo_triggered();

    void on_actionlarge_near_triggered();

    void on_actionsmall_near_triggered();

    void on_actionlarge_bilinear_triggered();

    void on_actionsmall_bilinear_triggered();

    void on_actionlarge_bicubic_triggered();

    void on_actionsmall_bicubic_triggered();
    void on_actionhighContrast_triggered();

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
    BlendWindow * blendWindow = nullptr;
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
    // 应用一个快照：写历史 + 同步尺寸 + 刷新显示
    void ApplySnapshot(const ImageSnapshot &snap, bool keepRotateBase = false);
    // 只刷新画面(不修改已提交的像素数据)，供预览/取消预览使用
    void SetDisplayPixmap(const std::vector<uint8_t> &data, int32_t width, int32_t height);
    void ApplyDisplayPixmap(QPixmap pixmap);
    void RefreshDisplay();

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

    QTimer *m_previewTimer = nullptr;
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
    SelectionOverlay *m_overlay = nullptr;
    SelectionMask m_mask;                  // 选区掩码(唯一真值)
    QRect m_selection;                     // 掩码包围盒(供复制/裁剪/绘制使用)
    QAction *m_selectionToolAction = nullptr;
    QMenu *m_selectionMenu = nullptr;

    // ---- 图层 ----
    void SetupLayerPanel();      // 右侧图层面板(替换"开发中"占位)
    void UpdateLayerPanel();     // 刷新列表/不透明度/混合模式(不触发信号)
    void RepaintComposite();     // 重新合成并刷新画面(不改历史)
    // 重新合成所有图层；返回可直接显示的像素(单图层时就是 imageData 本身)
    const std::vector<uint8_t> &BuildComposite();
    // 保存/导出用像素：多图层时为合成结果
    const std::vector<uint8_t> &DisplayPixels();
    // 用"当前图层栈 + 新像素"构造快照(活动图层像素单独存 data)
    ImageSnapshot MakeSnapshot(std::vector<uint8_t> pixels, int32_t width, int32_t height,
                               const ContentQuad &quad) const;
    // 从快照恢复图层栈与像素(撤销/重做/应用快照共用)
    void RestoreSnapshot(const ImageSnapshot &snap);
    // 图层结构变化(增删/合并/排序)写一步历史
    void CommitLayerStructure(const QString &title);
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
};
#endif // MAINWINDOW_H
