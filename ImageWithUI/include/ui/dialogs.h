/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

#ifndef EDITDIALOGS_H
#define EDITDIALOGS_H

#include "qt_headers.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>

// ---- 统一的「滑块 + 数值框」参数控件 ----
// 滑块与数值框始终双向同步：拖滑块数字跟着变，改数字滑块跟着走
// 数值框在上、滑块在下的一整行(带标签)
QWidget *MakeSliderRow(const QString &label, QSpinBox *spin, QWidget *parent = nullptr);
QWidget *MakeSliderRow(const QString &label, QDoubleSpinBox *spin, QWidget *parent = nullptr);
// 只有数值框 + 滑块的紧凑单元(用于 3×3 网格)
QWidget *MakeSliderCell(QSpinBox *spin, QWidget *parent = nullptr);
QWidget *MakeSliderCell(QDoubleSpinBox *spin, QWidget *parent = nullptr);

// 通用数值对话框：每行一个「标签 + 数值框 + 滑块」，确认后按序号取值
struct NumberParam {
    QString label;
    double min = 0.0;
    double max = 1.0;
    double value = 0.0;
    double step = 1.0;
    int decimals = 0;   // 0 表示整数
};

class NumberDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NumberDialog(const QString &title, const QList<NumberParam> &params,
                          QWidget *parent = nullptr);
    double value(int index) const;

signals:
    void paramsChanged();

private:
    QList<QDoubleSpinBox *> m_spins;
};

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

signals:
    void paramsChanged();   // 任一参数变化(用来做实时预览)

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

signals:
    void paramsChanged();   // 任一参数变化(用来做实时预览)

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
