#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include "BMPFile.h"
#include "Function.h"
//QString test_path="D:\\Documents\\1Projects\\QT\\ImageWithUI\\ImageWithUI\\image\\Lenna.bmp";
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    //test function, removeable
    void TestImage();
    ~MainWindow();
public:
    //QImage
    QImage m_bmpImage;
    MyValue myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //create a object of object
    Function function;
private slots:
    //click menubar to open image
    void on_openImage_triggered();

    void on_btn_gray_clicked();

private:
    Ui::MainWindow *ui;

};
#endif // MAINWINDOW_H
