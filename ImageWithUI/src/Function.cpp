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

QImage Function::ToDisplayImage(const std::vector<uint8_t>& imageData, int32_t width, int32_t height){
    if (imageData.empty() || width <= 0 || height <= 0) {
        return QImage();
    }
    // 缓冲区必须容纳声明的画布尺寸，否则 QImage 包装后会越界读取
    if (imageData.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 3) {
        return QImage();
    }
    // 必须显式给出 bytesPerLine = width*3：
    // QImage 的 4 参数构造会把每行补齐到 4 字节对齐(width 不是 4 的倍数时每行多 1~3 字节)，
    // 后续 mirrored()/convert 按对齐后的行宽读取，就会读到缓冲区末尾之外，
    // 在 Debug CRT 的 memcpy 里触发 0xC0000005(旋转后宽度常不是 4 的倍数)。
    QImage image(imageData.data(), width, height, width * 3, QImage::Format_BGR888);
    // BMP 自底向上存储，翻转后才是正立的画面
    return image.mirrored(false, true);
}

// 眼部放大：径向放大(类似 PS 液化里的"膨胀"工具)
// 对圆内每个像素，把采样点往圆心收缩 factor 倍再双线性插值，等价于把眼睛内容整体放大；
// 收缩量由 (1-t²)² 控制(t=r/R)，所以圆心处最强、到半径处平滑归零，不会留下硬边圆环。
void Function::EnlargeEye(std::vector<uint8_t> &imageData, int32_t width, int32_t height,
                          int32_t centerX, int32_t centerY, int32_t radius, double strength)
{
    if (width <= 0 || height <= 0 || radius <= 0 ||
        imageData.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 3) {
        return;
    }
    // 强度上限留余量：取 1.0 时中心所有像素都会采样到同一点，放大后会糊成一团
    const double s = std::max(0.0, std::min(0.98, strength));
    const double radiusD = static_cast<double>(radius);
    const std::vector<uint8_t> src = imageData;      // 只读快照，圆外像素保持原样

    const int32_t yBegin = std::max<int32_t>(0, centerY - radius);
    const int32_t yEnd = std::min<int32_t>(height - 1, centerY + radius);

    for (int32_t row = yBegin; row <= yEnd; ++row) {
        const double dy = static_cast<double>(row) - static_cast<double>(centerY);
        const double halfWidthSq = radiusD * radiusD - dy * dy;
        if (halfWidthSq <= 0.0) {
            continue;                                // 该行完全在圆外
        }
        const double halfWidth = std::sqrt(halfWidthSq);
        const int32_t xBegin = std::max<int32_t>(0, static_cast<int32_t>(std::ceil(centerX - halfWidth)));
        const int32_t xEnd = std::min<int32_t>(width - 1, static_cast<int32_t>(std::floor(centerX + halfWidth)));

        for (int32_t x = xBegin; x <= xEnd; ++x) {
            const double dx = static_cast<double>(x) - static_cast<double>(centerX);
            const double distance = std::sqrt(dx * dx + dy * dy);
            if (distance >= radiusD) {
                continue;
            }
            const double t = distance / radiusD;
            const double falloff = 1.0 - t * t;
            const double factor = 1.0 - s * falloff * falloff;

            const double sampleX = std::max(0.0, std::min(static_cast<double>(width - 1),
                                                          static_cast<double>(centerX) + dx * factor));
            const double sampleY = std::max(0.0, std::min(static_cast<double>(height - 1),
                                                          static_cast<double>(centerY) + dy * factor));

            const int32_t x0 = static_cast<int32_t>(std::floor(sampleX));
            const int32_t y0 = static_cast<int32_t>(std::floor(sampleY));
            const int32_t x1 = std::min(x0 + 1, width - 1);
            const int32_t y1 = std::min(y0 + 1, height - 1);
            const double fx = sampleX - x0;
            const double fy = sampleY - y0;

            const size_t i00 = (static_cast<size_t>(y0) * width + x0) * 3;
            const size_t i01 = (static_cast<size_t>(y0) * width + x1) * 3;
            const size_t i10 = (static_cast<size_t>(y1) * width + x0) * 3;
            const size_t i11 = (static_cast<size_t>(y1) * width + x1) * 3;
            const size_t o = (static_cast<size_t>(row) * width + x) * 3;

            for (int c = 0; c < 3; ++c) {
                const double value =
                        src[i00 + c] * (1 - fx) * (1 - fy) +
                        src[i01 + c] * fx * (1 - fy) +
                        src[i10 + c] * (1 - fx) * fy +
                        src[i11 + c] * fx * fy;
                imageData[o + c] = static_cast<uint8_t>(std::max(0.0, std::min(255.0, value)));
            }
        }
    }
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

bool Function::RotateImage(std::vector<uint8_t> &imageData, int32_t &width, int32_t &height,
                           double_t angle, ContentQuad &quad)
{
    if (width <= 0 || height <= 0 || imageData.empty()) {
        return false;
    }
    if (imageData.size() != static_cast<size_t>(width) * height * 3) {
        return false;   // 缓冲区与宽高不一致时直接放弃，避免越界读写产生撕裂
    }
    // 内容区域数据损坏(非有限值/退化，例如子窗口结果丢失了几何信息) -> 回退为整幅画布
    bool quadBad = !quad.isValid();
    for (int i = 0; i < 4 && !quadBad; ++i) {
        if (!std::isfinite(quad.x[i]) || !std::isfinite(quad.y[i])) {
            quadBad = true;
        }
    }
    if (quadBad) {
        quad.setRect(width, height);
    }

    // 归一化角度到 [0, 360)
    double_t a = std::fmod(angle, 360.0);
    if (a < 0.0) {
        a += 360.0;
    }
    if (!std::isfinite(a)) {
        return false;   // 角度非法，保持原图
    }

    const int32_t W = width;
    const int32_t H = height;

    // ---- 精确无损路径：任意 90 度整数倍 ----
    // 这里不再要求 quad 与画布轴对齐：90 度旋转对整幅画布本身就是精确变换。
    // 若此时仍走"按内容外接矩形重建"的通用路径，每转一次画布就膨胀一圈(最大 1.41 倍)，
    // 反复旋转会让画布尺寸指数增长，最终申请到无法提交的虚拟内存，写入时触发访问冲突崩溃。
    const double_t nearestQuarter = std::round(a / 90.0) * 90.0;
    if (std::abs(a - nearestQuarter) < 1e-6) {
        const int quarter = ((static_cast<int>(std::lround(a / 90.0)) % 4) + 4) % 4;
        if (quarter == 0) {
            return false;   // 0/360 度：无变化
        }
        const int32_t newW = (quarter % 2 == 0) ? W : H;
        const int32_t newH = (quarter % 2 == 0) ? H : W;
        std::vector<uint8_t> rotated;
        try {
            rotated.assign(static_cast<size_t>(newW) * static_cast<size_t>(newH) * 3, 0);
        } catch (const std::bad_alloc &) {
            return false;   // 内存不足：保持原图，避免崩溃
        }
        // 内存行 r 对应视觉行 (H-1-r)；顺时针 90° 的映射为 dst=(W-1-c, r)
        for (int32_t r = 0; r < H; ++r) {
            for (int32_t c = 0; c < W; ++c) {
                int32_t nr = 0;
                int32_t nc = 0;
                switch (quarter) {
                case 1:  nr = W - 1 - c;  nc = r;          break; // 顺时针 90°
                case 2:  nr = H - 1 - r; nc = W - 1 - c; break; // 180°
                default: nr = c;         nc = H - 1 - r; break; // 逆时针 90°
                }
                const size_t src = (static_cast<size_t>(r) * W + c) * 3;
                const size_t dst = (static_cast<size_t>(nr) * newW + nc) * 3;
                rotated[dst] = imageData[src];
                rotated[dst + 1] = imageData[src + 1];
                rotated[dst + 2] = imageData[src + 2];
            }
        }
        imageData.swap(rotated);
        width = newW;
        height = newH;
        // 内容四边形按同一变换同步(视觉坐标，角点按画布边界)
        for (int i = 0; i < 4; ++i) {
            const double_t qx0 = quad.x[i];
            const double_t qy0 = quad.y[i];
            switch (quarter) {
            case 1:  quad.x[i] = H - qy0; quad.y[i] = qx0; break;
            case 2:  quad.x[i] = W - qx0; quad.y[i] = H - qy0; break;
            default: quad.x[i] = qy0;     quad.y[i] = W - qx0; break;
            }
        }
        return true;
    }

    // ---- 通用路径：按内容外接矩形重建画布 + 双线性采样 ----
    // 视觉坐标 y 向下；内存为 BMP 自底向上(第 0 行是画面最底行)
    const double_t rad = a * M_PI / 180.0;
    const double_t cosA = std::cos(rad);
    const double_t sinA = std::sin(rad);
    const double_t cx = W / 2.0;    // 旧画布中心(= 内容中心)
    const double_t cy = H / 2.0;

    // 内容四边形绕中心旋转(屏幕坐标顺时针)
    double_t qx[4];
    double_t qy[4];
    double_t minX = 1e30, maxX = -1e30, minY = 1e30, maxY = -1e30;
    for (int i = 0; i < 4; ++i) {
        const double_t px = quad.x[i] - cx;
        const double_t py = quad.y[i] - cy;
        qx[i] = cosA * px - sinA * py;
        qy[i] = sinA * px + cosA * py;
        minX = std::min(minX, qx[i]);
        maxX = std::max(maxX, qx[i]);
        minY = std::min(minY, qy[i]);
        maxY = std::max(maxY, qy[i]);
    }

    // 内容区域明显异常(外接矩形远超画布)时放弃本次旋转，防止按错误范围重建画布
    const double_t spanX = maxX - minX;
    const double_t spanY = maxY - minY;
    const double_t maxSpan = 8.0 * static_cast<double_t>(std::max(W, H));
    if (!std::isfinite(spanX) || !std::isfinite(spanY) ||
        spanX <= 0.0 || spanY <= 0.0 || spanX > maxSpan || spanY > maxSpan) {
        return false;
    }

    // 新画布 = 内容外接矩形(向上取整，内容居中，不多不少)
    const int32_t newW = std::max(1, static_cast<int32_t>(std::ceil(spanX)));
    const int32_t newH = std::max(1, static_cast<int32_t>(std::ceil(spanY)));
    const double_t halfNewW = newW / 2.0;
    const double_t halfNewH = newH / 2.0;

    // 像素总量上限：防止反复旋转导致画布无限膨胀，
    // 申请到无法提交的虚拟内存后写入会直接引发访问冲突(0xC0000005)。
    const size_t outPixels = static_cast<size_t>(newW) * static_cast<size_t>(newH);
    const size_t kMaxRotatePixels = 100ULL * 1000ULL * 1000ULL;   // 1 亿像素(约 300MB)
    if (outPixels > kMaxRotatePixels) {
        return false;
    }

    std::vector<uint8_t> rotated;
    try {
        rotated.assign(outPixels * 3, 0);
    } catch (const std::bad_alloc &) {
        return false;   // 内存不足：保持原图，避免崩溃
    }

    for (int32_t v = 0; v < newH; ++v) {
        for (int32_t x = 0; x < newW; ++x) {
            // 目标像素(视觉坐标)相对新画布中心
            const double_t dx = (x + 0.5) - halfNewW;
            const double_t dy = (v + 0.5) - halfNewH;
            // 反向旋转回源图(绝对视觉坐标)
            const double_t X = cosA * dx + sinA * dy + cx;
            const double_t Y = -sinA * dx + cosA * dy + cy;

            // 落在内容之外(旧画布的黑角) -> 保持黑色，不参与内容统计
            if (!quad.contains(X, Y)) {
                continue;
            }

            // 视觉坐标 -> 内存连续坐标(像素中心正好落在整数上)
            const double_t mc = X - 0.5;             // 列
            const double_t mr = (H - 0.5) - Y;       // 行(自底向上)

            const int32_t c0 = static_cast<int32_t>(std::floor(mc));
            const int32_t r0 = static_cast<int32_t>(std::floor(mr));
            const double_t fx = mc - c0;
            const double_t fy = mr - r0;

            const int32_t cr = std::max(0, std::min(c0, W - 1));
            const int32_t c1 = std::max(0, std::min(c0 + 1, W - 1));
            const int32_t rr = std::max(0, std::min(r0, H - 1));
            const int32_t r1 = std::max(0, std::min(r0 + 1, H - 1));

            const size_t i00 = (static_cast<size_t>(rr) * W + cr) * 3;
            const size_t i01 = (static_cast<size_t>(rr) * W + c1) * 3;
            const size_t i10 = (static_cast<size_t>(r1) * W + cr) * 3;
            const size_t i11 = (static_cast<size_t>(r1) * W + c1) * 3;

            const size_t dst = (static_cast<size_t>(newH - 1 - v) * newW + x) * 3;
            for (int ch = 0; ch < 3; ++ch) {
                const double_t value =
                        imageData[i00 + ch] * (1 - fx) * (1 - fy) +
                        imageData[i01 + ch] * fx * (1 - fy) +
                        imageData[i10 + ch] * (1 - fx) * fy +
                        imageData[i11 + ch] * fx * fy;
                rotated[dst + ch] = static_cast<uint8_t>(std::max(0.0, std::min(255.0, value)));
            }
        }
    }

    imageData.swap(rotated);
    width = newW;
    height = newH;
    // 更新内容四边形(相对新画布中心 -> 绝对坐标)
    for (int i = 0; i < 4; ++i) {
        quad.x[i] = qx[i] + halfNewW;
        quad.y[i] = qy[i] + halfNewH;
    }
    return true;
}

void Function::RotateReverse(std::vector<uint8_t> &imageData, int32_t &width, int32_t &height,
                              double_t angle, ContentQuad &quad)
{
    const double_t clockwiseAngle = 360.0 - angle;
    RotateImage(imageData, width, height, clockwiseAngle, quad);
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

// ------------------------------------------------------------------
// 几何变换(P2-A)：翻转/转置/裁剪
// 所有坐标均按【视觉】约定(y 向下)，内部再换算成自底向上的内存行，
// 保证与显示/保存结果一致。
// ------------------------------------------------------------------
void Function::FlipImage(std::vector<uint8_t> &imageData, int32_t &width, int32_t &height,
                         FlipMode mode, ContentQuad &quad)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    if (imageData.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 3) {
        return;
    }
    const int32_t W = width;
    const int32_t H = height;
    const bool swap = (mode == FlipMode::Transpose || mode == FlipMode::AntiTranspose);
    const int32_t outW = swap ? H : W;
    const int32_t outH = swap ? W : H;

    std::vector<uint8_t> out(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 3, 0);
    for (int32_t yv = 0; yv < outH; ++yv) {
        for (int32_t x = 0; x < outW; ++x) {
            int32_t sx = x;
            int32_t sy = yv;
            switch (mode) {
            case FlipMode::Horizontal:    sx = W - 1 - x;  break;
            case FlipMode::Vertical:      sy = H - 1 - yv; break;
            case FlipMode::Transpose:     sx = yv; sy = x; break;
            case FlipMode::AntiTranspose: sx = H - 1 - yv; sy = W - 1 - x; break;
            }
            const size_t src = (static_cast<size_t>(H - 1 - sy) * W + sx) * 3;
            const size_t dst = (static_cast<size_t>(outH - 1 - yv) * outW + x) * 3;
            out[dst]     = imageData[src];
            out[dst + 1] = imageData[src + 1];
            out[dst + 2] = imageData[src + 2];
        }
    }
    imageData.swap(out);
    width = outW;
    height = outH;

    // 内容区域同步变换(视觉坐标)
    for (int i = 0; i < 4; ++i) {
        const double qx = quad.x[i];
        const double qy = quad.y[i];
        switch (mode) {
        case FlipMode::Horizontal:    quad.x[i] = W - qx; quad.y[i] = qy; break;
        case FlipMode::Vertical:      quad.x[i] = qx; quad.y[i] = H - qy; break;
        case FlipMode::Transpose:     quad.x[i] = qy; quad.y[i] = qx; break;
        case FlipMode::AntiTranspose: quad.x[i] = H - qy; quad.y[i] = W - qx; break;
        }
    }
}

void Function::CropImage(std::vector<uint8_t> &imageData, int32_t &width, int32_t &height,
                         int32_t x, int32_t y, int32_t cw, int32_t ch, ContentQuad &quad)
{
    if (width <= 0 || height <= 0 || imageData.empty()) {
        return;
    }
    x = std::max(0, std::min(x, width - 1));
    y = std::max(0, std::min(y, height - 1));
    cw = std::max(1, std::min(cw, width - x));
    ch = std::max(1, std::min(ch, height - y));

    std::vector<uint8_t> out(static_cast<size_t>(cw) * static_cast<size_t>(ch) * 3, 0);
    for (int32_t yv = 0; yv < ch; ++yv) {
        const size_t srcRow = static_cast<size_t>(height - 1 - (y + yv)) * width;
        const size_t dstRow = static_cast<size_t>(ch - 1 - yv) * cw;
        for (int32_t i = 0; i < cw; ++i) {
            const size_t s = (srcRow + x + i) * 3;
            const size_t d = (dstRow + i) * 3;
            out[d]     = imageData[s];
            out[d + 1] = imageData[s + 1];
            out[d + 2] = imageData[s + 2];
        }
    }
    imageData.swap(out);
    width = cw;
    height = ch;
    quad.setRect(width, height);
}

// ------------------------------------------------------------------
// 增强(P2-A)：直方图均衡 / 形态学 / 卷积 / USM
// ------------------------------------------------------------------
void Function::BuildEqualizeLUT(const std::vector<uint8_t> &imageData, uint8_t lut[256])
{
    for (int i = 0; i < 256; ++i) {
        lut[i] = static_cast<uint8_t>(i);   // 默认恒等映射
    }
    const size_t n = imageData.size() / 3;
    if (n == 0) {
        return;
    }
    uint64_t hist[256] = {0};
    for (size_t p = 0; p < n; ++p) {
        const size_t i = p * 3;
        int gray = static_cast<int>(0.299 * imageData[i] + 0.587 * imageData[i + 1] + 0.114 * imageData[i + 2]);
        gray = std::max(0, std::min(255, gray));
        ++hist[gray];
    }
    uint64_t cdf[256];
    uint64_t acc = 0;
    for (int v = 0; v < 256; ++v) {
        acc += hist[v];
        cdf[v] = acc;
    }
    uint64_t cdfMin = 0;
    for (int v = 0; v < 256; ++v) {
        if (cdf[v] > 0) {
            cdfMin = cdf[v];
            break;
        }
    }
    const double denom = static_cast<double>(n) - static_cast<double>(cdfMin);
    if (denom <= 0.0) {
        return;   // 纯色图：保持原样
    }
    for (int v = 0; v < 256; ++v) {
        const double t = (static_cast<double>(cdf[v]) - static_cast<double>(cdfMin)) / denom * 255.0;
        lut[v] = static_cast<uint8_t>(std::max(0.0, std::min(255.0, t)));
    }
}

void Function::ApplyLUT(std::vector<uint8_t> &imageData, const uint8_t lut[256],
                        size_t startPixel, size_t endPixel)
{
    const size_t total = imageData.size() / 3;
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const size_t i = p * 3;
        imageData[i]     = lut[imageData[i]];
        imageData[i + 1] = lut[imageData[i + 1]];
        imageData[i + 2] = lut[imageData[i + 2]];
    }
}

