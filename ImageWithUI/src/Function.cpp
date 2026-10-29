#include "Function.h"
// 定义结构体表示像素
//先定义一个结构体来表示图像中的像素，包括红、绿和蓝三个通道的值
struct Pixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};
//边缘检测算子 copy
int sobelX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

int sobelY[3][3] = {
    {-1, -2, -1},
    {0,  0,  0},
    {1,  2,  1}
};

Function::Function()
{

}


bool Function::CreateMessagebox(QString title,QString message)
{
    QMessageBox* myBox = new QMessageBox;
    QPushButton* okBtn = new QPushButton("确定");
    QString str = message;
    myBox->setWindowTitle(title);
    myBox->setText(str);
    myBox->setWindowIcon(QIcon(":/icon/what.png"));
    myBox->addButton(okBtn, QMessageBox::AcceptRole);
    myBox->show();
    myBox->exec();//阻塞等待用户输入

    const bool clickedOk = (myBox->clickedButton() == okBtn);
    myBox->deleteLater();
    // 点"确定" -> false(流程可继续)；其它方式关闭 -> true(流程应中断)
    return !clickedOk;
}

QImage Function::ToDisplayImage(const std::vector<uint8_t>& imageData, int32_t width, int32_t height)
{
    if (imageData.empty() || width <= 0 || height <= 0) {
        return QImage();
    }
    QImage image(imageData.data(), width, height, QImage::Format_BGR888);
    // BMP 自底向上存储，翻转后才是正立的画面
    return image.mirrored(false, true);
}

//to gray
void Function::ConvertToGray(std::vector<uint8_t>& imageData, size_t startPixel, size_t endPixel)
{
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        uint8_t gray = static_cast<uint8_t>(0.299 * imageData[i] + 0.587 * imageData[i + 1] + 0.114 * imageData[i + 2]);
        imageData[i] = gray;
        imageData[i + 1] = gray;
        imageData[i + 2] = gray;
    }
}

uint64_t Function::GetSum(const std::vector<uint8_t> &imageData)
{
    uint64_t sum = 0;
    for (uint8_t v : imageData) {
        sum += v;
    }
    return sum;
}

double Function::CalAver(const std::vector<uint8_t> &imageData)
{
    if (imageData.empty()) {
        return 0.0;
    }
    const uint64_t sum = GetSum(imageData);
    return static_cast<double>(sum) / static_cast<double>(imageData.size());
}

double Function::CalStandard(const std::vector<uint8_t> &imageData, double aver)
{
    if (imageData.empty()) {
        return 0.0;
    }
    double_t tempSum = 0.0;
    for (uint8_t i : imageData) {
        const double_t d = static_cast<double_t>(i) - aver;
        tempSum += d * d;
    }
    const double_t variance = tempSum / static_cast<double_t>(imageData.size());
    return std::sqrt(variance);
}

void Function::AutoContrast(std::vector<uint8_t> &imageData, float_t aver, float_t standard, size_t startPixel, size_t endPixel)
{
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    // 纯色图(标准差为 0)除零 -> 系数取 1，保持原样
    const float_t factor = (standard > 1e-6) ? (128.0f / standard) : 1.0f;

    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        for (int c = 0; c < 3; ++c) {
            const int32_t v = static_cast<int32_t>(factor * (static_cast<float_t>(imageData[i + c]) - aver) + 128.0f);
            imageData[i + c] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
        }
    }
}

void Function::AverageBlur(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, size_t startPixel, size_t endPixel)
{
    if (width == 0 || height == 0) {
        return;
    }
    const size_t total = static_cast<size_t>(width) * height;
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const size_t y = p / width;
        const size_t x = p % width;
        const size_t index = p * 3;

        uint32_t sum = 0;
        uint32_t count = 0;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int64_t ny = static_cast<int64_t>(y) + dy;
                const int64_t nx = static_cast<int64_t>(x) + dx;
                if (ny >= 0 && ny < static_cast<int64_t>(height) && nx >= 0 && nx < static_cast<int64_t>(width)) {
                    const size_t neighborIndex = (static_cast<size_t>(ny) * width + static_cast<size_t>(nx)) * 3;
                    sum += imageData[neighborIndex];
                    ++count;
                }
            }
        }

        if (count == 0) {
            continue;
        }
        imageData[index] = static_cast<uint8_t>(sum / count);
    }
}

