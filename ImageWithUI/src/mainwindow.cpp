
// MainWindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Theme.h"
#include "editdialogs.h"
#include <QAction>
#include <QMenu>
#include <QImageWriter>
#include <QSignalBlocker>
#include <QFont>
#include <QKeyEvent>
#include <QApplication>

namespace {

// ---- 调整算子工厂：提交路径与实时预览路径共用同一份实现 ----

OpFn MakeBrightnessOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        f.Brightness(d, value);
    };
}

OpFn MakeContrastOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        f.Contrast(d, value);
    };
}

OpFn MakeSaturationOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.Saturation(d, ctx.width, ctx.height, value);
    };
}

OpFn MakeShadowOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.MakeShadow(d, out, value);
        d.swap(out);
    };
}

OpFn MakeHighlightOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.HighLight(d, out, value);
        d.swap(out);
    };
}

OpFn MakeThresholdOp(double threshold)
{
    return [threshold](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.ApplyThreshold(d, threshold, 0, ctx.pixelCount);
    };
}

// 锐化：高斯模糊 → 高反差保留 → 锐化，程度值同时作为模糊半径
OpFn MakeSharpenOp(double amount)
{
    return [amount](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> blurPromise;
        std::future<std::vector<uint8_t>> blurFuture = blurPromise.get_future();
        f.Gauss(d, blurPromise, ctx.width, ctx.height, amount);
        const std::vector<uint8_t> blurred = blurFuture.get();

        std::promise<std::vector<uint8_t>> highPromise;
        std::future<std::vector<uint8_t>> highFuture = highPromise.get_future();
        f.HighContrast(d, highPromise, blurred);
        const std::vector<uint8_t> highContrast = highFuture.get();

        std::promise<std::vector<uint8_t>> sharpPromise;
        std::future<std::vector<uint8_t>> sharpFuture = sharpPromise.get_future();
        f.Sharpen(d, sharpPromise, highContrast);
        d = sharpFuture.get();
    };
}

// 大图预览用：按整数步长做最近邻缩小(保持 BGR、逐行紧凑布局)
std::vector<uint8_t> DownsampleBgr(const std::vector<uint8_t> &src, int32_t w, int32_t h, int step,
                                   int32_t &outW, int32_t &outH)
{
    outW = w / step;
    outH = h / step;
    if (outW <= 0 || outH <= 0 || src.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3) {
        outW = 0;
        outH = 0;
        return {};
    }
    const size_t srcRow = static_cast<size_t>(w) * 3;
    const size_t dstRow = static_cast<size_t>(outW) * 3;
    std::vector<uint8_t> out(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 3, 0);
    for (int32_t y = 0; y < outH; ++y) {
        const uint8_t *s = src.data() + static_cast<size_t>(y) * static_cast<size_t>(step) * srcRow;
        uint8_t *d = out.data() + static_cast<size_t>(y) * dstRow;
        for (int32_t x = 0; x < outW; ++x) {
            const uint8_t *p = s + static_cast<size_t>(x) * static_cast<size_t>(step) * 3;
            d[0] = p[0];
            d[1] = p[1];
            d[2] = p[2];
            d += 3;
        }
    }
    return out;
}

// 预览超过这个像素数就先缩小再算，避免大图拖动参数卡顿
constexpr size_t kPreviewMaxPixels = 2000000;

}   // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocus();
    setWindowTitle("ImageWithUI · 数字图像处理工作台");
    setWindowIcon(QIcon(":/icon/logo.png"));

    //根据设备cpu线程数判断程序需要设置的线程
    const unsigned int cores = std::thread::hardware_concurrency();
    if (cores >= 2) {
        num_threads = static_cast<int>(cores / 3);   // 留出余量给界面线程
        if (num_threads < 1) num_threads = 1;
    } else {
        num_threads = 1;
    }
    qDebug() << "thread is:" << num_threads;

    _SetShortCut();
    _SetVisible(false);
    SetupLivePreview();   // 右侧调整参数的实时预览

    // 工具栏：常用操作快捷入口(原先工具栏为空)
    ui->toolBar->setMovable(false);
    ui->toolBar->setIconSize(QSize(20, 20));
    ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->toolBar->addAction(ui->openImage);
    ui->toolBar->addAction(ui->actionsave);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionundo);
    ui->toolBar->addAction(ui->actionredo);
    ui->toolBar->addAction(ui->actionreset);

    // 视图菜单：主题切换 / 适应窗口
    QMenu *viewMenu = menuBar()->addMenu(QString("视图"));
    QAction *themeAction = viewMenu->addAction(QString("切换深色/浅色主题"));
    themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(themeAction, &QAction::triggered, this, []() {
        Theme::Toggle();
    });
    QAction *fitAction = viewMenu->addAction(QString("图像适应窗口"));
    fitAction->setCheckable(true);
    fitAction->setChecked(m_fitToWindow);
    connect(fitAction, &QAction::toggled, this, [this](bool on) {
        m_fitToWindow = on;
        ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    });

    // 状态栏：左侧提示 + 右侧信息
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setText(QString("双击此处，或按 Ctrl+N 打开图片"));
    statusBar()->showMessage("就绪 · 请先打开一张图片");
    UpdateStatusInfo();
}


void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QMainWindow::mouseDoubleClickEvent(event);
        return;
    }
    // 事件坐标是窗口坐标，需要转换到 imageLabel 的局部坐标系再判断命中
    const QPoint pos = ui->imageLabel->mapFrom(this, event->pos());
    if (!ui->imageLabel->rect().contains(pos)) {
        return;
    }

    // 文件默认路径
    const QString defaultPath = QDir::homePath();
    // 常见图片格式（BMP/PNG/JPG/GIF/TIFF/WebP/TGA/ICO/SVG）
    const QString filter = QString("图片文件 (*.bmp *.png *.jpg *.jpeg *.gif *.tif *.tiff *.webp *.tga *.ico *.svg);;"
                                   "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;GIF (*.gif);;"
                                   "TIFF (*.tif *.tiff);;WebP (*.webp);;TGA (*.tga);;图标 (*.ico *.icns);;SVG (*.svg)");
    const QString path = QFileDialog::getOpenFileName(this, "选择图片文件", defaultPath, filter);
    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    LoadImage(path);
}

