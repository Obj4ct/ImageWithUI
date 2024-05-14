
// MainWindow.cpp
#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocus();

    setWindowIcon(QIcon(":/icon/logo.png"));

    //根据设备cpu线程数判断程序需要设置的线程
    if(std::thread::hardware_concurrency()>=2)
    {
        qDebug()<<"current thread more than 2";
        num_threads=std::ceil(std::thread::hardware_concurrency()/3);//向上取整
    }
    else{
        num_threads=1;
    }
    qDebug()<<"thread is:"<<num_threads;
    // num_threads=std::thread::hardware_concurrency()/3;
    _SetShortCut();
    _SetVisible(false);


}
void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 获取事件发生的位置
        QPoint pos = event->pos();
        // 判断事件是否发生在imageLabel上
        if (ui->imageLabel->rect().contains(pos)) {
            // 文件默认路径
            QString defaultPath = QDir::homePath();
            // 设置过滤
            QString filter = "BMP文件(*.bmp)";
            QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);
            if (path.isEmpty()) {
                qDebug() << "未选择文件";
                return;
            }
            else{
                _SetVisible(true);
                // 读取BMP文件并且存入变量中
                BMPPath = path.toStdString();
                myValue = MYFunction::ReadBMPFile(BMPPath);
                imageSize=myValue.imageData.size();
                segmentSize = imageSize / num_threads;  // 均分处理
                qDebug()<<"image size is:"<<imageSize;
                qDebug()<<"segment size is:"<<segmentSize;

                //input debug info
                //ImgInfo(myValue.bmp,myValue.bmpInfo);
                // 将图像数据转换为QImage
                //BGR排序



                m_bmpImage = QImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
                m_bmpImage.mirror(false,true);


                //        Debug::PrintImagePixelsToFile(m_bmpImage, "output.txt");
                //        qDebug()<<"write ok!";

                imageData=myValue.imageData;
                // 显示图像在imageLabel上
                QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
                ui->imageLabel->setPixmap(pixmap);
                SaveImageDataToHistory(imageData);
                canSave=true;

            }
        }
    }
}
void MainWindow::ResetImage(MyValue &myValue)
{

    imageData=myValue.imageData;
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);
}

void MainWindow::ShowImage(std::vector<uint8_t> &inImageData,MyValue value,int32_t width,int32_t height)
{

    imageData=inImageData;
    myValue=value;
    QImage image(inImageData.data(), width, height, QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);

}

void MainWindow::ResetAll(MyValue &myValue)
{
    imageData=myValue.imageData;
    m_bmpImage = QImage(imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
    m_bmpImage.mirror(false,true);
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);
    imageDataHistory.clear();
    redoImageDataHistory.clear();
}
ReturnValue MainWindow::CheckOK(QLineEdit * lineEdit)
{
    ReturnValue returnValue;
    if(lineEdit->text()==nullptr)
    {
        returnValue.isNull=true;
        return returnValue;
    }
    //判断高和宽是否正确输入数字
    QString inputText= lineEdit->text(); // 获取输入的文本
    bool isNumeric;
    double_t value = inputText.toDouble(&isNumeric);
    qDebug()<<"read value is "<<value;
    if(!isNumeric)
    {
        returnValue.isNumeric=false;
        return returnValue;
    }else{
        returnValue.isNumeric=true;
        returnValue.value=value;
        return returnValue;
    }
}


MainWindow::~MainWindow()
{
    delete ui;
}
// 撤销操作，从链表中获取上一步的图像数据进行恢复
OperationType MainWindow::SaveImageDataToHistory(std::vector<uint8_t> &imageData)
{
    // 保存当前图像数据到链表
    if (!imageDataHistory.empty())
    {
        if (imageData != imageDataHistory.back())
        {
            imageDataHistory.push_back(imageData);
            return UNDO;
        }
    }
    else
    {
        imageDataHistory.push_back(imageData);
        return UNDO;
    }

    return REDO; // 返回重做类型
}