void Function::Brightness(std::vector<uint8_t> &brightnessImageData, double_t brightnessValue)
{
    for (unsigned char &i: brightnessImageData) {
        double_t newValue = static_cast<double_t>(i) + brightnessValue;
        if (newValue < 0) {
            newValue = 0;
        } else if (newValue > 255) {
            newValue = 255;
        }
        i = static_cast<uint8_t>(newValue);
    }
}

void Function::Contrast(std::vector<uint8_t> &contrastImageData, double_t contrastValue)
{
    double_t factor = (100.0 + contrastValue) / 100.0;
    if (factor < 0.0) {
        factor = 0.0;
    }
    for (size_t i = 0; i + 2 < contrastImageData.size(); i += 3) {
        for (int c = 0; c < 3; ++c) {
            const int v = static_cast<int>(factor * (contrastImageData[i + c] - 128) + 128);
            contrastImageData[i + c] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
        }
    }
}

void Function::Saturation(std::vector<uint8_t> &saturationImageData, int32_t width, int32_t height, double_t saturationValue)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            const size_t index = (static_cast<size_t>(i) * width + j) * 3;
            if (index + 2 >= saturationImageData.size()) {
                break;
            }
            uint8_t r = saturationImageData[index];
            uint8_t g = saturationImageData[index + 1];
            uint8_t b = saturationImageData[index + 2];

            double h, s, v;
            RGBtoHSV(r, g, b, h, s, v);

            // 调整饱和度
            s *= saturationValue;

            // 确保饱和度在0到1之间
            s = std::max(0.0, std::min(1.0, s));

            HSVtoRGB(h, s, v, r, g, b);

            saturationImageData[index] = r;
            saturationImageData[index + 1] = g;
            saturationImageData[index + 2] = b;
        }
    }
}

void Function::HSVtoRGB(double h, double s, double v, uint8_t &r, uint8_t &g, uint8_t &b)
{
    if (s == 0.0) {
        r = g = b = static_cast<uint8_t>(std::max(0.0, std::min(255.0, v)));
    } else {
        h /= 60;
        int i = static_cast<int>(h);
        double f = h - i;
        double p = v * (1 - s);
        double q = v * (1 - s * f);
        double t = v * (1 - s * (1 - f));

        switch (i) {
        case 0: r = static_cast<uint8_t>(v); g = static_cast<uint8_t>(t); b = static_cast<uint8_t>(p); break;
        case 1: r = static_cast<uint8_t>(q); g = static_cast<uint8_t>(v); b = static_cast<uint8_t>(p); break;
        case 2: r = static_cast<uint8_t>(p); g = static_cast<uint8_t>(v); b = static_cast<uint8_t>(t); break;
        case 3: r = static_cast<uint8_t>(p); g = static_cast<uint8_t>(q); b = static_cast<uint8_t>(v); break;
        case 4: r = static_cast<uint8_t>(t); g = static_cast<uint8_t>(p); b = static_cast<uint8_t>(v); break;
        default: r = static_cast<uint8_t>(v); g = static_cast<uint8_t>(p); b = static_cast<uint8_t>(q); break;
        }
    }
}

void Function::RGBtoHSV(uint8_t r, uint8_t g, uint8_t b, double &h, double &s, double &v)
{
    const double minVal = std::min({static_cast<double>(r), static_cast<double>(g), static_cast<double>(b)});
    const double maxVal = std::max({static_cast<double>(r), static_cast<double>(g), static_cast<double>(b)});
    const double delta = maxVal - minVal;

    v = maxVal;

    if (maxVal == 0.0) {
        s = 0;
    } else {
        s = (delta / maxVal);
    }

    if (delta == 0.0) {
        h = 0;
    } else {
        if (r == maxVal) {
            h = (g - b) / delta;
        } else if (g == maxVal) {
            h = 2 + (b - r) / delta;
        } else {
            h = 4 + (r - g) / delta;
        }
        h *= 60;
        if (h < 0) {
            h += 360;
        }
    }
}

