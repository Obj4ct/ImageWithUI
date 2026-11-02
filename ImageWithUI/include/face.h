#ifndef FACE_H
#define FACE_H
#include "QTHeader.h"
#include <QTimer>
#include "Debug.h"
#include "BMPFile.h"
#include "Function.h"
class MainWindow;
class QComboBox;
class QSlider;
namespace Ui {
class Face;
}

// 图片微调窗口(沿用 Face 这个类名/文件名，功能已从"人像瘦脸"扩展为通用局部微调)
class Face : public QWidget
{
    Q_OBJECT

public:
    // 局部微调效果，参考 Photoshop 液化工具的做法
    enum RetouchEffect {
        Pucker = 0,   // 收拢(瘦脸)
        Bloat = 1,    // 膨胀(放大)
        Smooth = 2,   // 平滑(柔化)
        Twirl = 3,    // 漩涡
        Push = 4      // 推移(拖动)
    };

    explicit Face(MainWindow* mainWindow, MyValue myValue, QWidget *parent = nullptr);
    ~Face();

    // 在 work(BGR 紧凑、自底向上)上就地施加一次微调。
    // deltaX/deltaY 只对"推移"生效，单位是图像像素(与内存行方向一致)。
    static void ApplyEffect(std::vector<uint8_t> &work, int width, int height,
                            RetouchEffect effect, int centerX, int centerY,
                            int radius, double strength,
                            int deltaX = 0, int deltaY = 0);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void on_btn_ok_clicked();       // 重置(恢复打开窗口时的图像)
    void on_btn_apply_clicked();    // 写回主窗口并记入全局历史

private:
    Ui::Face *ui;
    MainWindow* mainWindow;
    Function function;
    MyValue myValue;
    MyValue newValue;
    std::vector<uint8_t> imageData;   // 打开窗口时的原始图像(重置基准)
    std::vector<uint8_t> m_work;      // 当前微调结果(可反复涂抹累积)
    QComboBox *m_effectCombo = nullptr;
    QLineEdit *m_strengthEdit = nullptr;
    QSlider *m_strengthSlider = nullptr;
    QSlider *m_radiusSlider = nullptr;
    bool m_stroking = false;
    QPoint m_lastPos;                 // 上一次落笔的图像坐标

private:
    void ShowImage(std::vector<uint8_t>&inImageData);
    // 控件坐标 → 图像像素坐标(失败返回 false)
    bool PointToImage(const QPoint &widgetPos, QPoint *imagePos) const;
    // 读取半径/力度(非法输入走默认值)
    int CurrentRadius() const;
    double CurrentStrength() const;
    // 在某个点落笔一次
    void ApplyAt(const QPoint &imagePos, int deltaX, int deltaY);
};

#endif // FACE_H
