#include "selftest.h"
#include "BMPFile.h"
#include "Function.h"
#include <QFileInfo>
#include <QTextStream>
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

    out() << "------------------------------------------------\n";
    out() << "通过 " << g_pass << " 项，失败 " << g_fail << " 项\n";
    out().flush();
    return (g_fail == 0) ? 0 : 1;
}

} // namespace Selftest
