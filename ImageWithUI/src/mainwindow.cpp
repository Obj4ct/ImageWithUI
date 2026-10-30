
// MainWindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "Theme.h"
#include "editdialogs.h"
#include <QAction>
#include <QMenu>
#include <QImageWriter>
#include <QSignalBlocker>
#include <QFont>
#include <QKeyEvent>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QPainter>
#include <QClipboard>
#include <QFontMetrics>

namespace {

// ---- 调整算子工厂：提交路径与实时预览路径共用同一份实现 ----

OpFn MakeBrightnessOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        f.Brightness(d, value);
    };
}

OpFn MakeContrastOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        f.Contrast(d, value);
    };
}

OpFn MakeSaturationOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.Saturation(d, ctx.width, ctx.height, value);
    };
}

OpFn MakeShadowOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.MakeShadow(d, out, value);
        d.swap(out);
    };
}

OpFn MakeHighlightOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        Function f;
        std::vector<uint8_t> out;
        f.HighLight(d, out, value);
        d.swap(out);
    };
}

OpFn MakeThresholdOp(double threshold)
{
    return [threshold](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.ApplyThreshold(d, threshold, 0, ctx.pixelCount);
    };
}

// 锐化：高斯模糊 → 高反差保留 → 锐化，程度值同时作为模糊半径
OpFn MakeSharpenOp(double amount)
{
    return [amount](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> blurPromise;
        std::future<std::vector<uint8_t>> blurFuture = blurPromise.get_future();
        f.Gauss(d, blurPromise, ctx.width, ctx.height, amount);
        const std::vector<uint8_t> blurred = blurFuture.get();

        std::promise<std::vector<uint8_t>> highPromise;
        std::future<std::vector<uint8_t>> highFuture = highPromise.get_future();
        f.HighContrast(d, highPromise, blurred);
        const std::vector<uint8_t> highContrast = highFuture.get();

        std::promise<std::vector<uint8_t>> sharpPromise;
        std::future<std::vector<uint8_t>> sharpFuture = sharpPromise.get_future();
        f.Sharpen(d, sharpPromise, highContrast);
        d = sharpFuture.get();
    };
}

// 大图预览用：按整数步长做最近邻缩小(保持 BGR、逐行紧凑布局)
std::vector<uint8_t> DownsampleBgr(const std::vector<uint8_t> &src, int32_t w, int32_t h, int step,
                                   int32_t &outW, int32_t &outH)
{
    outW = w / step;
    outH = h / step;
    if (outW <= 0 || outH <= 0 || src.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 3) {
        outW = 0;
        outH = 0;
        return {};
    }
    const size_t srcRow = static_cast<size_t>(w) * 3;
    const size_t dstRow = static_cast<size_t>(outW) * 3;
    std::vector<uint8_t> out(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 3, 0);
    for (int32_t y = 0; y < outH; ++y) {
        const uint8_t *s = src.data() + static_cast<size_t>(y) * static_cast<size_t>(step) * srcRow;
        uint8_t *d = out.data() + static_cast<size_t>(y) * dstRow;
        for (int32_t x = 0; x < outW; ++x) {
            const uint8_t *p = s + static_cast<size_t>(x) * static_cast<size_t>(step) * 3;
            d[0] = p[0];
            d[1] = p[1];
            d[2] = p[2];
            d += 3;
        }
    }
    return out;
}

// 预览超过这个像素数就先缩小再算，避免大图拖动参数卡顿
constexpr size_t kPreviewMaxPixels = 2000000;

// 把一个图层按不透明度/混合模式叠到合成缓冲上(dst 初始为白，尺寸与图层一致)
void BlendLayerInto(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src,
                    double opacity, int blend)
{
    const size_t bytes = std::min(dst.size(), src.size());
    const int op = qBound(0, static_cast<int>(opacity * 255.0 + 0.5), 255);
    if (op == 0) {
        return;
    }
    if (op == 255 && blend == 0) {
        dst.assign(src.begin(), src.begin() + static_cast<ptrdiff_t>(bytes));
        return;
    }
    for (size_t i = 0; i < bytes; ++i) {
        const int d = dst[i];
        const int s = src[i];
        int blended = s;
        if (blend == 1) {           // 正片叠底
            blended = d * s / 255;
        } else if (blend == 2) {    // 滤色
            blended = 255 - (255 - d) * (255 - s) / 255;
        }
        dst[i] = static_cast<uint8_t>((blended * op + d * (255 - op)) / 255);
    }
}

// 两份图层栈是否完全一致(用于历史去重)
bool SameLayerStack(const std::vector<Layer> &a, const std::vector<Layer> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || a[i].visible != b[i].visible ||
            a[i].blend != b[i].blend || std::abs(a[i].opacity - b[i].opacity) > 1e-9 ||
            a[i].data != b[i].data) {
            return false;
        }
    }
    return true;
}

}   // namespace

// ------------------------------------------------------------------
// 矩形选区覆盖层
// 画在 imageLabel 之上：负责拖拽建立/移动选区，以及"蚂蚁线"绘制。
// 选区始终以【图像像素】坐标保存(视觉坐标，y 向下)，与显示缩放无关。
// ------------------------------------------------------------------
class SelectionOverlay : public QWidget
{
public:
    explicit SelectionOverlay(QLabel *label)
        : QWidget(label)
    {
        setObjectName("selectionOverlay");
        setAttribute(Qt::WA_NoSystemBackground);
        // 未启用工具时鼠标穿透到标签，双击打开图片等原有交互不受影响
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setMouseTracking(true);

        // 蚂蚁线滚动动画
        m_anim = new QTimer(this);
        m_anim->setInterval(120);
        connect(m_anim, &QTimer::timeout, this, [this]() {
            if (m_sel.isEmpty()) {
                return;
            }
            m_dashOffset = (m_dashOffset + 2) % 8;
            update();
        });
        m_anim->start();
    }

    void setImageSize(int32_t w, int32_t h)
    {
        m_imgW = (w > 0) ? w : 0;
        m_imgH = (h > 0) ? h : 0;
        if (!m_sel.isEmpty()) {
            m_sel = m_sel.intersected(QRect(0, 0, m_imgW, m_imgH));
        }
        update();
    }

    // 程序设置选区(直接调用不会触发 onSelectionChanged)
    void setSelection(const QRect &imageRect)
    {
        QRect r = imageRect.normalized();
        r = (m_imgW > 0 && m_imgH > 0) ? r.intersected(QRect(0, 0, m_imgW, m_imgH)) : QRect();
        if (r != m_sel) {
            m_sel = r;
            update();
        }
    }

    void setToolActive(bool active)
    {
        m_toolActive = active;
        m_drag = None;
        setAttribute(Qt::WA_TransparentForMouseEvents, !active);
        setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
        update();
    }

    // 用户拖拽改变选区后回调
    std::function<void(const QRect &)> onSelectionChanged;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_sel.isEmpty() || m_imgW <= 0 || m_imgH <= 0) {
            return;
        }
        const QRect wr = toWidget(m_sel);
        if (wr.isEmpty()) {
            return;
        }
        QPainter p(this);
        p.fillRect(wr, QColor(34, 211, 238, 40));   // 选区高亮

        // 蚂蚁线：黑白虚线错位叠加，随时间滚动
        const QRectF border = QRectF(wr).adjusted(0.5, 0.5, -0.5, -0.5);
        QPen dark(QColor(0, 0, 0, 210));
        dark.setStyle(Qt::CustomDashLine);
        dark.setDashPattern(QVector<qreal>{4, 4});
        dark.setDashOffset(m_dashOffset);
        p.setPen(dark);
        p.drawRect(border);

        QPen light(QColor(255, 255, 255, 230));
        light.setStyle(Qt::CustomDashLine);
        light.setDashPattern(QVector<qreal>{4, 4});
        light.setDashOffset(m_dashOffset + 4);
        p.setPen(light);
        p.drawRect(border);

        // 四角把手
        p.setPen(QPen(QColor(34, 211, 238), 1));
        p.setBrush(QColor(34, 211, 238));
        const QPoint corners[4] = { wr.topLeft(), wr.topRight(), wr.bottomLeft(), wr.bottomRight() };
        for (const QPoint &c : corners) {
            p.drawRect(QRect(c.x() - 2, c.y() - 2, 5, 5));
        }

        // 尺寸提示(选区上方；贴边时放到内部)
        const QString text = QString("%1 × %2").arg(m_sel.width()).arg(m_sel.height());
        QFont f = font();
        f.setPointSize(9);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int tw = fm.boundingRect(text).width() + 10;
        const int th = fm.height() + 4;
        QRect box(wr.left(), wr.top() - th - 4, tw, th);
        if (box.top() < 0) {
            box.moveTop(wr.top() + 4);
        }
        if (box.right() > width() - 2) {
            box.moveRight(width() - 2);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(9, 13, 18, 220));
        p.drawRoundedRect(box, 4, 4);
        p.setPen(QColor(103, 232, 249));
        p.drawText(box, Qt::AlignCenter, text);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton || !m_toolActive || m_imgW <= 0 || m_imgH <= 0) {
            return;
        }
        const QRect dr = displayRect();
        if (dr.isEmpty() || !dr.adjusted(-2, -2, 2, 2).contains(event->pos())) {
            return;   // 点在图像外的画布空区域：不建立选区
        }
        if (!m_sel.isEmpty() && toWidget(m_sel).contains(event->pos())) {
            m_drag = Moving;   // 在选区内按下 = 整体移动选区
            m_pressPos = event->pos();
            m_startSel = m_sel;
            setCursor(Qt::SizeAllCursor);
        } else {
            m_drag = Creating;
            m_anchor = toImage(event->pos());
            setSelection(QRect(m_anchor, QSize(1, 1)));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!m_toolActive) {
            return;
        }
        const QPoint pos = event->pos();
        if (m_drag == None) {
            setCursor((!m_sel.isEmpty() && toWidget(m_sel).contains(pos)) ? Qt::SizeAllCursor
                                                                          : Qt::CrossCursor);
            return;
        }
        if (m_drag == Creating) {
            setSelection(QRect(m_anchor, toImage(pos)).normalized());
            return;
        }
        const QRect dr = displayRect();
        if (dr.isEmpty()) {
            return;
        }
        const double sx = std::max(1.0, static_cast<double>(dr.width()) / std::max(1, m_imgW));
        const double sy = std::max(1.0, static_cast<double>(dr.height()) / std::max(1, m_imgH));
        const int dx = qRound((pos.x() - m_pressPos.x()) / sx);
        const int dy = qRound((pos.y() - m_pressPos.y()) / sy);
        QRect moved = m_startSel.translated(dx, dy);
        moved.moveTo(qBound(0, moved.x(), m_imgW - moved.width()),
                     qBound(0, moved.y(), m_imgH - moved.height()));
        setSelection(moved);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        Q_UNUSED(event);
        if (m_drag == None) {
            return;
        }
        const bool wasCreating = (m_drag == Creating);
        m_drag = None;
        setCursor(Qt::CrossCursor);
        if (wasCreating && (m_sel.width() < 3 || m_sel.height() < 3)) {
            setSelection(QRect());   // 单击图像视为取消选区
        }
        if (onSelectionChanged) {
            onSelectionChanged(m_sel);
        }
    }

