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
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);
    if(isGray)
    {
        ui->btn_gray->setText("转为灰度图");
    }
    if(isAutoContrast)
    {
        ui->btn_autoContrast->setText("自动调整对比度");
    }
    if(isAverBlur)
    {
        ui->btn_averBlur->setText("均值模糊");
    }
    if(isColorBalance)
    {
        ui->btn_color_balance->setText("色彩平衡");
    }
    if(isColorMap)
    {
        ui->btn_colorMap->setText("色彩映射");
    }
    if(isReverse)
    {
        ui->btn_reverse_color->setText("色彩反转");
    }
    if(isComplementary)
    {
        ui->btn_complementary->setText("补色");
    }
    if(isFishEye)
    {
        ui->btn_fish_eye->setText("鱼眼镜头");
    }
    if(isMedianBlur)
    {
        ui->btn_medianBlur->setText("中值模糊");
    }
    if(isSharpen)
    {
        ui->btn_sharpen->setText("一键锐化");
    }
    if(isTensor)
    {
        ui->btn_tensor->setText("边缘检测");
    }
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
    }

}



void MainWindow::on_btn_gray_clicked()
{
    // 切换按钮状态
    std::vector<uint8_t> grayImageData=imageData;
    if (ui->btn_gray->text() == "转为灰度图") {
        // 转换为灰度图
        function.ConvertToGray(grayImageData);

        ShowImage(grayImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isGray=true;
        // 更新按钮文本
        ui->btn_gray->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_gray->setText("转为灰度图");
    }
}



void MainWindow::on_btn_autoContrast_clicked()
{
    std::vector<uint8_t> contrastImageData=imageData;
    function.AutoContrast(contrastImageData);
    // 切换按钮状态
    if (ui->btn_autoContrast->text() == "自动调整对比度") {
        // 转换为灰度图
        function.AutoContrast(contrastImageData);

        ShowImage(contrastImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isAutoContrast=true;
        // 更新按钮文本
        ui->btn_autoContrast->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);

        // 更新按钮文本
        ui->btn_autoContrast->setText("自动调整对比度");
    }
}


void MainWindow::on_btn_averBlur_clicked()
{
    std::vector<uint8_t> averBLurImageData=imageData;
    function.AverageBlur(averBLurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    // 切换按钮状态
    if (ui->btn_averBlur->text() == "均值模糊") {
        // 转换为灰度图
        function.AverageBlur(averBLurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        ShowImage(averBLurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isAverBlur=true;
        // 更新按钮文本
        ui->btn_averBlur->setText("取消");
    } else {
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_averBlur->setText("均值模糊");
    }
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
        std::vector<uint8_t> brightnessImageData=imageData;
        qDebug()<<returnValue.value;
        function.Brightness(brightnessImageData,returnValue.value);
        ShowImage(brightnessImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
        std::vector<uint8_t> contrastImageData=imageData;
        function.Contrast(contrastImageData,returnValue.value);
        ShowImage(contrastImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
        std::vector<uint8_t> saturationImageData=imageData;
        function.Saturation(saturationImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
        ShowImage(saturationImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_color_balance_clicked()
{
    std::vector<uint8_t> balanceImageData=imageData;

    if (ui->btn_color_balance->text() == "色彩平衡") {
        // 转换为灰度图
        function.ColorBalance(balanceImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isColorBalance=true;
        ShowImage(balanceImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        // 更新按钮文本
        ui->btn_color_balance->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_color_balance->setText("色彩平衡");
    }
}
void MainWindow::on_btn_colorMap_clicked()
{
    // 切换按钮状态
    std::vector<uint8_t> colorMapImageData=imageData;
    if (ui->btn_colorMap->text() == "色彩映射") {
        // 转换为灰度图
        function.ColorMap(colorMapImageData,colorMap);

        ShowImage(colorMapImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isColorMap=true;
        // 更新按钮文本
        ui->btn_colorMap->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_colorMap->setText("色彩映射");
    }
}


void MainWindow::on_btn_reverse_color_clicked()
{
    // 切换按钮状态
    std::vector<uint8_t> reverseImageData=imageData;
    if (ui->btn_reverse_color->text() == "色彩反转") {
        // 转换为灰度图
        function.InvertColors(reverseImageData);

        ShowImage(reverseImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isReverse=true;
        // 更新按钮文本
        ui->btn_reverse_color->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_reverse_color->setText("色彩反转");
    }
}


void MainWindow::on_btn_complementary_clicked()
{

    // 切换按钮状态
    std::vector<uint8_t> complementaryImageData=imageData;
    if (ui->btn_complementary->text() == "补色") {
        // 转换为灰度图
        function.Complementary(complementaryImageData);
        ShowImage(complementaryImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isComplementary=true;

        // 更新按钮文本
        ui->btn_complementary->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_complementary->setText("补色");
    }

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
    std::vector<uint8_t> fishEyeImageData;;
    if (ui->btn_fish_eye->text() == "鱼眼镜头") {
        std::vector<uint8_t> fishEyeImageData=function.Fisheye(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());

        ShowImage(fishEyeImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isFishEye=true;

        // 更新按钮文本
        ui->btn_fish_eye->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_fish_eye->setText("鱼眼镜头");
    }
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
        std::vector<uint8_t> gaussImageData=function.Gauss(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);
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
        std::vector<uint8_t> ImageData=function.Gauss(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight(),returnValue.value);

        std::vector<uint8_t> blurImageData = function.Gauss(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),returnValue.value);
        std::vector<uint8_t> highContrastImageData = function.HighContrast(imageData, blurImageData);
        ShowImage(highContrastImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
        function.RotateImage(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);
        ShowImage(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
        function.RotateReverse(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), returnValue.value);
        ShowImage(imageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
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
    std::vector<uint8_t> medianBlurImageData=imageData;
    if (ui->btn_medianBlur->text() == "中值模糊") {
        qDebug()<<"in median";
        function.MedianBlur(medianBlurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        ShowImage(medianBlurImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isMedianBlur=true;

        // 更新按钮文本
        ui->btn_medianBlur->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_medianBlur->setText("中值模糊");
    }
}


void MainWindow::on_btn_mosaic_clicked()
{

    qDebug() << "I am in a mosaic window!";
    Mosaic *mosaic = new Mosaic(this,myValue);
    mosaic->show();
}




void MainWindow::on_btn_shadow_ok_clicked()
{
    std::vector<uint8_t>tempImageData=imageData;
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
        function.MakeShadow(tempImageData,shadowImageData,returnValue.value);
        ShowImage(shadowImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_highlight_ok_clicked()
{
    std::vector<uint8_t>tempImageData=imageData;
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
        function.HighLight(tempImageData,highLightImageData,returnValue.value);
        ShowImage(highLightImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_sharpen_clicked()
{
    std::vector<uint8_t>tempImageData=imageData;
    if (ui->btn_sharpen->text() == "一键锐化") {
        qDebug()<<"in sharpen";
        std::vector<uint8_t> blurImageData = function.Gauss(tempImageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),10);
        std::vector<uint8_t> highContrast = function.HighContrast(tempImageData, blurImageData);
        std::vector<uint8_t> sharpenImageData = function.Sharpen(tempImageData, highContrast);
        ShowImage(sharpenImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isSharpen=true;

        // 更新按钮文本
        ui->btn_sharpen->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_sharpen->setText("一键锐化");
    }


}


void MainWindow::on_btn_tailor_clicked()
{

    qDebug() << "I am in a tailor window!";
    Tailor *tailor = new Tailor(this,myValue);
    tailor->show();
}


void MainWindow::on_btn_tensor_clicked()
{
    std::vector<uint8_t>tempImageData=imageData;
    if (ui->btn_tensor->text() == "边缘检测") {
        qDebug()<<"in edge";
        std::vector<uint8_t> edgeImageData = function.SobelEdge(imageData, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());

        ShowImage(edgeImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
        isTensor=true;

        // 更新按钮文本
        ui->btn_tensor->setText("取消");
    } else {
        // 恢复原始图像
        ResetImage(myValue);
        // 更新按钮文本
        ui->btn_tensor->setText("边缘检测");
    }
}


void MainWindow::on_btn_threshold_ok_clicked()
{
    std::vector<uint8_t>tempImageData=imageData;
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
        function.ApplyThreshold(tempImageData,returnValue.value);
        ShowImage(tempImageData,myValue.bmpInfo.GetWidth(),myValue.bmpInfo.GetHeight());
    }
}


void MainWindow::on_btn_inter_clicked()
{
    qDebug() << "I am in a inter window!";
    Interpolation *inter = new Interpolation(this,myValue);
    inter->show();
}

