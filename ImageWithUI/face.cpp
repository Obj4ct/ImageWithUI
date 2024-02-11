#include "mainwindow.h"
#include "face.h"
#include "ui_face.h"



Face::Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Face), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);

    setWindowTitle(QString("图像微调"));
    setWindowIcon(QIcon(":/icon/logo.png"));
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
void Face::mousePressEvent(QMouseEvent* event)
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
        faceCenterX=firstClick.x();
        faceCenterY=firstClick.y();
        QString str = QString("%1, %2").arg(faceCenterX).arg(faceCenterY);
        ui->label_first->setText(str);
    } else if (clickCount == 1) {
        // 第二次点击，保存坐标到secondClick
        secondClick = QPoint(imageX, imageY);
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        faceCenterX=secondClick.x();
        faceCenterY=secondClick.y();
        QString str = QString("%1, %2").arg(faceCenterX).arg(faceCenterY);
        ui->label_first->setText(str);

    }


}
Face::~Face()
{
    delete ui;
}

void Face::on_btn_ok_clicked()
{
    imageData=mainWindow->imageData;
    QString inputText= ui->lineEdit_radius->text(); // 获取输入的文本
    bool isNumeric;
    double_t value = inputText.toDouble(&isNumeric);
    if(ui->label_first->text()==nullptr||faceCenterX==0||faceCenterY==0)
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
        std::thread faceThread(std::bind(&Face::_Face, this, std::ref(imageData), newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), faceCenterX, faceCenterY, value, warpIntensity));
        faceThread.join();

        //_Face(newValue.imageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),faceCenterX,faceCenterY,value,warpIntensity);
        ShowImage(imageData);
        mainWindow->SaveImageDataToHistory(imageData);

    }

}

void Face::on_btn_save_clicked()
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



void Face::ShowImage(std::vector<uint8_t> &inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

}

void Face::_Face(std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t centerX, int32_t centerY, int32_t radius, double intensity)
{
    // 遍历图像像素
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixelIndex = (y * width + x) * 3;

            // 计算像素到中心点的距离
            double distance = std::sqrt(static_cast<double>((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY)));

            if (distance < radius) {

                double warpAmount = intensity * std::sin(distance / radius * 3.14159265);


                int newX = static_cast<int>(x + warpAmount);
                int newY = static_cast<int>(y + warpAmount);

                newX = std::max(0, std::min(width - 1, newX));
                newY = std::max(0, std::min(height - 1, newY));

                int targetPixelIndex = (newY * width + newX) * 3;

                imageData[pixelIndex] = imageData[targetPixelIndex];
                imageData[pixelIndex + 1] = imageData[targetPixelIndex + 1];
                imageData[pixelIndex + 2] = imageData[targetPixelIndex + 2];
            }
        }
    }
}


void Face::on_btn_apply_clicked()
{
    qDebug()<<"apply";
    mainWindow->ShowImage(imageData ,newValue,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight());
}

