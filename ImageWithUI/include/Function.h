#ifndef FUNCTION_H
#define FUNCTION_H
#include "BMPFile.h"
#include "stdcpp.h"
#include "QTHeader.h"

// 图像内存布局约定：
//   BMP 为自底向上存储，imageData 的第 0 行是画面的最底行；
//   显示时通过 mirrored(false, true) 垂直翻转。
//   因此凡是涉及"坐标映射"的算子（旋转等）必须按该约定换算，
//   否则显示与保存结果会上下颠倒。
//
// 分段约定：
//   所有支持并行处理的算子，其 start/end 均为【像素序号】(不是字节偏移)，
//   由调用方按像素数均分，保证每个像素的 B/G/R 三个通道不会被拆散。

class Function
{
public:
    Function();
    //create messagebox; 返回 true 表示用户未点"确定"(即关闭/取消)
    bool CreateMessagebox(QString title,QString message);

    //gray
    void ConvertToGray(std::vector<uint8_t>& imageData, size_t startPixel, size_t endPixel);
    uint64_t GetSum(const std::vector<uint8_t> &imageData);
    //平均值
    double CalAver(const std::vector<uint8_t> &imageData);
    //标准差 需要先计算出平均值
    double CalStandard(const std::vector<uint8_t> &imageData, double aver);
    //自动对比度
    void AutoContrast(std::vector<uint8_t> &imageData, float_t aver, float_t standard, size_t startPixel, size_t endPixel);

    //均值模糊
    void AverageBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height, size_t startPixel, size_t endPixel);

    //色彩通道
    //亮度
    void Brightness(std::vector<uint8_t> &brightnessImageData, double_t brightnessValue);
    //对比度
    void Contrast(std::vector<uint8_t> &contrastImageData, double_t contrastValue);
    //饱和度
    void Saturation(std::vector<uint8_t> &saturationImageData, int32_t width, int32_t height, double_t saturationValue);
    // 函数用于将HSV颜色转换回RGB颜色
    void HSVtoRGB(double h, double s, double v, uint8_t &r, uint8_t &g, uint8_t &b);
    // 函数用于将RGB颜色转换为HSV颜色
    void RGBtoHSV(uint8_t r, uint8_t g, uint8_t b, double &h, double &s, double &v);

    //色彩平衡(按亮度整体缩放，保持色相)
    void ColorBalance(std::vector<uint8_t> &imageData, int32_t width, int32_t height);

    //颜色反转
    void InvertColors(std::vector<uint8_t>& imageData, size_t startPixel, size_t endPixel);
    //补色
    void Complementary(std::vector<uint8_t>& imageData, size_t startPixel, size_t endPixel);

    //鱼眼镜头
    void Fisheye(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>>& result, int32_t width, int32_t height);
    //高斯模糊
    double Gaussian(double sigma, int x, int y);
    void Gauss(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, int width, int height, double sigma);
    //高反差保留 原图-高斯图
    void HighContrast(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, const std::vector<uint8_t> &blurImageData);

    //图像旋转
    //顺时针旋转
    void RotateImage(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle);
    //逆时针旋转
    void RotateReverse(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle);
    //中值模糊
    uint8_t CalculateMedian(std::vector<uint8_t>& window);
    void MedianBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height);

    //阴影和高光
    void MakeShadow(std::vector<uint8_t>& imageData, std::vector<uint8_t> &shadowImageData, double_t shadowValue);
    void HighLight(std::vector<uint8_t>& imageData, std::vector<uint8_t> &highLightImageData, double_t pixel);

    //图像锐化
    void Sharpen(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, const std::vector<uint8_t> &highContrastImageData);

    //边缘检测
    std::vector<uint8_t> SobelEdge(const std::vector<uint8_t>& imageData, int width, int height);
    //阈值处理
    void ApplyThreshold(std::vector<uint8_t>& imageData, double_t threshold, size_t startPixel, size_t endPixel);

    //马赛克
    void FullMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t degree);

    //把 BGR 像素缓冲转成可直接显示的 QImage(自动处理自底向上翻转)
    static QImage ToDisplayImage(const std::vector<uint8_t>& imageData, int32_t width, int32_t height);
};

#endif // FUNCTION_H
