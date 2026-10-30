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
    previewData = imageData;
    newValue = myValue;
    const QImage image = Function::ToDisplayImage(imageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    // 显示原始图像在imageLabel上
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));

    // 参数或坐标变化后自动预览：停止输入 250ms 就从原图重算一次
    previewTimer = new QTimer(this);
    previewTimer->setSingleShot(true);
    previewTimer->setInterval(250);
    connect(previewTimer, &QTimer::timeout, this, [this]() { RunPreview(false); });
    connect(ui->lineEdit_range, &QLineEdit::textChanged, this, &eye::SchedulePreview);
    connect(ui->lineEdit_increase, &QLineEdit::textChanged, this, &eye::SchedulePreview);
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
    SchedulePreview();   // 坐标变了就重新预览
}

void eye::SchedulePreview()
{
    if (previewTimer != nullptr) {
        previewTimer->start();
    }
}

bool eye::RunPreview(bool reportErrors)
{
    if (previewTimer != nullptr) {
        previewTimer->stop();
    }

    const ReturnValue returnRange = mainWindow->CheckOK(ui->lineEdit_range);
    const ReturnValue returnNum = mainWindow->CheckOK(ui->lineEdit_increase);
    QString error;
    if (returnRange.isNull || returnNum.isNull) {
        error = "请输入";
    } else if (!returnRange.isNumeric || !returnNum.isNumeric) {
        error = "输入数字";
    } else if (returnRange.value <= 0) {
        error = "半径必须大于 0";
    } else if (returnNum.value <= 0 || returnNum.value > 1) {
        error = "放大强度需在 0 和 1 之间（如 0.3 约放大 1.4 倍）";
    } else if (reportErrors && !hasFirst) {
        error = "还没成功获取坐标值";
    }

    if (!error.isEmpty()) {
        if (reportErrors) {
            function.CreateMessagebox("提示", error);
        }
        previewData = imageData;   // 参数不完整：回到原图
        ShowImage(previewData);
        return false;
    }

    const int32_t width = newValue.bmpInfo.GetWidth();
    const int32_t height = newValue.bmpInfo.GetHeight();
    const int32_t radius = static_cast<int32_t>(returnRange.value);
    const double strength = returnNum.value;

    // 每次都从原图重算，反复调参数不会层层叠加
    previewData = imageData;
    // 两次点击对应左右两只眼睛，逐个做径向放大(圆外像素不受影响)
    if (hasFirst) {
        Function::EnlargeEye(previewData, width, height, firstX, firstY, radius, strength);
    }
    if (hasSecond) {
        Function::EnlargeEye(previewData, width, height, secondX, secondY, radius, strength);
    }
    ShowImage(previewData);
    return true;
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
    qDebug() << "click preview";
    RunPreview(true);
}

void eye::on_btn_apply_clicked()
{
    qDebug() << "apply";
    // 先保证预览是最新参数的结果；参数非法时不写回主窗口
    if (!RunPreview(true)) {
        return;
    }
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(previewData);
}