void Function::HistogramEqualize(std::vector<uint8_t> &imageData, int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    uint8_t lut[256];
    BuildEqualizeLUT(imageData, lut);
    ApplyLUT(imageData, lut, 0, imageData.size() / 3);
}

void Function::Morphology(std::vector<uint8_t> &imageData, int32_t width, int32_t height,
                          MorphType type, size_t startPixel, size_t endPixel)
{
    if (width <= 0 || height <= 0 || imageData.empty()) {
        return;
    }
    const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t last = std::min(endPixel, total);

    auto clampIndex = [width, height](int64_t x, int64_t y) -> size_t {
        const int64_t cx = std::max<int64_t>(0, std::min<int64_t>(width - 1, x));
        const int64_t cy = std::max<int64_t>(0, std::min<int64_t>(height - 1, y));
        return (static_cast<size_t>(cy) * static_cast<size_t>(width) + static_cast<size_t>(cx)) * 3;
    };
    // 单趟形态学：src 只读，dst 只写本段(可分段并行)
    auto runPass = [&](const std::vector<uint8_t> &src, std::vector<uint8_t> &dst, bool dilate) {
        for (size_t p = startPixel; p < last; ++p) {
            const int32_t x = static_cast<int32_t>(p % width);
            const int32_t y = static_cast<int32_t>(p / width);
            const size_t di = p * 3;
            for (int c = 0; c < 3; ++c) {
                uint8_t v = dilate ? 0 : 255;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const uint8_t n = src[clampIndex(x + dx, y + dy) + c];
                        v = dilate ? std::max(v, n) : std::min(v, n);
                    }
                }
                dst[di + c] = v;
            }
        }
    };

    std::vector<uint8_t> snapshot(imageData);
    switch (type) {
    case MorphType::Erode:
        runPass(snapshot, imageData, false);
        break;
    case MorphType::Dilate:
        runPass(snapshot, imageData, true);
        break;
    case MorphType::Open: {
        std::vector<uint8_t> tmp(snapshot.size(), 0);
        runPass(snapshot, tmp, false);
        runPass(tmp, imageData, true);
        break;
    }
    case MorphType::Close: {
        std::vector<uint8_t> tmp(snapshot.size(), 0);
        runPass(snapshot, tmp, true);
        runPass(tmp, imageData, false);
        break;
    }
    }
}

