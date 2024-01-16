#ifndef TAILOR_H
#define TAILOR_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
class MainWindow;
namespace Ui {
class Tailor;
}

class Tailor : public QWidget
{
    Q_OBJECT

public:
    explicit Tailor(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Tailor();

protected:
    void mousePressEvent(QMouseEvent* event) override;
private slots:
    void on_btn_ok_clicked();


    void on_btn_save_clicked();

    void on_btn_reset_clicked();

private:
    Ui::Tailor *ui;
    MainWindow* mainWindow;
    BMPInfo bmpInfo;
    Function function;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t>imageData;
    QImage m_bmpImage;
    QPoint firstClick;
    QPoint secondClick;
    int firstX;
    int firstY;
    int secondX;
    int secondY;
    int clickCount=0;
private:
    //内容处理
    void TailorImg(int32_t startX, int32_t startY, int32_t endX, int32_t endY,
                   std::vector<uint8_t> &imageData, BMPInfo &newBmpInfo, BMP &bmp, uint32_t originWidth);
    void ShowImage(std::vector<uint8_t>&inImageData);
    void ResetImage();
};

#endif // TAILOR_H
