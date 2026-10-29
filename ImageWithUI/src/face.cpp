#include "mainwindow.h"
#include "face.h"
#include "ui_face.h"



Face::Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Face), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);

    setWindowTitle(QString("图像微调"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    imageData = mainWindow->imageData;
    newValue = myValue;

    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void Face::mousePressEvent(QMouseEvent* event)
{
    // 事件坐标是控件坐标，需要先转换到 imageLabel 的局部坐标系
    const QPoint labelPos = ui->imageLabel->mapFrom(this, event->pos());
    if (!ui->imageLabel->rect().contains(labelPos)) {
        return;
    }
    // 图像是自底向上存储的，视觉行 y 对应内存行 (height-1-y)
    const int imageX = labelPos.x();
    const int imageY = newValue.bmpInfo.GetHeight() - 1 - labelPos.y();

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
}

Face::~Face()
{
    delete ui;
}

void Face::on_btn_ok_clicked()
{
    const QString inputText = ui->lineEdit_radius->text().trimmed();
    bool isNumeric = false;
    const double_t value = inputText.toDouble(&isNumeric);

    if (!hasFirst) {
        function.CreateMessagebox("提示", "还没成功获取坐标值");
        return;
    }
    if (!isNumeric) {
        function.CreateMessagebox("提示", "请输入数字");
        return;
    }
    if (value <= 0) {
        function.CreateMessagebox("提示", "半径必须大于 0");
        return;
    }

    const int32_t width = newValue.bmpInfo.GetWidth();
    const int32_t height = newValue.bmpInfo.GetHeight();
    const int32_t radius = static_cast<int32_t>(value);

    // 读快照写新缓冲，避免原地覆盖像素造成拖影
    const std::vector<uint8_t> src = imageData;
    std::vector<uint8_t> dst(src.size(), 0);
    _Face(src, dst, width, height, faceCenterX, faceCenterY, radius, warpIntensity);

    imageData = dst;
    ShowImage(imageData);
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
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(imageData);
}
