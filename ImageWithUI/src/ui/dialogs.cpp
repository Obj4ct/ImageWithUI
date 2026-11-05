/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

#include "dialogs.h"

#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QLabel>
#include <QEvent>
#include <QColorDialog>
#include <QFontComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPixmap>
#include <functional>
#include <cmath>

// ------------------------------------------------------------------
// 滑块 + 数值框(所有数值参数统一走这一套控件)
// ------------------------------------------------------------------
namespace {

QSlider *NewSlider(QWidget *parent)
{
    QSlider *slider = new QSlider(Qt::Horizontal, parent);
    slider->setMinimumHeight(18);
    return slider;
}

void BindSliderToSpin(QSlider *slider, QSpinBox *spin)
{
    slider->setRange(spin->minimum(), spin->maximum());
    slider->setValue(spin->value());
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin](int value) {
        if (spin->value() != value) {
            QSignalBlocker blocker(spin);
            spin->setValue(value);
        }
    });
    QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider,
                     [slider](int value) {
                         if (slider->value() != value) {
                             QSignalBlocker blocker(slider);
                             slider->setValue(value);
                         }
                     });
}

// 小数参数：滑块按 10^decimals 换算成整数刻度
void BindSliderToSpin(QSlider *slider, QDoubleSpinBox *spin)
{
    const int scale = static_cast<int>(std::lround(std::pow(10.0, spin->decimals())));
    slider->setRange(static_cast<int>(std::lround(spin->minimum() * scale)),
                     static_cast<int>(std::lround(spin->maximum() * scale)));
    slider->setValue(static_cast<int>(std::lround(spin->value() * scale)));
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin, scale](int value) {
        QSignalBlocker blocker(spin);
        spin->setValue(static_cast<double>(value) / scale);
    });
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider,
                     [slider, scale](double value) {
                         QSignalBlocker blocker(slider);
                         slider->setValue(static_cast<int>(std::lround(value * scale)));
                     });
}

// 双击数值框恢复打开对话框时的初始值；方向键步进由 spin 自身负责(1 个 singleStep)
class ResetSpinFilter : public QObject
{
public:
    explicit ResetSpinFilter(std::function<void()> reset, QObject *parent = nullptr)
        : QObject(parent), m_reset(std::move(reset))
    {
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick) {
            m_reset();
            return true;
        }
        return false;
    }

private:
    std::function<void()> m_reset;
};

template <typename Spin>
void InstallSpinReset(Spin *spin)
{
    const auto initial = spin->value();
    spin->installEventFilter(new ResetSpinFilter([spin, initial]() {
        spin->setValue(initial);
    }, spin));
}

}   // namespace

QWidget *MakeSliderRow(const QString &label, QSpinBox *spin, QWidget *parent)
{
    QWidget *box = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    QHBoxLayout *top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(new QLabel(label, box));
    top->addStretch();
    top->addWidget(spin);
    layout->addLayout(top);

    InstallSpinReset(spin);
    QSlider *slider = NewSlider(box);
    BindSliderToSpin(slider, spin);
    layout->addWidget(slider);
    return box;
}

QWidget *MakeSliderRow(const QString &label, QDoubleSpinBox *spin, QWidget *parent)
{
    QWidget *box = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);

    QHBoxLayout *top = new QHBoxLayout();
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(new QLabel(label, box));
    top->addStretch();
    top->addWidget(spin);
    layout->addLayout(top);

    InstallSpinReset(spin);
    QSlider *slider = NewSlider(box);
    BindSliderToSpin(slider, spin);
    layout->addWidget(slider);
    return box;
}

QWidget *MakeSliderCell(QSpinBox *spin, QWidget *parent)
{
    QWidget *box = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(spin);
    InstallSpinReset(spin);
    QSlider *slider = NewSlider(box);
    BindSliderToSpin(slider, spin);
    layout->addWidget(slider);
    return box;
}

QWidget *MakeSliderCell(QDoubleSpinBox *spin, QWidget *parent)
{
    QWidget *box = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(spin);
    InstallSpinReset(spin);
    QSlider *slider = NewSlider(box);
    BindSliderToSpin(slider, spin);
    layout->addWidget(slider);
    return box;
}

