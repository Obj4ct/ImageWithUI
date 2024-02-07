#include "eye.h"
#include "ui_eye.h"

eye::eye(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::eye), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    newValue=myValue;
    m_bmpImage = QImage(newValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
    imageData=newValue.imageData;
    // 进行垂直翻转
    m_bmpImage = m_bmpImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);
    //ui->imageLabel->setScaledContents(true);
    Function function;

}

void eye::mousePressEvent(QMouseEvent* event)
{
    QPoint clickPos = event->pos();
    if (clickCount == 0) {

        // 第一次点击，保存坐标到firstClick
        firstClick = clickPos;
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "First click at: " << firstClick;
        clickCount++;
        firstX=firstClick.x();
        firstY=firstClick.y();
        QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_first->setText(str);
    } else if (clickCount == 1) {
        // 第二次点击，保存坐标到secondClick
        secondClick = clickPos;
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

                int x0 = static_cast<int>(newX);
                int y0 = static_cast<int>(newY);
                int x1 = std::min(x0 + 1, width - 1);
                int y1 = std::min(y0 + 1, height - 1);
                double dx = newX - x0;
                double dy = newY - y0;

                int targetPixelIndex00 = (y0 * width + x0) * 3;
                int targetPixelIndex01 = (y0 * width + x1) * 3;
                int targetPixelIndex10 = (y1 * width + x0) * 3;
                int targetPixelIndex11 = (y1 * width + x1) * 3;

                for (int c = 0; c < 3; c++) {
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
    //ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void eye::ResetImage()
{
    newValue.imageData=myValue.imageData;
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);
    firstX=0;
    firstY=0;
    secondX=0;
    secondY=0;
    QString str1 = QString("%1, %2").arg(firstX).arg(firstY);
    ui->label_first->setText(str1);

    QString str2 = QString("%1, %2").arg(secondX).arg(secondY);
    ui->label_second->setText(str2);

}
eye::~eye()
{
    delete ui;
}

void eye::on_btn_save_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(nullptr, "保存文件", "", "BMP文件(*.bmp)");
    if (filePath.isEmpty()) {
        qDebug() << "Save operation canceled.";
        return;
    }
    savePath=filePath.toStdString();

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    std::string str=fileName.toStdString();
    std::cout<<"filename is "<<str<<std::endl;
    MYFunction::WriteBMPFile(str,newValue.imageData,newValue.bmp,newValue.bmpInfo);
    qDebug()<<"succeed!";

}




void eye::on_btn_ok_clicked()
{
    qDebug()<<"click ok";
    if(ui->label_first->text()==nullptr||ui->label_second->text()==nullptr||firstX==0||firstY==0||secondX==0||secondY==0)
    {
        if(!function.CreateMessagebox("提示","还没成功获取两个坐标值"))
        {
            return;
        }
    }
    else{
        int num_threads = 4;
        std::vector<std::thread> threads;
        std::mutex mtx; // 用于线程同步

        // 分段处理图像数据
        std::vector<size_t> segmentStarts;
        size_t segmentSize = myValue.bmpInfo.GetHeight() / num_threads;
        for (int i = 0; i < num_threads; i++) {
            size_t start = i * segmentSize;
            size_t end = (i == num_threads - 1) ? myValue.bmpInfo.GetHeight() : start + segmentSize;
            segmentStarts.push_back(start);
            threads.emplace_back([&, start, end] {  // 使用 [&] 捕获列表
                Eye(newValue.imageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), firstX, firstY, eyeRadius, warpIntensity, start, end);
                Eye(newValue.imageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), secondX, secondY, eyeRadius, warpIntensity, start, end);
                std::lock_guard<std::mutex> lock(mtx);
            });
        }

        for (auto &thread : threads) {
            thread.join();
        }
        ShowImage(newValue.imageData);
    }
}


void eye::on_btn_ok_2_clicked()
{
    ResetImage();
}

