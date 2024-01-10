#include "Function.h"

Function::Function()
{

}
//to gray
void Function::ConvertToGray(std::vector<uint8_t>& imageData)
{
    for (size_t i = 0; i < imageData.size(); i += 3) {
        uint8_t r = imageData[i];
        uint8_t g = imageData[i + 1];
        uint8_t b = imageData[i + 2];
        // cal gray
        auto gray = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
        // gary to every chanel
        imageData[i] = gray;
        imageData[i + 1] = gray;
        imageData[i + 2] = gray;
    }
}

double Function::CalAver(const std::vector<uint8_t> &imageData)
{
    double sum = 0.0;
    for (uint8_t pixel: imageData) {
        sum += pixel;
    }
    return sum / imageData.size();
}

double Function::CalStandard(const std::vector<uint8_t> &imageData, double aver)
{
    uint32_t tempSum = 0, sum = 0, variance;
    for (int i = 0; i < imageData.size(); i++) {
        sum += imageData[i];
    }
    aver = sum / imageData.size();
    for (int i = 0; i < imageData.size(); ++i) {
        tempSum += ((imageData[i] - aver) * (imageData[i] - aver));

    }
    variance = tempSum / imageData.size();


    double_t standard = std::sqrt(variance);
    return standard;
}
void Function::AutoContrast(std::vector<uint8_t> &imageData)
{
    double aver = CalAver(imageData);
    double standard = CalStandard(imageData, aver);
    //缩放因子 中间亮度值
    double factor = 128.0 / standard;

    for (size_t i = 0; i < imageData.size(); i += 3) {

        imageData[i] = std::max(0, std::min(255,
                                            static_cast<int>(factor * (imageData[i] - aver) + 128)));
        imageData[i + 1] = std::max(0, std::min(255,
                                                static_cast<int>(factor * (imageData[i + 1] - aver) +
                                                128)));
        imageData[i + 2] = std::max(0, std::min(255,
                                                static_cast<int>(factor * (imageData[i + 2] - aver) +
                                                128)));
    }
}

void Function::AverageBlur(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height)
{
    std::vector<uint8_t> blurImage(imageData);

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint32_t index = (y * width + x) * 3;
            uint32_t left = imageData[index - 3];
            uint32_t right = imageData[index + 3];
            uint32_t up = imageData[index - width * 3];
            uint32_t down = imageData[index + width * 3];
            uint32_t mid = imageData[index];
            uint32_t leftUp = imageData[index - width * 3 + 3];
            uint32_t rightUp = imageData[index - width * 3 - 3];
            uint32_t leftDown = imageData[index + width * 3 - 3];
            uint32_t rightDown = imageData[index + width * 3 + 3];
            auto averPix = (left + right + up + down + mid + leftUp + rightUp + leftDown + rightDown) / 9;
            imageData[index] = static_cast<uint8_t>(averPix);
        }
    }
}

float Function::cubicWeight(float t)
{
    // Cubic interpolation kernel
    if (std::abs(t) <= 1.0f) {
        return 1.5f * std::abs(t) * std::abs(t) * std::abs(t) - 2.5f * t * t + 1.0f;
    } else if (std::abs(t) <= 2.0f) {
        return -0.5f * std::abs(t) * std::abs(t) * std::abs(t) + 2.5f * t * t - 4.0f * std::abs(t) + 2.0f;
    } else {
        return 0.0f;
    }
}
std::vector<uint8_t> Function::
LargeImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight) {
    std::vector<uint8_t> resizedImage(newWidth * newHeight * 3);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            auto srcX = static_cast<float>(x) * width / newWidth;
            auto srcY = static_cast<float>(y) * height / newHeight;

            srcX = std::max(0.0f, std::min(srcX, static_cast<float>(width - 1)));
            srcY = std::max(0.0f, std::min(srcY, static_cast<float>(height - 1)));


            float weights[4][4];

            // 权重
            for (int j = -1; j <= 2; ++j) {
                for (int i = -1; i <= 2; ++i) {
                    int xi = static_cast<int>(std::floor(srcX)) + i;
                    int yj = static_cast<int>(std::floor(srcY)) + j;
                    xi = std::max(0, std::min(xi, width - 1));
                    yj = std::max(0, std::min(yj, height - 1));
                    float wx = cubicWeight(srcX - (xi + 0.5f));
                    float wy = cubicWeight(srcY - (yj + 0.5f));
                    weights[j + 1][i + 1] = wx * wy;
                }
            }

            // 插值
            for (int channel = 0; channel < 3; ++channel) {
                float interpolatedValue = 0.0f;
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        int xi = static_cast<int>(std::floor(srcX)) + i - 1;
                        int yj = static_cast<int>(std::floor(srcY)) + j - 1;
                        xi = std::max(0, std::min(xi, width - 1));
                        yj = std::max(0, std::min(yj, height - 1));
                        interpolatedValue += weights[j][i] * imageData[(yj * width + xi) * 3 + channel];
                    }
                }

                interpolatedValue = std::max(0.0f, std::min(255.0f, interpolatedValue));
                resizedImage[(y * newWidth + x) * 3 + channel] = static_cast<uint8_t>(interpolatedValue);
            }
        }
    }
    return resizedImage;
}

