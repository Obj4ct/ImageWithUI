#ifndef MOSAIC_H
#define MOSAIC_H

#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"

class MainWindow;
namespace Ui {
class Mosaic;
}

class Mosaic : public QWidget
{
    Q_OBJECT

public:
    explicit Mosaic(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Mosaic();

private slots:
    void on_btn_area_ok_clicked();

    void on_btn_full_ok_clicked();

    void on_btn_reset_clicked();

    void on_btn_save_clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    Ui::Mosaic *ui;
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
    void ShowImage(std::vector<uint8_t>& inImageData);
    void ResetImage();
    void FullMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height, uint32_t degree);
    void AreaMosaic(std::vector<uint8_t> &imageData, uint32_t width, uint32_t height,
                   uint32_t beginX, uint32_t beginY, uint32_t endX, uint32_t endY, uint32_t degree);
};

#endif // MOSAIC_H
