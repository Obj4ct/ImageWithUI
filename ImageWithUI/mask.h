#ifndef MASK_H
#define MASK_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
class MainWindow;
namespace Ui {
class Mask;
}

class Mask : public QWidget
{
    Q_OBJECT

public:
    explicit Mask(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Mask();

private slots:
    void on_btn_save_clicked();

    void on_btn_ok_clicked();

    void on_btn_reset_clicked();
protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    Ui::Mask *ui;
    MainWindow* mainWindow;
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
    double warpIntensity = 5;
private:
    void ShowImage(std::vector<uint8_t>&inImageData);
    void ResetImage();
    //需要用到function类里面的高斯模糊函数
    std::vector<uint8_t> LocalSkinSmoothing(std::vector<uint8_t> &imageData, int width, int height,
                                            int startX, int startY, int endX, int endY, int radius);
};

#endif // MASK_H
