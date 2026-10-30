#ifndef EYE_H
#define EYE_H
#include "QTHeader.h"
#include <QTimer>
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
    // 当前预览结果(供自检使用)
    const std::vector<uint8_t> &PreviewData() const { return previewData; }

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
    // 参数/坐标变化后延时重算预览
    void SchedulePreview();
    // 以原图为基准重算一次预览；参数非法时回滚到原图
    // reportErrors 为 true 时(点"预览"/"应用")才弹提示，实时预览保持静默
    bool RunPreview(bool reportErrors);

    // 预览也是"从原图重算"，反复调整参数不会层层叠加
    std::vector<uint8_t> previewData;
    QTimer *previewTimer = nullptr;
};

#endif // EYE_H