void MainWindow::UndoImageProcessing()
{
    if (!imageDataHistory.empty())
    {
        std::vector<uint8_t> imageDataToRedo = imageDataHistory.back();
        // 保存当前图像数据到重做链表
        redoImageDataHistory.push_back(imageDataToRedo);
        imageDataHistory.pop_back();
        if(!imageDataHistory.empty())
        {
            std::vector<uint8_t> currentImageData=imageDataHistory.back();

            // 恢复图像数据
            imageData = currentImageData;


            // 更新图像显示
            ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
            qDebug() << "Undo operation";
        }
        else{
            qDebug()<<"empty";
            // 更新图像显示
            ShowImage(myValue.imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
            imageData=myValue.imageData;

        }
    }
    else
    {
        qDebug() << "Undo operation failed: history is empty";
    }
}
void MainWindow::RedoImageProcessing()
{
    if (!redoImageDataHistory.empty())
    {
        std::vector<uint8_t> nextImageData = redoImageDataHistory.back();
        imageDataHistory.push_back(nextImageData);
        redoImageDataHistory.pop_back();

        // 恢复图像数据
        imageData = nextImageData;

        // 更新图像显示
        ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
        qDebug() << "Redo operation";
    }
    else
    {
        qDebug() << "Redo operation failed: no redo history";
    }
}

void MainWindow::on_openImage_triggered()
{
    //clear list
    imageDataHistory.clear();
    redoImageDataHistory.clear();
    // 文件默认路径
    QString defaultPath = QDir::homePath();
    // 设置过滤
    QString filter = "BMP文件(*.bmp)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    else{
        _SetVisible(true);
        // 读取BMP文件并且存入变量中
        BMPPath = path.toStdString();
        myValue = MYFunction::ReadBMPFile(BMPPath);
        imageSize=myValue.imageData.size();
        segmentSize = imageSize / num_threads;  // 均分处理
        qDebug()<<"image size is:"<<imageSize;
        qDebug()<<"segment size is:"<<segmentSize;

        //input debug info
        //ImgInfo(myValue.bmp,myValue.bmpInfo);
        // 将图像数据转换为QImage
        //BGR排序



        m_bmpImage = QImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
        m_bmpImage.mirror(false,true);


        //        Debug::PrintImagePixelsToFile(m_bmpImage, "output.txt");
        //        qDebug()<<"write ok!";

        imageData=myValue.imageData;
        // 显示图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
        ui->imageLabel->setPixmap(pixmap);
        SaveImageDataToHistory(imageData);
        canSave=true;

    }
}
void MainWindow::on_actionsave_triggered()
{
    std::vector<uint8_t> saveImageData;
    //不能保存文件时
    if(!canSave)
    {
        function.CreateMessagebox("提示", "没有任何文件");
        qDebug() << "文件为空！";
        return;
    }
    //可以保存文件并且链表不为空
    else if (canSave && !imageDataHistory.empty())
    {
        // 获取最近保存的图像数据
        saveImageData = imageDataHistory.back();
    }
    else if (imageDataHistory.empty())
    {

        // 如果没有历史记录，保存原始图像数据
        saveImageData = myValue.imageData;
    }

    QString filePath = QFileDialog::getSaveFileName(nullptr, "保存文件", "", "BMP文件(*.bmp)");
    if (filePath.isEmpty()) {
        qDebug() << "保存操作已取消。";
        return;
    }

    savePath = filePath.toStdString();

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    std::string str = fileName.toStdString();
    std::cout << "文件名为 " << str << std::endl;

    // 使用 saveImageData 而不是 myValue.imageData
    MYFunction::WriteBMPFile(str, saveImageData, myValue.bmp, myValue.bmpInfo);
    qDebug() << "保存成功！";
}

