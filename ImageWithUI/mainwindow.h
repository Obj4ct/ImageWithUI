#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include"QTHeader.h"
#include "BMPFile.h"
#include "Function.h"
#include "blendwindow.h"
#include "eye.h"
#include "mask.h"
#include "mosaic.h"
#include "tailor.h"
#include "interpolation.h"
#include "face.h"
#include "colormap.h"
struct ReturnValue{
    bool isNull;
    bool isNumeric;
    double_t value;
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
    void SaveImageDataToHistory(std::vector<uint8_t>& imageData);
    void UndoImageProcessing();
    //清理每段数据,防止数据冲突引发异常
    void ClearSegmentData();
    ~MainWindow();
public:
    std::list<std::vector<uint8_t>> imageDataHistory;

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

    void on_btn_gray_clicked();

    void on_btn_autoContrast_clicked();

    void on_btn_averBlur_clicked();

    void on_btn_resetAll_clicked();

    void on_btn_blend_clicked();

    void on_btn_brightness_clicked();

    void on_btn_contrast_clicked();

    void on_btn_saturation_clicked();

    void on_btn_color_balance_clicked();

    void on_btn_colorMap_clicked();

    void on_btn_reverse_color_clicked();

    void on_btn_complementary_clicked();

    void on_btn_eye_clicked();

    void on_btn_face_clicked();

    void on_btn_fish_eye_clicked();

    void on_btn_gauss_ok_clicked();

    void on_btn_highContrast_ok_clicked();

    void on_btn_rotate_ok_clicked();

    void on_btn_rotate_r_clicked();

    void on_btn_mask_clicked();

    void on_btn_medianBlur_clicked();

    void on_btn_mosaic_clicked();

    void on_btn_shadow_ok_clicked();

    void on_btn_highlight_ok_clicked();

    void on_btn_sharpen_clicked();

    void on_btn_tailor_clicked();

    void on_btn_tensor_clicked();

    void on_btn_threshold_ok_clicked();

    void on_btn_inter_clicked();

    void on_btn_undo_clicked();

private:
    Ui::MainWindow *ui;
    BlendWindow * blendWindow;
    bool canSave=false;
    void closeEvent(QCloseEvent *event) override;

};
#endif // MAINWINDOW_H
