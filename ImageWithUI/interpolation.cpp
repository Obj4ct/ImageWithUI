#include "interpolation.h"
#include "ui_interpolation.h"

Interpolation::Interpolation(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Interpolation), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);

    imageData=mainWindow->imageData;
    newValue=mainWindow->myValue;
    Function function;

}

void Interpolation::ShowImage(const std::vector<uint8_t>& imageData, int32_t width, int32_t height)
{
    // 创建新的窗口对象
    newWindow = new QMainWindow();
    newWindow->setWindowTitle(QString("图像缩放"));
    newWindow->setWindowIcon(QIcon(":/icon/logo.png"));

    // 创建一个 QLabel 来显示图像
    QLabel* imageLabel = new QLabel(newWindow);

    // 使用 QImage 加载图像数据
    QImage newImage(imageData.data(), width, height, QImage::Format_BGR888);
    newImage = newImage.mirrored(false, true);

    // 设置 QLabel 的图像
    imageLabel->setPixmap(QPixmap::fromImage(newImage));

    // 设置新窗口的中心部件为 QLabel
    newWindow->setCentralWidget(imageLabel);

    // 添加保存按钮
    QPushButton* saveButton = new QPushButton("保存", newWindow);
    connect(saveButton, &QPushButton::clicked, [=]() {
        SaveImageToFile(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    });


    // 设置新窗口的底部工具栏
    QToolBar* toolBar = newWindow->addToolBar("SaveToolBar");
    toolBar->addWidget(saveButton);

    // 显示新窗口
    newWindow->show();
}


std::vector<uint8_t> Interpolation::LargeImage_Nearest(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(newHeight * newWidth * 3);

       // 放大因子
       double scaleX = static_cast<double>(newWidth) / width;
       double scaleY = static_cast<double>(newHeight) / height;

       for (int32_t y = 0; y < newHeight; y++) {
           for (int32_t x = 0; x < newWidth; x++) {
               // 原图坐标
               auto srcX = static_cast<int32_t>(x / scaleX);
               auto srcY = static_cast<int32_t>(y / scaleY);
               int32_t srcIndex = (srcY * width + srcX) * 3;
               int32_t destIndex = (y * newWidth + x) * 3;
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
    std::vector<uint8_t> resizedImage(newWidth*newHeight*3);

    // 计算缩放因子
    double scaleX = static_cast<double>(width) / newWidth;
    double scaleY = static_cast<double>(height) / newHeight;

    for (int32_t y = 0; y < newHeight; y++) {
        for (int32_t x = 0; x < newWidth; x++) {
            // 原图坐标
            auto srcX = static_cast<int32_t>(x * scaleX);
            auto srcY = static_cast<int32_t>(y * scaleY);
            // 复制到目标
            int32_t srcIndex = (srcY * width + srcX) * 3;
            int32_t destIndex = (y * newWidth + x) * 3;
            resizedImage[destIndex] = imageData[srcIndex];
            resizedImage[destIndex + 1] = imageData[srcIndex + 1];
            resizedImage[destIndex + 2] = imageData[srcIndex + 2];
        }
    }

    return resizedImage;

}

std::vector<uint8_t> Interpolation::
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
                    float wx = cubicWeight(srcX - (xi + 0.9f));
                    float wy = cubicWeight(srcY - (yj + 0.9f));
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

std::vector<uint8_t> Interpolation::
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

std::vector<uint8_t> Interpolation::SmallImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
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
            // 计算最近的像素点坐标
            //            auto x1 = static_cast<int32_t>(srcX);
            //            auto x2 = static_cast<int32_t>(x1 + 1);
            //            auto y1 = static_cast<int32_t>(srcY);
            //            auto y2 = static_cast<int32_t>(y1 + 1);
            auto x1 = std::max(0, std::min(static_cast<int32_t>(srcX), width - 1));
            auto x2 = std::max(0, std::min(static_cast<int32_t>(x1 + 1), width - 1));
            auto y1 = std::max(0, std::min(static_cast<int32_t>(srcY), height - 1));
            auto y2 = std::max(0, std::min(static_cast<int32_t>(y1 + 1), height - 1));
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

std::vector<uint8_t> Interpolation::LargeImage_Bilinear(const std::vector<uint8_t> &imageData, int32_t width, int32_t height, int32_t newWidth, int32_t newHeight)
{
    std::vector<uint8_t> resizedImage(newWidth * newHeight * 3);

    // 计算放大因子
    double scaleX = static_cast<double>(newWidth) / width;
    double scaleY = static_cast<double>(newHeight) / height;
    for (int32_t y = 0; y < newHeight; y++)
    {
        for (int32_t x = 0; x < newWidth; x++)
        {
            // 原图坐标
            auto srcX = x / scaleX;
            auto srcY = y / scaleY;
            // 计算最近的像素点坐标
            auto x1 = std::max(0, std::min(static_cast<int32_t>(srcX), width - 1));
            auto x2 = std::max(0, std::min(static_cast<int32_t>(x1 + 1), width - 1));
            auto y1 = std::max(0, std::min(static_cast<int32_t>(srcY), height - 1));
            auto y2 = std::max(0, std::min(static_cast<int32_t>(y1 + 1), height - 1));

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


void Interpolation::ShowLargeNear(int height,int width)
{
    std::vector<uint8_t> nearestImageData=imageData;
        std::vector<uint8_t> largeImageData=LargeImage_Nearest(nearestImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()*width,newValue.bmpInfo.GetHeight()*height);
        ShowImage(largeImageData, newValue.bmpInfo.GetWidth() * width, newValue.bmpInfo.GetHeight() * height);
        qDebug()<<"ok";

}

void Interpolation::ShowSmallNear(int height,int width)
{
    std::vector<uint8_t> nearestImageData=imageData;

        std::vector<uint8_t> smallImageData=SmallImage_Nearest(nearestImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()/width,newValue.bmpInfo.GetHeight()/height);
        ShowImage(smallImageData, newValue.bmpInfo.GetWidth() / width, newValue.bmpInfo.GetHeight() / height);
        qDebug()<<"ok";


}


void Interpolation::ShowSmallBilinear(int height,int width)
{
    std::vector<uint8_t> bilinearImageData=imageData;

        std::vector<uint8_t> smallImageData=LargeImage_Bilinear(bilinearImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()/width,newValue.bmpInfo.GetHeight()/height);
        ShowImage(smallImageData, newValue.bmpInfo.GetWidth() / width, newValue.bmpInfo.GetHeight() / height);

}

void Interpolation::ShowLargeBilinear(int height,int width)
{
    std::vector<uint8_t> bilinearImageData=imageData;
        std::vector<uint8_t> largeImageData=LargeImage_Bilinear(bilinearImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()*width,newValue.bmpInfo.GetHeight()*height);
        std::cout << "largeImageData size: " << largeImageData.size() << std::endl;
        ShowImage(largeImageData, newValue.bmpInfo.GetWidth() * width, newValue.bmpInfo.GetHeight() * height);

}


void Interpolation::ShowSmallBicubic(int height,int width)
{
    std::vector<uint8_t> biCubicImageData=imageData;

        std::vector<uint8_t> smallImageData=SmallImage_BiCubic(biCubicImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()/width,newValue.bmpInfo.GetHeight()/height);
        ShowImage(smallImageData, newValue.bmpInfo.GetWidth() / width, newValue.bmpInfo.GetHeight() / height);


}


void Interpolation::ShowLargeBicubic(int height,int width)
{
    std::vector<uint8_t> biCubicImageData=imageData;


        std::vector<uint8_t> largeImageData=LargeImage_BiCubic(biCubicImageData,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(),newValue.bmpInfo.GetWidth()*width,newValue.bmpInfo.GetHeight()*height);
        ShowImage(largeImageData, newValue.bmpInfo.GetWidth() * width, newValue.bmpInfo.GetHeight() * height);


}

void Interpolation::SaveImageToFile(const std::vector<uint8_t> &imageData, int32_t width, int32_t height)
{
    qDebug()<<"in save";
    QString filePath = QFileDialog::getSaveFileName(nullptr, "保存文件", "", "BMP文件(*.bmp)");
    if (filePath.isEmpty()) {
        qDebug() << "Save operation canceled.";
        return;
    }
    savePath=filePath.toStdString();

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    std::string str=fileName.toStdString();
    std::cout<<"filename is "<<str<<std::endl;
    MYFunction::WriteBMPFile(str,newValue.imageData,newValue.bmp,newValue.bmpInfo);
    qDebug()<<"succeed!";
}

