#include "mainwindow.h"
#include "face.h"
#include "ui_face.h"



Face::Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Face), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    // 清掉 .ui 里自带的浅色样式，统一交给全局主题(Theme.h)接管
    setStyleSheet(QString());

    setWindowTitle(QString("图像微调"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    imageData = mainWindow->imageData;
    previewData = imageData;
    newValue = myValue;

    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));

    // 半径或中心点变化后自动预览：停止输入 250ms 就从原图重算一次
    previewTimer = new QTimer(this);
    previewTimer->setSingleShot(true);
    previewTimer->setInterval(250);
    connect(previewTimer, &QTimer::timeout, this, [this]() { RunPreview(false); });
    connect(ui->lineEdit_radius, &QLineEdit::textChanged, this, &Face::SchedulePreview);
}

void Face::mousePressEvent(QMouseEvent* event)
{
    const QPixmap *pixmap = ui->imageLabel->pixmap();
    if (pixmap == nullptr || pixmap->isNull()) {
        return;
    }
    // 事件坐标是控件坐标，需要先转换到 imageLabel 的局部坐标系
    const QPoint labelPos = ui->imageLabel->mapFrom(this, event->pos());
    // imageLabel 使用 AlignCenter 显示图像，必须扣掉图像相对控件左上角的居中偏移，
    // 否则点击位置会整体偏移，处理中心和点击位置对不上。
    const int offsetX = (ui->imageLabel->width() - pixmap->width()) / 2;
    const int offsetY = (ui->imageLabel->height() - pixmap->height()) / 2;
    const int displayX = labelPos.x() - offsetX;
    const int displayY = labelPos.y() - offsetY;
    if (displayX < 0 || displayY < 0 ||
        displayX >= pixmap->width() || displayY >= pixmap->height()) {
        return;
    }
    const int32_t imageWidth = newValue.bmpInfo.GetWidth();
    const int32_t imageHeight = newValue.bmpInfo.GetHeight();
    if (imageWidth <= 0 || imageHeight <= 0) {
        return;
    }
    // 显示图与像素一一对应；按比例换算，兼容未来启用缩放显示的情况
    const int imageX = static_cast<int>(
            static_cast<long long>(displayX) * imageWidth / pixmap->width());
    // 图像是自底向上存储的，视觉行 y 对应内存行 (height-1-y)
    const int imageY = imageHeight - 1 - static_cast<int>(
            static_cast<long long>(displayY) * imageHeight / pixmap->height());

    if (clickCount == 0) {
        // 第一次点击，保存坐标到firstClick
        firstClick = QPoint(imageX, imageY);
        qDebug() << "First click at: " << firstClick;
        clickCount++;
        hasFirst = true;
        firstX = firstClick.x();
        firstY = firstClick.y();
        faceCenterX = firstX;
        faceCenterY = firstY;
        const QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_first->setText(str);
    } else {
        // 再次点击：重新移动瘦脸中心点
        secondClick = QPoint(imageX, imageY);
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        secondX = secondClick.x();
        secondY = secondClick.y();
        firstX = secondX;
        firstY = secondY;
        faceCenterX = firstX;
        faceCenterY = firstY;
        const QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_first->setText(str);
    }
    SchedulePreview();   // 中心点变了就重新预览
}

Face::~Face()
{
    delete ui;
}

void Face::on_btn_ok_clicked()
{
    RunPreview(true);
}

void Face::SchedulePreview()
{
    if (previewTimer != nullptr) {
        previewTimer->start();
    }
}

bool Face::RunPreview(bool reportErrors)
{
    if (previewTimer != nullptr) {
        previewTimer->stop();
    }

    const QString inputText = ui->lineEdit_radius->text().trimmed();
    bool isNumeric = false;
    const double_t value = inputText.toDouble(&isNumeric);

    QString error;
    if (inputText.isEmpty()) {
        error = "请输入半径";
    } else if (!isNumeric) {
        error = "请输入数字";
    } else if (value <= 0) {
        error = "半径必须大于 0";
    } else if (reportErrors && !hasFirst) {
        error = "还没成功获取坐标值";
    }

    if (!error.isEmpty()) {
        if (reportErrors) {
            function.CreateMessagebox("提示", error);
        }
        previewData = imageData;   // 参数不完整：回到原图
        ShowImage(previewData);
        return false;
    }

    const int32_t width = newValue.bmpInfo.GetWidth();
    const int32_t height = newValue.bmpInfo.GetHeight();
    const int32_t radius = static_cast<int32_t>(value);

    // 每次都从原图重算(读 src 写 dst)，反复预览不会出现拖影
    previewData = imageData;
    if (hasFirst) {
        std::vector<uint8_t> dst(previewData.size(), 0);
        _Face(previewData, dst, width, height, faceCenterX, faceCenterY, radius, warpIntensity);
        previewData.swap(dst);
    }
    ShowImage(previewData);
    return true;
}



void Face::ShowImage(std::vector<uint8_t> &inImageData)
{
    const QImage image = Function::ToDisplayImage(inImageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void Face::_Face(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst,
                 int32_t width, int32_t height,
                 int32_t centerX, int32_t centerY, int32_t radius, double intensity)
{
    if (width <= 0 || height <= 0 || radius <= 0) {
        dst = src;
        return;
    }
    // 遍历图像像素
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const size_t pixelIndex = (static_cast<size_t>(y) * width + x) * 3;

            // 计算像素到中心点的距离
            const double dx = x - centerX;
            const double dy = static_cast<double>(y) - centerY;
            const double distance = std::sqrt(dx * dx + dy * dy);

            if (distance < radius) {
                const double warpAmount = intensity * std::sin(distance / radius * 3.14159265);

                int newX = static_cast<int>(x + warpAmount);
                int newY = static_cast<int>(y + warpAmount);

                newX = std::max(0, std::min(width - 1, newX));
                newY = std::max(0, std::min(height - 1, newY));

                const size_t targetPixelIndex = (static_cast<size_t>(newY) * width + newX) * 3;

                dst[pixelIndex] = src[targetPixelIndex];
                dst[pixelIndex + 1] = src[targetPixelIndex + 1];
                dst[pixelIndex + 2] = src[targetPixelIndex + 2];
            } else {
                dst[pixelIndex] = src[pixelIndex];
                dst[pixelIndex + 1] = src[pixelIndex + 1];
                dst[pixelIndex + 2] = src[pixelIndex + 2];
            }
        }
    }
}


void Face::on_btn_apply_clicked()
{
    qDebug() << "apply";
    // 先保证预览是最新参数的结果；参数非法时不写回主窗口
    if (!RunPreview(true)) {
        return;
    }
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(previewData);
}
