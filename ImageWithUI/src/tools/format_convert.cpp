/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

// 图片格式转换：单文件转换核心 + 批量转换窗口

#include "format_convert.h"
#include "bmp_file.h"
#include "image_ops.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

struct FormatInfo
{
    const char *suffix;       // 对外后缀
    const char *writerFormat; // QImageWriter 使用的格式名
    const char *label;        // 下拉框显示文字
    bool alpha;               // 是否支持透明通道
};

const FormatInfo kFormats[] = {
    {"png", "png", "PNG（无损，支持透明）", true},
    {"bmp", "bmp", "BMP（无压缩，体积大）", false},
    {"jpg", "jpeg", "JPEG（有损压缩，体积小）", false},
    {"tif", "tiff", "TIFF（无损，支持透明）", true},
    {"webp", "webp", "WebP（高压缩，支持透明）", true},
};

const FormatInfo *FindFormat(const QString &suffix)
{
    for (const FormatInfo &info : kFormats) {
        if (suffix.compare(QLatin1String(info.suffix), Qt::CaseInsensitive) == 0) {
            return &info;
        }
    }
    return nullptr;
}

// 白底压平：与编辑器保存时"其余格式按白底压平"的口径一致
void FlattenToWhite(uchar *dst, int r, int g, int b, int a)
{
    const int inv = 255 - a;
    dst[0] = static_cast<uchar>((r * a + 255 * inv + 127) / 255);
    dst[1] = static_cast<uchar>((g * a + 255 * inv + 127) / 255);
    dst[2] = static_cast<uchar>((b * a + 255 * inv + 127) / 255);
    dst[3] = 255;
}

} // namespace

QStringList FormatConvert::SupportedSuffixes()
{
    QStringList writable;
    const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for (const QByteArray &format : formats) {
        writable << QString::fromLatin1(format);
    }
    QStringList out;
    for (const FormatInfo &info : kFormats) {
        if (writable.contains(QLatin1String(info.writerFormat))) {
            out << QLatin1String(info.suffix);
        }
    }
    return out;
}

bool FormatConvert::SupportsAlpha(const QString &suffix)
{
    const FormatInfo *info = FindFormat(suffix);
    return info != nullptr && info->alpha;
}

bool FormatConvert::ConvertFile(const QString &srcPath, const QString &dstPath,
                                const FormatConvertOptions &options, QString &error)
{
    error.clear();
    const FormatInfo *target = FindFormat(options.suffix);
    if (target == nullptr) {
        error = QString("不支持的目标格式：%1").arg(options.suffix);
        return false;
    }
    if (!QFileInfo::exists(srcPath)) {
        error = QString("源文件不存在：%1").arg(QFileInfo(srcPath).fileName());
        return false;
    }

    BitmapImage loaded;
    if (!BmpIO::ReadImageFile(srcPath.toStdString(), loaded, error)) {
        return false;
    }
    const int width = loaded.bmpInfo.GetWidth();
    const int height = loaded.bmpInfo.GetHeight();
    if (width <= 0 || height <= 0 || loaded.imageData.empty()) {
        error = QString("图像内容为空：%1").arg(QFileInfo(srcPath).fileName());
        return false;
    }

    QImage image = ImageOps::ToDisplayImage(loaded.imageData, width, height);
    if (image.isNull()) {
        error = QString("图像数据无效：%1").arg(QFileInfo(srcPath).fileName());
        return false;
    }

    // 源图带 alpha 时：目标支持透明就保留，否则按白底压平
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const bool hasAlpha = loaded.alphaData.size() == pixelCount;
    if (hasAlpha) {
        const bool keepAlpha = options.keepAlpha && target->alpha;
        QImage rgba(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y) {
            uchar *dst = rgba.scanLine(y);
            // 内部缓冲自底向上：画面第 y 行对应 alpha/imageData 的第 height-1-y 行
            const size_t row = static_cast<size_t>(height - 1 - y);
            for (int x = 0; x < width; ++x) {
                const size_t pixel = row * static_cast<size_t>(width) + static_cast<size_t>(x);
                const size_t i = pixel * 3;
                const int b = loaded.imageData[i + 0];
                const int g = loaded.imageData[i + 1];
                const int r = loaded.imageData[i + 2];
                const int a = loaded.alphaData[pixel];
                uchar *out = dst + static_cast<size_t>(x) * 4;
                if (keepAlpha) {
                    out[0] = static_cast<uchar>(r);
                    out[1] = static_cast<uchar>(g);
                    out[2] = static_cast<uchar>(b);
                    out[3] = static_cast<uchar>(a);
                } else {
                    FlattenToWhite(out, r, g, b, a);
                }
            }
        }
        image = rgba;
    }

    if (!target->alpha && image.hasAlphaChannel()) {
        image = image.convertToFormat(QImage::Format_RGB32);   // JPEG/BMP 无透明通道
    }

    QImageWriter writer(dstPath, QByteArray(target->writerFormat));
    if (!writer.canWrite()) {
        error = QString("当前环境无法写入 %1（缺少图像插件或目录不可写）")
                    .arg(QLatin1String(target->suffix));
        return false;
    }
    if (qstrcmp(target->writerFormat, "jpeg") == 0 ||
        qstrcmp(target->writerFormat, "webp") == 0) {
        writer.setQuality(qBound(1, options.quality, 100));
    } else if (qstrcmp(target->writerFormat, "png") == 0) {
        // 与项目 PNG 导出保持一致：压缩级别 9 → 最小文件
        const int level = qBound(0, options.pngCompression, 9);
        writer.setQuality((9 - level) * 100 / 9);
    }
    if (!writer.write(image)) {
        error = QString("写入失败：%1").arg(writer.errorString());
        return false;
    }
    return true;
}

