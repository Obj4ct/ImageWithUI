/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */


// MainWindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "theme.h"
#include "dialogs.h"
#include "slider_field.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QImageWriter>
#include <QSignalBlocker>
#include <QFont>
#include <QKeyEvent>
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScreen>
#include <QAction>
#include <QPainter>
#include <QClipboard>
#include <QColorDialog>
#include <QTransform>
#include <QLineF>
#include <QPolygonF>
#include <QFontMetrics>
#include <QSlider>
#include <QCheckBox>
#include <QtMath>
#include <algorithm>

namespace {

// ---- 调整算子工厂：提交路径与实时预览路径共用同一份实现 ----

OpFn MakeBrightnessOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        ImageOps f;
        f.Brightness(d, value);
    };
}

OpFn MakeContrastOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        ImageOps f;
        f.Contrast(d, value);
    };
}

OpFn MakeSaturationOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.Saturation(d, ctx.width, ctx.height, value);
    };
}

OpFn MakeShadowOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        ImageOps f;
        std::vector<uint8_t> out;
        f.MakeShadow(d, out, value);
        d.swap(out);
    };
}

OpFn MakeHighlightOp(double value)
{
    return [value](std::vector<uint8_t> &d, OpContext &) {
        ImageOps f;
        std::vector<uint8_t> out;
        f.HighLight(d, out, value);
        d.swap(out);
    };
}

OpFn MakeThresholdOp(double threshold)
{
    return [threshold](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.ApplyThreshold(d, threshold, 0, ctx.pixelCount);
    };
}

// 锐化：高斯模糊 → 高反差保留 → 锐化，程度值同时作为模糊半径
OpFn MakeSharpenOp(double amount)
{
    return [amount](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
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

// QImage(BGR888) -> 内部 BGR 自底向上紧凑缓冲
std::vector<uint8_t> BgrFromQImage(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }
    const QImage bgr = image.convertToFormat(QImage::Format_BGR888);
    const int32_t w = bgr.width();
    const int32_t h = bgr.height();
    if (w <= 0 || h <= 0) {
        return {};
    }
    std::vector<uint8_t> out(static_cast<size_t>(w) * static_cast<size_t>(h) * 3, 0);
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    const int srcStride = bgr.bytesPerLine();
    const uint8_t *bits = bgr.constBits();
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *src = bits + static_cast<ptrdiff_t>(y) * srcStride;
        // 内部缓冲为自底向上：源第 y 行(视觉自上而下)写到目标 h-1-y 行
        uint8_t *dst = out.data() + static_cast<size_t>(h - 1 - y) * rowBytes;
        std::memcpy(dst, src, rowBytes);
    }
    return out;
}

// 预览超过这个像素数就先缩小再算，避免大图拖动参数卡顿
constexpr size_t kPreviewMaxPixels = 2000000;

// 把一个图层按不透明度/混合模式叠到合成缓冲上(dst 初始为白，尺寸与图层一致)
// mask 非空且尺寸与画布一致时，按蒙版覆盖度进一步衰减该像素的混合强度：
// 蒙版 0 → 该像素完全不参与合成，255 → 与不透明度正常混合，中间值线性过渡。
void BlendLayerInto(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src,
                    double opacity, int blend, const SelectionMask *mask = nullptr)
{
    const size_t bytes = std::min(dst.size(), src.size());
    const int op = qBound(0, static_cast<int>(opacity * 255.0 + 0.5), 255);
    if (op == 0) {
        return;
    }
    const bool useMask = (mask != nullptr && !mask->isEmpty() &&
                          mask->width > 0 && mask->height > 0 &&
                          mask->data.size() ==
                              static_cast<size_t>(mask->width) * static_cast<size_t>(mask->height));
    if (!useMask) {
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
        return;
    }

    // 有蒙版：逐像素把不透明度再乘以覆盖度。
    // 蒙版按【视觉坐标】存储(y 向下)，内存缓冲是自底向上，所以视觉行 y 对应内存行 h-1-y。
    const int32_t w = mask->width;
    const int32_t h = mask->height;
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    if (bytes < rowBytes * static_cast<size_t>(h)) {
        return;
    }
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *mrow = mask->data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const size_t off = static_cast<size_t>(h - 1 - y) * rowBytes;
        for (int32_t x = 0; x < w; ++x) {
            const int a = mrow[x];
            if (a == 0) {
                continue;   // 完全隐藏在蒙版里，保留下层结果
            }
            const int eff = op * a / 255;
            if (eff <= 0) {
                continue;
            }
            const size_t i = off + static_cast<size_t>(x) * 3;
            for (int c = 0; c < 3; ++c) {
                const int d = dst[i + static_cast<size_t>(c)];
                const int s = src[i + static_cast<size_t>(c)];
                int blended = s;
                if (blend == 1) {
                    blended = d * s / 255;
                } else if (blend == 2) {
                    blended = 255 - (255 - d) * (255 - s) / 255;
                }
                dst[i + static_cast<size_t>(c)] =
                    static_cast<uint8_t>((blended * eff + d * (255 - eff)) / 255);
            }
        }
    }
}

// 按选区掩码把 src 与 dst 混合：掩码 0 → 用原像素，255 → 用算子结果，
// 中间值按覆盖度加权(羽化边缘因此能平滑过渡)。
// 掩码与图像尺寸不一致(例如旋转改变了画布)时视为没有选区。
void RestoreOutsideSelection(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src,
                             int32_t w, int32_t h, const SelectionMask &mask)
{
    if (w <= 0 || h <= 0 || mask.isEmpty() ||
        mask.width != w || mask.height != h ||
        mask.data.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) {
        return;
    }
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
    if (dst.size() < bytes || src.size() < bytes) {
        return;
    }
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    // 只处理掩码包围盒内的行，其余整行直接还原(大图 + 小选区时省掉大量拷贝)
    int32_t top = h;
    int32_t bottom = -1;
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *mrow = mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        bool any = false;
        for (int32_t x = 0; x < w; ++x) {
            if (mrow[x] != 0) {
                any = true;
                break;
            }
        }
        if (any) {
            if (top == h) {
                top = y;
            }
            bottom = y;
        }
    }
    if (bottom < top) {
        return;
    }
    for (int32_t y = 0; y < h; ++y) {
        // 视觉行 y 对应内存行 h-1-y(内存第 0 行是画面最底行)
        const size_t off = static_cast<size_t>(h - 1 - y) * rowBytes;
        if (y < top || y > bottom) {
            std::copy_n(src.begin() + static_cast<ptrdiff_t>(off), rowBytes,
                        dst.begin() + static_cast<ptrdiff_t>(off));
            continue;
        }
        const uint8_t *mrow = mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int32_t x = 0; x < w; ++x) {
            const int a = mrow[x];
            if (a == 0) {
                std::copy_n(src.begin() + static_cast<ptrdiff_t>(off) + static_cast<size_t>(x) * 3, 3,
                            dst.begin() + static_cast<ptrdiff_t>(off) + static_cast<size_t>(x) * 3);
            } else if (a == 255) {
                continue;
            } else {
                for (int c = 0; c < 3; ++c) {
                    const size_t i = off + static_cast<size_t>(x) * 3 + static_cast<size_t>(c);
                    dst[i] = static_cast<uint8_t>((static_cast<int>(dst[i]) * a +
                                                   static_cast<int>(src[i]) * (255 - a)) / 255);
                }
            }
        }
    }
}


// 对掩码做一次可分离盒式模糊(先水平后垂直，边缘按复制处理)，用于羽化
void BoxBlurMask(std::vector<uint8_t> &data, int32_t w, int32_t h, int32_t r)
{
    if (r < 1 || w <= 0 || h <= 0 ||
        data.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) {
        return;
    }
    const int32_t win = 2 * r + 1;
    std::vector<uint8_t> tmp(data.size());
    // 水平
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *src = data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        uint8_t *dst = tmp.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        int sum = 0;
        for (int32_t k = -r; k <= r; ++k) {
            sum += src[qBound(0, k, w - 1)];
        }
        for (int32_t x = 0; x < w; ++x) {
            dst[x] = static_cast<uint8_t>(sum / win);
            sum -= src[qBound(0, x - r, w - 1)];
            sum += src[qBound(0, x + r + 1, w - 1)];
        }
    }
    // 垂直
    for (int32_t x = 0; x < w; ++x) {
        int sum = 0;
        for (int32_t k = -r; k <= r; ++k) {
            sum += tmp[static_cast<size_t>(qBound(0, k, h - 1)) * static_cast<size_t>(w)
                       + static_cast<size_t>(x)];
        }
        for (int32_t y = 0; y < h; ++y) {
            data[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                static_cast<uint8_t>(sum / win);
            sum -= tmp[static_cast<size_t>(qBound(0, y - r, h - 1)) * static_cast<size_t>(w)
                       + static_cast<size_t>(x)];
            sum += tmp[static_cast<size_t>(qBound(0, y + r + 1, h - 1)) * static_cast<size_t>(w)
                       + static_cast<size_t>(x)];
        }
    }
}

// 平移掩码：超出画布的部分丢弃(不做环绕)
SelectionMask TranslateMask(const SelectionMask &mask, int dx, int dy)
{
    SelectionMask out;
    if (mask.isEmpty()) {
        return out;
    }
    const int32_t w = mask.width;
    const int32_t h = mask.height;
    out.width = w;
    out.height = h;
    out.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int32_t y = 0; y < h; ++y) {
        const int32_t sy = y - dy;
        if (sy < 0 || sy >= h) {
            continue;
        }
        const uint8_t *src = mask.data.data() + static_cast<size_t>(sy) * static_cast<size_t>(w);
        uint8_t *dst = out.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const int32_t x0 = std::max(0, -dx);
        const int32_t x1 = std::min(w, w - dx);
        for (int32_t x = x0; x < x1; ++x) {
            dst[x + dx] = src[x];
        }
    }
    return out;
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
            a[i].maskEnabled != b[i].maskEnabled || a[i].mask.width != b[i].mask.width ||
            a[i].mask.height != b[i].mask.height || a[i].mask.data != b[i].mask.data ||
            a[i].data != b[i].data) {
            return false;
        }
    }
    return true;
}

