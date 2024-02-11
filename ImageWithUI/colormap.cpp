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
    imageData=mainWindow->imageData;
    newValue=myValue;
    QImage image(imageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

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
void ColorMap::_ColorMap(std::vector<uint8_t> &imageData, std::vector<uint8_t> &colorMap)
{
    //计算颜色映射表中颜色的数量,3表示channel
    int numColors = colorMap.size()/3;
    //转灰度图先
    //ConvertToGray(imageData);
    for (size_t i = 0; i < imageData.size(); i += 3) {
        uint8_t gray = imageData[i];
        //先归一化然后执行 乘法
        int index = static_cast<int>(static_cast<float>(gray) / 255.0f * (numColors - 1));

        imageData[i] = colorMap[index * 3];
        imageData[i + 1] = colorMap[index * 3 + 1];
        imageData[i + 2] = colorMap[index * 3 + 2];
    }

}

void ColorMap::ShowImage(std::vector<uint8_t> &inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

}

void ColorMap::on_comboBox_colorMap_currentIndexChanged(int index)
{
    imageData=mainWindow->imageData;
    if (index == 0) {
        qDebug()<<"current index is 0";
    } else if (index == 1) {

        qDebug()<<"current index is 1";
        auto func = std::bind(&Function::ConvertToGray, &function, std::ref(imageData), std::placeholders::_1, std::placeholders::_2);

        for (uint8_t i = 0; i < mainWindow->num_threads; i++)  // 创建多个线程
        {
            size_t start = i * mainWindow->segmentSize;  // 计算当前线程负责的数据段的起始位置
            size_t end = (i == mainWindow->num_threads - 1) ? newValue.imageData.size() : start + mainWindow->segmentSize;  // 结尾,同上
            mainWindow->segmentStarts.push_back(start);
            mainWindow->threads.emplace_back(func, start, end);
        }

        for (auto &thread : mainWindow->threads) {
            thread.join();
        }


        _ColorMap(imageData,colorMap1);
        ShowImage(imageData);
        mainWindow->SaveImageDataToHistory(imageData);
        mainWindow->ClearSegmentData();
    } else if (index == 2) {

        qDebug()<<"current index is 2";
        auto func = std::bind(&Function::ConvertToGray, &function, std::ref(imageData), std::placeholders::_1, std::placeholders::_2);

        for (uint8_t i = 0; i < mainWindow->num_threads; i++)  // 创建多个线程
        {
            size_t start = i * mainWindow->segmentSize;  // 计算当前线程负责的数据段的起始位置
            size_t end = (i == mainWindow->num_threads - 1) ? newValue.imageData.size() : start + mainWindow->segmentSize;  // 结尾,同上
            mainWindow->segmentStarts.push_back(start);
            mainWindow->threads.emplace_back(func, start, end);
        }

        for (auto &thread : mainWindow->threads) {
            thread.join();
        }


        _ColorMap(imageData,colorMap2);
        ShowImage(imageData);
        mainWindow->SaveImageDataToHistory(imageData);
        mainWindow->ClearSegmentData();
    } else if (index == 3) {

        qDebug()<<"current index is 3";
        auto func = std::bind(&Function::ConvertToGray, &function, std::ref(imageData), std::placeholders::_1, std::placeholders::_2);

        for (uint8_t i = 0; i < mainWindow->num_threads; i++)  // 创建多个线程
        {
            size_t start = i * mainWindow->segmentSize;  // 计算当前线程负责的数据段的起始位置
            size_t end = (i == mainWindow->num_threads - 1) ? newValue.imageData.size() : start + mainWindow->segmentSize;  // 结尾,同上
            mainWindow->segmentStarts.push_back(start);
            mainWindow->threads.emplace_back(func, start, end);
        }

        for (auto &thread : mainWindow->threads) {
            thread.join();
        }


        _ColorMap(imageData,colorMap3);
        ShowImage(imageData);
        mainWindow->SaveImageDataToHistory(imageData);
        mainWindow->ClearSegmentData();
    } else if (index == 4) {

        qDebug()<<"current index is 4";
        auto func = std::bind(&Function::ConvertToGray, &function, std::ref(imageData), std::placeholders::_1, std::placeholders::_2);

        for (uint8_t i = 0; i < mainWindow->num_threads; i++)  // 创建多个线程
        {
            size_t start = i * mainWindow->segmentSize;  // 计算当前线程负责的数据段的起始位置
            size_t end = (i == mainWindow->num_threads - 1) ? newValue.imageData.size() : start + mainWindow->segmentSize;  // 结尾,同上
            mainWindow->segmentStarts.push_back(start);
            mainWindow->threads.emplace_back(func, start, end);
        }

        for (auto &thread : mainWindow->threads) {
            thread.join();
        }


        _ColorMap(imageData,colorMap4);
        ShowImage(imageData);
        mainWindow->SaveImageDataToHistory(imageData);
        mainWindow->ClearSegmentData();
    }
}


void ColorMap::on_btn_save_clicked()
{
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


void ColorMap::on_btn_apply_clicked()
{
    qDebug()<<"apply";
    mainWindow->ShowImage(imageData ,newValue,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight());
}