void Function::ColorBalance(std::vector<uint8_t> &imageData, int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const size_t pixelCount = static_cast<size_t>(width) * height;

    // 统计整幅图的平均亮度与各通道均值
    double_t graySum = 0.0;
    double_t channelSum[3] = {0.0, 0.0, 0.0};
    for (size_t p = 0; p < pixelCount; ++p) {
        const size_t index = p * 3;
        if (index + 2 >= imageData.size()) {
            break;
        }
        const double_t r = imageData[index];
        const double_t g = imageData[index + 1];
        const double_t b = imageData[index + 2];
        graySum += 0.299 * r + 0.587 * g + 0.114 * b;
        channelSum[0] += r;
        channelSum[1] += g;
        channelSum[2] += b;
    }

    const double_t avgGray = graySum / static_cast<double_t>(pixelCount);
    const double_t avgR = channelSum[0] / static_cast<double_t>(pixelCount);
    const double_t avgG = channelSum[1] / static_cast<double_t>(pixelCount);
    const double_t avgB = channelSum[2] / static_cast<double_t>(pixelCount);

    // 目标：把三通道均值拉到同一亮度基准(灰度均值)，保持色相不变
    const double_t target = (avgGray > 1.0) ? avgGray : 1.0;
    double_t factor[3] = {1.0, 1.0, 1.0};
    if (avgR > 1.0) factor[0] = target / avgR;
    if (avgG > 1.0) factor[1] = target / avgG;
    if (avgB > 1.0) factor[2] = target / avgB;

    for (size_t p = 0; p < pixelCount; ++p) {
        const size_t index = p * 3;
        if (index + 2 >= imageData.size()) {
            break;
        }
        for (int c = 0; c < 3; ++c) {
            const int v = static_cast<int>(imageData[index + c] * factor[c]);
            imageData[index + c] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
        }
    }
}

void Function::InvertColors(std::vector<uint8_t>& imageData, size_t startPixel, size_t endPixel) {
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        imageData[i] = static_cast<uint8_t>(255 - imageData[i]);
        imageData[i + 1] = static_cast<uint8_t>(255 - imageData[i + 1]);
        imageData[i + 2] = static_cast<uint8_t>(255 - imageData[i + 2]);
    }
}

void Function::Complementary(std::vector<uint8_t> &imageData, size_t startPixel, size_t endPixel)
{
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        const int r = imageData[i];
        const int g = imageData[i + 1];
        const int b = imageData[i + 2];
        const int maxRgb = std::max(std::max(r, g), b);
        const int minRgb = std::min(std::min(r, g), b);
        imageData[i]     = static_cast<uint8_t>(maxRgb + minRgb - r);
        imageData[i + 1] = static_cast<uint8_t>(maxRgb + minRgb - g);
        imageData[i + 2] = static_cast<uint8_t>(maxRgb + minRgb - b);
    }
}

void Function::Fisheye(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>>& result, int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        result.set_value(std::vector<uint8_t>());
        return;
    }
    // 首先确定图像的中心点坐标，以及最大半径
    const int32_t centerX = width / 2;
    const int32_t centerY = height / 2;
    const int32_t maxRadius = std::min(centerX, centerY);

    std::vector<uint8_t> fisheyeImage(static_cast<size_t>(width) * height * 3, 0);
    if (maxRadius <= 0) {
        result.set_value(fisheyeImage);
        return;
    }
    // 遍历每个像素，计算像素到图像中心的距离，并检查是否小于最大半径，以确保像素位于鱼眼效果的范围内。
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int32_t offsetX = x - centerX;
            const int32_t offsetY = y - centerY;
            const float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetY * offsetY));
            // 小于=应用鱼眼
            if (distance < maxRadius) {
                // 当前像素相对于图像中心点的极坐标角度
                const float angle = std::atan2(static_cast<float>(offsetY), static_cast<float>(offsetX));
                // 用于计算每个像素的新半径
                const float normalizedRadius = distance / maxRadius;
                // 进行非线性变换
                const float mappedRadius = normalizedRadius * normalizedRadius;

                // 极坐标变换
                const int32_t newX = static_cast<int32_t>(centerX + maxRadius * mappedRadius * std::cos(angle));
                const int32_t newY = static_cast<int32_t>(centerY + maxRadius * mappedRadius * std::sin(angle));

                // 边界保护，避免极坐标取整越界
                const int32_t cx = std::max(0, std::min(width - 1, newX));
                const int32_t cy = std::max(0, std::min(height - 1, newY));

                const size_t srcIndex = (static_cast<size_t>(cy) * width + cx) * 3;
                const size_t dstIndex = (static_cast<size_t>(y) * width + x) * 3;
                for (int channel = 0; channel < 3; ++channel) {
                    fisheyeImage[dstIndex + channel] = imageData[srcIndex + channel];
                }
            }
            // 超出范围保持黑色(初始化即为 0)
        }
    }

    result.set_value(fisheyeImage);
}