// 输入框里按 Esc：放弃当前参数预览
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        const QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape && PreviewFieldIndex(qobject_cast<QLineEdit *>(watched)) >= 0) {
            CancelPreview();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::ResetImage(MyValue &myValue)
{
    imageData = myValue.imageData;
    // 恢复原始图像
    const QImage originalImage = Function::ToDisplayImage(myValue.imageData,
                                                          myValue.bmpInfo.GetWidth(),
                                                          myValue.bmpInfo.GetHeight());
    ui->imageLabel->setText(QString());
    ui->imageLabel->setPixmap(QPixmap::fromImage(originalImage));
}

void MainWindow::ShowImage(const std::vector<uint8_t> &inImageData, MyValue value, int32_t width, int32_t height)
{
    imageData = inImageData;
    myValue = value;

    SetDisplayPixmap(inImageData, width, height);
    UpdateStatusInfo();
}

// 只更新画面显示，不改动 imageData/myValue —— 预览与取消预览都走这里
void MainWindow::SetDisplayPixmap(const std::vector<uint8_t> &data, int32_t width, int32_t height)
{
    const QImage image = Function::ToDisplayImage(data, width, height);
    if (image.isNull()) {
        return;
    }
    ApplyDisplayPixmap(QPixmap::fromImage(image));
}

// 统一的"贴到 imageLabel"入口：适应窗口时按窗口尺寸缩放
void MainWindow::ApplyDisplayPixmap(QPixmap pixmap)
{
    if (m_fitToWindow) {
        const QSize target = ui->imageLabel->size();
        if (target.isValid() && !target.isEmpty()) {
            pixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    ui->imageLabel->setText(QString());
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(pixmap);
}

// 画面回到"已提交"的图像状态
void MainWindow::RefreshDisplay()
{
    if (imageData.empty()) {
        return;
    }
    SetDisplayPixmap(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}

// ------------------------------------------------------------------
// 参数实时预览
// ------------------------------------------------------------------

int MainWindow::PreviewFieldIndex(const QLineEdit *edit) const
{
    for (size_t i = 0; i < m_previewFields.size(); ++i) {
        if (m_previewFields[i].edit == edit) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MainWindow::SetupLivePreview()
{
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(280);   // 停止输入 280ms 后才开始计算
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::RunPreview);

    struct Desk {
        QLineEdit *edit;
        const char *title;
        double min;
        double max;
        OpFn (*makeOp)(double);
    };
    const Desk desks[] = {
        { ui->lineEdit_sharpen,    "锐化",      0.0,   200.0, &MakeSharpenOp },
        { ui->lineEdit_highlight,  "高光调整",  0.0,   255.0, &MakeHighlightOp },
        { ui->lineEdit_threshold,  "阈值处理",  0.0,   255.0, &MakeThresholdOp },
        { ui->lineEdit_shadow,     "阴影调整",  0.0,   100.0, &MakeShadowOp },
        { ui->lineEdit_brightness, "亮度调整", -150.0, 150.0, &MakeBrightnessOp },
        { ui->lineEdit_contrast,   "对比度调整", -50.0, 100.0, &MakeContrastOp },
        { ui->lineEdit_saturation, "饱和度调整", -100.0, 100.0, &MakeSaturationOp },
    };

    for (const Desk &d : desks) {
        PreviewField field;
        field.edit = d.edit;
        field.title = QString::fromUtf8(d.title);
        field.min = d.min;
        field.max = d.max;
        field.makeOp = d.makeOp;
        m_previewFields.push_back(field);

        const int index = static_cast<int>(m_previewFields.size()) - 1;
        connect(d.edit, &QLineEdit::textChanged, this, [this, index]() {
            OnPreviewTextChanged(index);
        });
        d.edit->installEventFilter(this);   // Esc 取消预览
    }

    // 焦点离开右侧调整面板(或窗口失去焦点)时回滚到已提交的图像
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!m_previewActive) {
            return;
        }
        if (now != nullptr && (now == ui->widget_14 || ui->widget_14->isAncestorOf(now))) {
            return;
        }
        CancelPreview();
    });
}

void MainWindow::OnPreviewTextChanged(int fieldIndex)
{
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(m_previewFields.size())) {
        return;
    }
    if (imageData.empty() || m_busy) {
        return;
    }
    m_pendingFieldIndex = fieldIndex;
    m_previewTimer->start();   // 防抖：连续输入时不断重新计时
}

// 在【已提交的图像】上跑一次算子，只刷新画面，不写历史
void MainWindow::RunPreview()
{
    if (m_previewRunning) {
        m_previewQueued = true;   // 上一次还没算完，等它结束后再算最新输入
        return;
    }
    if (m_pendingFieldIndex < 0 || m_pendingFieldIndex >= static_cast<int>(m_previewFields.size())) {
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        CancelPreview();
        return;
    }

    const PreviewField &field = m_previewFields[static_cast<size_t>(m_pendingFieldIndex)];
    const ReturnValue rv = CheckOK(field.edit);
    // 输入不完整/非法/越界：静默回到已提交的画面(提交时会给出提示)
    if (rv.isNull || !rv.isNumeric || rv.value < field.min || rv.value > field.max) {
        CancelPreview();
        return;
    }

    const double value = rv.value;
    const OpFn op = field.makeOp(value);
    const int index = m_pendingFieldIndex;

    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();

    // 大图：按整数步长最近邻缩小后在缩略图上算预览，拖动参数不会卡顿；
    // 这种结果只用于显示，点"应用"时会用全分辨率重新计算
    int step = 1;
    while (static_cast<size_t>(width / step) * static_cast<size_t>(height / step) > kPreviewMaxPixels) {
        ++step;
    }
    const bool downsampled = step > 1;
    int32_t workW = width;
    int32_t workH = height;
    std::vector<uint8_t> source = imageData;
    if (downsampled) {
        source = DownsampleBgr(imageData, width, height, step, workW, workH);
        if (source.empty()) {
            CancelPreview();
            return;
        }
    }

    const ContentQuad quad = m_contentQuad;
    const int gen = ++m_previewGeneration;
    m_previewRunning = true;

    auto result = std::make_shared<ImageSnapshot>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
            [this, watcher, result, gen, value, index, downsampled, workW, workH, width, height]() {
        watcher->deleteLater();
        m_previewRunning = false;
        // 世代号已过期 → 这条结果对应的是旧输入，直接丢弃
        const bool usable = gen == m_previewGeneration && !result->data.empty() &&
                            (!downsampled || (result->width == workW && result->height == workH));
        if (usable) {
            m_previewValue = value;
            m_previewFieldIndex = index;
            m_previewActive = true;
            m_previewDownsampled = downsampled;
            if (downsampled) {
                // 低分辨率结果只用来显示，不能提交
                m_previewResult = ImageSnapshot();
                m_previewImage = Function::ToDisplayImage(result->data, result->width, result->height);
                QPixmap pixmap = QPixmap::fromImage(m_previewImage);
                if (!m_fitToWindow) {
                    pixmap = pixmap.scaled(QSize(width, height),
                                           Qt::IgnoreAspectRatio, Qt::FastTransformation);
                }
                ApplyDisplayPixmap(pixmap);
            } else {
                m_previewImage = QImage();
                m_previewResult = std::move(*result);
                SetDisplayPixmap(m_previewResult.data, m_previewResult.width, m_previewResult.height);
            }
            UpdateStatus(QString("预览中：%1 = %2%3")
                             .arg(m_previewFields[static_cast<size_t>(index)].title)
                             .arg(value)
                             .arg(downsampled ? "（低分辨率快速预览）" : ""));
        }
        if (m_previewQueued) {
            m_previewQueued = false;
            RunPreview();
        }
    });

    watcher->setFuture(QtConcurrent::run([source, op, workW, workH, quad, result]() mutable {
        std::vector<uint8_t> work = std::move(source);
        OpContext ctx;
        ctx.width = workW;
        ctx.height = workH;
        ctx.pixelCount = work.size() / 3;
        ctx.numThreads = 1;
        ctx.segmentSize = ctx.pixelCount;
        ctx.quad = quad;
        op(work, ctx);
        result->data = std::move(work);
        result->width = ctx.width;
        result->height = ctx.height;
        result->quad = ctx.quad;
    }));
}