private:
    enum DragMode { None, Creating, Moving };

    // 图像在控件中的显示矩形(标签按 AlignCenter 居中)
    QRect displayRect() const
    {
        QLabel *label = qobject_cast<QLabel *>(parentWidget());
        if (label == nullptr || label->pixmap() == nullptr || label->pixmap()->isNull()) {
            return QRect();
        }
        const QRect area = label->contentsRect();
        const QSize sz = label->pixmap()->size();
        const QPoint topLeft = area.topLeft() + QPoint((area.width() - sz.width()) / 2,
                                                       (area.height() - sz.height()) / 2);
        return QRect(topLeft, sz);
    }

    QPoint toImage(const QPoint &p) const
    {
        const QRect dr = displayRect();
        if (dr.isEmpty()) {
            return QPoint();
        }
        const double sx = static_cast<double>(dr.width()) / std::max(1, m_imgW);
        const double sy = static_cast<double>(dr.height()) / std::max(1, m_imgH);
        return QPoint(qBound(0, static_cast<int>((p.x() - dr.left()) / sx), m_imgW),
                      qBound(0, static_cast<int>((p.y() - dr.top()) / sy), m_imgH));
    }

    QRect toWidget(const QRect &r) const
    {
        const QRect dr = displayRect();
        if (dr.isEmpty() || r.isEmpty()) {
            return QRect();
        }
        const double sx = static_cast<double>(dr.width()) / std::max(1, m_imgW);
        const double sy = static_cast<double>(dr.height()) / std::max(1, m_imgH);
        const QRectF out(dr.left() + r.x() * sx, dr.top() + r.y() * sy,
                         r.width() * sx, r.height() * sy);
        return out.toAlignedRect();
    }

    QTimer *m_anim = nullptr;
    int m_dashOffset = 0;
    bool m_toolActive = false;
    int32_t m_imgW = 0;
    int32_t m_imgH = 0;
    QRect m_sel;                 // 选区(图像像素坐标)
    DragMode m_drag = None;
    QPoint m_pressPos;           // 移动起点(控件坐标)
    QPoint m_anchor;             // 建立选区锚点(图像坐标)
    QRect m_startSel;            // 移动前的选区
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocus();
    setWindowTitle("ImageWithUI · 数字图像处理工作台");
    setWindowIcon(QIcon(":/icon/logo.png"));

    //根据设备cpu线程数判断程序需要设置的线程
    const unsigned int cores = std::thread::hardware_concurrency();
    if (cores >= 2) {
        num_threads = static_cast<int>(cores / 3);   // 留出余量给界面线程
        if (num_threads < 1) num_threads = 1;
    } else {
        num_threads = 1;
    }
    qDebug() << "thread is:" << num_threads;

    _SetShortCut();
    _SetVisible(false);
    SetupDocumentTabs();   // 多文档标签页
    SetupLivePreview();   // 右侧调整参数的实时预览

    // 工具栏：常用操作快捷入口(原先工具栏为空)
    ui->toolBar->setMovable(false);
    ui->toolBar->setIconSize(QSize(20, 20));
    ui->toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->toolBar->addAction(ui->openImage);
    ui->toolBar->addAction(ui->actionsave);
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionundo);
    ui->toolBar->addAction(ui->actionredo);
    ui->toolBar->addAction(ui->actionreset);

    // 视图菜单：主题切换 / 适应窗口
    QMenu *viewMenu = menuBar()->addMenu(QString("视图"));
    QAction *themeAction = viewMenu->addAction(QString("切换深色/浅色主题"));
    themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(themeAction, &QAction::triggered, this, []() {
        Theme::Toggle();
    });
    QAction *fitAction = viewMenu->addAction(QString("图像适应窗口"));
    fitAction->setCheckable(true);
    fitAction->setChecked(m_fitToWindow);
    connect(fitAction, &QAction::toggled, this, [this](bool on) {
        m_fitToWindow = on;
        ShowImage(imageData, myValue, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    });

    // 状态栏：左侧提示 + 右侧信息
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setText(QString("双击此处，或按 Ctrl+N 打开图片"));
    statusBar()->showMessage("就绪 · 请先打开一张图片");
    UpdateStatusInfo();
    SetupSelectionTool();   // 矩形选区覆盖层 + "选择"菜单
    SetupLayerPanel();      // 右侧图层面板 + "图层"菜单
}


void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QMainWindow::mouseDoubleClickEvent(event);
        return;
    }
    // 事件坐标是窗口坐标，需要转换到 imageLabel 的局部坐标系再判断命中
    const QPoint pos = ui->imageLabel->mapFrom(this, event->pos());
    if (!ui->imageLabel->rect().contains(pos)) {
        return;
    }

    // 文件默认路径
    const QString defaultPath = QDir::homePath();
    // 常见图片格式（BMP/PNG/JPG/GIF/TIFF/WebP/TGA/ICO/SVG）
    const QString filter = QString("图片文件 (*.bmp *.png *.jpg *.jpeg *.gif *.tif *.tiff *.webp *.tga *.ico *.svg);;"
                                   "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;GIF (*.gif);;"
                                   "TIFF (*.tif *.tiff);;WebP (*.webp);;TGA (*.tga);;图标 (*.ico *.icns);;SVG (*.svg)");
    const QString path = QFileDialog::getOpenFileName(this, "选择图片文件", defaultPath, filter);
    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    LoadImage(path);
}

// 输入框里按 Esc：放弃当前参数预览
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 标签尺寸变化时，选区覆盖层跟着铺满标签
    if (watched == ui->imageLabel &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        if (m_overlay != nullptr) {
            m_overlay->setGeometry(ui->imageLabel->rect());
            m_overlay->raise();
        }
    }
    if (event->type() == QEvent::KeyPress) {
        const QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape && PreviewFieldIndex(qobject_cast<QLineEdit *>(watched)) >= 0) {
            CancelPreview();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::ResetImage(MyValue &myValue)
{
    imageData = myValue.imageData;
    // 恢复原始图像
    const QImage originalImage = Function::ToDisplayImage(myValue.imageData,
                                                          myValue.bmpInfo.GetWidth(),
                                                          myValue.bmpInfo.GetHeight());
    ui->imageLabel->setText(QString());
    ui->imageLabel->setPixmap(QPixmap::fromImage(originalImage));
}

void MainWindow::ShowImage(const std::vector<uint8_t> &inImageData, MyValue value, int32_t width, int32_t height)
{
    imageData = inImageData;
    myValue = value;

    // 多图层时显示合成结果；单图层时就是 imageData 本身
    const std::vector<uint8_t> &shown = BuildComposite();
    SetDisplayPixmap(shown, width, height);
    SyncSelectionToImage();
    UpdateStatusInfo();
}

// 只更新画面显示，不改动 imageData/myValue —— 预览与取消预览都走这里
void MainWindow::SetDisplayPixmap(const std::vector<uint8_t> &data, int32_t width, int32_t height)
{
    const QImage image = Function::ToDisplayImage(data, width, height);
    if (image.isNull()) {
        return;
    }
    ApplyDisplayPixmap(QPixmap::fromImage(image));
}

// 统一的"贴到 imageLabel"入口：适应窗口时按窗口尺寸缩放
void MainWindow::ApplyDisplayPixmap(QPixmap pixmap)
{
    if (m_fitToWindow) {
        const QSize target = ui->imageLabel->size();
        if (target.isValid() && !target.isEmpty()) {
            pixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    ui->imageLabel->setText(QString());
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setPixmap(pixmap);
}

// 画面回到"已提交"的图像状态
void MainWindow::RefreshDisplay()
{
    if (imageData.empty()) {
        return;
    }
    const std::vector<uint8_t> &shown = BuildComposite();
    SetDisplayPixmap(shown, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    SyncSelectionToImage();
}

// ------------------------------------------------------------------
// 矩形选区
// ------------------------------------------------------------------

void MainWindow::SetupSelectionTool()
{
    m_overlay = new SelectionOverlay(ui->imageLabel);
    m_overlay->setGeometry(ui->imageLabel->rect());
    m_overlay->setVisible(false);
    m_overlay->onSelectionChanged = [this](const QRect &rect) {
        m_selection = rect;
        if (rect.isEmpty()) {
            UpdateStatus("已取消选区");
        } else {
            UpdateStatus(QString("选区 %1 × %2 px").arg(rect.width()).arg(rect.height()));
        }
    };
    ui->imageLabel->installEventFilter(this);

    // 工具栏：矩形选框(勾选后才在图像上拖拽建立选区，避免误触)
    m_selectionToolAction = new QAction(QString("矩形选框"), this);
    m_selectionToolAction->setCheckable(true);
    m_selectionToolAction->setToolTip(QString("选中后在图像上按住左键拖动建立矩形选区；在选区内拖动可整体移动"));
    connect(m_selectionToolAction, &QAction::toggled, this, [this](bool on) {
        if (m_overlay != nullptr) {
            m_overlay->setToolActive(on);
        }
        UpdateStatus(on ? QString("矩形选框：在图像上拖动建立选区") : QString("选区工具已关闭"));
    });
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(m_selectionToolAction);

    // "选择"菜单(放在"编辑"之后、"图像模糊"之前)
    m_selectionMenu = new QMenu(QString("选择"), this);
    m_selectionMenu->addAction(m_selectionToolAction);
    m_selectionMenu->addSeparator();

    auto addItem = [this](const QString &text, const QKeySequence &shortcut,
                          std::function<void()> handler) {
        QAction *action = new QAction(text, this);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        connect(action, &QAction::triggered, this, handler);
        m_selectionMenu->addAction(action);
        return action;
    };

    // 快捷键带 Shift，避免抢走输入框里的 Ctrl+A / Ctrl+C
    QAction *copyAction = addItem(QString("复制选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C),
                                  [this]() { CopySelection(); });
    QAction *cropAction = addItem(QString("裁剪到选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
                                  [this]() { CropToSelection(); });
    QAction *eraseAction = addItem(QString("清除选区内容"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Delete),
                                   [this]() { ClearSelectionContent(); });
    addItem(QString("全选"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), [this]() { SelectAll(); });
    addItem(QString("取消选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), [this]() {
        ClearSelection();
        UpdateStatus("已取消选区");
    });

    // 没有选区时禁用需要选区的操作
    connect(m_selectionMenu, &QMenu::aboutToShow, this, [this, copyAction, cropAction, eraseAction]() {
        const bool has = HasSelection();
        copyAction->setEnabled(has);
        cropAction->setEnabled(has);
        eraseAction->setEnabled(has);
    });

    menuBar()->insertMenu(ui->menu_blur->menuAction(), m_selectionMenu);
    m_selectionMenu->menuAction()->setVisible(!imageData.empty());
}

void MainWindow::UpdateSelectionOverlay()
{
    if (m_overlay == nullptr) {
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    const bool hasImage = !imageData.empty() && w > 0 && h > 0;
    m_overlay->setGeometry(ui->imageLabel->rect());
    m_overlay->setImageSize(hasImage ? w : 0, hasImage ? h : 0);
    m_overlay->setSelection(hasImage ? m_selection : QRect());
    m_overlay->setVisible(hasImage);
    m_overlay->raise();
}

// 图像尺寸变化后把选区夹回图像范围(裁剪/旋转/撤销等都会走这里)
void MainWindow::SyncSelectionToImage()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        m_selection = QRect();
    } else if (!m_selection.isEmpty()) {
        m_selection = m_selection.intersected(QRect(0, 0, w, h));
    }
    UpdateSelectionOverlay();
}

void MainWindow::SetSelection(const QRect &imageRect)
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        m_selection = QRect();
    } else {
        m_selection = imageRect.normalized().intersected(QRect(0, 0, w, h));
    }
    UpdateSelectionOverlay();
}

void MainWindow::SelectAll()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    SetSelection(QRect(0, 0, w, h));
    UpdateStatus(QString("已全选 %1 × %2 px").arg(w).arg(h));
}

void MainWindow::ClearSelection()
{
    m_selection = QRect();
    UpdateSelectionOverlay();
}

void MainWindow::CopySelection()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_selection.isEmpty()) {
        UpdateStatus("请先建立矩形选区");
        return;
    }
    const QImage full = Function::ToDisplayImage(imageData, w, h);
    const QImage sub = full.copy(m_selection.intersected(full.rect()));
    if (sub.isNull() || sub.width() <= 0 || sub.height() <= 0) {
        UpdateStatus("复制选区失败");
        return;
    }
    QApplication::clipboard()->setImage(sub);
    UpdateStatus(QString("已复制选区 %1 × %2 px 到剪贴板").arg(sub.width()).arg(sub.height()));
}

