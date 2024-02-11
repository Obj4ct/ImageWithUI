#ifndef EYE_H
#define EYE_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"

class MainWindow;
namespace Ui {
class eye;
}

class eye : public QWidget
{
    Q_OBJECT

public:
    explicit eye(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~eye();

private slots:

    void on_btn_ok_clicked();


    void on_btn_apply_clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    Ui::eye *ui;
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
    int clickCount=0;;
private:
    void Eye(std::vector<uint8_t> &imageData, int32_t width, int32_t height,
             int32_t centerX, int32_t centerY, int32_t radius, double intensity,size_t startRow,size_t endRow);
    void ShowImage(std::vector<uint8_t>&inImageData);
};

#endif // EYE_H