// 放弃预览：停止等待中的任务，并把画面恢复成已提交的图像
void MainWindow::CancelPreview()
{
    ++m_previewGeneration;   // 让在途结果失效
    m_previewQueued = false;
    m_pendingFieldIndex = -1;
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }
    if (!m_previewActive) {
        return;
    }
    m_previewActive = false;
    m_previewFieldIndex = -1;
    m_previewValue = 0.0;
    m_previewResult = ImageSnapshot();
    m_previewImage = QImage();
    m_previewDownsampled = false;
    RefreshDisplay();
}

// 预览结果与当前输入完全一致时直接提交，省掉一次重复计算
bool MainWindow::CommitPreview(const QString &title, QLineEdit *edit, double value)
{
    const int index = PreviewFieldIndex(edit);
    const bool match = m_previewActive && m_previewFieldIndex == index && m_previewValue == value;
    // 低分辨率预览不能提交(结果会比全分辨率差)，交给调用方重新按全分辨率计算
    const bool canCommit = match && !m_previewDownsampled && !m_previewResult.data.empty();
    const bool wasPreviewing = m_previewActive;
    // 无论是否命中，先清掉预览状态：不一致时由调用方走正常算子流程
    const ImageSnapshot snap = canCommit ? m_previewResult : ImageSnapshot();
    ++m_previewGeneration;
    m_previewQueued = false;
    m_pendingFieldIndex = -1;
    m_previewTimer->stop();
    m_previewActive = false;
    m_previewFieldIndex = -1;
    m_previewValue = 0.0;
    m_previewResult = ImageSnapshot();
    m_previewImage = QImage();
    m_previewDownsampled = false;

    if (!canCommit) {
        if (wasPreviewing) {
            RefreshDisplay();   // 有未提交的预览，先回滚
        }
        return false;
    }
    ApplySnapshot(snap, false);
    UpdateStatus(title + " 已应用");
    return true;
}