// 两份快照是否完全一致(用于全局历史去重)
bool SameSnapshot(const ImageSnapshot &a, const ImageSnapshot &b)
{
    return a.data == b.data && a.width == b.width && a.height == b.height &&
           a.activeLayer == b.activeLayer && SameLayerStack(a.layers, b.layers);
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

    // 同步主窗口的掩码：preview 为半透明缩略图(空表示无选区)，bbox 为掩码包围盒
    void setMask(const QImage &preview, const QRect &bbox)
    {
        m_maskImage = preview;
        setSelection(bbox);
        update();
    }

    // 切换工具类型：0 矩形选框 / 1 自由套索 / 2 多边形套索 / 3 磁性套索
    void setToolKind(int kind)
    {
        m_tool = qBound(0, kind, 3);
        m_drag = None;
        m_points.clear();
        m_poly.clear();
        m_hoverValid = false;
        update();
    }

    void setToolActive(bool active)
    {
        if (!active && m_transform) {
            endTransform();
        }
        m_toolActive = active;
        m_drag = None;
        m_points.clear();
        m_poly.clear();
        m_hoverValid = false;
        m_gradientDragging = false;
        setAttribute(Qt::WA_TransparentForMouseEvents, !active);
        setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
        if (active) {
            setFocus();   // 多边形套索要靠键盘 Enter 闭合、Esc 取消
        }
        update();
    }

    // ---- 画笔/橡皮：0 关闭 / 1 画笔 / 2 橡皮 ----
    void setBrushMode(int mode)
    {
        m_brushMode = qBound(0, mode, 2);
        m_brushDrawing = false;
        m_brushPoints.clear();
        m_drag = None;
        m_hoverValid = false;
        update();
    }

    void setBrushParams(int size, int hardness, double opacity, const QColor &color)
    {
        m_brushSize = qBound(1, size, 200);
        m_brushHardness = qBound(0, hardness, 100);
        m_brushOpacity = qBound(0.0, opacity, 1.0);
        m_brushColor = color;
        update();
    }

    int brushMode() const { return m_brushMode; }

    // ---- 渐变工具：拖拽定义 from→to，松开时提交 ----
    void setGradientMode(bool active)
    {
        m_gradientMode = active;
        m_gradientDragging = false;
        m_drag = None;
        update();
    }

    bool gradientMode() const { return m_gradientMode; }

    void beginTransform(const QRect &baseRect, const QImage &whiteFill, const QImage &content)
    {
        if (baseRect.isEmpty() || whiteFill.isNull() || content.isNull()) {
            return;
        }
        m_transform = true;
        m_transformBase = QRectF(baseRect);
        m_transformWhite = whiteFill;
        m_transformContent = content;
        m_sx = m_sy = 1.0;
        m_angle = 0.0;
        m_drag = None;
        m_handle = -1;
        setCursor(Qt::ArrowCursor);
        update();
    }

    void endTransform()
    {
        m_transform = false;
        m_drag = None;
        m_handle = -1;
        m_transformWhite = QImage();
        m_transformContent = QImage();
        update();
    }

    bool isTransformActive() const { return m_transform; }

    // 用户拖拽建立选区后回调(add = 按住 Shift，表示并入已有选区)
    std::function<void(const QRect &, bool)> onSelectionChanged;
    // 画笔笔画：按下、拖动(上一采样点, 当前点)、松开；坐标均为图像像素
    std::function<void(const QPoint &)> onBrushStart;
    std::function<void(const QPoint &, const QPoint &)> onBrushSegment;
    std::function<void()> onBrushEnd;
    // 渐变拖拽结束：参数为图像像素坐标的起点与终点
    std::function<void(const QPoint &, const QPoint &)> onGradientFinished;
    // 用户拖动整体移动选区后回调(图像像素位移；content=true 表示同时搬动像素)
    std::function<void(int, int, bool)> onSelectionMoved;
    // 套索松开后回调(add = Shift 加选，subtract = Alt 减选)
    std::function<void(const QPolygon &, bool, bool)> onLassoFinished;
    // 图像边缘强度查询(供磁性套索吸附，返回 0 表示不是边缘)
    std::function<int(int, int)> edgeAt;
    // 变换拖动中回调(缩放 x、缩放 y、旋转角度)，供状态栏实时提示
    std::function<void(double, double, double)> onTransformChanged;
    // 变换结束回调(commit=true 提交；否则取消)，参数为最终变换值
    std::function<void(bool, double, double, double)> onTransformFinished;

protected:
    // 磁性吸附：把点吸到半径内梯度最强的像素
    QPoint snapPoint(const QPoint &p) const
    {
        return MainWindow::SnapSelectionEdge(edgeAt, p, 8);
    }

    // 多边形套索闭合：交给主窗口栅格化成掩码
    void finishPolygon()
    {
        const QPolygon poly = m_poly;
        m_poly.clear();
        m_hoverValid = false;
        if (poly.size() >= 3 && onLassoFinished) {
            onLassoFinished(poly, m_addMode, m_subtractMode);
        }
        update();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (m_transform) {
            const int key = event->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                if (onTransformFinished) {
                    onTransformFinished(true, m_sx, m_sy, m_angle);
                }
                return;
            }
            if (key == Qt::Key_Escape) {
                if (onTransformFinished) {
                    onTransformFinished(false, 1.0, 1.0, 0.0);
                }
                return;
            }
        }
        if (m_tool == 2 && !m_poly.isEmpty()) {
            const int key = event->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                finishPolygon();
                return;
            }
            if (key == Qt::Key_Escape) {
                m_poly.clear();
                m_hoverValid = false;
                update();
                return;
            }
            if (key == Qt::Key_Backspace) {
                m_poly.removeLast();
                update();
                return;
            }
        }
        QWidget::keyPressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        Q_UNUSED(event);
        if (m_transform) {
            if (onTransformFinished) {
                onTransformFinished(true, m_sx, m_sy, m_angle);
            }
            return;
        }
        if (m_tool == 2 && m_poly.size() >= 3) {
            finishPolygon();
        }
    }

    // 黑白虚线轨迹(套索/多边形套索共用)
    void drawTrail(QPainter &p, const QPolygon &imagePoints) const
    {
        QPolygon trail;
        for (const QPoint &ip : imagePoints) {
            trail << toWidgetPoint(ip);
        }
        if (trail.size() < 2) {
            return;
        }
        QPen dark(QColor(0, 0, 0, 210));
        dark.setStyle(Qt::CustomDashLine);
        dark.setDashPattern(QVector<qreal>{4, 4});
        dark.setDashOffset(m_dashOffset);
        p.setPen(dark);
        p.drawPolyline(trail);

        QPen light(QColor(255, 255, 255, 230));
        light.setStyle(Qt::CustomDashLine);
        light.setDashPattern(QVector<qreal>{4, 4});
        light.setDashOffset(m_dashOffset + 4);
        p.setPen(light);
        p.drawPolyline(trail);
    }

    // 画笔预览：用圆头折线近似"胶囊"笔迹，并在笔尖画一圈定位环
    void drawBrushPreview(QPainter &p) const
    {
        if (m_brushPoints.isEmpty()) {
            return;
        }
        QPolygonF trail;
        for (const QPoint &ip : m_brushPoints) {
            trail << QPointF(toWidgetPoint(ip));
        }
        QColor color = (m_brushMode == 2) ? QColor(255, 255, 255) : m_brushColor;
        color.setAlphaF(m_brushOpacity);
        const QRect dr = displayRect();
        const double sx = (dr.isEmpty() || m_imgW <= 0) ? 1.0
                                                        : static_cast<double>(dr.width()) / m_imgW;
        const double sy = (dr.isEmpty() || m_imgH <= 0) ? 1.0
                                                        : static_cast<double>(dr.height()) / m_imgH;
        const double penWidth = std::max(1.0, m_brushSize * (sx + sy) / 2.0);
        QPen pen(color);
        pen.setWidthF(penWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        if (trail.size() == 1) {
            p.drawEllipse(trail.first(), penWidth / 2.0, penWidth / 2.0);
        } else {
            p.drawPolyline(trail);
        }

        // 笔尖定位环(始终可见，便于控制大小)
        const QPointF tip = trail.last();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(34, 211, 238, 200), 1));
        p.drawEllipse(tip, std::max(1.5, m_brushSize * sx / 2.0),
                      std::max(1.5, m_brushSize * sy / 2.0));
    }

    // 渐变拖拽预览：起止点之间的虚线 + 两端标记
    void drawGradientPreview(QPainter &p) const
    {
        if (!m_gradientMode || !m_gradientDragging) {
            return;
        }
        const QPoint a = toWidgetPoint(m_gradientStart);
        const QPoint b = toWidgetPoint(m_gradientEnd);
        QPen dark(QColor(0, 0, 0, 210));
        dark.setStyle(Qt::CustomDashLine);
        dark.setDashPattern(QVector<qreal>{4, 4});
        dark.setDashOffset(m_dashOffset);
        p.setPen(dark);
        p.drawLine(a, b);

        QPen light(QColor(255, 255, 255, 230));
        light.setStyle(Qt::CustomDashLine);
        light.setDashPattern(QVector<qreal>{4, 4});
        light.setDashOffset(m_dashOffset + 4);
        p.setPen(light);
        p.drawLine(a, b);

        p.setPen(QPen(QColor(34, 211, 238), 1));
        p.setBrush(QColor(34, 211, 238));
        p.drawEllipse(a, 3, 3);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(b, 5, 5);
    }

    void paintEvent(QPaintEvent *) override
    {
        if (m_imgW <= 0 || m_imgH <= 0) {
            return;
        }
        QPainter p(this);

        // 变换会话：画"原位置刷白 + 变换后的内容 + 旋转后的手柄"
        if (m_transform) {
            paintTransform(p);
            return;
        }

        // 画笔/橡皮：笔迹预览画在选区蚂蚁线之下
        if (m_brushMode != 0) {
            drawBrushPreview(p);
        }
        // 渐变拖拽：显示起止方向线
        drawGradientPreview(p);

        // 多边形套索正在取点：画已确定的边 + 到光标的橡皮筋
        if (m_tool == 2 && !m_poly.isEmpty()) {
            QPolygon trail = m_poly;
            if (m_hoverValid) {
                trail << m_hover;
            }
            drawTrail(p, trail);
            p.setPen(QPen(QColor(34, 211, 238), 1));
            p.setBrush(QColor(34, 211, 238));
            for (const QPoint &ip : m_poly) {
                const QPoint w = toWidgetPoint(ip);
                p.drawRect(QRect(w.x() - 2, w.y() - 2, 4, 4));
            }
            // 悬停回首点：提示可以闭合
            const QPoint first = toWidgetPoint(m_poly.first());
            if (m_poly.size() >= 3 && m_hoverValid &&
                (toWidgetPoint(m_hover) - first).manhattanLength() <= 8) {
                p.setPen(QPen(QColor(255, 255, 255), 2));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(first, 5, 5);
            }
            return;
        }

        if (m_sel.isEmpty()) {
            return;
        }
        const QRect wr = toWidget(m_sel);
        if (wr.isEmpty()) {
            return;
        }

        // 套索/磁性套索正在绘制：只显示轨迹，不显示旧掩码与矩形框
        if ((m_tool == 1 || m_tool == 3) && m_drag == Creating) {
            drawTrail(p, m_points);
            return;
        }
        // 掩码高亮：把半透明缩略图拉伸到图像显示区域；
        // 拖动移动选区时按像素位移同步平移，建立新选区时先隐藏旧掩码
        if (!m_maskImage.isNull() && m_drag != Creating) {
            QRect target = displayRect();
            if (!target.isEmpty()) {
                if (m_drag == Moving && !m_startSel.isEmpty()) {
                    const double sx = static_cast<double>(target.width()) / std::max(1, m_imgW);
                    const double sy = static_cast<double>(target.height()) / std::max(1, m_imgH);
                    target.translate(qRound((m_sel.x() - m_startSel.x()) * sx),
                                     qRound((m_sel.y() - m_startSel.y()) * sy));
                }
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                p.drawImage(target, m_maskImage);
            }
        }

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
        if (event->button() != Qt::LeftButton) {
            return;
        }
        if (m_transform) {
            const int handle = hitHandle(event->pos());
            if (handle < 0) {
                return;   // 变换模式下只响应手柄
            }
            m_handle = handle;
            m_startSx = m_sx;
            m_startSy = m_sy;
            m_startAngle = m_angle;
            m_drag = (handle == 8) ? TransformRotate : TransformScale;
            const QPointF center(m_transformBase.x() + m_transformBase.width() / 2.0,
                                 m_transformBase.y() + m_transformBase.height() / 2.0);
            const QPointF ip = toImageF(event->pos());
            m_startPointerAngle = qRadiansToDegrees(qAtan2(ip.y() - center.y(), ip.x() - center.x()));
            setCursor(cursorForHandle(handle));
            update();
            return;
        }
        if (m_gradientMode && m_imgW > 0 && m_imgH > 0) {
            const QRect dr = displayRect();
            if (dr.isEmpty() || !dr.adjusted(-2, -2, 2, 2).contains(event->pos())) {
                return;
            }
            QPoint ip = toImage(event->pos());
            ip.setX(qBound(0, ip.x(), m_imgW - 1));
            ip.setY(qBound(0, ip.y(), m_imgH - 1));
            m_gradientStart = ip;
            m_gradientEnd = ip;
            m_gradientDragging = true;
            update();
            return;
        }
        if (m_brushMode != 0 && m_imgW > 0 && m_imgH > 0) {
            const QRect dr = displayRect();
            if (dr.isEmpty() || !dr.adjusted(-2, -2, 2, 2).contains(event->pos())) {
                return;
            }
            QPoint ip = toImage(event->pos());
            ip.setX(qBound(0, ip.x(), m_imgW - 1));
            ip.setY(qBound(0, ip.y(), m_imgH - 1));
            m_brushDrawing = true;
            m_brushPoints.clear();
            m_brushPoints.append(ip);
            if (onBrushStart) {
                onBrushStart(ip);
            }
            update();
            return;
        }
        if (!m_toolActive || m_imgW <= 0 || m_imgH <= 0) {
            return;
        }
        const QRect dr = displayRect();
        if (dr.isEmpty() || !dr.adjusted(-2, -2, 2, 2).contains(event->pos())) {
            return;   // 点在图像外的画布空区域：不建立选区
        }
        const bool insideSel = !m_sel.isEmpty() && toWidget(m_sel).contains(event->pos());
        // Ctrl + 在选区内按下：同时搬动选区内像素(任何工具下都可用)
        if (insideSel && (event->modifiers() & Qt::ControlModifier)) {
            m_drag = Moving;
            m_pressPos = event->pos();
            m_startSel = m_sel;
            m_addMode = false;
            m_subtractMode = false;
            m_moveContent = true;
            setCursor(Qt::SizeAllCursor);
            return;
        }
        m_moveContent = false;
        if (m_tool == 2) {   // 多边形套索：单击落点；点回首点/双击/Enter 闭合
            setFocus();
            const QPoint ip = toImage(event->pos());
            if (m_poly.isEmpty()) {
                m_addMode = (event->modifiers() & Qt::ShiftModifier);
                m_subtractMode = (event->modifiers() & Qt::AltModifier);
            } else if (m_poly.size() >= 3 &&
                       (ip - m_poly.first()).manhattanLength() <= 6) {
                finishPolygon();
                return;
            } else if ((ip - m_poly.last()).manhattanLength() <= 1) {
                return;   // 重复落点，忽略
            }
            m_poly.append(ip);
            m_hover = ip;
            m_hoverValid = true;
            update();
            return;
        }
        if (m_tool == 1 || m_tool == 3) {   // 自由/磁性套索：按下开始记录轨迹
            m_drag = Creating;
            m_points.clear();
            QPoint ip = toImage(event->pos());
            if (m_tool == 3) {
                ip = snapPoint(ip);   // 磁性吸附到附近强边缘
            }
            m_points.append(ip);
            m_addMode = (event->modifiers() & Qt::ShiftModifier);
            m_subtractMode = (event->modifiers() & Qt::AltModifier);
            m_sel = QRect(ip, QSize(1, 1));
            update();
            return;
        }
        if (!m_sel.isEmpty() && toWidget(m_sel).contains(event->pos())) {
            m_drag = Moving;   // 在选区内按下 = 整体移动选区
            m_pressPos = event->pos();
            m_startSel = m_sel;
            m_addMode = false;
            setCursor(Qt::SizeAllCursor);
        } else {
            m_drag = Creating;
            m_anchor = toImage(event->pos());
            m_addMode = (event->modifiers() & Qt::ShiftModifier);   // Shift = 加选
            setSelection(QRect(m_anchor, QSize(1, 1)));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPoint pos = event->pos();
        if (m_transform) {
            if (m_drag == None) {
                const int handle = hitHandle(pos);
                setCursor(cursorForHandle(handle));
                return;
            }
            const QPointF center(m_transformBase.x() + m_transformBase.width() / 2.0,
                                 m_transformBase.y() + m_transformBase.height() / 2.0);
            const QPointF ip = toImageF(pos);
            if (m_drag == TransformRotate) {
                const double pointer = qRadiansToDegrees(qAtan2(ip.y() - center.y(), ip.x() - center.x()));
                double angle = m_startAngle + (pointer - m_startPointerAngle);
                while (angle > 180.0) angle -= 360.0;
                while (angle <= -180.0) angle += 360.0;
                if (event->modifiers() & Qt::ShiftModifier) {
                    angle = std::round(angle / 15.0) * 15.0;
                }
                m_angle = angle;
                updateTransformStatus();
                update();
                return;
            }
            if (m_drag == TransformScale) {
                // 先把鼠标点逆旋转回"无旋转"坐标系，再按手柄基准偏移求两轴缩放
                const double rad = qDegreesToRadians(m_angle);
                const double cosA = std::cos(rad);
                const double sinA = std::sin(rad);
                const QPointF dMouse(ip.x() - center.x(), ip.y() - center.y());
                const QPointF local(dMouse.x() * cosA + dMouse.y() * sinA,
                                    -dMouse.x() * sinA + dMouse.y() * cosA);
                const QPointF d = baseHandle(m_handle) - center;
                double sx = (std::abs(d.x()) > 0.5) ? local.x() / d.x() : m_startSx;
                double sy = (std::abs(d.y()) > 0.5) ? local.y() / d.y() : m_startSy;
                const bool corner = (m_handle == 0 || m_handle == 2 || m_handle == 4 || m_handle == 6);
                if (event->modifiers() & Qt::ShiftModifier) {
                    if (corner) {
                        const double f = QLineF(QPointF(0, 0), local).length() /
                                         std::max(1e-6, QLineF(QPointF(0, 0), d).length());
                        sx = sy = f;
                    } else if (std::abs(d.x()) > 0.5) {
                        sy = sx;   // 边中点：把变化的轴同步到另一轴，保持等比
                    } else {
                        sx = sy;
                    }
                }
                m_sx = qBound(0.02, std::abs(sx), 20.0);
                m_sy = qBound(0.02, std::abs(sy), 20.0);
                updateTransformStatus();
                update();
                return;
            }
            return;
        }
        if (m_gradientMode) {
            if (m_imgW <= 0 || m_imgH <= 0) {
                return;
            }
            if (!m_gradientDragging) {
                setCursor(Qt::CrossCursor);
                return;
            }
            if (!(event->buttons() & Qt::LeftButton)) {
                return;
            }
            QPoint ip = toImage(pos);
            ip.setX(qBound(0, ip.x(), m_imgW - 1));
            ip.setY(qBound(0, ip.y(), m_imgH - 1));
            m_gradientEnd = ip;
            update();
            return;
        }
        if (m_brushMode != 0) {
            if (!m_brushDrawing) {
                setCursor(Qt::CrossCursor);
                return;
            }
            if (!(event->buttons() & Qt::LeftButton)) {
                return;
            }
            QPoint ip = toImage(pos);
            ip.setX(qBound(0, ip.x(), m_imgW - 1));
            ip.setY(qBound(0, ip.y(), m_imgH - 1));
            if (!m_brushPoints.isEmpty() && m_brushPoints.last() == ip) {
                return;
            }
            if (!m_brushPoints.isEmpty() && onBrushSegment) {
                onBrushSegment(m_brushPoints.last(), ip);
            }
            m_brushPoints.append(ip);
            update();
            return;
        }
        if (!m_toolActive) {
            return;
        }
        if (m_drag == None) {
            if (m_tool == 2 && !m_poly.isEmpty()) {
                m_hover = toImage(pos);
                m_hoverValid = true;
                update();
                return;
            }
            setCursor((!m_sel.isEmpty() && toWidget(m_sel).contains(pos)) ? Qt::SizeAllCursor
                                                                          : Qt::CrossCursor);
            return;
        }
        if (m_drag == Creating && (m_tool == 1 || m_tool == 3)) {
            QPoint ip = toImage(pos);
            if (m_tool == 3) {
                ip = snapPoint(ip);
            }
            if (!m_points.isEmpty() && (m_points.last() - ip).manhattanLength() < 2) {
                return;   // 轨迹点太密，忽略
            }
            m_points.append(ip);
            QRect bb = m_points.boundingRect();
            m_sel = bb.isValid() ? bb : QRect(ip, QSize(1, 1));
            update();
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
        if (m_gradientMode) {
            if (m_gradientDragging && event->button() == Qt::LeftButton && m_imgW > 0 && m_imgH > 0) {
                QPoint ip = toImage(event->pos());
                ip.setX(qBound(0, ip.x(), std::max(0, m_imgW - 1)));
                ip.setY(qBound(0, ip.y(), std::max(0, m_imgH - 1)));
                m_gradientEnd = ip;
                m_gradientDragging = false;
                const QPoint from = m_gradientStart;
                const QPoint to = m_gradientEnd;
                update();
                if (onGradientFinished) {
                    onGradientFinished(from, to);
                }
            }
            return;
        }
        if (m_brushMode != 0) {
            if (m_brushDrawing) {
                QPoint ip = toImage(event->pos());
                ip.setX(qBound(0, ip.x(), std::max(0, m_imgW - 1)));
                ip.setY(qBound(0, ip.y(), std::max(0, m_imgH - 1)));
                if (!m_brushPoints.isEmpty() && m_brushPoints.last() != ip &&
                    onBrushSegment) {
                    onBrushSegment(m_brushPoints.last(), ip);
                }
                m_brushDrawing = false;
                if (onBrushEnd) {
                    onBrushEnd();
                }
            }
            m_brushPoints.clear();
            update();
            return;
        }
        if (m_transform) {
            if (m_drag == TransformScale || m_drag == TransformRotate) {
                m_drag = None;
                m_handle = -1;
                setCursor(Qt::ArrowCursor);
                update();
            }
            return;   // 变换不因松开鼠标而提交，等 Enter / 双击
        }
        if (m_drag == None) {
            return;
        }
        const bool wasCreating = (m_drag == Creating);
        const bool wasMoving = (m_drag == Moving);
        m_drag = None;
        setCursor(Qt::CrossCursor);

        if (wasCreating && (m_tool == 1 || m_tool == 3)) {
            const QPolygon poly = m_points;
            m_points.clear();
            if (poly.size() >= 3 && onLassoFinished) {
                onLassoFinished(poly, m_addMode, m_subtractMode);
            } else if (onSelectionChanged) {
                onSelectionChanged(QRect(), false);
            }
            return;
        }

        if (wasCreating && (m_sel.width() < 3 || m_sel.height() < 3)) {
            setSelection(QRect());   // 单击图像视为取消选区
            if (onSelectionChanged) {
                onSelectionChanged(QRect(), false);
            }
            return;
        }
        if (wasMoving) {
            const int dx = m_sel.x() - m_startSel.x();
            const int dy = m_sel.y() - m_startSel.y();
            if ((dx != 0 || dy != 0) && onSelectionMoved) {
                onSelectionMoved(dx, dy, m_moveContent);
            }
            return;
        }
        if (onSelectionChanged) {
            onSelectionChanged(m_sel, m_addMode);
        }
    }

private:
    enum DragMode { None, Creating, Moving, TransformScale, TransformRotate };

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

    QPoint toWidgetPoint(const QPoint &p) const
    {
        const QRect dr = displayRect();
        if (dr.isEmpty()) {
            return QPoint();
        }
        const double sx = static_cast<double>(dr.width()) / std::max(1, m_imgW);
        const double sy = static_cast<double>(dr.height()) / std::max(1, m_imgH);
        return QPoint(dr.left() + qRound(p.x() * sx), dr.top() + qRound(p.y() * sy));
    }

    // ---- 交互式变换的坐标/手柄工具 ----
    // 连续坐标映射：与 toWidget/toImage 同一套 displayRect() 口径
    QPointF toWidgetF(const QPointF &p) const
    {
        const QRect dr = displayRect();
        if (dr.isEmpty()) {
            return QPointF();
        }
        const double sx = static_cast<double>(dr.width()) / std::max(1, m_imgW);
        const double sy = static_cast<double>(dr.height()) / std::max(1, m_imgH);
        return QPointF(dr.left() + p.x() * sx, dr.top() + p.y() * sy);
    }

    QPointF toImageF(const QPointF &p) const
    {
        const QRect dr = displayRect();
        if (dr.isEmpty()) {
            return QPointF();
        }
        const double sx = static_cast<double>(dr.width()) / std::max(1, m_imgW);
        const double sy = static_cast<double>(dr.height()) / std::max(1, m_imgH);
        return QPointF((p.x() - dr.left()) / sx, (p.y() - dr.top()) / sy);
    }

    // 基准矩形上的手柄点(图像坐标)：0..7 缩放，8 视同 1(上中点)
    QPointF baseHandle(int index) const
    {
        const QRectF r = m_transformBase;
        const double cx = r.x() + r.width() / 2.0;
        const double cy = r.y() + r.height() / 2.0;
        switch (index) {
        case 0: return QPointF(r.left(), r.top());
        case 1: return QPointF(cx, r.top());
        case 2: return QPointF(r.right(), r.top());
        case 3: return QPointF(r.right(), cy);
        case 4: return QPointF(r.right(), r.bottom());
        case 5: return QPointF(cx, r.bottom());
        case 6: return QPointF(r.left(), r.bottom());
        case 7: return QPointF(r.left(), cy);
        default: return QPointF(cx, r.top());
        }
    }

    // P' = C + Rθ( S(sx, sy) · (P − C) )，与提交内核的 QTransform 顺序一致
    QPointF transformPoint(const QPointF &p) const
    {
        const QRectF r = m_transformBase;
        const double cx = r.x() + r.width() / 2.0;
        const double cy = r.y() + r.height() / 2.0;
        const double rad = qDegreesToRadians(m_angle);
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        const QPointF d((p.x() - cx) * m_sx, (p.y() - cy) * m_sy);
        return QPointF(cx + d.x() * c - d.y() * s, cy + d.x() * s + d.y() * c);
    }

    // 变换后的手柄位置；旋转手柄从上中点沿旋转方向外移 28 个控件像素
    QPointF handlePoint(int index) const
    {
        if (index != 8) {
            return transformPoint(baseHandle(index));
        }
        const QRect dr = displayRect();
        const double scale = (dr.isEmpty() || m_imgH <= 0)
                                 ? 1.0
                                 : static_cast<double>(dr.height()) / m_imgH;
        const double rad = qDegreesToRadians(m_angle);
        const QPointF top = transformPoint(baseHandle(1));
        return top + QPointF(28.0 * std::sin(rad) / std::max(0.01, scale),
                             -28.0 * std::cos(rad) / std::max(0.01, scale));
    }

    // 命中测试(控件坐标)：先测旋转手柄，再测 8 个缩放手柄
    int hitHandle(const QPoint &widgetPos) const
    {
        const QPointF pos(widgetPos);
        for (int i = 8; i >= 0; --i) {
            const QPointF w = toWidgetF(handlePoint(i));
            if (QLineF(w, pos).length() <= 9.0) {
                return i;
            }
        }
        return -1;
    }

    static Qt::CursorShape cursorForHandle(int handle)
    {
        switch (handle) {
        case 8: return Qt::OpenHandCursor;
        case 1: case 5: return Qt::SizeVerCursor;
        case 3: case 7: return Qt::SizeHorCursor;
        case 0: case 4: return Qt::SizeFDiagCursor;
        case 2: case 6: return Qt::SizeBDiagCursor;
        default: return Qt::ArrowCursor;
        }
    }

    void updateTransformStatus()
    {
        if (onTransformChanged) {
            onTransformChanged(m_sx, m_sy, m_angle);
        }
    }

    // 实时预览：原位置刷白 → 变换后的内容 → 旋转后的轮廓与手柄
    void paintTransform(QPainter &p) const
    {
        if (m_transformWhite.isNull() || m_transformContent.isNull()) {
            return;
        }
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QPointF tl = toWidgetF(m_transformBase.topLeft());
        const QPointF br = toWidgetF(m_transformBase.bottomRight());
        const QRectF baseWidget(tl, br);
        if (baseWidget.isEmpty()) {
            return;
        }

        // 1) 原选区按覆盖度刷白(alpha = 掩码覆盖度)
        p.drawImage(baseWidget, m_transformWhite);

        // 2) 变换后的内容：缩放 → 旋转，锚点为选区中心
        const QPointF center = toWidgetF(QPointF(m_transformBase.x() + m_transformBase.width() / 2.0,
                                                 m_transformBase.y() + m_transformBase.height() / 2.0));
        p.save();
        p.translate(center);
        p.rotate(m_angle);
        p.scale(m_sx, m_sy);
        p.translate(-center);
        p.drawImage(baseWidget, m_transformContent);
        p.restore();

        // 3) 旋转后的虚线轮廓
        QPolygonF outline;
        outline << toWidgetF(transformPoint(baseHandle(0)))
                << toWidgetF(transformPoint(baseHandle(2)))
                << toWidgetF(transformPoint(baseHandle(4)))
                << toWidgetF(transformPoint(baseHandle(6)));
        QPen dark(QColor(0, 0, 0, 210));
        dark.setStyle(Qt::CustomDashLine);
        dark.setDashPattern(QVector<qreal>{4, 4});
        dark.setDashOffset(m_dashOffset);
        p.setPen(dark);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(outline);

        QPen light(QColor(255, 255, 255, 230));
        light.setStyle(Qt::CustomDashLine);
        light.setDashPattern(QVector<qreal>{4, 4});
        light.setDashOffset(m_dashOffset + 4);
        p.setPen(light);
        p.drawPolygon(outline);

        // 4) 旋转手柄连接线 + 圆点
        const QPointF topMid = toWidgetF(transformPoint(baseHandle(1)));
        const QPointF rot = toWidgetF(handlePoint(8));
        p.setPen(QPen(QColor(34, 211, 238), 1));
        p.drawLine(topMid, rot);

        // 5) 8 个缩放手柄
        p.setBrush(QColor(34, 211, 238));
        for (int i = 0; i < 8; ++i) {
            const QPointF w = toWidgetF(handlePoint(i));
            p.drawRect(QRectF(w.x() - 3.5, w.y() - 3.5, 7.0, 7.0));
        }
        p.setBrush(QColor(9, 13, 18));
        p.drawEllipse(rot, 5, 5);
        p.setBrush(QColor(34, 211, 238));
        p.drawEllipse(rot, 3, 3);

        // 6) 参数提示(放在轮廓包围盒上方；贴顶时放到下方)
        QString text;
        if (std::abs(m_sx - m_sy) < 1e-6) {
            text = QString("缩放 %1%").arg(m_sx * 100.0, 0, 'f', 0);
        } else {
            text = QString("缩放 %1% × %2%")
                       .arg(m_sx * 100.0, 0, 'f', 0)
                       .arg(m_sy * 100.0, 0, 'f', 0);
        }
        if (std::abs(m_angle) >= 0.05) {
            text += QString(" · 旋转 %1°").arg(m_angle, 0, 'f', 1);
        }
        QFont f = font();
        f.setPointSize(9);
        p.setFont(f);
        const QFontMetrics fm(f);
        const QRectF bounds = outline.boundingRect();
        const int tw = fm.boundingRect(text).width() + 10;
        const int th = fm.height() + 4;
        QRectF box(bounds.left() + (bounds.width() - tw) / 2.0, bounds.top() - th - 8, tw, th);
        if (box.top() < 0) {
            box.moveTop(bounds.bottom() + 8);
        }
        if (box.right() > width() - 2) {
            box.moveRight(width() - 2);
        }
        if (box.left() < 2) {
            box.moveLeft(2);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(9, 13, 18, 220));
        p.drawRoundedRect(box, 4, 4);
        p.setPen(QColor(103, 232, 249));
        p.drawText(box, Qt::AlignCenter, text);
    }

    QTimer *m_anim = nullptr;
    int m_dashOffset = 0;
    bool m_toolActive = false;
    int32_t m_imgW = 0;
    int32_t m_imgH = 0;
    QRect m_sel;                 // 选区(图像像素坐标)
    QImage m_maskImage;          // 掩码半透明缩略图(由主窗口生成)
    DragMode m_drag = None;
    int m_tool = 0;              // 0 矩形 / 1 自由套索 / 2 多边形套索 / 3 磁性套索
    // ---- 画笔/橡皮 ----
    int m_brushMode = 0;           // 0 关闭 / 1 画笔 / 2 橡皮
    bool m_brushDrawing = false;
    QPolygon m_brushPoints;        // 当前笔画的预览轨迹(图像像素)
    int m_brushSize = 20;          // 直径(图像像素)
    int m_brushHardness = 70;      // 0~100，边缘由硬到柔
    double m_brushOpacity = 0.8;   // 0.0~1.0
    QColor m_brushColor = QColor(20, 20, 20);
    // ---- 渐变工具 ----
    bool m_gradientMode = false;
    bool m_gradientDragging = false;
    QPoint m_gradientStart;        // 拖拽起点(图像像素)
    QPoint m_gradientEnd;          // 拖拽终点(图像像素)
    QPolygon m_points;           // 套索轨迹(图像像素坐标)
    QPolygon m_poly;             // 多边形套索已确定的顶点(图像像素坐标)
    QPoint m_hover;              // 多边形套索的橡皮筋终点
    bool m_hoverValid = false;
    bool m_addMode = false;      // 本次拖拽是否为加选(Shift)
    bool m_subtractMode = false; // 本次套索是否为减选(Alt)
    bool m_moveContent = false;  // 本次拖动是否同时搬动选区内像素(Ctrl)
    QPoint m_pressPos;           // 移动起点(控件坐标)
    QPoint m_anchor;             // 建立选区锚点(图像坐标)
    QRect m_startSel;            // 移动前的选区

    // ---- 交互式变换会话 ----
    bool m_transform = false;          // 是否处于拖拽手柄变换
    QImage m_transformWhite;           // 基准矩形大小，白色 + 掩码 alpha
    QImage m_transformContent;         // 基准矩形大小，原像素 + 掩码 alpha
    QRectF m_transformBase;            // 基准矩形(图像坐标)
    double m_sx = 1.0;                 // 当前水平缩放
    double m_sy = 1.0;                 // 当前垂直缩放
    double m_angle = 0.0;              // 当前旋转角度(度)
    int m_handle = -1;                 // 0..7 缩放 / 8 旋转
    double m_startSx = 1.0;            // 本次拖动开始时的缩放
    double m_startSy = 1.0;
    double m_startAngle = 0.0;         // 本次拖动开始时的角度
    double m_startPointerAngle = 0.0;  // 按下时指针相对选区中心的角度
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFocus();
    setWindowTitle("ImageWithUI · 图片编辑器");
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
    ui->toolBar->addSeparator();
    ui->toolBar->addAction(ui->actionscale);

    // 视图菜单：主题切换 / 适应窗口
    QMenu *viewMenu = menuBar()->addMenu(QString("视图"));
    // Ctrl+T 已交给「自由变换（拖拽手柄）」，主题切换改用 Ctrl+Shift+T
    QAction *themeAction = viewMenu->addAction(QString("切换深色/浅色主题"));
    themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
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
    SetupChannelPanel();    // 右侧通道面板(存储/载入选区)
    SetupBrushPanel();      // 右侧画笔参数面板
    SetupGradientPanel();   // 右侧渐变参数面板

    // ---- 右侧面板放进滚动区 ----
    // 窗口变矮时面板可以滚动，而不是把控件压扁或把窗口撑出屏幕
    QScrollArea *panelScroll = new QScrollArea(ui->widget);
    panelScroll->setObjectName("panelScroll");
    panelScroll->setWidgetResizable(true);
    panelScroll->setFrameShape(QFrame::NoFrame);
    panelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QLayout *panelLayout = ui->widget->layout();
    if (panelLayout != nullptr) {
        panelLayout->removeWidget(ui->widget_14);
        panelScroll->setWidget(ui->widget_14);
        panelLayout->addWidget(panelScroll);
    }

    // ---- 窗口自适应 ----
    // 初始尺寸取"设计尺寸"与"可用屏幕 92%"的较小值，并居中；
    // 最小尺寸随屏幕收缩，保证小屏/高缩放下窗口能完整放进屏幕
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    const QSize design(1091, 800);
    const int fitW = qMin(design.width(), static_cast<int>(avail.width() * 0.92));
    const int fitH = qMin(design.height(), static_cast<int>(avail.height() * 0.92));
    resize(fitW, fitH);
    setMinimumSize(qMin(760, fitW), qMin(520, fitH));
    move(avail.left() + (avail.width() - fitW) / 2, avail.top() + (avail.height() - fitH) / 2);

    // 把带快捷键的动作用于整个应用：焦点在图片微调/放大眼睛等子窗口时，
    // Ctrl+Z、Ctrl+S 这些依然生效(默认的 WindowShortcut 只在主窗口激活时有效)
    for (QAction *action : findChildren<QAction *>()) {
        if (!action->shortcut().isEmpty()) {
            action->setShortcutContext(Qt::ApplicationShortcut);
        }
    }
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
        const QLineEdit *edit = qobject_cast<QLineEdit *>(watched);
        if (key->key() == Qt::Key_Escape &&
            (PreviewFieldIndex(edit) >= 0 || edit == ui->lineEdit_rotate ||
             edit == ui->lineEdit_rotate_r)) {
            CancelPreview();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::ResetImage(BitmapImage &myValue)
{
    imageData = myValue.imageData;
    // 恢复原始图像
    const QImage originalImage = ImageOps::ToDisplayImage(myValue.imageData,
                                                          myValue.bmpInfo.GetWidth(),
                                                          myValue.bmpInfo.GetHeight());
    ui->imageLabel->setText(QString());
    ui->imageLabel->setPixmap(QPixmap::fromImage(originalImage));
}

void MainWindow::ShowImage(const std::vector<uint8_t> &inImageData, BitmapImage value, int32_t width, int32_t height)
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
    const QImage image = ImageOps::ToDisplayImage(data, width, height);
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
    m_overlay->onSelectionChanged = [this](const QRect &rect, bool add) {
        SetSelection(rect, add);
        if (rect.isEmpty()) {
            UpdateStatus("已取消选区");
            return;
        }
        const int count = std::count_if(m_mask.data.begin(), m_mask.data.end(),
                                        [](uint8_t v) { return v != 0; });
        UpdateStatus(QString("选区 %1 × %2 px（覆盖 %3 像素）%4 · 调整类操作只在选区内生效")
                         .arg(m_selection.width()).arg(m_selection.height())
                         .arg(count)
                         .arg(add ? QString("，已加选") : QString()));
    };
    m_overlay->onSelectionMoved = [this](int dx, int dy, bool content) {
        if (content) {
            MoveSelectionContent(dx, dy);
        } else {
            TranslateSelection(dx, dy);
            UpdateStatus(QString("选区已移动 (%1, %2)").arg(dx).arg(dy));
        }
    };
    m_overlay->onLassoFinished = [this](const QPolygon &poly, bool add, bool subtract) {
        SetSelectionPolygon(poly, add, subtract);
    };
    // 磁性套索吸附用的边缘强度：像素亮度梯度的曼哈顿幅值
    m_overlay->edgeAt = [this](int x, int y) -> int {
        const int32_t w = myValue.bmpInfo.GetWidth();
        const int32_t h = myValue.bmpInfo.GetHeight();
        if (imageData.empty() || w <= 0 || h <= 0) {
            return 0;
        }
        auto luma = [w, h](const std::vector<uint8_t> &px, int xx, int yy) -> int {
            xx = qBound(0, xx, w - 1);
            yy = qBound(0, yy, h - 1);
            const size_t i = (static_cast<size_t>(h - 1 - yy) * static_cast<size_t>(w) +
                              static_cast<size_t>(xx)) * 3;
            return (static_cast<int>(px[i]) + px[i + 1] + px[i + 2]) / 3;
        };
        const int gx = luma(imageData, x + 1, y) - luma(imageData, x - 1, y);
        const int gy = luma(imageData, x, y + 1) - luma(imageData, x, y - 1);
        return qAbs(gx) + qAbs(gy);
    };
    // 拖拽手柄变换：拖动中刷新状态栏；回车/双击提交，Esc 取消
    m_overlay->onTransformChanged = [this](double sx, double sy, double angle) {
        QString text;
        if (std::abs(sx - sy) < 1e-6) {
            text = QString("自由变换：缩放 %1%").arg(sx * 100.0, 0, 'f', 0);
        } else {
            text = QString("自由变换：缩放 %1% × %2%")
                       .arg(sx * 100.0, 0, 'f', 0)
                       .arg(sy * 100.0, 0, 'f', 0);
        }
        if (std::abs(angle) >= 0.05) {
            text += QString(" · 旋转 %1°").arg(angle, 0, 'f', 1);
        }
        UpdateStatus(text + "（回车提交，Esc 取消）");
    };
    m_overlay->onTransformFinished = [this](bool commit, double sx, double sy, double angle) {
        FinishInteractiveTransform(commit, sx, sy, angle);
    };
    // 画笔/橡皮：笔画开始记录快照，松开时合并为一步历史
    m_overlay->onBrushStart = [this](const QPoint &p) { BeginBrushStroke(p); };
    m_overlay->onBrushSegment = [this](const QPoint &from, const QPoint &to) {
        BrushSegmentTo(from, to);
    };
    m_overlay->onBrushEnd = [this]() { EndBrushStroke(); };
    m_overlay->setBrushParams(m_brushSize, m_brushHardness, m_brushOpacity, m_brushColor);
    m_overlay->onGradientFinished = [this](const QPoint &from, const QPoint &to) {
        ApplyGradientFill(from, to, m_gradientRadial);
    };
    ui->imageLabel->installEventFilter(this);

    // 工具栏：矩形选框 / 自由套索 / 多边形套索 / 磁性套索(勾选后才在图像上拖拽，避免误触)
    QActionGroup *toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    m_selectionToolAction = new QAction(QString("矩形选区"), this);
    m_selectionToolAction->setCheckable(true);
    m_selectionToolAction->setToolTip(QString("选中后在图像上按住左键拖动建立矩形选区；Shift 加选；在选区内拖动可移动选区，按住 Ctrl 拖动则同时移动选区内像素"));
    toolGroup->addAction(m_selectionToolAction);

    m_lassoToolAction = new QAction(QString("手绘选区"), this);
    m_lassoToolAction->setCheckable(true);
    m_lassoToolAction->setToolTip(QString("选中后沿图像拖动描出边界，松开建立不规则选区；Shift 加选，Alt 减选"));

    m_polyToolAction = new QAction(QString("多边形选区"), this);
    m_polyToolAction->setCheckable(true);
    m_polyToolAction->setToolTip(QString("依次单击落点描边，点回首点 / 双击 / 按 Enter 闭合；Backspace 撤销上一点，Esc 取消；Shift 加选，Alt 减选"));

    m_magnetToolAction = new QAction(QString("自动贴边选区"), this);
    m_magnetToolAction->setCheckable(true);
    m_magnetToolAction->setToolTip(QString("沿目标边缘拖动，轨迹点会自动吸附到 8 px 内最强的边缘上；松开闭合；Shift 加选，Alt 减选"));

    m_brushToolAction = new QAction(QString("画笔"), this);
    m_brushToolAction->setCheckable(true);
    m_brushToolAction->setToolTip(QString("在活动图层上按住左键绘制；笔迹遵守当前选区覆盖度"));

    m_eraserToolAction = new QAction(QString("橡皮"), this);
    m_eraserToolAction->setCheckable(true);
    m_eraserToolAction->setToolTip(QString("用白色覆盖活动图层(当前没有透明通道)；笔迹遵守当前选区覆盖度"));

    m_gradientToolAction = new QAction(QString("渐变"), this);
    m_gradientToolAction->setCheckable(true);
    m_gradientToolAction->setToolTip(QString("在活动图层上按拖拽方向填充线性/径向渐变；遵守当前选区覆盖度"));

    // 所有画布工具共用一套状态同步：任一被勾选时启用覆盖层，并切换工具类型
    auto applyToolState = [this]() {
        // 切换工具时放弃未提交的拖拽变换，避免手柄状态残留
        if (m_overlay != nullptr && m_overlay->isTransformActive()) {
            CancelInteractiveTransform();
        }
        const bool rect = m_selectionToolAction->isChecked();
        const bool lasso = m_lassoToolAction->isChecked();
        const bool poly = m_polyToolAction->isChecked();
        const bool magnet = m_magnetToolAction->isChecked();
        const bool brush = m_brushToolAction->isChecked();
        const bool eraser = m_eraserToolAction->isChecked();
        const bool gradient = m_gradientToolAction->isChecked();
        const int kind = magnet ? 3 : poly ? 2 : lasso ? 1 : 0;
        if (m_overlay != nullptr) {
            m_overlay->setToolKind(kind);
            m_overlay->setBrushMode(brush ? 1 : (eraser ? 2 : 0));
            m_overlay->setGradientMode(gradient);
            m_overlay->setToolActive(rect || lasso || poly || magnet || brush || eraser || gradient);
        }
        if (brush) {
            UpdateStatus(QString("画笔：按住左键在活动图层上绘制；笔迹遵守选区"));
        } else if (eraser) {
            UpdateStatus(QString("橡皮：用白色覆盖活动图层；笔迹遵守选区"));
        } else if (gradient) {
            UpdateStatus(QString("渐变：在画布上拖拽定义方向/半径；松开后填充，遵守选区"));
        } else if (magnet) {
            UpdateStatus(QString("自动贴边选区：沿边缘拖动，轨迹会自动贴过去；Shift 加选、Alt 减选"));
        } else if (poly) {
            UpdateStatus(QString("多边形选区：单击落点，点回起点/双击/回车闭合；退格撤点、Esc 取消"));
        } else if (lasso) {
            UpdateStatus(QString("手绘选区：按住鼠标沿图像描一圈，松开后建立选区；Shift 加选、Alt 减选"));
        } else if (rect) {
            UpdateStatus(QString("矩形选区：在图像上拖动框出选区；按住 Shift 加选"));
        } else {
            UpdateStatus(QString("画布工具已关闭"));
        }
    };
    toolGroup->addAction(m_lassoToolAction);
    toolGroup->addAction(m_polyToolAction);
    toolGroup->addAction(m_magnetToolAction);
    toolGroup->addAction(m_brushToolAction);
    toolGroup->addAction(m_eraserToolAction);
    toolGroup->addAction(m_gradientToolAction);
    connect(m_selectionToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_lassoToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_polyToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_magnetToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_brushToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_eraserToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    connect(m_gradientToolAction, &QAction::toggled, this, [applyToolState](bool) { applyToolState(); });
    ui->toolBox->addAction(m_selectionToolAction);
    ui->toolBox->addAction(m_lassoToolAction);
    ui->toolBox->addAction(m_polyToolAction);
    ui->toolBox->addAction(m_magnetToolAction);
    ui->toolBox->addAction(m_brushToolAction);
    ui->toolBox->addAction(m_eraserToolAction);
    ui->toolBox->addAction(m_gradientToolAction);

    // "选择"菜单(放在"编辑"之后、"图像模糊"之前)
    m_selectionMenu = new QMenu(QString("选择"), this);
    m_selectionMenu->addAction(m_selectionToolAction);
    m_selectionMenu->addAction(m_lassoToolAction);
    m_selectionMenu->addAction(m_polyToolAction);
    m_selectionMenu->addAction(m_magnetToolAction);
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
    QAction *pasteAction = addItem(QString("粘贴到选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
                                   [this]() { PasteIntoSelection(); });
    QAction *fillAction = addItem(QString("给选区填色…"), QKeySequence(), [this]() {
        if (!HasSelection()) {
            UpdateStatus("请先建立选区");
            return;
        }
        const QColor color = QColorDialog::getColor(Qt::white, this, QString("给选区填色"));
        if (color.isValid()) {
            FillSelection(color);
        }
    });
    QAction *strokeAction = addItem(QString("给选区描边…"), QKeySequence(), [this]() {
        if (!HasSelection()) {
            UpdateStatus("请先建立选区");
            return;
        }
        const QColor color = QColorDialog::getColor(Qt::white, this, QString("描边颜色"));
        if (!color.isValid()) {
            return;
        }
        NumberDialog dlg(QString("给选区描边"),
                         { NumberParam{QString("宽度（像素）"), 1, 50, 2, 1, 0} }, this);
        if (dlg.exec() == QDialog::Accepted) {
            StrokeSelection(color, static_cast<int>(dlg.value(0)));
        }
    });
    // Ctrl+T 交给拖拽手柄；数值输入入口保留
    QAction *interactiveTransform = addItem(QString("自由变换（拖拽手柄）"),
                                            QKeySequence(Qt::CTRL | Qt::Key_T),
                                            [this]() { BeginInteractiveTransform(); });
    QAction *transformAction = addItem(QString("自由变换…"), QKeySequence(), [this]() {
        if (!HasSelection()) {
            UpdateStatus("请先建立选区");
            return;
        }
        NumberDialog dlg(QString("自由变换"),
                         { NumberParam{QString("缩放（%）"), 10, 400, 100, 1, 0},
                           NumberParam{QString("旋转（度）"), -180, 180, 0, 1, 0} },
                         this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        FreeTransformSelection(dlg.value(0), dlg.value(1));
    });
    QAction *cropAction = addItem(QString("裁到选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
                                  [this]() { CropToSelection(); });
    QAction *eraseAction = addItem(QString("清空选区内容"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Delete),
                                   [this]() { ClearSelectionContent(); });
    addItem(QString("全选"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), [this]() { SelectAll(); });
    addItem(QString("反选"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I), [this]() { InvertSelection(); });
    QAction *featherAction = addItem(QString("让边缘柔和…"), QKeySequence(), [this]() {
        if (!HasSelection()) {
            UpdateStatus("请先建立选区");
            return;
        }
        NumberDialog dlg(QString("让边缘柔和"),
                         { NumberParam{QString("柔和半径（像素）"), 0.5, 64, 4, 0.5, 1} }, this);
        if (dlg.exec() == QDialog::Accepted) {
            FeatherSelection(dlg.value(0));
        }
    });
    auto promptRefine = [this](int mode) {
        if (!HasSelection()) {
            UpdateStatus("请先建立选区");
            return;
        }
        const QString label = (mode == 0) ? QString("扩大选区") : (mode == 1) ? QString("缩小选区")
                                                                             : QString("磨平边缘");
        NumberDialog dlg(label, { NumberParam{QString("半径（像素）"), 1, 200, 2, 1, 0} }, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        const int radius = static_cast<int>(dlg.value(0));
        if (mode == 0) {
            GrowSelection(radius);
        } else if (mode == 1) {
            ShrinkSelection(radius);
        } else {
            SmoothSelection(radius);
        }
    };
    QAction *growAction = addItem(QString("扩大选区…"), QKeySequence(), [promptRefine]() { promptRefine(0); });
    QAction *shrinkAction = addItem(QString("缩小选区…"), QKeySequence(), [promptRefine]() { promptRefine(1); });
    QAction *smoothAction = addItem(QString("磨平边缘…"), QKeySequence(), [promptRefine]() { promptRefine(2); });
    addItem(QString("取消选区"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), [this]() {
        ClearSelection();
        UpdateStatus("已取消选区");
    });

    // 选区通道：每次弹出时按当前通道列表重建，避免增删后索引失效
    m_channelMenu = new QMenu(QString("选区通道"), this);
    connect(m_channelMenu, &QMenu::aboutToShow, this, [this]() {
        m_channelMenu->clear();
        QAction *storeAction = m_channelMenu->addAction(QString("存储当前选区…"));
        storeAction->setEnabled(HasSelection());
        connect(storeAction, &QAction::triggered, this, [this]() { PromptStoreChannel(); });
        m_channelMenu->addSeparator();
        if (m_channels.empty()) {
            QAction *noneAction = m_channelMenu->addAction(QString("（暂无通道）"));
            noneAction->setEnabled(false);
            return;
        }
        QMenu *loadMenu = m_channelMenu->addMenu(QString("载入选区"));
        QMenu *deleteMenu = m_channelMenu->addMenu(QString("删除通道"));
        for (int i = 0; i < static_cast<int>(m_channels.size()); ++i) {
            const QString name = m_channels[static_cast<size_t>(i)].name;
            QAction *loadAction = loadMenu->addAction(name);
            connect(loadAction, &QAction::triggered, this, [this, i]() { LoadSelectionChannel(i); });
            QAction *deleteAction = deleteMenu->addAction(name);
            connect(deleteAction, &QAction::triggered, this, [this, i]() {
                DeleteSelectionChannel(i);
            });
        }
    });
    m_selectionMenu->addSeparator();
    m_selectionMenu->addMenu(m_channelMenu);

    // 没有选区时禁用需要选区的操作
    connect(m_selectionMenu, &QMenu::aboutToShow, this,
            [this, copyAction, pasteAction, fillAction, strokeAction, interactiveTransform,
             transformAction, cropAction, eraseAction, featherAction, growAction, shrinkAction,
             smoothAction]() {
        const bool has = HasSelection();
        copyAction->setEnabled(has);
        pasteAction->setEnabled(has);
        fillAction->setEnabled(has);
        strokeAction->setEnabled(has);
        interactiveTransform->setEnabled(has);
        transformAction->setEnabled(has);
        cropAction->setEnabled(has);
        eraseAction->setEnabled(has);
        featherAction->setEnabled(has);
        growAction->setEnabled(has);
        shrinkAction->setEnabled(has);
        smoothAction->setEnabled(has);
    });

    // "工具"菜单：画笔/橡皮/渐变
    QMenu *toolMenu = new QMenu(QString("工具"), this);
    toolMenu->addAction(m_brushToolAction);
    toolMenu->addAction(m_eraserToolAction);
    toolMenu->addAction(m_gradientToolAction);
    menuBar()->insertMenu(m_selectionMenu->menuAction(), toolMenu);

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
    m_overlay->setMask(hasImage ? BuildMaskPreview() : QImage(),
                       hasImage ? m_selection : QRect());
    m_overlay->setVisible(hasImage);
    m_overlay->raise();
}

// 图像尺寸变化后把选区夹回图像范围(裁剪/旋转/撤销等都会走这里)
void MainWindow::SyncSelectionToImage()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    // 撤销/裁剪/换图后变换基准已失效，先静默退出拖拽手柄
    if (m_overlay != nullptr && m_overlay->isTransformActive() &&
        (imageData.empty() || w <= 0 || h <= 0 || !HasSelection() ||
         m_mask.width != w || m_mask.height != h)) {
        CancelInteractiveTransform();
    }
    if (imageData.empty() || w <= 0 || h <= 0) {
        m_mask.clear();
        m_selection = QRect();
    } else if (!m_mask.isEmpty()) {
        // 画布尺寸变了(裁剪/旋转)：原掩码坐标已失效，直接取消选区
        if (m_mask.width != w || m_mask.height != h) {
            m_mask.clear();
        }
        m_selection = MaskBoundingRect();
        if (m_selection.isEmpty()) {
            m_mask.clear();
        }
    } else {
        m_selection = QRect();
    }
    UpdateSelectionOverlay();
}

QRect MainWindow::MaskBoundingRect() const
{
    if (m_mask.isEmpty()) {
        return QRect();
    }
    const int32_t w = m_mask.width;
    const int32_t h = m_mask.height;
    int32_t x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *row = m_mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        int32_t first = -1;
        int32_t last = -1;
        for (int32_t x = 0; x < w; ++x) {
            if (row[x] != 0) {
                if (first < 0) {
                    first = x;
                }
                last = x;
            }
        }
        if (first < 0) {
            continue;
        }
        if (y < y0) {
            y0 = y;
        }
        y1 = y;
        x0 = std::min(x0, first);
        x1 = std::max(x1, last);
    }
    if (x1 < x0 || y1 < y0) {
        return QRect();
    }
    return QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

// 掩码 → 半透明青色缩略图(长边不超过 512)，覆盖层直接拉伸显示
QImage MainWindow::BuildMaskPreview() const
{
    if (m_mask.isEmpty()) {
        return QImage();
    }
    const int32_t w = m_mask.width;
    const int32_t h = m_mask.height;
    int step = 1;
    while (std::max(w, h) / step > 512) {
        ++step;
    }
    const int32_t ow = (w + step - 1) / step;
    const int32_t oh = (h + step - 1) / step;
    // 注意：必须用非预乘的 ARGB32。若用 Premultiplied 格式却写入
    // 未预乘的颜色(颜色分量大于 alpha)，Qt 会画出高饱和的异常色块
    QImage img(ow, oh, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    for (int32_t y = 0; y < oh; ++y) {
        const int32_t sy = std::min(h - 1, y * step + step / 2);
        QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
        const uint8_t *src = m_mask.data.data() + static_cast<size_t>(sy) * static_cast<size_t>(w);
        for (int32_t x = 0; x < ow; ++x) {
            const int32_t sx = std::min(w - 1, x * step + step / 2);
            const int alpha = static_cast<int>(src[sx]) * 90 / 255;
            dst[x] = qRgba(34, 211, 238, alpha);
        }
    }
    return img;
}

// 按整数步长抽样掩码(口径与 DownsampleBgr 一致，供低分辨率预览使用)
SelectionMask MainWindow::DownsampleMask(const SelectionMask &mask, int step)
{
    SelectionMask out;
    if (mask.isEmpty() || step <= 0) {
        return out;
    }
    out.width = mask.width / step;
    out.height = mask.height / step;
    if (out.width <= 0 || out.height <= 0) {
        out.width = 0;
        out.height = 0;
        return out;
    }
    out.data.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0);
    for (int32_t y = 0; y < out.height; ++y) {
        const uint8_t *src = mask.data.data() + static_cast<size_t>(mask.width)
                             * static_cast<size_t>(y * step);
        uint8_t *dst = out.data.data() + static_cast<size_t>(y) * static_cast<size_t>(out.width);
        for (int32_t x = 0; x < out.width; ++x) {
            dst[x] = src[x * step];
        }
    }
    return out;
}

// add=true 时把矩形并入已有掩码(Shift 加选)，否则整份替换
void MainWindow::SetSelection(const QRect &imageRect, bool add)
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        m_mask.clear();
        m_selection = QRect();
        UpdateSelectionOverlay();
        return;
    }
    const QRect r = imageRect.normalized().intersected(QRect(0, 0, w, h));
    if (r.isEmpty()) {
        if (!add) {
            m_mask.clear();
            m_selection = QRect();
            UpdateSelectionOverlay();
        }
        return;
    }
    if (!add || m_mask.isEmpty() || m_mask.width != w || m_mask.height != h) {
        m_mask.width = w;
        m_mask.height = h;
        m_mask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    }
    for (int32_t y = r.y(); y < r.y() + r.height(); ++y) {
        uint8_t *row = m_mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        std::fill(row + r.x(), row + r.x() + r.width(), static_cast<uint8_t>(255));
    }
    m_selection = MaskBoundingRect();
    UpdateSelectionOverlay();
}

void MainWindow::SetSelectionPolygon(const QPolygon &points, bool add, bool subtract)
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        m_mask.clear();
        m_selection = QRect();
        UpdateSelectionOverlay();
        return;
    }
    if (points.size() < 3) {
        if (!add && !subtract) {
            ClearSelection();
            UpdateStatus("套索轨迹太短，已取消选区");
        }
        return;
    }

    // 用 QPainter 把多边形栅格化成与图像同尺寸的图(白色 = 在多边形内)
    QImage shape(w, h, QImage::Format_RGB32);
    shape.fill(Qt::black);
    {
        QPainter p(&shape);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPolygon(points);
    }
    SelectionMask target;
    target.width = w;
    target.height = h;
    target.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int32_t y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(shape.constScanLine(y));
        uint8_t *dst = target.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int32_t x = 0; x < w; ++x) {
            dst[x] = qRed(line[x]) >= 128 ? 255 : 0;
        }
    }

    if (subtract) {
        // 减选：按覆盖度从已有掩码里扣掉多边形区域(没有选区时无效果)
        if (!m_mask.isEmpty() && m_mask.width == w && m_mask.height == h) {
            for (size_t i = 0; i < m_mask.data.size(); ++i) {
                m_mask.data[i] = static_cast<uint8_t>(static_cast<int>(m_mask.data[i]) *
                                                      (255 - static_cast<int>(target.data[i])) / 255);
            }
        }
    } else if (add && !m_mask.isEmpty() && m_mask.width == w && m_mask.height == h) {
        for (size_t i = 0; i < m_mask.data.size(); ++i) {
            m_mask.data[i] = std::max(m_mask.data[i], target.data[i]);
        }
    } else {
        m_mask = std::move(target);
    }

    m_selection = MaskBoundingRect();
    if (m_selection.isEmpty()) {
        m_mask.clear();
        UpdateSelectionOverlay();
        UpdateStatus("套索选区为空，已取消选区");
        return;
    }
    UpdateSelectionOverlay();
    UpdateStatus(QString("套索选区 %1 × %2 px%3")
                     .arg(m_selection.width()).arg(m_selection.height())
                     .arg(subtract ? QString("，已减选") : add ? QString("，已加选") : QString()));
}

