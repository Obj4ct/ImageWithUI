#include "editdialogs.h"

#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>

// ------------------------------------------------------------------
// CropDialog
// ------------------------------------------------------------------
CropDialog::CropDialog(const QImage &displayImage, QWidget *parent)
    : QDialog(parent),
      m_image(displayImage)
{
    setWindowTitle("图像裁剪 · 拖拽框选要保留的区域");
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
    setWindowTitle("自定义卷积核 (3×3)");

    QVBoxLayout *root = new QVBoxLayout(this);
    QComboBox *preset = new QComboBox(this);
    preset->addItems({QString("自定义"),
                      QString("恒等(不改变图像)"),
                      QString("高斯模糊"),
                      QString("锐化"),
                      QString("浮雕"),
                      QString("描边"),
                      QString("拉普拉斯(边缘)")});
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
        grid->addWidget(spin, i / 3, i % 3);
    }
    root->addLayout(grid);

    QFormLayout *form = new QFormLayout();
    m_scaleSpin = new QDoubleSpinBox(this);
    m_scaleSpin->setRange(0.01, 100.0);
    m_scaleSpin->setDecimals(2);
    m_scaleSpin->setValue(1.0);
    form->addRow(QString("归一化除数"), m_scaleSpin);

    m_offsetSpin = new QDoubleSpinBox(this);
    m_offsetSpin->setRange(-255.0, 255.0);
    m_offsetSpin->setDecimals(1);
    m_offsetSpin->setValue(0.0);
    form->addRow(QString("偏移量"), m_offsetSpin);
    root->addLayout(form);

    QLabel *tip = new QLabel(QString("卷积结果 = Σ(核系数 × 邻域像素) / 除数 + 偏移，越界按边缘复制。"), this);
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
    setWindowTitle("USM 锐化 (非锐化掩模)");

    QVBoxLayout *root = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    m_radius = new QDoubleSpinBox(this);
    m_radius->setRange(0.1, 20.0);
    m_radius->setDecimals(1);
    m_radius->setSingleStep(0.1);
    m_radius->setValue(1.2);
    form->addRow(QString("半径 (高斯 σ)"), m_radius);

    m_amount = new QSpinBox(this);
    m_amount->setRange(0, 300);
    m_amount->setSuffix(QString(" %"));
    m_amount->setValue(100);
    form->addRow(QString("强度"), m_amount);

    m_threshold = new QSpinBox(this);
    m_threshold->setRange(0, 255);
    m_threshold->setValue(6);
    form->addRow(QString("保护阈值"), m_threshold);

    root->addLayout(form);
    QLabel *tip = new QLabel(QString("细部反差低于阈值时不参与锐化，可避免放大噪点。"), this);
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