void MainWindow::ResetAll(MyValue &myValue)
{
    CancelPreview();
    imageData = myValue.imageData;
    // 恢复原始尺寸(旋转等算子可能改过画布大小)
    if (m_originalWidth > 0 && m_originalHeight > 0) {
        myValue.bmpInfo.SetWidth(m_originalWidth);
        myValue.bmpInfo.SetHeight(m_originalHeight);
        m_contentQuad.setRect(m_originalWidth, m_originalHeight);
    }
    const QImage img = Function::ToDisplayImage(imageData,
                                                myValue.bmpInfo.GetWidth(),
                                                myValue.bmpInfo.GetHeight());
    QPixmap pixmap = QPixmap::fromImage(img);
    if (m_fitToWindow) {
        const QSize target = ui->imageLabel->size();
        if (target.isValid() && !target.isEmpty()) {
            pixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    ui->imageLabel->setPixmap(pixmap);
    imageDataHistory.clear();
    redoImageDataHistory.clear();
    {
        // 重置后把"原图"作为历史第 1 步，历史面板与撤销行为保持一致
        ImageSnapshot snap;
        snap.data = imageData;
        snap.width = myValue.bmpInfo.GetWidth();
        snap.height = myValue.bmpInfo.GetHeight();
        snap.quad = m_contentQuad;
        SaveImageDataToHistory(snap);
    }
    InvalidateRotateBase();
    UpdateStatus("已重置为原始图像");
    UpdateStatusInfo();
    UpdateHistoryPanel();
}

ReturnValue MainWindow::CheckOK(QLineEdit * lineEdit)
{
    ReturnValue returnValue;                 // 默认 isNull=true, isNumeric=false, value=0
    if (lineEdit == nullptr) {
        return returnValue;
    }
    const QString inputText = lineEdit->text().trimmed(); // 获取输入的文本
    if (inputText.isEmpty()) {
        returnValue.isNull = true;
        return returnValue;
    }
    //判断高和宽是否正确输入数字
    bool isNumeric = false;
    const double_t value = inputText.toDouble(&isNumeric);
    if (!isNumeric) {
        returnValue.isNull = false;
        returnValue.isNumeric = false;
        return returnValue;
    }
    returnValue.isNull = false;
    returnValue.isNumeric = true;
    returnValue.value = value;
    return returnValue;
}


MainWindow::~MainWindow()
{
    delete ui;
}

// 撤销操作，从链表中获取上一步的图像数据进行恢复
OperationType MainWindow::SaveImageDataToHistory(const ImageSnapshot &snap)
{
    // 保存当前图像数据到链表
    if (!imageDataHistory.empty()) {
        const ImageSnapshot &back = imageDataHistory.back();
        if (snap.data != back.data || snap.width != back.width || snap.height != back.height) {
            imageDataHistory.push_back(snap);
            // 控制历史长度，避免大图下内存无限增长
            while (static_cast<int>(imageDataHistory.size()) > m_historyLimit) {
                imageDataHistory.pop_front();
            }
            return UNDO;
        }
    } else {
        imageDataHistory.push_back(snap);
        return UNDO;
    }

    return REDO; // 返回重做类型
}

// 应用一个快照：写历史 + 同步尺寸 + 刷新显示
void MainWindow::ApplySnapshot(const ImageSnapshot &snap, bool keepRotateBase)
{
    if (snap.data.empty()) {
        return;
    }
    // 尺寸与像素数据必须严格匹配，否则显示阶段会读越界
    if (snap.width <= 0 || snap.height <= 0 ||
        snap.data.size() != static_cast<size_t>(snap.width) * static_cast<size_t>(snap.height) * 3) {
        UpdateStatus("结果尺寸与像素数据不一致，已忽略本次操作");
        return;
    }
    // 只有旋转结果才继续沿用无损基准；其它算子改的是像素本身，
    // 基准必须换成新的当前图像，否则后续旋转会丢掉这些修改
    if (!keepRotateBase) {
        InvalidateRotateBase();
    }
    SaveImageDataToHistory(snap);
    m_contentQuad = snap.quad;
    // 尺寸可能已被算子改变(例如旋转)，同步到文件头信息
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    myValue.bmpInfo.SetImageSize(static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    myValue.bmp.SetFileSize(sizeof(BMP) + sizeof(BMPInfo) +
                            static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    ShowImage(snap.data, myValue, snap.width, snap.height);
    UpdateHistoryPanel();
}


void MainWindow::UndoImageProcessing()
{
    if (imageDataHistory.size() <= 1) {
        qDebug() << "Undo operation failed: no more history";
        UpdateStatus("已经是最早的状态了");
        return;
    }
    // 当前状态压入重做栈
    CancelPreview();
    redoImageDataHistory.push_back(imageDataHistory.back());
    imageDataHistory.pop_back();

    const ImageSnapshot &snap = imageDataHistory.back();
    imageData = snap.data;
    m_contentQuad = snap.quad;
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    ShowImage(imageData, myValue, snap.width, snap.height);
    InvalidateRotateBase();   // 撤销后以当前图像作为新的旋转基准
    qDebug() << "Undo operation";
    UpdateStatus("已撤销");
    UpdateHistoryPanel();
}

void MainWindow::RedoImageProcessing()
{
    if (redoImageDataHistory.empty()) {
        qDebug() << "Redo operation failed: no redo history";
        UpdateStatus("没有可重做的操作");
        return;
    }
    CancelPreview();
    const ImageSnapshot snap = redoImageDataHistory.back();
    imageDataHistory.push_back(snap);
    redoImageDataHistory.pop_back();

    // 恢复图像数据
    imageData = snap.data;
    m_contentQuad = snap.quad;
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    ShowImage(imageData, myValue, snap.width, snap.height);
    InvalidateRotateBase();   // 重做后以当前图像作为新的旋转基准
    qDebug() << "Redo operation";
    UpdateStatus("已重做");
    UpdateHistoryPanel();
}

// 刷新右侧"历史记录"面板：每次图像状态变化后调用
void MainWindow::UpdateHistoryPanel()
{
    if (ui->historyList == nullptr) {
        return;
    }
    // 重建列表期间屏蔽信号，避免 setCurrentRow 反过来触发点击槽
    const QSignalBlocker blocker(ui->historyList);
    ui->historyList->clear();
    const int lastIndex = static_cast<int>(imageDataHistory.size()) - 1;
    int index = 0;
    for (auto it = imageDataHistory.begin(); it != imageDataHistory.end(); ++it, ++index) {
        QString text = QString("步骤 %1 · %2×%3").arg(index + 1).arg(it->width).arg(it->height);
        if (index == 0) {
            text += QString(" · 原图");
        }
        QListWidgetItem *entry = new QListWidgetItem(text);
        if (index == lastIndex) {
            // 当前步用加粗 + 箭头标记；列表禁用选中，避免系统高亮色破坏深色主题
            QFont font = entry->font();
            font.setBold(true);
            entry->setFont(font);
            entry->setText(QString("▶ ") + text);
        }
        ui->historyList->addItem(entry);
    }
    ui->historyList->scrollToBottom();
}

// 点击历史项：连续撤销/重做到目标步骤(历史是栈，当前状态永远是最后一项)
void MainWindow::on_historyList_itemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }
    const int target = ui->historyList->row(item);
    int current = static_cast<int>(imageDataHistory.size()) - 1;
    while (current > target && imageDataHistory.size() > 1) {
        UndoImageProcessing();
        --current;
    }
    while (current < target && !redoImageDataHistory.empty()) {
        RedoImageProcessing();
        ++current;
    }
}

// 打开图片文件并初始化界面状态（BMP 走自有解析器，其它格式由 Qt 插件解码）
bool MainWindow::LoadImage(const QString &path)
{
    CancelPreview();   // 打开新图前先回滚未提交的预览
    MyValue loaded;
    QString errMsg;
    if (!MYFunction::ReadImageFile(path.toStdString(), loaded, errMsg)) {
        function.CreateMessagebox("打开失败", errMsg);
        UpdateStatus("打开失败：" + errMsg);
        return false;
    }

    _SetVisible(true);
    BMPPath = path.toStdString();
    myValue = loaded;
    imageSize = myValue.imageData.size();
    pixelCount = imageSize / 3;
    segmentSize = pixelCount / static_cast<size_t>(std::max(1, num_threads));
    if (segmentSize == 0) segmentSize = 1;
    qDebug() << "image size is:" << imageSize << "pixel count:" << pixelCount;

    imageData = myValue.imageData;
    originalImageData = myValue.imageData;

    imageDataHistory.clear();
    redoImageDataHistory.clear();
    {
        ImageSnapshot snap;
        snap.data = imageData;
        snap.width = myValue.bmpInfo.GetWidth();
        snap.height = myValue.bmpInfo.GetHeight();
        snap.quad.setRect(snap.width, snap.height);
        m_originalWidth = snap.width;
        m_originalHeight = snap.height;
        m_contentQuad = snap.quad;
        InvalidateRotateBase();
        SaveImageDataToHistory(snap);
    }

    m_bmpImage = Function::ToDisplayImage(imageData,
                                          myValue.bmpInfo.GetWidth(),
                                          myValue.bmpInfo.GetHeight());
    ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    canSave = true;
    UpdateStatus(QString("已打开 %1").arg(QFileInfo(path).fileName()));
    UpdateStatusInfo();
    UpdateHistoryPanel();
    return true;
}

void MainWindow::on_openImage_triggered()
{
    // 文件默认路径
    const QString defaultPath = QDir::homePath();
    // 常见图片格式（BMP/PNG/JPG/GIF/TIFF/WebP/TGA/ICO/SVG）
    const QString filter = QString("图片文件 (*.bmp *.png *.jpg *.jpeg *.gif *.tif *.tiff *.webp *.tga *.ico *.svg);;"
                                   "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;GIF (*.gif);;"
                                   "TIFF (*.tif *.tiff);;WebP (*.webp);;TGA (*.tga);;图标 (*.ico *.icns);;SVG (*.svg)");
    const QString path = QFileDialog::getOpenFileName(this, "选择图片文件", defaultPath, filter);
    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    LoadImage(path);
}

void MainWindow::on_actionsave_triggered()
{
    if (!canSave || imageData.empty()) {
        function.CreateMessagebox("提示", "没有任何文件");
        qDebug() << "文件为空！";
        return;
    }

    // 保存当前正在显示的图像(而不是历史栈里的旧数据)
    const QString filter = QString("BMP 图像 (*.bmp);;PNG 图像 (*.png);;JPEG 图像 (*.jpg *.jpeg);;"
                                   "TIFF 图像 (*.tif *.tiff);;WebP 图像 (*.webp)");
    QString filePath = QFileDialog::getSaveFileName(this, "保存图像", "", filter);
    if (filePath.isEmpty()) {
        qDebug() << "保存操作已取消。";
        return;
    }

    QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix.isEmpty()) {
        filePath += ".bmp";
        suffix = "bmp";
    }

    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();

    // BMP：沿用项目自己的读写实现，保证自底向上的内存布局不被破坏
    if (suffix == "bmp") {
        BMP outBmp = myValue.bmp;
        BMPInfo outInfo = myValue.bmpInfo;
        MYFunction::SetBMPHeaderValues(outBmp, outInfo, width, height, outInfo.GetBitsPerPixel());
        if (MYFunction::WriteBMPFile(filePath.toStdString(), imageData, outBmp, outInfo)) {
            UpdateStatus(QString("已保存到 %1").arg(QFileInfo(filePath).fileName()));
        } else {
            function.CreateMessagebox("保存失败", "无法写入文件，请检查路径是否有效");
        }
        return;
    }

    // 其它格式：交给 Qt 图像插件（PNG / JPEG / TIFF / WebP）
    const QImage image = Function::ToDisplayImage(imageData, width, height);
    if (image.isNull()) {
        function.CreateMessagebox("保存失败", "当前图像数据无效");
        return;
    }
    QImageWriter writer(filePath);
    if (suffix == "jpg" || suffix == "jpeg") {
        writer.setQuality(92);
    }
    if (writer.write(image)) {
        UpdateStatus(QString("已导出 %1").arg(QFileInfo(filePath).fileName()));
    } else {
        function.CreateMessagebox("保存失败",
                                  "无法写入该格式：" + writer.errorString() +
                                  "\n（部分格式需要对应的 Qt 图像插件支持）");
        UpdateStatus("导出失败：" + QFileInfo(filePath).fileName());
    }
}

//清理每段数据,防止数据冲突引发异常
void MainWindow::ClearSegmentData()
{
    threads.clear();
    segmentStarts.clear();
}

// ------------------------------------------------------------------
// 算子调度
// ------------------------------------------------------------------

// 把算子按线程分段执行(以像素为单位，保证 BGR 三通道不会被拆散)
void MainWindow::RunSegmented(const OpContext &ctx,
                              const std::function<void(size_t, size_t)> &fn)
{
    const int n = std::max(1, ctx.numThreads);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const size_t start = static_cast<size_t>(i) * ctx.segmentSize;
        const size_t end = (i == n - 1) ? ctx.pixelCount
                                        : std::min(ctx.pixelCount, start + ctx.segmentSize);
        if (start >= end) {
            continue;
        }
        workers.emplace_back([&fn, start, end]() {
            fn(start, end);
        });
    }
    for (auto &t : workers) {
        t.join();
    }
}