double Function::Gaussian(double sigma, int x, int y)
{
    if (sigma <= 1e-6) {
        return (x == 0 && y == 0) ? 1.0 : 0.0;
    }
    return exp(-(x * x + y * y) / (2.0 * sigma * sigma)) / (2.0 * M_PI * sigma * sigma);
}

void Function::Gauss(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, int width, int height, double sigma)
{
    if (width <= 0 || height <= 0) {
        result.set_value(std::vector<uint8_t>());
        return;
    }
    // sigma 为 0 会导致除零/NaN，这里钳制为一个极小但有效的值
    if (sigma <= 1e-3) {
        sigma = 1e-3;
    }

    std::vector<uint8_t> blurImageData(imageData.size(), 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float_t r = 0.0;
            float_t g = 0.0;
            float_t b = 0.0;
            float_t weightSum = 0.0;

            for (int j = -2; j <= 2; ++j) {
                for (int i = -2; i <= 2; ++i) {
                    // 用 int 计算坐标，避免无符号下溢导致越界读
                    const int pixelX = x + i;
                    const int pixelY = y + j;
                    if (pixelX >= 0 && pixelX < width && pixelY >= 0 && pixelY < height) {
                        const size_t pixelIndex = (static_cast<size_t>(pixelY) * width + pixelX) * 3;
                        const float_t weight = static_cast<float_t>(Gaussian(sigma, i, j));
                        r += static_cast<float_t>(imageData[pixelIndex]) * weight;
                        g += static_cast<float_t>(imageData[pixelIndex + 1]) * weight;
                        b += static_cast<float_t>(imageData[pixelIndex + 2]) * weight;
                        weightSum += weight;
                    }
                }
            }

            const size_t index = (static_cast<size_t>(y) * width + x) * 3;
            if (weightSum <= 0.0) {
                blurImageData[index] = imageData[index];
                blurImageData[index + 1] = imageData[index + 1];
                blurImageData[index + 2] = imageData[index + 2];
                continue;
            }
            blurImageData[index]     = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r / weightSum)));
            blurImageData[index + 1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g / weightSum)));
            blurImageData[index + 2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b / weightSum)));
        }
    }

    result.set_value(blurImageData);
}

void Function::HighContrast(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, const std::vector<uint8_t> &blurImageData)
{
    const size_t n = std::min(imageData.size(), blurImageData.size());
    std::vector<uint8_t> highContrastImageData(n);
    for (size_t i = 0; i < n; i++) {
        const int between = static_cast<int>(imageData[i]) - static_cast<int>(blurImageData[i]);
        const int v = between + 128;  // 调整到 0-255 范围
        highContrastImageData[i] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
    }

    result.set_value(highContrastImageData);
}

void Function::RotateImage(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    const double_t radians = angle * M_PI / 180.0;
    const double_t cosA = std::cos(radians);
    const double_t sinA = std::sin(radians);

    const int32_t centerX = width / 2;
    const int32_t centerY = height / 2;

    // 输出仍是 BMP 的自底向上布局：视觉行 v 对应内存行 (height-1-v)
    std::vector<uint8_t> rotatedImageData(imageData.size(), 0);

    for (int v = 0; v < height; ++v) {
        for (int x = 0; x < width; ++x) {
            // 在视觉坐标系(自顶向下)里做旋转
            const double_t rotatedX = cosA * (x - centerX) - sinA * (v - centerY) + centerX;
            const double_t rotatedY = sinA * (x - centerX) + cosA * (v - centerY) + centerY;

            if (rotatedX >= 0 && rotatedX < width && rotatedY >= 0 && rotatedY < height) {
                const int sx = static_cast<int>(std::round(rotatedX));
                const int sy = static_cast<int>(std::round(rotatedY));
                // 源像素同样按自底向上换算
                const size_t srcIndex = (static_cast<size_t>(height - 1 - sy) * width + sx) * 3;
                const size_t dstIndex = (static_cast<size_t>(height - 1 - v) * width + x) * 3;
                if (srcIndex + 2 < imageData.size() && dstIndex + 2 < rotatedImageData.size()) {
                    rotatedImageData[dstIndex]     = imageData[srcIndex];
                    rotatedImageData[dstIndex + 1] = imageData[srcIndex + 1];
                    rotatedImageData[dstIndex + 2] = imageData[srcIndex + 2];
                }
            }
        }
    }

    imageData = rotatedImageData;
}

