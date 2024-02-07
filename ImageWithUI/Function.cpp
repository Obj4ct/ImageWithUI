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
std::string savePath = "";
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

    if (myBox->clickedButton() == okBtn)
    {
        return false;
    }
}
//to gray
void Function::ConvertToGray(std::vector<uint8_t>& imageData,size_t start,size_t end)
{
    for (int i = start; i < end; i += 3) {
         uint8_t gray = static_cast<uint8_t>(0.299 * imageData[i] + 0.587 * imageData[i + 1] + 0.114 * imageData[i + 2]);
         imageData[i] = gray;
         imageData[i + 1] = gray;
         imageData[i + 2] = gray;
    }
}

uint32_t Function::GetSum(const std::vector<uint8_t> &imageData)
{
    return std::accumulate(imageData.begin(), imageData.end(), 0);
}
double Function::CalAver(const std::vector<uint8_t> &imageData)
{
    uint32_t sum = GetSum(imageData);
    return sum / imageData.size();
}

double Function::CalStandard(const std::vector<uint8_t> &imageData, double aver)
{
    float_t tempSum = 0.0;
    for (uint8_t i : imageData) {
        tempSum += (i - aver) * (i - aver);
    }
    float_t variance = tempSum / static_cast<float_t>(imageData.size());
    return std::sqrt(variance);
    double_t standard = std::sqrt(variance);
    return standard;
}
void Function::AutoContrast(std::vector<uint8_t> &imageData, float_t aver, float_t standard, size_t start, size_t end)
{
                                float_t factor = 128.0 / standard;

                                    for (size_t i = start; i < end; i += 3) {
                                        imageData[i] = std::max(0, std::min(255, static_cast<int32_t>(factor * (imageData[i] - aver) + 128)));
                                        imageData[i + 1] = std::max(0, std::min(255, static_cast<int32_t>(factor * (imageData[i + 1] - aver) + 128)));
                                        imageData[i + 2] = std::max(0, std::min(255, static_cast<int32_t>(factor * (imageData[i + 2] - aver) + 128)));
                                    }
}

void Function::AverageBlur(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height,size_t start ,size_t end)
{
    for (size_t i = start; i < end; i += 3) {
        size_t y = i / (width * 3);
        size_t x = (i / 3) % width;
        size_t index = i;

        uint32_t sum = 0;
        size_t count = 0;

        for (int8_t dy = -1; dy <= 1; dy++) {
            for (int8_t dx = -1; dx <= 1; dx++) {
                if (y + dy >= 0 && y + dy < height && x + dx >= 0 && x + dx < width) {
                    size_t neighborIndex = ((y + dy) * width + (x + dx)) * 3;
                    sum += imageData[neighborIndex];
                    count++;
                }
            }
        }

        uint8_t averagePixel = static_cast<uint8_t>(sum / count);
        imageData[index] = averagePixel;
    }
}




void Function::Brightness(std::vector<uint8_t> &brightnessImageData, double_t brightnessValue)
{
    if (brightnessValue >= -150 && brightnessValue <= 150) {
        for (unsigned char &i: brightnessImageData) {
            double_t newValue = static_cast<double_t>(i) + brightnessValue;
            if (newValue < 0) {
                newValue = 0;
            } else if (newValue > 255) {
                newValue = 255;
            }
            i = static_cast<uint8_t>(newValue);
        }
    } else {
        std::cout << "out of range,brightnessValue is between -150 to 150, please try again!" << std::endl;
        if(!CreateMessagebox("提示","超出范围,范围是[-150,150]"))
        {
            return;
        }

    }
}

void Function::Contrast(std::vector<uint8_t> &contrastImageData, double_t contrastValue)
{
    if (contrastValue >= -50 && contrastValue <= 100) {
        double_t factor = (100.0 + contrastValue) / 100.0;
        for (size_t i = 0; i < contrastImageData.size(); i += 3) {

            contrastImageData[i] = std::max(0, std::min(255,
                                                        static_cast<int>(factor * (contrastImageData[i] - 128) + 128)));
            contrastImageData[i + 1] = std::max(0, std::min(255,
                                                            static_cast<int>(factor * (contrastImageData[i + 1] - 128) +
                                                            128)));
            contrastImageData[i + 2] = std::max(0, std::min(255,
                                                            static_cast<int>(factor * (contrastImageData[i + 2] - 128) +
                                                            128)));
        }

    } else {
        std::cout << "out of range,brightnessValue is between -150 to 150, please try again!" << std::endl;
        if(!CreateMessagebox("提示","超出范围,范围是[-50,100]"))
        {
            return;
        }
    }
}

