#include "tailor.h"
#include "ui_tailor.h"

Tailor::Tailor(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Tailor), mainWindow(mainWindow), myValue(myValue)
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


void Tailor::mousePressEvent(QMouseEvent* event)
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
//void Tailor::TailorImg(int32_t startX, int32_t startY, int32_t endX, int32_t endY,
//                       std::vector<uint8_t> &imageData, BMPInfo &newBmpInfo, BMP &bmp, uint32_t originWidth) {

//    int32_t cropX = startX;
//    int32_t cropY = startY;
//    int32_t cropWidth = endX - startX;
//    int32_t cropHeight = endY - startY;

//    bmpInfo.WriteToBMPInfo(imageData, cropHeight, cropWidth, newBmpInfo, bmp);

//    int32_t imageWidth = newValue.bmpInfo.GetWidth();
//    int32_t imageHeight = newValue.bmpInfo.GetHeight();

//    for (int y = cropY; y < cropY + cropHeight && y < imageHeight; y++) {
//        for (int x = cropX; x < cropX + cropWidth && x < imageWidth; x++) {
//            uint32_t originIndex = (y * originWidth + x) * 3;
//            int index = ((y - cropY) * cropWidth + (x - cropX)) * 3;

//            // 检查索引是否在有效范围内
//            if (originIndex < imageData.size() && index < imageData.size()) {
//                imageData[index] = imageData[originIndex];     // Red
//                imageData[index + 1] = imageData[originIndex + 1]; // Green
//                imageData[index + 2] = imageData[originIndex + 2]; // Blue
//            } else {
//                // 处理索引越界的情况，例如输出错误信息或采取其他适当的措施
//                qDebug() << "Index out of range!";
//            }
//        }
//    }
//}

void Tailor::TailorImg(int32_t startX, int32_t startY, int32_t endX, int32_t endY,
                       std::vector<uint8_t> &imageData, BMPInfo &newBmpInfo, BMP &bmp, uint32_t originWidth) {

    int32_t cropX = startX;
    int32_t cropY = startY;
    int32_t cropWidth = endX - startX;
    int32_t cropHeight = endY - startY;

    bmpInfo.WriteToBMPInfo(imageData, cropHeight, cropWidth, newBmpInfo, bmp);

    int32_t imageWidth = newValue.bmpInfo.GetWidth();
    int32_t imageHeight = newValue.bmpInfo.GetHeight();

    for (int y = cropY; y < cropY + cropHeight && y < imageHeight; y++) {
        for (int x = cropX; x < cropX + cropWidth && x < imageWidth; x++) {
            double x_ratio = (double)(x - cropX) / cropWidth;
            double y_ratio = (double)(y - cropY) / cropHeight;

            int x_origin = x_ratio * (endX - startX) + startX;
            int y_origin = y_ratio * (endY - startY) + startY;

            uint32_t originIndex = (y_origin * originWidth + x_origin) * 3;
            int index = ((y - cropY) * cropWidth + (x - cropX)) * 3;

            // 检查索引是否在有效范围内
            if (originIndex < imageData.size() && index < imageData.size()) {
                imageData[index] = imageData[originIndex];     // Red
                imageData[index + 1] = imageData[originIndex + 1]; // Green
                imageData[index + 2] = imageData[originIndex + 2]; // Blue
            } else {
                // 处理索引越界的情况，例如输出错误信息或采取其他适当的措施
                qDebug() << "Index out of range!";
            }
        }
    }
}

void Tailor::ShowImage(std::vector<uint8_t> &inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void Tailor::ResetImage()
{
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



Tailor::~Tailor()
{
    delete ui;
}



void Tailor::on_btn_ok_clicked()
{
    int32_t startX = firstX;
    int32_t startY = firstY;
    int32_t endX = secondX;
    int32_t endY = secondY;
    TailorImg(startX, startY, endX, endY, imageData,newValue.bmpInfo,newValue.bmp, newValue.bmpInfo.GetWidth());
    ShowImage(imageData);
}



void Tailor::on_btn_save_clicked()
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


void Tailor::on_btn_reset_clicked()
{
    ResetImage();
}

