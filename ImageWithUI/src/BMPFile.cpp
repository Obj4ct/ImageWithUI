//加载BMP文件
#include "BMPFile.h"
#include <cstdio>

//读取BMP文件
bool MYFunction::ReadBMPFile(const std::string &fileName, MyValue &outValue, QString &errMsg)
{
    errMsg.clear();

    // 用 C 标准 IO 一次性读入整个文件；解析全部基于内存缓冲并做边界检查
    std::FILE *fp = std::fopen(fileName.c_str(), "rb");
    if (fp == nullptr) {
        errMsg = QString("无法打开文件：%1").arg(QString::fromStdString(fileName));
        return false;
    }

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        errMsg = "无法读取文件长度";
        return false;
    }
    const long fileLength = std::ftell(fp);
    std::rewind(fp);
    if (fileLength <= 0) {
        std::fclose(fp);
        errMsg = "文件为空或无法读取";
        return false;
    }

    std::vector<char> buffer(static_cast<size_t>(fileLength));
    const size_t readBytes = std::fread(buffer.data(), 1, buffer.size(), fp);
    std::fclose(fp);

    if (readBytes != buffer.size()) {
        errMsg = "文件读取不完整";
        return false;
    }

    const size_t fileSize = buffer.size();
    if (fileSize < sizeof(BMP) + sizeof(BMPInfo)) {
        errMsg = "文件太小，不是有效的 BMP 文件";
        return false;
    }

    BMP bmp;
    BMPInfo bmpInfo;
    std::memcpy(&bmp, buffer.data(), sizeof(BMP));
    if (bmp.GetFileType() != 0x4D42) {  // BM ASCII
        errMsg = "文件标识不是 'BM'，不是有效的 BMP 文件";
        return false;
    }
    std::memcpy(&bmpInfo, buffer.data() + sizeof(BMP), sizeof(BMPInfo));

    // ---- 头信息校验 ----
    if (bmpInfo.GetHeaderSize() < 40) {
        errMsg = QString("不支持的 BMP 信息头（%1 字节），仅支持 BITMAPINFOHEADER 及以上").arg(bmpInfo.GetHeaderSize());
        return false;
    }
    if (bmpInfo.GetWidth() <= 0) {
        errMsg = "图像宽度非法";
        return false;
    }
    // height 为负表示自顶向下存储，取绝对值并在读入后翻转
    const bool topDown = bmpInfo.GetHeight() < 0;
    const int32_t height = std::abs(bmpInfo.GetHeight());
    if (height == 0) {
        errMsg = "图像高度非法";
        return false;
    }
    if (bmpInfo.GetBitsPerPixel() != 24) {
        errMsg = QString("仅支持 24 位 BMP，当前文件为 %1 位").arg(bmpInfo.GetBitsPerPixel());
        return false;
    }
    if (bmpInfo.GetCompression() != 0) {
        errMsg = "仅支持未压缩的 BMP 文件";
        return false;
    }

    const uint32_t dataOffset = bmp.GetDataOffset();
    if (dataOffset < sizeof(BMP) + sizeof(BMPInfo) || dataOffset >= fileSize) {
        errMsg = "像素数据偏移量非法，文件可能已损坏";
        return false;
    }

    // 期望的像素数据长度：优先用头部声明，否则按 w*h*3 推算
    const uint64_t expectedSize = static_cast<uint64_t>(bmpInfo.GetWidth()) * height * 3;
    uint32_t imageDataSize = bmpInfo.GetImageSize();
    if (imageDataSize == 0 || imageDataSize > expectedSize) {
        imageDataSize = static_cast<uint32_t>(expectedSize);
    }
    if (static_cast<uint64_t>(dataOffset) + imageDataSize > fileSize) {
        errMsg = "像素数据长度与文件实际大小不符，文件可能被截断";
        return false;
    }

    std::vector<uint8_t> imageData(imageDataSize);
    std::memcpy(imageData.data(), buffer.data() + dataOffset, imageDataSize);

    // 自顶向下的文件需要翻转成自底向上，和本程序的处理约定保持一致
    if (topDown) {
        const size_t rowBytes = static_cast<size_t>(bmpInfo.GetWidth()) * 3;
        std::vector<uint8_t> flipped(imageData.size());
        for (int32_t row = 0; row < height; ++row) {
            const size_t src = static_cast<size_t>(height - 1 - row) * rowBytes;
            std::copy(imageData.begin() + src, imageData.begin() + src + rowBytes,
                      flipped.begin() + static_cast<size_t>(row) * rowBytes);
        }
        imageData.swap(flipped);
        bmpInfo.SetHeight(height);
    }

    outValue.imageData = std::move(imageData);
    outValue.bmp = bmp;
    outValue.bmpInfo = bmpInfo;
    return true;
}


