#include "interpolation.h"
#include "ui_interpolation.h"

Interpolation::Interpolation(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Interpolation), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);

    imageData = mainWindow->CurrentImageData();
    newValue = myValue;
    Function function;

}

Interpolation::~Interpolation()
{
    if (newWindow) {
        newWindow->close();
        delete newWindow;
        newWindow = nullptr;
    }
    delete ui;
}

void Interpolation::ShowImage(const std::vector<uint8_t>& imageData, int32_t width, int32_t height)
{
    if (!newWindow) {
        // 创建新的窗口对象(只创建一次，重复调用时复用)
        newWindow = new QMainWindow();
        newWindow->setWindowTitle(QString("图像缩放"));
        newWindow->setWindowIcon(QIcon(":/icon/logo.png"));
        newWindow->resize(std::max(640, width), std::max(480, height));
    }

    // 创建一个 QLabel 来显示图像
    QLabel* imageLabel = new QLabel(newWindow);
    const QImage newImage = Function::ToDisplayImage(imageData, width, height);

    // 设置 QLabel 的图像
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setPixmap(QPixmap::fromImage(newImage));

    // 设置新窗口的中心部件为 QLabel
    newWindow->setCentralWidget(imageLabel);

    // 显示新窗口
    newWindow->show();
    newWindow->raise();
}


std::vector<uint8_t> Interpolation::LargeImage_Nearest(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newHeight) * newWidth * 3);

    // 放大因子
    //计算缩小因子 scaleX 和 scaleY，scaleX 和 scaleY 是水平和垂直方向的缩放因子。
    const double scaleX = static_cast<double>(newWidth) / width;
    const double scaleY = static_cast<double>(newHeight) / height;

    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            //x 和 y 是新图像中的像素坐标。srcX 和 srcY 是通过缩放因子计算得到的原始图像中最近邻的像素坐标。
            const auto srcX = std::max(0, std::min(width - 1, static_cast<int32_t>(x / scaleX)));
            const auto srcY = std::max(0, std::min(height - 1, static_cast<int32_t>(y / scaleY)));
            const size_t srcIndex = (static_cast<size_t>(srcY) * width + srcX) * 3;
            const size_t destIndex = (static_cast<size_t>(y) * newWidth + x) * 3;
            // 复制到目标
            resizedImage[destIndex] = imageData[srcIndex];
            resizedImage[destIndex + 1] = imageData[srcIndex + 1];
            resizedImage[destIndex + 2] = imageData[srcIndex + 2];
        }
    }

    return resizedImage;
}


std::vector<uint8_t> Interpolation::SmallImage_Nearest(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newWidth) * newHeight * 3);

    // 计算缩放因子
    const double scaleX = static_cast<double>(width) / newWidth;
    const double scaleY = static_cast<double>(height) / newHeight;

    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            const auto srcX = std::max(0, std::min(width - 1, static_cast<int32_t>(x * scaleX)));
            const auto srcY = std::max(0, std::min(height - 1, static_cast<int32_t>(y * scaleY)));
            // 复制到目标
            const size_t srcIndex = (static_cast<size_t>(srcY) * width + srcX) * 3;
            const size_t destIndex = (static_cast<size_t>(y) * newWidth + x) * 3;
            resizedImage[destIndex] = imageData[srcIndex];
            resizedImage[destIndex + 1] = imageData[srcIndex + 1];
            resizedImage[destIndex + 2] = imageData[srcIndex + 2];
        }
    }

    return resizedImage;

}

