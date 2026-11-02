/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2023 素曜
 *
 * This source file is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 */


#ifndef DEBUG_H
#define DEBUG_H
#include "bmp_file.h"

#include "qt_headers.h"
class Debug{
public:


    static void ImgInfo(BMP &inBmp, BMPInfo &inBmpInfo);

    static int ReadBinTxt(const std::string &inName, const std::string &outName);
    // 保存二进制
    static int outputBinToFile(std::vector<uint8_t> &imageData, const std::string &name);
    //保存像素
    static int OutputPixToFile(std::vector<uint8_t> &imageData, const std::string &name);

    // temp image for debug
   static void TempImage(std::vector<uint8_t> &imageData, const std::string &name, BMP &bmp, BMPInfo &bmpInfo);
    // 十六进制
    static void WriteBMPHexToFile(const std::string &inputFileName, const std::string &outputFileName);
    static void PrintImagePixelsToFile(const QImage& image, const QString& filePath);
};

#endif  // DEBUG_H
