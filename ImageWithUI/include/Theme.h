#ifndef THEME_H
#define THEME_H
#include <QApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSlider>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QStatusBar>
#include <QScrollBar>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QWidget>
#include <QString>

// 界面主题：深色(默认) / 浅色
// 所有控件的圆角、留白、配色集中在这里，方便统一调整
namespace Theme {

inline bool &DarkFlag()
{
    static bool dark = true;
    return dark;
}

inline QString DarkStyleSheet()
{
    return QString::fromUtf8(R"(
/* ---------- 基础 ---------- */
QWidget {
    background-color: #1b1d21;
    color: #e3e6ea;
    font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: 13px;
}
QMainWindow::separator { background: #2b2e34; width: 1px; height: 1px; }
QToolTip {
    background-color: #2b2e34;
    color: #e3e6ea;
    border: 1px solid #3a3e46;
    border-radius: 6px;
    padding: 6px 10px;
}

/* ---------- 菜单栏 / 菜单 ---------- */
QMenuBar {
    background-color: #16181c;
    border-bottom: 1px solid #26292f;
    padding: 4px 6px;
}
QMenuBar::item {
    background: transparent;
    padding: 7px 14px;
    border-radius: 6px;
    margin: 0 2px;
}
QMenuBar::item:selected { background-color: #2f81f7; color: #ffffff; }
QMenu {
    background-color: #22252b;
    border: 1px solid #34383f;
    border-radius: 10px;
    padding: 8px;
}
QMenu::item {
    padding: 8px 26px 8px 22px;
    border-radius: 6px;
    margin: 2px 4px;
}
QMenu::item:selected { background-color: #2f81f7; color: #ffffff; }
QMenu::item:disabled { color: #6b7280; }
QMenu::separator { height: 1px; background: #34383f; margin: 6px 10px; }

/* ---------- 工具栏 ---------- */
QToolBar {
    background-color: #191b1f;
    border: none;
    border-bottom: 1px solid #26292f;
    padding: 6px 8px;
    spacing: 6px;
}
QToolBar QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 6px 12px;
    color: #cfd4da;
}
QToolBar QToolButton:hover { background-color: #262a30; border-color: #343940; color: #ffffff; }
QToolBar QToolButton:pressed { background-color: #2f81f7; color: #ffffff; }

/* ---------- 按钮 ---------- */
QPushButton {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                     stop:0 #343941, stop:1 #2a2e34);
    border: 1px solid #3b4049;
    border-radius: 8px;
    padding: 8px 16px;
    color: #e3e6ea;
    min-height: 18px;
}
QPushButton:hover {
    background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                     stop:0 #3d434c, stop:1 #31363d);
    border-color: #4d545e;
}
QPushButton:pressed { background-color: #2f81f7; border-color: #2f81f7; color: #ffffff; }
QPushButton:disabled { color: #6b7280; background-color: #23262b; border-color: #2d3138; }
QPushButton:focus { border-color: #2f81f7; }

/* ---------- 输入框 ---------- */
QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #14161a;
    border: 1px solid #34383f;
    border-radius: 8px;
    padding: 7px 10px;
    color: #e3e6ea;
    selection-background-color: #2f81f7;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #2f81f7; }
QLineEdit:disabled, QSpinBox:disabled { color: #6b7280; background-color: #1f2226; }

/* ---------- 下拉框 ---------- */
QComboBox {
    background-color: #23262b;
    border: 1px solid #34383f;
    border-radius: 8px;
    padding: 7px 12px;
    min-width: 90px;
}
QComboBox:hover { border-color: #4d545e; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background-color: #22252b;
    border: 1px solid #34383f;
    border-radius: 8px;
    selection-background-color: #2f81f7;
    padding: 4px;
    outline: none;
}

/* ---------- 滑块 ---------- */
QSlider::groove:horizontal {
    height: 5px;
    background: #34383f;
    border-radius: 3px;
}
QSlider::sub-page:horizontal { background: #2f81f7; border-radius: 3px; }
QSlider::handle:horizontal {
    background: #f2f4f7;
    border: 2px solid #2f81f7;
    width: 14px;
    height: 14px;
    margin: -6px 0;
    border-radius: 9px;
}
QSlider::handle:horizontal:hover { background: #2f81f7; }

/* ---------- 分组 / 画布 ---------- */
QGroupBox {
    border: 1px solid #2c3037;
    border-radius: 10px;
    margin-top: 16px;
    padding: 14px 12px 12px 12px;
    background-color: #1e2126;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #8fb8ff;
    font-weight: 600;
}
QLabel#imageLabel {
    background-color: #101215;
    border: 1px solid #2c3037;
    border-radius: 12px;
    color: #6b7280;
    font-size: 14px;
}
QFrame[frameShape="4"] { color: #8a9099; }

/* ---------- 状态栏 ---------- */
QStatusBar {
    background-color: #16181c;
    border-top: 1px solid #26292f;
    color: #9aa1a9;
}
QStatusBar QLabel { color: #9aa1a9; padding: 2px 8px; }

/* ---------- 滚动条 ---------- */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #3a3f47;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #4a505a; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: #3a3f47;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #4a505a; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

/* ---------- 其它 ---------- */
QCheckBox { spacing: 8px; }
QProgressBar {
    border: 1px solid #34383f;
    border-radius: 6px;
    background: #14161a;
    text-align: center;
    height: 8px;
}
QProgressBar::chunk { background: #2f81f7; border-radius: 5px; }
)");
}

inline QString LightStyleSheet()
{
    return QString::fromUtf8(R"(
QWidget {
    background-color: #f4f5f7;
    color: #1f2328;
    font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: 13px;
}
QMainWindow::separator { background: #d8dbe0; width: 1px; height: 1px; }
QToolTip {
    background-color: #ffffff;
    color: #1f2328;
    border: 1px solid #d0d4da;
    border-radius: 6px;
    padding: 6px 10px;
}
QMenuBar {
    background-color: #ffffff;
    border-bottom: 1px solid #e3e6ea;
    padding: 4px 6px;
}
QMenuBar::item { background: transparent; padding: 7px 14px; border-radius: 6px; margin: 0 2px; }
QMenuBar::item:selected { background-color: #2f81f7; color: #ffffff; }
QMenu {
    background-color: #ffffff;
    border: 1px solid #dfe3e8;
    border-radius: 10px;
    padding: 8px;
}
QMenu::item { padding: 8px 26px 8px 22px; border-radius: 6px; margin: 2px 4px; }
QMenu::item:selected { background-color: #2f81f7; color: #ffffff; }
QMenu::separator { height: 1px; background: #e3e6ea; margin: 6px 10px; }
QToolBar { background-color: #ffffff; border: none; border-bottom: 1px solid #e3e6ea; padding: 6px 8px; spacing: 6px; }
QToolBar QToolButton {
    background: transparent; border: 1px solid transparent; border-radius: 7px;
    padding: 6px 12px; color: #3c424a;
}
QToolBar QToolButton:hover { background-color: #eef1f5; border-color: #dfe3e8; }
QToolBar QToolButton:pressed { background-color: #2f81f7; color: #ffffff; }
QPushButton {
    background-color: #ffffff;
    border: 1px solid #d7dbe1;
    border-radius: 8px;
    padding: 8px 16px;
    color: #1f2328;
    min-height: 18px;
}
QPushButton:hover { background-color: #f0f3f7; border-color: #c3c9d1; }
QPushButton:pressed { background-color: #2f81f7; border-color: #2f81f7; color: #ffffff; }
QPushButton:disabled { color: #a4abb4; background-color: #f2f4f6; }
QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #ffffff;
    border: 1px solid #d7dbe1;
    border-radius: 8px;
    padding: 7px 10px;
    color: #1f2328;
    selection-background-color: #2f81f7;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #2f81f7; }
QComboBox {
    background-color: #ffffff; border: 1px solid #d7dbe1; border-radius: 8px;
    padding: 7px 12px; min-width: 90px;
}
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background-color: #ffffff; border: 1px solid #dfe3e8; border-radius: 8px;
    selection-background-color: #2f81f7; padding: 4px; outline: none;
}
QSlider::groove:horizontal { height: 5px; background: #dde1e6; border-radius: 3px; }
QSlider::sub-page:horizontal { background: #2f81f7; border-radius: 3px; }
QSlider::handle:horizontal {
    background: #ffffff; border: 2px solid #2f81f7; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 9px;
}
QGroupBox {
    border: 1px solid #e0e4e9; border-radius: 10px; margin-top: 16px;
    padding: 14px 12px 12px 12px; background-color: #ffffff;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left; left: 12px;
    padding: 0 6px; color: #1f6feb; font-weight: 600;
}
QLabel#imageLabel {
    background-color: #e9ecf0; border: 1px solid #d7dbe1; border-radius: 12px;
    color: #8a9099; font-size: 14px;
}
QStatusBar { background-color: #ffffff; border-top: 1px solid #e3e6ea; color: #6b7280; }
QStatusBar QLabel { color: #6b7280; padding: 2px 8px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #c3c9d1; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #aab1ba; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #c3c9d1; border-radius: 4px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
QCheckBox { spacing: 8px; }
)");
}

// 应用主题
inline void Apply(QApplication *app, bool dark)
{
    DarkFlag() = dark;
    if (!app) {
        return;
    }
    app->setStyleSheet(dark ? DarkStyleSheet() : LightStyleSheet());
}

// 切换主题并作用到整个应用
inline void Toggle()
{
    Apply(qApp, !DarkFlag());
}

} // namespace Theme

#endif // THEME_H
