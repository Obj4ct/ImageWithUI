#include "mask.h"
#include "ui_mask.h"
Mask::Mask(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Mask), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    setWindowTitle(QString("磨皮"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    newValue=myValue;
    m_bmpImage = QImage(newValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(),QImage::Format_BGR888);
    imageData=newValue.imageData;
    // 进行垂直翻转
    m_bmpImage = m_bmpImage.mirrored(false, true);
    // 显示图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(m_bmpImage);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true);
    Function function;

}

Mask::~Mask()
{
    delete ui;
}

void Mask::ShowImage(std::vector<uint8_t> &inImageData)
{
    QImage image(inImageData.data(), newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight(), QImage::Format_BGR888);

    // 进行垂直翻转
    image = image.mirrored(false, true);

    // 显示灰度图像在imageLabel上
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->imageLabel->setPixmap(pixmap);
    ui->imageLabel->setScaledContents(true); // 使图像适应 label 大小
}

void Mask::ResetImage()
{
    newValue.imageData=myValue.imageData;
    // 恢复原始图像
    QImage originalImage(myValue.imageData.data(), myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight(), QImage::Format_BGR888);
    originalImage = originalImage.mirrored(false, true);
    QPixmap originalPixmap = QPixmap::fromImage(originalImage);
    ui->imageLabel->setPixmap(originalPixmap);
    firstX=0;
    firstY=0;
    secondX=0;
    secondY=0;
    QString str1 = QString("%1, %2").arg(firstX).arg(firstY);
    ui->label_firstPos->setText(str1);

    QString str2 = QString("%1, %2").arg(secondX).arg(secondY);
    ui->label_secondPos->setText(str2);
}

void Mask::LocalSkinSmoothing(std::vector<uint8_t> &imageData,std::promise<std::vector<uint8_t>>& result, int width, int height, int startX, int startY, int endX, int endY, int radius)
{
    std::vector<uint8_t> smoothedData = imageData;

        for (int y = startY; y < endY; y++) {
            for (int x = startX; x < endX; x++) {
                double r = 0.0;
                double g = 0.0;
                double b = 0.0;
                double weightSum = 0.0;

                for (int j = -radius; j <= radius; j++) {
                    for (int i = -radius; i <= radius; i++) {
                        int pixelX = x + i;
                        int pixelY = y + j;
                        if (pixelX >= startX && pixelX < endX && pixelY >= startY && pixelY < endY) {
                            int pixelIndex = (pixelY * width + pixelX) * 3;
                            double weight =function.Gaussian(radius, i, j);
                            r += static_cast<double>(imageData[pixelIndex]) * weight;
                            g += static_cast<double>(imageData[pixelIndex + 1]) * weight;
                            b += static_cast<double>(imageData[pixelIndex + 2]) * weight;
                            weightSum += weight;
                        }
                    }
                }

                int index = (y * width + x) * 3;
                smoothedData[index] = static_cast<uint8_t>(r / weightSum);
                smoothedData[index + 1] = static_cast<uint8_t>(g / weightSum);
                smoothedData[index + 2] = static_cast<uint8_t>(b / weightSum);
            }
        }

        result.set_value(smoothedData);
}

void Mask::on_btn_save_clicked()
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
//core function

void Mask::on_btn_ok_clicked()
{
    QString inputText= ui->lineEdit_radius->text(); // 获取输入的文本
    bool isNumeric;
    double_t value = inputText.toDouble(&isNumeric);
    if(ui->label_firstPos->text()==nullptr||ui->label_secondPos->text()==nullptr||firstX==0||firstY==0||secondX==0||secondY==0)
    {
        qDebug()<<"not ok";
        if(!function.CreateMessagebox("提示","还没成功获取坐标值"))
        {
            return;
        }
    }else if(!isNumeric){
        QMessageBox* myBox = new QMessageBox;
        QPushButton* okBtn = new QPushButton("确定");
        QString str = "请输入数字";
        myBox->setWindowTitle("提示");
        myBox->setText(str);
        myBox->setWindowIcon(QIcon(":/icon/what.png"));
        myBox->addButton(okBtn, QMessageBox::AcceptRole);
        myBox->show();
        myBox->exec();//阻塞等待用户输入

        if (myBox->clickedButton() == okBtn)
        {
            return;
        }
    }
    else {
        qDebug()<<"click ok";

        std::promise<std::vector<uint8_t>> promise;
        std::future<std::vector<uint8_t>> future = promise.get_future();
        std::thread thread(&Mask::LocalSkinSmoothing, this, std::ref(newValue.imageData), std::ref(promise), newValue.bmpInfo.GetWidth(), newValue.bmpInfo.GetHeight(), firstX, firstY, secondX, secondY, value);

        // 等待异步任务完成并获取值
        std::vector<uint8_t> resultImage = future.get();

        thread.join();
        // std::vector<uint8_t> smoothData = LocalSkinSmoothing(newValue.imageData, newValue.bmpInfo.GetWidth(),newValue.bmpInfo.GetHeight() ,firstX, firstY, secondX, secondY, value);
        newValue.imageData = resultImage;
        ShowImage(resultImage);

    }
}


void Mask::on_btn_reset_clicked()
{
    ResetImage();
}

void Mask::mousePressEvent(QMouseEvent *event)
{
    QPoint clickPos = event->pos();
    if (clickCount == 0) {

        // 第一次点击，保存坐标到firstClick
        firstClick = clickPos;
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "First click at: " << firstClick;
        clickCount++;
        firstX=firstClick.x();
        firstY=firstClick.y();
        QString str = QString("%1, %2").arg(firstX).arg(firstY);
        ui->label_firstPos->setText(str);
    } else if (clickCount == 1) {
        // 第二次点击，保存坐标到secondClick
        secondClick = clickPos;
        qDebug()<<"current click count is "<<clickCount;
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        secondX=secondClick.x();
        secondY=secondClick.y();
        QString str = QString("%1, %2").arg(secondX).arg(secondY);
        ui->label_secondPos->setText(str);

    }


}

