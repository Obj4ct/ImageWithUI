#include "selftest.h"
#include "BMPFile.h"
#include "Function.h"
#include "mainwindow.h"
#include "eye.h"
#include <QFileInfo>
#include <QTextStream>
#include <QEventLoop>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QMouseEvent>
#include <QTabBar>
#include <cstdio>

namespace {

// 同时输出到控制台和 UTF-8 报告文件，避免控制台编码问题
class Reporter {
public:
    Reporter() {
        m_file.setFileName(QDir::tempPath() + "/imagewithui_selftest.txt");
        m_file.open(QIODevice::WriteOnly | QIODevice::Text);
        m_stream.setDevice(&m_file);
        m_stream.setCodec("UTF-8");
    }
    ~Reporter() {
        m_stream.flush();
    }
    Reporter &operator<<(const QString &text) {
        printf("%s", text.toLocal8Bit().constData());
        m_stream << text;
        m_stream.flush();
        fflush(stdout);
        return *this;
    }
    Reporter &operator<<(const char *text) {
        return operator<<(QString::fromUtf8(text));
    }
    template <typename T>
    Reporter &operator<<(const T &value) {
        return operator<<(QString::number(value));
    }
    void flush() {
        m_stream.flush();
        fflush(stdout);
    }
private:
    QFile m_file;
    QTextStream m_stream;
};

Reporter &out()
{
    static Reporter r;
    return r;
}

// 统计"内容像素"数量(非纯黑像素)，用于判断图像是否被裁掉
static size_t countContent(const std::vector<uint8_t> &data)
{
    size_t n = 0;
    for (size_t i = 0; i + 2 < data.size(); i += 3) {
        if (data[i] != 0 || data[i + 1] != 0 || data[i + 2] != 0) {
            ++n;
        }
    }
    return n;
}

int g_pass = 0;
int g_fail = 0;

// 校验一份算子输出：尺寸必须一致，且不能是全黑/全白的死数据
void check(const QString &name, const std::vector<uint8_t> &data,
           size_t expectBytes, bool expectChanged = true,
           const std::vector<uint8_t> *source = nullptr)
{
    bool ok = true;
    QString detail;
    if (data.size() != expectBytes) {
        ok = false;
        detail = QString("尺寸不符：期望 %1 实际 %2").arg(expectBytes).arg(data.size());
    } else {
        size_t zero = 0, full = 0;
        for (uint8_t v : data) {
            if (v == 0) ++zero;
            if (v == 255) ++full;
        }
        if (data.empty() || zero == data.size()) {
            ok = false;
            detail = "输出全为 0";
        } else if (full == data.size()) {
            ok = false;
            detail = "输出全为 255";
        } else if (expectChanged && source && *source == data) {
            ok = false;
            detail = "输出与输入完全相同(算子未生效)";
        }
    }
    if (ok) {
        ++g_pass;
        out() << "[PASS] " << name << "  " << detail << "\n";
    } else {
        ++g_fail;
        out() << "[FAIL] " << name << "  " << detail << "\n";
    }
    out().flush();
}

// 构造运行目录下的候选样例路径
QString resolveImage(const QString &given)
{
    if (!given.isEmpty() && QFileInfo::exists(given)) {
        return given;
    }
    const QStringList candidates = {
        QStringLiteral("resources/image/Lenna.bmp"),
        QStringLiteral("../resources/image/Lenna.bmp"),
        QStringLiteral("../../resources/image/Lenna.bmp"),
        QStringLiteral("E:/ImageWithUI/ImageWithUI/resources/image/Lenna.bmp"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) {
            return c;
        }
    }
    return QString();
}

} // namespace

