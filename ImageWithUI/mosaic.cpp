#include "mosaic.h"
#include "ui_mosaic.h"

Mosaic::Mosaic(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Mosaic), mainWindow(mainWindow), myValue(myValue)
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

Mosaic::~Mosaic()
{
    delete ui;
}

void Mosaic::on_btn_area_ok_clicked()
{
    QString inputText= ui->lineEdit_area_mosaic->text(); // 获取输入的文本
    bool isNumeric;
    double_t value = inputText.toDouble(&isNumeric);
    if(ui->label_firstPos->text()==nullptr||ui->label_secondPos->text()==nullptr)
    {
        if(!function.CreateMessagebox("提示","还没成功获取两个坐标值"))
        {
            return;
        }
    }else if(!isNumeric){
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "请输入数字";
        myBox->setWindowTitle("提示");
        myBox->setText(str);
        myBox->addButton(okBtn, QMessageBox::AcceptRole);
        myBox->show();
        myBox->exec();//阻塞等待用户输入

        if (myBox->clickedButton() == okBtn)
        {
            return;
        }
    }
    else {
        qDebug()<<"click ok";
        AreaMosaic(newValue.imageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),firstX,firstY,secondX,secondY,value);
        ShowImage(newValue.imageData);

    }
}


void Mosaic::on_btn_full_ok_clicked()
{
    QString inputText= ui->lineEdit_full_mosaic->text(); // 获取输入的文本
    bool isNumeric;
    double_t value = inputText.toDouble(&isNumeric);
    if(!isNumeric){
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "请输入数字";
        myBox->setWindowTitle("提示");
        myBox->setText(str);
        myBox->addButton(okBtn, QMessageBox::AcceptRole);
        myBox->show();
        myBox->exec();//阻塞等待用户输入

        if (myBox->clickedButton() == okBtn)
        {
            return;
        }
    }
    else {
        qDebug()<<"click ok";
        FullMosaic(newValue.imageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),value);
        ShowImage(newValue.imageData);

    }
}


void Mosaic::on_btn_reset_clicked()
{
    ResetImage();
}


void Mosaic::on_btn_save_clicked()
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

void Mosaic::mousePressEvent(QMouseEvent *event)
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
        ui->label_firstPos->setText(str);
    } else if (clickCount == 1) {
        // 第二次点击，保存坐标到secondClick
        secondClick = clickPos;
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        secondX=secondClick.x();
        secondY=secondClick.y();
        QString str = QString("%1, %2").arg(secondX).arg(secondY);
        ui->label_secondPos->setText(str);

    }


}

void Mosaic::ShowImage(std::vector<uint8_t> &inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void Mosaic::ResetImage()
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
    ui->label_firstPos->setText(str1);

    QString str2 = QString("%1, %2").arg(secondX).arg(secondY);
    ui->label_secondPos->setText(str2);
}

void Mosaic::FullMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t degree)
{
    // foreach pix 加上步长应该是移动像素方便处理下一个像素
    for (uint32_t y = 0; y < height; y += degree) {
        for (uint32_t x = 0; x < width; x += degree) {
            uint32_t totalR = 0;
            uint32_t totalG = 0;
            uint32_t totalB = 0;
            int mosaic = 0;
            // avg
            for (uint32_t dy = 0; dy < degree && y + dy < height; dy++) {
                for (uint32_t dx = 0; dx < degree && x + dx < width; dx++) {
                    uint32_t index = ((y + dy) * width + (x + dx)) * 3;
                    totalR += imageData[index];
                    totalG += imageData[index + 1];
                    totalB += imageData[index + 2];
                    mosaic++;
                }
            }
            auto averageR = totalR / mosaic;
            auto averageG = totalG / mosaic;
            auto averageB = totalB / mosaic;
            // set each pix


            for (uint32_t dy = 0; dy < degree && y + dy < height; dy++) {
                for (uint32_t dx = 0; dx < degree && x + dx < width; dx++) {
                    uint32_t index = ((y + dy) * width + (x + dx)) * 3;
                    imageData[index] = averageR;
                    imageData[index + 1] = averageG;
                    imageData[index + 2] = averageB;
                }
            }
        }
    }
}

void Mosaic::AreaMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t beginX, uint32_t beginY, uint32_t endX, uint32_t endY, uint32_t degree)
{
    if (endX > width || endY > height) {
        std::cout << "error! Mosaic area exceeds image boundaries" << std::endl;
        return;
    }

    for (uint32_t y = beginY; y < endY; y += degree) {
        for (uint32_t x = beginX; x < endX; x += degree) {
            int totalR = 0;
            int totalG = 0;
            int totalB = 0;
            int pixelCount = 0;

            for (int dy = 0; dy < degree && y + dy < height; dy++) {
                for (int dx = 0; dx < degree && x + dx < width; dx++) {
                    uint32_t pixelIndex = ((y + dy) * width + (x + dx)) * 3;
                    totalR += imageData[pixelIndex];
                    totalG += imageData[pixelIndex + 1];
                    totalB += imageData[pixelIndex + 2];
                    pixelCount++;
                }
            }

            int averageR = totalR / pixelCount;
            int averageG = totalG / pixelCount;
            int averageB = totalB / pixelCount;

            for (int dy = 0; dy < degree && y + dy < height; dy++) {
                for (int dx = 0; dx < degree && x + dx < width; dx++) {
                    uint32_t pixelIndex = ((y + dy) * width + (x + dx)) * 3;
                    imageData[pixelIndex] = averageR;
                    imageData[pixelIndex + 1] = averageG;
                    imageData[pixelIndex + 2] = averageB;
                }
            }
        }
    }

}



