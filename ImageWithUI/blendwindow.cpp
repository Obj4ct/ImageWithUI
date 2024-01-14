#include "blendwindow.h"
#include "ui_blendwindow.h"



BlendWindow::BlendWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlendWindow) {
    ui->setupUi(this);
    ui->widget_select_blendMode->setVisible(false);

    ui->widget_select_alpha->setVisible(false);

    // 设置滑块的最小值和最大值
    ui->horizontalSlider_alpha->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_alpha->setMaximum(10);  // 设置最大值

    currentAlpha = 0;
    // 设置下拉框的初始状态
    ui->comboBox_blendMode->addItem("<请选择>");
    ui->comboBox_blendMode->addItem("正常");
    ui->comboBox_blendMode->addItem("正片叠底");
    ui->comboBox_blendMode->addItem("滤色");
    ui->comboBox_blendMode->addItem("叠加");

}
int BlendWindow::selectCount=0;
BlendWindow::~BlendWindow() {
    delete ui;
}

void BlendWindow::on_btn_open_1_clicked() {
    QString defaultPath = QDir::currentPath();
    QString filter = "BMP文件(*.BMP)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }

    std::string BMPPath = path.toStdString();
    myValue_1 = MYFunction::ReadBMPFile(BMPPath);

    originalImageData_1 = myValue_1.imageData;

    bmpImage_1 = QImage(originalImageData_1.data(), myValue_1.bmpInfo.GetWidth(),
                        myValue_1.bmpInfo.GetHeight(), QImage::Format_BGR888);

    bmpImage_1 = bmpImage_1.mirrored(false, true);

    QPixmap pixmap = QPixmap::fromImage(bmpImage_1);
    ui->imageLabel_1->setPixmap(pixmap);
    ui->imageLabel_1->setScaledContents(true);
    selectCount++;
    qDebug()<<selectCount;
    if(selectCount==2)
    {
        ui->widget_select_blendMode->setVisible(true);
    }
}

void BlendWindow::on_btn_open_2_clicked() {
    QString defaultPath = QDir::currentPath();
    QString filter = "BMP文件(*.BMP)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }

    std::string BMPPath = path.toStdString();
    myValue_2 = MYFunction::ReadBMPFile(BMPPath);

    originalImageData_2 = myValue_2.imageData;
    bmpImage_2 = QImage(originalImageData_2.data(), myValue_2.bmpInfo.GetWidth(),
                        myValue_2.bmpInfo.GetHeight(), QImage::Format_BGR888);

    bmpImage_2 = bmpImage_2.mirrored(false, true);

    QPixmap pixmap = QPixmap::fromImage(bmpImage_2);
    ui->imageLabel_2->setPixmap(pixmap);
    ui->imageLabel_2->setScaledContents(true);
    selectCount++;
    qDebug()<<selectCount;
    if(selectCount==2)
    {
        ui->widget_select_blendMode->setVisible(true);
    }
}

void BlendWindow::BlendImages()
{
    int32_t width = myValue_1.bmpInfo.GetWidth();
    int32_t height = myValue_1.bmpInfo.GetHeight();
    BlendMode blendMode = currentBlendMode;
    originalImageData_1=myValue_1.imageData;
    originalImageData_2=myValue_2.imageData;
    Effect(originalImageData_1, originalImageData_2, width, height, blendMode);


    //out image for debug
    //    MYFunction::WriteBMPFile("outputBlendedImage.bmp", originalImageData_1, myValue_1.bmp,
    //                             myValue_1.bmpInfo);


    //store to blendedimage

    ShowImage();
}

void BlendWindow::ShowImage()
{
    blendedImage = QImage(originalImageData_1.data(), myValue_1.bmpInfo.GetWidth(), myValue_1.bmpInfo.GetHeight(),QImage::Format_BGR888);
    blendedImageData=myValue_1.imageData;
    // 进行垂直翻转
    blendedImage = blendedImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(blendedImage);
    ui->blendImageLabel->setPixmap(pixmap);
    ui->blendImageLabel->setScaledContents(true); // 使图像适应 label 大小

}

