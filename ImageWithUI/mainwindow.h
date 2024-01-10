#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include "BMPFile.h"
#include "Function.h"
#include "blendwindow.h"
//QString test_path="D:\\Documents\\1Projects\\QT\\ImageWithUI\\ImageWithUI\\image\\Lenna.bmp";
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    //reset image
    void ResetImage(MyValue &myValue);
    ~MainWindow();
public:
    //QImage
    QImage m_bmpImage;
    MyValue myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //store original image data
    std::vector<uint8_t> originalImageData;

    //create a object of object
    Function function;
private slots:
    //click menubar to open image
    void on_openImage_triggered();

    void on_btn_gray_clicked();

    void on_btn_autoContrast_clicked();

    void on_btn_averBlur_clicked();

    void on_btn_small_biCubic_clicked();

    void on_btn_large_biCubic_clicked();

    void on_btn_resetAll_clicked();

    void on_btn_small_bilinear_clicked();

    void on_btn_large_bilinear_clicked();

    void on_btn_blend_clicked();

private:
    Ui::MainWindow *ui;
    BlendWindow * blendWindow;
};
#endif // MAINWINDOW_H