void MainWindow::CropToSelection()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    if (m_selection.isEmpty()) {
        UpdateStatus("请先建立矩形选区");
        return;
    }
    const QRect sel = m_selection;
    if (sel.width() < 4 || sel.height() < 4) {
        UpdateStatus("选区太小(至少 4 × 4)，已取消裁剪");
        return;
    }
    if (!RequireSingleLayer("裁剪到选区")) {
        return;
    }
    ApplyOperator("裁剪到选区", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.CropImage(d, ctx.width, ctx.height,
                    sel.x(), sel.y(), sel.width(), sel.height(), ctx.quad);
    }, nullptr, [this]() { ClearSelection(); });
}

void MainWindow::ClearSelectionContent()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    if (m_selection.isEmpty()) {
        UpdateStatus("请先建立矩形选区");
        return;
    }
    const QRect sel = m_selection;
    ApplyOperator("清除选区内容", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        // 视觉行 y 对应内存行 height-1-y(第 0 行 = 画面最底行)
        const int32_t rows = std::min(sel.height(), ctx.height - sel.y());
        const int32_t cols = std::min(sel.width(), ctx.width - sel.x());
        for (int32_t i = 0; i < rows; ++i) {
            const int32_t memRow = ctx.height - 1 - (sel.y() + i);
            if (memRow < 0 || memRow >= ctx.height) {
                continue;
            }
            uint8_t *p = d.data() + (static_cast<size_t>(memRow) * static_cast<size_t>(ctx.width)
                                     + static_cast<size_t>(sel.x())) * 3;
            std::fill(p, p + static_cast<size_t>(cols) * 3, static_cast<uint8_t>(255));
        }
    }, nullptr, [this]() { UpdateStatus("已清除选区内容"); });
}

void MainWindow::SetupLayerPanel()
{
    // 把占位标签换成真正的图层面板
    QWidget *panel = new QWidget(ui->group_layers);
    panel->setObjectName("layerPanel");
    QVBoxLayout *box = new QVBoxLayout(panel);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(8);

    // 混合模式
    QWidget *blendRow = new QWidget(panel);
    QHBoxLayout *blendLay = new QHBoxLayout(blendRow);
    blendLay->setContentsMargins(0, 0, 0, 0);
    blendLay->setSpacing(8);
    QLabel *blendLabel = new QLabel(QString("混合模式"), blendRow);
    blendLabel->setObjectName("layerBlendLabel");
    m_layerBlendCombo = new QComboBox(blendRow);
    m_layerBlendCombo->addItems(QStringList() << QString("正常") << QString("正片叠底")
                                             << QString("滤色"));
    blendLay->addWidget(blendLabel);
    blendLay->addWidget(m_layerBlendCombo, 1);
    box->addWidget(blendRow);

    // 不透明度
    QWidget *opacityRow = new QWidget(panel);
    QHBoxLayout *opacityLay = new QHBoxLayout(opacityRow);
    opacityLay->setContentsMargins(0, 0, 0, 0);
    opacityLay->setSpacing(8);
    QLabel *opacityLabel = new QLabel(QString("不透明度"), opacityRow);
    opacityLabel->setObjectName("layerOpacityLabel");
    m_layerOpacitySlider = new QSlider(Qt::Horizontal, opacityRow);
    m_layerOpacitySlider->setRange(0, 100);
    m_layerOpacitySlider->setValue(100);
    m_layerOpacityLabel = new QLabel(QString("100%"), opacityRow);
    m_layerOpacityLabel->setObjectName("layerInfoLabel");
    m_layerOpacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opacityLay->addWidget(opacityLabel);
    opacityLay->addWidget(m_layerOpacitySlider, 1);
    opacityLay->addWidget(m_layerOpacityLabel);
    box->addWidget(opacityRow);

    // 图层列表：最上层显示在最上面
    m_layerList = new QListWidget(panel);
    m_layerList->setObjectName("layerList");
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setMinimumHeight(140);
    m_layerList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->addWidget(m_layerList, 1);

    auto addButton = [this, panel](QHBoxLayout *lay, const QString &text,
                                   std::function<void()> handler) {
        QPushButton *btn = new QPushButton(text, panel);
        btn->setObjectName("layerBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(btn, &QPushButton::clicked, this, handler);
        lay->addWidget(btn);
    };
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(6);
    addButton(row1, QString("新建"), [this]() { AddLayer(); });
    addButton(row1, QString("复制"), [this]() { DuplicateLayer(); });
    addButton(row1, QString("删除"), [this]() { DeleteActiveLayer(); });
    box->addLayout(row1);

    QHBoxLayout *row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(6);
    addButton(row2, QString("上移"), [this]() { MoveActiveLayerUp(); });
    addButton(row2, QString("下移"), [this]() { MoveActiveLayerDown(); });
    addButton(row2, QString("合并"), [this]() { MergeAllLayers(); });
    box->addLayout(row2);

    QLayout *groupLayout = ui->group_layers->layout();
    if (groupLayout != nullptr) {
        delete groupLayout->replaceWidget(ui->label_layers_hint, panel);
    }
    ui->label_layers_hint->hide();

    // 列表点击切换活动图层(行序与图层栈相反)
    connect(m_layerList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || m_layerList == nullptr) {
            return;
        }
        SetActiveLayer(LayerCount() - 1 - row);
    });
    // 勾选框 = 图层可见性
    connect(m_layerList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (item == nullptr || m_layerList == nullptr) {
            return;
        }
        const int row = m_layerList->row(item);
        if (row < 0) {
            return;
        }
        SetLayerVisible(LayerCount() - 1 - row, item->checkState() == Qt::Checked);
    });
    connect(m_layerOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        m_layerOpacityLabel->setText(QString("%1%").arg(value));
        SetLayerOpacity(m_activeLayer, value / 100.0);
    });
    connect(m_layerBlendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { SetLayerBlend(m_activeLayer, index); });

    // "图层"菜单(放在"选择"之前)
    m_layerMenu = new QMenu(QString("图层"), this);
    auto addAction = [this](const QString &text, const QKeySequence &shortcut,
                            std::function<void()> handler) {
        QAction *action = new QAction(text, this);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        connect(action, &QAction::triggered, this, handler);
        m_layerMenu->addAction(action);
    };
    addAction(QString("新建图层"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N),
              [this]() { AddLayer(); });
    addAction(QString("复制图层"), QKeySequence(Qt::CTRL | Qt::Key_J),
              [this]() { DuplicateLayer(); });
    addAction(QString("删除图层"), QKeySequence(Qt::CTRL | Qt::Key_Delete),
              [this]() { DeleteActiveLayer(); });
    m_layerMenu->addSeparator();
    addAction(QString("上移图层"), QKeySequence(Qt::CTRL | Qt::Key_BracketRight),
              [this]() { MoveActiveLayerUp(); });
    addAction(QString("下移图层"), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft),
              [this]() { MoveActiveLayerDown(); });
    m_layerMenu->addSeparator();
    addAction(QString("合并所有图层"), QKeySequence(Qt::CTRL | Qt::Key_E),
              [this]() { MergeAllLayers(); });

    if (m_selectionMenu != nullptr) {
        menuBar()->insertMenu(m_selectionMenu->menuAction(), m_layerMenu);
    } else {
        menuBar()->insertMenu(ui->menu_blur->menuAction(), m_layerMenu);
    }
    m_layerMenu->menuAction()->setVisible(!imageData.empty());

    UpdateLayerPanel();
}

void MainWindow::UpdateLayerPanel()
{
    if (m_layerList == nullptr) {
        return;
    }
    // 重建列表期间必须屏蔽信号，否则会反过来触发切换/可见性
    QSignalBlocker blockList(m_layerList);
    QSignalBlocker blockSlider(m_layerOpacitySlider);
    QSignalBlocker blockCombo(m_layerBlendCombo);

    m_layerList->clear();
    const int n = LayerCount();
    for (int index = n - 1; index >= 0; --index) {   // 顶层排在最上面
        const Layer &layer = m_layers[static_cast<size_t>(index)];
        QListWidgetItem *item = new QListWidgetItem(layer.name, m_layerList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
    }

    if (n <= 0) {
        m_layerOpacitySlider->setValue(100);
        m_layerOpacityLabel->setText(QString("100%"));
        m_layerBlendCombo->setCurrentIndex(0);
        return;
    }

    const int active = qBound(0, m_activeLayer, n - 1);
    const Layer &layer = m_layers[static_cast<size_t>(active)];
    m_layerList->setCurrentRow(n - 1 - active);
    m_layerOpacitySlider->setValue(qBound(0, static_cast<int>(layer.opacity * 100.0 + 0.5), 100));
    m_layerOpacityLabel->setText(QString("%1%").arg(m_layerOpacitySlider->value()));
    m_layerBlendCombo->setCurrentIndex(qBound(0, layer.blend, 2));
}

// ------------------------------------------------------------------
// 图层
// 约定：imageData 永远是【当前活动图层】的像素；画面与保存都走合成结果
// ------------------------------------------------------------------

const std::vector<uint8_t> &MainWindow::BuildComposite()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (m_layers.size() <= 1 || w <= 0 || h <= 0 || imageData.empty()) {
        m_composite.clear();
        return imageData;
    }
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
    m_composite.assign(bytes, 255);   // 没有透明度通道：空白按白底处理
    for (int i = 0; i < static_cast<int>(m_layers.size()); ++i) {
        const Layer &layer = m_layers[static_cast<size_t>(i)];
        if (!layer.visible) {
            continue;
        }
        // 活动图层以 imageData 为准(其余图层用各自保存的像素)
        const std::vector<uint8_t> &src = (i == m_activeLayer) ? imageData : layer.data;
        if (src.size() != bytes) {
            continue;
        }
        BlendLayerInto(m_composite, src, layer.opacity, layer.blend);
    }
    return m_composite;
}

const std::vector<uint8_t> &MainWindow::DisplayPixels()
{
    if (m_layers.size() > 1) {
        return BuildComposite();
    }
    return imageData;
}

ImageSnapshot MainWindow::MakeSnapshot(std::vector<uint8_t> pixels, int32_t width, int32_t height,
                                       const ContentQuad &quad) const
{
    ImageSnapshot snap;
    snap.width = width;
    snap.height = height;
    snap.quad = quad;
    snap.layers = m_layers;
    snap.activeLayer = m_activeLayer;
    if (!snap.layers.empty()) {
        const int idx = qBound(0, snap.activeLayer, static_cast<int>(snap.layers.size()) - 1);
        snap.layers[static_cast<size_t>(idx)].data.clear();   // 像素统一存在 snap.data 里
        snap.activeLayer = idx;
    }
    snap.data = std::move(pixels);
    return snap;
}

void MainWindow::RestoreSnapshot(const ImageSnapshot &snap)
{
    imageData = snap.data;
    if (snap.layers.empty()) {
        // 老快照(单图层)：直接用像素建一个图层
        m_layers.clear();
        m_layers.push_back(Layer{QString("图层 1"), snap.data});
        m_activeLayer = 0;
        return;
    }
    m_layers = snap.layers;
    m_activeLayer = qBound(0, snap.activeLayer, static_cast<int>(m_layers.size()) - 1);
    Layer &active = m_layers[static_cast<size_t>(m_activeLayer)];
    if (active.data.empty()) {
        active.data = snap.data;   // 快照里活动图层的像素单独存在 data
    }
}