void MainWindow::SetBusy(bool busy)
{
    m_busy = busy;
    // 处理中禁用主控区域，避免并发修改同一份像素数据
    ui->widget->setEnabled(!busy);
    ui->menu_edit->setEnabled(!busy);
    ui->menu_blur->setEnabled(!busy);
    ui->menu_base->setEnabled(!busy);
    ui->menu_unbase->setEnabled(!busy);
    ui->menu_transform->setEnabled(!busy);
    ui->menu_enhance->setEnabled(!busy);
    ui->menu_morph->setEnabled(!busy);
    // 处理中显示等待光标
    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::UpdateStatus(const QString &text)
{
    statusBar()->showMessage(text);
}

void MainWindow::UpdateStatusInfo()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    QString info;
    if (canSave && w > 0 && h > 0) {
        info = QString("%1 × %2 px   |   线程 %3   |   历史 %4 步")
                   .arg(w).arg(h).arg(num_threads).arg(static_cast<int>(imageDataHistory.size()));
    } else {
        info = QString("线程 %1").arg(num_threads);
    }
    if (QLabel *right = findChild<QLabel *>("statusInfoLabel")) {
        right->setText(info);
    } else {
        QLabel *label = new QLabel(info, this);
        label->setObjectName("statusInfoLabel");
        statusBar()->addPermanentWidget(label);
    }
}