bool MYFunction::WriteBMPFile(const std::string &filePath, const std::vector<uint8_t> &imageData, BMP &bmp, BMPInfo &bmpInfo)
{
    if (filePath.empty()) {
        return false;
    }
    std::FILE *fp = std::fopen(filePath.c_str(), "wb");
    if (fp == nullptr) {
        return false;
    }
    bool ok = true;
    ok = ok && (std::fwrite(&bmp, 1, sizeof(BMP), fp) == sizeof(BMP));
    ok = ok && (std::fwrite(&bmpInfo, 1, sizeof(BMPInfo), fp) == sizeof(BMPInfo));
    if (ok && bmp.GetDataOffset() > sizeof(BMP) + sizeof(BMPInfo)) {
        // 头部与像素数据之间的空隙补 0
        const size_t gap = bmp.GetDataOffset() - (sizeof(BMP) + sizeof(BMPInfo));
        std::vector<char> zeros(gap, 0);
        ok = ok && (std::fwrite(zeros.data(), 1, gap, fp) == gap);
    }
    ok = ok && (std::fwrite(imageData.data(), 1, imageData.size(), fp) == imageData.size());
    std::fclose(fp);
    return ok;
}

void MYFunction::SetBMPHeaderValues(BMP &bmp, BMPInfo &bmpInfo, int width, int height, uint16_t bitsPerPixel)
{

    // 设置 BMP 文件头信息
    bmp.SetFileType(0x4D42);                                                                 // BMP 文件类型标识 "BM"
    bmp.SetFileSize(sizeof(BMP) + sizeof(BMPInfo) + (width * height * (bitsPerPixel / 8)));  // 文件总大小
    bmp.SetReserved1(0);                                                                     // 保留字段1，通常设置为0
    bmp.SetReserved2(0);                                                                     // 保留字段2，通常设置为0
    bmp.SetDataOffset(sizeof(BMP) + sizeof(BMPInfo));                                        // 图像数据在文件中的偏移量
    // 设置 BMP 信息头信息
    bmpInfo.SetHeaderSize(sizeof(BMPInfo));               // BMP 文件头大小
    bmpInfo.SetWidth(width);                              // 图像宽度
    bmpInfo.SetHeight(height);                            // 图像高度
    bmpInfo.SetPlanes(1);                                 // 位图平面数，通常为1
    bmpInfo.SetBitsPerPixel(bitsPerPixel);                // 每像素位数
    bmpInfo.SetCompression(0);                            // 压缩方式，通常不压缩
    bmpInfo.SetImageSize(bmp.GetFileSize() - bmp.GetDataOffset());  // 图像数据大小
    bmpInfo.SetXPixelsPerMeter(0);                        // 水平像素密度
    bmpInfo.SetYPixelsPerMeter(0);                        // 垂直像素密度
    bmpInfo.SetColorsUsed(0);                             // 使用的颜色数，通常不使用调色板
    bmpInfo.SetColorsImportant(0);                        // 重要颜色数，通常不指定
}



void BMPInfo::WriteToBMPInfo(std::vector<uint8_t> &imageData, int32_t cropHeight, int32_t cropWidth, BMPInfo &newBmpInfo, BMP &bmp)
{

    newBmpInfo.height = cropHeight;
    newBmpInfo.width = cropWidth;
    newBmpInfo.imageSize = newBmpInfo.width * newBmpInfo.height * 3;//120000
    bmp.fileSize = bmp.dataOffset + newBmpInfo.imageSize;
    imageData.resize(newBmpInfo.imageSize);
    qDebug()<<newBmpInfo.height;
    qDebug()<<newBmpInfo.width;
    qDebug()<<newBmpInfo.imageSize;
    qDebug()<<imageData.size();
}