void Function::Saturation(std::vector<uint8_t> &saturationImageData, int32_t width, int32_t height, double_t saturationValue)
{
    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            int index = (i * width + j) * 3;
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
        r = g = b = static_cast<uint8_t>(v);
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
    double minVal = std::min({r, g, b});
    double maxVal = std::max({r, g, b});
    double delta = maxVal - minVal;

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
    // count
    double_t totalRed = 0.0;
    double_t totalGreen = 0.0;
    double_t totalBlue = 0.0;

    // for_each pix
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int index = (i * width + j) * 3;
            uint8_t r = imageData[index];
            uint8_t g = imageData[index + 1];
            uint8_t b = imageData[index + 2];
            totalRed += static_cast<double_t>(r);
            totalGreen += static_cast<double_t>(g);
            totalBlue += static_cast<double_t>(b);
        }
    }

    // avg
    double_t avgRed = totalRed / (width * height);
    double_t avgGreen = totalGreen / (width * height);
    double_t avgBlue = totalBlue / (width * height);

    // factor
    double_t redFactor = avgRed > 0.0 ? avgRed / 255.0 : 1.0;
    double_t greenFactor = avgGreen > 0.0 ? avgGreen / 255.0 : 1.0;
    double_t blueFactor = avgBlue > 0.0 ? avgBlue / 255.0 : 1.0;

    // adjust
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int index = (i * width + j) * 3;
            uint8_t r = imageData[index];
            uint8_t g = imageData[index + 1];
            uint8_t b = imageData[index + 2];

            // adjust new pix by factor
            r = static_cast<uint8_t>(r * redFactor);
            g = static_cast<uint8_t>(g * greenFactor);
            b = static_cast<uint8_t>(b * blueFactor);

            //update pix
            imageData[index] = r;
            imageData[index + 1] = g;
            imageData[index + 2] = b;
        }
    }
}


void Function::InvertColors(std::vector<uint8_t>& imageData,size_t start,size_t end) {
    for (int i = start; i < end; i += 3) {
        imageData[i] = 255 - imageData[i];
        imageData[i + 1] = 255 - imageData[i + 1];
        imageData[i + 2] = 255 - imageData[i + 2];
    }
}

void Function::Complementary(std::vector<uint8_t> &imageData,size_t start,size_t end)
{
    for (size_t i = start; i < end; i += 3) {
        size_t r = imageData[i];
        size_t g = imageData[i + 1];
        size_t b = imageData[i + 2];
        size_t maxRgb = std::max(std::max(r, g), b);
        size_t minRgb = std::min(std::min(r, g), b);
        imageData[i] = maxRgb + minRgb - r;
        imageData[i + 1] = maxRgb + minRgb - g;
        imageData[i + 2] = maxRgb + minRgb - b;
    }
}


void Function::Fisheye(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>>& result, int32_t width, int32_t height)
{
    // 首先确定图像的中心点坐标，以及最大半径
    int32_t centerX = width / 2;
    int32_t centerY = height / 2;
    int32_t maxRadius = std::min(centerX, centerY);

    std::vector<uint8_t> fisheyeImage(width * height * 3);
    // 遍历每个像素，计算像素到图像中心的距离，并检查是否小于最大半径，以确保像素位于鱼眼效果的范围内。
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int32_t offsetX = x - centerX;
            int32_t offsetY = y - centerY;
            float distance = std::sqrt(static_cast<float>(offsetX * offsetX + offsetY * offsetY));
            // 小于=应用鱼眼
            if (distance < maxRadius) {
                // 当前像素相对于图像中心点的极坐标角度
                float angle = std::atan2(static_cast<float>(offsetY), static_cast<float>(offsetX));
                // 用于计算每个像素的新半径
                float normalizedRadius = distance / maxRadius;
                // 进行非线性变换
                float mappedRadius = normalizedRadius * normalizedRadius;

                // 极坐标变换
                int32_t newX = static_cast<int32_t>(centerX + maxRadius * mappedRadius * std::cos(angle));
                int32_t newY = static_cast<int32_t>(centerY + maxRadius * mappedRadius * std::sin(angle));

                for (int channel = 0; channel < 3; ++channel) {
                    fisheyeImage[(y * width + x) * 3 + channel] = imageData[(newY * width + newX) * 3 + channel];
                }
            }
            else {
                // 超出范围保持原始颜色
                for (int channel = 0; channel < 3; ++channel) {
                    fisheyeImage[(y * width + x) * 3 + channel] = 0;
                }
            }
        }
    }

    result.set_value(fisheyeImage);
}