void MainWindow::SetSelectionMask(const SelectionMask &mask)
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (mask.isEmpty() || imageData.empty() || w <= 0 || h <= 0 ||
        mask.width != w || mask.height != h) {
        m_mask.clear();
        m_selection = QRect();
    } else {
        m_mask = mask;
        m_selection = MaskBoundingRect();
        if (m_selection.isEmpty()) {
            m_mask.clear();
        }
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
    m_mask.width = w;
    m_mask.height = h;
    m_mask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 255);
    m_selection = QRect(0, 0, w, h);
    UpdateSelectionOverlay();
    UpdateStatus(QString("已全选 %1 × %2 px").arg(w).arg(h));
}

void MainWindow::InvertSelection()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_mask.isEmpty()) {
        // 没有选区 = 没有任何像素被选中，反选即全选
        SelectAll();
        UpdateStatus(QString("没有选区，已反选为全选 %1 × %2 px").arg(w).arg(h));
        return;
    }
    if (m_mask.width != w || m_mask.height != h) {
        m_mask.width = w;
        m_mask.height = h;
        m_mask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    }
    for (uint8_t &v : m_mask.data) {
        v = static_cast<uint8_t>(255 - v);
    }
    m_selection = MaskBoundingRect();
    if (m_selection.isEmpty()) {
        m_mask.clear();
        UpdateSelectionOverlay();
        UpdateStatus("已反选：选区为空（相当于取消选区）");
        return;
    }
    UpdateSelectionOverlay();
    UpdateStatus(QString("已反选：选区 %1 × %2 px").arg(m_selection.width()).arg(m_selection.height()));
}