//清理每段数据,防止数据冲突引发异常
void MainWindow::ClearSegmentData()
{
    threads.clear();
    segmentStarts.clear();
}
void MainWindow::on_actiongray_triggered()
{
    std::vector<uint8_t> tempImageData = imageData;
    if (!imageDataHistory.empty()) {
        tempImageData = imageDataHistory.back();  // 复制当前图像数据
    }
    // 使用 std::bind 将成员函数包装成可调用对象
    auto func = std::bind(&Function::ConvertToGray, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);

    for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
    {
        size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
        size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
        segmentStarts.push_back(start);

        // 使用可调用对象传递给线程
        threads.emplace_back(func, start, end);
    }
    for (auto &thread : threads) {
        thread.join();
    }
    SaveImageDataToHistory(tempImageData);  // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    ClearSegmentData();
}
void MainWindow::on_actionautoContrast_triggered()
{
    qDebug()<<"click auto contrast";
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    auto func = std::bind(&Function::AutoContrast, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
    float_t aver = function.CalAver(imageData);
    float_t standard = function.CalStandard(imageData, aver);

    for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
    {
        size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
        size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
        segmentStarts.push_back(start);
        threads.emplace_back(func, std::ref(aver), std::ref(standard), start, end);
    }

    for (auto &thread : threads) {
        thread.join();
    }
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    ClearSegmentData();

}


void MainWindow::on_actionaver_triggered()
{
    qDebug()<<"aver";
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }

    auto func = std::bind(&Function::AverageBlur, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

    for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
    {
        size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
        size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
        segmentStarts.push_back(start);

        // 使用可调用对象传递给线程
        threads.emplace_back(func,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),start, end);
    }
    for (auto &thread : threads) {
        thread.join();
    }

    //function.AverageBlur(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    ClearSegmentData();
}

void MainWindow::on_btn_resetAll_clicked()
{
    ResetAll(myValue);
}


void MainWindow::on_actionblend_triggered()
{
    qDebug()<<"i am in a blend window!";
    blendWindow = new BlendWindow(this,myValue);
    blendWindow->show();

}


void MainWindow::on_btn_brightness_clicked()
{
    ReturnValue returnValue=CheckOK(ui->lineEdit_brightness);
    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{

        std::vector<uint8_t> tempImageData=imageData;
        qDebug()<<returnValue.value;
        if(returnValue.value < -150 || returnValue.value > 150)
        {
            std::cout << "out of range,brightnessValue is between -150 to 150, please try again!" << std::endl;
            if(!function.CreateMessagebox("提示","超出范围,范围是[-150,150]"))
            {
                return;
            }
        }
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }

        auto func = std::bind(&Function::Brightness, &function, std::ref(tempImageData), std::placeholders::_1);
        std::thread brightThread(func, returnValue.value);
        brightThread.join();
        //function.Brightness(tempImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_contrast_clicked()
{

    ReturnValue returnValue=CheckOK(ui->lineEdit_contrast);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{

        std::vector<uint8_t> tempImageData=imageData;
        qDebug()<<returnValue.value;
        if(returnValue.value < -50 || returnValue.value > 100)
        {
            std::cout << "out of range,brightnessValue is between -50 to 100, please try again!" << std::endl;
            if(!function.CreateMessagebox("提示","超出范围,范围是[-50,100]"))
            {
                return;
            }
        }
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        auto func = std::bind(&Function::Contrast, &function, std::ref(tempImageData), std::placeholders::_1);
        std::thread constrastThread(func,returnValue.value);
        //function.Contrast(tempImageData,returnValue.value);
        constrastThread.join();
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_saturation_clicked()
{
    //判断高和宽是否正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_saturation);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }

        auto func = std::bind(&Function::Saturation, &function, std::ref(tempImageData), std::placeholders::_1,std::placeholders::_2,std::placeholders::_3);
        std::thread saturationThread(func,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
        saturationThread.join();
        //        function.Saturation(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    }
}



void MainWindow::on_actioncolorBalance_triggered()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }


    function.ColorBalance(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData,myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());

}

void MainWindow::on_actioncolorMap_triggered()
{
    qDebug() << "I am in a color map window!";
    ColorMap *colorMap= new ColorMap(this,myValue);
    colorMap->show();
}


void MainWindow::on_actionreverse_triggered()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    auto func = std::bind(&Function::InvertColors, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);

    for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
    {
        size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
        size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
        segmentStarts.push_back(start);

        // 使用可调用对象传递给线程
        threads.emplace_back(func, start, end);
    }
    for (auto &thread : threads) {
        thread.join();
    }

    //function.InvertColors(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData,myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    ClearSegmentData();
}


void MainWindow::on_actioncomplementary_triggered()
{

    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    auto func = std::bind(&Function::Complementary, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);

    for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
    {
        size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
        size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
        segmentStarts.push_back(start);

        // 使用可调用对象传递给线程
        threads.emplace_back(func, start, end);
    }
    for (auto &thread : threads) {
        thread.join();
    }


    //function.Complementary(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());


    ClearSegmentData();

}


void MainWindow::on_actionliquefaction_triggered()
{
    qDebug() << "I am in a eye window!";
    eye *myEye= new eye(this,myValue);
    myEye->show();
}


void MainWindow::on_actionface_triggered()
{
    qDebug() << "I am in a face window!";
    Face *face = new Face(this,myValue);
    face->show();

}


void MainWindow::on_actionfishEye_triggered()
{

    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }

    // 创建一个 std::promise 对象，用于接收函数的结果
    std::promise<std::vector<uint8_t>> promise;
    std::future<std::vector<uint8_t>> future = promise.get_future();

    // 使用 std::bind 调用成员函数
    auto func = std::bind(&Function::Fisheye, &function, std::ref(tempImageData), std::ref(promise), std::placeholders::_1, std::placeholders::_2);

    // 启动一个线程执行绑定的函数
    std::thread thread(func, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());  // 这里传入具体的参数

    // 等待线程执行完成
    thread.join();

    // 获取函数的结果
    std::vector<uint8_t> result = future.get();
    SaveImageDataToHistory(result); // 保存当前图像数据到链表
    ShowImage(result, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());



}


