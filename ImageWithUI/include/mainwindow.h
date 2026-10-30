#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include <QListWidgetItem>
#include <QTimer>
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

// 历史快照：必须连同尺寸一起保存，
// 因为旋转等算子会改变画布宽高，只存像素会导致撤销/保存时尺寸对不上
struct ImageSnapshot {
    std::vector<uint8_t> data;
    int32_t width = 0;
    int32_t height = 0;
    ContentQuad quad;         // 内容区域(撤销/重做要一起恢复)
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
                       std::function<void()> onDone = nullptr);
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
};
#endif // MAINWINDOW_H
