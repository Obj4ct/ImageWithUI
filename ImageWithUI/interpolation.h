#ifndef INTERPOLATION_H
#define INTERPOLATION_H

#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
#include "mainwindow.h"
class MainWindow;
namespace Ui {
class Interpolation;
}

class Interpolation : public QWidget
{
    Q_OBJECT

public:
    explicit Interpolation(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    void ShowLargeNear(int height,int width);
    void ShowSmallNear(int height,int width);
    void ShowSmallBilinear(int height,int width);
    void ShowLargeBilinear(int height,int width);
    void ShowSmallBicubic(int height,int width);
    void ShowLargeBicubic(int height,int width);
private:
    Ui::Interpolation *ui;
    MainWindow* mainWindow;
    QMainWindow* newWindow;
    Function function;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t>imageData;
public slots:
    void SaveImageToFile(const std::vector<uint8_t>& imageData, int32_t width, int32_t height);


private:
    //双立方插值相关
    //权重函数
    float cubicWeight(float t);
    std::vector<uint8_t>
    LargeImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight);
    std::vector<uint8_t>
    SmallImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight);

    //双线性插值
    std::vector<uint8_t>
    SmallImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth,
                        int32_t newHeight);
    std::vector<uint8_t>
    LargeImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth,
                        int32_t newHeight);

    //最临近插值
    std::vector<uint8_t>
    SmallImage_Nearest(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight);
    std::vector<uint8_t>
    LargeImage_Nearest(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight);

    void ShowImage(const std::vector<uint8_t> &imageData,int width,int height);
};

#endif // INTERPOLATION_H
