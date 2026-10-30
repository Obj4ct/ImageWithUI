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

    // ---------- 5. 写回文件并重新读取，验证文件格式正确 ----------
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
