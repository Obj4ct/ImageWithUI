#ifndef FACE_H
#define FACE_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
class MainWindow;
namespace Ui {
class Face;
}

class Face : public QWidget
{
    Q_OBJECT

public:
    explicit Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Face();
protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void on_btn_ok_clicked();

    void on_btn_ok_2_clicked();

    void on_btn_save_clicked();

private:
    Ui::Face *ui;
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
    int32_t faceCenterX;
    int32_t faceCenterY;
    int32_t faceRadius;
private:
    void ResetImage();
    void ShowImage(std::vector<uint8_t>&inImageData);
    //瘦脸
    void _Face(std::vector<uint8_t> &imageData, int32_t width, int32_t height,
                  int32_t centerX, int32_t centerY, int32_t radius, double intensity);


};

#endif // FACE_H
