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
    imageData=mainWindow->imageData;
    newValue=myValue;
    QImage image(imageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

    Function function;

}

void eye::mousePressEvent(QMouseEvent* event)
{
    QPoint clickPos = event->pos();
    // 将窗口坐标转换为图像坐标
    int imageX = clickPos.x();
    int imageY = newValue.bmpInfo.GetHeight() - clickPos.y();  // 图像上下翻转
    if (clickCount == 0) {

        // 第一次点击，保存坐标到firstClick
        firstClick = QPoint(imageX, imageY);
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "First click at: " << firstClick;
        clickCount++;
        firstX=firstClick.x();
        firstY=firstClick.y();
        QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_first->setText(str);
    } else if (clickCount == 1) {
        // 第二次点击，保存坐标到secondClick
        secondClick = QPoint(imageX, imageY);
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        secondX=secondClick.x();
        secondY=secondClick.y();
        QString str = QString("%1, %2").arg(secondX).arg(secondY);
        ui->label_second->setText(str);

    }


}

void eye::Eye(std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t centerX, int32_t centerY, int32_t radius, double intensity,size_t startRow,size_t endRow)
{
    for (int y = startRow; y < endRow; y++) {
        for (int x = 0; x < width; x++) {
            int pixelIndex = (y * width + x) * 3;

            double distance = std::sqrt(static_cast<double>((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY)));

            if (distance < radius) {
                double warpAmount = intensity * std::sin(distance / radius * 3.14159265);
                double newX = x + warpAmount;
                double newY = y + warpAmount;
                //---------------------//
                //x0 和 y0 是 newX 和 newY 的整数部分，表示目标像素坐标的左上角像素位置。
                int x0 = static_cast<int>(newX);
                int y0 = static_cast<int>(newY);
                //x1 和 y1 是 x0 和 y0 分别加 1 后的值，表示目标像素坐标的右下角像素位置。这里使用 std::min 函数确保不超出图像边界。
                int x1 = std::min(x0 + 1, width - 1);
                int y1 = std::min(y0 + 1, height - 1);
                //dx 和 dy 是 newX - x0 和 newY - y0，表示目标像素坐标的浮点偏移量。
                double dx = newX - x0;
                double dy = newY - y0;
                //这样，x0 和 y0 表示左上角像素，x1 和 y1 表示右下角像素，而 dx 和 dy 表示在这个像素坐标中的相对位置。这些信息将用于进行双线性插值，以获取目标像素的颜色值。
                //--------------------//
                int targetPixelIndex00 = (y0 * width + x0) * 3;
                int targetPixelIndex01 = (y0 * width + x1) * 3;
                int targetPixelIndex10 = (y1 * width + x0) * 3;
                int targetPixelIndex11 = (y1 * width + x1) * 3;


                for (int c = 0; c < 3; c++) {
                    //计算了目标像素的颜色值。在这里，targetPixelIndex00、targetPixelIndex01、targetPixelIndex10 和 targetPixelIndex11 分别是目标像素周围四个相邻像素的索引。
                    //imageData[targetPixelIndex00 + c] 表示左上角像素在当前颜色通道上的值乘以 (1 - dx) * (1 - dy)。
                    //imageData[targetPixelIndex01 + c] 表示右上角像素在当前颜色通道上的值乘以 dx * (1 - dy)。
                    //imageData[targetPixelIndex10 + c] 表示左下角像素在当前颜色通道上的值乘以 (1 - dx) * dy。
                    //imageData[targetPixelIndex11 + c] 表示右下角像素在当前颜色通道上的值乘以 dx * dy。
                    //这些值通过线性插值相加，然后被转换为无符号8位整数，并存储在 imageData[pixelIndex + c] 中，完成了对目标像素颜色的插值。
                    double interpolatedValue = imageData[targetPixelIndex00 + c] * (1 - dx) * (1 - dy) +
                            imageData[targetPixelIndex01 + c] * dx * (1 - dy) +
                            imageData[targetPixelIndex10 + c] * (1 - dx) * dy +
                            imageData[targetPixelIndex11 + c] * dx * dy;

                    imageData[pixelIndex + c] = static_cast<uint8_t>(interpolatedValue);
                }
            }
        }
    }
}

void eye::ShowImage(std::vector<uint8_t>&inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

}

eye::~eye()
{
    delete ui;
}






void eye::on_btn_ok_clicked()
{
    //imageData=mainWindow->imageData;
    qDebug()<<"click ok";
    ReturnValue returnRange=mainWindow->CheckOK(ui->lineEdit_range);
    ReturnValue returnNum=mainWindow->CheckOK(ui->lineEdit_increase);
    if(returnRange.isNull==true||returnNum.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnRange.isNumeric==false||returnNum.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }
    else if(ui->label_first->text()==nullptr||ui->label_second->text()==nullptr||firstX==0||firstY==0||secondX==0||secondY==0)
    {
        if(!function.CreateMessagebox("提示","还没成功获取两个坐标值"))
        {
            return;
        }
    }
    else{
        int num_threads =mainWindow->num_threads;
        std::vector<std::thread> threads;
        std::mutex mtx; // 用于线程同步

        // 分段处理图像数据
        std::vector<size_t> segmentStarts;
        size_t segmentSize = newValue.bmpInfo.GetHeight() / num_threads;
        for (int i = 0; i < num_threads; i++) {
            size_t start = i * segmentSize;
            size_t end = (i == num_threads - 1) ? newValue.bmpInfo.GetHeight() : start + segmentSize;
            segmentStarts.push_back(start);
            threads.emplace_back([&, start, end] {  // 使用 [&] 捕获列表
                Eye(imageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), firstX, firstY, returnRange.value, returnNum.value, start, end);
                Eye(imageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), secondX, secondY, returnRange.value, returnNum.value, start, end);
                std::lock_guard<std::mutex> lock(mtx);
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }
        ShowImage(imageData);

    }
}




void eye::on_btn_apply_clicked()
{
    qDebug()<<"apply";
    mainWindow->ShowImage(imageData,newValue,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight());
}