std::vector<uint8_t> Interpolation::
LargeImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight) {
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newWidth) * newHeight * 3);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            auto srcX = static_cast<float>(x) * width / newWidth;
            auto srcY = static_cast<float>(y) * height / newHeight;

            srcX = std::max(0.0f, std::min(srcX, static_cast<float>(width - 1)));
            srcY = std::max(0.0f, std::min(srcY, static_cast<float>(height - 1)));


            float weights[4][4];

            // 权重(统一使用 0.5 的中心偏移，和缩小路径保持一致)
            for (int j = -1; j <= 2; ++j) {
                for (int i = -1; i <= 2; ++i) {
                    int xi = static_cast<int>(std::floor(srcX)) + i;
                    int yj = static_cast<int>(std::floor(srcY)) + j;
                    xi = std::max(0, std::min(xi, width - 1));
                    yj = std::max(0, std::min(yj, height - 1));
                    const float wx = cubicWeight(srcX - (xi + 0.5f));
                    const float wy = cubicWeight(srcY - (yj + 0.5f));
                    weights[j + 1][i + 1] = wx * wy;
                }
            }

            // 插值
            for (int channel = 0; channel < 3; ++channel) {
                float interpolatedValue = 0.0f;
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        const int xi = std::max(0, std::min(width - 1, static_cast<int>(std::floor(srcX)) + i - 1));
                        const int yj = std::max(0, std::min(height - 1, static_cast<int>(std::floor(srcY)) + j - 1));
                        interpolatedValue += weights[j][i] * imageData[(static_cast<size_t>(yj) * width + xi) * 3 + channel];
                    }
                }
                //1.由于像素值通常在 0 到 255 之间，所以使用 std::max(0.0f, std::min(255.0f, interpolatedValue)) 将插值后的值截取到这个范围内。
                //2.将截取后的插值像素值转换为 uint8_t 类型，并存储在 resizedImage 中的相应位置。
                interpolatedValue = std::max(0.0f, std::min(255.0f, interpolatedValue));
                resizedImage[(static_cast<size_t>(y) * newWidth + x) * 3 + channel] = static_cast<uint8_t>(interpolatedValue);
            }
        }
    }
    return resizedImage;
}

std::vector<uint8_t> Interpolation::
SmallImage_BiCubic(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight) {
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newWidth) * newHeight * 3);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            auto srcX = static_cast<float>(x) * width / newWidth;
            auto srcY = static_cast<float>(y) * height / newHeight;
            //接下来需要对 (srcX, srcY) 进行边界检查，确保它在原图像的有效范围内。这是为了防止插值过程中访问到图像边界外的无效像素。
            srcX = std::max(0.0f, std::min(srcX, static_cast<float>(width - 1)));
            srcY = std::max(0.0f, std::min(srcY, static_cast<float>(height - 1)));

            float weights[4][4];
            //权重(与放大路径统一使用 0.5 中心偏移)
            for (int j = -1; j <= 2; ++j) {
                for (int i = -1; i <= 2; ++i) {
                    int xi = static_cast<int>(std::floor(srcX)) + i;
                    int yj = static_cast<int>(std::floor(srcY)) + j;

                    xi = std::max(0, std::min(xi, width - 1));
                    yj = std::max(0, std::min(yj, height - 1));

                    const float wx = cubicWeight(srcX - (xi + 0.5f));
                    const float wy = cubicWeight(srcY - (yj + 0.5f));
                    weights[j + 1][i + 1] = wx * wy;
                }
            }


            // 插值
            for (int channel = 0; channel < 3; ++channel) {
                float interpolatedValue = 0.0f;
                for (int j = 0; j < 4; ++j) {
                    for (int i = 0; i < 4; ++i) {
                        const int xi = std::max(0, std::min(width - 1, static_cast<int>(std::floor(srcX)) + i - 1));
                        const int yj = std::max(0, std::min(height - 1, static_cast<int>(std::floor(srcY)) + j - 1));
                        interpolatedValue += weights[j][i] * imageData[(static_cast<size_t>(yj) * width + xi) * 3 + channel];
                    }
                }

                interpolatedValue = std::max(0.0f, std::min(255.0f, interpolatedValue));
                resizedImage[(static_cast<size_t>(y) * newWidth + x) * 3 + channel] = static_cast<uint8_t>(interpolatedValue);
            }
        }
    }
    return resizedImage;
}