// 重新合成并刷新画面(不改历史)，用于可见性/不透明度/混合模式这类显示属性变更
void MainWindow::RepaintComposite()
{
    if (imageData.empty()) {
        return;
    }
    const std::vector<uint8_t> &shown = BuildComposite();
    SetDisplayPixmap(shown, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
}

bool MainWindow::RequireSingleLayer(const QString &opName)
{
    if (m_layers.size() <= 1) {
        return true;
    }
    UpdateStatus(opName + "：多图层下需要先合并图层");
    function.CreateMessagebox("提示",
                              opName + " 会改变画布几何，多图层下暂不支持。\n"
                              "请先执行「图层 → 合并所有图层」后再试。");
    return false;
}

// 图层结构变化(新建/删除/复制/排序/合并)记一步历史
void MainWindow::CommitLayerStructure(const QString &title)
{
    if (imageData.empty()) {
        return;
    }
    if (m_activeLayer >= 0 && m_activeLayer < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    }
    ImageSnapshot snap = MakeSnapshot(imageData, myValue.bmpInfo.GetWidth(),
                                      myValue.bmpInfo.GetHeight(), m_contentQuad);
    ApplySnapshot(snap);
    UpdateStatus(title + " 完成");
}

bool MainWindow::SetActiveLayer(int index)
{
    if (index < 0 || index >= static_cast<int>(m_layers.size()) || index == m_activeLayer) {
        return false;
    }
    // 写回当前活动图层，再切到目标图层
    if (m_activeLayer >= 0 && m_activeLayer < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    }
    m_activeLayer = index;
    imageData = m_layers[static_cast<size_t>(index)].data;
    UpdateLayerPanel();
    UpdateStatus(QString("当前图层：%1").arg(m_layers[static_cast<size_t>(index)].name));
    return true;
}

void MainWindow::AddLayer()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    CancelPreview();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    const size_t bytes = static_cast<size_t>(myValue.bmpInfo.GetWidth()) *
                         static_cast<size_t>(myValue.bmpInfo.GetHeight()) * 3;
    Layer layer;
    layer.name = QString("图层 %1").arg(static_cast<int>(m_layers.size()) + 1);
    layer.data.assign(bytes, 255);   // 新建图层为纯白
    const int insertAt = m_activeLayer + 1;
    m_layers.insert(m_layers.begin() + insertAt, std::move(layer));
    m_activeLayer = insertAt;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("新建图层");
}

void MainWindow::DuplicateLayer()
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    CancelPreview();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    Layer copy = m_layers[static_cast<size_t>(m_activeLayer)];   // 连属性一起复制
    copy.name = copy.name + " 副本";
    const int insertAt = m_activeLayer + 1;
    m_layers.insert(m_layers.begin() + insertAt, std::move(copy));
    m_activeLayer = insertAt;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("复制图层");
}

void MainWindow::DeleteActiveLayer()
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_layers.size() <= 1) {
        UpdateStatus("至少需要保留一个图层");
        return;
    }
    CancelPreview();
    m_layers.erase(m_layers.begin() + m_activeLayer);
    m_activeLayer = qBound(0, m_activeLayer, static_cast<int>(m_layers.size()) - 1);
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("删除图层");
}

void MainWindow::MergeAllLayers()
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_layers.size() <= 1) {
        UpdateStatus("当前已经是单图层");
        return;
    }
    CancelPreview();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    // 合并成"看到的画面"(隐藏图层不参与)
    const std::vector<uint8_t> flat = BuildComposite();
    m_layers.clear();
    m_layers.push_back(Layer{QString("合并图层"), flat});
    m_activeLayer = 0;
    imageData = flat;
    UpdateLayerPanel();
    CommitLayerStructure("合并所有图层");
}

void MainWindow::MoveActiveLayerUp()
{
    if (m_busy || imageData.empty() || m_layers.size() <= 1) {
        return;
    }
    if (m_activeLayer >= static_cast<int>(m_layers.size()) - 1) {
        UpdateStatus("已经在最上层");
        return;
    }
    CancelPreview();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    std::swap(m_layers[static_cast<size_t>(m_activeLayer)],
              m_layers[static_cast<size_t>(m_activeLayer + 1)]);
    ++m_activeLayer;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("上移图层");
}

void MainWindow::MoveActiveLayerDown()
{
    if (m_busy || imageData.empty() || m_layers.size() <= 1) {
        return;
    }
    if (m_activeLayer <= 0) {
        UpdateStatus("已经在最下层");
        return;
    }
    CancelPreview();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    std::swap(m_layers[static_cast<size_t>(m_activeLayer)],
              m_layers[static_cast<size_t>(m_activeLayer - 1)]);
    --m_activeLayer;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("下移图层");
}

void MainWindow::SetLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) {
        return;
    }
    Layer &layer = m_layers[static_cast<size_t>(index)];
    if (layer.visible == visible) {
        return;
    }
    layer.visible = visible;
    RepaintComposite();
    UpdateStatus(QString("%1 %2").arg(layer.name, visible ? "显示" : "已隐藏"));
}

void MainWindow::SetLayerOpacity(int index, double opacity)
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) {
        return;
    }
    const double value = qBound(0.0, opacity, 1.0);
    Layer &layer = m_layers[static_cast<size_t>(index)];
    if (std::abs(layer.opacity - value) < 1e-9) {
        return;
    }
    layer.opacity = value;
    RepaintComposite();
}

void MainWindow::SetLayerBlend(int index, int blend)
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) {
        return;
    }
    const int value = qBound(0, blend, 2);
    Layer &layer = m_layers[static_cast<size_t>(index)];
    if (layer.blend == value) {
        return;
    }
    layer.blend = value;
    RepaintComposite();
}

std::vector<uint8_t> MainWindow::CompositePixels()
{
    if (m_layers.size() <= 1) {
        return imageData;
    }
    return BuildComposite();
}

// ------------------------------------------------------------------
// 参数实时预览
// ------------------------------------------------------------------

int MainWindow::PreviewFieldIndex(const QLineEdit *edit) const
{
    for (size_t i = 0; i < m_previewFields.size(); ++i) {
        if (m_previewFields[i].edit == edit) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MainWindow::SetupLivePreview()
{
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(280);   // 停止输入 280ms 后才开始计算
    connect(m_previewTimer, &QTimer::timeout, this, &MainWindow::RunPreview);

    struct Desk {
        QLineEdit *edit;
        const char *title;
        double min;
        double max;
        OpFn (*makeOp)(double);
    };
    const Desk desks[] = {
        { ui->lineEdit_sharpen,    "锐化",      0.0,   200.0, &MakeSharpenOp },
        { ui->lineEdit_highlight,  "高光调整",  0.0,   255.0, &MakeHighlightOp },
        { ui->lineEdit_threshold,  "阈值处理",  0.0,   255.0, &MakeThresholdOp },
        { ui->lineEdit_shadow,     "阴影调整",  0.0,   100.0, &MakeShadowOp },
        { ui->lineEdit_brightness, "亮度调整", -150.0, 150.0, &MakeBrightnessOp },
        { ui->lineEdit_contrast,   "对比度调整", -50.0, 100.0, &MakeContrastOp },
        { ui->lineEdit_saturation, "饱和度调整", -100.0, 100.0, &MakeSaturationOp },
    };

    for (const Desk &d : desks) {
        PreviewField field;
        field.edit = d.edit;
        field.title = QString::fromUtf8(d.title);
        field.min = d.min;
        field.max = d.max;
        field.makeOp = d.makeOp;
        m_previewFields.push_back(field);

        const int index = static_cast<int>(m_previewFields.size()) - 1;
        connect(d.edit, &QLineEdit::textChanged, this, [this, index]() {
            OnPreviewTextChanged(index);
        });
        d.edit->installEventFilter(this);   // Esc 取消预览
    }

    // 焦点离开右侧调整面板(或窗口失去焦点)时回滚到已提交的图像
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!m_previewActive) {
            return;
        }
        if (now != nullptr && (now == ui->widget_14 || ui->widget_14->isAncestorOf(now))) {
            return;
        }
        CancelPreview();
    });
}

void MainWindow::OnPreviewTextChanged(int fieldIndex)
{
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(m_previewFields.size())) {
        return;
    }
    if (imageData.empty() || m_busy) {
        return;
    }
    // 多图层时打开实时预览会与"只改活动图层"的提交结果不一致，直接跳过预览
    if (m_layers.size() > 1) {
        return;
    }
    m_pendingFieldIndex = fieldIndex;
    m_previewTimer->start();   // 防抖：连续输入时不断重新计时
}

// 在【已提交的图像】上跑一次算子，只刷新画面，不写历史
void MainWindow::RunPreview()
{
    if (m_previewRunning) {
        m_previewQueued = true;   // 上一次还没算完，等它结束后再算最新输入
        return;
    }
    if (m_pendingFieldIndex < 0 || m_pendingFieldIndex >= static_cast<int>(m_previewFields.size())) {
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        CancelPreview();
        return;
    }
    if (m_layers.size() > 1) {   // 多图层不做实时预览，提交时直接作用于活动图层
        CancelPreview();
        return;
    }

    const PreviewField &field = m_previewFields[static_cast<size_t>(m_pendingFieldIndex)];
    const ReturnValue rv = CheckOK(field.edit);
    // 输入不完整/非法/越界：静默回到已提交的画面(提交时会给出提示)
    if (rv.isNull || !rv.isNumeric || rv.value < field.min || rv.value > field.max) {
        CancelPreview();
        return;
    }

    const double value = rv.value;
    const OpFn op = field.makeOp(value);
    const int index = m_pendingFieldIndex;

    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();

    // 大图：按整数步长最近邻缩小后在缩略图上算预览，拖动参数不会卡顿；
    // 这种结果只用于显示，点"应用"时会用全分辨率重新计算
    int step = 1;
    while (static_cast<size_t>(width / step) * static_cast<size_t>(height / step) > kPreviewMaxPixels) {
        ++step;
    }
    const bool downsampled = step > 1;
    int32_t workW = width;
    int32_t workH = height;
    std::vector<uint8_t> source = imageData;
    if (downsampled) {
        source = DownsampleBgr(imageData, width, height, step, workW, workH);
        if (source.empty()) {
            CancelPreview();
            return;
        }
    }

    const ContentQuad quad = m_contentQuad;
    const int gen = ++m_previewGeneration;
    m_previewRunning = true;

    auto result = std::make_shared<ImageSnapshot>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
            [this, watcher, result, gen, value, index, downsampled, workW, workH, width, height]() {
        watcher->deleteLater();
        m_previewRunning = false;
        // 世代号已过期 → 这条结果对应的是旧输入，直接丢弃
        const bool usable = gen == m_previewGeneration && !result->data.empty() &&
                            (!downsampled || (result->width == workW && result->height == workH));
        if (usable) {
            m_previewValue = value;
            m_previewFieldIndex = index;
            m_previewActive = true;
            m_previewDownsampled = downsampled;
            if (downsampled) {
                // 低分辨率结果只用来显示，不能提交
                m_previewResult = ImageSnapshot();
                m_previewImage = Function::ToDisplayImage(result->data, result->width, result->height);
                QPixmap pixmap = QPixmap::fromImage(m_previewImage);
                if (!m_fitToWindow) {
                    pixmap = pixmap.scaled(QSize(width, height),
                                           Qt::IgnoreAspectRatio, Qt::FastTransformation);
                }
                ApplyDisplayPixmap(pixmap);
            } else {
                m_previewImage = QImage();
                m_previewResult = std::move(*result);
                SetDisplayPixmap(m_previewResult.data, m_previewResult.width, m_previewResult.height);
            }
            UpdateStatus(QString("预览中：%1 = %2%3")
                             .arg(m_previewFields[static_cast<size_t>(index)].title)
                             .arg(value)
                             .arg(downsampled ? "（低分辨率快速预览）" : ""));
        }
        if (m_previewQueued) {
            m_previewQueued = false;
            RunPreview();
        }
    });

    watcher->setFuture(QtConcurrent::run([source, op, workW, workH, quad, result]() mutable {
        std::vector<uint8_t> work = std::move(source);
        OpContext ctx;
        ctx.width = workW;
        ctx.height = workH;
        ctx.pixelCount = work.size() / 3;
        ctx.numThreads = 1;
        ctx.segmentSize = ctx.pixelCount;
        ctx.quad = quad;
        op(work, ctx);
        result->data = std::move(work);
        result->width = ctx.width;
        result->height = ctx.height;
        result->quad = ctx.quad;
    }));
}