void BlendWindow::SwitchBlendMode(uint8_t &destR, uint8_t &destG, uint8_t &destB, uint8_t srcR, uint8_t srcG, uint8_t srcB,
                                  BlendMode blendMode,double alpha) {

    //originalImageData_1=myValue_1.imageData;
    switch (blendMode) {
    // 正常模式 在“正常”模式下，“混合色”的显示与不透明度的设置有关。
    //线性插值
    case BLEND_NORMAL: {

        destR = static_cast<uint8_t>((1.0 - alpha) * destR + alpha * srcR);
        destG = static_cast<uint8_t>((1.0 - alpha) * destG + alpha * srcG);
        destB = static_cast<uint8_t>((1.0 - alpha) * destB + alpha * srcB);
        break;
    }

        // 正片叠底 其主要特点是以正片作为底图，再将其他图像叠加于其上，并根据叠加图像的颜色来调整底图的 亮度 和 饱和度，产生丰富的合成效果。
        // 这种混合模式的效果是将源像素的颜色应用于目标像素，具有"遮罩"效果，通常用于创建阴影或混合颜色的效果。
        //   相乘:这种混合模式的核心概念是模拟颜色的遮罩效应或阴影效果。这种效果是通过将源像素的颜色与目标像素的颜色相乘来实现的。
        //  255 需要保证像素的范围
    case BLEND_MULTIPLY: {
        destR = static_cast<uint8_t>((destR * srcR) / 255);
        destG = static_cast<uint8_t>((destG * srcG) / 255);
        destB = static_cast<uint8_t>((destB * srcB) / 255);
        break;
    }
        // 滤色 ps中滤色的作用结果和正片叠底刚好相反，它是将 两个颜色的互补色 的像素值相乘，然后除以255得到的最终色的像素值。通常执行滤色模式后的颜色都较浅。
    case BLEND_SCREEN: {
        destR = static_cast<uint8_t>((255 - destR) * (255 - srcR) / 255);
        destG = static_cast<uint8_t>((255 - destG) * (255 - srcG) / 255);
        destB = static_cast<uint8_t>((255 - destB) * (255 - srcB) / 255);
        break;
    }
        //“叠加”模式把图像的“基色”颜色与“混合色”颜色相混合产生一种中间色。
        //根据目标像素的亮度选择源像素的亮度值。如果目标像素较暗（R、G、B < 128）,则使用 2 * dest * src / 255 计算；
        // 如果目标像素较亮（R、G、B >= 128），则使用 255 - (2 * (255 - dest) * (255 - src)) / 255 计算
    case BLEND_OVERLAY: {
        if (destR < 128) {
            destR = static_cast<uint8_t>((2 * destR * srcR) / 255);
        } else {
            destR = static_cast<uint8_t>(255 - (2 * (255 - destR) * (255 - srcR)) / 255);
        }

        if (destG < 128) {
            destG = static_cast<uint8_t>((2 * destG * srcG) / 255);
        } else {
            destG = static_cast<uint8_t>(255 - (2 * (255 - destG) * (255 - srcG)) / 255);
        }

        if (destB < 128) {
            destB = static_cast<uint8_t>((2 * destB * srcB) / 255);
        } else {
            destB = static_cast<uint8_t>(255 - (2 * (255 - destB) * (255 - srcB)) / 255);
        }
        break;
    }

    default:
        break;
    }

}

void BlendWindow::Effect(std::vector<uint8_t> &imageData, const std::vector<uint8_t> &effectData, int width, int height,
                         BlendMode blendMode) {
    double alpha=currentAlpha;
    if (imageData.size() != effectData.size()) {
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "不支持不同大小的图片混合!!";
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


    for (size_t i = 0; i < imageData.size(); i += 3) {

        //获取原图RGB
        uint8_t &destR = imageData[i];
        uint8_t &destG = imageData[i + 1];
        uint8_t &destB = imageData[i + 2];
        //获取背景RGB
        uint8_t srcR = effectData[i];
        uint8_t srcG = effectData[i + 1];
        uint8_t srcB = effectData[i + 2];
        SwitchBlendMode(destR, destG, destB, srcR, srcG, srcB, blendMode,alpha);
    }
}


void BlendWindow::on_comboBox_blendMode_currentIndexChanged(int index)
{

    if (index == 0) {
        qDebug()<<"current index is 0";
    } else if (index == 1) {
        // "正常"
        ui->widget_select_alpha->setVisible(true);
        currentBlendMode = BLEND_NORMAL;
        BlendImages();
    } else if (index == 2) {
        // "正片叠底"
        ui->widget_select_alpha->setVisible(false);
        currentBlendMode = BLEND_MULTIPLY;
        BlendImages();
    } else if (index == 3) {
        // "滤色"
        ui->widget_select_alpha->setVisible(false);
        currentBlendMode = BLEND_SCREEN;
        BlendImages();
    } else if (index == 4) {
        // "叠加"
        ui->widget_select_alpha->setVisible(false);
        currentBlendMode = BLEND_OVERLAY;
        BlendImages();
    }
}
void BlendWindow::on_horizontalSlider_alpha_valueChanged(int value)
{
    currentAlpha = static_cast<double>(value) / ui->horizontalSlider_alpha->maximum();
    qDebug() << "current alpha is " << currentAlpha;
    BlendImages();
}