std::vector<uint8_t> Interpolation::SmallImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newWidth) * newHeight * 3);
    // 计算缩小因子
    const double scaleX = static_cast<double>(width) / newWidth;
    const double scaleY = static_cast<double>(height) / newHeight;
    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            const auto srcX = x * scaleX;
            const auto srcY = y * scaleY;
            // 计算最近的像素点坐标
            const auto x1 = std::max(0, std::min(static_cast<int32_t>(srcX), width - 1));
            const auto x2 = std::max(0, std::min(static_cast<int32_t>(x1 + 1), width - 1));
            const auto y1 = std::max(0, std::min(static_cast<int32_t>(srcY), height - 1));
            const auto y2 = std::max(0, std::min(static_cast<int32_t>(y1 + 1), height - 1));
            // 权重 计算目标像素的权重 w1、w2、w3 和 w4，这些权重表示了目标像素与最近的四个原始像素之间的关系。
            const auto tx = srcX - x1;
            const auto ty = srcY - y1;
            const auto w1 = (1.0 - tx) * (1.0 - ty);
            const auto w2 = tx * (1.0 - ty);
            const auto w3 = (1.0 - tx) * ty;
            const auto w4 = tx * ty;
            // 新像素值
            //双线性插值考虑了目标像素与最近的四个原始像素之间的权重关系，计算了四个权重 w1、w2、w3 和 w4。
            const size_t destIndex = (static_cast<size_t>(y) * newWidth + x) * 3;
            const size_t srcIndex1 = (static_cast<size_t>(y1) * width + x1) * 3;
            const size_t srcIndex2 = (static_cast<size_t>(y1) * width + x2) * 3;
            const size_t srcIndex3 = (static_cast<size_t>(y2) * width + x1) * 3;
            const size_t srcIndex4 = (static_cast<size_t>(y2) * width + x2) * 3;
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

std::vector<uint8_t> Interpolation::LargeImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(static_cast<size_t>(newWidth) * newHeight * 3);

    // 计算放大因子
    const double scaleX = static_cast<double>(newWidth) / width;
    const double scaleY = static_cast<double>(newHeight) / height;
    for (int32_t y = 0; y < newHeight; y++)
    {
        for (int32_t x = 0; x < newWidth; x++)
        {
            // 原图坐标
            const auto srcX = x / scaleX;
            const auto srcY = y / scaleY;
            // 计算最近的像素点坐标
            const auto x1 = std::max(0, std::min(static_cast<int32_t>(srcX), width - 1));
            const auto x2 = std::max(0, std::min(static_cast<int32_t>(x1 + 1), width - 1));
            const auto y1 = std::max(0, std::min(static_cast<int32_t>(srcY), height - 1));
            const auto y2 = std::max(0, std::min(static_cast<int32_t>(y1 + 1), height - 1));

            // 权重
            const auto tx = srcX - x1;
            const auto ty = srcY - y1;
            const auto w1 = (1.0 - tx) * (1.0 - ty);
            const auto w2 = tx * (1.0 - ty);
            const auto w3 = (1.0 - tx) * ty;
            const auto w4 = tx * ty;
            // 新像素值
            //双线性插值考虑了目标像素与最近的四个原始像素之间的权重关系，计算了四个权重 w1、w2、w3 和 w4。
            const size_t destIndex = (static_cast<size_t>(y) * newWidth + x) * 3;
            const size_t srcIndex1 = (static_cast<size_t>(y1) * width + x1) * 3;
            const size_t srcIndex2 = (static_cast<size_t>(y1) * width + x2) * 3;
            const size_t srcIndex3 = (static_cast<size_t>(y2) * width + x1) * 3;
            const size_t srcIndex4 = (static_cast<size_t>(y2) * width + x2) * 3;
            for (int32_t channel = 0; channel < 3; channel++)
            {
                resizedImage[destIndex + channel] = static_cast<uint8_t>(
                            w1 * imageData[srcIndex1 + channel] +
                        w2 * imageData[srcIndex2 + channel] +
                        w3 * imageData[srcIndex3 + channel] +
                        w4 * imageData[srcIndex4 + channel]);
            }
        }
    }
    return resizedImage;
}

float Interpolation::cubicWeight(float t)
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


// 统一的保存流程：询问 -> 写头信息 -> 落盘
bool Interpolation::AskAndSave(const std::vector<uint8_t> &result, int32_t newWidth, int32_t newHeight)
{
    const QMessageBox::StandardButton reply =
        QMessageBox::question(this, "确认", "是否保存文件?", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        qDebug() << "用户取消操作";
        return false;
    }

    MYFunction::SetBMPHeaderValues(newValue.bmp, newValue.bmpInfo, newWidth, newHeight,
                                    newValue.bmpInfo.GetBitsPerPixel());
    QString filePath = QFileDialog::getSaveFileName(nullptr, "保存文件", "", "BMP文件(*.bmp)");
    if (filePath.isEmpty()) {
        return false;
    }
    if (!filePath.endsWith(".bmp", Qt::CaseInsensitive)) {
        filePath += ".bmp";
    }
    // 直接使用完整路径写文件(不再依赖全局 savePath)
    return MYFunction::WriteBMPFile(filePath.toStdString(), result, newValue.bmp, newValue.bmpInfo);
}


