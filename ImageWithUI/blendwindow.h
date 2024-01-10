#ifndef BLENDWINDOW_H
#define BLENDWINDOW_H
#include "QTHeader.h"
#include "BMPFile.h"
#include "Debug.h"
namespace Ui {
class BlendWindow;
}

class BlendWindow : public QWidget
{
    Q_OBJECT

public:
    explicit BlendWindow(QWidget *parent = nullptr);
    ~BlendWindow();
public:
    QImage bmpImage_1;
    QImage bmpImage_2;
    MyValue myValue_1;
    MyValue myValue_2;
    //store image data
    std::vector<uint8_t>imageData_1;
    std::vector<uint8_t>imageData_2;
    //store original image data
    std::vector<uint8_t> originalImageData_1;
    std::vector<uint8_t> originalImageData_2;
private slots:

    void on_btn_open_1_clicked();

    void on_btn_open_2_clicked();

private:
    Ui::BlendWindow *ui;
};

#endif // BLENDWINDOW_H