void Function::Convolve3x3(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst,
                           int32_t width, int32_t height, const double *kernel,
                           double scale, double offset, size_t startPixel, size_t endPixel)
{
    const size_t total = static_cast<size_t>(std::max(0, width)) * static_cast<size_t>(std::max(0, height));
    if (total == 0 || src.size() < total * 3 || kernel == nullptr) {
        dst.clear();
        return;
    }
    dst.assign(total * 3, 0);
    if (std::abs(scale) < 1e-9) {
        scale = 1.0;   // 除零保护
    }
    const size_t last = std::min(endPixel, total);
    for (size_t p = startPixel; p < last; ++p) {
        const int32_t x = static_cast<int32_t>(p % width);
        const int32_t y = static_cast<int32_t>(p / width);
        const size_t di = p * 3;
        for (int c = 0; c < 3; ++c) {
            double sum = 0.0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    const int64_t sx = std::max<int64_t>(0, std::min<int64_t>(width - 1, x + kx));
                    const int64_t sy = std::max<int64_t>(0, std::min<int64_t>(height - 1, y + ky));
                    const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(width) +
                                       static_cast<size_t>(sx)) * 3;
                    sum += static_cast<double>(src[si + c]) * kernel[(ky + 1) * 3 + (kx + 1)];
                }
            }
            const int v = static_cast<int>(std::lround(sum / scale + offset));
            dst[di + c] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
        }
    }
}

void Function::USMSharpen(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst,
                          int32_t width, int32_t height,
                          double radius, double amount, double threshold)
{
    dst = src;
    if (amount <= 0.0 || width <= 0 || height <= 0 || src.empty()) {
        return;
    }
    std::promise<std::vector<uint8_t>> promise;
    std::future<std::vector<uint8_t>> future = promise.get_future();
    Gauss(src, promise, width, height, std::max(0.1, radius));
    const std::vector<uint8_t> blurred = future.get();
    const size_t n = std::min(src.size(), blurred.size());
    const double thr = std::max(0.0, threshold);
    for (size_t i = 0; i < n; ++i) {
        const double d = static_cast<double>(src[i]) - static_cast<double>(blurred[i]);
        if (std::abs(d) < thr) {
            continue;   // 低于阈值视为噪声/平滑区，不锐化
        }
        const int v = static_cast<int>(std::lround(static_cast<double>(src[i]) + amount * d));
        dst[i] = static_cast<uint8_t>(std::max(0, std::min(255, v)));
    }
}
