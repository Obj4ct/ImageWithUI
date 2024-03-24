#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include "BMPFile.h"
#include "Function.h"
#include "blendwindow.h"
#include "eye.h"
#include "interpolation.h"
#include "face.h"
#include "colormap.h"

struct ReturnValue{
    bool isNull;
    bool isNumeric;
    double_t value;
};
enum OperationType
{
    UNDO,
    REDO
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
    void ShowImage(std::vector<uint8_t> &inImageData,MyValue value,int32_t width,int32_t height);
    void ResetAll(MyValue &myValue);
    ReturnValue CheckOK(QLineEdit* lineEdit);
    OperationType SaveImageDataToHistory(std::vector<uint8_t>& imageData);
    void UndoImageProcessing();
    void RedoImageProcessing();
    //清理每段数据,防止数据冲突引发异常
    void ClearSegmentData();
    ~MainWindow();
public:
    std::string BMPPath;
    std::list<std::vector<uint8_t>> imageDataHistory;
    std::list<std::vector<uint8_t>> redoImageDataHistory;
    //分段处理数据
    int num_threads;
    size_t imageSize;
    std::vector<std::thread> threads;              // 存储线程对象，每个线程对象将处理图像数据的不同部分
    size_t segmentSize; // 均分处理
    std::vector<size_t> segmentStarts; // 用来存储每个数据段的起始位置
    //QImage
    QImage m_bmpImage;
    MyValue myValue;
    //store image data
    std::vector<uint8_t>imageData;
    //store original image data
    std::vector<uint8_t> originalImageData;

    //create a object of object
    Function function;

    double sigma;
    double angle;
private slots:

    //click menubar to open image
    void on_openImage_triggered();
    void on_actionsave_triggered();


    void on_btn_resetAll_clicked();


    void on_btn_brightness_clicked();

    void on_btn_contrast_clicked();

    void on_btn_saturation_clicked();

    void on_btn_rotate_ok_clicked();

    void on_btn_rotate_r_clicked();



    void on_btn_shadow_ok_clicked();

    void on_btn_highlight_ok_clicked();

    void on_btn_sharpen_clicked();

    void on_btn_threshold_ok_clicked();

    void on_btn_undo_clicked();

    void on_actionmedian_triggered();

    void on_actionaver_triggered();

    void on_actiongauss_triggered();

    void on_actiongray_triggered();

    void on_actionreverse_triggered();

    void on_actioncomplementary_triggered();

    void on_actionautoContrast_triggered();

    void on_actioncolorBalance_triggered();

    void on_actionblend_triggered();

    void on_actionmosaic_triggered();

    void on_actionsobel_triggered();

    void on_actionliquefaction_triggered();

    void on_actioncolorMap_triggered();

    void on_actionfishEye_triggered();

    void on_actionface_triggered();

    void on_actionundo_triggered();

    void on_actionreset_triggered();


    void on_actionscale_triggered();

    void on_actionredo_triggered();

    void on_actionlarge_near_triggered();

    void on_actionsmall_near_triggered();

    void on_actionlarge_bilinear_triggered();

    void on_actionsmall_bilinear_triggered();

    void on_actionlarge_bicubic_triggered();

    void on_actionsmall_bicubic_triggered();
    void on_actionhighContrast_triggered();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    Ui::MainWindow *ui;
    BlendWindow * blendWindow;
    bool canSave=false;
    void closeEvent(QCloseEvent *event) override;
    void _SetVisible(bool set);
    void _SetShortCut();

};
#endif // MAINWINDOW_H