void MainWindow::on_actiongauss_triggered()
{
    qDebug()<<"gauss";
    bool dialogClosed = false;
    while (!dialogClosed) {
        bool ok;
        double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 0, 0, 10, 1, &ok);

        if (ok) {

            qDebug() << "用户输入的程度:" << degree;
            std::vector<uint8_t> tempImageData=imageData;
            if(!imageDataHistory.empty())
            {
                tempImageData = imageDataHistory.back(); // 复制当前图像数据
            }
            std::promise<std::vector<uint8_t>> promise;
            std::future<std::vector<uint8_t>> future = promise.get_future();

            // 使用 std::bind 调用成员函数
            auto func = std::bind(&Function::Gauss, &function, std::ref(tempImageData), std::ref(promise), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);

            // 启动一个线程执行绑定的函数
            std::thread thread(func, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),degree);
            std::vector<uint8_t> resultImage = future.get();
            // 等待线程执行完成
            thread.join();

            SaveImageDataToHistory(resultImage); // 保存当前图像数据到链表
            ShowImage(resultImage,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        } else {

            qDebug() << "用户取消输入";
            dialogClosed = true; // 设置标志以退出循环
        }
    }


}


void MainWindow::on_btn_rotate_ok_clicked()
{
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_rotate);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        auto func = std::bind(&Function::RotateImage, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);
        std::thread rotateThread(func, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);

        rotateThread.join();
        SaveImageDataToHistory(tempImageData);// 保存当前图像数据到链表
        ShowImage(tempImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());


    }

}


void MainWindow::on_btn_rotate_r_clicked()
{
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_rotate_r);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        auto func = std::bind(&Function::RotateReverse, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);
        std::thread rotateThread(func, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);
        rotateThread.join();
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    }

}



void MainWindow::on_actionmedian_triggered()
{
    qDebug()<<"median";
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    auto func = std::bind(&Function::MedianBlur, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);
    std::thread medianBlur(func,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    medianBlur.join();

    //function.MedianBlur(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData,myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());


}



void MainWindow::on_actionmosaic_triggered()
{

    bool dialogClosed = false;
    while (!dialogClosed) {
        bool ok;
        double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 0, 1, 100, 1, &ok);
        if (ok) {

            qDebug() << "用户输入的程度:" << degree;
            std::vector<uint8_t> tempImageData=imageData;
            if(!imageDataHistory.empty())
            {
                tempImageData = imageDataHistory.back(); // 复制当前图像数据
            }

            function.FullMosaic(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),degree);
            SaveImageDataToHistory(imageData); // 保存当前图像数据到链表
            ShowImage(imageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        } else {

            qDebug() << "用户取消输入";
            dialogClosed = true; // 设置标志以退出循环
            return;
        }
    }
}