void MainWindow::FeatherSelection(double radius)
{
    if (m_mask.isEmpty()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const int r = qRound(radius);
    if (r < 1) {
        UpdateStatus("柔和半径太小（至少 0.5 像素）");
        return;
    }
    // 两次盒式模糊逼近高斯，边界过渡更自然
    BoxBlurMask(m_mask.data, m_mask.width, m_mask.height, r);
    BoxBlurMask(m_mask.data, m_mask.width, m_mask.height, r);
    m_selection = MaskBoundingRect();
    if (m_selection.isEmpty()) {
        m_mask.clear();
        UpdateSelectionOverlay();
        UpdateStatus("处理后选区为空，已取消选区");
        return;
    }
    UpdateSelectionOverlay();
    UpdateStatus(QString("已让边缘柔和（半径 %1 px）：选区 %2 × %3 px")
                     .arg(radius)
                     .arg(m_selection.width())
                     .arg(m_selection.height()));
}

void MainWindow::GrowSelection(int radius)
{
    RefineSelection(radius, 0);
}

void MainWindow::ShrinkSelection(int radius)
{
    RefineSelection(radius, 1);
}

void MainWindow::SmoothSelection(int radius)
{
    RefineSelection(radius, 2);
}

// 扩展/收缩/平滑统一实现：先用半径 r 的盒式模糊得到"半径内覆盖度"，
// 再按阈值二值化 —— 有任何覆盖即选中(扩展)、全窗口覆盖才选中(收缩)、
// 过半覆盖才选中(平滑)。模糊对整幅掩码做 O(1)/像素，不做逐像素圆盘采样。
void MainWindow::RefineSelection(int radius, int mode)
{
    if (m_mask.isEmpty()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const int r = radius;
    if (r < 1) {
        UpdateStatus("半径太小(至少 1 像素)");
        return;
    }
    const QString label = (mode == 0) ? QString("扩展") : (mode == 1) ? QString("收缩") : QString("平滑");

    BoxBlurMask(m_mask.data, m_mask.width, m_mask.height, r);
    for (uint8_t &v : m_mask.data) {
        if (mode == 0) {
            v = (v > 0) ? 255 : 0;
        } else if (mode == 1) {
            v = (v == 255) ? 255 : 0;
        } else {
            v = (v >= 128) ? 255 : 0;
        }
    }

    m_selection = MaskBoundingRect();
    if (m_selection.isEmpty()) {
        m_mask.clear();
        UpdateSelectionOverlay();
        UpdateStatus(QString("%1 %2 px 后选区为空，已取消选区").arg(label).arg(r));
        return;
    }
    UpdateSelectionOverlay();
    UpdateStatus(QString("%1选区 %2 px：选区 %3 × %4 px")
                     .arg(label).arg(r).arg(m_selection.width()).arg(m_selection.height()));
}

void MainWindow::TranslateSelection(int dx, int dy)
{
    if (m_mask.isEmpty() || (dx == 0 && dy == 0)) {
        return;
    }
    m_mask = TranslateMask(m_mask, dx, dy);
    m_selection = MaskBoundingRect();
    if (m_selection.isEmpty()) {
        m_mask.clear();
    }
    UpdateSelectionOverlay();
}

QPoint MainWindow::SnapSelectionEdge(const std::function<int(int, int)> &edgeAt,
                                     const QPoint &p, int radius)
{
    if (!edgeAt || radius < 1) {
        return p;
    }
    int bestScore = 0;
    QPoint best = p;
    bool found = false;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int dist = qAbs(dx) + qAbs(dy);
            if (dist > radius) {
                continue;   // 圆形搜索窗口
            }
            const int mag = edgeAt(p.x() + dx, p.y() + dy);
            if (mag <= 0) {
                continue;
            }
            // 强度优先，同时惩罚距离，避免为了强边缘跳得太远
            const int score = mag * 4 - dist * 3;
            if (!found || score > bestScore) {
                found = true;
                bestScore = score;
                best = QPoint(p.x() + dx, p.y() + dy);
            }
        }
    }
    return found ? best : p;
}

void MainWindow::MoveSelectionContent(int dx, int dy)
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (!HasSelection() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区");
        return;
    }
    if (dx == 0 && dy == 0) {
        return;
    }

    const std::vector<uint8_t> orig = imageData;   // 内容来源永远是搬动前的像素
    std::vector<uint8_t> result = orig;
    SelectionMask moved = TranslateMask(m_mask, dx, dy);

    // 1) 原选区按覆盖度刷白：相当于把这段内容"剪走"
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *mrow = m_mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const size_t rowBase = static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w);
        for (int32_t x = 0; x < w; ++x) {
            const int a = mrow[x];
            if (a == 0) {
                continue;
            }
            uint8_t *p = result.data() + (rowBase + static_cast<size_t>(x)) * 3;
            for (int c = 0; c < 3; ++c) {
                p[c] = static_cast<uint8_t>((255 * a + static_cast<int>(p[c]) * (255 - a)) / 255);
            }
        }
    }
    // 2) 把内容按新掩码覆盖度贴到新位置(重叠区域读的是搬动前的像素)
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *mrow = moved.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const size_t rowBase = static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w);
        const int32_t sy = y - dy;
        for (int32_t x = 0; x < w; ++x) {
            const int a = mrow[x];
            if (a == 0) {
                continue;
            }
            const int32_t sx = x - dx;
            uint8_t *dst = result.data() + (rowBase + static_cast<size_t>(x)) * 3;
            if (sx < 0 || sx >= w || sy < 0 || sy >= h) {
                dst[0] = dst[1] = dst[2] = 255;   // 移出画布的部分留白
                continue;
            }
            const uint8_t *src = orig.data() +
                                 (static_cast<size_t>(h - 1 - sy) * static_cast<size_t>(w) +
                                  static_cast<size_t>(sx)) * 3;
            for (int c = 0; c < 3; ++c) {
                dst[c] = static_cast<uint8_t>((a * static_cast<int>(src[c]) +
                                               (255 - a) * static_cast<int>(dst[c])) / 255);
            }
        }
    }

    m_mask = std::move(moved);
    m_selection = MaskBoundingRect();
    ApplySnapshot(MakeSnapshot(std::move(result), w, h, m_contentQuad), false, "移动选区内容");
    UpdateStatus(QString("已移动选区内容 (%1, %2)").arg(dx).arg(dy));
}

void MainWindow::PasteIntoSelection()
{
    const QImage clip = QApplication::clipboard()->image();
    if (clip.isNull()) {
        UpdateStatus("剪贴板里没有图像，无法粘贴");
        return;
    }
    PasteImageIntoSelection(clip);
}

void MainWindow::PasteImageIntoSelection(const QImage &image)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (image.isNull()) {
        UpdateStatus("没有可粘贴的图像");
        return;
    }
    if (!HasSelection() || m_selection.isEmpty()) {
        UpdateStatus("请先建立选区，再粘贴");
        return;
    }

    const QRect sel = m_selection.intersected(QRect(0, 0, w, h));
    if (sel.isEmpty()) {
        UpdateStatus("选区超出画布范围，无法粘贴");
        return;
    }
    // 等比缩放到选区包围盒内并居中，避免拉伸变形
    const QImage src = image.convertToFormat(QImage::Format_RGB32);
    const QImage scaled = src.scaled(sel.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (scaled.isNull()) {
        UpdateStatus("粘贴失败：图像缩放异常");
        return;
    }
    const int ox = sel.x() + (sel.width() - scaled.width()) / 2;
    const int oy = sel.y() + (sel.height() - scaled.height()) / 2;

    std::vector<uint8_t> result = imageData;
    for (int y = 0; y < scaled.height(); ++y) {
        const int vy = oy + y;
        if (vy < 0 || vy >= h) {
            continue;
        }
        const QRgb *line = reinterpret_cast<const QRgb *>(scaled.constScanLine(y));
        const size_t rowBase = static_cast<size_t>(h - 1 - vy) * static_cast<size_t>(w);
        for (int x = 0; x < scaled.width(); ++x) {
            const int vx = ox + x;
            if (vx < 0 || vx >= w) {
                continue;
            }
            uint8_t *p = result.data() + (rowBase + static_cast<size_t>(vx)) * 3;
            p[0] = static_cast<uint8_t>(qBlue(line[x]));
            p[1] = static_cast<uint8_t>(qGreen(line[x]));
            p[2] = static_cast<uint8_t>(qRed(line[x]));
        }
    }
    ApplyResult(result, "粘贴图片到选区");   // 内部会把选区外的像素还原，羽化边缘自然过渡
    UpdateStatus(QString("已粘贴图像到选区（%1 × %2 px）")
                     .arg(scaled.width()).arg(scaled.height()));
}

void MainWindow::ClearSelection()
{
    m_mask.clear();
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
    if (!HasSelection() || m_selection.isEmpty()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const QImage full = ImageOps::ToDisplayImage(imageData, w, h);
    const QImage sub = full.copy(m_selection.intersected(full.rect()));
    if (sub.isNull() || sub.width() <= 0 || sub.height() <= 0) {
        UpdateStatus("复制选区失败");
        return;
    }
    QApplication::clipboard()->setImage(sub);
    UpdateStatus(QString("已复制选区范围 %1 × %2 px 到剪贴板").arg(sub.width()).arg(sub.height()));
}

void MainWindow::CropToSelection()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张图片");
        return;
    }
    if (!HasSelection()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const QRect sel = m_selection;
    if (sel.width() < 4 || sel.height() < 4) {
        UpdateStatus("选区太小(至少 4 × 4)，已取消裁剪");
        return;
    }
    if (!RequireSingleLayer("裁到选区")) {
        return;
    }
    ApplyOperator("裁到选区", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.CropImage(d, ctx.width, ctx.height,
                    sel.x(), sel.y(), sel.width(), sel.height(), ctx.quad);
    }, nullptr, [this]() { ClearSelection(); }, false);
}

void MainWindow::ClearSelectionContent()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张图片");
        return;
    }
    if (!HasSelection()) {
        UpdateStatus("请先建立选区");
        return;
    }
    // 按掩码覆盖度把像素刷白：羽化过的边缘会得到半透明白
    const SelectionMask mask = m_mask;
    ApplyOperator("清空选区内容", [mask](std::vector<uint8_t> &d, OpContext &ctx) {
        if (mask.isEmpty() || mask.width != ctx.width || mask.height != ctx.height) {
            return;
        }
        for (int32_t y = 0; y < ctx.height; ++y) {
            const uint8_t *mrow = mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(ctx.width);
            // 视觉行 y 对应内存行 height-1-y(第 0 行 = 画面最底行)
            const size_t rowBase = static_cast<size_t>(ctx.height - 1 - y) * static_cast<size_t>(ctx.width);
            for (int32_t x = 0; x < ctx.width; ++x) {
                const int a = mrow[x];
                if (a == 0) {
                    continue;
                }
                uint8_t *p = d.data() + (rowBase + static_cast<size_t>(x)) * 3;
                if (a == 255) {
                    p[0] = p[1] = p[2] = 255;
                } else {
                    for (int c = 0; c < 3; ++c) {
                        p[c] = static_cast<uint8_t>((255 * a + static_cast<int>(p[c]) * (255 - a)) / 255);
                    }
                }
            }
        }
    }, nullptr, [this]() { UpdateStatus("已清空选区内容"); }, false);
}

void MainWindow::FillSelection(const QColor &color)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (!HasSelection()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const SelectionMask mask = m_mask;
    const int cr = color.red();
    const int cg = color.green();
    const int cb = color.blue();
    ApplyOperator("给选区填色", [mask, cr, cg, cb](std::vector<uint8_t> &d, OpContext &ctx) {
        if (mask.isEmpty() || mask.width != ctx.width || mask.height != ctx.height) {
            return;
        }
        for (int32_t y = 0; y < ctx.height; ++y) {
            const uint8_t *mrow = mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(ctx.width);
            const size_t rowBase = static_cast<size_t>(ctx.height - 1 - y) * static_cast<size_t>(ctx.width);
            for (int32_t x = 0; x < ctx.width; ++x) {
                const int a = mrow[x];
                if (a == 0) {
                    continue;
                }
                uint8_t *p = d.data() + (rowBase + static_cast<size_t>(x)) * 3;
                p[0] = static_cast<uint8_t>((cb * a + static_cast<int>(p[0]) * (255 - a)) / 255);
                p[1] = static_cast<uint8_t>((cg * a + static_cast<int>(p[1]) * (255 - a)) / 255);
                p[2] = static_cast<uint8_t>((cr * a + static_cast<int>(p[2]) * (255 - a)) / 255);
            }
        }
    }, nullptr, [this, color]() {
        UpdateStatus(QString("已给选区填上 %1").arg(color.name()));
    }, false);
}

