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
    ui->imageLabel->setScaledContents(true);
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

void eye::Eye(std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t centerX, int32_t centerY, int32_t radius, double intensity)
{
    // 遍历图像像素
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixelIndex = (y * width + x) * 3;

            // 计算像素到中心点的距离
            double distance = std::sqrt(
                    static_cast<double>((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY)));

            if (distance < radius) {

                double warpAmount = intensity * std::sin(distance / radius * 3.14159265);

                // 使用线性插值计算目标像素坐标
                double newX = x + warpAmount;
                double newY = y + warpAmount;

                // 使用双线性插值获取目标像素的颜色值
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

                // 使用双线性插值计算目标像素的颜色值
                for (int c = 0; c < 3; c++) {
                    double interpolatedValue =
                            imageData[targetPixelIndex00 + c] * (1 - dx) * (1 - dy) +
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
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void eye::ResetImage()
{

    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);

}
eye::~eye()
{
    delete ui;
}

void eye::on_btn_save_clicked()
{

}




void eye::on_btn_ok_clicked()
{
    if(ui->label_first->text()==nullptr||ui->label_second->text()==nullptr)
    {
        if(!function.CreateMessagebox("提示","还没成功获取两个坐标值"));
        {
            return;
        }
    }
    else{
        qDebug()<<"click ok";
        Eye(newValue.imageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),firstX,firstY,eyeRadius,warpIntensity);
        Eye(newValue.imageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),secondX,secondY,eyeRadius,warpIntensity);
        ShowImage(newValue.imageData);
    }
}


void eye::on_btn_ok_2_clicked()
{
    ResetImage();
}