// ------------------------------------------------------------------
// 批量转换窗口
// ------------------------------------------------------------------

FormatConvertDialog::FormatConvertDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString("图片格式转换"));
    setMinimumSize(720, 520);

    QVBoxLayout *root = new QVBoxLayout(this);

    // 文件列表操作
    QHBoxLayout *fileRow = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QString("添加图片…"), this);
    QPushButton *removeBtn = new QPushButton(QString("移除选中"), this);
    QPushButton *clearBtn = new QPushButton(QString("清空"), this);
    fileRow->addWidget(addBtn);
    fileRow->addWidget(removeBtn);
    fileRow->addWidget(clearBtn);
    fileRow->addStretch();
    root->addLayout(fileRow);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels(QStringList() << QString("文件") << QString("尺寸")
                                                    << QString("状态"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    root->addWidget(m_table, 1);

    // 输出设置
    QGroupBox *options = new QGroupBox(QString("输出设置"), this);
    QGridLayout *grid = new QGridLayout(options);

    grid->addWidget(new QLabel(QString("目标格式"), options), 0, 0);
    m_format = new QComboBox(options);
    m_format->setObjectName("convertFormat");
    const QStringList suffixes = FormatConvert::SupportedSuffixes();
    for (const FormatInfo &info : kFormats) {
        if (suffixes.contains(QLatin1String(info.suffix))) {
            m_format->addItem(QString::fromUtf8(info.label), QLatin1String(info.suffix));
        }
    }
    grid->addWidget(m_format, 0, 1);

    grid->addWidget(new QLabel(QString("质量（JPEG/WebP）"), options), 1, 0);
    QHBoxLayout *qualityRow = new QHBoxLayout();
    m_quality = new QSlider(Qt::Horizontal, options);
    m_quality->setRange(1, 100);
    m_quality->setValue(92);
    m_qualityValue = new QLabel(QString("92"), options);
    m_qualityValue->setMinimumWidth(28);
    qualityRow->addWidget(m_quality, 1);
    qualityRow->addWidget(m_qualityValue);
    grid->addLayout(qualityRow, 1, 1);

    grid->addWidget(new QLabel(QString("PNG 压缩级别"), options), 2, 0);
    QHBoxLayout *compressionRow = new QHBoxLayout();
    m_compression = new QSlider(Qt::Horizontal, options);
    m_compression->setRange(0, 9);
    m_compression->setValue(6);
    m_compressionValue = new QLabel(QString("6"), options);
    m_compressionValue->setMinimumWidth(28);
    compressionRow->addWidget(m_compression, 1);
    compressionRow->addWidget(m_compressionValue);
    grid->addLayout(compressionRow, 2, 1);

    grid->addWidget(new QLabel(QString("输出目录"), options), 3, 0);
    QHBoxLayout *dirRow = new QHBoxLayout();
    m_outputDir = new QLineEdit(options);
    m_outputDir->setPlaceholderText(QString("留空表示与源文件同目录"));
    QPushButton *browseBtn = new QPushButton(QString("浏览…"), options);
    dirRow->addWidget(m_outputDir, 1);
    dirRow->addWidget(browseBtn);
    grid->addLayout(dirRow, 3, 1);

    m_keepAlpha = new QCheckBox(QString("保留透明通道（目标格式支持时有效，否则白底压平）"), options);
    m_keepAlpha->setChecked(true);
    grid->addWidget(m_keepAlpha, 4, 0, 1, 2);
    grid->setColumnStretch(1, 1);
    root->addWidget(options);

    // 进度与结果
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    root->addWidget(m_progress);
    m_summary = new QLabel(QString("尚未开始转换"), this);
    root->addWidget(m_summary);

    // 底部按钮
    QHBoxLayout *bottom = new QHBoxLayout();
    bottom->addStretch();
    m_convertBtn = new QPushButton(QString("开始转换"), this);
    m_convertBtn->setDefault(true);
    QPushButton *closeBtn = new QPushButton(QString("关闭"), this);
    bottom->addWidget(m_convertBtn);
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);

    connect(addBtn, &QPushButton::clicked, this, [this]() { AddFiles(); });
    connect(removeBtn, &QPushButton::clicked, this, &FormatConvertDialog::RemoveSelected);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_table->setRowCount(0);
        m_summary->setText(QString("列表已清空"));
    });
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QString("选择输出目录"), m_outputDir->text());
        if (!dir.isEmpty()) {
            m_outputDir->setText(dir);
        }
    });
    connect(m_quality, &QSlider::valueChanged, this, [this](int value) {
        m_qualityValue->setText(QString::number(value));
    });
    connect(m_compression, &QSlider::valueChanged, this, [this](int value) {
        m_compressionValue->setText(QString::number(value));
    });
    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { UpdateOptionState(); });
    connect(m_convertBtn, &QPushButton::clicked, this, &FormatConvertDialog::StartConvert);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    UpdateOptionState();
}

