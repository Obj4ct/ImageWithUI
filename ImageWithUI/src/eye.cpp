#include "mainwindow.h"
#include "eye.h"
#include "ui_eye.h"

eye::eye(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::eye), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    setWindowTitle(QString("眼部区域液化"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    //默认显示原始图像
    imageData = mainWindow->imageData;
    newValue = myValue;
    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    // 显示原始图像在imageLabel上
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void eye::mousePressEvent(QMouseEvent* event)
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
        const QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_first->setText(str);
    } else {
        // 第二次点击，保存坐标到secondClick
        secondClick = QPoint(imageX, imageY);
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        hasSecond = true;
        secondX = secondClick.x();
        secondY = secondClick.y();
        const QString str = QString("%1, %2").arg(secondX).arg(secondY);
        ui->label_second->setText(str);
    }
}

void eye::Eye(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst,
              int32_t width, int32_t height,
              int32_t centerX, int32_t centerY, int32_t radius, double intensity,
              size_t startRow, size_t endRow)
{
    if (width <= 0 || height <= 0 || radius <= 0) {
        return;
    }
    for (size_t row = startRow; row < endRow && row < static_cast<size_t>(height); ++row) {
        for (int x = 0; x < width; ++x) {
            const size_t pixelIndex = (row * width + static_cast<size_t>(x)) * 3;

            const double dxCenter = x - centerX;
            const double dyCenter = static_cast<double>(row) - centerY;
            const double distance = std::sqrt(dxCenter * dxCenter + dyCenter * dyCenter);

            if (distance < radius) {
                const double warpAmount = intensity * std::sin(distance / radius * 3.14159265);
                const double newX = x + warpAmount;
                const double newY = static_cast<double>(row) + warpAmount;
                //---------------------//
                //x0 和 y0 是 newX 和 newY 的整数部分，表示目标像素坐标的左上角像素位置。
                int x0 = static_cast<int>(std::floor(newX));
                int y0 = static_cast<int>(std::floor(newY));
                // 边界保护：液化位移可能把采样点推到图像之外
                x0 = std::max(0, std::min(width - 1, x0));
                y0 = std::max(0, std::min(height - 1, y0));
                //x1 和 y1 是 x0 和 y0 分别加 1 后的值，表示目标像素坐标的右下角像素位置。这里使用 std::min 函数确保不超出图像边界。
                const int x1 = std::min(x0 + 1, width - 1);
                const int y1 = std::min(y0 + 1, height - 1);
                //dx 和 dy 是 newX - x0 和 newY - y0，表示目标像素坐标的浮点偏移量。
                const double fx = newX - x0;
                const double fy = newY - y0;
                //--------------------//
                const size_t targetPixelIndex00 = (static_cast<size_t>(y0) * width + x0) * 3;
                const size_t targetPixelIndex01 = (static_cast<size_t>(y0) * width + x1) * 3;
                const size_t targetPixelIndex10 = (static_cast<size_t>(y1) * width + x0) * 3;
                const size_t targetPixelIndex11 = (static_cast<size_t>(y1) * width + x1) * 3;

                for (int c = 0; c < 3; c++) {
                    // 双线性插值取样
                    const double interpolatedValue =
                            src[targetPixelIndex00 + c] * (1 - fx) * (1 - fy) +
                            src[targetPixelIndex01 + c] * fx * (1 - fy) +
                            src[targetPixelIndex10 + c] * (1 - fx) * fy +
                            src[targetPixelIndex11 + c] * fx * fy;

                    dst[pixelIndex + c] = static_cast<uint8_t>(std::max(0.0, std::min(255.0, interpolatedValue)));
                }
            } else {
                // 区域外保持原样
                dst[pixelIndex] = src[pixelIndex];
                dst[pixelIndex + 1] = src[pixelIndex + 1];
                dst[pixelIndex + 2] = src[pixelIndex + 2];
            }
        }
    }
}

void eye::ShowImage(std::vector<uint8_t>&inImageData)
{
    const QImage image = Function::ToDisplayImage(inImageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

eye::~eye()
{
    delete ui;
}

void eye::on_btn_ok_clicked()
{
    qDebug() << "click ok";
    ReturnValue returnRange = mainWindow->CheckOK(ui->lineEdit_range);
    ReturnValue returnNum = mainWindow->CheckOK(ui->lineEdit_increase);
    if (returnRange.isNull || returnNum.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnRange.isNumeric || !returnNum.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnRange.value <= 0) {
        function.CreateMessagebox("提示", "半径必须大于 0");
        return;
    }
    if (!hasFirst || !hasSecond) {
        function.CreateMessagebox("提示", "还没成功获取两个坐标值");
        return;
    }

    const int32_t width = newValue.bmpInfo.GetWidth();
    const int32_t height = newValue.bmpInfo.GetHeight();
    const int32_t radius = static_cast<int32_t>(returnRange.value);
    const double intensity = returnNum.value;

    // 输入快照与输出缓冲分离：线程之间只写自己负责的行，避免数据竞争
    const std::vector<uint8_t> src = imageData;
    std::vector<uint8_t> tmp = src;   // 第一个点的结果
    std::vector<uint8_t> dst = src;   // 第二个点的结果

    const int numThreads = std::max(1, mainWindow->num_threads);
    const size_t rows = static_cast<size_t>(height);
    const size_t rowsPerThread = std::max<size_t>(1, rows / static_cast<size_t>(numThreads));

    auto runPhase = [this, &src, width, height, radius, intensity, numThreads, rows, rowsPerThread](
                        const std::vector<uint8_t> &in, std::vector<uint8_t> &out,
                        int32_t cx, int32_t cy) {
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(numThreads));
        for (int i = 0; i < numThreads; ++i) {
            const size_t start = static_cast<size_t>(i) * rowsPerThread;
            const size_t end = (i == numThreads - 1) ? rows : std::min(rows, start + rowsPerThread);
            if (start >= end) {
                continue;
            }
            workers.emplace_back([this, &in, &out, width, height, radius, intensity, cx, cy, start, end]() {
                this->Eye(in, out, width, height, cx, cy, radius, intensity, start, end);
            });
        }
        for (auto &thread : workers) {
            thread.join();
        }
    };

    // 两阶段：先做第一个点，全部线程结束后再做第二个点(否则会跨线程读写同一份数据)
    runPhase(src, tmp, firstX, firstY);
    runPhase(tmp, dst, secondX, secondY);

    imageData = dst;
    ShowImage(imageData);
}

void eye::on_btn_apply_clicked()
{
    qDebug() << "apply";
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(imageData);
}
