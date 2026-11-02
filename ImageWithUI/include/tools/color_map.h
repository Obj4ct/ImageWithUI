/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */

#ifndef COLORMAP_H
#define COLORMAP_H
#include "qt_headers.h"
#include "bmp_file.h"
#include "debug.h"
#include "image_ops.h"
class MainWindow;
namespace Ui {
class ColorMap;
}
class ColorMap : public QWidget
{
    Q_OBJECT

public:
    explicit ColorMap(MainWindow* mainWindow, BitmapImage myValue, QWidget *parent = nullptr);
    ~ColorMap();

private slots:
    void on_comboBox_colorMap_currentIndexChanged(int index);

    void on_btn_apply_clicked();

private:
    Ui::ColorMap *ui;
    MainWindow* mainWindow;
    ImageOps function;
    BitmapImage myValue;
    BitmapImage newValue;
    std::vector<uint8_t>imageData;
    QImage m_bmpImage;
private:
    void _ColorMap(std::vector<uint8_t> &imageData, std::vector<uint8_t> &colorMap);
    void ShowImage(std::vector<uint8_t>&inImageData);
    void ResetImage();
};

#endif // COLORMAP_H
