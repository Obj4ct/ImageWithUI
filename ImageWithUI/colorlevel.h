// ColorLevel.h
#ifndef COLORLEVEL_H
#define COLORLEVEL_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QImage>

namespace Ui {
class ColorLevel;
}
class ColorLevel : public QWidget {
    Q_OBJECT

public:
    explicit ColorLevel( QWidget *parent = nullptr);
    ~ColorLevel();
private slots:
    void on_horizontalSlider_r_valueChanged(int value);

    void on_horizontalSlider_g_valueChanged(int value);

    void on_horizontalSlider_b_valueChanged(int value);

    void on_horizontalSlider_rgb_valueChanged(int value);

private:
    Ui::ColorLevel *ui;
    QImage m_image,m_tempImage;
    uint32_t m_width;
    uint32_t m_height;
    QColor m_color;
    // 用于跟踪状态
    int m_initialState;
    int m_initialRedValue;
    int m_initialGreenValue;
    int m_initialBlueValue;
    int m_initialRGBValue;

    //old
    QSlider *sliderR;
    QSlider *sliderG;
    QSlider *sliderB;
    QSlider *sliderRGB;
    QLabel *imageLabel;

    QImage originalImage;
    QImage adjustedImage;

    double brightnessR;
    double contrastR;
    double brightnessG;
    double contrastG;
    double brightnessB;
    double contrastB;
    double brightnessRGB;
    double contrastRGB;

    void GetImageInfo();
    void UpdateImageByRedChannel(int redIncrement);
    void UpdateImageByGreenChannel();
    void UpdateImageByBlueChannel();
    void UpdateImageByRGBChannel();
    void UpdateImage();
};

#endif // COLORLEVEL_H
