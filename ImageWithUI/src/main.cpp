#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QTimer>
#include <QPixmap>
#include "mainwindow.h"
#include "Theme.h"
#include "selftest.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 命令行自检模式： ImageWithUI.exe --selftest [图片路径]
    const QStringList args = a.arguments();
    if (args.contains("--selftest")) {
        QString imagePath;
        for (int i = 1; i < args.size(); ++i) {   // 从 1 开始，跳过 exe 自身路径
            const QString &arg = args.at(i);
            if (!arg.startsWith("--")) {
                imagePath = arg;
                break;
            }
        }
        return Selftest::Run(imagePath);
    }

    // 统一使用 Fusion 基础风格，再叠加自定义主题
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QFont font("Microsoft YaHei UI", 10);
    a.setFont(font);
    Theme::Apply(&a, true);      // 默认深色主题

    // 截图模式： ImageWithUI.exe --screenshot out.png [图片路径]
    // 用于界面回归验证，抓一张窗口截图后退出
    if (args.contains("--screenshot")) {
        QString shotPath;
        QString imagePath;
        for (int i = 1; i < args.size(); ++i) {
            const QString &arg = args.at(i);
            if (arg.startsWith("--")) {
                continue;
            }
            if (shotPath.isEmpty()) {
                shotPath = arg;
            } else if (imagePath.isEmpty()) {
                imagePath = arg;
            }
        }
        if (shotPath.isEmpty()) {
            return 2;
        }

        MainWindow w;
        w.resize(1280, 820);
        w.show();
        if (!imagePath.isEmpty()) {
            w.OpenImage(imagePath);
        }
        QTimer::singleShot(800, &w, [&w, shotPath]() {
            w.grab().save(shotPath);
            qApp->quit();
        });
        return a.exec();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
