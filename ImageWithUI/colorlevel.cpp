// ColorLevel.cpp
#include "ColorLevel.h"
#include "ui_colorlevel.h"
ColorLevel::ColorLevel(QWidget *parent)
    : QWidget(parent), ui(new Ui::ColorLevel) {

    ui->setupUi(this);
    GetImageInfo();
    m_initialRedValue =0;
    m_initialGreenValue = 0;
    m_initialBlueValue =0;
    m_initialRGBValue = 0;
}
void ColorLevel::GetImageInfo()
{
    //加载图片
    m_image.load("D:\\Desktop\\image\\in\\Lenna.bmp");
    //设置label
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));
    m_width=m_image.width();
    m_height=m_image.height();
}

void ColorLevel::UpdateImage()
{
    // 遍历图片像素
    for (int x = 0; x < m_image.width(); x++)
    {
        for (int y = 0; y < m_image.height(); y++)
        {
            QRgb pixel = m_image.pixel(x, y);
            int red = qRed(pixel);
            int green = qGreen(pixel);
            int blue = qBlue(pixel);

            // 根据初始状态的通道值来更新像素
            red = qBound(0, red + m_initialRedValue, 255);
            green = qBound(0, green + m_initialGreenValue, 255);
            blue = qBound(0, blue + m_initialBlueValue, 255);

            m_image.setPixel(x, y, qRgb(red, green, blue));
        }
    }

    // 显示更新后的图像
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));
}

void ColorLevel::UpdateImageByRedChannel(int redIncrement)
{
    // 遍历图片像素
    for (int x = 0; x < m_image.width(); x++)
    {
        for (int y = 0; y < m_image.height(); y++)
        {
            QRgb pixel = m_image.pixel(x, y);
            int red = qRed(pixel);
            int green = qGreen(pixel);
            int blue = qBlue(pixel);

            // 根据传入的增量值来更新红色通道
            red = qBound(0, red + redIncrement, 255);

            m_image.setPixel(x, y, qRgb(red, green, blue));
        }
    }

    // 显示更新后的图像
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));
}

void ColorLevel::UpdateImageByGreenChannel()
{
    int curValue = ui->horizontalSlider_g->value();

    // 遍历图片像素
    for (int x = 0; x < m_image.width(); x++) {
        for (int y = 0; y < m_image.height(); y++) {
            QRgb pixel = m_image.pixel(x, y);
            int red = qRed(pixel);
            int green = qGreen(pixel);
            int blue = qBlue(pixel);
            green = qBound(0, red + curValue, 255);

            m_image.setPixel(x, y, qRgb(red, green, blue));
        }
    }

    // 显示
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));
}

void ColorLevel::UpdateImageByBlueChannel()
{
    int curValue = ui->horizontalSlider_b->value();

    // 遍历图片像素
    for (int x = 0; x < m_image.width(); x++) {
        for (int y = 0; y < m_image.height(); y++) {
            QRgb pixel = m_image.pixel(x, y);
            int red = qRed(pixel);
            int green = qGreen(pixel);
            int blue = qBlue(pixel);
            blue = qBound(0, red + curValue, 255);
            m_image.setPixel(x, y, qRgb(red, green, blue));
        }
    }

    // 显示
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));

}

void ColorLevel::UpdateImageByRGBChannel()
{

    int curValue = ui->horizontalSlider_rgb->value();
    // 遍历图片像素
    for (int x = 0; x < m_image.width(); x++)
    {
        for (int y = 0; y < m_image.height(); y++)
        {
            QRgb pixel = m_image.pixel(x, y);
            int red = qRed(pixel);
            int green = qGreen(pixel);
            int blue = qBlue(pixel);
            red=qBound(0,red+curValue,255);
            green = qBound(0, green + curValue, 255);
            blue=qBound(0,blue+curValue,255);
            m_image.setPixel(x, y, qRgb(red, green, blue));
        }
    }
    // 显示
    ui->imageLabel->setPixmap(QPixmap::fromImage(m_image));

}


ColorLevel::~ColorLevel()
{
    delete ui;
}




void ColorLevel::on_horizontalSlider_r_valueChanged(int value)
{
    ui->horizontalSlider_r->setRange(0, 255);
    int diff = value - m_initialRedValue;

    if (diff > 0)
    {
        // 向右移动，改变红色通道像素值
        UpdateImageByRedChannel(diff);
    }
    else if (diff < 0)
    {
        // 向左移动，恢复初始状态
        m_initialRedValue = value;
        // 恢复图像
        UpdateImage();
    }
    // 如果 diff 等于 0，什么都不做，因为滑块没有移动
}

void ColorLevel::on_horizontalSlider_g_valueChanged(int value)
{
    ui->horizontalSlider_g->setRange(0,255);
    UpdateImageByGreenChannel();
}


void ColorLevel::on_horizontalSlider_b_valueChanged(int value)
{
    ui->horizontalSlider_b->setRange(0,255);
    UpdateImageByBlueChannel();
}


void ColorLevel::on_horizontalSlider_rgb_valueChanged(int value)
{
    ui->horizontalSlider_rgb->setRange(0,255);
    UpdateImageByRGBChannel();
}




