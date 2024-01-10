
#ifndef DEBUG_H
#define DEBUG_H
#include "BMPFile.h"

inline void ImgInfo(BMP &inBmp, BMPInfo &inBmpInfo)
{
    std::cout << "this is a BMP file" << std::endl
              << "BMPHeader info:" << std::endl
              << "---------------" << std::endl
              << "fileType : " << inBmp.GetFileType() << std::endl
              << "fileSize : " << inBmp.GetFileSize() << std::endl
              << "reserved1 : " << inBmp.GetReserved1() << std::endl
              << "reserved2 : " << inBmp.GetReserved2() << std::endl
              << "dataOffset : " << inBmp.GetDataOffset() << std::endl
              << "---------------" << std::endl
              << "BMPInfo:" << std::endl
              << "headerSize : " << inBmpInfo.GetHeaderSize() << std::endl
              << "width : " << inBmpInfo.GetWidth() << std::endl
              << "height : " << inBmpInfo.GetHeight() << std::endl
              << "planes : " << inBmpInfo.GetPlanes() << std::endl
              << "bitsPerPixel : " << inBmpInfo.GetBitsPerPixel() << std::endl
              << "compression : " << inBmpInfo.GetCompression() << std::endl
              << "imageSize : " << inBmpInfo.GetImageSize() << std::endl
              << "xPixelsPerMeter : " << inBmpInfo.GetXPixelsPerMeter() << std::endl
              << "yPixelsPerMeter : " << inBmpInfo.GetYPixelsPerMeter() << std::endl
              << "colorsUsed : " << inBmpInfo.GetColorsUsed() << std::endl
              << "colorsImportant : " << inBmpInfo.GetColorsImportant() << std::endl
              << "-----------------" << std::endl;
}

//int ReadBinTxt(const std::string &inName, const std::string &outName)
//{
//    std::ifstream inputFile(inName, std::ios::binary);
//    if (!inputFile.is_open()) {
//        std::cerr << "unable to open this file" << std::endl;
//        ;
//        inputFile.close();
//        return 1;
//    }
//    std::ofstream outputFile("../debug/" + outName);

//    if (!outputFile.is_open()) {
//        std::cerr << "unable to create output file!" << std::endl;
//        outputFile.close();
//        return 1;
//    }

//    char byte;
//    while (inputFile.get(byte)) {
//        outputFile << static_cast<int>(byte) << " ";
//    }

//    inputFile.close();
//    outputFile.close();

//    std::cout << "read binary ok" << std::endl;
//    return 0;
//}
//// 保存二进制
//int outputBinToFile(std::vector<uint8_t> &imageData, const std::string &name)
//{
//    // open output file
//    std::ofstream outputFile("../debug/" + name, std::ios::binary);  //
//    if (!outputFile.is_open()) {
//        std::cerr << "unable to open this file!" << std::endl;
//        return 1;
//    }
//    outputFile.write(reinterpret_cast<const char *>(imageData.data()), static_cast<std::streamsize>(imageData.size()));
//    outputFile.close();
//    std::cout << "-----------" << std::endl;
//    std::cout << "binary generate!" << std::endl;
//    std::cout << "file called " << name << std::endl;
//    std::cout << "-----------" << std::endl;

//    return 0;
//}
//// 保存像素
//int OutputPixToFile(std::vector<uint8_t> &imageData, const std::string &name)
//{
//    // open output file
//    std::ofstream outputFile(name);

//    if (!outputFile.is_open()) {
//        std::cerr << "unable to open this file !" << std::endl;
//        return 1;
//    }
//    int count=0;
//    for (uint8_t byte: imageData) {
//        outputFile << static_cast<int>(byte) << " ";
//        count++;
//        if(count%3==0)
//        {
//            outputFile<<"    ";
//        }
//    }
//    outputFile.close();
//    std::cout << "-----------" << std::endl;
//    std::cout << "pix generate！" << std::endl;
//    std::cout << "file called " << name << std::endl;
//    std::cout << "-----------" << std::endl;
//    return 0;
//}

//// temp image for debug
//void TempImage(std::vector<uint8_t> &imageData, const std::string &name, BMP &bmp, BMPInfo &bmpInfo)
//{
//    // open output file
//    std::ofstream outputFile("../debug/" + name);

//    if (!outputFile.is_open()) {
//        std::cout << "unable to create this file" << std::endl;
//        exit(0);
//    }
//    outputFile.write(reinterpret_cast<const char *>(&bmp), sizeof(BMP));

//    outputFile.write(reinterpret_cast<const char *>(&bmpInfo), sizeof(BMPInfo));
//    outputFile.seekp(bmp.GetDataOffset());

//    // write
//    outputFile.write(reinterpret_cast<const char *>(imageData.data()), imageData.size());

//    // close file
//    outputFile.close();
//    std::cout << "-----------" << std::endl;
//    std::cout << "temp image already generated!" << std::endl;
//    std::cout << "file called " << name << std::endl;
//    std::cout << "-----------" << std::endl;
//}
//// 十六进制
//void WriteBMPHexToFile(const std::string &inputFileName, const std::string &outputFileName)
//{
//    std::ifstream inputFile(inputFileName, std::ios::binary);
//    if (!inputFile.is_open()) {
//        std::cout << "Unable to open input BMP file: " << inputFileName << std::endl;
//        return;
//    }

//    std::ofstream outputFile("../debug/" + outputFileName);
//    if (!outputFile.is_open()) {
//        std::cout << "Unable to create output file: " << outputFileName << std::endl;
//        return;
//    }

//    char byte;
//    while (inputFile.get(byte)) {
//        // 将字节以十六进制格式写入输出文件
//        outputFile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(byte)) << " ";
//    }

//    // 关闭文件
//    inputFile.close();
//    outputFile.close();
//    std::cout << "-----------" << std::endl;
//    std::cout << "hex generated" << std::endl;
//    std::cout << "file called " << outputFileName << std::endl;
//    std::cout << "-----------" << std::endl;
//}

#endif  // DEBUG_H