namespace Selftest {

int Run(const QString &imagePath)
{
    out() << "================ ImageWithUI 自检 ================\n";
    out().flush();

    // ---------- 1. 非法文件必须优雅报错，不能崩溃 ----------
    {
        const QString bogus = QDir::tempPath() + "/imagewithui_bogus.bmp";
        QFile f(bogus);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("NOT-A-BMP-FILE");
        f.close();
        MyValue v;
        QString err;
        const bool ok = MYFunction::ReadBMPFile(bogus.toStdString(), v, err);
        if (!ok && !err.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 非法文件被正确拒绝 : " << err << "\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 非法文件未被拒绝\n";
        }
        out().flush();
        QFile::remove(bogus);
    }

    // ---------- 2. 读取样例图 ----------
    const QString path = resolveImage(imagePath);
    if (path.isEmpty()) {
        ++g_fail;
        out() << "[FAIL] 找不到样例图片(可用 --selftest <路径> 指定)\n";
        out().flush();
        return 1;
    }

    MyValue base;
    QString err;
    if (!MYFunction::ReadBMPFile(path.toStdString(), base, err)) {
        ++g_fail;
        out() << "[FAIL] 读取样例图失败 : " << err << "\n";
        out().flush();
        return 1;
    }
    ++g_pass;
    out() << "[PASS] 读取样例图 " << QFileInfo(path).fileName()
          << "  " << base.bmpInfo.GetWidth() << "x" << base.bmpInfo.GetHeight()
          << "  字节 " << base.imageData.size() << "\n";
    out().flush();

    const int32_t w = base.bmpInfo.GetWidth();
    const int32_t h = base.bmpInfo.GetHeight();
    const size_t bytes = base.imageData.size();
    const size_t pixels = bytes / 3;
    const int threads = 5;
    const size_t seg = pixels / threads;

    Function fn;
    auto copyOf = [&base]() { return base.imageData; };

    // ---------- 3. 分段算子(按像素分段，检查通道是否错位) ----------
    {
        auto data = copyOf();
        std::vector<std::thread> workers;
        for (int i = 0; i < threads; ++i) {
            const size_t s = static_cast<size_t>(i) * seg;
            const size_t e = (i == threads - 1) ? pixels : std::min(pixels, s + seg);
            workers.emplace_back([&fn, &data, s, e]() { fn.ConvertToGray(data, s, e); });
        }
        for (auto &t : workers) t.join();
        // 灰度后三通道必须相等(若分段按字节切开会不相等)
        bool channelsEqual = true;
        for (size_t p = 0; p + 2 < data.size(); p += 3) {
            if (data[p] != data[p + 1] || data[p + 1] != data[p + 2]) {
                channelsEqual = false;
                break;
            }
        }
        if (channelsEqual) {
            ++g_pass;
            out() << "[PASS] 分段灰度 : 通道对齐正确\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 分段灰度 : 通道错位(分段未按像素对齐)\n";
        }
        out().flush();
    }

    // ---------- 4. 逐个算子 ----------
    {
        auto data = copyOf();
        fn.InvertColors(data, 0, pixels);
        check("颜色反转", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.Complementary(data, 0, pixels);
        check("补色", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        const float_t aver = static_cast<float_t>(fn.CalAver(data));
        const float_t stdv = static_cast<float_t>(fn.CalStandard(data, aver));
        fn.AutoContrast(data, aver, stdv, 0, pixels);
        check("自动对比度", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.AverageBlur(data, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0, pixels);
        check("均值模糊", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.MedianBlur(data, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        check("中值滤波", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        std::promise<std::vector<uint8_t>> p;
        auto f = p.get_future();
        fn.Gauss(data, p, w, h, 1.0);
        const auto blurred = f.get();
        check("高斯模糊", blurred, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        std::promise<std::vector<uint8_t>> p1, p2;
        auto f1 = p1.get_future();
        auto f2 = p2.get_future();
        fn.Gauss(data, p1, w, h, 1.0);
        const auto blurred = f1.get();
        fn.HighContrast(data, p2, blurred);
        const auto high = f2.get();
        check("高反差保留", high, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        std::promise<std::vector<uint8_t>> p1, p2, p3;
        auto f1 = p1.get_future();
        auto f2 = p2.get_future();
        auto f3 = p3.get_future();
        fn.Gauss(data, p1, w, h, 1.0);
        const auto blurred = f1.get();
        fn.HighContrast(data, p2, blurred);
        const auto high = f2.get();
        fn.Sharpen(data, p3, high);
        const auto sharp = f3.get();
        check("锐化", sharp, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        const auto edge = fn.SobelEdge(data, w, h);
        check("Sobel 边缘检测", edge, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.ApplyThreshold(data, 128.0, 0, pixels);
        check("阈值处理", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.FullMosaic(data, static_cast<uint32_t>(w), static_cast<uint32_t>(h), 8);
        check("马赛克", data, bytes, true, &base.imageData);
    }
    {
        // 90 度整数倍：必须精确无损(转 90 再转 270 应与原图逐字节相同)
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(rw, rh);
        fn.RotateImage(data, rw, rh, 90.0, quad);
        const bool sizeSwapped = (rw == h && rh == w);
        fn.RotateImage(data, rw, rh, 270.0, quad);
        const bool lossless = (data == base.imageData) && (rw == w && rh == h);
        if (sizeSwapped && lossless) {
            ++g_pass;
            out() << "[PASS] 旋转 90 度 : 尺寸互换且逐像素无损\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 旋转 90 度 : 尺寸互换=" << (sizeSwapped ? "Y" : "N")
                  << " 无损=" << (lossless ? "Y" : "N") << "\n";
        }
        out().flush();
    }
    {
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(rw, rh);
        fn.RotateReverse(data, rw, rh, 30.0, quad);
        const bool ok = (data.size() == static_cast<size_t>(rw) * rh * 3) && !data.empty();
        if (ok) {
            ++g_pass;
            out() << "[PASS] 逆时针旋转 : 输出 " << rw << "x" << rh << "\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 逆时针旋转 : 尺寸异常\n";
        }
        out().flush();
    }
    {
        // 回归测试：连续旋转 12 次(共 360 度)，内容不能被逐渐裁掉
        // (旧实现每次都会把超出画布的部分变成黑色，反复旋转后图像会退化成圆形)
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(rw, rh);
        for (int i = 0; i < 12; ++i) {
            fn.RotateImage(data, rw, rh, 30.0, quad);
        }
        {
            // 落盘一份旋转结果，便于人工核对
            BMP rb = base.bmp;
            BMPInfo ri = base.bmpInfo;
            MYFunction::SetBMPHeaderValues(rb, ri, rw, rh, ri.GetBitsPerPixel());
            MYFunction::WriteBMPFile((QDir::tempPath() + "/imagewithui_rot30.bmp").toStdString(), data, rb, ri);
        }
        const double ratio = static_cast<double>(countContent(data)) /
                             static_cast<double>(std::max<size_t>(1, countContent(base.imageData)));
        const double areaRatio = (static_cast<double>(rw) * rh) / (static_cast<double>(w) * h);
        if (ratio > 0.85 && areaRatio < 4.0) {
            ++g_pass;
            out() << "[PASS] 连续旋转 12 次 : 内容保留率 " << int(ratio * 100)
                  << "%, 画布面积 " << int(areaRatio * 100) << "%\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 连续旋转 12 次 : 内容保留率 " << int(ratio * 100)
                  << "%, 画布面积 " << int(areaRatio * 100) << "% (应 >85% 且 <400%)\n";
        }
        out().flush();
    }
    {
        auto data = copyOf();
        std::promise<std::vector<uint8_t>> p;
        auto f = p.get_future();
        fn.Fisheye(data, p, w, h);
        const auto result = f.get();
        check("鱼眼效果", result, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.Brightness(data, 30.0);
        check("亮度调整", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.Contrast(data, 20.0);
        check("对比度调整", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.Saturation(data, w, h, 1.5);
        check("饱和度调整", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        fn.ColorBalance(data, w, h);
        check("色彩平衡", data, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        std::vector<uint8_t> shadow;
        fn.MakeShadow(data, shadow, 30.0);
        check("阴影调整", shadow, bytes, true, &base.imageData);
    }
    {
        auto data = copyOf();
        std::vector<uint8_t> hl;
        fn.HighLight(data, hl, 100.0);
        check("高光调整", hl, bytes, true, &base.imageData);
    }
    // 边界参数：不能崩溃
    {
        auto data = copyOf();
        fn.ApplyThreshold(data, 0.0, 0, pixels);
        fn.AutoContrast(data, 100.0f, 0.0f, 0, pixels);
        std::promise<std::vector<uint8_t>> p;
        auto f = p.get_future();
        fn.Gauss(data, p, w, h, 0.0);
        const auto blurred = f.get();
        check("边界参数(阈值0/标准差0/sigma0)", blurred, bytes, false, nullptr);
    }

    // ---------- 4.5 P2-A 新增算子 ----------
    auto expect = [](const QString &name, bool ok, const QString &detail) {
        if (ok) {
            ++g_pass;
            out() << "[PASS] " << name << "  " << detail << "\n";
        } else {
            ++g_fail;
            out() << "[FAIL] " << name << "  " << detail << "\n";
        }
        out().flush();
    };
    {
        // 水平翻转两次必须逐字节复原
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Horizontal, quad);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Horizontal, quad);
        expect("水平翻转两次复原", data == base.imageData && rw == w && rh == h, "");
    }
    {
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Vertical, quad);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Vertical, quad);
        expect("垂直翻转两次复原", data == base.imageData && rw == w && rh == h, "");
    }
    {
        // 转置：宽高互换，两次转置复原
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Transpose, quad);
        const bool swapped = (rw == h && rh == w);
        fn.FlipImage(data, rw, rh, Function::FlipMode::Transpose, quad);
        expect("转置两次复原", swapped && data == base.imageData && rw == w && rh == h,
               swapped ? "" : "宽高未交换");
    }
    {
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.FlipImage(data, rw, rh, Function::FlipMode::AntiTranspose, quad);
        fn.FlipImage(data, rw, rh, Function::FlipMode::AntiTranspose, quad);
        expect("副对角镜像两次复原", data == base.imageData && rw == w && rh == h, "");
    }
    {
        // 裁剪：尺寸正确，且新图左上角(视觉)对应源图 (x,y)
        auto data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        const int32_t cx = w / 4, cy = h / 4;
        const int32_t cw = w / 2, ch = h / 2;
        fn.CropImage(data, rw, rh, cx, cy, cw, ch, quad);
        const size_t srcIdx = (static_cast<size_t>(h - 1 - cy) * w + cx) * 3;
        const size_t dstIdx = static_cast<size_t>(ch - 1) * cw * 3;
        const bool dimOk = (rw == cw && rh == ch && data.size() == static_cast<size_t>(cw) * ch * 3);
        const bool pixOk = dimOk && data[dstIdx] == base.imageData[srcIdx] &&
                           data[dstIdx + 1] == base.imageData[srcIdx + 1] &&
                           data[dstIdx + 2] == base.imageData[srcIdx + 2];
        expect("裁剪尺寸与像素映射", dimOk && pixOk,
               QString("%1x%2 -> %3x%4").arg(w).arg(h).arg(rw).arg(rh));
    }
    {
        // 直方图均衡化：尺寸不变、内容改变，且亮度直方图熵上升(分布更均匀)
        auto data = copyOf();
        fn.HistogramEqualize(data, w, h);
        auto lumEntropy = [](const std::vector<uint8_t> &d) {
            double hist[256] = {0};
            size_t n = 0;
            for (size_t i = 0; i + 2 < d.size(); i += 3) {
                int g = static_cast<int>(0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2]);
                g = std::max(0, std::min(255, g));
                hist[g] += 1.0;
                ++n;
            }
            double entropy = 0.0;
            if (n == 0) {
                return entropy;
            }
            for (int v = 0; v < 256; ++v) {
                if (hist[v] > 0) {
                    const double p = hist[v] / static_cast<double>(n);
                    entropy -= p * std::log2(p);
                }
            }
            return entropy;
        };
        const double e0 = lumEntropy(base.imageData);
        const double e1 = lumEntropy(data);
        expect("直方图均衡化", data.size() == bytes && data != base.imageData && e1 > e0 + 0.05,
               QString("亮度熵 %1 -> %2").arg(e0, 0, 'f', 3).arg(e1, 0, 'f', 3));
    }
    {
        // 腐蚀：逐通道只会变小或不变
        auto data = copyOf();
        fn.Morphology(data, w, h, Function::MorphType::Erode, 0, pixels);
        bool notGreater = true;
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] > base.imageData[i]) {
                notGreater = false;
                break;
            }
        }
        expect("腐蚀(只减不增)", data.size() == bytes && data != base.imageData && notGreater, "");
    }
    {
        auto data = copyOf();
        fn.Morphology(data, w, h, Function::MorphType::Dilate, 0, pixels);
        bool notLess = true;
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] < base.imageData[i]) {
                notLess = false;
                break;
            }
        }
        expect("膨胀(只增不减)", data.size() == bytes && data != base.imageData && notLess, "");
    }
    {
        auto data = copyOf();
        fn.Morphology(data, w, h, Function::MorphType::Open, 0, pixels);
        expect("开运算", data.size() == bytes && data != base.imageData, "");
    }
    {
        auto data = copyOf();
        fn.Morphology(data, w, h, Function::MorphType::Close, 0, pixels);
        expect("闭运算", data.size() == bytes && data != base.imageData, "");
    }
    {
        // 恒等核必须逐字节不改变图像
        auto data = copyOf();
        double identity[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
        std::vector<uint8_t> out;
        fn.Convolve3x3(data, out, w, h, identity, 1.0, 0.0, 0, pixels);
        expect("卷积：恒等核", out == base.imageData, "");
    }
    {
        auto data = copyOf();
        double sharpen[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
        std::vector<uint8_t> out;
        fn.Convolve3x3(data, out, w, h, sharpen, 1.0, 0.0, 0, pixels);
        check("卷积：锐化核", out, bytes, true, &base.imageData);
    }
    {
        // USM 强度 0 = 不变
        auto data = copyOf();
        std::vector<uint8_t> out;
        fn.USMSharpen(data, out, w, h, 1.2, 0.0, 6.0);
        expect("USM 强度0不改变", out == base.imageData, "");
    }
    {
        auto data = copyOf();
        std::vector<uint8_t> out;
        fn.USMSharpen(data, out, w, h, 1.2, 1.5, 0.0);
        check("USM 锐化", out, bytes, true, &base.imageData);
    }
    {
        // 旋转稳定性(崩溃回归)：任意角度旋转后 quad 不再是轴对齐矩形，
        // 此时连续 90° 旋转必须仍然只交换宽高、不扩大画布像素总量；
        // 旧实现会按内容外接矩形重建画布，每转一次膨胀一圈，反复旋转最终撑爆内存。
        std::vector<uint8_t> data = base.imageData;
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.RotateImage(data, rw, rh, 23.0, quad);   // 让 quad 变成斜置四边形
        bool ok = quad.isValid() && data.size() == static_cast<size_t>(rw) * rh * 3;
        const size_t pixelsBefore = static_cast<size_t>(rw) * rh;
        for (int i = 0; i < 4 && ok; ++i) {
            fn.RotateImage(data, rw, rh, 90.0, quad);
            if (data.size() != static_cast<size_t>(rw) * rh * 3 ||
                static_cast<size_t>(rw) * rh != pixelsBefore) {
                ok = false;
            }
        }
        const QImage img = ok ? Function::ToDisplayImage(data, rw, rh) : QImage();
        expect("反复 90° 旋转不膨胀", ok && !img.isNull() && img.width() == rw,
               QString("画布像素 %1 -> %2×%3").arg(pixelsBefore).arg(rw).arg(rh));
    }
    {
        // 画质回归：旋转必须"从同一基准按累计角度重采样一次"。
        // 若像旧实现那样在上一次(已被插值糊过)的结果上反复插值，图片会越来越模糊。
        // 用拉普拉斯能量衡量锐利度：基准累计方案必须明显更锐利。
        auto laplacianEnergy = [](const std::vector<uint8_t> &d, int32_t dw, int32_t dh) {
            if (dw < 3 || dh < 3 || d.size() < static_cast<size_t>(dw) * dh * 3) {
                return 0.0;
            }
            auto lum = [&d, dw](int32_t x, int32_t y) {
                const size_t i = (static_cast<size_t>(y) * dw + x) * 3;
                return 0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2];
            };
            double sum = 0.0;
            for (int32_t y = 1; y + 1 < dh; ++y) {
                for (int32_t x = 1; x + 1 < dw; ++x) {
                    const double v = 4 * lum(x, y) - lum(x - 1, y) - lum(x + 1, y)
                                     - lum(x, y - 1) - lum(x, y + 1);
                    sum += (v < 0.0) ? -v : v;
                }
            }
            return sum / (static_cast<double>(dw - 2) * static_cast<double>(dh - 2));
        };
        ContentQuad q0;
        q0.setRect(w, h);
        // (A) 基准 + 累计角度 60°：只插值一次
        std::vector<uint8_t> once = base.imageData;
        int32_t ow = w, oh = h;
        ContentQuad oq = q0;
        fn.RotateImage(once, ow, oh, 60.0, oq);
        // (B) 在上一次结果上再插值，共 6 次 10°
        std::vector<uint8_t> chain = base.imageData;
        int32_t cw = w, ch = h;
        ContentQuad cq = q0;
        for (int i = 0; i < 6; ++i) {
            fn.RotateImage(chain, cw, ch, 10.0, cq);
        }
        const double sharpBase = laplacianEnergy(once, ow, oh);
        const double sharpChain = laplacianEnergy(chain, cw, ch);
        expect("旋转画质：基准累计优于反复插值", sharpBase > sharpChain * 1.05,
               QString("锐度 %1 vs %2").arg(sharpBase, 0, 'f', 2).arg(sharpChain, 0, 'f', 2));
    }
    {
        // 非法角度(0 度)不应改变任何内容
        std::vector<uint8_t> data = copyOf();
        int32_t rw = w, rh = h;
        ContentQuad quad;
        quad.setRect(w, h);
        fn.RotateImage(data, rw, rh, 0.0, quad);
        expect("旋转 0° 保持不变", data == base.imageData && rw == w && rh == h, "");
    }
    {
        // 多格式导出：PNG 落盘后能被重新读取，尺寸一致
        const QImage img = Function::ToDisplayImage(base.imageData, w, h);
        const QString pngPath = QDir::tempPath() + "/imagewithui_selftest_out.png";
        const bool saved = img.save(pngPath, "PNG");
        QImage back(pngPath);
        expect("导出 PNG", saved && back.width() == w && back.height() == h,
               saved ? "" : "保存失败");
        QFile::remove(pngPath);
    }


    {
        // 显示路径崩溃回归：缓冲区是紧凑的 width*3 字节/行，
        // 构造 QImage 时必须显式给 bytesPerLine=width*3。
        // 若用 4 参数构造，Qt 会把行宽补齐到 4 字节对齐，宽度不是 4 的倍数时
        // 于是每行错读 1~3 字节：画面撕裂(像素错位)，并读到缓冲区末尾之外
        // (Debug CRT 的 memcpy 里概率性触发 0xC0000005 —— 任意角度旋转后宽度常不是 4 的倍数)。
        const int32_t tw = 101, th = 9;          // 101*3 = 303，不是 4 的倍数
        std::vector<uint8_t> buf(static_cast<size_t>(tw) * th * 3);
        for (int32_t mr = 0; mr < th; ++mr) {    // mr 为内存行(BMP 自底向上)
            for (int32_t c = 0; c < tw; ++c) {
                const size_t i = (static_cast<size_t>(mr) * tw + c) * 3;
                buf[i + 0] = static_cast<uint8_t>((c * 7 + mr * 13) % 256);
                buf[i + 1] = static_cast<uint8_t>((c * 5 + mr * 11 + 60) % 256);
                buf[i + 2] = static_cast<uint8_t>((c * 3 + mr * 17 + 120) % 256);
            }
        }
        const QImage img = Function::ToDisplayImage(buf, tw, th);
        int mismatch = 0;
        if (img.isNull() || img.width() != tw || img.height() != th) {
            mismatch = tw * th;
        } else {
            for (int32_t vy = 0; vy < th; ++vy) {        // 视觉行
                const int32_t mr = th - 1 - vy;          // 对应的内存行
                for (int32_t c = 0; c < tw; ++c) {
                    const size_t i = (static_cast<size_t>(mr) * tw + c) * 3;
                    const QRgb p = img.pixel(c, vy);
                    if (qBlue(p) != buf[i] || qGreen(p) != buf[i + 1] || qRed(p) != buf[i + 2]) {
                        ++mismatch;
                    }
                }
            }
        }
        expect("显示路径行宽正确(宽非4倍数)", mismatch == 0,
               QString("错位像素 %1 / %2").arg(mismatch).arg(tw * th));
    }

    // ---------- 5. 非 BMP 格式读取：PNG 往返(通道顺序 + 自底向上布局) ----------
    {
        QImage pngSrc(4, 3, QImage::Format_RGB32);
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 4; ++x) {
                // 每个像素颜色都不同，一旦通道顺序或上下翻转错误必然失败
                pngSrc.setPixel(x, y, qRgb(10 + x, 20 + y, 30 + x + y));
            }
        }
        const QString pngPath = QDir::tempPath() + "/imagewithui_selftest_fmt.png";
        const bool savedPng = pngSrc.save(pngPath, "PNG");

        MyValue mv;
        QString e;
        const bool readOk = savedPng && MYFunction::ReadImageFile(pngPath.toStdString(), mv, e);
        bool ok = readOk && mv.bmpInfo.GetWidth() == 4 && mv.bmpInfo.GetHeight() == 3 &&
                  mv.imageData.size() == static_cast<size_t>(4 * 3 * 3);
        if (ok) {
            for (int y = 0; y < 3 && ok; ++y) {
                const int srcY = 2 - y;  // 内部第 0 行 = 画面最底行
                for (int x = 0; x < 4; ++x) {
                    const size_t i = (static_cast<size_t>(y) * 4 + static_cast<size_t>(x)) * 3;
                    if (mv.imageData[i] != static_cast<uint8_t>(30 + x + srcY) ||  // B
                        mv.imageData[i + 1] != static_cast<uint8_t>(20 + srcY) ||  // G
                        mv.imageData[i + 2] != static_cast<uint8_t>(10 + x)) {    // R
                        ok = false;
                        break;
                    }
                }
            }
        }
        if (ok) {
            ++g_pass;
            out() << "[PASS] PNG 读取 : 通道顺序与自底向上布局正确\n";
        } else {
            ++g_fail;
            out() << "[FAIL] PNG 读取失败 : " << e << "\n";
        }
        out().flush();
        QFile::remove(pngPath);
    }

    // ---------- 6. 眼部放大：径向放大(中心不被平移、圆外像素不变) ----------
    {
        const int32_t w = 41, h = 41, cx = 20, cy = 20, r = 12;
        std::vector<uint8_t> img(static_cast<size_t>(w) * h * 3, 0);
        // 圆心画一个 3×3 的白色方块
        for (int32_t y = cy - 1; y <= cy + 1; ++y) {
            for (int32_t x = cx - 1; x <= cx + 1; ++x) {
                const size_t i = (static_cast<size_t>(y) * w + x) * 3;
                img[i] = img[i + 1] = img[i + 2] = 255;
            }
        }
        auto brightCount = [&img]() {
            size_t n = 0;
            for (size_t i = 0; i + 2 < img.size(); i += 3) {
                if (img[i] >= 128 && img[i + 1] >= 128 && img[i + 2] >= 128) {
                    ++n;
                }
            }
            return n;
        };
        const size_t before = brightCount();
        Function::EnlargeEye(img, w, h, cx, cy, r, 0.6);
        const size_t after = brightCount();

        // 白色方块被径向放大后面积应明显变大
        const bool grew = after > before && after * 2 > before * 3;
        // 质心必须仍在圆心附近：旧实现是沿对角线整体平移，质心会明显跑偏
        double sumX = 0, sumY = 0;
        size_t n = 0;
        for (int32_t y = 0; y < h; ++y) {
            for (int32_t x = 0; x < w; ++x) {
                const size_t i = (static_cast<size_t>(y) * w + x) * 3;
                if (img[i] >= 128) {
                    sumX += x;
                    sumY += y;
                    ++n;
                }
            }
        }
        const bool centered = n > 0 &&
                              std::abs(sumX / static_cast<double>(n) - cx) < 0.8 &&
                              std::abs(sumY / static_cast<double>(n) - cy) < 0.8;
        // 半径之外的像素不允许被改动
        bool outsideIntact = true;
        for (int32_t y = 0; y < h && outsideIntact; ++y) {
            for (int32_t x = 0; x < w; ++x) {
                const double d = std::sqrt(static_cast<double>(x - cx) * (x - cx) +
                                           static_cast<double>(y - cy) * (y - cy));
                if (d > static_cast<double>(r)) {
                    const size_t i = (static_cast<size_t>(y) * w + x) * 3;
                    if (img[i] != 0 || img[i + 1] != 0 || img[i + 2] != 0) {
                        outsideIntact = false;
                        break;
                    }
                }
            }
        }
        if (grew && centered && outsideIntact) {
            ++g_pass;
            out() << "[PASS] 眼部放大 : 亮块面积 " << before << " -> " << after
                  << "，质心 (" << sumX / static_cast<double>(n) << ", "
                  << sumY / static_cast<double>(n) << ")，圆外未改动\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 眼部放大 : grew=" << (grew ? 1 : 0) << " centered=" << (centered ? 1 : 0)
                  << " outsideIntact=" << (outsideIntact ? 1 : 0)
                  << " 面积 " << before << " -> " << after << "\n";
        }
        out().flush();
    }

    // ---------- 7. 写回文件并重新读取，验证文件格式正确 ----------
    {
        auto data = copyOf();
        fn.ConvertToGray(data, 0, pixels);
        BMP outBmp = base.bmp;
        BMPInfo outInfo = base.bmpInfo;
        MYFunction::SetBMPHeaderValues(outBmp, outInfo, w, h, outInfo.GetBitsPerPixel());
        const QString outPath = QDir::tempPath() + "/imagewithui_selftest_out.bmp";
        const bool saved = MYFunction::WriteBMPFile(outPath.toStdString(), data, outBmp, outInfo);
        MyValue back;
        QString err2;
        const bool loaded = saved && MYFunction::ReadBMPFile(outPath.toStdString(), back, err2);
        if (loaded && back.imageData.size() == data.size()) {
            ++g_pass;
            out() << "[PASS] 保存并重新读取 : 数据一致(" << back.imageData.size() << " 字节)\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 保存并重新读取失败 : " << err2 << "\n";
        }
        out().flush();
        QFile::remove(outPath);
    }

    // ---------- 8. 参数实时预览：改输入只刷新画面，点"应用"才写历史 ----------
    {
        const QString previewPath = QDir::tempPath() + "/imagewithui_preview_src.png";
        QImage src(64, 48, QImage::Format_RGB32);
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                src.setPixel(x, y, qRgb(20 + x, 30 + y, 60));
            }
        }
        const bool srcOk = src.save(previewPath, "PNG");

        QString failReason;
        {
            MainWindow win;
            QLineEdit *edit = win.findChild<QLineEdit *>("lineEdit_brightness");
            QPushButton *apply = win.findChild<QPushButton *>("btn_brightness");
            if (!srcOk) {
                failReason = "无法生成测试图";
            } else if (!win.OpenImage(previewPath)) {
                failReason = "无法打开测试图";
            } else if (edit == nullptr || apply == nullptr) {
                failReason = "找不到亮度输入框/应用按钮";
            } else {
                const size_t historyBefore = win.imageDataHistory.size();
                const std::vector<uint8_t> pixelsBefore = win.CurrentImageData();

                // 输入参数后等待防抖 + 后台计算完成，画面应进入预览态
                edit->setText("40");
                QEventLoop loop;
                QTimer::singleShot(1500, &loop, &QEventLoop::quit);
                loop.exec();

                if (win.imageDataHistory.size() != historyBefore) {
                    failReason = "预览写入了历史";
                } else if (win.CurrentImageData() != pixelsBefore) {
                    failReason = "预览修改了已提交的像素";
                } else if (!win.IsPreviewActive()) {
                    failReason = "预览没有生成";
                } else {
                    // 点"应用"：预览结果提交，历史加一步
                    apply->click();
                    if (win.imageDataHistory.size() != historyBefore + 1) {
                        failReason = "应用预览后历史未增加";
                    } else if (win.CurrentImageData() == pixelsBefore) {
                        failReason = "应用预览后像素没有变化";
                    } else if (win.IsPreviewActive()) {
                        failReason = "应用后仍停留在预览态";
                    }
                }
            }
        }
        QFile::remove(previewPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 参数实时预览 : 预览不写历史，应用后历史 +1\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 参数实时预览 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 9. 子窗口实时预览(眼部放大)：改参数即预览，点"应用"才写回主窗口 ----------
    {
        QString failReason;
        {
            MainWindow win;
            if (!win.OpenImage(path)) {
                failReason = "无法打开样例图";
            } else {
                const size_t historyBefore = win.imageDataHistory.size();
                eye eyeWin(&win, win.myValue);
                eyeWin.resize(1000, 800);
                eyeWin.show();
                QApplication::processEvents();

                // 在图上点两下，作为左右眼的中心
                QLabel *view = eyeWin.findChild<QLabel *>("imageLabel");
                QLineEdit *rangeEdit = eyeWin.findChild<QLineEdit *>("lineEdit_range");
                QLineEdit *strengthEdit = eyeWin.findChild<QLineEdit *>("lineEdit_increase");
                QPushButton *applyBtn = eyeWin.findChild<QPushButton *>("btn_apply");
                if (view == nullptr || rangeEdit == nullptr ||
                    strengthEdit == nullptr || applyBtn == nullptr) {
                    failReason = "找不到子窗口控件";
                } else {
                    const QRect area = view->rect();
                    const QPoint localCenter = area.center();
                    for (int i = 0; i < 2; ++i) {
                        const QPoint local = localCenter + QPoint(i == 0 ? -60 : 60, 0);
                        QMouseEvent press(QEvent::MouseButtonPress,
                                          eyeWin.mapFromGlobal(view->mapToGlobal(local)),
                                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(&eyeWin, &press);
                    }
                    rangeEdit->setText("40");
                    strengthEdit->setText("0.5");

                    // 等防抖 + 预览计算完成
                    QEventLoop loop;
                    QTimer::singleShot(800, &loop, &QEventLoop::quit);
                    loop.exec();

                    if (win.imageDataHistory.size() != historyBefore) {
                        failReason = "子窗口预览写入了主窗口历史";
                    } else if (eyeWin.PreviewData() == win.CurrentImageData()) {
                        failReason = "子窗口预览没有生成";
                    } else {
                        applyBtn->click();   // 应用：结果写回主窗口
                        if (win.imageDataHistory.size() != historyBefore + 1) {
                            failReason = "应用后主窗口历史未增加";
                        }
                    }
                }
                eyeWin.hide();
            }
        }
        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 子窗口实时预览(眼部放大) : 预览不写历史，应用后写回主窗口\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 子窗口实时预览(眼部放大) : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 10. 大图预览降采样：预览走缩略图，应用仍按全分辨率提交 ----------
    {
        const QString bigPath = QDir::tempPath() + "/imagewithui_preview_big.png";
        bool srcOk = false;
        {
            QImage big(2200, 1500, QImage::Format_RGB32);   // 330 万像素，超过预览阈值
            big.fill(qRgb(90, 110, 130));
            srcOk = big.save(bigPath, "PNG");
        }

        QString failReason;
        {
            MainWindow win;
            if (!srcOk) {
                failReason = "无法生成大图";
            } else if (!win.OpenImage(bigPath)) {
                failReason = "无法打开大图";
            } else {
                const int32_t fullWidth = win.myValue.bmpInfo.GetWidth();
                const int32_t fullHeight = win.myValue.bmpInfo.GetHeight();
                const size_t historyBefore = win.imageDataHistory.size();
                const std::vector<uint8_t> pixelsBefore = win.CurrentImageData();
                QLineEdit *edit = win.findChild<QLineEdit *>("lineEdit_brightness");
                QPushButton *apply = win.findChild<QPushButton *>("btn_brightness");
                if (edit == nullptr || apply == nullptr) {
                    failReason = "找不到亮度输入框/应用按钮";
                } else {
                    edit->setText("50");
                    QEventLoop loop;
                    QTimer::singleShot(2500, &loop, &QEventLoop::quit);
                    loop.exec();

                    if (win.imageDataHistory.size() != historyBefore) {
                        failReason = "大图预览写入了历史";
                    } else if (win.CurrentImageData() != pixelsBefore) {
                        failReason = "大图预览修改了已提交的像素";
                    } else if (!win.IsPreviewActive()) {
                        failReason = "大图预览没有生成";
                    } else {
                        // 低分辨率预览不能提交，应用时应按全分辨率重算并写回历史
                        apply->click();
                        QEventLoop loop2;
                        QTimer::singleShot(3000, &loop2, &QEventLoop::quit);
                        loop2.exec();

                        if (win.imageDataHistory.size() != historyBefore + 1) {
                            failReason = "大图应用后历史未增加";
                        } else if (win.myValue.bmpInfo.GetWidth() != fullWidth ||
                                   win.myValue.bmpInfo.GetHeight() != fullHeight) {
                            failReason = "大图应用后尺寸变了";
                        } else if (win.CurrentImageData() == pixelsBefore) {
                            failReason = "大图应用后像素没有变化";
                        } else if (win.IsPreviewActive()) {
                            failReason = "大图应用后仍停留在预览态";
                        }
                    }
                }
            }
        }
        QFile::remove(bigPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 大图预览降采样 : 缩略图预览不写历史，应用按全分辨率提交\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 大图预览降采样 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 11. 多文档标签页：每个文档的像素/尺寸/历史互不干扰 ----------
    {
        const QString docAPath = QDir::tempPath() + "/imagewithui_doc_a.png";
        const QString docBPath = QDir::tempPath() + "/imagewithui_doc_b.png";
        {
            QImage a(64, 48, QImage::Format_RGB32);
            a.fill(qRgb(200, 60, 60));
            a.save(docAPath, "PNG");
            QImage b(80, 60, QImage::Format_RGB32);
            b.fill(qRgb(60, 60, 200));
            b.save(docBPath, "PNG");
        }

        QString failReason;
        {
            MainWindow win;
            if (!win.OpenImage(docAPath) || !win.OpenImage(docBPath)) {
                failReason = "无法打开测试图";
            } else if (win.DocumentCount() != 2 || win.ActiveDocumentIndex() != 1) {
                failReason = "打开两张图后文档数/活动文档不对";
            } else {
                const std::vector<uint8_t> bPixels = win.CurrentImageData();
                const int32_t bWidth = win.myValue.bmpInfo.GetWidth();
                const int32_t bHeight = win.myValue.bmpInfo.GetHeight();

                QTabBar *bar = win.findChild<QTabBar *>("documentTabBar");
                if (bar == nullptr || bar->count() != 2) {
                    failReason = "标签栏没有正确创建";
                } else {
                    bar->setCurrentIndex(0);   // 切到文档 A
                    if (win.ActiveDocumentIndex() != 0) {
                        failReason = "切换标签后活动文档下标不对";
                    } else if (win.myValue.bmpInfo.GetWidth() != 64 ||
                               win.myValue.bmpInfo.GetHeight() != 48) {
                        failReason = "切换标签后尺寸没有跟着换";
                    } else if (win.imageDataHistory.size() != 1) {
                        failReason = "切换标签后历史没有跟着换";
                    } else {
                        // 在文档 A 上做一次亮度调整，不能影响文档 B
                        QLineEdit *edit = win.findChild<QLineEdit *>("lineEdit_brightness");
                        QPushButton *apply = win.findChild<QPushButton *>("btn_brightness");
                        if (edit != nullptr && apply != nullptr) {
                            edit->setText("30");
                            QEventLoop loop;
                            QTimer::singleShot(1200, &loop, &QEventLoop::quit);
                            loop.exec();
                            apply->click();
                            if (win.imageDataHistory.size() != 2) {
                                failReason = "文档 A 应用调整后历史未增加";
                            }
                        }

                        bar->setCurrentIndex(1);   // 切回文档 B
                        if (!failReason.isEmpty()) {
                            // 已有失败原因，保持
                        } else if (win.ActiveDocumentIndex() != 1) {
                            failReason = "切回文档 B 失败";
                        } else if (win.myValue.bmpInfo.GetWidth() != bWidth ||
                                   win.myValue.bmpInfo.GetHeight() != bHeight) {
                            failReason = "文档 B 的尺寸被破坏";
                        } else if (win.CurrentImageData() != bPixels) {
                            failReason = "文档 B 的像素被文档 A 的修改影响";
                        } else if (win.imageDataHistory.size() != 1) {
                            failReason = "文档 B 的历史被污染";
                        } else {
                            // 关闭当前标签，应回到文档 A
                            QMetaObject::invokeMethod(bar, "tabCloseRequested", Q_ARG(int, 1));
                            if (win.DocumentCount() != 1 || win.ActiveDocumentIndex() != 0) {
                                failReason = "关闭标签后文档数/活动文档不对";
                            } else if (win.myValue.bmpInfo.GetWidth() != 64) {
                                failReason = "关闭标签后没有切回文档 A";
                            }
                        }
                    }
                }
            }
        }
        QFile::remove(docAPath);
        QFile::remove(docBPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 多文档标签页 : 标签切换保持各自像素/尺寸/历史，关闭后自动切换\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 多文档标签页 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 12. 矩形选区：夹取/按文档保存/清除内容/裁剪 ----------
    {
        const QString selPathA = QDir::tempPath() + "/imagewithui_sel_a.png";
        const QString selPathB = QDir::tempPath() + "/imagewithui_sel_b.png";
        {
            QImage a(80, 60, QImage::Format_RGB32);
            a.fill(qRgb(40, 120, 200));
            a.save(selPathA, "PNG");
            QImage b(80, 60, QImage::Format_RGB32);
            b.fill(qRgb(200, 120, 40));
            b.save(selPathB, "PNG");
        }

        QString failReason;
        {
            MainWindow win;
            if (!win.OpenImage(selPathA)) {
                failReason = "无法打开测试图";
            } else {
                // 超出图像范围的选区应被夹到图像内
                win.SetSelection(QRect(-5, -5, 100, 100));
                if (!win.HasSelection() || win.SelectionRect() != QRect(0, 0, 80, 60)) {
                    failReason = "选区没有被夹到图像范围内";
                } else {
                    win.SetSelection(QRect(10, 20, 30, 20));
                    if (win.SelectionRect() != QRect(10, 20, 30, 20)) {
                        failReason = "选区设置失败";
                    }
                }

                // 选区按文档保存：切到新文档应为空，切回来应恢复
                if (failReason.isEmpty()) {
                    QTabBar *bar = win.findChild<QTabBar *>("documentTabBar");
                    if (!win.OpenImage(selPathB) || bar == nullptr || bar->count() != 2) {
                        failReason = "打开第二张图失败";
                    } else if (!win.HasSelection()) {
                        // 新文档没有选区是正确的
                    } else {
                        failReason = "打开新文档后继承了上一文档的选区";
                    }
                    if (failReason.isEmpty()) {
                        bar->setCurrentIndex(0);   // 切回文档 A
                        if (win.SelectionRect() != QRect(10, 20, 30, 20)) {
                            failReason = "切回文档 A 后选区没有恢复";
                        }
                    }
                }

                // 清除选区内容：历史 +1，选区内部变白、外部不变
                if (failReason.isEmpty()) {
                    const size_t historyBefore = win.imageDataHistory.size();
                    win.ClearSelectionContent();
                    QEventLoop loop;
                    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
                    loop.exec();

                    if (win.imageDataHistory.size() != historyBefore + 1) {
                        failReason = "清除选区内容后历史未增加";
                    } else {
                        const std::vector<uint8_t> pixels = win.CurrentImageData();
                        const QRect sel = win.SelectionRect();
                        const int32_t w = win.myValue.bmpInfo.GetWidth();
                        const int32_t h = win.myValue.bmpInfo.GetHeight();
                        const int32_t cx = sel.x() + sel.width() / 2;
                        const int32_t cy = sel.y() + sel.height() / 2;
                        const size_t inside = (static_cast<size_t>(h - 1 - cy) * w + cx) * 3;
                        const size_t outside = (static_cast<size_t>(h - 1) * w + 0) * 3;
                        if (inside + 2 >= pixels.size() || outside + 2 >= pixels.size()) {
                            failReason = "像素索引越界";
                        } else if (pixels[inside] != 255 || pixels[inside + 1] != 255 ||
                                   pixels[inside + 2] != 255) {
                            failReason = "选区内部没有变白";
                        } else if (pixels[outside] == 255 && pixels[outside + 1] == 255) {
                            failReason = "选区外部也被改动了";
                        }
                    }
                }

                // 裁剪到选区：尺寸变成选区大小，完成后选区清空
                if (failReason.isEmpty()) {
                    const QRect sel = win.SelectionRect();
                    win.CropToSelection();
                    QEventLoop loop;
                    QTimer::singleShot(1200, &loop, &QEventLoop::quit);
                    loop.exec();

                    if (win.myValue.bmpInfo.GetWidth() != sel.width() ||
                        win.myValue.bmpInfo.GetHeight() != sel.height()) {
                        failReason = "裁剪到选区后尺寸不对";
                    } else if (win.HasSelection()) {
                        failReason = "裁剪到选区后选区没有清空";
                    }
                }
            }
        }
        QFile::remove(selPathA);
        QFile::remove(selPathB);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 矩形选区 : 夹取/按文档保存/清除内容/裁剪到选区\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 矩形选区 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 13. 图层：新建/不透明度/可见性/复制/删除/合并 ----------
    {
        const QString layerPath = QDir::tempPath() + "/imagewithui_selftest_layers.png";
        {
            QImage img(80, 60, QImage::Format_RGB32);
            img.fill(qRgb(40, 120, 200));   // 内存里是 BGR(200,120,40)
            img.save(layerPath, "PNG");
        }

        QString failReason;
        {
            MainWindow win;
            if (!win.OpenImage(layerPath)) {
                failReason = "无法打开测试图";
            } else if (win.LayerCount() != 1 || win.ActiveLayerIndex() != 0) {
                failReason = "打开图片后应为单图层";
            } else {
                const int32_t w = win.myValue.bmpInfo.GetWidth();
                const int32_t h = win.myValue.bmpInfo.GetHeight();
                const size_t mid = (static_cast<size_t>(h / 2) * static_cast<size_t>(w)
                                    + static_cast<size_t>(w / 2)) * 3;

                // 新建空白图层：叠在当前图层之上并成为活动图层，合成应被白色覆盖
                const size_t histBeforeAdd = win.imageDataHistory.size();
                win.AddLayer();
                if (win.LayerCount() != 2 || win.ActiveLayerIndex() != 1) {
                    failReason = "新建图层后图层数/活动图层不对";
                } else if (win.imageDataHistory.size() != histBeforeAdd + 1) {
                    failReason = "新建图层后历史未增加";
                } else {
                    const std::vector<uint8_t> flat = win.CompositePixels();
                    if (flat.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 3) {
                        failReason = "合成像素尺寸不对";
                    } else if (flat[mid] != 255 || flat[mid + 1] != 255 || flat[mid + 2] != 255) {
                        failReason = "新建的白色图层没有覆盖底层";
                    }
                }

                // 不透明度 0：顶层完全透明，合成回落到原图(属性变化不写历史)
                if (failReason.isEmpty()) {
                    const size_t histBeforeOpacity = win.imageDataHistory.size();
                    win.SetLayerOpacity(1, 0.0);
                    const std::vector<uint8_t> flat = win.CompositePixels();
                    if (win.imageDataHistory.size() != histBeforeOpacity) {
                        failReason = "不透明度改变不应写入历史";
                    } else if (flat[mid] != 200 || flat[mid + 1] != 120 || flat[mid + 2] != 40) {
                        failReason = "不透明度 0 时合成没有回落到原图";
                    }
                }

                // 隐藏顶层：同样回落到原图，且不写历史
                if (failReason.isEmpty()) {
                    const size_t histBeforeHide = win.imageDataHistory.size();
                    win.SetLayerVisible(1, false);
                    const std::vector<uint8_t> flat = win.CompositePixels();
                    if (win.imageDataHistory.size() != histBeforeHide) {
                        failReason = "可见性改变不应写入历史";
                    } else if (flat[mid] != 200 || flat[mid + 1] != 120 || flat[mid + 2] != 40) {
                        failReason = "隐藏顶层后合成没有回落到原图";
                    }
                }

                if (failReason.isEmpty()) {   // 恢复显示与不透明度，供后续步骤
                    win.SetLayerVisible(1, true);
                    win.SetLayerOpacity(1, 1.0);
                }

                // 复制图层：在活动图层之上插入副本
                const size_t histBeforeDup = win.imageDataHistory.size();
                win.DuplicateLayer();
                if (failReason.isEmpty()) {
                    if (win.LayerCount() != 3 || win.ActiveLayerIndex() != 2) {
                        failReason = "复制图层后图层数/活动图层不对";
                    } else if (win.imageDataHistory.size() != histBeforeDup + 1) {
                        failReason = "复制图层后历史未增加";
                    }
                }

                // 删除活动图层：回到两层
                const size_t histBeforeDel = win.imageDataHistory.size();
                win.DeleteActiveLayer();
                if (failReason.isEmpty()) {
                    if (win.LayerCount() != 2) {
                        failReason = "删除图层后图层数不对";
                    } else if (win.imageDataHistory.size() != histBeforeDel + 1) {
                        failReason = "删除图层后历史未增加";
                    } else if (win.ActiveLayerIndex() != 1) {
                        failReason = "删除图层后活动图层不对";
                    }
                }

                // 合并所有图层：回到单图层，画面不变
                const std::vector<uint8_t> beforeMerge = win.CompositePixels();
                const size_t histBeforeMerge = win.imageDataHistory.size();
                win.MergeAllLayers();
                if (failReason.isEmpty()) {
                    if (win.LayerCount() != 1 || win.ActiveLayerIndex() != 0) {
                        failReason = "合并后应为单图层";
                    } else if (win.imageDataHistory.size() != histBeforeMerge + 1) {
                        failReason = "合并图层后历史未增加";
                    } else if (win.CompositePixels() != beforeMerge) {
                        failReason = "合并后合成结果发生变化";
                    }
                }
            }
        }
        QFile::remove(layerPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 图层系统 : 新建/不透明度/可见性/复制/删除/合并\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 图层系统 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 14. 选区限制：调整类算子只改选区内 ----------
    {
        const QString selOpPath = QDir::tempPath() + "/imagewithui_selftest_selop.png";
        {
            QImage img(80, 60, QImage::Format_RGB32);
            img.fill(qRgb(40, 120, 200));   // 内存里是 BGR(200,120,40)
            img.save(selOpPath, "PNG");
        }

        QString failReason;
        {
            MainWindow win;
            if (!win.OpenImage(selOpPath)) {
                failReason = "无法打开测试图";
            } else {
                win.SetSelection(QRect(10, 10, 20, 20));
                QLineEdit *edit = win.findChild<QLineEdit *>("lineEdit_brightness");
                QPushButton *btn = win.findChild<QPushButton *>("btn_brightness");
                if (edit == nullptr || btn == nullptr) {
                    failReason = "找不到亮度输入框/应用按钮";
                } else {
                    const size_t historyBefore = win.imageDataHistory.size();
                    edit->setText("50");
                    btn->click();
                    QEventLoop loop;
                    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
                    loop.exec();

                    const int32_t w = win.myValue.bmpInfo.GetWidth();
                    const int32_t h = win.myValue.bmpInfo.GetHeight();
                    if (win.imageDataHistory.size() != historyBefore + 1) {
                        failReason = "选区调整后历史未增加";
                    } else if (w != 80 || h != 60) {
                        failReason = "选区调整改变了画布尺寸";
                    } else {
                        const std::vector<uint8_t> pixels = win.CurrentImageData();
                        // 选区内取样(视觉 20,20)，选区外取样(视觉 0,0)
                        const size_t inside = (static_cast<size_t>(h - 1 - 20) * w + 20) * 3;
                        const size_t outside = (static_cast<size_t>(h - 1) * w + 0) * 3;
                        if (inside + 2 >= pixels.size() || outside + 2 >= pixels.size()) {
                            failReason = "像素索引越界";
                        } else if (pixels[inside] == 200 && pixels[inside + 1] == 120 &&
                                   pixels[inside + 2] == 40) {
                            failReason = "选区内像素没有被调整";
                        } else if (pixels[outside] != 200 || pixels[outside + 1] != 120 ||
                                   pixels[outside + 2] != 40) {
                            failReason = "选区外像素被改动了";
                        }
                    }
                }
            }
        }
        QFile::remove(selOpPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 选区限制 : 调整类算子只作用于选区，选区外保持原样\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 选区限制 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 15. 选区掩码：反选 / Shift 加选 / 羽化 ----------
    {
        const QString maskPath = QDir::tempPath() + "/imagewithui_selftest_mask.png";
        {
            QImage img(80, 60, QImage::Format_RGB32);
            img.fill(qRgb(40, 120, 200));   // 内存里是 BGR(200,120,40)
            img.save(maskPath, "PNG");
        }

        QString failReason;
        auto waitOp = []() {
            QEventLoop loop;
            QTimer::singleShot(1500, &loop, &QEventLoop::quit);
            loop.exec();
        };
        auto applyBrightness = [](MainWindow &win, const QString &value) {
            QLineEdit *edit = win.findChild<QLineEdit *>("lineEdit_brightness");
            QPushButton *btn = win.findChild<QPushButton *>("btn_brightness");
            if (edit == nullptr || btn == nullptr) {
                return false;
            }
            edit->setText(value);
            btn->click();
            return true;
        };
        auto maskAt = [](MainWindow &win, int x, int y) -> int {
            const SelectionMask &m = win.CurrentSelectionMask();
            const int32_t w = win.myValue.bmpInfo.GetWidth();
            if (m.isEmpty() || w <= 0 || x < 0 || y < 0 || x >= m.width || y >= m.height) {
                return -1;
            }
            return m.data[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        };
        auto pixelAt = [](MainWindow &win, int x, int y, int out[3]) -> bool {
            const std::vector<uint8_t> &px = win.CurrentImageData();
            const int32_t w = win.myValue.bmpInfo.GetWidth();
            const int32_t h = win.myValue.bmpInfo.GetHeight();
            if (w <= 0 || h <= 0 || x < 0 || y < 0 || x >= w || y >= h) {
                return false;
            }
            const size_t i = (static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w)
                              + static_cast<size_t>(x)) * 3;
            if (i + 2 >= px.size()) {
                return false;
            }
            out[0] = px[i];
            out[1] = px[i + 1];
            out[2] = px[i + 2];
            return true;
        };
        const int kOrig[3] = {200, 120, 40};

        // (1) 反选：原选区外变成选区(被调整)，原选区内被排除(保持原样)
        if (failReason.isEmpty()) {
            MainWindow win;
            if (!win.OpenImage(maskPath)) {
                failReason = "无法打开测试图";
            } else {
                win.SetSelection(QRect(10, 10, 20, 20));
                win.InvertSelection();
                if (!win.HasSelection()) {
                    failReason = "反选后应仍有选区";
                } else if (maskAt(win, 5, 5) != 255) {
                    failReason = "反选后原选区外未被选中";
                } else if (maskAt(win, 20, 20) != 0) {
                    failReason = "反选后原选区内未被排除";
                } else if (!applyBrightness(win, "50")) {
                    failReason = "找不到亮度输入框/应用按钮";
                } else {
                    waitOp();
                    int inside[3] = {0, 0, 0};
                    int outside[3] = {0, 0, 0};
                    if (!pixelAt(win, 20, 20, inside) || !pixelAt(win, 5, 5, outside)) {
                        failReason = "像素索引越界";
                    } else if (inside[0] != kOrig[0] || inside[1] != kOrig[1] || inside[2] != kOrig[2]) {
                        failReason = "反选后原选区内的像素被改动了";
                    } else if (outside[0] == kOrig[0] && outside[1] == kOrig[1] && outside[2] == kOrig[2]) {
                        failReason = "反选后原选区外的像素没有被调整";
                    }
                }
            }
        }

        // (2) Shift 加选：两块都生效，两块之间不变
        if (failReason.isEmpty()) {
            MainWindow win;
            if (!win.OpenImage(maskPath)) {
                failReason = "无法打开测试图";
            } else {
                win.SetSelection(QRect(10, 10, 10, 10));
                win.SetSelection(QRect(30, 10, 10, 10), true);
                if (win.SelectionRect() != QRect(10, 10, 30, 10)) {
                    failReason = "加选后包围盒不对";
                } else if (maskAt(win, 15, 15) != 255 || maskAt(win, 35, 15) != 255) {
                    failReason = "加选后没有覆盖两块矩形";
                } else if (maskAt(win, 25, 15) != 0) {
                    failReason = "加选后两块之间的像素不应被选中";
                } else {
                    win.ClearSelectionContent();
                    waitOp();
                    int first[3] = {0, 0, 0};
                    int second[3] = {0, 0, 0};
                    int middle[3] = {0, 0, 0};
                    if (!pixelAt(win, 15, 15, first) || !pixelAt(win, 35, 15, second) ||
                        !pixelAt(win, 25, 15, middle)) {
                        failReason = "像素索引越界";
                    } else if (first[0] != 255 || second[0] != 255) {
                        failReason = "加选的两个矩形没有被清除";
                    } else if (middle[0] != kOrig[0] || middle[1] != kOrig[1] || middle[2] != kOrig[2]) {
                        failReason = "加选两块之间的像素被改动了";
                    }
                }
            }
        }

        // (3) 羽化：边界出现中间覆盖度，清除后得到半透明白
        if (failReason.isEmpty()) {
            MainWindow win;
            if (!win.OpenImage(maskPath)) {
                failReason = "无法打开测试图";
            } else {
                win.SetSelection(QRect(10, 10, 20, 20));
                win.FeatherSelection(3.0);
                const int edge = maskAt(win, 20, 8);   // 选区上边界外 2 px
                if (maskAt(win, 20, 20) != 255) {
                    failReason = "羽化后选区中心应保持完全选中";
                } else if (edge <= 0 || edge >= 255) {
                    failReason = "羽化后边界没有出现中间覆盖度";
                } else {
                    win.ClearSelectionContent();
                    waitOp();
                    int c[3] = {0, 0, 0};
                    if (!pixelAt(win, 20, 8, c)) {
                        failReason = "像素索引越界";
                    } else if (c[0] != kOrig[0] && c[1] != kOrig[1] && c[2] != kOrig[2] && c[0] < 255) {
                        // 半透明白：B 通道应大于原值，但整体没有变成纯白
                    } else {
                        failReason = "羽化边界没有产生半透明过渡";
                    }
                }
            }
        }
        QFile::remove(maskPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 选区掩码 : 反选/Shift 加选/羽化边界过渡\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 选区掩码 : " << failReason << "\n";
        }
        out().flush();
    }

    // ---------- 16. 套索选区：多边形栅格化 + 加选 / 减选 ----------
    {
        const QString lassoPath = QDir::tempPath() + "/imagewithui_selftest_lasso.png";
        {
            QImage img(80, 60, QImage::Format_RGB32);
            img.fill(qRgb(40, 120, 200));
            img.save(lassoPath, "PNG");
        }

        QString failReason;
        auto maskValue = [](MainWindow &win, int x, int y) -> int {
            const SelectionMask &m = win.CurrentSelectionMask();
            const int32_t w = win.myValue.bmpInfo.GetWidth();
            if (m.isEmpty() || w <= 0 || x < 0 || y < 0 || x >= m.width || y >= m.height) {
                return -1;
            }
            return m.data[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        };

        MainWindow win;
        if (!win.OpenImage(lassoPath)) {
            failReason = "无法打开测试图";
        } else {
            // (1) 三角形套索：内部被选中，外部与远处不被选中
            QPolygon tri;
            tri << QPoint(5, 5) << QPoint(35, 5) << QPoint(20, 35);
            win.SetSelectionPolygon(tri);
            if (!win.HasSelection()) {
                failReason = "三角形套索没有建立选区";
            } else if (maskValue(win, 20, 10) != 255) {
                failReason = "三角形内部未被选中";
            } else if (maskValue(win, 2, 10) != 0 || maskValue(win, 20, 45) != 0) {
                failReason = "三角形外部被错误选中";
            }

            // (2) Shift 加选另一块矩形多边形：两块都选中
            if (failReason.isEmpty()) {
                QPolygon block;
                block << QPoint(60, 10) << QPoint(75, 10) << QPoint(75, 25) << QPoint(60, 25);
                win.SetSelectionPolygon(block, true);
                if (maskValue(win, 65, 15) != 255) {
                    failReason = "套索加选后新区域未被选中";
                } else if (maskValue(win, 20, 10) != 255) {
                    failReason = "套索加选把原有选区弄丢了";
                } else if (win.SelectionRect().width() < 60) {
                    failReason = "套索加选后包围盒没有覆盖两块区域";
                }
            }

            // (3) Alt 减选：从已有选区内扣掉一块
            if (failReason.isEmpty()) {
                QPolygon cut;
                cut << QPoint(20, 5) << QPoint(40, 5) << QPoint(40, 30) << QPoint(20, 30);
                win.SetSelectionPolygon(cut, false, true);
                if (maskValue(win, 20, 10) != 0) {
                    failReason = "套索减选没有扣掉目标区域";
                } else if (maskValue(win, 65, 15) != 255) {
                    failReason = "套索减选误伤了其他区域";
                } else if (!win.HasSelection()) {
                    failReason = "套索减选后应仍有剩余选区";
                }
            }
        }
        QFile::remove(lassoPath);

        if (failReason.isEmpty()) {
            ++g_pass;
            out() << "[PASS] 套索选区 : 多边形内部/外部判定正确，加选与减选生效\n";
        } else {
            ++g_fail;
            out() << "[FAIL] 套索选区 : " << failReason << "\n";
        }
        out().flush();
    }

    out() << "------------------------------------------------\n";
    out() << "通过 " << g_pass << " 项，失败 " << g_fail << " 项\n";
    out().flush();
    return (g_fail == 0) ? 0 : 1;
}

} // namespace Selftest
