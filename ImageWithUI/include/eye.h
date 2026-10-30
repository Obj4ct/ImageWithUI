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
    int firstX = 0;
    int firstY = 0;
    int secondX = 0;
    int secondY = 0;
    int clickCount = 0;
    bool hasFirst = false;   // 是否已经取到第一个点
    bool hasSecond = false;  // 是否已经取到第二个点
private:
    void ShowImage(std::vector<uint8_t>&inImageData);
};

#endif // EYE_H