// 在后台线程执行算子；完成后自动写历史并刷新显示
void MainWindow::ApplyOperator(const QString &title,
                               std::function<void(std::vector<uint8_t> &, OpContext &)> op,
                               const ImageSnapshot *sourceOverride,
                               std::function<void()> onDone)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    CancelPreview();   // 新算子从已提交的图像出发

    // 输入既可以是当前图像，也可以是旋转专用的一次性快照(无损基准)
    const std::vector<uint8_t> *input = &imageData;
    int32_t inWidth = myValue.bmpInfo.GetWidth();
    int32_t inHeight = myValue.bmpInfo.GetHeight();
    ContentQuad inQuad = m_contentQuad;
    if (sourceOverride && !sourceOverride->data.empty()) {
        input = &sourceOverride->data;
        inWidth = sourceOverride->width;
        inHeight = sourceOverride->height;
        inQuad = sourceOverride->quad;
    }

    OpContext ctx;
    ctx.width = inWidth;
    ctx.height = inHeight;
    ctx.pixelCount = input->size() / 3;
    ctx.numThreads = num_threads;
    ctx.segmentSize = ctx.pixelCount / static_cast<size_t>(std::max(1, num_threads));
    if (ctx.segmentSize == 0) ctx.segmentSize = 1;
    ctx.quad = inQuad;

    const std::vector<uint8_t> source = *input;   // 在 UI 线程取快照，避免数据竞争
    const bool fromRotation = (sourceOverride != nullptr);
    SetBusy(true);
    UpdateStatus(title + " 处理中…");

    auto result = std::make_shared<ImageSnapshot>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
            [this, watcher, result, title, fromRotation, onDone]() {
        watcher->deleteLater();
        if (result && !result->data.empty()) {
            ApplySnapshot(*result, fromRotation);
            UpdateStatus(title + " 完成");
        } else {
            UpdateStatus(title + " 未产生结果");
        }
        SetBusy(false);
        UpdateStatusInfo();
        if (onDone) {
            onDone();
        }
    });
    watcher->setFuture(QtConcurrent::run([source, op, ctx, result]() mutable {
        std::vector<uint8_t> work = source;
        op(work, ctx);              // 算子可以通过 ctx 修改画布宽高与内容区域(例如旋转)
        result->data = std::move(work);
        result->width = ctx.width;
        result->height = ctx.height;
        result->quad = ctx.quad;
    }));
}

// 任何改动像素/几何的非旋转操作、撤销、重做、重置、重新打开图片，
// 都会让"旋转无损基准"失效(下次旋转时以当前图像作为新基准)
void MainWindow::InvalidateRotateBase()
{
    m_rotBaseValid = false;
    m_rotTotal = 0.0;
    std::vector<uint8_t>().swap(m_rotBase);   // 真正释放内存
}

// 旋转：始终从无损基准按累计角度重新采样一次，
// 这样无论转多少次，插值只发生一次，不会越转越糊
void MainWindow::ApplyRotation(double deltaDegrees, const QString &title)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }

    // 第一次旋转：把当前图像作为基准(此时它还没有被旋转插值糊过)
    if (!m_rotBaseValid || m_rotBase.empty() ||
        m_rotBase.size() != static_cast<size_t>(m_rotBaseW) * m_rotBaseH * 3) {
        m_rotBase = imageData;
        m_rotBaseW = myValue.bmpInfo.GetWidth();
        m_rotBaseH = myValue.bmpInfo.GetHeight();
        m_rotBaseQuad = m_contentQuad;
        if (!m_rotBaseQuad.isValid()) {
            m_rotBaseQuad.setRect(m_rotBaseW, m_rotBaseH);
        }
        m_rotTotal = 0.0;
        m_rotBaseValid = true;
    }

    // 累计角度归一到 [0, 360)
    double total = std::fmod(m_rotTotal + deltaDegrees, 360.0);
    if (total < 0.0) {
        total += 360.0;
    }
    if (!std::isfinite(total)) {
        return;
    }

    ImageSnapshot base;
    base.data = m_rotBase;
    base.width = m_rotBaseW;
    base.height = m_rotBaseH;
    base.quad = m_rotBaseQuad;

    auto applied = std::make_shared<bool>(false);
    ApplyOperator(
        title,
        [total, applied](std::vector<uint8_t> &d, OpContext &ctx) {
            Function f;
            *applied = f.RotateImage(d, ctx.width, ctx.height, total, ctx.quad);
        },
        &base,
        [this, total, applied]() {
            if (*applied) {
                m_rotTotal = total;      // 基准继续沿用，角度已累计
            } else {
                // 本次旋转没有生效(角度为 0/360 的整数倍、或画布超出上限)：
                // 基准作废，回到"以当前图像为基准"的状态，避免状态错位
                InvalidateRotateBase();
            }
        });
}

// 子窗口结果写回主窗口
void MainWindow::ApplyResult(std::vector<uint8_t> &result)
{
    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();
    if (result.empty() || width <= 0 || height <= 0 ||
        result.size() != static_cast<size_t>(width) * height * 3) {
        UpdateStatus("子窗口结果与当前图像不一致，已忽略");
        return;
    }
    if (!m_contentQuad.isValid()) {
        m_contentQuad.setRect(width, height);
    }
    ImageSnapshot snap;
    snap.data = result;
    snap.width = width;
    snap.height = height;
    // 子窗口只改像素、不改画布几何；必须沿用当前内容区域，
    // 否则 quad 被重置为全 0 后，下一次旋转会退化成 1x1 黑图。
    snap.quad = m_contentQuad;
    ApplySnapshot(snap);
    UpdateStatus("已应用子窗口结果");
}

// ------------------------------------------------------------------
// 算子槽函数
// ------------------------------------------------------------------

void MainWindow::on_actiongray_triggered()
{
    ApplyOperator("灰度化", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.ConvertToGray(d, s, e);
        });
    });
}

