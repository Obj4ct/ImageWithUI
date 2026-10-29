
// MainWindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Theme.h"
#include <QAction>
#include <QMenu>

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
    ui->imageLabel->setText(QString("双击此处，或按 Ctrl+N 打开 24 位 BMP 图像"));
    statusBar()->showMessage("就绪 · 请先打开一张 BMP 图像");
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
    // 设置过滤
    const QString filter = "BMP文件(*.bmp)";
    const QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);
    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    LoadImage(path);
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

void MainWindow::ShowImage(std::vector<uint8_t> &inImageData, MyValue value, int32_t width, int32_t height)
{
    imageData = inImageData;
    myValue = value;

    const QImage image = Function::ToDisplayImage(inImageData, width, height);
    if (image.isNull()) {
        return;
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    if (m_fitToWindow) {
        const QSize target = ui->imageLabel->size();
        if (target.isValid() && !target.isEmpty()) {
            pixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    ui->imageLabel->setText(QString());
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(pixmap);
    UpdateStatusInfo();
}

void MainWindow::ResetAll(MyValue &myValue)
{
    imageData = myValue.imageData;
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
    UpdateStatus("已重置为原始图像");
    UpdateStatusInfo();
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
OperationType MainWindow::SaveImageDataToHistory(std::vector<uint8_t> &data)
{
    // 保存当前图像数据到链表
    if (!imageDataHistory.empty()) {
        if (data != imageDataHistory.back()) {
            imageDataHistory.push_back(data);
            // 控制历史长度，避免大图下内存无限增长
            while (static_cast<int>(imageDataHistory.size()) > m_historyLimit) {
                imageDataHistory.pop_front();
            }
            return UNDO;
        }
    } else {
        imageDataHistory.push_back(data);
        return UNDO;
    }

    return REDO; // 返回重做类型
}


void MainWindow::UndoImageProcessing()
{
    if (imageDataHistory.size() <= 1) {
        qDebug() << "Undo operation failed: no more history";
        UpdateStatus("已经是最早的状态了");
        return;
    }
    // 当前状态压入重做栈
    redoImageDataHistory.push_back(imageDataHistory.back());
    imageDataHistory.pop_back();

    std::vector<uint8_t> currentImageData = imageDataHistory.back();
    imageData = currentImageData;
    ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    qDebug() << "Undo operation";
    UpdateStatus("已撤销");
}

void MainWindow::RedoImageProcessing()
{
    if (redoImageDataHistory.empty()) {
        qDebug() << "Redo operation failed: no redo history";
        UpdateStatus("没有可重做的操作");
        return;
    }
    std::vector<uint8_t> nextImageData = redoImageDataHistory.back();
    imageDataHistory.push_back(nextImageData);
    redoImageDataHistory.pop_back();

    // 恢复图像数据
    imageData = nextImageData;
    ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    qDebug() << "Redo operation";
    UpdateStatus("已重做");
}

// 打开 BMP 文件并初始化界面状态
bool MainWindow::LoadImage(const QString &path)
{
    MyValue loaded;
    QString errMsg;
    if (!MYFunction::ReadBMPFile(path.toStdString(), loaded, errMsg)) {
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
    SaveImageDataToHistory(imageData);

    m_bmpImage = Function::ToDisplayImage(imageData,
                                          myValue.bmpInfo.GetWidth(),
                                          myValue.bmpInfo.GetHeight());
    ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    canSave = true;
    UpdateStatus(QString("已打开 %1").arg(QFileInfo(path).fileName()));
    UpdateStatusInfo();
    return true;
}

void MainWindow::on_openImage_triggered()
{
    // 文件默认路径
    const QString defaultPath = QDir::homePath();
    // 设置过滤
    const QString filter = "BMP文件(*.bmp)";
    const QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);
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
    std::vector<uint8_t> saveImageData = imageData;

    QString filePath = QFileDialog::getSaveFileName(nullptr, "保存文件", "", "BMP文件(*.bmp)");
    if (filePath.isEmpty()) {
        qDebug() << "保存操作已取消。";
        return;
    }
    if (!filePath.endsWith(".bmp", Qt::CaseInsensitive)) {
        filePath += ".bmp";
    }

    // 用当前数据的实际尺寸重写头信息，保证保存出来的文件可被正常读取
    BMP outBmp = myValue.bmp;
    BMPInfo outInfo = myValue.bmpInfo;
    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();
    MYFunction::SetBMPHeaderValues(outBmp, outInfo, width, height, outInfo.GetBitsPerPixel());

    if (MYFunction::WriteBMPFile(filePath.toStdString(), saveImageData, outBmp, outInfo)) {
        UpdateStatus(QString("已保存到 %1").arg(QFileInfo(filePath).fileName()));
        qDebug() << "保存成功！";
    } else {
        function.CreateMessagebox("保存失败", "无法写入文件，请检查路径是否有效");
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
                               std::function<void(std::vector<uint8_t> &, const OpContext &)> op)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }

    OpContext ctx;
    ctx.width = myValue.bmpInfo.GetWidth();
    ctx.height = myValue.bmpInfo.GetHeight();
    ctx.pixelCount = imageData.size() / 3;
    ctx.numThreads = num_threads;
    ctx.segmentSize = ctx.pixelCount / static_cast<size_t>(std::max(1, num_threads));
    if (ctx.segmentSize == 0) ctx.segmentSize = 1;

    const std::vector<uint8_t> source = imageData;   // 在 UI 线程取快照，避免数据竞争
    SetBusy(true);
    UpdateStatus(title + " 处理中…");

    auto result = std::make_shared<std::vector<uint8_t>>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher, result, title]() {
        watcher->deleteLater();
        if (result && !result->empty()) {
            SaveImageDataToHistory(*result);
            ShowImage(*result, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
            UpdateStatus(title + " 完成");
        } else {
            UpdateStatus(title + " 未产生结果");
        }
        SetBusy(false);
        UpdateStatusInfo();
    });
    watcher->setFuture(QtConcurrent::run([source, op, ctx, result]() mutable {
        std::vector<uint8_t> work = source;
        op(work, ctx);
        *result = std::move(work);
    }));
}

// 子窗口结果写回主窗口
void MainWindow::ApplyResult(std::vector<uint8_t> &result)
{
    if (result.empty()) {
        return;
    }
    SaveImageDataToHistory(result);
    ShowImage(result, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
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
    ApplyOperator("亮度调整", [value](std::vector<uint8_t> &d, const OpContext &) {
        Function f;
        f.Brightness(d, value);
    });
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
    ApplyOperator("对比度调整", [value](std::vector<uint8_t> &d, const OpContext &) {
        Function f;
        f.Contrast(d, value);
    });
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
    const double_t value = returnValue.value;
    ApplyOperator("饱和度调整", [value](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        f.Saturation(d, ctx.width, ctx.height, value);
    });
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
    ApplyOperator("顺时针旋转", [angle](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        f.RotateImage(d, ctx.width, ctx.height, angle);
    });
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
    ApplyOperator("逆时针旋转", [angle](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        f.RotateReverse(d, ctx.width, ctx.height, angle);
    });
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
    const double_t value = returnValue.value;
    ApplyOperator("阴影调整", [value](std::vector<uint8_t> &d, const OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.MakeShadow(d, out, value);
        d.swap(out);
    });
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
    const double_t value = returnValue.value;
    ApplyOperator("高光调整", [value](std::vector<uint8_t> &d, const OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.HighLight(d, out, value);
        d.swap(out);
    });
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
    const double_t amount = returnValue.value;
    ApplyOperator("锐化", [amount](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        // 1) 高斯模糊
        std::promise<std::vector<uint8_t>> blurPromise;
        std::future<std::vector<uint8_t>> blurFuture = blurPromise.get_future();
        f.Gauss(d, blurPromise, ctx.width, ctx.height, amount);
        const std::vector<uint8_t> blurred = blurFuture.get();
        // 2) 高反差保留
        std::promise<std::vector<uint8_t>> highPromise;
        std::future<std::vector<uint8_t>> highFuture = highPromise.get_future();
        f.HighContrast(d, highPromise, blurred);
        const std::vector<uint8_t> highContrast = highFuture.get();
        // 3) 锐化
        std::promise<std::vector<uint8_t>> sharpPromise;
        std::future<std::vector<uint8_t>> sharpFuture = sharpPromise.get_future();
        f.Sharpen(d, sharpPromise, highContrast);
        d = sharpFuture.get();
    });
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
    ApplyOperator("阈值处理", [this, threshold](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d, threshold](size_t s, size_t e) {
            this->function.ApplyThreshold(d, threshold, s, e);
        });
    });
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