void MainWindow::StrokeSelection(const QColor &color, int width)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (!HasSelection()) {
        UpdateStatus("请先建立选区");
        return;
    }
    const int thick = qBound(1, width, 100);
    const SelectionMask mask = m_mask;
    const int cr = color.red();
    const int cg = color.green();
    const int cb = color.blue();
    // 向内描边：把掩码收缩 thick 像素，原掩码减去收缩后剩下的就是边界带
    ApplyOperator("给选区描边", [mask, thick, cr, cg, cb](std::vector<uint8_t> &d, OpContext &ctx) {
        if (mask.isEmpty() || mask.width != ctx.width || mask.height != ctx.height) {
            return;
        }
        SelectionMask eroded = mask;
        BoxBlurMask(eroded.data, ctx.width, ctx.height, thick);
        for (uint8_t &v : eroded.data) {
            v = (v == 255) ? 255 : 0;
        }
        for (int32_t y = 0; y < ctx.height; ++y) {
            const uint8_t *mrow = mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(ctx.width);
            const uint8_t *erow = eroded.data.data() + static_cast<size_t>(y) * static_cast<size_t>(ctx.width);
            const size_t rowBase = static_cast<size_t>(ctx.height - 1 - y) * static_cast<size_t>(ctx.width);
            for (int32_t x = 0; x < ctx.width; ++x) {
                const int a = mrow[x];
                if (a == 0 || erow[x] == 255) {
                    continue;   // 选区外、或已经深入选区内部
                }
                uint8_t *p = d.data() + (rowBase + static_cast<size_t>(x)) * 3;
                p[0] = static_cast<uint8_t>((cb * a + static_cast<int>(p[0]) * (255 - a)) / 255);
                p[1] = static_cast<uint8_t>((cg * a + static_cast<int>(p[1]) * (255 - a)) / 255);
                p[2] = static_cast<uint8_t>((cr * a + static_cast<int>(p[2]) * (255 - a)) / 255);
            }
        }
    }, nullptr, [this, color, thick]() {
        UpdateStatus(QString("已用 %1 描边 %2 px").arg(color.name()).arg(thick));
    }, false);
}

void MainWindow::FreeTransformSelection(double scalePercent, double angleDegrees)
{
    ApplyFreeTransform(scalePercent / 100.0, scalePercent / 100.0, angleDegrees,
                       QStringLiteral("自由变换"));
}

// 非等比缩放 + 旋转：交互式手柄提交与自检都走这里
void MainWindow::ApplyFreeTransform(double scaleX, double scaleY, double angleDegrees,
                                    const QString &label)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (!HasSelection() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区");
        return;
    }
    const double sx = qBound(0.01, scaleX, 20.0);
    const double sy = qBound(0.01, scaleY, 20.0);
    const double angle = angleDegrees;
    if (std::abs(sx - 1.0) < 1e-6 && std::abs(sy - 1.0) < 1e-6 && std::abs(angle) < 1e-6) {
        return;   // 没有变换参数
    }

    const QRect sel = m_selection.intersected(QRect(0, 0, w, h));
    if (sel.isEmpty()) {
        UpdateStatus("选区超出画布范围，无法变换");
        return;
    }

    const std::vector<uint8_t> orig = imageData;   // 内容来源永远是变换前的像素
    std::vector<uint8_t> result = orig;

    // 1) 原选区按覆盖度刷白
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *mrow = m_mask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        const size_t rowBase = static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w);
        for (int32_t x = 0; x < w; ++x) {
            const int a = mrow[x];
            if (a == 0) {
                continue;
            }
            uint8_t *p = result.data() + (rowBase + static_cast<size_t>(x)) * 3;
            for (int c = 0; c < 3; ++c) {
                p[c] = static_cast<uint8_t>((255 * a + static_cast<int>(p[c]) * (255 - a)) / 255);
            }
        }
    }

    // 2) 把选区内容抽成带 alpha(掩码覆盖度)的图像，供整体变换
    QImage content(sel.width(), sel.height(), QImage::Format_ARGB32);
    content.fill(Qt::transparent);
    for (int y = 0; y < sel.height(); ++y) {
        const int vy = sel.y() + y;
        const uint8_t *mrow = m_mask.data.data() + static_cast<size_t>(vy) * static_cast<size_t>(w);
        QRgb *line = reinterpret_cast<QRgb *>(content.scanLine(y));
        for (int x = 0; x < sel.width(); ++x) {
            const int vx = sel.x() + x;
            const int a = mrow[vx];
            if (a == 0) {
                continue;
            }
            const size_t idx = (static_cast<size_t>(h - 1 - vy) * static_cast<size_t>(w) +
                                static_cast<size_t>(vx)) * 3;
            line[x] = qRgba(orig[idx + 2], orig[idx + 1], orig[idx], a);
        }
    }

    // 3) 以选区中心为锚点缩放 + 旋转，画到与画布同尺寸的透明层
    QImage canvas(w, h, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    {
        QPainter p(&canvas);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const double cx = sel.x() + sel.width() / 2.0;
        const double cy = sel.y() + sel.height() / 2.0;
        QTransform t;
        t.translate(cx, cy);
        t.rotate(angle);
        t.scale(sx, sy);
        t.translate(-cx, -cy);
        p.setTransform(t);
        p.drawImage(QPointF(sel.x(), sel.y()), content);
    }

    // 4) 按变换后 alpha 合成到画布，同时得到新的选区掩码
    SelectionMask newMask;
    newMask.width = w;
    newMask.height = h;
    newMask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int32_t y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(canvas.constScanLine(y));
        const size_t rowBase = static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w);
        uint8_t *mrow = newMask.data.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int32_t x = 0; x < w; ++x) {
            const int a = qAlpha(line[x]);
            if (a == 0) {
                continue;
            }
            mrow[x] = static_cast<uint8_t>(a);
            uint8_t *p = result.data() + (rowBase + static_cast<size_t>(x)) * 3;
            p[0] = static_cast<uint8_t>((qBlue(line[x]) * a + static_cast<int>(p[0]) * (255 - a)) / 255);
            p[1] = static_cast<uint8_t>((qGreen(line[x]) * a + static_cast<int>(p[1]) * (255 - a)) / 255);
            p[2] = static_cast<uint8_t>((qRed(line[x]) * a + static_cast<int>(p[2]) * (255 - a)) / 255);
        }
    }

    m_mask = std::move(newMask);
    m_selection = MaskBoundingRect();
    ApplySnapshot(MakeSnapshot(std::move(result), w, h, m_contentQuad), false, label);

    QString message;
    if (std::abs(sx - sy) < 1e-6) {
        message = QString("已自由变换选区内容：缩放 %1%").arg(sx * 100.0, 0, 'f', 1);
    } else {
        message = QString("已自由变换选区内容：缩放 %1% × %2%")
                      .arg(sx * 100.0, 0, 'f', 1)
                      .arg(sy * 100.0, 0, 'f', 1);
    }
    if (std::abs(angle) >= 0.05) {
        message += QString(" · 旋转 %1°").arg(angle, 0, 'f', 1);
    }
    UpdateStatus(message);
}

// 进入拖拽手柄变换：把选区内容抽成"刷白层 + 内容层"两张预览图交给覆盖层
void MainWindow::BeginInteractiveTransform()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || w <= 0 || h <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (!HasSelection() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区");
        return;
    }
    const QRect sel = m_selection.intersected(QRect(0, 0, w, h));
    if (sel.isEmpty()) {
        UpdateStatus("选区超出画布范围，无法变换");
        return;
    }
    CancelPreview();   // 变换基于已提交的图像

    QImage white(sel.width(), sel.height(), QImage::Format_ARGB32);
    QImage content(sel.width(), sel.height(), QImage::Format_ARGB32);
    white.fill(Qt::transparent);
    content.fill(Qt::transparent);
    for (int y = 0; y < sel.height(); ++y) {
        const int vy = sel.y() + y;
        const uint8_t *mrow = m_mask.data.data() + static_cast<size_t>(vy) * static_cast<size_t>(w);
        QRgb *wline = reinterpret_cast<QRgb *>(white.scanLine(y));
        QRgb *cline = reinterpret_cast<QRgb *>(content.scanLine(y));
        for (int x = 0; x < sel.width(); ++x) {
            const int vx = sel.x() + x;
            const int a = mrow[vx];
            if (a == 0) {
                continue;
            }
            wline[x] = qRgba(255, 255, 255, a);
            const size_t idx = (static_cast<size_t>(h - 1 - vy) * static_cast<size_t>(w) +
                                static_cast<size_t>(vx)) * 3;
            cline[x] = qRgba(imageData[idx + 2], imageData[idx + 1], imageData[idx], a);
        }
    }

    m_overlay->setToolActive(true);
    m_overlay->beginTransform(sel, white, content);
    m_overlay->setFocus();
    UpdateStatus("自由变换：拖拽角点缩放，拖拽顶部圆点旋转；回车提交，Esc 取消");
}

bool MainWindow::AnySelectionToolChecked() const
{
    const bool brush = (m_brushToolAction != nullptr) && m_brushToolAction->isChecked();
    const bool eraser = (m_eraserToolAction != nullptr) && m_eraserToolAction->isChecked();
    return m_selectionToolAction->isChecked() || m_lassoToolAction->isChecked() ||
           m_polyToolAction->isChecked() || m_magnetToolAction->isChecked() ||
           brush || eraser;
}

// 变换结束：提交走 ApplyFreeTransform，取消只退出会话(画布本来就没被改过)
void MainWindow::FinishInteractiveTransform(bool commit, double scaleX, double scaleY,
                                            double angleDegrees)
{
    if (m_overlay == nullptr || !m_overlay->isTransformActive()) {
        return;
    }
    m_overlay->endTransform();
    m_overlay->setToolActive(AnySelectionToolChecked());
    if (!commit) {
        UpdateStatus("已取消自由变换");
        return;
    }
    if (std::abs(scaleX - 1.0) < 1e-6 && std::abs(scaleY - 1.0) < 1e-6 &&
        std::abs(angleDegrees) < 1e-6) {
        UpdateStatus("自由变换未做改动");
        return;
    }
    ApplyFreeTransform(scaleX, scaleY, angleDegrees, QStringLiteral("自由变换"));
}

// 撤销/裁剪/切换工具导致的静默退出
void MainWindow::CancelInteractiveTransform()
{
    if (m_overlay != nullptr && m_overlay->isTransformActive()) {
        m_overlay->endTransform();
        m_overlay->setToolActive(AnySelectionToolChecked());
        UpdateStatus("已取消自由变换");
    }
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

    // ---- 蒙版菜单：面板按钮与"图层"菜单共用同一份动作 ----
    m_layerMaskMenu = new QMenu(QString("蒙版"), this);
    auto maskAction = [this](const QString &text, std::function<void()> handler) {
        QAction *action = m_layerMaskMenu->addAction(text);
        connect(action, &QAction::triggered, this, handler);
        return action;
    };
    m_maskFromSelectionAction = maskAction(QString("从选区添加蒙版"),
                                           [this]() { AddLayerMaskFromSelection(); });
    maskAction(QString("添加蒙版（全白）"), [this]() { AddLayerMaskFill(true); });
    maskAction(QString("添加蒙版（全黑）"), [this]() { AddLayerMaskFill(false); });
    m_layerMaskMenu->addSeparator();
    m_maskRevealSelectionAction = maskAction(QString("用选区显示"),
                                             [this]() { ApplySelectionToLayerMask(true); });
    m_maskHideSelectionAction = maskAction(QString("用选区隐藏"),
                                           [this]() { ApplySelectionToLayerMask(false); });
    m_maskInvertAction = maskAction(QString("蒙版反相"), [this]() { InvertLayerMask(); });
    m_layerMaskMenu->addSeparator();
    m_maskToggleAction = maskAction(QString("停用蒙版"), [this]() { ToggleLayerMask(); });
    m_maskRemoveAction = maskAction(QString("删除蒙版"), [this]() { RemoveLayerMask(); });
    // 弹出前按当前图层/选区刷新可用性，避免展示一堆无效命令
    connect(m_layerMaskMenu, &QMenu::aboutToShow, this, [this]() {
        const bool hasImage = !imageData.empty();
        const bool hasMask = hasImage && HasLayerMask(m_activeLayer);
        const bool hasSelection = hasImage && HasSelection();
        const bool maskEnabled = IsLayerMaskEnabled(m_activeLayer);
        if (m_maskFromSelectionAction != nullptr) {
            m_maskFromSelectionAction->setEnabled(hasSelection);
        }
        if (m_maskRevealSelectionAction != nullptr) {
            m_maskRevealSelectionAction->setEnabled(hasSelection);
        }
        if (m_maskHideSelectionAction != nullptr) {
            m_maskHideSelectionAction->setEnabled(hasSelection);
        }
        if (m_maskInvertAction != nullptr) {
            m_maskInvertAction->setEnabled(hasMask);
        }
        if (m_maskRemoveAction != nullptr) {
            m_maskRemoveAction->setEnabled(hasMask);
        }
        if (m_maskToggleAction != nullptr) {
            m_maskToggleAction->setEnabled(hasMask);
            m_maskToggleAction->setText(maskEnabled ? QString("停用蒙版")
                                                    : QString("启用蒙版"));
        }
    });

    QHBoxLayout *row3 = new QHBoxLayout();
    row3->setContentsMargins(0, 0, 0, 0);
    row3->setSpacing(6);
    QPushButton *maskButton = new QPushButton(QString("蒙版"), panel);
    maskButton->setObjectName("layerBtn");
    maskButton->setCursor(Qt::PointingHandCursor);
    maskButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    maskButton->setMenu(m_layerMaskMenu);
    row3->addWidget(maskButton);
    box->addLayout(row3);

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
    m_layerMenu->addSeparator();
    if (m_layerMaskMenu != nullptr) {
        m_layerMenu->addMenu(m_layerMaskMenu);   // 与图层面板的"蒙版"按钮共用动作
    }

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
        QString label = layer.name;
        if (!layer.mask.isEmpty()) {
            label += layer.maskEnabled ? QString("  [蒙版]") : QString("  [蒙版·停用]");
        }
        QListWidgetItem *item = new QListWidgetItem(label, m_layerList);
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

bool MainWindow::AnyLayerMaskEnabled() const
{
    for (const Layer &layer : m_layers) {
        if (layer.maskEnabled && !layer.mask.isEmpty()) {
            return true;
        }
    }
    return false;
}

const std::vector<uint8_t> &MainWindow::BuildComposite()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    // 单图层且没有生效蒙版时不需要合成，直接显示 imageData
    const bool needComposite = (m_layers.size() > 1) || AnyLayerMaskEnabled();
    if (!needComposite || w <= 0 || h <= 0 || imageData.empty()) {
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
        // 蒙版尺寸必须与画布一致；画布尺寸变化后由 MakeSnapshot 清理旧蒙版
        const SelectionMask *mask = nullptr;
        if (layer.maskEnabled && !layer.mask.isEmpty() &&
            layer.mask.width == w && layer.mask.height == h) {
            mask = &layer.mask;
        }
        BlendLayerInto(m_composite, src, layer.opacity, layer.blend, mask);
    }
    return m_composite;
}

const std::vector<uint8_t> &MainWindow::DisplayPixels()
{
    if (m_layers.size() > 1 || AnyLayerMaskEnabled()) {
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
    // 画布尺寸刚改变时(旋转/缩放/裁剪)，旧蒙版尺寸已不匹配，直接丢弃避免错位显示。
    // "操作前"的快照仍按旧尺寸保存蒙版，撤销回到旧尺寸时能完整恢复。
    for (Layer &layer : snap.layers) {
        if (!layer.mask.isEmpty() &&
            (layer.mask.width != width || layer.mask.height != height)) {
            layer.mask.clear();
        }
    }
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
    const bool multiLayer = m_layers.size() > 1;
    const bool hasMask = HasLayerMask(m_activeLayer);
    if (!multiLayer && !hasMask) {
        return true;
    }
    if (multiLayer) {
        UpdateStatus(opName + "：多图层下需要先合并图层");
        function.CreateMessagebox("提示",
                                  opName + " 会改变画布几何，多图层下暂不支持。\n"
                                  "请先执行「图层 → 合并所有图层」后再试。");
    } else {
        UpdateStatus(opName + "：带蒙版时需要先删除蒙版");
        function.CreateMessagebox("提示",
                                  opName + " 会翻转/改变画布几何，无法保持图层蒙版对齐。\n"
                                  "请先执行「图层 → 蒙版 → 删除蒙版」后再试。");
    }
    return false;
}

// 图层结构变化(新建/删除/复制/排序/合并)记一步历史
void MainWindow::CommitLayerStructure(const QString &title, const ImageSnapshot &before)
{
    if (imageData.empty()) {
        return;
    }
    if (m_activeLayer >= 0 && m_activeLayer < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    }
    ImageSnapshot snap = MakeSnapshot(imageData, myValue.bmpInfo.GetWidth(),
                                      myValue.bmpInfo.GetHeight(), m_contentQuad);
    ApplySnapshot(snap, false, title, &before);
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
    const ImageSnapshot before = CurrentSnapshot();
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
    CommitLayerStructure("新建图层", before);
}

void MainWindow::DuplicateLayer()
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    Layer copy = m_layers[static_cast<size_t>(m_activeLayer)];   // 连属性一起复制
    copy.name = copy.name + " 副本";
    const int insertAt = m_activeLayer + 1;
    m_layers.insert(m_layers.begin() + insertAt, std::move(copy));
    m_activeLayer = insertAt;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("复制图层", before);
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
    const ImageSnapshot before = CurrentSnapshot();
    m_layers.erase(m_layers.begin() + m_activeLayer);
    m_activeLayer = qBound(0, m_activeLayer, static_cast<int>(m_layers.size()) - 1);
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("删除图层", before);
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
    const ImageSnapshot before = CurrentSnapshot();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    // 合并成"看到的画面"(隐藏图层不参与)
    const std::vector<uint8_t> flat = BuildComposite();
    m_layers.clear();
    m_layers.push_back(Layer{QString("合并图层"), flat});
    m_activeLayer = 0;
    imageData = flat;
    UpdateLayerPanel();
    CommitLayerStructure("合并所有图层", before);
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
    const ImageSnapshot before = CurrentSnapshot();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    std::swap(m_layers[static_cast<size_t>(m_activeLayer)],
              m_layers[static_cast<size_t>(m_activeLayer + 1)]);
    ++m_activeLayer;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("上移图层", before);
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
    const ImageSnapshot before = CurrentSnapshot();
    m_layers[static_cast<size_t>(m_activeLayer)].data = imageData;
    std::swap(m_layers[static_cast<size_t>(m_activeLayer)],
              m_layers[static_cast<size_t>(m_activeLayer - 1)]);
    --m_activeLayer;
    imageData = m_layers[static_cast<size_t>(m_activeLayer)].data;
    UpdateLayerPanel();
    CommitLayerStructure("下移图层", before);
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

// ------------------------------------------------------------------
// 图层蒙版
// 蒙版与画布同尺寸、按视觉坐标存储；0 表示该像素被隐藏，255 表示完全显示。
// 蒙版的增删改都走 CommitLayerStructure，因此可以撤销/重做。
// ------------------------------------------------------------------

bool MainWindow::HasLayerMask(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_layers.size())) {
        return false;
    }
    return !m_layers[static_cast<size_t>(index)].mask.isEmpty();
}

bool MainWindow::IsLayerMaskEnabled(int index) const
{
    if (!HasLayerMask(index)) {
        return false;
    }
    return m_layers[static_cast<size_t>(index)].maskEnabled;
}

void MainWindow::AddLayerMaskFromSelection()
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (m_mask.isEmpty() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区，再添加图层蒙版");
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    layer.mask.width = w;
    layer.mask.height = h;
    layer.mask.data = m_mask.data;   // 选区内 255、选区外 0：只显示选区内容
    layer.maskEnabled = true;
    CommitLayerStructure("添加图层蒙版", before);
}

void MainWindow::AddLayerMaskFill(bool white)
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (w <= 0 || h <= 0) {
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    layer.mask.width = w;
    layer.mask.height = h;
    layer.mask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h),
                          white ? 255 : 0);
    layer.maskEnabled = true;
    CommitLayerStructure(white ? "添加全白蒙版" : "添加全黑蒙版", before);
}

void MainWindow::ApplySelectionToLayerMask(bool reveal)
{
    if (m_busy || imageData.empty()) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (m_mask.isEmpty() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区，再编辑图层蒙版");
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    // 还没有蒙版时先补一张全白蒙版，选区操作在其上叠加
    if (layer.mask.isEmpty() || layer.mask.width != w || layer.mask.height != h) {
        layer.mask.width = w;
        layer.mask.height = h;
        layer.mask.data.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 255);
    }
    for (size_t i = 0; i < layer.mask.data.size(); ++i) {
        const int a = m_mask.data[i];
        if (reveal) {
            // 显示选区：取较大值，羽化边缘也能平滑过渡
            layer.mask.data[i] = static_cast<uint8_t>(std::max<int>(layer.mask.data[i], a));
        } else {
            // 隐藏选区：按选区覆盖度衰减
            layer.mask.data[i] = static_cast<uint8_t>(
                static_cast<int>(layer.mask.data[i]) * (255 - a) / 255);
        }
    }
    layer.maskEnabled = true;
    CommitLayerStructure(reveal ? "蒙版显示选区" : "蒙版隐藏选区", before);
}

