#ifndef COLORLEVEL_H
#define COLORLEVEL_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
//#include "mainwindow.h"
class MainWindow;
namespace Ui {
class ColorLevel;
}

class ColorLevel : public QWidget
{
    Q_OBJECT

public:
   explicit ColorLevel(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~ColorLevel();
private slots:
    void on_horizontalSlider_R_valueChanged(int value);

    void on_horizontalSlider_G_valueChanged(int value);

    void on_horizontalSlider_B_valueChanged(int value);

    void on_horizontalSlider_RGB_valueChanged(int value);

private:
    Ui::ColorLevel *ui;
    MainWindow* mainWindow;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t>imageData;

    std::vector<uint8_t>rImageData;
    std::vector<uint8_t>gImageData;
    std::vector<uint8_t>bImageData;
    std::vector<uint8_t>rgbImageData;
    QImage m_bmpImage;
    double_t currentR;
    double_t currentG;
    double_t currentB;
    double_t currentRGB;
    double_t brightness;
    double_t contrast;
private:
    void ShowImage(std::vector<uint8_t> &imageData);
    void ShowImage();
    //色阶调整 R通道
    void ColorLevelChanel_R(std::vector<uint8_t> &rImageData, int32_t width, int32_t height, double_t brightness,
                            double_t contrast);
    void ColorLevelChanel_G(std::vector<uint8_t> &gImageData, int32_t width, int32_t height, double_t brightness,
                            double_t contrast);
    void ColorLevelChanel_B(std::vector<uint8_t> &bImageData, int32_t width, int32_t height, double_t brightness,
                            double_t contrast);
    void ColorLevelChanel_RGB(std::vector<uint8_t> &rgbImageData, int32_t width, int32_t height, double_t brightness,
                              double_t contrast);

};

#endif // COLORLEVEL_H