// 放弃预览：停止等待中的任务，并把画面恢复成已提交的图像
void MainWindow::CancelPreview()
{
    ++m_previewGeneration;   // 让在途结果失效
    m_previewQueued = false;
    m_pendingFieldIndex = -1;
    if (m_previewTimer != nullptr) {
        m_previewTimer->stop();
    }
    if (!m_previewActive) {
        return;
    }
    m_previewActive = false;
    m_previewFieldIndex = -1;
    m_previewValue = 0.0;
    m_previewResult = ImageSnapshot();
    m_previewImage = QImage();
    m_previewDownsampled = false;
    RefreshDisplay();
}

// 预览结果与当前输入完全一致时直接提交，省掉一次重复计算
bool MainWindow::CommitPreview(const QString &title, QLineEdit *edit, double value)
{
    const int index = PreviewFieldIndex(edit);
    const bool match = m_previewActive && m_previewFieldIndex == index && m_previewValue == value;
    // 低分辨率预览不能提交(结果会比全分辨率差)，交给调用方重新按全分辨率计算
    const bool canCommit = match && !m_previewDownsampled && !m_previewResult.data.empty();
    const bool wasPreviewing = m_previewActive;
    // 无论是否命中，先清掉预览状态：不一致时由调用方走正常算子流程
    const ImageSnapshot snap = canCommit ? m_previewResult : ImageSnapshot();
    ++m_previewGeneration;
    m_previewQueued = false;
    m_pendingFieldIndex = -1;
    m_previewTimer->stop();
    m_previewActive = false;
    m_previewFieldIndex = -1;
    m_previewValue = 0.0;
    m_previewResult = ImageSnapshot();
    m_previewImage = QImage();
    m_previewDownsampled = false;

    if (!canCommit) {
        if (wasPreviewing) {
            RefreshDisplay();   // 有未提交的预览，先回滚
        }
        return false;
    }
    ApplySnapshot(snap, false);
    UpdateStatus(title + " 已应用");
    return true;
}

void MainWindow::ResetAll(MyValue &myValue)
{
    CancelPreview();
    imageData = myValue.imageData;
    // 重置后回到单图层
    m_layers.clear();
    m_layers.push_back(Layer{QString("图层 1"), imageData});
    m_activeLayer = 0;
    // 恢复原始尺寸(旋转等算子可能改过画布大小)
    if (m_originalWidth > 0 && m_originalHeight > 0) {
        myValue.bmpInfo.SetWidth(m_originalWidth);
        myValue.bmpInfo.SetHeight(m_originalHeight);
        m_contentQuad.setRect(m_originalWidth, m_originalHeight);
    }
    const QImage img = Function::ToDisplayImage(imageData,
                                                myValue.bmpInfo.GetWidth(),
                                                myValue.bmpInfo.GetHeight());
    QPixmap pixmap = QPixmap::fromImage(img);
    if (m_fitToWindow) {
        const QSize target = ui->imageLabel->size();
        if (target.isValid() && !target.isEmpty()) {
            pixmap = pixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    ui->imageLabel->setPixmap(pixmap);
    SyncSelectionToImage();
    imageDataHistory.clear();
    redoImageDataHistory.clear();
    {
        // 重置后把"原图"作为历史第 1 步，历史面板与撤销行为保持一致
        ImageSnapshot snap = MakeSnapshot(imageData,
                                          myValue.bmpInfo.GetWidth(),
                                          myValue.bmpInfo.GetHeight(),
                                          m_contentQuad);
        SaveImageDataToHistory(snap);
    }
    InvalidateRotateBase();
    UpdateStatus("已重置为原始图像");
    UpdateStatusInfo();
    UpdateLayerPanel();
    UpdateHistoryPanel();
}

ReturnValue MainWindow::CheckOK(QLineEdit * lineEdit)
{
    ReturnValue returnValue;                 // 默认 isNull=true, isNumeric=false, value=0
    if (lineEdit == nullptr) {
        return returnValue;
    }
    const QString inputText = lineEdit->text().trimmed(); // 获取输入的文本
    if (inputText.isEmpty()) {
        returnValue.isNull = true;
        return returnValue;
    }
    //判断高和宽是否正确输入数字
    bool isNumeric = false;
    const double_t value = inputText.toDouble(&isNumeric);
    if (!isNumeric) {
        returnValue.isNull = false;
        returnValue.isNumeric = false;
        return returnValue;
    }
    returnValue.isNull = false;
    returnValue.isNumeric = true;
    returnValue.value = value;
    return returnValue;
}


MainWindow::~MainWindow()
{
    delete ui;
}

// 撤销操作，从链表中获取上一步的图像数据进行恢复
OperationType MainWindow::SaveImageDataToHistory(const ImageSnapshot &snap)
{
    // 保存当前图像数据到链表
    if (!imageDataHistory.empty()) {
        const ImageSnapshot &back = imageDataHistory.back();
        const bool samePixels = (snap.data == back.data && snap.width == back.width &&
                                 snap.height == back.height);
        // 像素相同但图层结构不同(新建/删除/排序/合并)也算一步
        if (!samePixels || !SameLayerStack(snap.layers, back.layers)) {
            imageDataHistory.push_back(snap);
            // 控制历史长度，避免大图下内存无限增长
            while (static_cast<int>(imageDataHistory.size()) > m_historyLimit) {
                imageDataHistory.pop_front();
            }
            return UNDO;
        }
    } else {
        imageDataHistory.push_back(snap);
        return UNDO;
    }

    return REDO; // 返回重做类型
}

// 应用一个快照：写历史 + 同步尺寸 + 刷新显示
void MainWindow::ApplySnapshot(const ImageSnapshot &snap, bool keepRotateBase)
{
    if (snap.data.empty()) {
        return;
    }
    // 尺寸与像素数据必须严格匹配，否则显示阶段会读越界
    if (snap.width <= 0 || snap.height <= 0 ||
        snap.data.size() != static_cast<size_t>(snap.width) * static_cast<size_t>(snap.height) * 3) {
        UpdateStatus("结果尺寸与像素数据不一致，已忽略本次操作");
        return;
    }
    // 只有旋转结果才继续沿用无损基准；其它算子改的是像素本身，
    // 基准必须换成新的当前图像，否则后续旋转会丢掉这些修改
    if (!keepRotateBase) {
        InvalidateRotateBase();
    }
    SaveImageDataToHistory(snap);
    m_contentQuad = snap.quad;
    // 尺寸可能已被算子改变(例如旋转)，同步到文件头信息
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    myValue.bmpInfo.SetImageSize(static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    myValue.bmp.SetFileSize(sizeof(BMP) + sizeof(BMPInfo) +
                            static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    RestoreSnapshot(snap);
    ShowImage(imageData, myValue, snap.width, snap.height);
    UpdateLayerPanel();
    UpdateHistoryPanel();
}


void MainWindow::UndoImageProcessing()
{
    if (imageDataHistory.size() <= 1) {
        qDebug() << "Undo operation failed: no more history";
        UpdateStatus("已经是最早的状态了");
        return;
    }
    // 当前状态压入重做栈
    CancelPreview();
    redoImageDataHistory.push_back(imageDataHistory.back());
    imageDataHistory.pop_back();

    const ImageSnapshot &snap = imageDataHistory.back();
    RestoreSnapshot(snap);
    m_contentQuad = snap.quad;
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    ShowImage(imageData, myValue, snap.width, snap.height);
    InvalidateRotateBase();   // 撤销后以当前图像作为新的旋转基准
    qDebug() << "Undo operation";
    UpdateStatus("已撤销");
    UpdateLayerPanel();
    UpdateHistoryPanel();
}

void MainWindow::RedoImageProcessing()
{
    if (redoImageDataHistory.empty()) {
        qDebug() << "Redo operation failed: no redo history";
        UpdateStatus("没有可重做的操作");
        return;
    }
    CancelPreview();
    const ImageSnapshot snap = redoImageDataHistory.back();
    imageDataHistory.push_back(snap);
    redoImageDataHistory.pop_back();

    // 恢复图像数据
    RestoreSnapshot(snap);
    m_contentQuad = snap.quad;
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    ShowImage(imageData, myValue, snap.width, snap.height);
    InvalidateRotateBase();   // 重做后以当前图像作为新的旋转基准
    qDebug() << "Redo operation";
    UpdateStatus("已重做");
    UpdateLayerPanel();
    UpdateHistoryPanel();
}

// 刷新右侧"历史记录"面板：每次图像状态变化后调用
void MainWindow::UpdateHistoryPanel()
{
    if (ui->historyList == nullptr) {
        return;
    }
    // 重建列表期间屏蔽信号，避免 setCurrentRow 反过来触发点击槽
    const QSignalBlocker blocker(ui->historyList);
    ui->historyList->clear();
    const int lastIndex = static_cast<int>(imageDataHistory.size()) - 1;
    int index = 0;
    for (auto it = imageDataHistory.begin(); it != imageDataHistory.end(); ++it, ++index) {
        QString text = QString("步骤 %1 · %2×%3").arg(index + 1).arg(it->width).arg(it->height);
        if (index == 0) {
            text += QString(" · 原图");
        }
        QListWidgetItem *entry = new QListWidgetItem(text);
        if (index == lastIndex) {
            // 当前步用加粗 + 箭头标记；列表禁用选中，避免系统高亮色破坏深色主题
            QFont font = entry->font();
            font.setBold(true);
            entry->setFont(font);
            entry->setText(QString("▶ ") + text);
        }
        ui->historyList->addItem(entry);
    }
    ui->historyList->scrollToBottom();
}

// 点击历史项：连续撤销/重做到目标步骤(历史是栈，当前状态永远是最后一项)
void MainWindow::on_historyList_itemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }
    const int target = ui->historyList->row(item);
    int current = static_cast<int>(imageDataHistory.size()) - 1;
    while (current > target && imageDataHistory.size() > 1) {
        UndoImageProcessing();
        --current;
    }
    while (current < target && !redoImageDataHistory.empty()) {
        RedoImageProcessing();
        ++current;
    }
}

// 打开图片文件并初始化界面状态（BMP 走自有解析器，其它格式由 Qt 插件解码）
bool MainWindow::LoadImage(const QString &path)
{
    CancelPreview();   // 打开新图前先回滚未提交的预览
    MyValue loaded;
    QString errMsg;
    if (!MYFunction::ReadImageFile(path.toStdString(), loaded, errMsg)) {
        function.CreateMessagebox("打开失败", errMsg);
        UpdateStatus("打开失败：" + errMsg);
        return false;
    }
    if (loaded.imageData.empty() || loaded.bmpInfo.GetWidth() <= 0 || loaded.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("打开失败", "图像内容为空");
        UpdateStatus("打开失败：图像内容为空");
        return false;
    }
    qDebug() << "image size is:" << loaded.imageData.size()
             << "pixel count:" << loaded.imageData.size() / 3;

    // 每打开一张图就新建一个文档(标签页)
    Document doc;
    doc.path = path;
    doc.value = loaded;
    doc.imageData = loaded.imageData;
    doc.originalImageData = loaded.imageData;
    doc.originalWidth = loaded.bmpInfo.GetWidth();
    doc.originalHeight = loaded.bmpInfo.GetHeight();
    doc.quad.setRect(doc.originalWidth, doc.originalHeight);
    doc.canSave = true;
    doc.layers.push_back(Layer{QString("图层 1"), doc.imageData});
    doc.activeLayer = 0;
    {
        // 打开后的状态就是历史第 1 步
        ImageSnapshot snap;
        snap.data = doc.imageData;
        snap.width = doc.originalWidth;
        snap.height = doc.originalHeight;
        snap.quad = doc.quad;
        doc.history.push_back(snap);
    }

    m_docs.push_back(std::move(doc));
    _SetVisible(true);
    ActivateDocument(static_cast<int>(m_docs.size()) - 1);
    UpdateStatus(QString("已打开 %1").arg(QFileInfo(path).fileName()));
    return true;
}

// ------------------------------------------------------------------
// 多文档标签页
// ------------------------------------------------------------------

void MainWindow::SetupDocumentTabs()
{
    m_tabBar = new QTabBar(this);
    m_tabBar->setObjectName("documentTabBar");
    m_tabBar->setTabsClosable(true);
    // 标签顺序与 m_docs 一一对应，不允许拖动重排(否则下标会和文档错位)
    m_tabBar->setMovable(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setVisible(false);
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        ActivateDocument(index);
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) {
        CloseDocument(index);
    });

    // 显示区改为 "标签栏 + 图像" 的纵向布局(不改 .ui，运行时重排即可)
    QWidget *viewBox = new QWidget(this);
    viewBox->setObjectName("viewBox");
    QVBoxLayout *viewLayout = new QVBoxLayout(viewBox);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    viewLayout->setSpacing(6);
    ui->horizontalLayout_4->removeWidget(ui->imageLabel);
    viewLayout->addWidget(m_tabBar);
    viewLayout->addWidget(ui->imageLabel, 1);
    ui->horizontalLayout_4->insertWidget(0, viewBox, 1);

    // 文件菜单增加"关闭当前图像"
    QAction *closeAction = new QAction(QString("关闭当前图像"), this);
    closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    ui->menu->addSeparator();
    ui->menu->addAction(closeAction);
    connect(closeAction, &QAction::triggered, this, [this]() {
        CloseDocument(m_activeDoc);
    });
}

void MainWindow::UpdateTabBar()
{
    if (m_tabBar == nullptr) {
        return;
    }
    // 重建期间屏蔽信号，避免 setCurrentIndex 反过来触发切换
    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) {
        m_tabBar->removeTab(0);
    }
    for (size_t i = 0; i < m_docs.size(); ++i) {
        const QString name = m_docs[i].path.isEmpty()
                                 ? QString("未命名 %1").arg(static_cast<int>(i) + 1)
                                 : QFileInfo(m_docs[i].path).fileName();
        const int tabIndex = m_tabBar->addTab(name);
        m_tabBar->setTabToolTip(tabIndex, m_docs[i].path);

        // 自绘关闭按钮：QSS 会覆盖 Qt 默认的关闭图标，这里用文字"✕"保证可见
        QToolButton *closeBtn = new QToolButton(m_tabBar);
        closeBtn->setObjectName("tabCloseButton");
        closeBtn->setText(QString::fromUtf8("\xE2\x9C\x95"));
        closeBtn->setToolTip(QString("关闭该图像"));
        closeBtn->setCursor(Qt::ArrowCursor);
        closeBtn->setAutoRaise(true);
        closeBtn->setFixedSize(16, 16);
        connect(closeBtn, &QToolButton::clicked, this, [this, tabIndex]() {
            CloseDocument(tabIndex);
        });
        m_tabBar->setTabButton(tabIndex, QTabBar::RightSide, closeBtn);
    }
    if (m_activeDoc >= 0 && m_activeDoc < m_tabBar->count()) {
        m_tabBar->setCurrentIndex(m_activeDoc);
    }
    m_tabBar->blockSignals(false);
    m_tabBar->setVisible(!m_docs.empty());
}