void MainWindow::InvertLayerMask()
{
    if (m_busy || !HasLayerMask(m_activeLayer)) {
        UpdateStatus("当前图层没有蒙版");
        return;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    if (layer.mask.width != w || layer.mask.height != h) {
        UpdateStatus("蒙版尺寸与画布不一致，请重新添加蒙版");
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    for (uint8_t &v : layer.mask.data) {
        v = static_cast<uint8_t>(255 - v);
    }
    CommitLayerStructure("蒙版反相", before);
}

void MainWindow::ToggleLayerMask()
{
    if (!HasLayerMask(m_activeLayer)) {
        UpdateStatus("当前图层没有蒙版");
        return;
    }
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    layer.maskEnabled = !layer.maskEnabled;
    // 与可见性/不透明度一致：显示属性变化不写历史，只重绘
    RepaintComposite();
    UpdateLayerPanel();
    UpdateStatus(layer.maskEnabled ? "已启用图层蒙版" : "已停用图层蒙版");
}

void MainWindow::RemoveLayerMask()
{
    if (m_busy || !HasLayerMask(m_activeLayer)) {
        UpdateStatus("当前图层没有蒙版");
        return;
    }
    CancelPreview();
    const ImageSnapshot before = CurrentSnapshot();
    Layer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    layer.mask.clear();
    layer.maskEnabled = true;
    CommitLayerStructure("删除图层蒙版", before);
}

// ------------------------------------------------------------------
// 画笔 / 橡皮
// 笔画先累积到一张 0~255 的覆盖度掩码(视觉坐标)，
// 松开时一次性按选区覆盖度与不透明度混入活动图层，避免重复采样加深。
// ------------------------------------------------------------------

void MainWindow::SetBrushParams(int size, int hardness, double opacity, const QColor &color)
{
    m_brushSize = qBound(1, size, 200);
    m_brushHardness = qBound(0, hardness, 100);
    m_brushOpacity = qBound(0.0, opacity, 1.0);
    if (color.isValid()) {
        m_brushColor = color;
    }
    UpdateBrushParams();
}

void MainWindow::UpdateBrushParams()
{
    if (m_overlay != nullptr) {
        m_overlay->setBrushParams(m_brushSize, m_brushHardness, m_brushOpacity, m_brushColor);
    }
}

void MainWindow::StampBrushCapsule(const QPoint &from, const QPoint &to)
{
    if (!m_brushStrokeActive || m_brushMaskW <= 0 || m_brushMaskH <= 0 ||
        m_brushMask.size() != static_cast<size_t>(m_brushMaskW) * m_brushMaskH) {
        return;
    }
    const double radius = std::max(0.5, m_brushSize / 2.0);
    const double inner = radius * (m_brushHardness / 100.0);
    const int pad = static_cast<int>(std::ceil(radius)) + 1;
    const int minX = std::max(0, std::min(from.x(), to.x()) - pad);
    const int maxX = std::min(m_brushMaskW - 1, std::max(from.x(), to.x()) + pad);
    const int minY = std::max(0, std::min(from.y(), to.y()) - pad);
    const int maxY = std::min(m_brushMaskH - 1, std::max(from.y(), to.y()) + pad);
    const double dx = to.x() - from.x();
    const double dy = to.y() - from.y();
    const double len2 = dx * dx + dy * dy;

    for (int y = minY; y <= maxY; ++y) {
        uint8_t *row = m_brushMask.data() + static_cast<size_t>(y) * m_brushMaskW;
        for (int x = minX; x <= maxX; ++x) {
            // 点到线段的距离(线段退化为点时就是点到点)
            double t = 0.0;
            if (len2 > 1e-9) {
                t = ((x - from.x()) * dx + (y - from.y()) * dy) / len2;
                t = qBound(0.0, t, 1.0);
            }
            const double px = from.x() + t * dx;
            const double py = from.y() + t * dy;
            const double dist = std::hypot(x - px, y - py);
            if (dist > radius) {
                continue;
            }
            double coverage = 255.0;
            if (dist > inner && radius > inner) {
                coverage = 255.0 * (radius - dist) / (radius - inner);
            }
            row[x] = std::max(row[x], static_cast<uint8_t>(qBound(0, static_cast<int>(coverage), 255)));
        }
    }
}

void MainWindow::BeginBrushStroke(const QPoint &point)
{
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        return;
    }
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    CancelPreview();   // 笔画基于已提交的像素
    m_brushBefore = CurrentSnapshot();
    m_brushErase = (m_eraserToolAction != nullptr) && m_eraserToolAction->isChecked();
    m_brushMaskW = myValue.bmpInfo.GetWidth();
    m_brushMaskH = myValue.bmpInfo.GetHeight();
    m_brushMask.assign(static_cast<size_t>(m_brushMaskW) * m_brushMaskH, 0);
    m_brushStrokeActive = true;
    StampBrushCapsule(point, point);
}

void MainWindow::BrushSegmentTo(const QPoint &from, const QPoint &to)
{
    if (m_brushStrokeActive) {
        StampBrushCapsule(from, to);
    }
}

// 把笔画掩码混入活动图层；返回是否真的改动了像素
bool MainWindow::ApplyPendingBrush()
{
    if (m_brushMaskW <= 0 || m_brushMaskH <= 0 || imageData.empty() ||
        m_brushMask.size() != static_cast<size_t>(m_brushMaskW) * m_brushMaskH) {
        return false;
    }
    const bool selValid = !m_mask.isEmpty() && m_mask.width == m_brushMaskW &&
                          m_mask.height == m_brushMaskH &&
                          m_mask.data.size() == m_brushMask.size();
    const int opacity = qBound(0, static_cast<int>(m_brushOpacity * 255.0 + 0.5), 255);
    if (opacity <= 0) {
        return false;
    }
    const uint8_t colorB = static_cast<uint8_t>(m_brushColor.blue());
    const uint8_t colorG = static_cast<uint8_t>(m_brushColor.green());
    const uint8_t colorR = static_cast<uint8_t>(m_brushColor.red());
    bool changed = false;
    const size_t rowBytes = static_cast<size_t>(m_brushMaskW) * 3;

    for (int y = 0; y < m_brushMaskH; ++y) {
        const size_t maskRow = static_cast<size_t>(y) * m_brushMaskW;
        const size_t off = static_cast<size_t>(m_brushMaskH - 1 - y) * rowBytes;
        for (int x = 0; x < m_brushMaskW; ++x) {
            int cov = m_brushMask[maskRow + x];
            if (cov == 0) {
                continue;
            }
            if (selValid) {
                cov = cov * m_mask.data[maskRow + x] / 255;
            }
            cov = cov * opacity / 255;
            if (cov <= 0) {
                continue;
            }
            const size_t i = off + static_cast<size_t>(x) * 3;
            for (int c = 0; c < 3; ++c) {
                const int target = m_brushErase ? 255
                                                : (c == 0 ? colorB : (c == 1 ? colorG : colorR));
                imageData[i + c] = static_cast<uint8_t>(
                    (static_cast<int>(imageData[i + c]) * (255 - cov) + target * cov) / 255);
            }
            changed = true;
        }
    }
    return changed;
}

void MainWindow::EndBrushStroke()
{
    if (!m_brushStrokeActive) {
        return;
    }
    m_brushStrokeActive = false;
    const bool changed = ApplyPendingBrush();
    std::vector<uint8_t>().swap(m_brushMask);
    m_brushMaskW = 0;
    m_brushMaskH = 0;
    if (!changed) {
        UpdateStatus("笔画没有产生改动");
        return;
    }
    CommitLayerStructure(m_brushErase ? QString("橡皮") : QString("画笔"), m_brushBefore);
}

void MainWindow::PaintBrushStroke(const QPoint &from, const QPoint &to, bool erase)
{
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    CancelPreview();
    m_brushBefore = CurrentSnapshot();
    m_brushErase = erase;
    m_brushMaskW = myValue.bmpInfo.GetWidth();
    m_brushMaskH = myValue.bmpInfo.GetHeight();
    m_brushMask.assign(static_cast<size_t>(m_brushMaskW) * m_brushMaskH, 0);
    m_brushStrokeActive = true;
    StampBrushCapsule(from, to);
    EndBrushStroke();
}

void MainWindow::SetupBrushPanel()
{
    QWidget *host = ui->group_layers->parentWidget();
    QVBoxLayout *hostLayout = (host != nullptr) ? qobject_cast<QVBoxLayout *>(host->layout())
                                                : nullptr;
    if (host == nullptr || hostLayout == nullptr) {
        return;
    }

    m_brushGroup = new QGroupBox(QString("画笔"), host);
    m_brushGroup->setObjectName("group_brush");
    QVBoxLayout *box = new QVBoxLayout(m_brushGroup);
    box->setContentsMargins(8, 10, 8, 8);
    box->setSpacing(6);

    auto addSliderRow = [this, box](const QString &name, int minValue, int maxValue,
                                    int value, const QString &suffix,
                                    QLabel **valueOut = nullptr) -> QSlider * {
        QWidget *row = new QWidget(m_brushGroup);
        QHBoxLayout *lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        QLabel *title = new QLabel(name, row);
        title->setMinimumWidth(42);
        QSlider *slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(minValue, maxValue);
        slider->setValue(value);
        QLabel *valueLabel = new QLabel(QString("%1%2").arg(value).arg(suffix), row);
        valueLabel->setObjectName("layerInfoLabel");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setMinimumWidth(44);
        lay->addWidget(title);
        lay->addWidget(slider, 1);
        lay->addWidget(valueLabel);
        box->addWidget(row);
        if (valueOut != nullptr) {
            *valueOut = valueLabel;
        }
        return slider;
    };

    QLabel *sizeValue = nullptr;
    QSlider *sizeSlider = addSliderRow(QString("大小"), 1, 200, m_brushSize,
                                       QString(" px"), &sizeValue);
    connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeValue](int v) {
        m_brushSize = v;
        UpdateBrushParams();
        if (sizeValue != nullptr) {
            sizeValue->setText(QString("%1 px").arg(v));
        }
    });

    QLabel *hardnessValue = nullptr;
    QSlider *hardnessSlider = addSliderRow(QString("硬度"), 0, 100, m_brushHardness,
                                           QString("%"), &hardnessValue);
    connect(hardnessSlider, &QSlider::valueChanged, this, [this, hardnessValue](int v) {
        m_brushHardness = v;
        UpdateBrushParams();
        if (hardnessValue != nullptr) {
            hardnessValue->setText(QString("%1%").arg(v));
        }
    });

    QLabel *opacityValue = nullptr;
    QSlider *opacitySlider = addSliderRow(QString("不透明"), 0, 100,
                                          qRound(m_brushOpacity * 100.0),
                                          QString("%"), &opacityValue);
    connect(opacitySlider, &QSlider::valueChanged, this, [this, opacityValue](int v) {
        m_brushOpacity = v / 100.0;
        UpdateBrushParams();
        if (opacityValue != nullptr) {
            opacityValue->setText(QString("%1%").arg(v));
        }
    });

    // 颜色
    QWidget *colorRow = new QWidget(m_brushGroup);
    QHBoxLayout *colorLay = new QHBoxLayout(colorRow);
    colorLay->setContentsMargins(0, 0, 0, 0);
    colorLay->setSpacing(8);
    QLabel *colorTitle = new QLabel(QString("颜色"), colorRow);
    colorTitle->setMinimumWidth(42);
    QPushButton *colorButton = new QPushButton(QString("选择…"), colorRow);
    colorButton->setObjectName("layerBtn");
    colorButton->setCursor(Qt::PointingHandCursor);
    QLabel *swatch = new QLabel(colorRow);
    swatch->setFixedSize(26, 18);
    swatch->setStyleSheet(QString("background:%1;border:1px solid #3b4455;border-radius:3px;")
                              .arg(m_brushColor.name()));
    colorLay->addWidget(colorTitle);
    colorLay->addWidget(colorButton);
    colorLay->addWidget(swatch, 1);
    box->addWidget(colorRow);
    connect(colorButton, &QPushButton::clicked, this, [this, swatch]() {
        const QColor color = QColorDialog::getColor(m_brushColor, this, QString("画笔颜色"));
        if (!color.isValid()) {
            return;
        }
        m_brushColor = color;
        swatch->setStyleSheet(
            QString("background:%1;border:1px solid #3b4455;border-radius:3px;")
                .arg(color.name()));
        UpdateBrushParams();
    });

    // 放在图层面板上方
    const int layerIndex = hostLayout->indexOf(ui->group_layers);
    hostLayout->insertWidget(layerIndex < 0 ? 0 : layerIndex, m_brushGroup);
    UpdateBrushParams();
}

// ------------------------------------------------------------------
// 渐变工具
// 线性：按拖拽方向投影；径向：以起点为圆心、拖拽长度为半径。
// 都按选区覆盖度与不透明度混入活动图层，提交为一步历史。
// ------------------------------------------------------------------

void MainWindow::SetGradientParams(bool radial, bool reverse, double opacity,
                                   const QColor &startColor, const QColor &endColor)
{
    m_gradientRadial = radial;
    m_gradientReverse = reverse;
    m_gradientOpacity = qBound(0.0, opacity, 1.0);
    if (startColor.isValid()) {
        m_gradientStartColor = startColor;
    }
    if (endColor.isValid()) {
        m_gradientEndColor = endColor;
    }
    UpdateGradientControls();
}

void MainWindow::UpdateGradientControls()
{
    const QString style = QString("background:%1;border:1px solid #3b4455;border-radius:3px;");
    if (m_gradientStartSwatch != nullptr) {
        m_gradientStartSwatch->setStyleSheet(style.arg(m_gradientStartColor.name()));
    }
    if (m_gradientEndSwatch != nullptr) {
        m_gradientEndSwatch->setStyleSheet(style.arg(m_gradientEndColor.name()));
    }
}

void MainWindow::ApplyGradientFill(const QPoint &from, const QPoint &to, bool radial)
{
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        UpdateStatus("请先打开一张图像");
        return;
    }
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    CancelPreview();   // 渐变基于已提交的像素

    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    const ImageSnapshot before = CurrentSnapshot();

    const bool selValid = !m_mask.isEmpty() && m_mask.width == w && m_mask.height == h &&
                          m_mask.data.size() == static_cast<size_t>(w) * static_cast<size_t>(h);
    int x0 = 0;
    int y0 = 0;
    int x1 = w - 1;
    int y1 = h - 1;
    if (selValid && !m_selection.isEmpty()) {
        const QRect r = m_selection.intersected(QRect(0, 0, w, h));
        if (r.isEmpty()) {
            UpdateStatus("选区不在画布内，渐变未生效");
            return;
        }
        x0 = r.left();
        y0 = r.top();
        x1 = r.right();
        y1 = r.bottom();
    }

    const double dx = to.x() - from.x();
    const double dy = to.y() - from.y();
    const double len2 = dx * dx + dy * dy;
    const double len = std::sqrt(len2);
    const int opacity = qBound(0, qRound(m_gradientOpacity * 255.0), 255);
    if (opacity <= 0) {
        UpdateStatus("渐变不透明度为 0，未产生改动");
        return;
    }
    const int startB = m_gradientStartColor.blue();
    const int startG = m_gradientStartColor.green();
    const int startR = m_gradientStartColor.red();
    const int endB = m_gradientEndColor.blue();
    const int endG = m_gradientEndColor.green();
    const int endR = m_gradientEndColor.red();

    bool changed = false;
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    for (int y = y0; y <= y1; ++y) {
        const size_t maskRow = static_cast<size_t>(y) * static_cast<size_t>(w);
        const size_t off = static_cast<size_t>(h - 1 - y) * rowBytes;
        for (int x = x0; x <= x1; ++x) {
            int cov = opacity;
            if (selValid) {
                cov = cov * m_mask.data[maskRow + static_cast<size_t>(x)] / 255;
            }
            if (cov <= 0) {
                continue;
            }
            double t = 0.0;
            if (len2 > 1e-9) {
                if (radial) {
                    t = std::hypot(x - from.x(), y - from.y()) / len;
                } else {
                    t = ((x - from.x()) * dx + (y - from.y()) * dy) / len2;
                }
            }
            t = qBound(0.0, t, 1.0);
            if (m_gradientReverse) {
                t = 1.0 - t;
            }
            const int target[3] = {
                qRound(startB + (endB - startB) * t),
                qRound(startG + (endG - startG) * t),
                qRound(startR + (endR - startR) * t),
            };
            const size_t i = off + static_cast<size_t>(x) * 3;
            for (int c = 0; c < 3; ++c) {
                const uint8_t value = static_cast<uint8_t>(
                    (static_cast<int>(imageData[i + c]) * (255 - cov) + target[c] * cov) / 255);
                if (imageData[i + c] != value) {
                    imageData[i + c] = value;
                    changed = true;
                }
            }
        }
    }

    if (!changed) {
        UpdateStatus("渐变没有产生改动");
        return;
    }
    CommitLayerStructure(radial ? QString("径向渐变") : QString("线性渐变"), before);
}

void MainWindow::SetupGradientPanel()
{
    QWidget *host = ui->group_layers->parentWidget();
    QVBoxLayout *hostLayout = (host != nullptr) ? qobject_cast<QVBoxLayout *>(host->layout())
                                                : nullptr;
    if (host == nullptr || hostLayout == nullptr) {
        return;
    }

    m_gradientGroup = new QGroupBox(QString("渐变"), host);
    m_gradientGroup->setObjectName("group_gradient");
    QVBoxLayout *box = new QVBoxLayout(m_gradientGroup);
    box->setContentsMargins(8, 10, 8, 8);
    box->setSpacing(6);

    // 类型
    QWidget *typeRow = new QWidget(m_gradientGroup);
    QHBoxLayout *typeLay = new QHBoxLayout(typeRow);
    typeLay->setContentsMargins(0, 0, 0, 0);
    typeLay->setSpacing(8);
    QLabel *typeTitle = new QLabel(QString("类型"), typeRow);
    typeTitle->setMinimumWidth(42);
    QComboBox *typeCombo = new QComboBox(typeRow);
    typeCombo->addItem(QString("线性"));
    typeCombo->addItem(QString("径向"));
    typeCombo->setCurrentIndex(m_gradientRadial ? 1 : 0);
    typeLay->addWidget(typeTitle);
    typeLay->addWidget(typeCombo, 1);
    box->addWidget(typeRow);
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_gradientRadial = (index == 1);
    });

    // 两端颜色
    auto addColorRow = [this, box](const QString &title, const QColor &initial,
                                   const QString &dialogTitle, QLabel **swatchOut,
                                   QColor *target) {
        QWidget *row = new QWidget(m_gradientGroup);
        QHBoxLayout *lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        QLabel *label = new QLabel(title, row);
        label->setMinimumWidth(42);
        QPushButton *button = new QPushButton(QString("选择…"), row);
        button->setObjectName("layerBtn");
        button->setCursor(Qt::PointingHandCursor);
        QLabel *swatch = new QLabel(row);
        swatch->setFixedSize(26, 18);
        swatch->setStyleSheet(
            QString("background:%1;border:1px solid #3b4455;border-radius:3px;").arg(initial.name()));
        lay->addWidget(label);
        lay->addWidget(button);
        lay->addWidget(swatch, 1);
        box->addWidget(row);
        if (swatchOut != nullptr) {
            *swatchOut = swatch;
        }
        connect(button, &QPushButton::clicked, this, [this, target, dialogTitle]() {
            const QColor color = QColorDialog::getColor(*target, this, dialogTitle);
            if (!color.isValid()) {
                return;
            }
            *target = color;
            UpdateGradientControls();
        });
        return swatch;
    };
    addColorRow(QString("起始色"), m_gradientStartColor, QString("渐变起始色"),
                &m_gradientStartSwatch, &m_gradientStartColor);
    addColorRow(QString("结束色"), m_gradientEndColor, QString("渐变结束色"),
                &m_gradientEndSwatch, &m_gradientEndColor);

    // 反向
    QCheckBox *reverseBox = new QCheckBox(QString("反向"), m_gradientGroup);
    reverseBox->setChecked(m_gradientReverse);
    box->addWidget(reverseBox);
    connect(reverseBox, &QCheckBox::toggled, this, [this](bool on) { m_gradientReverse = on; });

    // 不透明度
    QWidget *opacityRow = new QWidget(m_gradientGroup);
    QHBoxLayout *opacityLay = new QHBoxLayout(opacityRow);
    opacityLay->setContentsMargins(0, 0, 0, 0);
    opacityLay->setSpacing(8);
    QLabel *opacityTitle = new QLabel(QString("不透明"), opacityRow);
    opacityTitle->setMinimumWidth(42);
    QSlider *opacitySlider = new QSlider(Qt::Horizontal, opacityRow);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(qRound(m_gradientOpacity * 100.0));
    QLabel *opacityValue = new QLabel(QString("%1%").arg(qRound(m_gradientOpacity * 100.0)),
                                      opacityRow);
    opacityValue->setObjectName("layerInfoLabel");
    opacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opacityValue->setMinimumWidth(44);
    opacityLay->addWidget(opacityTitle);
    opacityLay->addWidget(opacitySlider, 1);
    opacityLay->addWidget(opacityValue);
    box->addWidget(opacityRow);
    connect(opacitySlider, &QSlider::valueChanged, this, [this, opacityValue](int v) {
        m_gradientOpacity = v / 100.0;
        opacityValue->setText(QString("%1%").arg(v));
    });

    // 放在画笔面板上方(画笔面板可能尚未创建)
    int insertIndex = hostLayout->indexOf(ui->group_layers);
    if (m_brushGroup != nullptr) {
        const int brushIndex = hostLayout->indexOf(m_brushGroup);
        if (brushIndex >= 0) {
            insertIndex = brushIndex;
        }
    }
    hostLayout->insertWidget(insertIndex < 0 ? 0 : insertIndex, m_gradientGroup);
    UpdateGradientControls();
}