void FormatConvertDialog::AddFiles(const QStringList &paths)
{
    QStringList files = paths;
    if (files.isEmpty()) {
        files = QFileDialog::getOpenFileNames(
            this, QString("选择要转换的图片"), QString(),
            QString("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.gif *.tga *.ico);;"
                    "所有文件 (*.*)"));
    }
    for (const QString &path : files) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        bool exists = false;
        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (m_table->item(row, 0)->data(Qt::UserRole).toString() == absolute) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem *nameItem = new QTableWidgetItem(QFileInfo(absolute).fileName());
        nameItem->setData(Qt::UserRole, absolute);
        nameItem->setToolTip(absolute);
        m_table->setItem(row, 0, nameItem);

        const QSize size = QImageReader(absolute).size();
        m_table->setItem(row, 1, new QTableWidgetItem(
                                    size.isValid() ? QString("%1 × %2").arg(size.width()).arg(size.height())
                                                   : QString("—")));
        m_table->setItem(row, 2, new QTableWidgetItem(QString("待转换")));
    }
    m_progress->setRange(0, std::max(1, m_table->rowCount()));
    m_progress->setValue(0);
    m_summary->setText(QString("共 %1 个文件，等待转换").arg(m_table->rowCount()));
}

void FormatConvertDialog::RemoveSelected()
{
    const QModelIndexList selected = m_table->selectionModel()->selectedRows();
    for (int i = selected.size() - 1; i >= 0; --i) {
        m_table->removeRow(selected.at(i).row());
    }
    m_summary->setText(QString("共 %1 个文件，等待转换").arg(m_table->rowCount()));
}

void FormatConvertDialog::UpdateOptionState()
{
    const QString suffix = m_format->currentData().toString();
    m_quality->setEnabled(suffix == "jpg" || suffix == "webp");
    m_compression->setEnabled(suffix == "png");
    m_keepAlpha->setEnabled(FormatConvert::SupportsAlpha(suffix));
}

QString FormatConvertDialog::BuildTargetPath(const QString &srcPath) const
{
    const QFileInfo info(srcPath);
    QString dir = m_outputDir->text().trimmed();
    if (dir.isEmpty()) {
        dir = info.absolutePath();
    }
    const QString suffix = m_format->currentData().toString();
    return QDir(dir).filePath(info.completeBaseName() + "." + suffix);
}

void FormatConvertDialog::StartConvert()
{
    if (m_table->rowCount() == 0) {
        m_summary->setText(QString("请先添加要转换的图片"));
        return;
    }
    const QString suffix = m_format->currentData().toString();
    if (suffix.isEmpty()) {
        m_summary->setText(QString("当前环境没有可用的目标格式"));
        return;
    }

    FormatConvertOptions options;
    options.suffix = suffix;
    options.quality = m_quality->value();
    options.pngCompression = m_compression->value();
    options.keepAlpha = m_keepAlpha->isChecked();

    m_convertBtn->setEnabled(false);
    m_progress->setRange(0, m_table->rowCount());
    m_progress->setValue(0);

    int okCount = 0;
    int skipCount = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString src = m_table->item(row, 0)->data(Qt::UserRole).toString();
        const QString dst = BuildTargetPath(src);
        QString status;
        if (QFileInfo(dst).absoluteFilePath() == QFileInfo(src).absoluteFilePath()) {
            status = QString("跳过（目标与源文件相同）");
            ++skipCount;
        } else if (!QDir().mkpath(QFileInfo(dst).absolutePath())) {
            status = QString("失败：无法创建输出目录");
        } else {
            QString error;
            if (FormatConvert::ConvertFile(src, dst, options, error)) {
                status = QString("成功 → %1").arg(QFileInfo(dst).fileName());
                ++okCount;
            } else {
                status = QString("失败：%1").arg(error);
            }
        }
        m_table->item(row, 2)->setText(status);
        m_progress->setValue(row + 1);
        QApplication::processEvents();   // 保持界面响应
    }

    m_converted += okCount;
    const int failed = m_table->rowCount() - okCount - skipCount;
    m_summary->setText(QString("完成：成功 %1，失败 %2，跳过 %3")
                           .arg(okCount).arg(failed).arg(skipCount));
    m_convertBtn->setEnabled(true);
}