void MainWindow::on_actionautoContrast_triggered()
{
    qDebug() << "click auto contrast";
    ApplyOperator("自动对比度", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        const float_t aver = static_cast<float_t>(this->function.CalAver(d));
        const float_t standard = static_cast<float_t>(this->function.CalStandard(d, aver));
        RunSegmented(ctx, [this, &d, aver, standard](size_t s, size_t e) {
            this->function.AutoContrast(d, aver, standard, s, e);
        });
    });
}

void MainWindow::on_actionaver_triggered()
{
    qDebug() << "aver";
    ApplyOperator("均值模糊", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d, ctx](size_t s, size_t e) {
            this->function.AverageBlur(d, static_cast<uint32_t>(ctx.width),
                                       static_cast<uint32_t>(ctx.height), s, e);
        });
    });
}

void MainWindow::on_btn_resetAll_clicked()
{
    ResetAll(myValue);
}

void MainWindow::on_actionblend_triggered()
{
    qDebug() << "i am in a blend window!";
    blendWindow = new BlendWindow(this, myValue);
    blendWindow->show();
}

void MainWindow::on_btn_brightness_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_brightness);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -150 || returnValue.value > 150) {
        function.CreateMessagebox("提示", "超出范围，范围是[-150,150]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("亮度调整", ui->lineEdit_brightness, value)) {
        return;
    }
    ApplyOperator("亮度调整", MakeBrightnessOp(value));
}

void MainWindow::on_btn_contrast_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_contrast);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -50 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[-50,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("对比度调整", ui->lineEdit_contrast, value)) {
        return;
    }
    ApplyOperator("对比度调整", MakeContrastOp(value));
}

void MainWindow::on_btn_saturation_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_saturation);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -100 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[-100,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("饱和度调整", ui->lineEdit_saturation, value)) {
        return;
    }
    ApplyOperator("饱和度调整", MakeSaturationOp(value));
}

void MainWindow::on_actioncolorBalance_triggered()
{
    ApplyOperator("色彩平衡", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.ColorBalance(d, ctx.width, ctx.height);
    });
}

void MainWindow::on_actioncolorMap_triggered()
{
    qDebug() << "I am in a color map window!";
    ColorMap *colorMap = new ColorMap(this, myValue);
    colorMap->show();
}

void MainWindow::on_actionreverse_triggered()
{
    ApplyOperator("颜色反转", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.InvertColors(d, s, e);
        });
    });
}

void MainWindow::on_actioncomplementary_triggered()
{
    ApplyOperator("补色", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.Complementary(d, s, e);
        });
    });
}

void MainWindow::on_actionliquefaction_triggered()
{
    qDebug() << "I am in a eye window!";
    eye *myEye = new eye(this, myValue);
    myEye->show();
}

void MainWindow::on_actionface_triggered()
{
    qDebug() << "I am in a face window!";
    Face *face = new Face(this, myValue);
    face->show();
}

void MainWindow::on_actionfishEye_triggered()
{
    ApplyOperator("鱼眼效果", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::promise<std::vector<uint8_t>> promise;
        std::future<std::vector<uint8_t>> future = promise.get_future();
        this->function.Fisheye(d, promise, ctx.width, ctx.height);
        d = future.get();
    });
}

void MainWindow::on_actiongauss_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 0, 0, 10, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    qDebug() << "用户输入的程度:" << degree;
    ApplyOperator("高斯模糊", [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> promise;
        std::future<std::vector<uint8_t>> future = promise.get_future();
        f.Gauss(d, promise, ctx.width, ctx.height, degree);
        d = future.get();
    });
}

void MainWindow::on_btn_rotate_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_rotate);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    const double_t angle = returnValue.value;
    ApplyRotation(angle, "顺时针旋转");
}

void MainWindow::on_btn_rotate_r_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_rotate_r);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    const double_t angle = returnValue.value;
    ApplyRotation(-angle, "逆时针旋转");
}

void MainWindow::on_actionmedian_triggered()
{
    qDebug() << "median";
    ApplyOperator("中值滤波", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.MedianBlur(d, static_cast<uint32_t>(ctx.width), static_cast<uint32_t>(ctx.height));
    });
}

void MainWindow::on_actionmosaic_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 4, 1, 100, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    const uint32_t step = static_cast<uint32_t>(std::max(1.0, degree));
    ApplyOperator("马赛克", [this, step](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.FullMosaic(d, static_cast<uint32_t>(ctx.width),
                                  static_cast<uint32_t>(ctx.height), step);
    });
}

void MainWindow::on_btn_shadow_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_shadow);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("阴影调整", ui->lineEdit_shadow, value)) {
        return;
    }
    ApplyOperator("阴影调整", MakeShadowOp(value));
}

void MainWindow::on_btn_highlight_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_highlight);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 255) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,255]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("高光调整", ui->lineEdit_highlight, value)) {
        return;
    }
    ApplyOperator("高光调整", MakeHighlightOp(value));
}

void MainWindow::on_btn_sharpen_clicked()
{
    qDebug() << "ok";
    ReturnValue returnValue = CheckOK(ui->lineEdit_sharpen);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 200) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,200]");
        return;
    }
    const double_t amount = returnValue.value;
    if (CommitPreview("锐化", ui->lineEdit_sharpen, amount)) {
        return;
    }
    ApplyOperator("锐化", MakeSharpenOp(amount));
}

void MainWindow::on_actionsobel_triggered()
{
    ApplyOperator("Sobel 边缘检测", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        d = this->function.SobelEdge(d, ctx.width, ctx.height);
    });
}

void MainWindow::on_btn_threshold_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_threshold);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 255) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,255]");
        return;
    }
    const double_t threshold = returnValue.value;
    if (CommitPreview("阈值处理", ui->lineEdit_threshold, threshold)) {
        return;
    }
    ApplyOperator("阈值处理", MakeThresholdOp(threshold));
}

void MainWindow::on_actionscale_triggered()
{
    qDebug() << "I am in a inter window!";
    Interpolation *inter = new Interpolation(this, myValue);
    inter->show();
}

void MainWindow::on_btn_undo_clicked()
{
    UndoImageProcessing();
}

void MainWindow::on_actionundo_triggered()
{
    UndoImageProcessing();
}

void MainWindow::on_actionreset_triggered()
{
    ResetAll(myValue);
}

void MainWindow::on_actionredo_triggered()
{
    qDebug() << "redo";
    RedoImageProcessing();
}