// ------------------------------------------------------------------
// NumberDialog
// ------------------------------------------------------------------
NumberDialog::NumberDialog(const QString &title, const QList<NumberParam> &params,
                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    QVBoxLayout *root = new QVBoxLayout(this);
    for (const NumberParam &param : params) {
        QDoubleSpinBox *spin = new QDoubleSpinBox(this);
        spin->setRange(param.min, param.max);
        spin->setDecimals(param.decimals);
        spin->setSingleStep(param.step);
        spin->setValue(param.value);
        spin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        root->addWidget(MakeSliderRow(param.label, spin, this));
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() { emit paramsChanged(); });
        m_spins.append(spin);
    }

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QString("应用"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

double NumberDialog::value(int index) const
{
    if (index < 0 || index >= m_spins.size()) {
        return 0.0;
    }
    return m_spins.at(index)->value();
}

// ------------------------------------------------------------------
// CropDialog
// ------------------------------------------------------------------
CropDialog::CropDialog(const QImage &displayImage, QWidget *parent)
    : QDialog(parent),
      m_image(displayImage)
{
    setWindowTitle("裁剪图像 · 拖拽框选要保留的区域");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    const int maxW = 880;
    const int maxH = 600;
    double s = 1.0;
    if (m_image.width() > maxW || m_image.height() > maxH) {
        s = std::min(static_cast<double>(maxW) / m_image.width(),
                     static_cast<double>(maxH) / m_image.height());
    }
    m_scale = s;
    m_viewSize = QSize(std::max(64, static_cast<int>(m_image.width() * s)),
                       std::max(64, static_cast<int>(m_image.height() * s)));
    m_scaled = QPixmap::fromImage(
        m_image.scaled(m_viewSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    const int dialogW = std::max(m_viewSize.width(), 380);
    setFixedSize(dialogW, m_viewSize.height() + 52);

    m_hint = new QLabel(this);
    m_hint->setGeometry(8, m_viewSize.height() + 8, dialogW - 210, 28);

    m_okBtn = new QPushButton("应用裁剪", this);
    m_okBtn->setEnabled(false);
    m_okBtn->setGeometry(dialogW - 192, m_viewSize.height() + 8, 90, 30);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);

    QPushButton *cancelBtn = new QPushButton("取消", this);
    cancelBtn->setGeometry(dialogW - 96, m_viewSize.height() + 8, 80, 30);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    refreshHint();
}

QPoint CropDialog::toImagePoint(const QPoint &widgetPos) const
{
    const int x = std::max(0, std::min(m_viewSize.width() - 1, widgetPos.x()));
    const int y = std::max(0, std::min(m_viewSize.height() - 1, widgetPos.y()));
    const int ix = std::min(m_image.width() - 1, static_cast<int>(x / m_scale));
    const int iy = std::min(m_image.height() - 1, static_cast<int>(y / m_scale));
    return QPoint(std::max(0, ix), std::max(0, iy));
}

void CropDialog::refreshHint()
{
    if (!m_hint) {
        return;
    }
    if (m_selection.isEmpty()) {
        m_hint->setText(QString("在图上按住鼠标左键拖拽出裁剪框"));
    } else {
        m_hint->setText(QString("选区 %1 × %2 px @ (%3, %4)")
                            .arg(m_selection.width())
                            .arg(m_selection.height())
                            .arg(m_selection.x())
                            .arg(m_selection.y()));
    }
}

void CropDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QDialog::mousePressEvent(event);
        return;
    }
    if (!QRect(QPoint(0, 0), m_viewSize).contains(event->pos())) {
        return;
    }
    m_anchor = toImagePoint(event->pos());
    m_current = m_anchor;
    m_dragging = true;
    m_selection = QRect(m_anchor, QSize(1, 1));
    update();
}

void CropDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QDialog::mouseMoveEvent(event);
        return;
    }
    m_current = toImagePoint(event->pos());
    const int x = std::min(m_anchor.x(), m_current.x());
    const int y = std::min(m_anchor.y(), m_current.y());
    const int w = std::abs(m_current.x() - m_anchor.x()) + 1;
    const int h = std::abs(m_current.y() - m_anchor.y()) + 1;
    m_selection = QRect(x, y, w, h);
    refreshHint();
    update();
}

void CropDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging) {
        QDialog::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
    // 太小的选区视为误操作
    const bool valid = m_selection.width() >= 4 && m_selection.height() >= 4;
    m_okBtn->setEnabled(valid);
    refreshHint();
    update();
}

void CropDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(QRect(0, 0, width(), m_viewSize.height()), palette().window());
    painter.drawPixmap(0, 0, m_scaled);

    if (m_selection.isEmpty()) {
        return;
    }
    const QRect viewSel(static_cast<int>(m_selection.x() * m_scale),
                        static_cast<int>(m_selection.y() * m_scale),
                        std::max(1, static_cast<int>(m_selection.width() * m_scale)),
                        std::max(1, static_cast<int>(m_selection.height() * m_scale)));
    const int viewW = m_viewSize.width();
    const int viewH = m_viewSize.height();

    // 选区外压暗，突出保留区域
    const QColor dim(0, 0, 0, 110);
    painter.fillRect(QRect(0, 0, viewW, viewSel.top()), dim);
    painter.fillRect(QRect(0, viewSel.bottom() + 1, viewW, viewH - viewSel.bottom() - 1), dim);
    painter.fillRect(QRect(0, viewSel.top(), viewSel.left(), viewSel.height()), dim);
    painter.fillRect(QRect(viewSel.right() + 1, viewSel.top(),
                           viewW - viewSel.right() - 1, viewSel.height()), dim);

    painter.setPen(QPen(QColor(90, 170, 255), 2));
    painter.drawRect(viewSel.adjusted(-1, -1, 0, 0));
    // 三分线，便于构图
    painter.setPen(QPen(QColor(255, 255, 255, 120), 1, Qt::DashLine));
    for (int i = 1; i <= 2; ++i) {
        const int gx = viewSel.left() + viewSel.width() * i / 3;
        const int gy = viewSel.top() + viewSel.height() * i / 3;
        painter.drawLine(gx, viewSel.top(), gx, viewSel.bottom());
        painter.drawLine(viewSel.left(), gy, viewSel.right(), gy);
    }
}

// ------------------------------------------------------------------
// KernelDialog
// ------------------------------------------------------------------
KernelDialog::KernelDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("自定义滤镜（3×3）");

    QVBoxLayout *root = new QVBoxLayout(this);
    QComboBox *preset = new QComboBox(this);
    preset->addItems({QString("自定义"),
                      QString("不改变画面"),
                      QString("高斯模糊"),
                      QString("让边缘更清晰"),
                      QString("浮雕"),
                      QString("描出轮廓"),
                      QString("找边缘")});
    root->addWidget(preset);

    QGridLayout *grid = new QGridLayout();
    for (int i = 0; i < 9; ++i) {
        QDoubleSpinBox *spin = new QDoubleSpinBox(this);
        spin->setRange(-20.0, 20.0);
        spin->setDecimals(2);
        spin->setSingleStep(0.1);
        spin->setValue(i == 4 ? 1.0 : 0.0);
        spin->setAlignment(Qt::AlignCenter);
        m_cells[i] = spin;
        grid->addWidget(MakeSliderCell(spin, this), i / 3, i % 3);
    }
    root->addLayout(grid);

    m_scaleSpin = new QDoubleSpinBox(this);
    m_scaleSpin->setRange(0.01, 100.0);
    m_scaleSpin->setDecimals(2);
    m_scaleSpin->setValue(1.0);
    root->addWidget(MakeSliderRow(QString("亮度缩放"), m_scaleSpin, this));

    m_offsetSpin = new QDoubleSpinBox(this);
    m_offsetSpin->setRange(-255.0, 255.0);
    m_offsetSpin->setDecimals(1);
    m_offsetSpin->setValue(0.0);
    root->addWidget(MakeSliderRow(QString("整体变亮/变暗"), m_offsetSpin, this));

    QLabel *tip = new QLabel(QString("每个像素 = 周围 9 个像素按权重相加 ÷ 亮度缩放 + 整体变亮/变暗，越界按边缘像素处理。"), this);
    tip->setWordWrap(true);
    root->addWidget(tip);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QString("应用"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    connect(preset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) { applyPreset(index); });

    // 任一参数变化就发信号，主窗口借此做实时预览
    for (int i = 0; i < 9; ++i) {
        connect(m_cells[i], QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() { emit paramsChanged(); });
    }
    connect(m_scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { emit paramsChanged(); });
    connect(m_offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { emit paramsChanged(); });
}

