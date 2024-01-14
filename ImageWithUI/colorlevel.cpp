#include "colorlevel.h"
#include "ui_colorlevel.h"

ColorLevel::ColorLevel(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::ColorLevel), mainWindow(mainWindow), myValue(myValue)
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
    // 设置滑块的最小值和最大值
    ui->horizontalSlider_R->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_R->setMaximum(10);

    // 设置滑块的最小值和最大值
    ui->horizontalSlider_G->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_G->setMaximum(10);

    // 设置滑块的最小值和最大值
    ui->horizontalSlider_B->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_B->setMaximum(10);

    // 设置滑块的最小值和最大值
    ui->horizontalSlider_RGB->setMinimum(0);  // 设置最小值
    ui->horizontalSlider_RGB->setMaximum(10);
    currentR=0;

}

ColorLevel::~ColorLevel()
{
    delete ui;
}

void ColorLevel::ShowImage(std::vector<uint8_t> &imageData)
{
    QImage image(imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}
void ColorLevel::ShowImage()
{
    QImage image(imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在 imageLabel 上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}


void ColorLevel::ColorLevelChanel_R(std::vector<uint8_t> &rImageData, int32_t width, int32_t height, double_t brightness, double_t contrast)
{

    for (int i = 0; i < rImageData.size(); i += 3) {
        uint8_t r = rImageData[i];

        // brightness
        r = std::min(255, std::max(0, static_cast<int>(r + brightness)));
        // contrast
        //128：midGray
        r = std::min(255, std::max(0, static_cast<int>((r - 128) * contrast + 128)));
        rImageData[i] = r;
    }
}


void ColorLevel::ColorLevelChanel_G(std::vector<uint8_t> &gImageData, int32_t width, int32_t height, double_t brightness,
                        double_t contrast) {

    for (int i = 0; i < gImageData.size(); i += 3) {
        uint8_t G = gImageData[i + 1];

        // brightness
        G = std::min(255, std::max(0, static_cast<int>(G + brightness)));
        // contrast
        //128：midGray
        G = std::min(255, std::max(0, static_cast<int>((G - 128) * contrast + 128)));
        gImageData[i + 1] = G;
    }


}

void ColorLevel::ColorLevelChanel_B(std::vector<uint8_t> &bImageData, int32_t width, int32_t height, double_t brightness,
                        double_t contrast) {

    for (int i = 0; i < bImageData.size(); i += 3) {
        uint8_t B = bImageData[i + 2];

        // brightness
        B = std::min(255, std::max(0, static_cast<int>(B + brightness)));
        // contrast
        //128：midGray
        B = std::min(255, std::max(0, static_cast<int>((B - 128) * contrast + 128)));
        bImageData[i + 2] = B;
    }


}

void ColorLevel::ColorLevelChanel_RGB(std::vector<uint8_t> &rgbImageData, int32_t width, int32_t height, double_t brightness,
                          double_t contrast) {
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            int index = (i * width + j) * 3;
            uint8_t r = rgbImageData[index];
            uint8_t g = rgbImageData[index + 1];
            uint8_t b = rgbImageData[index + 2];
            //brightness
            r = std::min(255, std::max(0, static_cast<int>(r + brightness)));
            g = std::min(255, std::max(0, static_cast<int>(g + brightness)));
            b = std::min(255, std::max(0, static_cast<int>(b + brightness)));
            //contrast
            r = std::min(255, std::max(0, static_cast<int>((r - 128) * contrast + 128)));
            g = std::min(255, std::max(0, static_cast<int>((g - 128) * contrast + 128)));
            b = std::min(255, std::max(0, static_cast<int>((b - 128) * contrast + 128)));

            // update
            rgbImageData[index] = r;
            rgbImageData[index + 1] = g;
            rgbImageData[index + 2] = b;
        }
    }
}


void ColorLevel::on_horizontalSlider_R_valueChanged(int value)
{
    rImageData=imageData;
    currentR=static_cast<double>(value)/ui->horizontalSlider_R->maximum();

    brightness=currentR;
    contrast=currentR;
    qDebug()<<"current R is "<<currentR;
    qDebug()<<"current brightness is "<<currentR;
    qDebug()<<"current contrast is "<<currentR;
    ColorLevelChanel_R(rImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),brightness,contrast);

    ShowImage(rImageData);
}

void ColorLevel::on_horizontalSlider_G_valueChanged(int value)
{
    gImageData=imageData;
    currentG=static_cast<double>(value)/ui->horizontalSlider_G->maximum();
    brightness=currentG;
    contrast=currentG;
    qDebug()<<"current G is "<<currentG;
    qDebug()<<"current brightness is "<<currentG;
    qDebug()<<"current contrast is "<<currentG;

    ColorLevelChanel_G(gImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),brightness,contrast);

    ShowImage(gImageData);
}


void ColorLevel::on_horizontalSlider_B_valueChanged(int value)
{
    bImageData=imageData;
    currentB=static_cast<double>(value)/ui->horizontalSlider_B->maximum();
    brightness=currentB;
    contrast=currentB;
    qDebug()<<"current B is "<<currentB;
    qDebug()<<"current brightness is "<<currentB;
    qDebug()<<"current contrast is "<<currentB;

    ColorLevelChanel_G(bImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),brightness,contrast);

    ShowImage(bImageData);
}


void ColorLevel::on_horizontalSlider_RGB_valueChanged(int value)
{
    rgbImageData=imageData;
    currentRGB=static_cast<double>(value)/ui->horizontalSlider_RGB->maximum();
    brightness=currentRGB;
    contrast=currentRGB;
    qDebug()<<"current RGB is "<<currentRGB;
    qDebug()<<"current brightness is "<<currentRGB;
    qDebug()<<"current contrast is "<<currentRGB;

    ColorLevelChanel_G(rgbImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),brightness,contrast);

    ShowImage(rgbImageData);
}
\