void MainWindow::on_btn_shadow_ok_clicked()
{
    std::vector<uint8_t> shadowImageData(imageData.size());
    shadowImageData=imageData;
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_shadow);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }

        auto func = std::bind(&Function::MakeShadow, &function, std::ref(tempImageData),std::ref(shadowImageData), std::placeholders::_1);
        std::thread tMakeShadow(func, std::ref(returnValue.value));

        tMakeShadow.join();

        //function.MakeShadow(tempImageData,shadowImageData,returnValue.value);

        SaveImageDataToHistory(shadowImageData); // 保存当前图像数据到链表
        ShowImage(shadowImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());




    }
}


void MainWindow::on_btn_highlight_ok_clicked()
{

    std::vector<uint8_t> highLightImageData(imageData.size());
    highLightImageData=imageData;
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_highlight);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{

        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        function.HighLight(tempImageData,highLightImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(highLightImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_sharpen_clicked()
{
    qDebug()<<"ok";
    ReturnValue returnValue=CheckOK(ui->lineEdit_sharpen);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }
    else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        std::promise<std::vector<uint8_t>> blurPromise;
        std::promise<std::vector<uint8_t>> highPromise;
        std::promise<std::vector<uint8_t>> sharpenPromise;


        std::future<std::vector<uint8_t>> blurValue=blurPromise.get_future();
        std::future<std::vector<uint8_t>> highValue=highPromise.get_future();
        std::future<std::vector<uint8_t>> sharpenValue=sharpenPromise.get_future();
        auto gaussFunc = std::bind(&Function::Gauss, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3,std::placeholders::_4);
        auto highContrastFunc = std::bind(&Function::HighContrast, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);
        auto sharpenFunc = std::bind(&Function::Sharpen, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2);


        std::thread gauss(gaussFunc,std::ref(blurPromise),myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),returnValue.value);
        std::vector<uint8_t> gaussValue = blurValue.get();
        gauss.join();
        std::thread highContrast(highContrastFunc, std::ref(highPromise), std::ref(gaussValue));
        std::vector<uint8_t> highContrastValue = highValue.get();
        highContrast.join();

        std::thread sharpen(sharpenFunc, std::ref(sharpenPromise), std::ref(highContrastValue));
        std::vector<uint8_t> finalResult = sharpenValue.get();
        sharpen.join();

        SaveImageDataToHistory(finalResult); // 保存当前图像数据到链表

        ShowImage(finalResult,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}





void MainWindow::on_actionsobel_triggered()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }


    std::vector<uint8_t> edgeImageData = function.SobelEdge(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    SaveImageDataToHistory(edgeImageData); // 保存当前图像数据到链表
    ShowImage(edgeImageData,myValue,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());



}


void MainWindow::on_btn_threshold_ok_clicked()
{
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_threshold);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        auto func = std::bind(&Function::ApplyThreshold, &function, std::ref(tempImageData), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);

        for (uint8_t i = 0; i < num_threads; i++)  // 创建多个线程
        {
            size_t start = i * segmentSize;  // 计算当前线程负责的数据段的起始位置
            size_t end = (i == num_threads - 1) ? imageSize : start + segmentSize;  // 结尾,同上
            segmentStarts.push_back(start);

            // 使用可调用对象传递给线程
            threads.emplace_back(func, returnValue.value,start, end);
        }
        for (auto &thread : threads) {
            thread.join();
        }


        //function.ApplyThreshold(tempImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
        ClearSegmentData();
    }
}

void MainWindow::on_actionscale_triggered()
{
    qDebug() << "I am in a inter window!";
    Interpolation *inter = new Interpolation(this,myValue);
    inter->show();
}


void MainWindow::on_btn_undo_clicked()
{
    UndoImageProcessing();
}

void MainWindow::on_actionundo_triggered()
{
    UndoImageProcessing();
}

void MainWindow::on_actionreset_triggered()
{
    ResetAll(myValue);
}

void MainWindow::on_actionredo_triggered()
{
    qDebug()<<"redo";
    RedoImageProcessing();
}

