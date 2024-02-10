#ifndef FUNCTION_H
#define FUNCTION_H
#include "BMPFile.h"
#include "stdcpp.h"
#include "QTHeader.h"

extern std::vector<unsigned char> colorMap;
struct Pixel;
extern int sobelX[3][3];
extern int sobelY[3][3];

class Function
{
public:
    Function();
    //create messagebox
    bool CreateMessagebox(QString title,QString message);
    //gray
    void ConvertToGray(std::vector<uint8_t>& imageData,size_t start,size_t end);
    uint32_t GetSum(const std::vector<uint8_t> &imageData);
    //自动对比度相关
    //平均值
    double CalAver(const std::vector<uint8_t> &imageData);
    //标准差 需要先计算出平均值
    double CalStandard(const std::vector<uint8_t> &imageData, double aver);
    //自动对比度
    void AutoContrast(std::vector<uint8_t> &imageData, float_t aver, float_t standard, size_t start, size_t end);

    //均值模糊
    void AverageBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height,size_t start,size_t end);



    //色彩通道
    //亮度
    void Brightness(std::vector<uint8_t> &brightnessImageData, double_t brightnessValue);
    //对比度
    void Contrast(std::vector<uint8_t> &contrastImageData, double_t contrastValue);
    //饱和度  饱和度函数是在RGB颜色空间下工作的，但这种颜色空间不太适合修改饱和度。通常，会将图像转换为HSV（色相、饱和度、明度）颜色空间，然后在饱和度通道上进行修改，最后再将图像转换回RGB。
    void Saturation(std::vector<uint8_t> &saturationImageData, int32_t width, int32_t height, double_t saturationValue);
    // 函数用于将HSV颜色转换回RGB颜色
    void HSVtoRGB(double h, double s, double v, uint8_t &r, uint8_t &g, uint8_t &b);
    // 函数用于将RGB颜色转换为HSV颜色
    void RGBtoHSV(uint8_t r, uint8_t g, uint8_t b, double &h, double &s, double &v);


    //色彩平衡
    void ColorBalance(std::vector<uint8_t> &imageData, int32_t width, int32_t height);

    //色彩映射 (需要用到之前写的转为灰度图函数)
    void ColorMap(std::vector<uint8_t>& imageData, std::vector<uint8_t>& colorMap);
    //颜色反转
    void InvertColors(std::vector<uint8_t>& imageData,size_t start,size_t end);
    //补色
    void Complementary(std::vector<uint8_t>& imageData,size_t start,size_t end);

    //鱼眼镜头
    void Fisheye(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>>& result, int32_t width, int32_t height);
    //高斯模糊
    double Gaussian(double sigma, int x, int y);
    void Gauss(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>> &result, int width, int height, double sigma);
    //高反差保留 原图-高斯图
    void HighContrast(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>> &result,const std::vector<uint8_t> &blurImageData);

    //图像旋转
    //顺时针旋转
    void RotateImage(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle);
    //逆时针旋转
    void RotateReverse(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle);
    //中值模糊
    uint8_t CalculateMedian(std::vector<uint8_t>& window);
    void MedianBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height);

    //阴影和高光
    void MakeShadow(std::vector<uint8_t>& imageData,std::vector<uint8_t> &shadowImageData,uint32_t shadowValue);
    void HighLight(std::vector<uint8_t> &imageData,std::vector<uint8_t> &highLightImageData,uint32_t pixel);

    //图像锐化

    void Sharpen(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>> &result, const std::vector<uint8_t> &highContrastImageData);

    //边缘检测
    std::vector<uint8_t> SobelEdge(const std::vector<uint8_t>& imageData, int width, int height);
    //阈值处理
    void ApplyThreshold(std::vector<uint8_t>& imageData, uint32_t threshold,size_t start,size_t end);

    //马赛克
    void FullMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t degree);
    //重置图像
    void RestImage(MyValue &myValue);

};


#endif // FUNCTION_H