std::vector<uint8_t> MainWindow::CompositePixels()
{
    if (m_layers.size() <= 1 && !AnyLayerMaskEnabled()) {
        return imageData;
    }
    return BuildComposite();
}

// ------------------------------------------------------------------
// 选区通道
// 通道保存的是选区掩码本身，不随图层/像素编辑变化；随文档一起切换。
// 存储/载入/删除属于文档级操作，不写入图像历史(与选区本身的编辑一致)。
// ------------------------------------------------------------------

QString MainWindow::ChannelName(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_channels.size())) {
        return QString();
    }
    return m_channels[static_cast<size_t>(index)].name;
}

bool MainWindow::StoreSelectionChannel(const QString &name)
{
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return false;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || m_mask.isEmpty() || m_mask.width != w || m_mask.height != h) {
        UpdateStatus("请先建立选区，再存储为通道");
        return false;
    }
    QString finalName = name.trimmed();
    if (finalName.isEmpty()) {
        finalName = QString("通道 %1").arg(static_cast<int>(m_channels.size()) + 1);
    }
    SelectionChannel channel;
    channel.name = finalName;
    channel.mask = m_mask;   // 拷贝一份，后续选区变化不影响已存通道
    m_channels.push_back(std::move(channel));
    UpdateChannelPanel();
    UpdateStatus(QString("已存储选区通道「%1」").arg(finalName));
    return true;
}

bool MainWindow::LoadSelectionChannel(int index)
{
    if (index < 0 || index >= static_cast<int>(m_channels.size())) {
        return false;
    }
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    const SelectionChannel &channel = m_channels[static_cast<size_t>(index)];
    if (imageData.empty() || channel.mask.width != w || channel.mask.height != h) {
        UpdateStatus(QString("通道「%1」与当前画布尺寸不一致，无法载入").arg(channel.name));
        return false;
    }
    SetSelectionMask(channel.mask);
    UpdateStatus(QString("已载入通道「%1」").arg(channel.name));
    return HasSelection();
}

bool MainWindow::DeleteSelectionChannel(int index)
{
    if (index < 0 || index >= static_cast<int>(m_channels.size())) {
        return false;
    }
    const QString name = m_channels[static_cast<size_t>(index)].name;
    m_channels.erase(m_channels.begin() + index);
    UpdateChannelPanel();
    UpdateStatus(QString("已删除通道「%1」").arg(name));
    return true;
}

void MainWindow::PromptStoreChannel()
{
    if (!HasSelection()) {
        UpdateStatus("请先建立选区");
        return;
    }
    bool ok = false;
    const QString defaultName = QString("通道 %1").arg(static_cast<int>(m_channels.size()) + 1);
    const QString name = QInputDialog::getText(this, QString("存储选区"),
                                               QString("通道名称："), QLineEdit::Normal,
                                               defaultName, &ok);
    if (ok) {
        StoreSelectionChannel(name);
    }
}

void MainWindow::SetupChannelPanel()
{
    QWidget *host = ui->group_layers->parentWidget();
    QVBoxLayout *hostLayout = (host != nullptr) ? qobject_cast<QVBoxLayout *>(host->layout())
                                                : nullptr;
    if (host == nullptr || hostLayout == nullptr) {
        return;
    }

    m_channelGroup = new QGroupBox(QString("通道"), host);
    m_channelGroup->setObjectName("group_channels");
    QVBoxLayout *box = new QVBoxLayout(m_channelGroup);
    box->setContentsMargins(8, 10, 8, 8);
    box->setSpacing(6);

    m_channelList = new QListWidget(m_channelGroup);
    m_channelList->setObjectName("channelList");
    m_channelList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_channelList->setMinimumHeight(70);
    m_channelList->setMaximumHeight(110);
    m_channelList->setToolTip(QString("选中的通道可点「载入」重新套用为选区"));
    box->addWidget(m_channelList);

    QHBoxLayout *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    auto addButton = [this, group = m_channelGroup](QHBoxLayout *lay, const QString &text,
                                                    std::function<void()> handler) {
        QPushButton *btn = new QPushButton(text, group);
        btn->setObjectName("layerBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(btn, &QPushButton::clicked, this, handler);
        lay->addWidget(btn);
    };
    addButton(row, QString("存储"), [this]() { PromptStoreChannel(); });
    addButton(row, QString("载入"), [this]() {
        const int rowIndex = (m_channelList != nullptr) ? m_channelList->currentRow() : -1;
        if (rowIndex < 0) {
            UpdateStatus("请先选择一个通道");
            return;
        }
        LoadSelectionChannel(rowIndex);
    });
    addButton(row, QString("删除"), [this]() {
        const int rowIndex = (m_channelList != nullptr) ? m_channelList->currentRow() : -1;
        if (rowIndex < 0) {
            UpdateStatus("请先选择一个通道");
            return;
        }
        DeleteSelectionChannel(rowIndex);
    });
    box->addLayout(row);

    // 双击通道直接载入
    connect(m_channelList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) {
                if (item != nullptr) {
                    LoadSelectionChannel(m_channelList->row(item));
                }
            });

    // 放在图层面板下方、底部弹簧之前
    const int layerIndex = hostLayout->indexOf(ui->group_layers);
    hostLayout->insertWidget(layerIndex < 0 ? 0 : layerIndex + 1, m_channelGroup);
    UpdateChannelPanel();
}

void MainWindow::UpdateChannelPanel()
{
    if (m_channelList == nullptr) {
        return;
    }
    const int keepRow = m_channelList->currentRow();
    QSignalBlocker blocker(m_channelList);
    m_channelList->clear();
    for (const SelectionChannel &channel : m_channels) {
        m_channelList->addItem(channel.name);
    }
    if (!m_channels.empty()) {
        m_channelList->setCurrentRow(qBound(0, keepRow, static_cast<int>(m_channels.size()) - 1));
    }
}

void MainWindow::PruneChannelsForCanvas()
{
    const int32_t w = myValue.bmpInfo.GetWidth();
    const int32_t h = myValue.bmpInfo.GetHeight();
    const size_t before = m_channels.size();
    m_channels.erase(
        std::remove_if(m_channels.begin(), m_channels.end(),
                       [w, h](const SelectionChannel &channel) {
                           return channel.mask.width != w || channel.mask.height != h;
                       }),
        m_channels.end());
    if (m_channels.size() != before) {
        UpdateChannelPanel();
    }
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
        { ui->lineEdit_sharpen,    "让边缘更清晰",     0.0,   200.0, &MakeSharpenOp },
        { ui->lineEdit_highlight,  "压住过曝",   0.0,   255.0, &MakeHighlightOp },
        { ui->lineEdit_threshold,  "黑白分界",   0.0,   255.0, &MakeThresholdOp },
        { ui->lineEdit_shadow,     "提亮暗部",   0.0,   100.0, &MakeShadowOp },
        { ui->lineEdit_brightness, "亮度",   -150.0, 150.0, &MakeBrightnessOp },
        { ui->lineEdit_contrast,   "对比度",  -50.0, 100.0, &MakeContrastOp },
        { ui->lineEdit_saturation, "饱和度", -100.0, 100.0, &MakeSaturationOp },
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

    // 每个参数行下面挂一条滑块：拖滑块写数值框(触发实时预览)，
    // 手动改数值框时滑块同步跟随；数值框同时承担数值显示。
    struct SliderDesk {
        QWidget *row;
        QLineEdit *edit;
        const char *name;
        int minimum;
        int maximum;
        int initial;
    };
    const SliderDesk sliderDesks[] = {
        { ui->widget_4,  ui->lineEdit_brightness, "slider_brightness", -150, 150, 0 },
        { ui->widget_5,  ui->lineEdit_contrast,   "slider_contrast",    -50, 100, 0 },
        { ui->widget_6,  ui->lineEdit_saturation, "slider_saturation", -100, 100, 0 },
        { ui->widget_21, ui->lineEdit_shadow,     "slider_shadow",         0, 100, 0 },
        { ui->widget_9,  ui->lineEdit_highlight,  "slider_highlight",      0, 255, 128 },
        { ui->widget_8,  ui->lineEdit_sharpen,    "slider_sharpen",        0, 200, 0 },
        { ui->widget_10, ui->lineEdit_threshold,  "slider_threshold",      0, 255, 128 },
        { ui->widget_17, ui->lineEdit_rotate,     "slider_rotate",         0, 360, 0 },
        { ui->widget_18, ui->lineEdit_rotate_r,   "slider_rotate_r",       0, 360, 0 },
    };
    for (const SliderDesk &d : sliderDesks) {
        QSlider *slider = SliderField::Make(ui->widget_14, d.minimum, d.maximum, d.initial);
        slider->setObjectName(QString::fromLatin1(d.name));
        if (!SliderField::InsertAfter(d.row, slider)) {
            continue;
        }
        QSignalBlocker blocker(d.edit);   // 初始化数值不算用户输入，不触发预览
        d.edit->setText(QString::number(d.initial));
        SliderField::Bind(slider, d.edit);
    }

    // 旋转角度也做实时预览：拖滑块/改数值时先试转，点"应用"才真正提交
    auto connectRotatePreview = [this](QLineEdit *edit, double sign) {
        connect(edit, &QLineEdit::textChanged, this, [this, edit, sign]() {
            bool ok = false;
            const double value = edit->text().trimmed().toDouble(&ok);
            if (!ok || value < 0.0 || value > 360.0) {
                CancelPreview();
                return;
            }
            PreviewRotation(sign * value);
        });
        edit->installEventFilter(this);   // Esc 取消预览
    };
    connectRotatePreview(ui->lineEdit_rotate, 1.0);
    connectRotatePreview(ui->lineEdit_rotate_r, -1.0);

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
    // 多图层或生效中的图层蒙版下，实时预览会与"只改活动图层"的提交结果不一致，直接跳过预览
    if (m_layers.size() > 1 || AnyLayerMaskEnabled()) {
        return;
    }
    m_pendingFieldIndex = fieldIndex;
    m_customPreview = false;   // 面板输入优先，丢掉之前挂起的旋转/弹窗预览
    m_pendingCustomOp = nullptr;
    m_pendingCustomTitle.clear();
    m_previewTimer->start();   // 防抖：连续输入时不断重新计时
}

// 通用算子预览：对话框/旋转调参数时在【已提交的图像】上试算，只刷新画面
void MainWindow::PreviewCustomOp(const QString &title,
                                 std::function<void(std::vector<uint8_t> &, OpContext &)> op)
{
    if (imageData.empty() || m_busy || m_layers.size() > 1 || AnyLayerMaskEnabled() || !op) {
        CancelPreview();
        return;
    }
    m_customPreview = true;
    m_pendingCustomOp = std::move(op);
    m_pendingCustomTitle = title;
    m_pendingFieldIndex = -1;
    m_previewTimer->start(150);   // 拖动滑块时不必每次都算，停手 150ms 后再预览
}

