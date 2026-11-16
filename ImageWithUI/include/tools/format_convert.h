/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

#ifndef FORMAT_CONVERT_H
#define FORMAT_CONVERT_H

#include <QDialog>
#include <QString>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QTableWidget;

// 格式转换参数
struct FormatConvertOptions
{
    QString suffix;          // 目标后缀：png / bmp / jpg / tif / webp
    int quality = 92;        // JPEG/WebP 质量，1~100
    int pngCompression = 6;  // PNG 压缩级别，0~9(越大文件越小)
    bool keepAlpha = true;   // 目标格式支持透明时保留 alpha，否则白底压平
};

namespace FormatConvert {

// 当前 Qt 环境实际可写的目标格式后缀(固定顺序，缺插件的格式不会出现)
QStringList SupportedSuffixes();

// 目标格式是否支持透明通道
bool SupportsAlpha(const QString &suffix);

// 把单个图片文件转换成目标格式；dstPath 需带正确后缀。
// 与编辑器口径一致：源图有 alpha 且目标支持时保留透明，否则白底压平。
bool ConvertFile(const QString &srcPath, const QString &dstPath,
                 const FormatConvertOptions &options, QString &error);

} // namespace FormatConvert

// 批量图片格式转换窗口
class FormatConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormatConvertDialog(QWidget *parent = nullptr);

    // 添加文件到列表；paths 为空时弹出文件选择框
    void AddFiles(const QStringList &paths = QStringList());
    int ConvertedCount() const { return m_converted; }

private:
    void RemoveSelected();
    void UpdateOptionState();
    void StartConvert();
    QString BuildTargetPath(const QString &srcPath) const;

    QTableWidget *m_table = nullptr;
    QComboBox *m_format = nullptr;
    QSlider *m_quality = nullptr;
    QLabel *m_qualityValue = nullptr;
    QSlider *m_compression = nullptr;
    QLabel *m_compressionValue = nullptr;
    QLineEdit *m_outputDir = nullptr;
    QCheckBox *m_keepAlpha = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_convertBtn = nullptr;
    int m_converted = 0;
};

#endif // FORMAT_CONVERT_H
