#ifndef SLIDERFIELD_H
#define SLIDERFIELD_H

#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QObject>
#include <QSignalBlocker>
#include <QSlider>
#include <QWidget>
#include <cmath>

// 滑块 + 数值框的双向同步。
// 统一约定：滑块走整数刻度，文本框负责显示/输入；
// scale/decimals 让小数参数也能用滑块(例如 0~1 用 scale=100、decimals=2，
// 滑块刻度 0~100，文本框显示 0.00~1.00)。
namespace SliderField {

inline QSlider *Make(QWidget *parent, int min, int max, int value)
{
    QSlider *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(min, max);
    slider->setValue(value);
    slider->setMinimumHeight(18);
    return slider;
}

// 把控件插到 anchor 后面(父容器是纵/横布局时即"下方/右侧")
inline bool InsertAfter(QWidget *anchor, QWidget *widget)
{
    if (anchor == nullptr || widget == nullptr || anchor->parentWidget() == nullptr) {
        return false;
    }
    QBoxLayout *box = qobject_cast<QBoxLayout *>(anchor->parentWidget()->layout());
    if (box == nullptr) {
        return false;
    }
    const int index = box->indexOf(anchor);
    box->insertWidget(index >= 0 ? index + 1 : box->count(), widget);
    return true;
}

// 数值框微调：↑/↓ 走一个最小刻度(按住 Shift 走 10 个)，双击回到初始值。
// 普通点击/输入不受影响，滑块自身的拖动也不受影响。
class EditTweakFilter : public QObject
{
public:
    explicit EditTweakFilter(QSlider *slider)
        : QObject(slider),
          m_slider(slider),
          m_initial(slider != nullptr ? slider->value() : 0)
    {
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        if (m_slider == nullptr) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            const QKeyEvent *key = static_cast<QKeyEvent *>(event);
            int direction = 0;
            if (key->key() == Qt::Key_Up) {
                direction = 1;
            } else if (key->key() == Qt::Key_Down) {
                direction = -1;
            } else {
                return false;
            }
            const int ticks = (key->modifiers() & Qt::ShiftModifier) ? 10 : 1;
            const int next = qBound(m_slider->minimum(),
                                    m_slider->value() + direction * ticks,
                                    m_slider->maximum());
            m_slider->setValue(next);
            return true;
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            m_slider->setValue(m_initial);   // 双击数值框恢复默认值
            return true;
        }
        return false;
    }

private:
    QSlider *m_slider = nullptr;
    int m_initial = 0;
};

// 滑块与文本框双向同步：拖滑块写文本框，改文本框推滑块
inline void Bind(QSlider *slider, QLineEdit *edit, double scale = 1.0, int decimals = 0)
{
    if (slider == nullptr || edit == nullptr) {
        return;
    }
    QObject::connect(slider, &QSlider::valueChanged, edit,
                     [edit, scale, decimals](int value) {
                         edit->setText(QString::number(value / scale, 'f', decimals));
                     });
    QObject::connect(edit, &QLineEdit::textChanged, slider,
                     [slider, edit, scale](const QString &) {
                         bool ok = false;
                         const double value = edit->text().trimmed().toDouble(&ok);
                         if (!ok) {
                             return;
                         }
                         if (value < slider->minimum() / scale ||
                             value > slider->maximum() / scale) {
                             return;
                         }
                         QSignalBlocker blocker(slider);
                         slider->setValue(static_cast<int>(std::lround(value * scale)));
                     });
    edit->installEventFilter(new EditTweakFilter(slider));
}

}   // namespace SliderField
#endif // SLIDERFIELD_H