// 旋转预览：直接在(必要时缩小的)当前图像上试转一个角度。
// 提交时仍然走 ApplyRotation 的"无损基准"路径，预览不会影响旋转精度。
void MainWindow::PreviewRotation(double deltaDegrees)
{
    if (!std::isfinite(deltaDegrees)) {
        CancelPreview();
        return;
    }
    double angle = std::fmod(deltaDegrees, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    PreviewCustomOp(QString("旋转"), [angle](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.RotateImage(d, ctx.width, ctx.height, angle, ctx.quad);
    });
}

// 在【已提交的图像】上跑一次算子，只刷新画面，不写历史
void MainWindow::RunPreview()
{
    if (m_previewRunning) {
        m_previewQueued = true;   // 上一次还没算完，等它结束后再算最新输入
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        CancelPreview();
        return;
    }
    if (m_layers.size() > 1 || AnyLayerMaskEnabled()) {   // 多图层/有蒙版不做实时预览
        CancelPreview();
        return;
    }

    // 预览来源有两种：右侧输入框(按字段索引) / 通用算子(旋转、弹窗参数)
    const bool custom = m_customPreview;
    double value = 0.0;
    OpFn op;
    int index = -2;
    QString title;
    if (custom) {
        if (!m_pendingCustomOp) {
            CancelPreview();
            return;
        }
        op = m_pendingCustomOp;
        title = m_pendingCustomTitle;
    } else {
        if (m_pendingFieldIndex < 0 || m_pendingFieldIndex >= static_cast<int>(m_previewFields.size())) {
            return;
        }
        const PreviewField &field = m_previewFields[static_cast<size_t>(m_pendingFieldIndex)];
        const ReturnValue rv = CheckOK(field.edit);
        // 输入不完整/非法/越界：静默回到已提交的画面(提交时会给出提示)
        if (rv.isNull || !rv.isNumeric || rv.value < field.min || rv.value > field.max) {
            CancelPreview();
            return;
        }
        value = rv.value;
        op = field.makeOp(value);
        index = m_pendingFieldIndex;
        title = field.title;
    }

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
    const SelectionMask mask = m_mask;   // 预览也只在选区内生效，与提交一致
    const int gen = ++m_previewGeneration;
    m_previewRunning = true;

    auto result = std::make_shared<ImageSnapshot>();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
            [this, watcher, result, gen, value, index, downsampled, workW, workH, width, height,
             custom, title]() {
        watcher->deleteLater();
        m_previewRunning = false;
        // 世代号已过期 → 这条结果对应的是旧输入，直接丢弃
        // 旋转等几何算子会改变画布尺寸，低分辨率预览不再要求尺寸一致(仅用于显示)
        const bool usable = gen == m_previewGeneration && !result->data.empty() &&
                            (!downsampled || custom ||
                             (result->width == workW && result->height == workH));
        if (usable) {
            m_previewValue = value;
            m_previewFieldIndex = index;
            m_previewActive = true;
            m_previewDownsampled = downsampled;
            if (downsampled) {
                // 低分辨率结果只用来显示，不能提交
                m_previewResult = ImageSnapshot();
                m_previewImage = ImageOps::ToDisplayImage(result->data, result->width, result->height);
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
            if (custom) {
                UpdateStatus(QString("预览中：%1%2")
                                 .arg(title)
                                 .arg(downsampled ? "（低分辨率快速预览）" : ""));
            } else {
                UpdateStatus(QString("预览中：%1 = %2%3")
                                 .arg(m_previewFields[static_cast<size_t>(index)].title)
                                 .arg(value)
                                 .arg(downsampled ? "（低分辨率快速预览）" : ""));
            }
        }
        if (m_previewQueued) {
            m_previewQueued = false;
            RunPreview();
        }
    });

    watcher->setFuture(QtConcurrent::run([source, op, workW, workH, quad, result, mask, step]() mutable {
        std::vector<uint8_t> work = source;
        OpContext ctx;
        ctx.width = workW;
        ctx.height = workH;
        ctx.pixelCount = work.size() / 3;
        ctx.numThreads = 1;
        ctx.segmentSize = ctx.pixelCount;
        ctx.quad = quad;
        if (step > 1) {
            // 缩略图预览：内容区域坐标要按步长同比缩放，否则旋转会按原图坐标裁切
            for (int i = 0; i < 4; ++i) {
                ctx.quad.x[i] /= step;
                ctx.quad.y[i] /= step;
            }
        }
        op(work, ctx);
        // 缩略图预览需把掩码按步长抽样；选区外恢复原像素
        if (!mask.isEmpty() && ctx.width == workW && ctx.height == workH) {
            const SelectionMask scaled = MainWindow::DownsampleMask(mask, step);
            RestoreOutsideSelection(work, source, ctx.width, ctx.height, scaled);
        }
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
    m_customPreview = false;
    m_pendingCustomOp = nullptr;
    m_pendingCustomTitle.clear();
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
    m_customPreview = false;
    m_pendingCustomOp = nullptr;
    m_pendingCustomTitle.clear();
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
    ApplySnapshot(snap, false, title);
    UpdateStatus(title + " 已应用");
    return true;
}

void MainWindow::ResetAll(BitmapImage &myValue)
{
    // 重置前的状态作为一条全局历史(重置后可以撤销回来)
    const ImageSnapshot before = imageData.empty() ? ImageSnapshot() : CurrentSnapshot();
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
    const QImage img = ImageOps::ToDisplayImage(imageData,
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
    InvalidateRotateBase();
    PushHistory("还原成原图", before);
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

// 当前活动文档的完整快照
ImageSnapshot MainWindow::CurrentSnapshot() const
{
    return MakeSnapshot(imageData, myValue.bmpInfo.GetWidth(),
                        myValue.bmpInfo.GetHeight(), m_contentQuad);
}

// 写入一条全局历史：撤销栈保存"操作前状态"，并清空重做栈
void MainWindow::PushHistory(const QString &label, const ImageSnapshot &before)
{
    if (m_activeDoc < 0 || m_activeDoc >= static_cast<int>(m_docs.size())) {
        return;
    }
    if (SameSnapshot(before, CurrentSnapshot())) {
        return;
    }
    HistoryEntry entry;
    entry.docId = m_docs[static_cast<size_t>(m_activeDoc)].id;
    entry.label = label;
    entry.snap = before;
    imageDataHistory.push_back(std::move(entry));
    redoImageDataHistory.clear();
    // 控制历史长度，避免大图下内存无限增长
    while (static_cast<int>(imageDataHistory.size()) > m_historyLimit) {
        imageDataHistory.pop_front();
    }
    UpdateHistoryPanel();
}

// 应用一个快照：先把"操作前状态"压入全局历史，再写进当前文档
void MainWindow::ApplySnapshot(const ImageSnapshot &snap, bool keepRotateBase,
                               const QString &label, const ImageSnapshot *beforeOverride)
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
    const ImageSnapshot before = beforeOverride ? *beforeOverride : CurrentSnapshot();
    // 先写入新状态再记历史：这样 PushHistory 的“与当前状态相同”判断
    // 比较的是操作前后的差异，空操作才不会被误记成一步
    ApplyStateSnapshot(snap, keepRotateBase);
    PushHistory(label, before);
}

// 把快照内容写进当前文档并刷新显示(撤销/重做直接复用，不再写历史)
void MainWindow::ApplyStateSnapshot(const ImageSnapshot &snap, bool keepRotateBase)
{
    // 只有旋转结果才继续沿用无损基准；其它算子改的是像素本身，
    // 基准必须换成新的当前图像，否则后续旋转会丢掉这些修改
    if (!keepRotateBase) {
        InvalidateRotateBase();
    }
    m_contentQuad = snap.quad;
    // 尺寸可能已被算子改变(例如旋转)，同步到文件头信息
    myValue.bmpInfo.SetWidth(snap.width);
    myValue.bmpInfo.SetHeight(snap.height);
    myValue.bmpInfo.SetImageSize(static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    myValue.bmp.SetFileSize(sizeof(BMP) + sizeof(BMPInfo) +
                            static_cast<uint32_t>(snap.width) * static_cast<uint32_t>(snap.height) * 3);
    RestoreSnapshot(snap);
    PruneChannelsForCanvas();   // 画布尺寸变了，旧通道尺寸不再匹配
    ShowImage(imageData, myValue, snap.width, snap.height);
    UpdateLayerPanel();
    UpdateHistoryPanel();
}


void MainWindow::UndoImageProcessing()
{
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    CancelPreview();
    if (imageDataHistory.empty()) {
        UpdateStatus("已经是最早的状态了");
        return;
    }
    // 目标文档可能已被关闭：跳过它的记录，直到找到仍在的文档
    HistoryEntry entry;
    bool found = false;
    while (!imageDataHistory.empty()) {
        entry = std::move(imageDataHistory.back());
        imageDataHistory.pop_back();
        if (ActivateDocumentById(entry.docId)) {
            found = true;
            break;
        }
    }
    if (!found) {
        UpdateHistoryPanel();
        UpdateStatus("已经是最早的状态了");
        return;
    }
    // 当前(操作后)状态压入重做栈
    redoImageDataHistory.push_back(HistoryEntry{entry.docId, entry.label, CurrentSnapshot()});
    ApplyStateSnapshot(entry.snap, false);
    InvalidateRotateBase();   // 撤销后以当前图像作为新的旋转基准
    UpdateHistoryPanel();
    UpdateStatus("已撤销：" + entry.label);
}

void MainWindow::RedoImageProcessing()
{
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    CancelPreview();
    if (redoImageDataHistory.empty()) {
        UpdateStatus("没有可重做的操作");
        return;
    }
    HistoryEntry entry;
    bool found = false;
    while (!redoImageDataHistory.empty()) {
        entry = std::move(redoImageDataHistory.back());
        redoImageDataHistory.pop_back();
        if (ActivateDocumentById(entry.docId)) {
            found = true;
            break;
        }
    }
    if (!found) {
        UpdateHistoryPanel();
        UpdateStatus("没有可重做的操作");
        return;
    }
    // 当前(操作前)状态压回撤销栈
    imageDataHistory.push_back(HistoryEntry{entry.docId, entry.label, CurrentSnapshot()});
    while (static_cast<int>(imageDataHistory.size()) > m_historyLimit) {
        imageDataHistory.pop_front();
    }
    ApplyStateSnapshot(entry.snap, false);
    InvalidateRotateBase();   // 重做后以当前图像作为新的旋转基准
    UpdateHistoryPanel();
    UpdateStatus("已重做：" + entry.label);
}

// 刷新右侧"历史记录"面板：跨文档的全局历史
void MainWindow::UpdateHistoryPanel()
{
    if (ui->historyList == nullptr) {
        return;
    }
    // 重建列表期间屏蔽信号，避免 setCurrentRow 反过来触发点击槽
    const QSignalBlocker blocker(ui->historyList);
    ui->historyList->clear();

    // 撤销栈：从旧到新
    for (const HistoryEntry &entry : imageDataHistory) {
        ui->historyList->addItem(QString("%1 · %2")
                                     .arg(DocumentDisplayName(entry.docId), entry.label));
    }

    // 当前状态
    const QString currentName = (m_activeDoc >= 0 && m_activeDoc < static_cast<int>(m_docs.size()))
                                    ? DocumentDisplayName(m_docs[static_cast<size_t>(m_activeDoc)].id)
                                    : QString("未命名");
    QListWidgetItem *current = new QListWidgetItem(QString("▶ 当前：%1").arg(currentName));
    QFont font = current->font();
    font.setBold(true);
    current->setFont(font);
    ui->historyList->addItem(current);

    // 重做栈：从最近到最远，灰色显示
    for (auto it = redoImageDataHistory.rbegin(); it != redoImageDataHistory.rend(); ++it) {
        QListWidgetItem *entry = new QListWidgetItem(
            QString("↪ 待重做：%1 · %2").arg(DocumentDisplayName(it->docId), it->label));
        entry->setForeground(QBrush(QColor(140, 140, 140)));
        ui->historyList->addItem(entry);
    }

    ui->historyList->setCurrentRow(-1);
    ui->historyList->scrollToBottom();
}

// 点击历史项：连续撤销/重做到目标步(撤销栈存操作前状态，当前项位于栈顶之后)
void MainWindow::on_historyList_itemClicked(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }
    const int target = ui->historyList->row(item);
    const int undoCount = static_cast<int>(imageDataHistory.size());
    if (target < undoCount) {
        // 撤销到仅剩 target 条(该行对应的状态)
        while (static_cast<int>(imageDataHistory.size()) > target) {
            const size_t before = imageDataHistory.size();
            UndoImageProcessing();
            if (imageDataHistory.size() == before) {
                break;   // 无法继续撤销(忙/无可用文档)，避免死循环
            }
        }
    } else if (target > undoCount) {
        const int steps = target - undoCount;
        for (int i = 0; i < steps; ++i) {
            const size_t before = imageDataHistory.size();
            RedoImageProcessing();
            if (imageDataHistory.size() == before) {
                break;   // 无法继续重做，避免死循环
            }
        }
    }
}

// 打开图片文件并初始化界面状态（BMP 走自有解析器，其它格式由 Qt 插件解码）
bool MainWindow::LoadImage(const QString &path)
{
    CancelPreview();   // 打开新图前先回滚未提交的预览
    BitmapImage loaded;
    QString errMsg;
    if (!BmpIO::ReadImageFile(path.toStdString(), loaded, errMsg)) {
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
    doc.id = m_nextDocId++;
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
    myValue = BitmapImage();
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
    m_mask.clear();
    m_channels.clear();
    m_layers.clear();
    m_activeLayer = 0;
    m_composite.clear();
    UpdateSelectionOverlay();
    UpdateLayerPanel();
    UpdateChannelPanel();
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
        cur.mask = std::move(m_mask);
        cur.channels = std::move(m_channels);
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
    m_mask = std::move(doc.mask);
    m_channels = std::move(doc.channels);
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
    UpdateChannelPanel();
    UpdateTabBar();
}

// 按文档 id 切换到目标文档(全局撤销/重做用)
bool MainWindow::ActivateDocumentById(int docId)
{
    for (size_t i = 0; i < m_docs.size(); ++i) {
        if (m_docs[i].id != docId) {
            continue;
        }
        if (static_cast<int>(i) == m_activeDoc) {
            return true;
        }
        ActivateDocument(static_cast<int>(i));
        return true;
    }
    return false;
}

// 历史面板/状态提示用的文档名
QString MainWindow::DocumentDisplayName(int docId) const
{
    for (const Document &doc : m_docs) {
        if (doc.id != docId) {
            continue;
        }
        const QString name = QFileInfo(doc.path).fileName();
        return name.isEmpty() ? QString("未命名 %1").arg(docId) : name;
    }
    return QString("已关闭的文档");
}

// 文档关闭后清理它名下的全局历史
void MainWindow::PurgeHistoryForDocument(int docId)
{
    const auto match = [docId](const HistoryEntry &entry) { return entry.docId == docId; };
    imageDataHistory.erase(std::remove_if(imageDataHistory.begin(), imageDataHistory.end(), match),
                           imageDataHistory.end());
    redoImageDataHistory.erase(
        std::remove_if(redoImageDataHistory.begin(), redoImageDataHistory.end(), match),
        redoImageDataHistory.end());
    UpdateHistoryPanel();
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

    const int closingId = m_docs[static_cast<size_t>(index)].id;
    const bool wasActive = (index == m_activeDoc);
    if (wasActive) {
        ResetDocumentState();
        m_activeDoc = -1;
    }
    m_docs.erase(m_docs.begin() + index);
    PurgeHistoryForDocument(closingId);
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
        BmpIO::SetBMPHeaderValues(outBmp, outInfo, width, height, outInfo.GetBitsPerPixel());
        if (BmpIO::WriteBMPFile(filePath.toStdString(), pixels, outBmp, outInfo)) {
            BMPPath = filePath.toStdString();
            UpdateTabBar();
            UpdateStatus(QString("已保存到 %1").arg(QFileInfo(filePath).fileName()));
        } else {
            function.CreateMessagebox("保存失败", "无法写入文件，请检查路径是否有效");
        }
        return;
    }

    // 其它格式：交给 Qt 图像插件（PNG / JPEG / TIFF / WebP）
    const QImage image = ImageOps::ToDisplayImage(pixels, width, height);
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
                               std::function<void()> onDone,
                               bool restrictSelection)
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0 || myValue.bmpInfo.GetHeight() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张图片");
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
    // 有选区时只让调整类算子作用于选区(几何变换不走这条限制)
    const SelectionMask mask = (restrictSelection && !fromRotation) ? m_mask : SelectionMask();
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
            ApplySnapshot(*result, fromRotation, title);
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
    watcher->setFuture(QtConcurrent::run([source, op, ctx, result, mask, inWidth, inHeight]() mutable {
        std::vector<uint8_t> work = source;
        op(work, ctx);              // 算子可以通过 ctx 修改画布宽高与内容区域(例如旋转)
        // 尺寸不变时按掩码把选区外还原为输入像素，从而"只处理选区"
        if (!mask.isEmpty() && ctx.width == inWidth && ctx.height == inHeight) {
            RestoreOutsideSelection(work, source, ctx.width, ctx.height, mask);
        }
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
        function.CreateMessagebox("提示", "请先打开一张图片");
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
            ImageOps f;
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
        },
        false);
}

// 子窗口结果写回主窗口
void MainWindow::ApplyResult(std::vector<uint8_t> &result, const QString &label)
{
    const int32_t width = myValue.bmpInfo.GetWidth();
    const int32_t height = myValue.bmpInfo.GetHeight();
    if (result.empty() || width <= 0 || height <= 0 ||
        result.size() != static_cast<size_t>(width) * height * 3) {
        UpdateStatus("处理结果与当前图像尺寸不一致，已忽略");
        return;
    }
    if (!m_contentQuad.isValid()) {
        m_contentQuad.setRect(width, height);
    }
    // 有选区时只把结果写回选区内，选区外保持原样
    if (!m_mask.isEmpty()) {
        RestoreOutsideSelection(result, imageData, width, height, m_mask);
    }
    // 子窗口只改像素、不改画布几何；结果写回当前活动图层
    ImageSnapshot snap = MakeSnapshot(result, width, height, m_contentQuad);
    ApplySnapshot(snap, false, label);
    UpdateStatus("已应用处理结果");
}

// ------------------------------------------------------------------
// 算子槽函数
// ------------------------------------------------------------------

void MainWindow::on_actiongray_triggered()
{
    ApplyOperator("转为黑白", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
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
    ApplyOperator("轻微模糊", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
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
    if (CommitPreview("亮度", ui->lineEdit_brightness, value)) {
        return;
    }
    ApplyOperator("亮度", MakeBrightnessOp(value));
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
    if (CommitPreview("对比度", ui->lineEdit_contrast, value)) {
        return;
    }
    ApplyOperator("对比度", MakeContrastOp(value));
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
    if (CommitPreview("饱和度", ui->lineEdit_saturation, value)) {
        return;
    }
    ApplyOperator("饱和度", MakeSaturationOp(value));
}

void MainWindow::on_actioncolorBalance_triggered()
{
    ApplyOperator("校正偏色", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
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
    ApplyOperator("反相", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.InvertColors(d, s, e);
        });
    });
}

void MainWindow::on_actioncomplementary_triggered()
{
    ApplyOperator("互补色", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        RunSegmented(ctx, [this, &d](size_t s, size_t e) {
            this->function.Complementary(d, s, e);
        });
    });
}

void MainWindow::on_actioneyeZoom_triggered()
{
    qDebug() << "I am in a EyeZoom window!";
    EyeZoom *myEye = new EyeZoom(this, myValue);
    myEye->show();
}

void MainWindow::on_actionretouch_triggered()
{
    qDebug() << "I am in a retouch window!";
    Retouch *retouch = new Retouch(this, myValue);
    retouch->show();
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
    NumberDialog dlg(QString("高斯模糊"),
                     { NumberParam{QString("强度"), 0, 10, 0, 0.1, 1} }, this);
    connect(&dlg, &NumberDialog::paramsChanged, this, [this, &dlg]() {
        const double degree = dlg.value(0);
        PreviewCustomOp(QString("高斯模糊"),
                        [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
                            ImageOps f;
                            std::promise<std::vector<uint8_t>> promise;
                            std::future<std::vector<uint8_t>> future = promise.get_future();
                            f.Gauss(d, promise, ctx.width, ctx.height, degree);
                            d = future.get();
                        });
    });
    if (dlg.exec() != QDialog::Accepted) {
        CancelPreview();
        return;
    }
    CancelPreview();
    const double degree = dlg.value(0);
    ApplyOperator("高斯模糊", [degree](std::vector<uint8_t> &d, const OpContext &ctx) {
        ImageOps f;
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
    CancelPreview();   // 预览结果不能直接当提交结果，按无损基准重新计算
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
    CancelPreview();   // 预览结果不能直接当提交结果，按无损基准重新计算
    ApplyRotation(-angle, "逆时针旋转");
}

void MainWindow::on_actionmedian_triggered()
{
    qDebug() << "median";
    ApplyOperator("去除杂点", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.MedianBlur(d, static_cast<uint32_t>(ctx.width), static_cast<uint32_t>(ctx.height));
    });
}

void MainWindow::on_actionmosaic_triggered()
{
    NumberDialog dlg(QString("打马赛克"),
                     { NumberParam{QString("强度"), 1, 100, 4, 1, 0} }, this);
    connect(&dlg, &NumberDialog::paramsChanged, this, [this, &dlg]() {
        const uint32_t step = static_cast<uint32_t>(std::max(1.0, dlg.value(0)));
        PreviewCustomOp(QString("打马赛克"),
                        [step](std::vector<uint8_t> &d, const OpContext &ctx) {
                            ImageOps f;
                            f.FullMosaic(d, static_cast<uint32_t>(ctx.width),
                                         static_cast<uint32_t>(ctx.height), step);
                        });
    });
    if (dlg.exec() != QDialog::Accepted) {
        CancelPreview();
        return;
    }
    CancelPreview();
    const uint32_t step = static_cast<uint32_t>(std::max(1.0, dlg.value(0)));
    ApplyOperator("打马赛克", [this, step](std::vector<uint8_t> &d, const OpContext &ctx) {
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
    if (CommitPreview("提亮暗部", ui->lineEdit_shadow, value)) {
        return;
    }
    ApplyOperator("提亮暗部", MakeShadowOp(value));
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
    if (CommitPreview("压住过曝", ui->lineEdit_highlight, value)) {
        return;
    }
    ApplyOperator("压住过曝", MakeHighlightOp(value));
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
    if (CommitPreview("让边缘更清晰", ui->lineEdit_sharpen, amount)) {
        return;
    }
    ApplyOperator("让边缘更清晰", MakeSharpenOp(amount));
}

void MainWindow::on_actionsobel_triggered()
{
    ApplyOperator("描出轮廓", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
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
    if (CommitPreview("黑白分界", ui->lineEdit_threshold, threshold)) {
        return;
    }
    ApplyOperator("黑白分界", MakeThresholdOp(threshold));
}

void MainWindow::on_actionscale_triggered()
{
    if (m_busy) {
        UpdateStatus("正在处理，请稍候…");
        return;
    }
    const int32_t oldW = myValue.bmpInfo.GetWidth();
    const int32_t oldH = myValue.bmpInfo.GetHeight();
    if (imageData.empty() || oldW <= 0 || oldH <= 0) {
        UpdateStatus("请先打开一张图片");
        return;
    }
    if (!RequireSingleLayer("缩放图像")) {
        return;
    }
    CancelPreview();

    ScaleDialog dlg(oldW, oldH, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QSize target = dlg.targetSize();
    if (target.width() == oldW && target.height() == oldH) {
        function.CreateMessagebox("提示", "尺寸没有变化");
        return;
    }

    // 0 快速 / 1 平滑 / 2 最佳：Qt 没有"最佳"，用平滑代替
    const Qt::TransformationMode mode =
        (dlg.algorithm() == 0) ? Qt::FastTransformation : Qt::SmoothTransformation;

    const QImage img = ImageOps::ToDisplayImage(imageData, oldW, oldH);
    if (img.isNull()) {
        UpdateStatus("当前图像数据无效，无法缩放");
        return;
    }
    const QImage scaled = img.scaled(target, Qt::IgnoreAspectRatio, mode);
    std::vector<uint8_t> out = BgrFromQImage(scaled);
    const int32_t newW = target.width();
    const int32_t newH = target.height();
    if (out.size() != static_cast<size_t>(newW) * static_cast<size_t>(newH) * 3) {
        UpdateStatus("缩放结果无效，已取消");
        return;
    }

    ContentQuad quad = m_contentQuad;
    if (!quad.isValid()) {
        quad.setRect(oldW, oldH);
    }
    const double scaleX = static_cast<double>(newW) / static_cast<double>(oldW);
    const double scaleY = static_cast<double>(newH) / static_cast<double>(oldH);
    for (int i = 0; i < 4; ++i) {
        quad.x[i] *= scaleX;
        quad.y[i] *= scaleY;
    }

    ApplySnapshot(MakeSnapshot(std::move(out), newW, newH, quad), false, "缩放图像");
    UpdateStatus(QString("已缩放为 %1×%2").arg(newW).arg(newH));
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
    ui->toolBox->setVisible(set);
    ui->menu_edit->menuAction()->setVisible(set);
    ui->menu_blur->menuAction()->setVisible(set);
    ui->menu_base->menuAction()->setVisible(set);
    ui->menu_unbase->menuAction()->setVisible(set);
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

// ------------------------------------------------------------------
// P2-A：几何变换 / 增强 / 形态学 / 卷积 / USM
// ------------------------------------------------------------------
void MainWindow::on_actionflipH_triggered()
{
    if (!RequireSingleLayer("水平翻转")) {
        return;
    }
    ApplyOperator("水平翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.FlipImage(d, ctx.width, ctx.height, ImageOps::FlipMode::Horizontal, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actionflipV_triggered()
{
    if (!RequireSingleLayer("垂直翻转")) {
        return;
    }
    ApplyOperator("垂直翻转", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.FlipImage(d, ctx.width, ctx.height, ImageOps::FlipMode::Vertical, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actionrotate90_triggered()
{
    if (!RequireSingleLayer("旋转")) {
        return;
    }
    ApplyOperator("向右转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.RotateImage(d, ctx.width, ctx.height, 90.0, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actionrotate90ccw_triggered()
{
    if (!RequireSingleLayer("旋转")) {
        return;
    }
    ApplyOperator("向左转 90°", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.RotateImage(d, ctx.width, ctx.height, 270.0, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actiontranspose_triggered()
{
    if (!RequireSingleLayer("转置")) {
        return;
    }
    ApplyOperator("沿对角线翻转（左上-右下）", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.FlipImage(d, ctx.width, ctx.height, ImageOps::FlipMode::Transpose, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actionantitranspose_triggered()
{
    if (!RequireSingleLayer("反转置")) {
        return;
    }
    ApplyOperator("沿对角线翻转（右上-左下）", [](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.FlipImage(d, ctx.width, ctx.height, ImageOps::FlipMode::AntiTranspose, ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actiontailor_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (!RequireSingleLayer("裁剪图像")) {
        return;
    }
    if (imageData.empty() || myValue.bmpInfo.GetWidth() <= 0) {
        function.CreateMessagebox("提示", "请先打开一张图片");
        return;
    }
    const QImage display = ImageOps::ToDisplayImage(imageData,
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
    ApplyOperator("裁剪图像", [sel](std::vector<uint8_t> &d, OpContext &ctx) {
        ImageOps f;
        f.CropImage(d, ctx.width, ctx.height,
                    sel.x(), sel.y(), sel.width(), sel.height(), ctx.quad);
    }, nullptr, nullptr, false);
}

void MainWindow::on_actionequalize_triggered()
{
    ApplyOperator("自动拉大明暗", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        uint8_t lut[256];
        this->function.BuildEqualizeLUT(d, lut);
        RunSegmented(ctx, [this, &d, &lut](size_t s, size_t e) {
            this->function.ApplyLUT(d, lut, s, e);
        });
    });
}

void MainWindow::on_actionerode_triggered()
{
    ApplyOperator("去小亮点", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  ImageOps::MorphType::Erode, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actiondilate_triggered()
{
    ApplyOperator("补小暗点", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  ImageOps::MorphType::Dilate, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionopenMorph_triggered()
{
    ApplyOperator("先消后补", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  ImageOps::MorphType::Open, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actioncloseMorph_triggered()
{
    ApplyOperator("先补后消", [this](std::vector<uint8_t> &d, const OpContext &ctx) {
        this->function.Morphology(d, ctx.width, ctx.height,
                                  ImageOps::MorphType::Close, 0, ctx.pixelCount);
    });
}

void MainWindow::on_actionconvolution_triggered()
{
    if (m_busy) {
        UpdateStatus("上一个操作还在处理中，请稍候");
        return;
    }
    if (imageData.empty()) {
        function.CreateMessagebox("提示", "请先打开一张图片");
        return;
    }
    KernelDialog dlg(this);
    connect(&dlg, &KernelDialog::paramsChanged, this, [this, &dlg]() {
        const std::vector<double> kernel = dlg.kernel();
        const double scale = dlg.scale();
        const double offset = dlg.offset();
        PreviewCustomOp(QString("自定义滤镜"),
                        [this, kernel, scale, offset](std::vector<uint8_t> &d, const OpContext &ctx) {
                            std::vector<uint8_t> out;
                            this->function.Convolve3x3(d, out, ctx.width, ctx.height,
                                                       kernel.data(), scale, offset, 0, ctx.pixelCount);
                            if (!out.empty()) {
                                d.swap(out);
                            }
                        });
    });
    if (dlg.exec() != QDialog::Accepted) {
        CancelPreview();
        return;
    }
    CancelPreview();
    const std::vector<double> kernel = dlg.kernel();
    const double scale = dlg.scale();
    const double offset = dlg.offset();
    ApplyOperator("自定义滤镜", [this, kernel, scale, offset](std::vector<uint8_t> &d, const OpContext &ctx) {
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
        function.CreateMessagebox("提示", "请先打开一张图片");
        return;
    }
    USMDialog dlg(this);
    connect(&dlg, &USMDialog::paramsChanged, this, [this, &dlg]() {
        const double radius = dlg.radius();
        const double amount = dlg.amount();
        const double threshold = dlg.threshold();
        PreviewCustomOp(QString("智能锐化"),
                        [this, radius, amount, threshold](std::vector<uint8_t> &d, const OpContext &ctx) {
                            std::vector<uint8_t> out;
                            this->function.USMSharpen(d, out, ctx.width, ctx.height,
                                                      radius, amount, threshold);
                            if (!out.empty() && out.size() == d.size()) {
                                d.swap(out);
                            }
                        });
    });
    if (dlg.exec() != QDialog::Accepted) {
        CancelPreview();
        return;
    }
    CancelPreview();
    const double radius = dlg.radius();
    const double amount = dlg.amount();
    const double threshold = dlg.threshold();
    ApplyOperator("智能锐化", [this, radius, amount, threshold](std::vector<uint8_t> &d, const OpContext &ctx) {
        std::vector<uint8_t> out;
        this->function.USMSharpen(d, out, ctx.width, ctx.height, radius, amount, threshold);
        if (!out.empty() && out.size() == d.size()) {
            d.swap(out);
        }
    });
}