void Function::RotateReverse(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle)
{
    const double_t clockwiseAngle = 360.0 - angle;
    RotateImage(imageData, width, height, clockwiseAngle);
}

uint8_t Function::CalculateMedian(std::vector<uint8_t>& window)
{
    if (window.empty()) {
        return 0;
    }
    std::sort(window.begin(), window.end());
    return window[window.size() / 2];
}

void Function::MedianBlur(std::vector<uint8_t>& imageData, uint32_t width, uint32_t height)
{
    if (width < 3 || height < 3) {
        return;
    }
    // 读原图写副本，避免就地修改导致的伪影
    std::vector<uint8_t> blurImage(imageData);
    for (uint32_t y = 1; y + 1 < height; ++y) {
        for (uint32_t x = 1; x + 1 < width; ++x) {
            std::vector<uint8_t> window;
            window.reserve(9);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const size_t index = (static_cast<size_t>(y + dy) * width + (x + dx)) * 3;
                    window.push_back(imageData[index]);
                }
            }
            const size_t index = (static_cast<size_t>(y) * width + x) * 3;
            blurImage[index] = CalculateMedian(window);
        }
    }
    imageData = blurImage;
}

void Function::MakeShadow(std::vector<uint8_t>& imageData, std::vector<uint8_t> &shadowImageData, double_t shadowValue)
{
    shadowImageData.resize(imageData.size());
    const double_t amount = std::max(0.0, std::min(255.0, shadowValue));
    for (size_t i = 0; i + 2 < imageData.size(); i += 3) {
        shadowImageData[i]     = static_cast<uint8_t>(std::max(0.0, static_cast<double_t>(imageData[i])     - amount));
        shadowImageData[i + 1] = static_cast<uint8_t>(std::max(0.0, static_cast<double_t>(imageData[i + 1]) - amount));
        shadowImageData[i + 2] = static_cast<uint8_t>(std::max(0.0, static_cast<double_t>(imageData[i + 2]) - amount));
    }
}

void Function::HighLight(std::vector<uint8_t>& imageData, std::vector<uint8_t> &highLightImageData, double_t pixel)
{
    const int reductionAmount = 50;
    highLightImageData.resize(imageData.size());
    const double_t threshold = std::max(0.0, std::min(255.0, pixel));
    for (size_t i = 0; i < imageData.size(); ++i) {
        if (static_cast<double_t>(imageData[i]) > threshold) {
            // 降低像素值，确保不小于0
            const int newValue = static_cast<int>(imageData[i]) - reductionAmount;
            highLightImageData[i] = static_cast<uint8_t>(std::max(newValue, 0));
        } else {
            // 对于低于阈值的值，保持像素值不变
            highLightImageData[i] = imageData[i];
        }
    }
}

void Function::Sharpen(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result, const std::vector<uint8_t> &highContrastImageData)
{
    // 高反差图 = 原图 - 模糊图 + 128，所以 (高反差 - 128) 就是原图与模糊图的差值。
    // 锐化 = 原图 + 差值，再钳制到 0-255。
    const size_t n = std::min(imageData.size(), highContrastImageData.size());
    std::vector<uint8_t> sharpenImageData(n);
    for (size_t i = 0; i < n; i++) {
        const int diff = static_cast<int>(highContrastImageData[i]) - 128;
        const int v = static_cast<int>(imageData[i]) + diff;
        sharpenImageData[i] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
    }

    result.set_value(sharpenImageData);
}

