#include "mainwindow.h"
#include "blendwindow.h"
#include "ui_blendwindow.h"
#include <QSignalBlocker>
BlendWindow::BlendWindow(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::BlendWindow), mainWindow(mainWindow), myValue(myValue) {
    ui->setupUi(this);
    // 清掉 .ui 里自带的浅色样式，统一交给全局主题(Theme.h)接管
    setStyleSheet(QString());
    setWindowTitle(QString("图层混合"));
    setWindowIcon(QIcon(":/icon/logo.png"));

    // 设置滑块的最小值和最大值
    ui->horizontalSlider_alpha->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_alpha->setMaximum(10);  // 设置最大值
    // alpha 默认 1(不透明度 100%)，与常见图像软件一致；构造期间屏蔽信号，避免提前混合
    {
        const QSignalBlocker blocker(ui->horizontalSlider_alpha);
        ui->horizontalSlider_alpha->setValue(ui->horizontalSlider_alpha->maximum());
    }
    ui->widget_select_alpha->setVisible(true);
    currentAlpha = 1.0;
    // 设置下拉框的初始状态
    ui->comboBox_blendMode->addItem("<请选择>");
    ui->comboBox_blendMode->addItem("正常");
    ui->comboBox_blendMode->addItem("正片叠底");
    ui->comboBox_blendMode->addItem("滤色");
    ui->comboBox_blendMode->addItem("叠加");

    //默认显示原始图像
    originalImageData_1=mainWindow->imageData;
    myValue_1=myValue;

    // 用统一的转换入口：显式行宽(width*3)并校验缓冲区大小，
    // 避免宽度不是 4 的倍数时 QImage 按对齐行宽读取越界
    QImage image = Function::ToDisplayImage(originalImageData_1,
                                           myValue_1.bmpInfo.GetWidth(),
                                           myValue_1.bmpInfo.GetHeight());

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel_1->setPixmap(pixmap);
    ui->imageLabel_1->setScaledContents(true);
}

BlendWindow::~BlendWindow() {
    delete ui;
}


