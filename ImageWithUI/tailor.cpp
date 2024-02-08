#include "mainwindow.h"
#include "tailor.h"
#include "ui_tailor.h"
Tailor::Tailor(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Tailor), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    setWindowTitle(QString("图像裁剪"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    newValue=myValue;
    m_bmpImage = QImage(newValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
    imageData=newValue.imageData;
    // 进行垂直翻转
    m_bmpImage = m_bmpImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);

    Function function;

}

//void Tailor::TailorImg(int32_t cropX, int32_t cropY, int32_t cropHeight, int32_t cropWidth, std::vector<uint8_t> &imageData,
//               BMPInfo &newBmpInfo, BMP &bmp,uint32_t originWidth) {
//    qDebug()<<cropX;
//    qDebug()<<cropY;
//    qDebug()<<cropHeight;
//    qDebug()<<cropWidth;
//    bmpInfo.WriteToBMPInfo(imageData, cropHeight, cropWidth, newBmpInfo,bmp);
//    //qDebug()<<"ok";

//    for (int y = cropY; y < cropY + cropHeight; y++) {

//        for (int x = cropX; x < cropX + cropWidth; x++) {
//            uint32_t originIndex = (y * originWidth + x) * 3;
//            int index = ((y - cropY) * cropWidth + (x - cropX)) * 3;
//            imageData[index] = imageData[originIndex];     // Red
//            imageData[index + 1] = imageData[originIndex + 1]; // Green
//            imageData[index + 2] = imageData[originIndex + 2]; // Blue

//        }

//    }


//}
bool Tailor::TailorImg(int32_t cropX, int32_t cropY, int32_t cropHeight, int32_t cropWidth, std::vector<uint8_t> &imageData,
                       BMPInfo &newBmpInfo, BMP &bmp, uint32_t originWidth,uint32_t originHeight) {
    bmpInfo.WriteToBMPInfo(imageData, cropHeight, cropWidth, newBmpInfo, bmp);

    qDebug()<<cropX;
    qDebug()<<cropY;
    qDebug()<<cropWidth;
    qDebug()<<cropHeight;
    qDebug()<<originWidth;
    qDebug()<<originHeight;

    // 检查裁剪范围是否合法
    if (cropX < 0 || cropY < 0 || cropWidth <= 0 || cropHeight <= 0 ||
        cropX + cropWidth > originWidth || cropY + cropHeight > originHeight) {
        qDebug() << "Invalid crop dimensions. Check the specified coordinates and size.";
        return false;
    }

    for (int y = cropY; y < cropY + cropHeight; y++) {
        for (int x = cropX; x < cropX + cropWidth; x++) {
            uint32_t originIndex = (y * originWidth + x) * 3;
            int index = ((y - cropY) * cropWidth + (x - cropX)) * 3;

            // 请注意这里调整为 RGB 顺序
            imageData[index] = myValue.imageData[originIndex];       // Blue
            imageData[index + 1] = myValue.imageData[originIndex + 1]; // Green
            imageData[index + 2] = myValue.imageData[originIndex + 2]; // Red
        }
    }
    return true;
}

void Tailor::ShowImage(std::vector<uint8_t>& imageData, int32_t width, int32_t height)
{
    // 创建新的窗口对象
    newWindow = new QMainWindow();
    // 创建一个 QLabel 来显示图像
    QLabel* imageLabel = new QLabel(newWindow);

    // 使用 QImage 加载图像数据
    QImage newImage(imageData.data(), width, height, QImage::Format_BGR888);
    newImage = newImage.mirrored(false, true);

    // 设置 QLabel 的图像
    imageLabel->setPixmap(QPixmap::fromImage(newImage));


    // 设置新窗口的中心部件为 QLabel
    newWindow->setCentralWidget(imageLabel);

    // 显示新窗口
    newWindow->show();
}




Tailor::~Tailor()
{
    delete ui;
}



void Tailor::on_btn_ok_clicked()
{
    //正确输入数字
    ReturnValue returnValueX=mainWindow->CheckOK(ui->lineEdit_posX);
    ReturnValue returnValueY=mainWindow->CheckOK(ui->lineEdit_posY);
    ReturnValue returnValueWidth=mainWindow->CheckOK(ui->lineEdit_width);
    ReturnValue returnValueHeight=mainWindow->CheckOK(ui->lineEdit_height);

    if(returnValueX.isNull==true||returnValueY.isNull==true||returnValueWidth.isNull==true||returnValueHeight.isNull==true)
    {
        if(!function.CreateMessagebox("提示","请输入"))
            return;
    }else if(returnValueX.isNumeric==false||returnValueY.isNumeric==false||returnValueWidth.isNumeric==false||returnValueHeight.isNumeric==false){
        if(!function.CreateMessagebox("提示","输入数字"))
            return;
    }else{
        if(TailorImg(returnValueX.value, returnValueY.value, returnValueHeight.value, returnValueWidth.value, imageData,myValue.bmpInfo,myValue.bmp,newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight()))
        {
            ShowImage(imageData,returnValueWidth.value,returnValueHeight.value);
        }
        else{
            function.CreateMessagebox("提示","超出原始图像范围");
        }
    }
}

