// MainWindow.cpp
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include"Debug.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::on_openImage_triggered()
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

    // 读取BMP文件并且存入变量中
    std::string BMPPath = path.toStdString();
    MyValue myValue = MYFunction::ReadBMPFile(BMPPath);

    //debug first
    ImgInfo(myValue.bmp,myValue.bmpInfo);
    // 将图像数据转换为QImage
    //BGR排序
    m_bmpImage = QImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);

    // 进行垂直翻转
    m_bmpImage = m_bmpImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);
    //ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}


