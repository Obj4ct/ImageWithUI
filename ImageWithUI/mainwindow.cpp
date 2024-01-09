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
    myValue = MYFunction::ReadBMPFile(BMPPath);
    // 将图像数据转换为QImage
    //BGR排序
    m_bmpImage = QImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
    imageData=myValue.imageData;
    // 进行垂直翻转
    m_bmpImage = m_bmpImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);
    //ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}



void MainWindow::on_btn_gray_clicked()
{
    // 切换按钮状态
    if (ui->btn_gray->text() == "转为灰度图") {
        // 转换为灰度图
        function.ConvertToGray(imageData);

        // 创建 QImage
        int width = myValue.bmpInfo.GetWidth();
        int height = myValue.bmpInfo.GetHeight();
        QImage grayImage(imageData.data(), width, height, QImage::Format_BGR888);

        // 进行垂直翻转
        grayImage = grayImage.mirrored(false, true);

        // 显示灰度图像在imageLabel上
        QPixmap grayPixmap = QPixmap::fromImage(grayImage);
        ui->imageLabel->setPixmap(grayPixmap);

        // 更新按钮文本
        ui->btn_gray->setText("取消转换");
    } else {
        // 恢复原始图像
        QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
        originalImage = originalImage.mirrored(false, true);
        QPixmap originalPixmap = QPixmap::fromImage(originalImage);
        ui->imageLabel->setPixmap(originalPixmap);

        // 更新按钮文本
        ui->btn_gray->setText("转为灰度图");
    }
}


