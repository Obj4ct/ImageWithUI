#include "mainwindow.h"
#include "eye.h"
#include "ui_eye.h"

eye::eye(MainWindow* mainWindow, MyValue myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::eye), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    // 清掉 .ui 里自带的浅色样式，统一交给全局主题(Theme.h)接管
    setStyleSheet(QString());
    setWindowTitle(QString("眼部放大"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    //默认显示原始图像
    imageData = mainWindow->imageData;
    newValue = myValue;
    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    // 显示原始图像在imageLabel上
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void eye::mousePressEvent(QMouseEvent* event)
{
    const QPixmap *pixmap = ui->imageLabel->pixmap();
    if (pixmap == nullptr || pixmap->isNull()) {
        return;
    }
    // 事件坐标是控件坐标，需要先转换到 imageLabel 的局部坐标系
    const QPoint labelPos = ui->imageLabel->mapFrom(this, event->pos());
    // imageLabel 使用 AlignCenter 显示图像，必须扣掉图像相对控件左上角的居中偏移，
    // 否则点击位置会整体偏移(偏移量=(控件尺寸-图像尺寸)/2)，液化中心自然对不上。
    const int offsetX = (ui->imageLabel->width() - pixmap->width()) / 2;
    const int offsetY = (ui->imageLabel->height() - pixmap->height()) / 2;
    const int displayX = labelPos.x() - offsetX;
    const int displayY = labelPos.y() - offsetY;
    if (displayX < 0 || displayY < 0 ||
        displayX >= pixmap->width() || displayY >= pixmap->height()) {
        return;
    }
    const int32_t imageWidth = newValue.bmpInfo.GetWidth();
    const int32_t imageHeight = newValue.bmpInfo.GetHeight();
    if (imageWidth <= 0 || imageHeight <= 0) {
        return;
    }
    // 显示图与像素一一对应；按比例换算，兼容未来启用缩放显示的情况
    const int imageX = static_cast<int>(
            static_cast<long long>(displayX) * imageWidth / pixmap->width());
    // 图像是自底向上存储的，视觉行 y 对应内存行 (height-1-y)
    const int imageY = imageHeight - 1 - static_cast<int>(
            static_cast<long long>(displayY) * imageHeight / pixmap->height());

    if (clickCount == 0) {
        // 第一次点击，保存坐标到firstClick
        firstClick = QPoint(imageX, imageY);
        qDebug() << "First click at: " << firstClick;
        clickCount++;
        hasFirst = true;
        firstX = firstClick.x();
        firstY = firstClick.y();
        const QString str = QString("%1, %2").arg(firstX).arg(newValue.bmpInfo.GetHeight() - 1 - firstY);
        ui->label_first->setText(str);
    } else {
        // 第二次点击，保存坐标到secondClick
        secondClick = QPoint(imageX, imageY);
        qDebug() << "Second click at: " << secondClick;
        clickCount = 0;
        hasSecond = true;
        secondX = secondClick.x();
        secondY = secondClick.y();
        const QString str = QString("%1, %2").arg(secondX).arg(newValue.bmpInfo.GetHeight() - 1 - secondY);
        ui->label_second->setText(str);
    }
}

void eye::ShowImage(std::vector<uint8_t>&inImageData)
{
    const QImage image = Function::ToDisplayImage(inImageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

eye::~eye()
{
    delete ui;
}

void eye::on_btn_ok_clicked()
{
    qDebug() << "click ok";
    ReturnValue returnRange = mainWindow->CheckOK(ui->lineEdit_range);
    ReturnValue returnNum = mainWindow->CheckOK(ui->lineEdit_increase);
    if (returnRange.isNull || returnNum.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnRange.isNumeric || !returnNum.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnRange.value <= 0) {
        function.CreateMessagebox("提示", "半径必须大于 0");
        return;
    }
    // 强度是"径向收缩比例"，取值 0~1；0.3 约放大 1/(1-0.3)=1.43 倍
    if (returnNum.value <= 0 || returnNum.value > 1) {
        function.CreateMessagebox("提示", "放大强度需在 0 和 1 之间（如 0.3 约放大 1.4 倍）");
        return;
    }
    if (!hasFirst || !hasSecond) {
        function.CreateMessagebox("提示", "还没成功获取两个坐标值");
        return;
    }

    const int32_t width = newValue.bmpInfo.GetWidth();
    const int32_t height = newValue.bmpInfo.GetHeight();
    const int32_t radius = static_cast<int32_t>(returnRange.value);
    const double strength = returnNum.value;

    // 两次点击对应左右两只眼睛，逐个做径向放大(圆外像素不受影响)
    Function::EnlargeEye(imageData, width, height, firstX, firstY, radius, strength);
    Function::EnlargeEye(imageData, width, height, secondX, secondY, radius, strength);

    ShowImage(imageData);
}

void eye::on_btn_apply_clicked()
{
    qDebug() << "apply";
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(imageData);
}