std::vector<uint8_t> Function::SobelEdge(const std::vector<uint8_t> &imageData, int width, int height)
{
    std::vector<uint8_t> edgeImageData(imageData.size(), 0);
    if (width < 3 || height < 3) {
        return edgeImageData;
    }
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sumX = 0;
            int sumY = 0;

            for (int j = 0; j < 3; j++) {
                for (int i = 0; i < 3; i++) {
                    const int pixelX = x + i - 1;
                    const int pixelY = y + j - 1;
                    const size_t pixelIndex = (static_cast<size_t>(pixelY) * width + pixelX) * 3;
                    // 用灰度值做梯度，避免只看单一通道造成的偏色边缘
                    const int grayValue = static_cast<int>(
                        0.299 * imageData[pixelIndex] +
                        0.587 * imageData[pixelIndex + 1] +
                        0.114 * imageData[pixelIndex + 2]);
                    sumX += grayValue * sobelX[j][i];
                    sumY += grayValue * sobelY[j][i];
                }
            }
            //edge
            //梯度的强度，通常是水平和垂直梯度的绝对值之和。这个值用来表示像素的边缘强度。
            const int edgeValue = std::min(std::max(std::abs(sumX) + std::abs(sumY), 0), 255);
            const size_t index = (static_cast<size_t>(y) * width + x) * 3;
            edgeImageData[index] = static_cast<uint8_t>(edgeValue);
            edgeImageData[index + 1] = static_cast<uint8_t>(edgeValue);
            edgeImageData[index + 2] = static_cast<uint8_t>(edgeValue);
        }
    }

    return edgeImageData;
}

void Function::ApplyThreshold(std::vector<uint8_t>& imageData, double_t threshold, size_t startPixel, size_t endPixel)
{
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    // 阈值钳制到 [1,254]，避免 0 导致除零、255 导致除负
    const double_t t = std::max(1.0, std::min(254.0, threshold));

    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        const double_t r = imageData[i];
        const double_t g = imageData[i + 1];
        const double_t b = imageData[i + 2];

        // 计算灰度值并使用double类型保存
        const double_t gray = 0.299 * r + 0.587 * g + 0.114 * b;

        if (gray > t) {
            // 使用比例因子来调整颜色通道值
            const double_t factor = (gray - t) / (255.0 - t);
            imageData[i]     = static_cast<uint8_t>(std::min(255.0, r + 100.0 * factor));
            imageData[i + 1] = static_cast<uint8_t>(std::min(255.0, g + 100.0 * factor));
            imageData[i + 2] = static_cast<uint8_t>(std::min(255.0, b + 100.0 * factor));
        } else {
            // 使用比例因子来调整颜色通道值
            const double_t factor = gray / t;
            imageData[i]     = static_cast<uint8_t>(std::max(0.0, r * factor));
            imageData[i + 1] = static_cast<uint8_t>(std::max(0.0, g * factor));
            imageData[i + 2] = static_cast<uint8_t>(std::max(0.0, b * factor));
        }
    }
}

void Function::FullMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t degree)
{
    if (width == 0 || height == 0 || degree == 0) {
        return;
    }
    // foreach pix 加上步长应该是移动像素方便处理下一个像素
    for (uint32_t y = 0; y < height; y += degree) {
        for (uint32_t x = 0; x < width; x += degree) {
            uint32_t totalR = 0;
            uint32_t totalG = 0;
            uint32_t totalB = 0;
            int mosaic = 0;
            // avg
            for (uint32_t dy = 0; dy < degree && y + dy < height; dy++) {
                for (uint32_t dx = 0; dx < degree && x + dx < width; dx++) {
                    const size_t index = (static_cast<size_t>(y + dy) * width + (x + dx)) * 3;
                    totalR += imageData[index];
                    totalG += imageData[index + 1];
                    totalB += imageData[index + 2];
                    mosaic++;
                }
            }
            if (mosaic == 0) {
                continue;
            }
            const uint8_t averageR = static_cast<uint8_t>(totalR / mosaic);
            const uint8_t averageG = static_cast<uint8_t>(totalG / mosaic);
            const uint8_t averageB = static_cast<uint8_t>(totalB / mosaic);
            // set each pix
            for (uint32_t dy = 0; dy < degree && y + dy < height; dy++) {
                for (uint32_t dx = 0; dx < degree && x + dx < width; dx++) {
                    const size_t index = (static_cast<size_t>(y + dy) * width + (x + dx)) * 3;
                    imageData[index] = averageR;
                    imageData[index + 1] = averageG;
                    imageData[index + 2] = averageB;
                }
            }
        }
    }
}
