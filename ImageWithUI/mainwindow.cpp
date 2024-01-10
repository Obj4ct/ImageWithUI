// MainWindow.cpp
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include"Debug.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->widget->setVisible(false);
}

void MainWindow::ResetImage(MyValue &myValue)
{
    qDebug()<<"in";
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);

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
    else{
        ui->widget->setVisible(true);
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
        ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
    }

}



void MainWindow::on_btn_gray_clicked()
{
    // 切换按钮状态
    std::vector<uint8_t> grayImageData=imageData;
    if (ui->btn_gray->text() == "转为灰度图") {
        // 转换为灰度图
        function.ConvertToGray(grayImageData);

        // 创建 QImage
        int width = myValue.bmpInfo.GetWidth();
        int height = myValue.bmpInfo.GetHeight();
        QImage image(grayImageData.data(), width, height, QImage::Format_BGR888);

        // 进行垂直翻转
        image = image.mirrored(false, true);

        // 显示灰度图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(image);
        ui->imageLabel->setPixmap(pixmap);

        // 更新按钮文本
        ui->btn_gray->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_gray->setText("转为灰度图");
    }
}



void MainWindow::on_btn_autoContrast_clicked()
{
    std::vector<uint8_t> contrastImageData=imageData;
    function.AutoContrast(contrastImageData);
    // 切换按钮状态
    if (ui->btn_autoContrast->text() == "自动调整对比度") {
        // 转换为灰度图
        function.AutoContrast(contrastImageData);

        // 创建 QImage
        int width = myValue.bmpInfo.GetWidth();
        int height = myValue.bmpInfo.GetHeight();
        QImage image(contrastImageData.data(), width, height, QImage::Format_BGR888);

        // 进行垂直翻转
        image = image.mirrored(false, true);

        // 显示灰度图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(image);
        ui->imageLabel->setPixmap(pixmap);

        // 更新按钮文本
        ui->btn_autoContrast->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);

        // 更新按钮文本
        ui->btn_autoContrast->setText("自动调整对比度");
    }
}


void MainWindow::on_btn_averBlur_clicked()
{
    std::vector<uint8_t> averBLurImageData=imageData;
    function.AverageBlur(averBLurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    // 切换按钮状态
    if (ui->btn_averBlur->text() == "均值模糊") {
        // 转换为灰度图
        function.AverageBlur(averBLurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        // 创建 QImage
        int width = myValue.bmpInfo.GetWidth();
        int height = myValue.bmpInfo.GetHeight();
        QImage image(averBLurImageData.data(), width, height, QImage::Format_BGR888);

        // 进行垂直翻转
        image = image.mirrored(false, true);

        // 显示灰度图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(image);
        ui->imageLabel->setPixmap(pixmap);

        // 更新按钮文本
        ui->btn_averBlur->setText("取消");
    } else {
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_averBlur->setText("均值模糊");
    }
}


void MainWindow::on_btn_small_biCubic_clicked()
{
    std::vector<uint8_t> biCubicImageData=imageData;
    //判断高和宽是否正确输入数字
    QString inputTextHeight = ui->lineEdit_small_biCubic_height->text(); // 获取输入的文本
    bool heightIsNumeric;
    int32_t heightValue = inputTextHeight.toInt(&heightIsNumeric); // 将文本转换为int32_t

    QString inputTextWidth = ui->lineEdit_small_biCubic_width->text(); // 获取输入的文本
    bool widthIsNumeric;
    int32_t widthValue = inputTextWidth.toInt(&widthIsNumeric); // 将文本转换为int32_t
    //no text in it
    if(ui->lineEdit_small_biCubic_height->text()==nullptr&&ui->lineEdit_small_biCubic_width->text()==nullptr)
    {
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "请输入高和宽";
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
    //not a number
    else if(!heightIsNumeric||!widthIsNumeric){
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
    else{
        std::vector<uint8_t> smallImageData=function.SmallImage_BiCubic(biCubicImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),myValue.bmpInfo.GetWidth()/widthValue,myValue.bmpInfo.GetHeight()/heightValue);
        // 创建 QImage
        QImage image(smallImageData.data(), myValue.bmpInfo.GetWidth()/widthValue, myValue.bmpInfo.GetHeight()/heightValue, QImage::Format_BGR888);

        // 进行垂直翻转
        image = image.mirrored(false, true);
        QPixmap pixmap = QPixmap::fromImage(image);
        ui->imageLabel->setPixmap(pixmap);
        //ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
    }
}

//有bug 有时候会卡住...
void MainWindow::on_btn_large_biCubic_clicked()
{
    std::vector<uint8_t> biCubicImageData=imageData;
    //判断高和宽是否正确输入数字
    QString inputTextHeight = ui->lineEdit_large_biCubic_height->text(); // 获取输入的文本
    bool heightIsNumeric;
    int32_t heightValue = inputTextHeight.toInt(&heightIsNumeric); // 将文本转换为int32_t

    QString inputTextWidth = ui->lineEdit_large_biCubic_width->text(); // 获取输入的文本
    bool widthIsNumeric;
    int32_t widthValue = inputTextWidth.toInt(&widthIsNumeric); // 将文本转换为int32_t
    //no text in it
    if(ui->lineEdit_large_biCubic_height->text()==nullptr&&ui->lineEdit_large_biCubic_width->text()==nullptr)
    {
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "请输入高和宽";
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
    //not a number
    else if(!heightIsNumeric||!widthIsNumeric){
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
    else{
        std::vector<uint8_t> largeImageData=function.LargeImage_BiCubic(biCubicImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),myValue.bmpInfo.GetWidth()*widthValue,myValue.bmpInfo.GetHeight()*heightValue);
        // 创建 QImage
        QImage image(largeImageData.data(), myValue.bmpInfo.GetWidth()*widthValue, myValue.bmpInfo.GetHeight()*heightValue, QImage::Format_BGR888);

        // 进行垂直翻转
        image = image.mirrored(false, true);
        QPixmap pixmap = QPixmap::fromImage(image);
        ui->imageLabel->setPixmap(pixmap);
        //ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
    }

}

void MainWindow::on_btn_resetAll_clicked()
{
    ResetImage(myValue);
}

