#ifndef SELFTEST_H
#define SELFTEST_H
#include <QString>

// 命令行自检：用样例图跑一遍所有算子，校验输出是否合理
// 用法: ImageWithUI.exe --selftest [图片路径]
namespace Selftest {
int Run(const QString &imagePath);
}

#endif // SELFTEST_H