void MainWindow::_SetVisible(bool set)
{
    qDebug() << "Setting visibility to:" << set;
    ui->toolBar->setVisible(set);
    ui->menu_edit->menuAction()->setVisible(set);
    ui->menu_blur->menuAction()->setVisible(set);
    ui->menu_base->menuAction()->setVisible(set);
    ui->menu_unbase->menuAction()->setVisible(set);
    ui->menu_transform->menuAction()->setVisible(set);
    ui->menu_enhance->menuAction()->setVisible(set);
    ui->menu_morph->menuAction()->setVisible(set);
    ui->widget->setVisible(set);
}

void MainWindow::_SetShortCut()
{
    ui->openImage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    ui->actionsave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    ui->actionundo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    ui->actionreset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    ui->actionredo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("退出程序"), QString(tr("确认退出程序")),
                              QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::No) {
        event->ignore(); // 忽略退出信号，程序继续进行
    } else {
        event->accept(); // 接受退出信号，程序退出
    }
}

// 缩放相关：倍率必须 >= 1，否则会出现除零/取模崩溃
static bool AskScaleFactors(QWidget *parent, int &heightFactor, int &widthFactor)
{
    bool ok1 = false;
    bool ok2 = false;
    const int height = QInputDialog::getInt(parent, QObject::tr("输入高度倍数"), QObject::tr("高度倍数:"), 1, 1, 100, 1, &ok1);
    const int width = QInputDialog::getInt(parent, QObject::tr("输入宽度倍数"), QObject::tr("宽度倍数:"), 1, 1, 100, 1, &ok2);
    if (!ok1 || !ok2) {
        return false;
    }
    heightFactor = height;
    widthFactor = width;
    return true;
}

void MainWindow::on_actionlarge_near_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeNear(h, w);
}

void MainWindow::on_actionsmall_near_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallNear(h, w);
}

void MainWindow::on_actionlarge_bilinear_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeBilinear(h, w);
}

void MainWindow::on_actionsmall_bilinear_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallBilinear(h, w);
}

void MainWindow::on_actionlarge_bicubic_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeBicubic(h, w);
}

void MainWindow::on_actionsmall_bicubic_triggered()
{
    int h = 1, w = 1;
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallBicubic(h, w);
}

void MainWindow::on_actionhighContrast_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 1, 1, 100, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    ApplyOperator("高反差保留", [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> blurPromise;
        std::future<std::vector<uint8_t>> blurFuture = blurPromise.get_future();
        f.Gauss(d, blurPromise, ctx.width, ctx.height, degree);
        const std::vector<uint8_t> blurred = blurFuture.get();

        std::promise<std::vector<uint8_t>> highPromise;
        std::future<std::vector<uint8_t>> highFuture = highPromise.get_future();
        f.HighContrast(d, highPromise, blurred);
        d = highFuture.get();
    });
}

// ------------------------------------------------------------------
// P2-A：几何变换 / 增强 / 形态学 / 卷积 / USM
// ------------------------------------------------------------------
void MainWindow::on_actionflipH_triggered()
{
    ApplyOperator("水平翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Horizontal, ctx.quad);
    });
}

void MainWindow::on_actionflipV_triggered()
{
    ApplyOperator("垂直翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Vertical, ctx.quad);
    });
}

void MainWindow::on_actionrotate90_triggered()
{
    ApplyOperator("顺时针旋转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.RotateImage(d, ctx.width, ctx.height, 90.0, ctx.quad);
    });
}

void MainWindow::on_actionrotate90ccw_triggered()
{
    ApplyOperator("逆时针旋转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.RotateImage(d, ctx.width, ctx.height, 270.0, ctx.quad);
    });
}

void MainWindow::on_actiontranspose_triggered()
{
    ApplyOperator("沿主对角线镜像", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Transpose, ctx.quad);
    });
}

void MainWindow::on_actionantitranspose_triggered()
{
    ApplyOperator("沿副对角线镜像", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::AntiTranspose, ctx.quad);
    });
}

void MainWindow::on_actiontailor_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    const QImage display = Function::ToDisplayImage(imageData,
                                                    myValue.bmpInfo.GetWidth(),
                                                    myValue.bmpInfo.GetHeight());
    CropDialog dlg(display, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QRect sel = dlg.selectionInImage();
    if (sel.width() < 4 || sel.height() < 4) {
        UpdateStatus("裁剪选区太小，已取消");
        return;
    }
    ApplyOperator("图像裁剪", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.CropImage(d, ctx.width, ctx.height,
                    sel.x(), sel.y(), sel.width(), sel.height(), ctx.quad);
    });
}

void MainWindow::on_actionequalize_triggered()
{
    ApplyOperator("直方图均衡化", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        uint8_t lut[256];
        this->function.BuildEqualizeLUT(d, lut);
        RunSegmented(ctx, [this, &d, &lut](size_t s, size_t e) {
            this->function.ApplyLUT(d, lut, s, e);
        });
    });
}

void MainWindow::on_actionerode_triggered()
{
    ApplyOperator("腐蚀", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Erode, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actiondilate_triggered()
{
    ApplyOperator("膨胀", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Dilate, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionopenMorph_triggered()
{
    ApplyOperator("开运算", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Open, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actioncloseMorph_triggered()
{
    ApplyOperator("闭运算", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Close, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionconvolution_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty()) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    KernelDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const std::vector<double> kernel = dlg.kernel();
    const double scale = dlg.scale();
    const double offset = dlg.offset();
    ApplyOperator("自定义卷积", [this, kernel, scale, offset](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::vector<uint8_t> out;
        this->function.Convolve3x3(d, out, ctx.width, ctx.height,
                                   kernel.data(), scale, offset, 0, ctx.pixelCount);
        if (!out.empty()) {
            d.swap(out);
        }
    });
}

void MainWindow::on_actionusm_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty()) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    USMDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const double radius = dlg.radius();
    const double amount = dlg.amount();
    const double threshold = dlg.threshold();
    ApplyOperator("USM 锐化", [this, radius, amount, threshold](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::vector<uint8_t> out;
        this->function.USMSharpen(d, out, ctx.width, ctx.height, radius, amount, threshold);
        if (!out.empty() && out.size() == d.size()) {
            d.swap(out);
        }
    });
}
