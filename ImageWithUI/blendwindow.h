#ifndef BLENDWINDOW_H
#define BLENDWINDOW_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"

enum BlendMode {
    BLEND_NORMAL,     // 正常模式 在“正常”模式下，“混合色”的显示与不透明度的设置有关。
    BLEND_MULTIPLY,   // 正片叠底 其主要特点是以正片作为底图，再将其他图像叠加于其上，并根据叠加图像的颜色来调整底图的亮度和饱和度，产生丰富的合成效果。
    BLEND_SCREEN,     // 滤色 ps中滤色的作用结果和正片叠底刚好相反，它是将两个颜色的互补色的像素值相乘，然后除以255得到的最终色的像素值。通常执行滤色模式后的颜色都较浅。
    BLEND_OVERLAY,    // 叠加 　“叠加”模式把图像的“基色”颜色与“混合色”颜色相混合产生一种中间色。
    // “基色”内颜色比“混合色”颜色暗的颜色使“混合色”颜色倍增，比“混合色”颜色亮的颜色将使“混合色”颜色被遮盖，
    // 而图像内的高亮部分和阴影部分保持不变，因此对黑色或白色像素着色时“叠加”模式不起作用。
};
class MainWindow;
namespace Ui {
class BlendWindow;
}

class BlendWindow : public QWidget {
    Q_OBJECT

public:
    explicit BlendWindow(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~BlendWindow();

private slots:
    void on_btn_open_1_clicked();
    void on_btn_open_2_clicked();
    void on_btn_apply_clicked();
    void on_comboBox_blendMode_currentIndexChanged(int index);
    void on_horizontalSlider_alpha_valueChanged(int value);

private:
    Ui::BlendWindow *ui;
    MainWindow *mainWindow;
    MyValue myValue;
    QImage bmpImage_1;
    QImage bmpImage_2;
    QImage blendedImage;
    //first image value
    MyValue myValue_1;
    //second image value
    MyValue myValue_2;

    static int selectCount;
    //下拉框选项
    BlendMode currentBlendMode;
    double currentAlpha;
    //first image data
    std::vector<uint8_t> originalImageData_1;

    //seconde image data
    std::vector<uint8_t> originalImageData_2;

    std::vector<uint8_t> blendedImageData;



private:
    void SwitchBlendMode(uint8_t &destR, uint8_t &destG, uint8_t &destB,
                         uint8_t srcR, uint8_t srcG, uint8_t srcB,
                         BlendMode blendMode,double alpha);

    void Effect(std::vector<uint8_t> &imageData,
                const std::vector<uint8_t> &effectData,
                int width, int height,
                BlendMode blendMode,size_t start,size_t end);

    void BlendImages();
    void ShowImage();
    void UpdateBlendEffect();

};

#endif // BLENDWINDOW_H
