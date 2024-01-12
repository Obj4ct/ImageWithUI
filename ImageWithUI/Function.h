#ifndef FUNCTION_H
#define FUNCTION_H
#include "BMPFile.h"
#include "stdcpp.h"
#include "QTHeader.h"

class Function
{
public:
    Function();
    //create messagebox
    bool CreateMessagebox(QString title,QString message);
    //gray
    void ConvertToGray(std::vector<uint8_t>& imageData);

    //自动对比度相关
    //平均值
    double CalAver(const std::vector<uint8_t> &imageData);
    //标准差 需要先计算出平均值
    double CalStandard(const std::vector<uint8_t> &imageData, double aver);
    //自动对比度
    void AutoContrast(std::vector<uint8_t> &imageData);

    //均值模糊
    void AverageBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height);

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

    //色彩通道
    void Brightness(std::vector<uint8_t> &brightnessImageData, double_t brightnessValue);
    //重置图像
    void RestImage(MyValue &myValue);

};


#endif // FUNCTION_H
