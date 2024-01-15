#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include "BMPFile.h"
#include "Function.h"
#include "blendwindow.h"
#include "colorlevel.h"
//QString test_path="D:\\Documents\\1Projects\\QT\\ImageWithUI\\ImageWithUI\\image\\Lenna.bmp";
struct ReturnValue{
    bool isNull;
    bool isNumeric;
    int32_t value;
};
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
    void ShowImage(std::vector<uint8_t> &inImageData,int32_t width,int32_t height);
    void ResetAll(MyValue &myValue);
    ReturnValue CheckOK(QLineEdit* lineEdit);
    ~MainWindow();
public:
    //QImage
    QImage m_bmpImage;
    MyValue myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //store original image data
    std::vector<uint8_t> originalImageData;
    bool isGray,isAutoContrast,isAverBlur,isColorBalance,isColorMap,isReverse,isComplementary;

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

    void on_btn_brightness_clicked();

    void on_btn_contrast_clicked();

    void on_btn_saturation_clicked();

    void on_btn_color_balance_clicked();

    void on_btn_color_level_clicked();

    void on_btn_colorMap_clicked();

    void on_btn_reverse_color_clicked();

    void on_btn_complementary_clicked();

private:
    Ui::MainWindow *ui;
    BlendWindow * blendWindow;
    ColorLevel * colorLevel;

};
#endif // MAINWINDOW_H
