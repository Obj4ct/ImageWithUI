#include "mainwindow.h"
#include "retouch.h"
#include "ui_retouch.h"
#include "slider_field.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace {

inline int ClampInt(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

// 双线性取样。snap 是受影响区域的局部快照，坐标以快照左上角为原点。
void SampleBilinear(const std::vector<uint8_t> &snap, int w, int h,
                    double fx, double fy, uint8_t out[3])
{
    if (w <= 0 || h <= 0) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    fx = std::min(std::max(fx, 0.0), static_cast<double>(w - 1));
    fy = std::min(std::max(fy, 0.0), static_cast<double>(h - 1));
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const double tx = fx - x0;
    const double ty = fy - y0;

    for (int c = 0; c < 3; ++c) {
        const double v00 = snap[(static_cast<size_t>(y0) * w + x0) * 3 + c];
        const double v10 = snap[(static_cast<size_t>(y0) * w + x1) * 3 + c];
        const double v01 = snap[(static_cast<size_t>(y1) * w + x0) * 3 + c];
        const double v11 = snap[(static_cast<size_t>(y1) * w + x1) * 3 + c];
        const double value = (v00 * (1.0 - tx) + v10 * tx) * (1.0 - ty) +
                             (v01 * (1.0 - tx) + v11 * tx) * ty;
        out[c] = static_cast<uint8_t>(std::lround(std::min(255.0, std::max(0.0, value))));
    }
}

}   // namespace

Retouch::Retouch(MainWindow* mainWindow, BitmapImage myValue, QWidget *parent)
    : QWidget(parent), ui(new Ui::Retouch), mainWindow(mainWindow), myValue(myValue)
{
    ui->setupUi(this);
    // 清掉 .ui 里自带的浅色样式，统一交给全局主题(Theme.h)接管
    setStyleSheet(QString());

    setWindowTitle(QString("图片微调"));
    setWindowIcon(QIcon(":/icon/logo.png"));
    // 图片居中显示：控件坐标换算成图像坐标时，用"居中"的偏移量才对得上
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    imageData = mainWindow->imageData;
    m_work = imageData;
    newValue = myValue;

    // 不再需要"先点中心"的交互，隐藏坐标行
    ui->widget->setVisible(false);

    ShowImage(m_work);

    QVBoxLayout *root = qobject_cast<QVBoxLayout *>(ui->widget_5->layout());

    // 效果选择行(参考 Photoshop 液化：收拢/膨胀/平滑/漩涡/推移)
    QWidget *effectRow = new QWidget(ui->widget_5);
    QHBoxLayout *effectLayout = new QHBoxLayout(effectRow);
    effectLayout->setContentsMargins(0, 0, 0, 0);
    effectLayout->addWidget(new QLabel(QString("调整效果"), effectRow));
    m_effectCombo = new QComboBox(effectRow);
    m_effectCombo->addItem(QString("收拢（瘦脸）"));
    m_effectCombo->addItem(QString("膨胀（放大）"));
    m_effectCombo->addItem(QString("平滑（柔化）"));
    m_effectCombo->addItem(QString("漩涡"));
    m_effectCombo->addItem(QString("推移（按住拖动）"));
    effectLayout->addWidget(m_effectCombo, 1);
    if (root != nullptr) {
        root->insertWidget(0, effectRow);
    }

    // 力度行(0~100%，与滑块双向同步)
    QWidget *strengthRow = new QWidget(ui->widget_5);
    QHBoxLayout *strengthLayout = new QHBoxLayout(strengthRow);
    strengthLayout->setContentsMargins(0, 0, 0, 0);
    strengthLayout->addWidget(new QLabel(QString("力度（%）"), strengthRow));
    m_strengthEdit = new QLineEdit(strengthRow);
    m_strengthEdit->setMaximumWidth(64);
    m_strengthEdit->setText(QStringLiteral("40"));
    m_strengthEdit->setObjectName(QStringLiteral("lineEdit_strength"));
    m_strengthSlider = SliderField::Make(strengthRow, 0, 100, 40);
    m_strengthSlider->setObjectName(QStringLiteral("slider_strength"));
    strengthLayout->addWidget(m_strengthEdit);
    strengthLayout->addWidget(m_strengthSlider, 1);
    if (root != nullptr) {
        const int radiusRow = root->indexOf(ui->widget_2);
        root->insertWidget(radiusRow >= 0 ? radiusRow + 1 : root->count(), strengthRow);
    }
    SliderField::Bind(m_strengthSlider, m_strengthEdit);

    // 画笔大小滑块：拖滑块写数值，改数值时滑块跟着走
    m_radiusSlider = SliderField::Make(ui->widget_2, 1, 300, 80);
    m_radiusSlider->setObjectName(QStringLiteral("slider_radius"));
    if (SliderField::InsertAfter(ui->lineEdit_radius, m_radiusSlider)) {
        QSignalBlocker blocker(ui->lineEdit_radius);
        ui->lineEdit_radius->setText(QStringLiteral("80"));
        SliderField::Bind(m_radiusSlider, ui->lineEdit_radius);
    }
}

