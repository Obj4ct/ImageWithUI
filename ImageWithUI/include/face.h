#ifndef FACE_H
#define FACE_H
#include "QTHeader.h"
#include <QTimer>
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
class MainWindow;
namespace Ui {
class Face;
}

class Face : public QWidget
{
    Q_OBJECT

public:
    explicit Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Face();
protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void on_btn_ok_clicked();


    void on_btn_apply_clicked();

private:
    Ui::Face *ui;
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
    bool hasFirst = false;   // 是否已取到中心点(再次点击可移动中心)
    double warpIntensity = 5;
    int32_t faceCenterX = 0;
    int32_t faceCenterY = 0;
    int32_t faceRadius = 0;
private:
    void ResetImage();
    void ShowImage(std::vector<uint8_t>&inImageData);
    // 参数/中心点变化后延时重算预览
    void SchedulePreview();
    // 以原图为基准重算一次预览；参数非法时回滚到原图
    bool RunPreview(bool reportErrors);
    // 预览结果(应用时写回它，原图 imageData 始终保持不变)
    std::vector<uint8_t> previewData;
    QTimer *previewTimer = nullptr;
    //瘦脸：读 src 写 dst，避免原地覆盖造成的伪影
    void _Face(const std::vector<uint8_t> &src, std::vector<uint8_t> &dst,
               int32_t width, int32_t height,
               int32_t centerX, int32_t centerY, int32_t radius, double intensity);


};

#endif // FACE_H
