// MainWindow.cpp
#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->widget->setVisible(false);
    ui->widget_add->setVisible(false);
    ui->widget_operation->setVisible(false);
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

void MainWindow::ShowImage(std::vector<uint8_t> &inImageData,int32_t width,int32_t height)
{

    QImage image(inImageData.data(), width, height, QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void MainWindow::ResetAll(MyValue &myValue)
{
    imageData=myValue.imageData;
    //imageDataHistory.clear();
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);

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
void MainWindow::UndoImageProcessing()
{
    if (!imageDataHistory.empty())
    {
        std::vector<uint8_t> previousImageData = imageDataHistory.back();
        imageDataHistory.pop_back();

        // 恢复图像数据
        imageData = previousImageData;

        // 更新图像显示
        ShowImage(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
        qDebug()<<"undo ok";
    }
    else
    {
        imageData=myValue.imageData;
        ShowImage(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        qDebug()<<"list is empty and undo ok";
    }
}
void MainWindow::SaveImageDataToHistory(std::vector<uint8_t> &imageData)
{
    // 在图像处理函数中保存当前图像数据到链表
    imageDataHistory.push_back(imageData);


}
void MainWindow::on_openImage_triggered()
{
    // 文件默认路径
    QString defaultPath = QDir::currentPath();
    // 设置过滤
    QString filter = "BMP文件(*.bmp)";
    QString path = QFileDialog::getOpenFileName(this, "选择BMP文件", defaultPath, filter);

    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    else{
        ui->widget->setVisible(true);
        ui->widget_add->setVisible(true);
        ui->widget_operation->setVisible(true);
        // 读取BMP文件并且存入变量中
        std::string BMPPath = path.toStdString();
        myValue = MYFunction::ReadBMPFile(BMPPath);
        //input debug info
        ImgInfo(myValue.bmp,myValue.bmpInfo);
        // 将图像数据转换为QImage
        //BGR排序
        m_bmpImage = QImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
        imageData=myValue.imageData;
        // 进行垂直翻转
        m_bmpImage = m_bmpImage.mirrored(false, true);
        // 显示图像在imageLabel上
        QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
        ui->imageLabel->setPixmap(pixmap);
        ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
        canSave=true;
    }

}

void MainWindow::on_actionsave_triggered()
{
    std::vector<uint8_t> saveImageData;
    if (canSave&&!imageDataHistory.empty())
    {
        saveImageData=imageDataHistory.back();
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
        MYFunction::WriteBMPFile(str,saveImageData,myValue.bmp,myValue.bmpInfo);
        qDebug()<<"succeed!";
    }
    else if(canSave&&imageDataHistory.empty()){
        saveImageData=myValue.imageData;
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
        MYFunction::WriteBMPFile(str,saveImageData,myValue.bmp,myValue.bmpInfo);
        qDebug()<<"succeed!";
    }else{
        function.CreateMessagebox("提示","没有任何文件");
        qDebug()<<"file empty!";
        return;
    }

}



void MainWindow::on_btn_gray_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.ConvertToGray(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}



void MainWindow::on_btn_autoContrast_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.AutoContrast(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());

}


void MainWindow::on_btn_averBlur_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.AverageBlur(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}

void MainWindow::on_btn_resetAll_clicked()
{
    ResetAll(myValue);
}


void MainWindow::on_btn_blend_clicked()
{
    qDebug()<<"i am in a blend window!";
    blendWindow = new BlendWindow();
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
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        function.Brightness(tempImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
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
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }

        function.Contrast(tempImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
        function.Saturation(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_color_balance_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.ColorBalance(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}
void MainWindow::on_btn_colorMap_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.ColorMap(tempImageData,colorMap);

    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}


void MainWindow::on_btn_reverse_color_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }

    function.InvertColors(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}


void MainWindow::on_btn_complementary_clicked()
{

    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }

    function.Complementary(tempImageData);
    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());




}


void MainWindow::on_btn_eye_clicked()
{
    qDebug() << "I am in a eye window!";
    eye *myEye= new eye(this,myValue);
    myEye->show();
}


void MainWindow::on_btn_face_clicked()
{
    qDebug() << "I am in a face window!";
    Face *face = new Face(this,myValue);
    face->show();

}


void MainWindow::on_btn_fish_eye_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    std::vector<uint8_t> fishEyeImageData=function.Fisheye(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    SaveImageDataToHistory(fishEyeImageData); // 保存当前图像数据到链表
    ShowImage(fishEyeImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

}


void MainWindow::on_btn_gauss_ok_clicked()
{
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_gauss);

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
        std::vector<uint8_t> gaussImageData=function.Gauss(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
        SaveImageDataToHistory(gaussImageData); // 保存当前图像数据到链表
        ShowImage(gaussImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }

}


void MainWindow::on_btn_highContrast_ok_clicked()
{
    //正确输入数字
    ReturnValue returnValue=CheckOK(ui->lineEdit_highContrast);

    if(returnValue.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValue.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        //        std::vector<uint8_t> ImageData=function.Gauss(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);

        //        std::vector<uint8_t> blurImageData = function.Gauss(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),returnValue.value);
        //        std::vector<uint8_t> highContrastImageData = function.HighContrast(imageData, blurImageData);
        //        ShowImage(highContrastImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        std::vector<uint8_t> tempImageData=imageData;
        if(!imageDataHistory.empty())
        {
            tempImageData = imageDataHistory.back(); // 复制当前图像数据
        }
        std::vector<uint8_t> ImageData=function.Gauss(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);

        std::vector<uint8_t> blurImageData = function.Gauss(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),returnValue.value);
        std::vector<uint8_t> highContrastImageData = function.HighContrast(tempImageData, blurImageData);
        SaveImageDataToHistory(highContrastImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
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
        function.RotateImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

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
        function.RotateReverse(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    }

}


void MainWindow::on_btn_mask_clicked()
{
    qDebug() << "I am in a mask window!";
    Mask *mask = new Mask(this,myValue);
    mask->show();
}


void MainWindow::on_btn_medianBlur_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    function.MedianBlur(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

    SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
    ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());


}


void MainWindow::on_btn_mosaic_clicked()
{

    qDebug() << "I am in a mosaic window!";
    Mosaic *mosaic = new Mosaic(this,myValue);
    mosaic->show();
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

        function.MakeShadow(tempImageData,shadowImageData,returnValue.value);

        SaveImageDataToHistory(shadowImageData); // 保存当前图像数据到链表
        ShowImage(shadowImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());




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
        ShowImage(highLightImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_sharpen_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    std::vector<uint8_t> blurImageData = function.Gauss(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),10);
    std::vector<uint8_t> highContrast = function.HighContrast(tempImageData, blurImageData);
    std::vector<uint8_t> sharpenImageData = function.Sharpen(tempImageData, highContrast);

    SaveImageDataToHistory(sharpenImageData); // 保存当前图像数据到链表

    ShowImage(sharpenImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());



}


void MainWindow::on_btn_tailor_clicked()
{

    qDebug() << "I am in a tailor window!";
    Tailor *tailor = new Tailor(this,myValue);
    tailor->show();
}


void MainWindow::on_btn_tensor_clicked()
{
    std::vector<uint8_t> tempImageData=imageData;
    if(!imageDataHistory.empty())
    {
        tempImageData = imageDataHistory.back(); // 复制当前图像数据
    }
    std::vector<uint8_t> edgeImageData = function.SobelEdge(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    SaveImageDataToHistory(edgeImageData); // 保存当前图像数据到链表
    ShowImage(edgeImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());



}


void MainWindow::on_btn_threshold_ok_clicked()
{

    std::vector<uint8_t> highLightImageData(imageData.size());
    highLightImageData=imageData;
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

        function.ApplyThreshold(tempImageData,returnValue.value);
        SaveImageDataToHistory(tempImageData); // 保存当前图像数据到链表
        ShowImage(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_inter_clicked()
{
    qDebug() << "I am in a inter window!";
    Interpolation *inter = new Interpolation(this,myValue);
    inter->show();
}


void MainWindow::on_btn_undo_clicked()
{
    UndoImageProcessing();
}

