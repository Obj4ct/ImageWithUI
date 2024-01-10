#ifndef FUNCTION_H
#define FUNCTION_H
#include "BMPFile.h"
#include "stdcpp.h"
#include "QTHeader.h"
enum BlendMode {
    BLEND_NORMAL,     // 正常模式 在“正常”模式下，“混合色”的显示与不透明度的设置有关。
    BLEND_MULTIPLY,   // 正片叠底 其主要特点是以正片作为底图，再将其他图像叠加于其上，并根据叠加图像的颜色来调整底图的亮度和饱和度，产生丰富的合成效果。
    BLEND_SCREEN,     // 滤色 ps中滤色的作用结果和正片叠底刚好相反，它是将两个颜色的互补色的像素值相乘，然后除以255得到的最终色的像素值。通常执行滤色模式后的颜色都较浅。
    BLEND_OVERLAY,    // 叠加 　“叠加”模式把图像的“基色”颜色与“混合色”颜色相混合产生一种中间色。
                      // “基色”内颜色比“混合色”颜色暗的颜色使“混合色”颜色倍增，比“混合色”颜色亮的颜色将使“混合色”颜色被遮盖，
                      // 而图像内的高亮部分和阴影部分保持不变，因此对黑色或白色像素着色时“叠加”模式不起作用。
};
class Function
{
public:
    Function();
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

    //图层混合
    void SwitchBlendMode(uint8_t &destR, uint8_t &destG, uint8_t &destB, uint8_t srcR, uint8_t srcG, uint8_t srcB,
                         BlendMode blendMode, double alpha);
    //重置图像
    void RestImage(MyValue &myValue);
};


#endif // FUNCTION_H