void KernelDialog::applyPreset(int index)
{
    if (index <= 0) {
        return;   // 自定义：保留当前输入
    }
    double k[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    double scale = 1.0;
    double offset = 0.0;
    switch (index) {
    case 1:
        break;
    case 2: {
        const double g[9] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
        std::copy(g, g + 9, k);
        scale = 16.0;
        break;
    }
    case 3: {
        const double s[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
        std::copy(s, s + 9, k);
        break;
    }
    case 4: {
        const double e[9] = {-2, -1, 0, -1, 1, 1, 0, 1, 2};
        std::copy(e, e + 9, k);
        offset = 128.0;
        break;
    }
    case 5: {
        const double o[9] = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
        std::copy(o, o + 9, k);
        break;
    }
    case 6: {
        const double l[9] = {0, 1, 0, 1, -4, 1, 0, 1, 0};
        std::copy(l, l + 9, k);
        offset = 128.0;
        break;
    }
    default:
        break;
    }
    for (int i = 0; i < 9; ++i) {
        m_cells[i]->setValue(k[i]);
    }
    m_scaleSpin->setValue(scale);
    m_offsetSpin->setValue(offset);
}

std::vector<double> KernelDialog::kernel() const
{
    std::vector<double> k(9, 0.0);
    for (int i = 0; i < 9; ++i) {
        k[i] = m_cells[i]->value();
    }
    return k;
}

double KernelDialog::scale() const
{
    return m_scaleSpin->value();
}

double KernelDialog::offset() const
{
    return m_offsetSpin->value();
}

// ------------------------------------------------------------------
// USMDialog
// ------------------------------------------------------------------
USMDialog::USMDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("智能锐化");

    QVBoxLayout *root = new QVBoxLayout(this);

    m_radius = new QDoubleSpinBox(this);
    m_radius->setRange(0.1, 20.0);
    m_radius->setDecimals(1);
    m_radius->setSingleStep(0.1);
    m_radius->setValue(1.2);
    root->addWidget(MakeSliderRow(QString("锐化半径"), m_radius, this));

    m_amount = new QSpinBox(this);
    m_amount->setRange(0, 300);
    m_amount->setSuffix(QString(" %"));
    m_amount->setValue(100);
    root->addWidget(MakeSliderRow(QString("锐化程度"), m_amount, this));

    m_threshold = new QSpinBox(this);
    m_threshold->setRange(0, 255);
    m_threshold->setValue(6);
    root->addWidget(MakeSliderRow(QString("差异阈值（越大越只处理强边缘）"), m_threshold, this));

    // 参数一变就发信号，主窗口借此做实时预览
    connect(m_radius, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { emit paramsChanged(); });
    connect(m_amount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { emit paramsChanged(); });
    connect(m_threshold, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { emit paramsChanged(); });
    QLabel *tip = new QLabel(QString("细部反差低于差异阈值时不参与锐化，可避免把噪点一起放大。"), this);
    tip->setWordWrap(true);
    root->addWidget(tip);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QString("应用"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

double USMDialog::radius() const
{
    return m_radius->value();
}

double USMDialog::amount() const
{
    return static_cast<double>(m_amount->value()) / 100.0;
}

double USMDialog::threshold() const
{
    return static_cast<double>(m_threshold->value());
}

// ------------------------------------------------------------------
// ScaleDialog
// ------------------------------------------------------------------
ScaleDialog::ScaleDialog(int currentWidth, int currentHeight, QWidget *parent)
    : QDialog(parent),
      m_initialWidth(std::max(1, currentWidth)),
      m_initialHeight(std::max(1, currentHeight))
{
    setWindowTitle("缩放图像");

    QVBoxLayout *root = new QVBoxLayout(this);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 8192);
    m_widthSpin->setSuffix(QString(" px"));
    m_widthSpin->setValue(m_initialWidth);
    root->addWidget(MakeSliderRow(QString("宽度"), m_widthSpin, this));

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 8192);
    m_heightSpin->setSuffix(QString(" px"));
    m_heightSpin->setValue(m_initialHeight);
    root->addWidget(MakeSliderRow(QString("高度"), m_heightSpin, this));

    m_lockRatio = new QCheckBox(QString("锁定比例"), this);
    m_lockRatio->setChecked(true);
    root->addWidget(m_lockRatio);

    QHBoxLayout *algoRow = new QHBoxLayout();
    algoRow->addWidget(new QLabel(QString("算法"), this));
    m_algorithm = new QComboBox(this);
    m_algorithm->addItems({QString("快速（像素画/图标）"),
                           QString("平滑（照片）"),
                           QString("最佳（照片放大）")});
    algoRow->addWidget(m_algorithm, 1);
    root->addLayout(algoRow);

    // 锁定比例时同步另一边的尺寸；blockSignals 避免互相触发
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                if (!m_lockRatio->isChecked()) {
                    return;
                }
                QSignalBlocker blocker(m_heightSpin);
                const int h = qRound(static_cast<double>(value) *
                                     static_cast<double>(m_initialHeight) /
                                     static_cast<double>(m_initialWidth));
                m_heightSpin->setValue(std::min(8192, std::max(1, h)));
            });
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                if (!m_lockRatio->isChecked()) {
                    return;
                }
                QSignalBlocker blocker(m_widthSpin);
                const int w = qRound(static_cast<double>(value) *
                                     static_cast<double>(m_initialWidth) /
                                     static_cast<double>(m_initialHeight));
                m_widthSpin->setValue(std::min(8192, std::max(1, w)));
            });

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QString("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

QSize ScaleDialog::targetSize() const
{
    return QSize(m_widthSpin->value(), m_heightSpin->value());
}

int ScaleDialog::algorithm() const
{
    return m_algorithm->currentIndex();
}

// ------------------------------------------------------------------
// TextDialog
// ------------------------------------------------------------------
TextDialog::TextDialog(const QFont &initialFont, const QColor &initialColor, QWidget *parent,
                       bool editMode)
    : QDialog(parent),
      m_color(initialColor.isValid() ? initialColor : QColor(0, 0, 0))
{
    setWindowTitle(editMode ? QString("编辑文字图层") : QString("文字图层"));
    setMinimumWidth(440);

    QVBoxLayout *root = new QVBoxLayout(this);

    // 文本内容
    m_text = new QPlainTextEdit(this);
    m_text->setPlaceholderText(QString("输入文字，支持多行"));
    m_text->setMinimumHeight(72);
    root->addWidget(new QLabel(QString("内容"), this));
    root->addWidget(m_text);

    // 字体 + 字号
    QHBoxLayout *fontRow = new QHBoxLayout();
    m_font = new QFontComboBox(this);
    m_font->setCurrentFont(initialFont);
    m_size = new QSpinBox(this);
    m_size->setRange(6, 300);
    m_size->setSuffix(QString(" pt"));
    m_size->setValue(std::max(6, initialFont.pointSize()));
    m_size->setMaximumWidth(100);
    fontRow->addWidget(new QLabel(QString("字体"), this));
    fontRow->addWidget(m_font, 1);
    fontRow->addWidget(m_size);
    root->addLayout(fontRow);

    // 粗体 / 斜体 / 颜色 / 对齐
    QHBoxLayout *styleRow = new QHBoxLayout();
    m_bold = new QCheckBox(QString("粗体"), this);
    m_bold->setChecked(initialFont.bold());
    m_italic = new QCheckBox(QString("斜体"), this);
    m_italic->setChecked(initialFont.italic());
    m_colorBtn = new QPushButton(QString("颜色…"), this);
    m_align = new QComboBox(this);
    m_align->addItems({QString("左对齐"), QString("居中"), QString("右对齐")});
    styleRow->addWidget(m_bold);
    styleRow->addWidget(m_italic);
    styleRow->addWidget(m_colorBtn);
    styleRow->addWidget(new QLabel(QString("对齐"), this));
    styleRow->addWidget(m_align, 1);
    root->addLayout(styleRow);

    // 预览
    m_preview = new QLabel(this);
    m_preview->setFixedHeight(80);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral(
        "background:#ffffff;color:#111111;border:1px solid #3b4455;border-radius:4px;"));
    root->addWidget(m_preview);

    // 按钮
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(editMode ? QString("确定") : QString("添加"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    connect(m_text, &QPlainTextEdit::textChanged, this, [this]() { updatePreview(); });
    connect(m_font, &QFontComboBox::currentFontChanged, this, [this]() { updatePreview(); });
    connect(m_size, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { updatePreview(); });
    connect(m_bold, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    connect(m_italic, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    connect(m_align, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { updatePreview(); });
    connect(m_colorBtn, &QPushButton::clicked, this, [this]() {
        const QColor color = QColorDialog::getColor(m_color, this, QString("文字颜色"));
        if (color.isValid()) {
            m_color = color;
            updatePreview();
        }
    });

    updatePreview();
}

QString TextDialog::text() const
{
    return m_text->toPlainText();
}

QFont TextDialog::font() const
{
    QFont font = m_font->currentFont();
    font.setPointSize(m_size->value());
    font.setBold(m_bold->isChecked());
    font.setItalic(m_italic->isChecked());
    return font;
}

int TextDialog::alignment() const
{
    return m_align->currentIndex();
}

void TextDialog::setContent(const QString &text, int alignment)
{
    QSignalBlocker blockText(m_text);
    QSignalBlocker blockAlign(m_align);
    m_text->setPlainText(text);
    m_align->setCurrentIndex(qBound(0, alignment, 2));
    updatePreview();
}

void TextDialog::updatePreview()
{
    m_colorBtn->setStyleSheet(QString("background:%1;border:1px solid #3b4455;"
                                      "border-radius:3px;padding:2px 8px;")
                                  .arg(m_color.name()));
    const QString content = m_text->toPlainText();
    QPixmap pix(m_preview->width(), m_preview->height());
    pix.fill(QColor(255, 255, 255));
    if (!content.isEmpty()) {
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        QFont font = m_font->currentFont();
        font.setPointSize(qBound(6, m_size->value(), 28));   // 预览限制字号，避免溢出
        font.setBold(m_bold->isChecked());
        font.setItalic(m_italic->isChecked());
        painter.setFont(font);
        painter.setPen(m_color);
        const QRect box = pix.rect().adjusted(8, 4, -8, -4);
        painter.drawText(box, Qt::AlignCenter, content);
        painter.end();
    }
    m_preview->setPixmap(pix);
}

// ------------------------------------------------------------------
// PngExportDialog
// ------------------------------------------------------------------
PngExportDialog::PngExportDialog(int initialCompression, int initialDpi, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString("PNG 导出参数"));
    setMinimumWidth(360);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(QString("压缩级别（越大文件越小，编码越慢）"), this));

    m_compression = new QSpinBox(this);
    m_compression->setRange(0, 9);
    m_compression->setValue(qBound(0, initialCompression, 9));
    root->addWidget(MakeSliderRow(QString("压缩级别"), m_compression, this));

    m_dpi = new QSpinBox(this);
    m_dpi->setRange(0, 2000);
    m_dpi->setSuffix(QString(" DPI"));
    m_dpi->setSpecialValueText(QString("不写入"));
    m_dpi->setValue(qBound(0, initialDpi, 2000));
    QHBoxLayout *dpiRow = new QHBoxLayout();
    dpiRow->addWidget(new QLabel(QString("分辨率"), this));
    dpiRow->addStretch();
    dpiRow->addWidget(m_dpi);
    root->addLayout(dpiRow);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QString("导出"));
    box->button(QDialogButtonBox::Cancel)->setText(QString("取消"));
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

int PngExportDialog::compression() const
{
    return m_compression->value();
}

int PngExportDialog::dpi() const
{
    return m_dpi->value();
}