// 清空当前活动文档的全部成员状态
void MainWindow::ResetDocumentState()
{
    CancelPreview();
    BMPPath.clear();
    imageData.clear();
    originalImageData.clear();
    myValue = MyValue();
    imageDataHistory.clear();
    redoImageDataHistory.clear();
    m_originalWidth = 0;
    m_originalHeight = 0;
    m_contentQuad = ContentQuad();
    m_rotBaseValid = false;
    m_rotBase.clear();
    m_rotBaseW = 0;
    m_rotBaseH = 0;
    m_rotBaseQuad = ContentQuad();
    m_rotTotal = 0.0;
    canSave = false;
    imageSize = 0;
    pixelCount = 0;
    segmentSize = 0;
    m_selection = QRect();
    m_layers.clear();
    m_activeLayer = 0;
    m_composite.clear();
    UpdateSelectionOverlay();
    UpdateLayerPanel();
}

// 切换标签：把当前状态整体换出、把目标文档状态换入(移动语义，不拷贝像素)
void MainWindow::ActivateDocument(int index)
{
    if (m_busy) {
        UpdateStatus("正在处理中，请稍后再切换文档");
        UpdateTabBar();   // 把标签选中状态恢复成当前文档
        return;
    }
    if (index < 0 || index >= static_cast<int>(m_docs.size())) {
        return;
    }
    if (index == m_activeDoc) {
        UpdateTabBar();
        return;
    }
    CancelPreview();

    if (m_activeDoc >= 0 && m_activeDoc < static_cast<int>(m_docs.size())) {
        Document &cur = m_docs[static_cast<size_t>(m_activeDoc)];
        cur.path = QString::fromStdString(BMPPath);
        cur.value = std::move(myValue);
        cur.imageData = std::move(imageData);
        cur.originalImageData = std::move(originalImageData);
        cur.history = std::move(imageDataHistory);
        cur.redoHistory = std::move(redoImageDataHistory);
        cur.originalWidth = m_originalWidth;
        cur.originalHeight = m_originalHeight;
        cur.quad = m_contentQuad;
        cur.rotBaseValid = m_rotBaseValid;
        cur.rotBase = std::move(m_rotBase);
        cur.rotBaseW = m_rotBaseW;
        cur.rotBaseH = m_rotBaseH;
        cur.rotBaseQuad = m_rotBaseQuad;
        cur.rotTotal = m_rotTotal;
        cur.canSave = canSave;
        cur.selection = m_selection;
        cur.layers = std::move(m_layers);
        cur.activeLayer = m_activeLayer;
        ResetDocumentState();   // 成员里不再保留旧文档的副本
    }

    m_activeDoc = index;
    Document &doc = m_docs[static_cast<size_t>(index)];
    BMPPath = doc.path.toStdString();
    myValue = std::move(doc.value);
    imageData = std::move(doc.imageData);
    originalImageData = std::move(doc.originalImageData);
    imageDataHistory = std::move(doc.history);
    redoImageDataHistory = std::move(doc.redoHistory);
    m_originalWidth = doc.originalWidth;
    m_originalHeight = doc.originalHeight;
    m_contentQuad = doc.quad;
    m_rotBaseValid = doc.rotBaseValid;
    m_rotBase = std::move(doc.rotBase);
    m_rotBaseW = doc.rotBaseW;
    m_rotBaseH = doc.rotBaseH;
    m_rotBaseQuad = doc.rotBaseQuad;
    m_rotTotal = doc.rotTotal;
    canSave = doc.canSave;
    m_selection = doc.selection;
    m_layers = std::move(doc.layers);
    m_activeLayer = doc.activeLayer;
    if (m_layers.empty() && !imageData.empty()) {
        m_layers.push_back(Layer{QString("图层 1"), imageData});
        m_activeLayer = 0;
    }

    imageSize = imageData.size();
    pixelCount = imageSize / 3;
    segmentSize = pixelCount / static_cast<size_t>(std::max(1, num_threads));
    if (segmentSize == 0) {
        segmentSize = 1;
    }

    _SetVisible(!imageData.empty());
    if (imageData.empty()) {
        ui->imageLabel->setText(QString("双击此处，或按 Ctrl+N 打开图片"));
        ui->imageLabel->setPixmap(QPixmap());
    } else {
        const std::vector<uint8_t> &shown = BuildComposite();
        SetDisplayPixmap(shown, myValue.bmpInfo.GetWidth(), myValue.bmpInfo.GetHeight());
    }
    SyncSelectionToImage();
    UpdateStatusInfo();
    UpdateHistoryPanel();
    UpdateLayerPanel();
    UpdateTabBar();
}

void MainWindow::CloseDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(m_docs.size())) {
        return;
    }
    if (m_busy) {
        UpdateStatus("正在处理中，暂不能关闭图像");
        return;
    }

    const bool wasActive = (index == m_activeDoc);
    if (wasActive) {
        ResetDocumentState();
        m_activeDoc = -1;
    }
    m_docs.erase(m_docs.begin() + index);
    if (m_activeDoc > index) {
        --m_activeDoc;
    }

    if (m_docs.empty()) {
        ui->imageLabel->setText(QString("双击此处，或按 Ctrl+N 打开图片"));
        ui->imageLabel->setPixmap(QPixmap());
        _SetVisible(false);
        SyncSelectionToImage();
        UpdateStatus("已关闭全部图像");
        UpdateStatusInfo();
        UpdateHistoryPanel();
        UpdateTabBar();
        return;
    }
    const int target = wasActive ? std::min(index, static_cast<int>(m_docs.size()) - 1)
                                 : m_activeDoc;
    UpdateTabBar();
    ActivateDocument(target);
}


void MainWindow::on_openImage_triggered()
{
    // 文件默认路径
    const QString defaultPath = QDir::homePath();
    // 常见图片格式（BMP/PNG/JPG/GIF/TIFF/WebP/TGA/ICO/SVG）
    const QString filter = QString("图片文件 (*.bmp *.png *.jpg *.jpeg *.gif *.tif *.tiff *.webp *.tga *.ico *.svg);;"
                                   "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;GIF (*.gif);;"
                                   "TIFF (*.tif *.tiff);;WebP (*.webp);;TGA (*.tga);;图标 (*.ico *.icns);;SVG (*.svg)");
    const QString path = QFileDialog::getOpenFileName(this, "选择图片文件", defaultPath, filter);
    if (path.isEmpty()) {
        qDebug() << "未选择文件";
        return;
    }
    LoadImage(path);
}

void MainWindow::on_actionsave_triggered()
{
    if (!canSave || imageData.empty()) {
        function.CreateMessagebox("提示", "没有任何文件");
        qDebug() << "文件为空！";
        return;
    }

    // 保存当前正在显示的图像(而不是历史栈里的旧数据)
    const QString filter = QString("BMP 图像 (*.bmp);;PNG 图像 (*.png);;JPEG 图像 (*.jpg *.jpeg);;"
                                   "TIFF 图像 (*.tif *.tiff);;WebP 图像 (*.webp)");
    QString filePath = QFileDialog::getSaveFileName(this, "保存图像",
                                                    QString::fromStdString(BMPPath), filter);
    if (filePath.isEmpty()) {
        qDebug() << "保存操作已取消。";
        return;
    }

    QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix.isEmpty()) {
        filePath += ".bmp";
        suffix = "bmp";
    }

    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();
    // 保存的是"看到的画面"：多图层时先合成，单图层时就是当前图层
    const std::vector<uint8_t> &pixels = DisplayPixels();

    // BMP：沿用项目自己的读写实现，保证自底向上的内存布局不被破坏
    if (suffix == "bmp") {
        BMP outBmp = myValue.bmp;
        BMPInfo outInfo = myValue.bmpInfo;
        MYFunction::SetBMPHeaderValues(outBmp, outInfo, width, height, outInfo.GetBitsPerPixel());
        if (MYFunction::WriteBMPFile(filePath.toStdString(), pixels, outBmp, outInfo)) {
            BMPPath = filePath.toStdString();
            UpdateTabBar();
            UpdateStatus(QString("已保存到 %1").arg(QFileInfo(filePath).fileName()));
        } else {
            function.CreateMessagebox("保存失败", "无法写入文件，请检查路径是否有效");
        }
        return;
    }

    // 其它格式：交给 Qt 图像插件（PNG / JPEG / TIFF / WebP）
    const QImage image = Function::ToDisplayImage(pixels, width, height);
    if (image.isNull()) {
        function.CreateMessagebox("保存失败", "当前图像数据无效");
        return;
    }
    QImageWriter writer(filePath);
    if (suffix == "jpg" || suffix == "jpeg") {
        writer.setQuality(92);
    }
    if (writer.write(image)) {
        BMPPath = filePath.toStdString();
        UpdateTabBar();
        UpdateStatus(QString("已导出 %1").arg(QFileInfo(filePath).fileName()));
    } else {
        function.CreateMessagebox("保存失败",
                                  "无法写入该格式：" + writer.errorString() +
                                  "\n（部分格式需要对应的 Qt 图像插件支持）");
        UpdateStatus("导出失败：" + QFileInfo(filePath).fileName());
    }
}

