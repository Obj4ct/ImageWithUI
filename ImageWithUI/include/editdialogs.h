#ifndef EDITDIALOGS_H
#define EDITDIALOGS_H

#include "QTHeader.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>

// 裁剪对话框：在缩放后的预览图上拖拽框选，返回“图片像素坐标”下的选区
class CropDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CropDialog(const QImage &displayImage, QWidget *parent = nullptr);
    // 选区(图片像素坐标，视觉坐标，左上角为 (x,y))
    QRect selectionInImage() const { return m_selection; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint toImagePoint(const QPoint &widgetPos) const;
    void refreshHint();

    QImage m_image;        // 正立的预览图
    QPixmap m_scaled;      // 按比例缩放后的版本
    QSize m_viewSize;
    double m_scale = 1.0;
    QPoint m_anchor;       // 图片坐标
    QPoint m_current;      // 图片坐标
    QRect m_selection;     // 图片坐标
    bool m_dragging = false;
    QLabel *m_hint = nullptr;
    QPushButton *m_okBtn = nullptr;
};

// 3×3 卷积核对话框：九个系数 + 归一化除数 + 偏移，内置常用预设
class KernelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit KernelDialog(QWidget *parent = nullptr);
    std::vector<double> kernel() const;
    double scale() const;
    double offset() const;

private:
    void applyPreset(int index);

    QDoubleSpinBox *m_cells[9] = {nullptr};
    QDoubleSpinBox *m_scaleSpin = nullptr;
    QDoubleSpinBox *m_offsetSpin = nullptr;
};

// USM 锐化参数对话框：半径 / 强度 / 保护阈值
class USMDialog : public QDialog
{
    Q_OBJECT
public:
    explicit USMDialog(QWidget *parent = nullptr);
    double radius() const;
    double amount() const;      // 0~3 的倍率(界面以百分比显示)
    double threshold() const;   // 0~255

private:
    QDoubleSpinBox *m_radius = nullptr;
    QSpinBox *m_amount = nullptr;
    QSpinBox *m_threshold = nullptr;
};

// 缩放对话框：目标宽高 + 锁定比例 + 插值算法
class ScaleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScaleDialog(int currentWidth, int currentHeight, QWidget *parent = nullptr);
    QSize targetSize() const;
    // 0 快速 / 1 平滑 / 2 最佳
    int algorithm() const;

private:
    int m_initialWidth = 1;
    int m_initialHeight = 1;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QCheckBox *m_lockRatio = nullptr;
    QComboBox *m_algorithm = nullptr;
};

#endif // EDITDIALOGS_H