std::vector<uint8_t> Function::
SmallImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight) {
    std::vector<uint8_t> resizedImage(newWidth * newHeight * 3);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            auto srcX = static_cast<float>(x) * width / newWidth;
            auto srcY = static_cast<float>(y) * height / newHeight;

            srcX = std::max(0.0f, std::min(srcX, static_cast<float>(width - 1)));
            srcY = std::max(0.0f, std::min(srcY, static_cast<float>(height - 1)));

            float weights[4][4];
            //权重
            for (int j = -1; j <= 2; ++j) {
                for (int i = -1; i <= 2; ++i) {
                    int xi = static_cast<int>(std::floor(srcX)) + i;
                    int yj = static_cast<int>(std::floor(srcY)) + j;

                    xi = std::max(0, std::min(xi, width - 1));
                    yj = std::max(0, std::min(yj, height - 1));

                    float wx = cubicWeight(srcX - (xi + 0.5f));
                    float wy = cubicWeight(srcY - (yj + 0.5f));
                    weights[j + 1][i + 1] = wx * wy;
                }
            }

            // 插值
            for (int channel = 0; channel < 3; ++channel) {
                float interpolatedValue = 0.0f;
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        int xi = static_cast<int>(std::floor(srcX)) + i - 1;
                        int yj = static_cast<int>(std::floor(srcY)) + j - 1;
                        xi = std::max(0, std::min(xi, width - 1));
                        yj = std::max(0, std::min(yj, height - 1));

                        interpolatedValue += weights[j][i] * imageData[(yj * width + xi) * 3 + channel];
                    }
                }

                interpolatedValue = std::max(0.0f, std::min(255.0f, interpolatedValue));
                resizedImage[(y * newWidth + x) * 3 + channel] = static_cast<uint8_t>(interpolatedValue);
            }
        }
    }
    return resizedImage;
}

std::vector<uint8_t> Function::SmallImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(newWidth * newHeight * 3);
    // 计算缩小因子
    double scaleX = static_cast<double>(width) / newWidth;
    double scaleY = static_cast<double>(height) / newHeight;
    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            auto srcX = x * scaleX;
            auto srcY = y * scaleY;
            // 计算最近的像素点坐标?
            auto x1 = static_cast<int32_t>(srcX);
            auto x2 = static_cast<int32_t>(x1 + 1);
            auto y1 = static_cast<int32_t>(srcY);
            auto y2 = static_cast<int32_t>(y1 + 1);
            // 权重? 计算目标像素的权重 w1、w2、w3 和 w4，这些权重表示了目标像素与最近的四个原始像素之间的关系。
            auto tx = srcX - x1;
            auto ty = srcY - y1;
            auto w1 = (1.0 - tx) * (1.0 - ty);
            auto w2 = tx * (1.0 - ty);
            auto w3 = (1.0 - tx) * ty;
            auto w4 = tx * ty;
            // 新像素值?
            int32_t destIndex = (y * newWidth + x) * 3;
            int32_t srcIndex1 = (y1 * width + x1) * 3;
            int32_t srcIndex2 = (y1 * width + x2) * 3;
            int32_t srcIndex3 = (y2 * width + x1) * 3;
            int32_t srcIndex4 = (y2 * width + x2) * 3;
            for (int32_t channel = 0; channel < 3; channel++) {
                resizedImage[destIndex + channel] = static_cast<uint8_t>(
                            w1 * imageData[srcIndex1 + channel] +
                        w2 * imageData[srcIndex2 + channel] +
                        w3 * imageData[srcIndex3 + channel] +
                        w4 * imageData[srcIndex4 + channel]
                        );
            }
        }
    }

    return resizedImage;
}

std::vector<uint8_t> Function::LargeImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(newWidth * newHeight * 3);
    // 计算放大因子
    double scaleX = static_cast<double>(newWidth) / width;
    double scaleY = static_cast<double>(newHeight) / height;
    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            auto srcX = x / scaleX;
            auto srcY = y / scaleY;
            // 计算最近的像素点坐标
            auto x1 = static_cast<int32_t>(srcX);
            auto x2 = static_cast<int32_t>(x1 + 1);
            auto y1 = static_cast<int32_t>(srcY);
            auto y2 = static_cast<int32_t>(y1 + 1);
            // 权重?
            auto tx = srcX - x1;
            auto ty = srcY - y1;
            auto w1 = (1.0 - tx) * (1.0 - ty);
            auto w2 = tx * (1.0 - ty);
            auto w3 = (1.0 - tx) * ty;
            auto w4 = tx * ty;
            // 新像素值
            int32_t destIndex = (y * newWidth + x) * 3;
            int32_t srcIndex1 = (y1 * width + x1) * 3;
            int32_t srcIndex2 = (y1 * width + x2) * 3;
            int32_t srcIndex3 = (y2 * width + x1) * 3;
            int32_t srcIndex4 = (y2 * width + x2) * 3;
            for (int32_t channel = 0; channel < 3; channel++) {
                resizedImage[destIndex + channel] = static_cast<uint8_t>(
                            w1 * imageData[srcIndex1 + channel] +
                        w2 * imageData[srcIndex2 + channel] +
                        w3 * imageData[srcIndex3 + channel] +
                        w4 * imageData[srcIndex4 + channel]
                        );
            }
        }
    }
    return resizedImage;
}

void Function::SwitchBlendMode(uint8_t &destR, uint8_t &destG, uint8_t &destB, uint8_t srcR, uint8_t srcG, uint8_t srcB, BlendMode blendMode, double alpha)
{
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
                //  /255 需要保证像素的范围
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