double Function::Gaussian(double sigma, int x, int y)
{
 return exp(-(x * x + y * y) / (2.0 * sigma * sigma)) / (2.0 * M_PI * sigma * sigma);
}

void Function::Gauss(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result,int width, int height, double sigma)
{
    std::vector<uint8_t> blurImageData(imageData.size());

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            float_t r = 0.0;
            float_t g = 0.0;
            float_t b = 0.0;
            float_t weightSum = 0.0;

            for (int8_t j = -2; j <= 2; j++) {
                for (int8_t i = -2; i <= 2; i++) {
                    uint16_t pixelX = x + i;
                    uint16_t pixelY = y + j;
                    if (pixelX >= 0 && pixelX < width && pixelY >= 0 && pixelY < height) {
                        uint32_t pixelIndex = (pixelY * width + pixelX) * 3;
                        float_t weight = Gaussian(sigma, i, j);
                        r += static_cast<float_t>(imageData[pixelIndex]) * weight;
                        g += static_cast<float_t>(imageData[pixelIndex + 1]) * weight;
                        b += static_cast<float_t>(imageData[pixelIndex + 2]) * weight;
                        weightSum += weight;
                    }
                }
            }

            uint32_t index = (y * width + x) * 3;
            blurImageData[index] = static_cast<uint8_t>(r / weightSum);
            blurImageData[index + 1] = static_cast<uint8_t>(g / weightSum);
            blurImageData[index + 2] = static_cast<uint8_t>(b / weightSum);
        }
    }

    result.set_value(blurImageData);
}

void Function::HighContrast(const std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>> &result,const std::vector<uint8_t> &blurImageData)
{
    std::vector<uint8_t> highContrastImageData(imageData.size());
    for (size_t i = 0; i < imageData.size(); i++) {
        uint32_t between = static_cast<uint32_t>(imageData[i]) - static_cast<uint32_t>(blurImageData[i]);
        highContrastImageData[i] = static_cast<uint8_t>(between + 128);  // 调整到 0-255 范围
    }

    result.set_value(highContrastImageData);
}

void Function::RotateImage(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle)
{
    double_t radians = angle * M_PI / 180.0;
       std::vector<uint8_t> rotatedImageData(width * height * 3);

       int32_t centerX = width / 2;
       int32_t centerY = height / 2;

       for (int y = 0; y < height; ++y) {
           for (int x = 0; x < width; ++x) {
               double_t rotatedX = std::cos(radians) * (x - centerX) - std::sin(radians) * (y - centerY) + centerX;
               double_t rotatedY = std::sin(radians) * (x - centerX) + std::cos(radians) * (y - centerY) + centerY;

               if (rotatedX >= 0 && rotatedX < width && rotatedY >= 0 && rotatedY < height) {
                   int originalIndex =
                           static_cast<int>(std::round(rotatedY)) * width * 3 + static_cast<int>(std::round(rotatedX)) * 3;
                   int newIndex = y * width * 3 + x * 3;

                   if (originalIndex >= 0 && originalIndex < width * height * 3 && newIndex >= 0 && newIndex < width * height * 3) {
                       rotatedImageData[newIndex] = imageData[originalIndex];
                       rotatedImageData[newIndex + 1] = imageData[originalIndex + 1];
                       rotatedImageData[newIndex + 2] = imageData[originalIndex + 2];
                   }
               }
           }
       }

       imageData = rotatedImageData;

}


void Function::RotateReverse(std::vector<uint8_t> &imageData, int32_t width, int32_t height, double_t angle)
{
    double_t clockwiseAngle = 360.0 - angle;
    RotateImage(imageData, width, height, clockwiseAngle);
}

uint8_t Function::CalculateMedian(std::vector<uint8_t> &window)
{
    std::sort(window.begin(), window.end());
    return window[window.size() / 2];

}

void Function::MedianBlur(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height)
{
    std::vector<uint8_t> blurImage(imageData);
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            std::vector<uint8_t> window;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t index = ((y + dy) * width + (x + dx)) * 3;
                    window.push_back(imageData[index]);
                }
            }
            uint32_t index = (y * width + x) * 3;
            imageData[index] = CalculateMedian(window);
        }
    }
}