Retouch::~Retouch()
{
    delete ui;
}

bool Retouch::PointToImage(const QPoint &widgetPos, QPoint *imagePos) const
{
    const QPixmap *pixmap = ui->imageLabel->pixmap();
    if (pixmap == nullptr || pixmap->isNull() || imagePos == nullptr) {
        return false;
    }
    // 事件坐标是控件坐标，需要先转换到 imageLabel 的局部坐标系
    const QPoint labelPos = ui->imageLabel->mapFrom(this, widgetPos);
    // imageLabel 使用 AlignCenter 显示图像，必须扣掉图像相对控件左上角的居中偏移
    const int offsetX = (ui->imageLabel->width() - pixmap->width()) / 2;
    const int offsetY = (ui->imageLabel->height() - pixmap->height()) / 2;
    const int displayX = labelPos.x() - offsetX;
    const int displayY = labelPos.y() - offsetY;
    if (displayX < 0 || displayY < 0 ||
        displayX >= pixmap->width() || displayY >= pixmap->height()) {
        return false;
    }
    const int32_t imageWidth = newValue.bmpInfo.GetWidth();
    const int32_t imageHeight = newValue.bmpInfo.GetHeight();
    if (imageWidth <= 0 || imageHeight <= 0) {
        return false;
    }
    const int imageX = static_cast<int>(
            static_cast<long long>(displayX) * imageWidth / pixmap->width());
    // 图像是自底向上存储的，视觉行 y 对应内存行 (height-1-y)
    const int imageY = imageHeight - 1 - static_cast<int>(
            static_cast<long long>(displayY) * imageHeight / pixmap->height());
    *imagePos = QPoint(ClampInt(imageX, 0, imageWidth - 1),
                       ClampInt(imageY, 0, imageHeight - 1));
    return true;
}

int Retouch::CurrentRadius() const
{
    bool ok = false;
    const int value = ui->lineEdit_radius->text().trimmed().toInt(&ok);
    if (!ok || value <= 0) {
        return 80;
    }
    return std::min(300, value);
}

double Retouch::CurrentStrength() const
{
    bool ok = false;
    const double value = m_strengthEdit->text().trimmed().toDouble(&ok);
    if (!ok) {
        return 0.4;
    }
    return std::min(1.0, std::max(0.0, value / 100.0));
}

void Retouch::ApplyAt(const QPoint &imagePos, int deltaX, int deltaY)
{
    const int width = newValue.bmpInfo.GetWidth();
    const int height = newValue.bmpInfo.GetHeight();
    if (width <= 0 || height <= 0 || m_work.size() != static_cast<size_t>(width) * height * 3) {
        return;
    }
    const int index = m_effectCombo != nullptr ? m_effectCombo->currentIndex() : 0;
    const RetouchEffect effect = static_cast<RetouchEffect>(
            ClampInt(index, 0, static_cast<int>(Push)));
    ApplyEffect(m_work, width, height, effect, imagePos.x(), imagePos.y(),
                CurrentRadius(), CurrentStrength(), deltaX, deltaY);
    ShowImage(m_work);
}

void Retouch::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    QPoint imagePos;
    if (!PointToImage(event->pos(), &imagePos)) {
        return;
    }
    m_stroking = true;
    m_lastPos = imagePos;
    ApplyAt(imagePos, 0, 0);
}

void Retouch::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_stroking) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    QPoint imagePos;
    if (!PointToImage(event->pos(), &imagePos)) {
        return;
    }
    const int dx = imagePos.x() - m_lastPos.x();
    const int dy = imagePos.y() - m_lastPos.y();
    // 移动太小时不重复落笔，避免原地多次涂抹糊成一片
    const int minStep = std::max(2, CurrentRadius() / 8);
    if (std::abs(dx) + std::abs(dy) < minStep) {
        return;
    }
    const int limit = CurrentRadius();
    ApplyAt(imagePos, ClampInt(dx, -limit, limit), ClampInt(dy, -limit, limit));
    m_lastPos = imagePos;
}

void Retouch::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_stroking = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void Retouch::ShowImage(std::vector<uint8_t> &inImageData)
{
    const QImage image = ImageOps::ToDisplayImage(inImageData,
                                                  newValue.bmpInfo.GetWidth(),
                                                  newValue.bmpInfo.GetHeight());
    ui->imageLabel->setPixmap(QPixmap::fromImage(image));
}

void Retouch::on_btn_ok_clicked()
{
    // 重置：丢弃所有涂抹，回到打开窗口时的图像
    m_work = imageData;
    ShowImage(m_work);
}

