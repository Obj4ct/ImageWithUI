#ifndef COLORMAP_H
#define COLORMAP_H
#include "QTHeader.h"
#include "BMPFile.h"
#include "Debug.h"
#include "Function.h"
class MainWindow;
namespace Ui {
class ColorMap;
}
class ColorMap : public QWidget
{
    Q_OBJECT

public:
    explicit ColorMap(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~ColorMap();

private slots:
    void on_comboBox_colorMap_currentIndexChanged(int index);


    void on_btn_save_clicked();

    void on_btn_apply_clicked();

private:
    Ui::ColorMap *ui;
    MainWindow* mainWindow;
    Function function;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t>imageData;
    QImage m_bmpImage;
private:
    void _ColorMap(std::vector<uint8_t> &imageData, std::vector<uint8_t> &colorMap);
    void ShowImage(std::vector<uint8_t>&inImageData);
    void ResetImage();
};

#endif // COLORMAP_H