void BlendWindow::on_btn_open_clicked() {
    QString defaultPath = QDir::currentPath();
    QString filter = QString("图片文件 (*.bmp *.png *.jpg *.jpeg *.gif *.tif *.tiff *.webp *.tga *.ico *.svg)");
    QString path = QFileDialog::getOpenFileName(this, "选择图片文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }

    std::string BMPPath = path.toStdString();
    QString errMsg;
    if (!MYFunction::ReadImageFile(BMPPath, myValue_2, errMsg)) {
        Function function;
        function.CreateMessagebox("打开失败", errMsg);
        return;
    }

    originalImageData_2 = myValue_2.imageData;


    bmpImage_2 = Function::ToDisplayImage(originalImageData_2,
                                          myValue_2.bmpInfo.GetWidth(),
                                          myValue_2.bmpInfo.GetHeight());

    QPixmap pixmap = QPixmap::fromImage(bmpImage_2);
    ui->imageLabel_2->setPixmap(pixmap);


    ui->imageLabel_2->setScaledContents(true);
}

void BlendWindow::on_btn_apply_clicked()
{
    qDebug() << "apply";
    const int32_t width = myValue_1.bmpInfo.GetWidth();
    const int32_t height = myValue_1.bmpInfo.GetHeight();
    if (width <= 0 || height <= 0 ||
        originalImageData_1.size() != static_cast<size_t>(width) * height * 3) {
        Function function;
        function.CreateMessagebox("提示", "没有可应用的混合结果");
        return;
    }
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(originalImageData_1);
}

void BlendWindow::BlendImages()
{
    if (m_blending) {
        return;   // 防止模态提示框或连续拖动滑块导致的递归调用
    }
    m_blending = true;
    struct BlendGuard {
        bool &flag;
        ~BlendGuard() { flag = false; }
    } guard{m_blending};

    // 主窗口图像可能被旋转/替换/撤销过，每次都按当前状态取快照，
    // 不再使用构造时的 myValue_1 尺寸，避免缓冲区越界(黑图/崩溃)。
    myValue_1 = mainWindow->myValue;
    originalImageData_1 = mainWindow->imageData;

    if (myValue_2.imageData.empty()) {
        Function function;
        function.CreateMessagebox("提示", "请选择图片!!");
        return;
    }

    const int32_t width = myValue_1.bmpInfo.GetWidth();
    const int32_t height = myValue_1.bmpInfo.GetHeight();
    if (width <= 0 || height <= 0) {
        return;
    }
    const size_t expected = static_cast<size_t>(width) * height * 3;
    if (originalImageData_1.size() != expected) {
        Function function;
        function.CreateMessagebox("提示", "当前图像数据不完整，请重新打开图像");
        return;
    }
    if (myValue_2.imageData.size() != expected ||
        myValue_2.bmpInfo.GetWidth() != width ||
        myValue_2.bmpInfo.GetHeight() != height) {
        Function function;
        function.CreateMessagebox("提示", "图片大小不一致!!");
        return;
    }
    originalImageData_2 = myValue_2.imageData;

    const size_t pixelCount = expected / 3;
    const int threadCount = std::max(1, mainWindow->num_threads);
    // 向上取整切分，保证最后一段覆盖到末尾，不丢像素
    const size_t segmentSize = (pixelCount + static_cast<size_t>(threadCount) - 1) /
                               static_cast<size_t>(threadCount);
    const BlendMode blendMode = currentBlendMode;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(threadCount));
    for (int i = 0; i < threadCount; ++i) {
        const size_t start = std::min(pixelCount, static_cast<size_t>(i) * segmentSize);
        const size_t end = std::min(pixelCount, start + segmentSize);
        if (start >= end) {
            break;
        }
        threads.emplace_back(&BlendWindow::Effect, this,
                             std::ref(originalImageData_1),
                             std::cref(originalImageData_2),
                             width, height, blendMode, start, end);
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ShowImage();
}

void BlendWindow::ShowImage()
{
    const int32_t width = myValue_1.bmpInfo.GetWidth();
    const int32_t height = myValue_1.bmpInfo.GetHeight();
    if (width <= 0 || height <= 0 ||
        originalImageData_1.size() != static_cast<size_t>(width) * height * 3) {
        return;
    }
    QImage image = Function::ToDisplayImage(originalImageData_1, width, height);
    if (image.isNull()) {
        return;
    }
    // 显示图像在imageLabel上
    ui->blendImageLabel->setPixmap(QPixmap::fromImage(image));
    ui->blendImageLabel->setScaledContents(true);
}

void BlendWindow::SwitchBlendMode(uint8_t &destR, uint8_t &destG, uint8_t &destB, uint8_t srcR, uint8_t srcG, uint8_t srcB,
                                  BlendMode blendMode, double alpha) {
    // 先算出混合模式的目标色，再按 alpha 与原始底色做线性合成。
    // 这样“不透明度”对正片叠底/滤色/叠加同样有效，滑块不会失效。
    const double a = std::max(0.0, std::min(1.0, alpha));
    uint8_t mixR = destR;
    uint8_t mixG = destG;
    uint8_t mixB = destB;

    switch (blendMode) {
    // 正常模式：直接用源色
    case BLEND_NORMAL: {
        mixR = srcR;
        mixG = srcG;
        mixB = srcB;
        break;
    }

        // 正片叠底：源色与底色相乘，相当于给底色加一层遮罩/阴影
    case BLEND_MULTIPLY: {
        mixR = static_cast<uint8_t>((destR * srcR) / 255);
        mixG = static_cast<uint8_t>((destG * srcG) / 255);
        mixB = static_cast<uint8_t>((destB * srcB) / 255);
        break;
    }
        // 滤色：正片叠底的反向运算，结果整体变亮
    case BLEND_SCREEN: {
        mixR = static_cast<uint8_t>(255 - (255 - destR) * (255 - srcR) / 255);
        mixG = static_cast<uint8_t>(255 - (255 - destG) * (255 - srcG) / 255);
        mixB = static_cast<uint8_t>(255 - (255 - destB) * (255 - srcB) / 255);
        break;
    }
        // 叠加：底色暗时用正片叠底，底色亮时用滤色，保持高光与阴影
    case BLEND_OVERLAY: {
        if (destR < 128) {
            mixR = static_cast<uint8_t>((2 * destR * srcR) / 255);
        } else {
            mixR = static_cast<uint8_t>(255 - (2 * (255 - destR) * (255 - srcR)) / 255);
        }
        if (destG < 128) {
            mixG = static_cast<uint8_t>((2 * destG * srcG) / 255);
        } else {
            mixG = static_cast<uint8_t>(255 - (2 * (255 - destG) * (255 - srcG)) / 255);
        }
        if (destB < 128) {
            mixB = static_cast<uint8_t>((2 * destB * srcB) / 255);
        } else {
            mixB = static_cast<uint8_t>(255 - (2 * (255 - destB) * (255 - srcB)) / 255);
        }
        break;
    }

    default:
        return;
    }

    destR = static_cast<uint8_t>(destR * (1.0 - a) + mixR * a);
    destG = static_cast<uint8_t>(destG * (1.0 - a) + mixG * a);
    destB = static_cast<uint8_t>(destB * (1.0 - a) + mixB * a);
}

void BlendWindow::Effect(std::vector<uint8_t> &imageData, const std::vector<uint8_t> &effectData, int width, int height,
                         BlendMode blendMode,size_t start,size_t end) {
    double alpha=currentAlpha;

    for (size_t i = start; i < end; ++i) {
        uint8_t &destR = imageData[i * 3];
        uint8_t &destG = imageData[i * 3 + 1];
        uint8_t &destB = imageData[i * 3 + 2];
        uint8_t srcR = effectData[i * 3];
        uint8_t srcG = effectData[i * 3 + 1];
        uint8_t srcB = effectData[i * 3 + 2];
        SwitchBlendMode(destR, destG, destB, srcR, srcG, srcB, blendMode, alpha);
    }
}


void BlendWindow::on_comboBox_blendMode_currentIndexChanged(int index)
{
    // alpha 对四种模式都有效，所以滑块始终可见，不再随模式隐藏
    ui->widget_select_alpha->setVisible(true);

    if (index == 0) {
        qDebug() << "current index is 0";
        return;
    } else if (index == 1) {
        currentBlendMode = BLEND_NORMAL;
    } else if (index == 2) {
        currentBlendMode = BLEND_MULTIPLY;
    } else if (index == 3) {
        currentBlendMode = BLEND_SCREEN;
    } else if (index == 4) {
        currentBlendMode = BLEND_OVERLAY;
    } else {
        return;
    }
    BlendImages();
}
void BlendWindow::on_horizontalSlider_alpha_valueChanged(int value)
{
    currentAlpha = static_cast<double>(value) / ui->horizontalSlider_alpha->maximum();
    qDebug() << "current alpha is " << currentAlpha;
    BlendImages();
}