void Interpolation::ShowLargeNear(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> nearestImageData = imageData;
    const int32_t newWidth = newValue.bmpInfo.GetWidth() * width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() * height;
    const std::vector<uint8_t> largeImageData =
        LargeImage_Nearest(nearestImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    ShowImage(largeImageData, newWidth, newHeight);
    qDebug() << "ok";
    AskAndSave(largeImageData, newWidth, newHeight);
}

void Interpolation::ShowSmallNear(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> nearestImageData = imageData;
    if ((newValue.bmpInfo.GetWidth() % width) != 0 || (newValue.bmpInfo.GetHeight() % height) != 0) {
        qDebug() << "not a integer";
        function.CreateMessagebox("提示", "输出图像长宽非整数!!");
        return;
    }
    const int32_t newWidth = newValue.bmpInfo.GetWidth() / width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() / height;
    const std::vector<uint8_t> smallImageData =
        SmallImage_Nearest(nearestImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    ShowImage(smallImageData, newWidth, newHeight);
    qDebug() << "ok";
    AskAndSave(smallImageData, newWidth, newHeight);
}


void Interpolation::ShowSmallBilinear(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> bilinearImageData = imageData;
    if ((newValue.bmpInfo.GetWidth() % width) != 0 || (newValue.bmpInfo.GetHeight() % height) != 0) {
        qDebug() << "not a integer";
        function.CreateMessagebox("提示", "输出图像长宽非整数!!");
        return;
    }
    const int32_t newWidth = newValue.bmpInfo.GetWidth() / width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() / height;
    // 缩小必须走缩小版本的双线性插值(之前误用了放大函数)
    const std::vector<uint8_t> smallImageData =
        SmallImage_Bilinear(bilinearImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    ShowImage(smallImageData, newWidth, newHeight);
    AskAndSave(smallImageData, newWidth, newHeight);
}

void Interpolation::ShowLargeBilinear(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> bilinearImageData = imageData;
    const int32_t newWidth = newValue.bmpInfo.GetWidth() * width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() * height;
    const std::vector<uint8_t> largeImageData =
        LargeImage_Bilinear(bilinearImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    std::cout << "largeImageData size: " << largeImageData.size() << std::endl;
    ShowImage(largeImageData, newWidth, newHeight);
    AskAndSave(largeImageData, newWidth, newHeight);
}


void Interpolation::ShowSmallBicubic(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> biCubicImageData = imageData;
    if ((newValue.bmpInfo.GetWidth() % width) != 0 || (newValue.bmpInfo.GetHeight() % height) != 0) {
        qDebug() << "not a integer";
        function.CreateMessagebox("提示", "输出图像长宽非整数!!");
        return;
    }
    const int32_t newWidth = newValue.bmpInfo.GetWidth() / width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() / height;
    const std::vector<uint8_t> smallImageData =
        SmallImage_BiCubic(biCubicImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    ShowImage(smallImageData, newWidth, newHeight);
    AskAndSave(smallImageData, newWidth, newHeight);
}


void Interpolation::ShowLargeBicubic(int height,int width)
{
    if (width <= 0 || height <= 0) {
        function.CreateMessagebox("提示", "缩放倍数必须大于 0");
        return;
    }
    const std::vector<uint8_t> biCubicImageData = imageData;
    const int32_t newWidth = newValue.bmpInfo.GetWidth() * width;
    const int32_t newHeight = newValue.bmpInfo.GetHeight() * height;
    const std::vector<uint8_t> largeImageData =
        LargeImage_BiCubic(biCubicImageData, newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), newWidth, newHeight);
    ShowImage(largeImageData, newWidth, newHeight);
    AskAndSave(largeImageData, newWidth, newHeight);
}