//清理每段数据,防止数据冲突引发异常
void MainWindow::ClearSegmentData()
{
    threads.clear();
    segmentStarts.clear();
}

// ------------------------------------------------------------------
// 算子调度
// ------------------------------------------------------------------

// 把算子按线程分段执行(以像素为单位，保证 BGR 三通道不会被拆散)
void MainWindow::RunSegmented(const OpContext &ctx,
                              const std::function<void(size_t, size_t)> &fn)
{
    const int n = std::max(1, ctx.numThreads);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const size_t start = static_cast<size_t>(i) * ctx.segmentSize;
        const size_t end = (i == n - 1) ? ctx.pixelCount
                                        : std::min(ctx.pixelCount, start + ctx.segmentSize);
        if (start >= end) {
            continue;
        }
        workers.emplace_back([&fn, start, end]() {
            fn(start, end);
        });
    }
    for (auto &t : workers) {
        t.join();
    }
}

void MainWindow::SetBusy(bool busy)
{
    m_busy = busy;
    // 处理中禁止切换/关闭文档，避免结果写进错误的文档
    if (m_tabBar != nullptr) {
        m_tabBar->setEnabled(!busy);
    }
    // 处理中禁用主控区域，避免并发修改同一份像素数据
    ui->widget->setEnabled(!busy);
    ui->menu_edit->setEnabled(!busy);
    ui->menu_blur->setEnabled(!busy);
    ui->menu_base->setEnabled(!busy);
    ui->menu_unbase->setEnabled(!busy);
    ui->menu_transform->setEnabled(!busy);
    ui->menu_enhance->setEnabled(!busy);
    ui->menu_morph->setEnabled(!busy);
    if (m_selectionMenu != nullptr) {
        m_selectionMenu->setEnabled(!busy);
    }
    if (m_layerMenu != nullptr) {
        m_layerMenu->setEnabled(!busy);
    }
    if (m_overlay != nullptr) {
        m_overlay->setEnabled(!busy);   // 处理中不接受选区拖拽
    }
    // 处理中显示等待光标
    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::UpdateStatus(const QString &text)
{
    statusBar()->showMessage(text);
}

void MainWindow::UpdateStatusInfo()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    QString info;
    if (canSave && w > 0 && h > 0) {
        info = QString("%1 × %2 px   |   线程 %3   |   历史 %4 步")
                   .arg(w).arg(h).arg(num_threads).arg(static_cast<int>(imageDataHistory.size()));
    } else {
        info = QString("线程 %1").arg(num_threads);
    }
    if (QLabel *right = findChild<QLabel *>("statusInfoLabel")) {
        right->setText(info);
    } else {
        QLabel *label = new QLabel(info, this);
        label->setObjectName("statusInfoLabel");
        statusBar()->addPermanentWidget(label);
    }
}

// 在后台线程执行算子；完成后自动写历史并刷新显示
void MainWindow::ApplyOperator(const QString &title,
                               std::function<void(std::vector<uint8_t> &, OpContext &)> op,
                               const ImageSnapshot *sourceOverride,
                               std::function<void()> onDone)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    CancelPreview();   // 新算子从已提交的图像出发

    // 输入既可以是当前图像，也可以是旋转专用的一次性快照(无损基准)
    const std::vector<uint8_t> *input = &imageData;
    int32_t inWidth = myValue.bmpInfo.GetWidth();
    int32_t inHeight = myValue.bmpInfo.GetHeight();
    ContentQuad inQuad = m_contentQuad;
    if (sourceOverride && !sourceOverride->data.empty()) {
        input = &sourceOverride->data;
        inWidth = sourceOverride->width;
        inHeight = sourceOverride->height;
        inQuad = sourceOverride->quad;
    }

    OpContext ctx;
    ctx.width = inWidth;
    ctx.height = inHeight;
    ctx.pixelCount = input->size() / 3;
    ctx.numThreads = num_threads;
    ctx.segmentSize = ctx.pixelCount / static_cast<size_t>(std::max(1, num_threads));
    if (ctx.segmentSize == 0) ctx.segmentSize = 1;
    ctx.quad = inQuad;

    const std::vector<uint8_t> source = *input;   // 在 UI 线程取快照，避免数据竞争
    // 图层栈也取一份快照：结果写回"当前活动图层"，其余图层保持不变
    const std::vector<Layer> stack = m_layers;
    const int stackActive = m_activeLayer;
    const bool fromRotation = (sourceOverride != nullptr);
    SetBusy(true);
    UpdateStatus(title + " 处理中…");

    auto result = std::make_shared<ImageSnapshot>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
            [this, watcher, result, title, fromRotation, stack, stackActive, onDone]() {
        watcher->deleteLater();
        if (result && !result->data.empty()) {
            // 把结果作为活动图层的像素(活动图层的 data 单独存在 data 里，避免重复)
            result->layers = stack;
            if (!result->layers.empty()) {
                result->activeLayer = qBound(0, stackActive,
                                             static_cast<int>(result->layers.size()) - 1);
                result->layers[result->activeLayer].data.clear();
            }
            ApplySnapshot(*result, fromRotation);
            UpdateStatus(title + " 完成");
        } else {
            UpdateStatus(title + " 未产生结果");
        }
        SetBusy(false);
        UpdateStatusInfo();
        if (onDone) {
            onDone();
        }
    });
    watcher->setFuture(QtConcurrent::run([source, op, ctx, result]() mutable {
        std::vector<uint8_t> work = source;
        op(work, ctx);              // 算子可以通过 ctx 修改画布宽高与内容区域(例如旋转)
        result->data = std::move(work);
        result->width = ctx.width;
        result->height = ctx.height;
        result->quad = ctx.quad;
    }));
}

// 任何改动像素/几何的非旋转操作、撤销、重做、重置、重新打开图片，
// 都会让"旋转无损基准"失效(下次旋转时以当前图像作为新基准)
void MainWindow::InvalidateRotateBase()
{
    m_rotBaseValid = false;
    m_rotTotal = 0.0;
    std::vector<uint8_t>().swap(m_rotBase);   // 真正释放内存
}

// 旋转：始终从无损基准按累计角度重新采样一次，
// 这样无论转多少次，插值只发生一次，不会越转越糊
void MainWindow::ApplyRotation(double deltaDegrees, const QString &title)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (!RequireSingleLayer(title)) {
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }

    // 第一次旋转：把当前图像作为基准(此时它还没有被旋转插值糊过)
    if (!m_rotBaseValid || m_rotBase.empty() ||
        m_rotBase.size() != static_cast<size_t>(m_rotBaseW) * m_rotBaseH * 3) {
        m_rotBase = imageData;
        m_rotBaseW = myValue.bmpInfo.GetWidth();
        m_rotBaseH = myValue.bmpInfo.GetHeight();
        m_rotBaseQuad = m_contentQuad;
        if (!m_rotBaseQuad.isValid()) {
            m_rotBaseQuad.setRect(m_rotBaseW, m_rotBaseH);
        }
        m_rotTotal = 0.0;
        m_rotBaseValid = true;
    }

    // 累计角度归一到 [0, 360)
    double total = std::fmod(m_rotTotal + deltaDegrees, 360.0);
    if (total < 0.0) {
        total += 360.0;
    }
    if (!std::isfinite(total)) {
        return;
    }

    ImageSnapshot base;
    base.data = m_rotBase;
    base.width = m_rotBaseW;
    base.height = m_rotBaseH;
    base.quad = m_rotBaseQuad;

    auto applied = std::make_shared<bool>(false);
    ApplyOperator(
        title,
        [total, applied](std::vector<uint8_t> &d, OpContext &ctx) {
            Function f;
            *applied = f.RotateImage(d, ctx.width, ctx.height, total, ctx.quad);
        },
        &base,
        [this, total, applied]() {
            if (*applied) {
                m_rotTotal = total;      // 基准继续沿用，角度已累计
            } else {
                // 本次旋转没有生效(角度为 0/360 的整数倍、或画布超出上限)：
                // 基准作废，回到"以当前图像为基准"的状态，避免状态错位
                InvalidateRotateBase();
            }
        });
}

// 子窗口结果写回主窗口
void MainWindow::ApplyResult(std::vector<uint8_t> &result)
{
    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();
    if (result.empty() || width <= 0 || height <= 0 ||
        result.size() != static_cast<size_t>(width) * height * 3) {
        UpdateStatus("子窗口结果与当前图像不一致，已忽略");
        return;
    }
    if (!m_contentQuad.isValid()) {
        m_contentQuad.setRect(width, height);
    }
    // 子窗口只改像素、不改画布几何；结果写回当前活动图层
    ImageSnapshot snap = MakeSnapshot(result, width, height, m_contentQuad);
    ApplySnapshot(snap);
    UpdateStatus("已应用子窗口结果");
}

// ------------------------------------------------------------------
// 算子槽函数
// ------------------------------------------------------------------

void MainWindow::on_actiongray_triggered()
{
    ApplyOperator("灰度化", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.ConvertToGray(d, s, e);
        });
    });
}

void MainWindow::on_actionautoContrast_triggered()
{
    qDebug() << "click auto contrast";
    ApplyOperator("自动对比度", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        const float_t aver = static_cast<float_t>(this->function.CalAver(d));
        const float_t standard = static_cast<float_t>(this->function.CalStandard(d, aver));
        RunSegmented(ctx, [this, &d, aver, standard](size_t s, size_t e) {
            this->function.AutoContrast(d, aver, standard, s, e);
        });
    });
}

void MainWindow::on_actionaver_triggered()
{
    qDebug() << "aver";
    ApplyOperator("均值模糊", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d, ctx](size_t s, size_t e) {
            this->function.AverageBlur(d, static_cast<uint32_t>(ctx.width),
                                       static_cast<uint32_t>(ctx.height), s, e);
        });
    });
}

void MainWindow::on_btn_resetAll_clicked()
{
    ResetAll(myValue);
}

void MainWindow::on_actionblend_triggered()
{
    qDebug() << "i am in a blend window!";
    blendWindow = new BlendWindow(this, myValue);
    blendWindow->show();
}

void MainWindow::on_btn_brightness_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_brightness);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -150 || returnValue.value > 150) {
        function.CreateMessagebox("提示", "超出范围，范围是[-150,150]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("亮度调整", ui->lineEdit_brightness, value)) {
        return;
    }
    ApplyOperator("亮度调整", MakeBrightnessOp(value));
}

void MainWindow::on_btn_contrast_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_contrast);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -50 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[-50,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("对比度调整", ui->lineEdit_contrast, value)) {
        return;
    }
    ApplyOperator("对比度调整", MakeContrastOp(value));
}

void MainWindow::on_btn_saturation_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_saturation);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < -100 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[-100,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("饱和度调整", ui->lineEdit_saturation, value)) {
        return;
    }
    ApplyOperator("饱和度调整", MakeSaturationOp(value));
}

void MainWindow::on_actioncolorBalance_triggered()
{
    ApplyOperator("色彩平衡", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.ColorBalance(d, ctx.width, ctx.height);
    });
}

void MainWindow::on_actioncolorMap_triggered()
{
    qDebug() << "I am in a color map window!";
    ColorMap *colorMap = new ColorMap(this, myValue);
    colorMap->show();
}

void MainWindow::on_actionreverse_triggered()
{
    ApplyOperator("颜色反转", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.InvertColors(d, s, e);
        });
    });
}

void MainWindow::on_actioncomplementary_triggered()
{
    ApplyOperator("补色", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.Complementary(d, s, e);
        });
    });
}

void MainWindow::on_actionliquefaction_triggered()
{
    qDebug() << "I am in a eye window!";
    eye *myEye = new eye(this, myValue);
    myEye->show();
}

