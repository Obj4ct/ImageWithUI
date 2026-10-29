#include "mainwindow.h"
#include "colormap.h"
#include "ui_colormap.h"
// colorMap1: 红色过渡
std::vector<uint8_t> colorMap1 = {
    255, 0, 50,    // 深红色
    255, 0, 100,   // 红色
    255, 0, 150,   // 浅红色
    255, 0, 200,   // 粉红色
    255, 0, 250    // 亮粉红色
};

// colorMap2: 青色过渡
std::vector<uint8_t> colorMap2 = {
    50, 255, 255,   // 深青色
    100, 255, 255,  // 青色
    150, 255, 255,  // 浅青色
    200, 255, 255,  // 靛青色
    250, 255, 255   // 亮青色
};

// colorMap3: 紫色过渡
std::vector<uint8_t> colorMap3 = {
    255, 50, 255,   // 深紫色
    255, 100, 255,  // 紫色
    255, 150, 255,  // 浅紫色
    255, 200, 255,  // 粉紫色
    255, 250, 255   // 亮粉紫色
};

// colorMap4: 灰色过渡
std::vector<uint8_t> colorMap4 = {
    50, 50, 50,     // 深灰色
    100, 100, 100,  // 灰色
    150, 150, 150,  // 浅灰色
    200, 200, 200,  // 亮灰色
    250, 250, 250   // 亮亮灰色
};




ColorMap::ColorMap(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::ColorMap), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    setWindowTitle(QString("色彩映射"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    imageData = mainWindow->imageData;
    newValue = myValue;
    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    // 显示原始图像在imageLabel上
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));

    Function function;

    // 设置下拉框的初始状态
    ui->comboBox_colorMap->addItem("<请选择>");
    ui->comboBox_colorMap->addItem("模式1");
    ui->comboBox_colorMap->addItem("模式2");
    ui->comboBox_colorMap->addItem("模式3");
    ui->comboBox_colorMap->addItem("模式4");
}


ColorMap::~ColorMap()
{
    delete ui;
}

// 按灰度把像素映射到颜色表
void ColorMap::_ColorMap(std::vector<uint8_t> &imageData, std::vector<uint8_t> &colorMap)
{
    //计算颜色映射表中颜色的数量,3表示channel
    const int numColors = static_cast<int>(colorMap.size() / 3);
    if (numColors <= 0) {
        return;
    }
    for (size_t i = 0; i + 2 < imageData.size(); i += 3) {
        // 用完整的灰度值(而不是单一通道)作为映射依据
        const double gray = 0.299 * imageData[i] + 0.587 * imageData[i + 1] + 0.114 * imageData[i + 2];
        //先归一化然后执行 乘法
        int index = static_cast<int>(gray / 255.0 * (numColors - 1));
        index = std::max(0, std::min(numColors - 1, index));

        imageData[i] = colorMap[static_cast<size_t>(index) * 3];
        imageData[i + 1] = colorMap[static_cast<size_t>(index) * 3 + 1];
        imageData[i + 2] = colorMap[static_cast<size_t>(index) * 3 + 2];
    }

}

void ColorMap::ShowImage(std::vector<uint8_t> &inImageData)
{
    const QImage image = Function::ToDisplayImage(inImageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    // 显示图像在imageLabel上
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void ColorMap::on_comboBox_colorMap_currentIndexChanged(int index)
{
    if (index <= 0 || index > 4) {
        qDebug() << "current index is" << index;
        return;
    }
    imageData = mainWindow->CurrentImageData();

    // 先转灰度(按像素分段，与主窗口保持同一套分段约定)
    const size_t pixelCount = imageData.size() / 3;
    const int numThreads = std::max(1, mainWindow->num_threads);
    const size_t segmentSize = std::max<size_t>(1, pixelCount / static_cast<size_t>(numThreads));

    auto func = std::bind(&Function::ConvertToGray, &function, std::ref(imageData),
                           std::placeholders::_1, std::placeholders::_2);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(numThreads));
    for (int i = 0; i < numThreads; ++i) {
        const size_t start = static_cast<size_t>(i) * segmentSize;
        const size_t end = (i == numThreads - 1) ? pixelCount : std::min(pixelCount, start + segmentSize);
        if (start >= end) {
            continue;
        }
        workers.emplace_back(func, start, end);
    }
    for (auto &thread : workers) {
        thread.join();
    }

    switch (index) {
    case 1: _ColorMap(imageData, colorMap1); break;
    case 2: _ColorMap(imageData, colorMap2); break;
    case 3: _ColorMap(imageData, colorMap3); break;
    case 4: _ColorMap(imageData, colorMap4); break;
    default: break;
    }

    ShowImage(imageData);
}


void ColorMap::on_btn_apply_clicked()
{
    qDebug() << "apply";
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(imageData);
}