void Retouch::on_btn_apply_clicked()
{
    if (m_work.empty()) {
        return;
    }
    // 写回主窗口并记录历史(这样才能撤销)
    mainWindow->ApplyResult(m_work);
}

void Retouch::ApplyEffect(std::vector<uint8_t> &work, int width, int height,
                       RetouchEffect effect, int centerX, int centerY,
                       int radius, double strength, int deltaX, int deltaY)
{
    if (width <= 0 || height <= 0 || radius <= 0 ||
        work.size() < static_cast<size_t>(width) * height * 3) {
        return;
    }
    strength = std::min(1.0, std::max(0.0, strength));
    if (strength <= 0.0) {
        return;
    }
    centerX = ClampInt(centerX, 0, width - 1);
    centerY = ClampInt(centerY, 0, height - 1);

    // 只处理画笔覆盖的方形区域，边缘多留 2px 供双线性取样
    const int margin = 2;
    const int x0 = std::max(0, centerX - radius - margin);
    const int x1 = std::min(width - 1, centerX + radius + margin);
    const int y0 = std::max(0, centerY - radius - margin);
    const int y1 = std::min(height - 1, centerY + radius + margin);
    const int rw = x1 - x0 + 1;
    const int rh = y1 - y0 + 1;
    if (rw <= 0 || rh <= 0) {
        return;
    }

    // 先拷一份区域快照：本次涂抹一边读快照一边写结果，避免笔触内部互相取样
    std::vector<uint8_t> snap(static_cast<size_t>(rw) * rh * 3);
    for (int row = 0; row < rh; ++row) {
        std::memcpy(&snap[static_cast<size_t>(row) * rw * 3],
                    &work[(static_cast<size_t>(y0 + row) * width + x0) * 3],
                    static_cast<size_t>(rw) * 3);
    }

    const int smoothStep = std::max(1, radius / 8);

    for (int y = y0; y <= y1; ++y) {
        const double dy = static_cast<double>(y) - centerY;
        for (int x = x0; x <= x1; ++x) {
            const double dx = static_cast<double>(x) - centerX;
            const double distance = std::sqrt(dx * dx + dy * dy);
            if (distance >= radius) {
                continue;   // 画笔外原样保留
            }
            const double t = distance / radius;
            double weight = 1.0 - t * t;
            weight *= weight;   // 中心最强、边缘平滑衰减
            if (weight <= 0.0) {
                continue;
            }

            uint8_t color[3];
            const size_t dstIndex = (static_cast<size_t>(y) * width + x) * 3;

            switch (effect) {
            case Pucker:
            case Bloat: {
                // 收拢：取样点向外偏移(内容被吸向中心)；膨胀反过来
                const double scale = 1.0 + (effect == Pucker ? 0.6 : -0.6) * strength * weight;
                const double sx = centerX + dx * scale - x0;
                const double sy = centerY + dy * scale - y0;
                SampleBilinear(snap, rw, rh, sx, sy, color);
                break;
            }
            case Twirl: {
                const double angle = -strength * weight * 1.6;
                const double cosA = std::cos(angle);
                const double sinA = std::sin(angle);
                const double sx = centerX + dx * cosA - dy * sinA - x0;
                const double sy = centerY + dx * sinA + dy * cosA - y0;
                SampleBilinear(snap, rw, rh, sx, sy, color);
                break;
            }
            case Push: {
                // 取样点反向偏移，内容就跟着光标走
                const double sx = x - deltaX * strength * weight - x0;
                const double sy = y - deltaY * strength * weight - y0;
                SampleBilinear(snap, rw, rh, sx, sy, color);
                break;
            }
            case Smooth:
            default: {
                // 平滑：取周围 5x5 个采样点的均值，再按力度混回去(磨皮/柔化)
                double sum[3] = {0.0, 0.0, 0.0};
                int count = 0;
                for (int oy = -2; oy <= 2; ++oy) {
                    for (int ox = -2; ox <= 2; ++ox) {
                        uint8_t sample[3];
                        SampleBilinear(snap, rw, rh,
                                       x + ox * smoothStep - x0,
                                       y + oy * smoothStep - y0, sample);
                        sum[0] += sample[0];
                        sum[1] += sample[1];
                        sum[2] += sample[2];
                        ++count;
                    }
                }
                const double blend = strength * weight;
                for (int c = 0; c < 3; ++c) {
                    const double base = snap[(static_cast<size_t>(y - y0) * rw + (x - x0)) * 3 + c];
                    color[c] = static_cast<uint8_t>(std::lround(
                            std::min(255.0, std::max(0.0, base * (1.0 - blend) +
                                                           sum[c] / count * blend))));
                }
                break;
            }
            }

            work[dstIndex] = color[0];
            work[dstIndex + 1] = color[1];
            work[dstIndex + 2] = color[2];
        }
    }
}