void MainWindow::on_actionface_triggered()
{
    qDebug() << "I am in a face window!";
    Face *face = new Face(this, myValue);
    face->show();
}

void MainWindow::on_actionfishEye_triggered()
{
    ApplyOperator("鱼眼效果", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::promise<std::vector<uint8_t>> promise;
        std::future<std::vector<uint8_t>> future = promise.get_future();
        this->function.Fisheye(d, promise, ctx.width, ctx.height);
        d = future.get();
    });
}

void MainWindow::on_actiongauss_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 0, 0, 10, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    qDebug() << "用户输入的程度:" << degree;
    ApplyOperator("高斯模糊", [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> promise;
        std::future<std::vector<uint8_t>> future = promise.get_future();
        f.Gauss(d, promise, ctx.width, ctx.height, degree);
        d = future.get();
    });
}

void MainWindow::on_btn_rotate_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_rotate);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    const double_t angle = returnValue.value;
    ApplyRotation(angle, "顺时针旋转");
}

void MainWindow::on_btn_rotate_r_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_rotate_r);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    const double_t angle = returnValue.value;
    ApplyRotation(-angle, "逆时针旋转");
}

void MainWindow::on_actionmedian_triggered()
{
    qDebug() << "median";
    ApplyOperator("中值滤波", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.MedianBlur(d, static_cast<uint32_t>(ctx.width), static_cast<uint32_t>(ctx.height));
    });
}

void MainWindow::on_actionmosaic_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 4, 1, 100, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    const uint32_t step = static_cast<uint32_t>(std::max(1.0, degree));
    ApplyOperator("马赛克", [this, step](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.FullMosaic(d, static_cast<uint32_t>(ctx.width),
                                  static_cast<uint32_t>(ctx.height), step);
    });
}

void MainWindow::on_btn_shadow_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_shadow);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 100) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,100]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("阴影调整", ui->lineEdit_shadow, value)) {
        return;
    }
    ApplyOperator("阴影调整", MakeShadowOp(value));
}

void MainWindow::on_btn_highlight_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_highlight);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 255) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,255]");
        return;
    }
    const double_t value = returnValue.value;
    if (CommitPreview("高光调整", ui->lineEdit_highlight, value)) {
        return;
    }
    ApplyOperator("高光调整", MakeHighlightOp(value));
}

void MainWindow::on_btn_sharpen_clicked()
{
    qDebug() << "ok";
    ReturnValue returnValue = CheckOK(ui->lineEdit_sharpen);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 200) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,200]");
        return;
    }
    const double_t amount = returnValue.value;
    if (CommitPreview("锐化", ui->lineEdit_sharpen, amount)) {
        return;
    }
    ApplyOperator("锐化", MakeSharpenOp(amount));
}

void MainWindow::on_actionsobel_triggered()
{
    ApplyOperator("Sobel 边缘检测", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        d = this->function.SobelEdge(d, ctx.width, ctx.height);
    });
}

void MainWindow::on_btn_threshold_ok_clicked()
{
    ReturnValue returnValue = CheckOK(ui->lineEdit_threshold);
    if (returnValue.isNull) {
        function.CreateMessagebox("提示", "请输入");
        return;
    }
    if (!returnValue.isNumeric) {
        function.CreateMessagebox("提示", "输入数字");
        return;
    }
    if (returnValue.value < 0 || returnValue.value > 255) {
        function.CreateMessagebox("提示", "超出范围，范围是[0,255]");
        return;
    }
    const double_t threshold = returnValue.value;
    if (CommitPreview("阈值处理", ui->lineEdit_threshold, threshold)) {
        return;
    }
    ApplyOperator("阈值处理", MakeThresholdOp(threshold));
}

void MainWindow::on_actionscale_triggered()
{
    qDebug() << "I am in a inter window!";
    if (!RequireSingleLayer("缩放图像")) {
        return;
    }
    Interpolation *inter = new Interpolation(this, myValue);
    inter->show();
}

void MainWindow::on_btn_undo_clicked()
{
    UndoImageProcessing();
}

void MainWindow::on_actionundo_triggered()
{
    UndoImageProcessing();
}

void MainWindow::on_actionreset_triggered()
{
    ResetAll(myValue);
}

void MainWindow::on_actionredo_triggered()
{
    qDebug() << "redo";
    RedoImageProcessing();
}

void MainWindow::_SetVisible(bool set)
{
    qDebug() << "Setting visibility to:" << set;
    ui->toolBar->setVisible(set);
    ui->menu_edit->menuAction()->setVisible(set);
    ui->menu_blur->menuAction()->setVisible(set);
    ui->menu_base->menuAction()->setVisible(set);
    ui->menu_unbase->menuAction()->setVisible(set);
    ui->menu_transform->menuAction()->setVisible(set);
    ui->menu_enhance->menuAction()->setVisible(set);
    ui->menu_morph->menuAction()->setVisible(set);
    if (m_selectionMenu != nullptr) {
        m_selectionMenu->menuAction()->setVisible(set);
    }
    if (m_layerMenu != nullptr) {
        m_layerMenu->menuAction()->setVisible(set);
    }
    ui->widget->setVisible(set);
}

void MainWindow::_SetShortCut()
{
    ui->openImage->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    ui->actionsave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    ui->actionundo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    ui->actionreset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    ui->actionredo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("退出程序"), QString(tr("确认退出程序")),
                              QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::No) {
        event->ignore(); // 忽略退出信号，程序继续进行
    } else {
        event->accept(); // 接受退出信号，程序退出
    }
}

// 缩放相关：倍率必须 >= 1，否则会出现除零/取模崩溃
static bool AskScaleFactors(QWidget *parent, int &heightFactor, int &widthFactor)
{
    bool ok1 = false;
    bool ok2 = false;
    const int height = QInputDialog::getInt(parent, QObject::tr("输入高度倍数"), QObject::tr("高度倍数:"), 1, 1, 100, 1, &ok1);
    const int width = QInputDialog::getInt(parent, QObject::tr("输入宽度倍数"), QObject::tr("宽度倍数:"), 1, 1, 100, 1, &ok2);
    if (!ok1 || !ok2) {
        return false;
    }
    heightFactor = height;
    widthFactor = width;
    return true;
}

void MainWindow::on_actionlarge_near_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("放大图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeNear(h, w);
}

void MainWindow::on_actionsmall_near_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("缩小图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallNear(h, w);
}

void MainWindow::on_actionlarge_bilinear_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("放大图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeBilinear(h, w);
}

void MainWindow::on_actionsmall_bilinear_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("缩小图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallBilinear(h, w);
}

void MainWindow::on_actionlarge_bicubic_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("放大图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowLargeBicubic(h, w);
}

void MainWindow::on_actionsmall_bicubic_triggered()
{
    int h = 1, w = 1;
    if (!RequireSingleLayer("缩小图像")) {
        return;
    }
    if (!AskScaleFactors(this, h, w)) {
        qDebug() << "用户取消输入";
        return;
    }
    Interpolation inter(this, myValue);
    inter.ShowSmallBicubic(h, w);
}

void MainWindow::on_actionhighContrast_triggered()
{
    bool ok = false;
    const double degree = QInputDialog::getDouble(this, tr("输入程度"), tr("程度:"), 1, 1, 100, 1, &ok);
    if (!ok) {
        qDebug() << "用户取消输入";
        return;
    }
    ApplyOperator("高反差保留", [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
        Function f;
        std::promise<std::vector<uint8_t>> blurPromise;
        std::future<std::vector<uint8_t>> blurFuture = blurPromise.get_future();
        f.Gauss(d, blurPromise, ctx.width, ctx.height, degree);
        const std::vector<uint8_t> blurred = blurFuture.get();

        std::promise<std::vector<uint8_t>> highPromise;
        std::future<std::vector<uint8_t>> highFuture = highPromise.get_future();
        f.HighContrast(d, highPromise, blurred);
        d = highFuture.get();
    });
}

// ------------------------------------------------------------------
// P2-A：几何变换 / 增强 / 形态学 / 卷积 / USM
// ------------------------------------------------------------------
void MainWindow::on_actionflipH_triggered()
{
    if (!RequireSingleLayer("水平翻转")) {
        return;
    }
    ApplyOperator("水平翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Horizontal, ctx.quad);
    });
}

void MainWindow::on_actionflipV_triggered()
{
    if (!RequireSingleLayer("垂直翻转")) {
        return;
    }
    ApplyOperator("垂直翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Vertical, ctx.quad);
    });
}

void MainWindow::on_actionrotate90_triggered()
{
    if (!RequireSingleLayer("旋转")) {
        return;
    }
    ApplyOperator("顺时针旋转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.RotateImage(d, ctx.width, ctx.height, 90.0, ctx.quad);
    });
}

void MainWindow::on_actionrotate90ccw_triggered()
{
    if (!RequireSingleLayer("旋转")) {
        return;
    }
    ApplyOperator("逆时针旋转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.RotateImage(d, ctx.width, ctx.height, 270.0, ctx.quad);
    });
}

void MainWindow::on_actiontranspose_triggered()
{
    if (!RequireSingleLayer("转置")) {
        return;
    }
    ApplyOperator("沿主对角线镜像", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::Transpose, ctx.quad);
    });
}

void MainWindow::on_actionantitranspose_triggered()
{
    if (!RequireSingleLayer("反转置")) {
        return;
    }
    ApplyOperator("沿副对角线镜像", [](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.FlipImage(d, ctx.width, ctx.height, Function::FlipMode::AntiTranspose, ctx.quad);
    });
}

void MainWindow::on_actiontailor_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (!RequireSingleLayer("图像裁剪")) {
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    const QImage display = Function::ToDisplayImage(imageData,
                                                    myValue.bmpInfo.GetWidth(),
                                                    myValue.bmpInfo.GetHeight());
    CropDialog dlg(display, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QRect sel = dlg.selectionInImage();
    if (sel.width() < 4 || sel.height() < 4) {
        UpdateStatus("裁剪选区太小，已取消");
        return;
    }
    ApplyOperator("图像裁剪", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        Function f;
        f.CropImage(d, ctx.width, ctx.height,
                    sel.x(), sel.y(), sel.width(), sel.height(), ctx.quad);
    });
}

void MainWindow::on_actionequalize_triggered()
{
    ApplyOperator("直方图均衡化", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        uint8_t lut[256];
        this->function.BuildEqualizeLUT(d, lut);
        RunSegmented(ctx, [this, &d, &lut](size_t s, size_t e) {
            this->function.ApplyLUT(d, lut, s, e);
        });
    });
}

void MainWindow::on_actionerode_triggered()
{
    ApplyOperator("腐蚀", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Erode, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actiondilate_triggered()
{
    ApplyOperator("膨胀", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Dilate, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionopenMorph_triggered()
{
    ApplyOperator("开运算", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Open, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actioncloseMorph_triggered()
{
    ApplyOperator("闭运算", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  Function::MorphType::Close, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionconvolution_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty()) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    KernelDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const std::vector<double> kernel = dlg.kernel();
    const double scale = dlg.scale();
    const double offset = dlg.offset();
    ApplyOperator("自定义卷积", [this, kernel, scale, offset](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::vector<uint8_t> out;
        this->function.Convolve3x3(d, out, ctx.width, ctx.height,
                                   kernel.data(), scale, offset, 0, ctx.pixelCount);
        if (!out.empty()) {
            d.swap(out);
        }
    });
}

void MainWindow::on_actionusm_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty()) {
        function.CreateMessagebox("提示", "请先打开一张 BMP 图像");
        return;
    }
    USMDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const double radius = dlg.radius();
    const double amount = dlg.amount();
    const double threshold = dlg.threshold();
    ApplyOperator("USM 锐化", [this, radius, amount, threshold](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::vector<uint8_t> out;
        this->function.USMSharpen(d, out, ctx.width, ctx.height, radius, amount, threshold);
        if (!out.empty() && out.size() == d.size()) {
            d.swap(out);
        }
    });
}