void Function::MakeShadow(std::vector<uint8_t> &imageData, std::vector<uint8_t> &shadowImageData, uint32_t shadowValue)
{
    for (size_t i=0; i < imageData.size(); i += 3) {
        uint8_t r = imageData[i];
        uint8_t g = imageData[i + 1];
        uint8_t b = imageData[i + 2];
        shadowImageData[i] = (r >= shadowValue) ? r - shadowValue : 0;
        shadowImageData[i + 1] = (g >= shadowValue) ? g - shadowValue : 0;
        shadowImageData[i + 2] = (b >= shadowValue) ? b - shadowValue : 0;
    }
}

void Function::HighLight(std::vector<uint8_t> &imageData, std::vector<uint8_t> &highLightImageData, uint32_t pixel)
{
    // 确保 highLightImageData 的大小与 imageData 一致
    if (highLightImageData.size() != imageData.size()) {
        // 处理大小不一致的情况，这里简单示范直接将 highLightImageData 大小设置为 imageData 大小
        highLightImageData.resize(imageData.size());
    }

    for (size_t i = 0; i < imageData.size(); ++i) {
        // 对所有像素进行处理，不仅仅是大于 pixel 的像素
        int newValue = imageData[i] - 100;
        // 确保颜色值不会小于0
        highLightImageData[i] = (newValue >= 0) ? static_cast<uint8_t>(newValue) : 0;
    }
}

void Function::Sharpen(const std::vector<uint8_t> &imageData, std::promise<std::vector<uint8_t>> &result,const std::vector<uint8_t> &highContrastImageData)
{
    std::vector<uint8_t> sharpenImageData(imageData.size());

    for (size_t i = 0; i < imageData.size(); i++) {
        int addValue = static_cast<int>(imageData[i]) + static_cast<int>(highContrastImageData[i]);
        sharpenImageData[i] = static_cast<uint8_t>(std::max(std::min(addValue - 170, 255), 0));
    }

    result.set_value(sharpenImageData);
}

std::vector<uint8_t> Function::SobelEdge(const std::vector<uint8_t> &imageData, int width, int height)
{
    std::vector<uint8_t> edgeImageData(imageData.size());

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int sumX = 0;
            int sumY = 0;

            for (int j = 0; j < 3; j++) {
                for (int i = 0; i < 3; i++) {
                    int pixelX = x + i - 1;
                    int pixelY = y + j - 1;
                    int pixelIndex = (pixelY * width + pixelX) * 3;
                    int grayValue = static_cast<int>(imageData[pixelIndex]);
                    sumX += grayValue * sobelX[j][i];
                    sumY += grayValue * sobelY[j][i];
                }
            }
            //edge
            //梯度的强度，通常是水平和垂直梯度的绝对值之和。这个值用来表示像素的边缘强度。
            int edgeValue = std::min(std::max(std::abs(sumX) + std::abs(sumY), 0), 255);
            int index = (y * width + x) * 3;
            edgeImageData[index] = static_cast<uint8_t>(edgeValue);
            edgeImageData[index + 1] = static_cast<uint8_t>(edgeValue);
            edgeImageData[index + 2] = static_cast<uint8_t>(edgeValue);
        }
    }

    return edgeImageData;
}

void Function::ApplyThreshold(std::vector<uint8_t> &imageData, uint32_t threshold,size_t start,size_t end)
{
    for (size_t i = start; i < end; i += 3) {
           uint8_t r = imageData[i];
           uint8_t g = imageData[i + 1];
           uint8_t b = imageData[i + 2];

           // 计算灰度值并使用double类型保存
           float_t gray = 0.299 * r + 0.587 * g + 0.114 * b;

           if (gray > threshold) {
               // 使用比例因子来调整颜色通道值
               float_t factor = (gray - threshold) / (255.0 - threshold);
               imageData[i] = std::min(255, static_cast<int>(r + 100 * factor));
               imageData[i + 1] = std::min(255, static_cast<int>(g + 100 * factor));
               imageData[i + 2] = std::min(255, static_cast<int>(b + 100 * factor));
           }
           else {
               // 使用比例因子来调整颜色通道值
               float_t factor = gray / threshold;
               imageData[i] = std::max(0, static_cast<int>(r * factor));
               imageData[i + 1] = std::max(0, static_cast<int>(g * factor));
               imageData[i + 2] = std::max(0, static_cast<int>(b * factor));
           }
       }
}




