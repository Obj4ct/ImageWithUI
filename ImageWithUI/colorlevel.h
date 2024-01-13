#ifndef COLORLEVEL_H
#define COLORLEVEL_H
#include "QTHeader.h"
#include "Debug.h"
#include "BMPFile.h"
//#include "mainwindow.h"
class MainWindow;
namespace Ui {
class ColorLevel;
}

class ColorLevel : public QWidget
{
    Q_OBJECT

public:
   explicit ColorLevel(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~ColorLevel();

private:
    Ui::ColorLevel *ui;
    MainWindow* mainWindow;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t>imageData;
    QImage m_bmpImage;
};

#endif // COLORLEVEL_H