void MainWindow::_SetVisible(bool set)
{
    qDebug() << "Setting visibility to:" << set;
    ui->toolBar->setVisible(set);
    ui->menu_edit->menuAction()->setVisible(set);
    ui->menu_blur->menuAction()->setVisible(set);
    ui->menu_base->menuAction()->setVisible(set);
    ui->menu_unbase->menuAction()->setVisible(set);
    ui->widget->setVisible(set);

}

void MainWindow::_SetShortCut()
{
    ui->openImage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    ui->actionsave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    ui->actionundo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    ui->actionreset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    ui->actionredo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton button;

    button=QMessageBox::question(this,tr("退出程序"),QString(tr("确认退出程序")),QMessageBox::Yes|QMessageBox::No);

    if(button==QMessageBox::No)

    {

        event->ignore(); // 忽略退出信号，程序继续进行

    }

    else if(button==QMessageBox::Yes)

    {

        event->accept(); // 接受退出信号，程序退出

    }



    //TODO: 在退出窗口之前，实现希望做的操作


}

void MainWindow::on_actionlarge_near_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowLargeNear(height,width);


    } else {

        qDebug() << "用户取消输入";

    }

}


void MainWindow::on_actionsmall_near_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowSmallNear(height,width);


    } else {

        qDebug() << "用户取消输入";

    }
}


void MainWindow::on_actionlarge_bilinear_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowLargeBilinear(height,width);


    } else {

        qDebug() << "用户取消输入";

    }
}


void MainWindow::on_actionsmall_bilinear_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowSmallBilinear(height,width);


    } else {

        qDebug() << "用户取消输入";

    }
}


void MainWindow::on_actionlarge_bicubic_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowLargeBicubic(height,width);


    } else {

        qDebug() << "用户取消输入";

    }
}


void MainWindow::on_actionsmall_bicubic_triggered()
{
    bool ok1;
    bool ok2;
    int height = QInputDialog::getInt(this, tr("输入高度倍数"), tr("高度倍数:"), 0, 0, 100, 1, &ok1);
    int width = QInputDialog::getInt(this, tr("输入宽度倍数"), tr("宽度倍数:"), 0, 0, 100, 1, &ok2);

    if (ok1 && ok2) {

        qDebug() << "用户输入的高度倍数:" << height;
        qDebug() << "用户输入的宽度倍数:" << width;
        Interpolation inter(this,myValue);
        inter.ShowSmallBicubic(height,width);


    } else {

        qDebug() << "用户取消输入";

    }
}

void MainWindow::on_actionhighContrast_triggered()
{
    bool dialogClosed = false;
    while (!dialogClosed) {
        bool ok;
        double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 0, 1, 100, 1, &ok);
        if (ok) {

            qDebug() << "用户输入的程度:" << degree;
            std::vector<uint8_t> tempImageData=imageData;
            if(!imageDataHistory.empty())
            {
                tempImageData = imageDataHistory.back(); // 复制当前图像数据
            }

            std::promise<std::vector<uint8_t>> gaussPromise;
            std::promise<std::vector<uint8_t>> highContrastPromise;

            std::future<std::vector<uint8_t>> gaussFuture = gaussPromise.get_future();
            std::future<std::vector<uint8_t>> highContrastFuture = highContrastPromise.get_future();

            // 使用 std::bind 调用成员函数
            auto gaussFunc = std::bind(&Function::Gauss, &function, std::ref(tempImageData), std::ref(gaussPromise), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);

            auto highContrastFunc = std::bind(&Function::HighContrast, &function, std::ref(tempImageData), std::ref(highContrastPromise), std::placeholders::_1);



            std::thread gaussThread(gaussFunc,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),degree);

            // 等待异步任务完成并获取值
            std::vector<uint8_t> gaussValue = gaussFuture.get();
            gaussThread.join();

            std::thread highContrastThread(highContrastFunc, std::ref(gaussValue));
            std::vector<uint8_t> value = highContrastFuture.get();
            highContrastThread.join();
            SaveImageDataToHistory(value); // 保存当前图像数据到链表
            ShowImage(value, myValue,myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
        } else {

            qDebug() << "用户取消输入";
            dialogClosed = true; // 设置标志以退出循环
            return;
        }
    }
}

