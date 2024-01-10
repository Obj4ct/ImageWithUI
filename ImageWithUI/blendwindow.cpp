#include "blendwindow.h"
#include "ui_blendwindow.h"
BlendWindow::BlendWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlendWindow)
{
    ui->setupUi(this);
}

BlendWindow::~BlendWindow()
{
    delete ui;
}

void BlendWindow::on_btn_open_1_clicked()
{
    // 文件默认路径
    QString defaultPath = QDir::currentPath();
    // 设置过滤
    QString filter = "BMP文件(*.BMP)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    else{
        qDebug()<<"file path is :"<<path;
        // 读取BMP文件并且存入变量中
        std::string BMPPath = path.toStdString();
        myValue_1 = MYFunction::ReadBMPFile(BMPPath);
        //input debug info
        ImgInfo(myValue_1.bmp,myValue_1.bmpInfo);
        // 将图像数据转换为QImage
        //BGR排序
        bmpImage_1 = QImage(myValue_1.imageData.data(), myValue_1.bmpInfo.GetWidth(), myValue_1.bmpInfo.GetHeight(),QImage::Format_BGR888);
        imageData_1=myValue_1.imageData;
        // 进行垂直翻转
        bmpImage_1 = bmpImage_1.mirrored(false, true);
        // 显示图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(bmpImage_1);
        ui->imageLabel_1->setPixmap(pixmap);
        ui->imageLabel_1->setScaledContents(true); // 使图像适应 label 大小
    }
}
void BlendWindow::on_btn_open_2_clicked()
{
    // 文件默认路径
    QString defaultPath = QDir::currentPath();
    // 设置过滤
    QString filter = "BMP文件(*.BMP)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    else{
        // 读取BMP文件并且存入变量中
        std::string BMPPath = path.toStdString();
        myValue_2 = MYFunction::ReadBMPFile(BMPPath);
        //input debug info
        ImgInfo(myValue_2.bmp,myValue_2.bmpInfo);
        // 将图像数据转换为QImage
        //BGR排序
        bmpImage_2 = QImage(myValue_2.imageData.data(), myValue_2.bmpInfo.GetWidth(), myValue_2.bmpInfo.GetHeight(),QImage::Format_BGR888);
        imageData_2=myValue_2.imageData;
        // 进行垂直翻转
        bmpImage_2 = bmpImage_2.mirrored(false, true);
        // 显示图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(bmpImage_2);
        ui->imageLabel_2->setPixmap(pixmap);
        ui->imageLabel_2->setScaledContents(true); // 使图像适应 label 大小
    }
}

