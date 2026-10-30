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
// 配色、圆角、留白都集中在这里：
//   底色 #0b0d10(画布区) / #14171c(面板) / #0e1116(输入框)
//   主色 #22d3ee(青) 用于强调、焦点、选中；正文 #dfe6ee；次要文字 #8b96a5
namespace Theme {

inline bool &DarkFlag()
{
    static bool dark = true;
    return dark;
}

inline QString DarkStyleSheet()
{
    return QString::fromUtf8(R"(
/* ================= 基础 ================= */
QWidget {
    background-color: #0b0d10;
    color: #dfe6ee;
    font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: 13px;
}
QMainWindow { background-color: #0b0d10; }
QMainWindow::separator { background: #232830; width: 1px; height: 1px; }
QToolTip {
    background-color: #171b21;
    color: #dfe6ee;
    border: 1px solid #2d3440;
    border-radius: 6px;
    padding: 6px 10px;
}

/* ================= 菜单栏 ================= */
QMenuBar {
    background-color: #101318;
    border: none;
    border-bottom: 1px solid #1c222b;
    padding: 3px 8px;
}
QMenuBar::item {
    background: transparent;
    padding: 7px 13px;
    border-radius: 6px;
    margin: 0 1px;
    color: #c8d2de;
}
QMenuBar::item:selected { background-color: #1b2630; color: #67e8f9; }
QMenuBar::item:pressed { background-color: #123040; color: #ffffff; }
QMenu {
    background-color: #14171c;
    border: 1px solid #262d38;
    border-radius: 10px;
    padding: 7px;
}
QMenu::item {
    padding: 8px 30px 8px 20px;
    border-radius: 6px;
    margin: 1px 3px;
    color: #d5dee8;
}
QMenu::item:selected { background-color: #1b3a47; color: #8be9f9; }
QMenu::item:disabled { color: #5b6572; }
QMenu::separator { height: 1px; background: #2a313c; margin: 6px 10px; }

/* ================= 工具栏 ================= */
QToolBar {
    background-color: #101318;
    border: none;
    border-bottom: 1px solid #1c222b;
    padding: 6px 10px;
    spacing: 4px;
}
QToolBar QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 6px 11px;
    color: #b8c3d1;
}
QToolBar QToolButton:hover {
    background-color: #171f29;
    border-color: #2a3a49;
    color: #e8f6ff;
}
QToolBar QToolButton:pressed { background-color: #12333f; border-color: #1f5d70; color: #8be9f9; }
QToolBar::separator { background: #232830; width: 1px; margin: 4px 8px; }

/* ================= 文档标签栏 ================= */
QTabBar#documentTabBar { background: transparent; }
QTabBar#documentTabBar::tab {
    background: #14171c;
    border: 1px solid #232830;
    border-bottom: none;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    padding: 5px 12px;
    margin-right: 3px;
    color: #8b96a5;
    min-width: 88px;
}
QTabBar#documentTabBar::tab:hover { background: #1a2028; color: #c8d2de; }
QTabBar#documentTabBar::tab:selected {
    background: #0e1116;
    border-color: #22d3ee;
    color: #67e8f9;
}
QTabBar#documentTabBar QToolButton#tabCloseButton {
    background: transparent;
    border: none;
    padding: 0;
    min-width: 14px;
    max-width: 14px;
    color: #7b8794;
    font-size: 12px;
}
QTabBar#documentTabBar QToolButton#tabCloseButton:hover { color: #22d3ee; }

/* ================= 按钮 ================= */
QPushButton {
    background-color: #1a1e25;
    border: 1px solid #2a313c;
    border-radius: 8px;
    padding: 6px 14px;
    min-width: 54px;
    min-height: 20px;
    color: #dbe3ec;
}
QPushButton:hover {
    background-color: #212734;
    border-color: #3a4d5e;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: #0f3b48;
    border-color: #22d3ee;
    color: #8be9f9;
}
QPushButton:disabled { color: #59636f; background-color: #14171c; border-color: #232830; }
QPushButton:focus { border-color: #22d3ee; }

/* ================= 输入框 ================= */
QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #0e1116;
    border: 1px solid #262d38;
    border-radius: 7px;
    padding: 5px 9px;
    min-height: 20px;
    color: #e6f0f8;
    selection-background-color: #155163;
    selection-color: #eafcff;
}
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover { border-color: #33404e; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: #22d3ee;
    background-color: #0f151b;
}
QLineEdit:disabled, QSpinBox:disabled { color: #59636f; background-color: #14171c; }
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: #1a1e25;
    border: none;
    width: 14px;
}

/* ================= 下拉框 ================= */
QComboBox {
    background-color: #14171c;
    border: 1px solid #262d38;
    border-radius: 7px;
    padding: 6px 10px;
    min-width: 88px;
    color: #dfe6ee;
}
QComboBox:hover { border-color: #33404e; }
QComboBox:focus { border-color: #22d3ee; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #14171c;
    border: 1px solid #262d38;
    border-radius: 8px;
    selection-background-color: #1b3a47;
    selection-color: #8be9f9;
    padding: 4px;
    outline: none;
}

/* ================= 滑块 ================= */
QSlider::groove:horizontal { height: 4px; background: #232830; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #22d3ee; border-radius: 2px; }
QSlider::add-page:horizontal { background: #232830; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #eafcff;
    border: 1px solid #22d3ee;
    width: 12px;
    height: 12px;
    margin: -5px 0;
    border-radius: 7px;
}
QSlider::handle:horizontal:hover { background: #8be9f9; }

/* ================= 分组 / 画布 ================= */
QGroupBox {
    border: 1px solid #232830;
    border-radius: 10px;
    margin-top: 15px;
    padding: 12px 12px 10px 12px;
    background-color: #14171c;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #67e8f9;
    font-weight: 600;
}

/* ================= 列表 / 历史面板 ================= */
QListWidget, QListView, QTreeWidget, QTableWidget {
    background-color: #0e1116;
    border: 1px solid #262d38;
    border-radius: 8px;
    padding: 4px;
    outline: none;
    color: #c8d2de;
}
QListWidget::item { padding: 5px 8px; border-radius: 5px; margin: 1px 2px; }
QListWidget::item:hover { background-color: #17202a; }
QListWidget::item:selected { background-color: #1b3a47; color: #8be9f9; }
/* 面板内的列表要与卡片同色系，且不受"标签固定宽度"规则影响 */
QWidget#widget QListWidget#historyList {
    background-color: #0e1116;
    min-height: 120px;
    padding: 4px;
    selection-background-color: #1b3a47;
    selection-color: #8be9f9;
}
QWidget#widget QLabel#label_layers_hint {
    color: #64707d;
    font-size: 12px;
    min-width: 0;
    max-width: 16777215px;
    padding: 8px 0;
}
QDialog { background-color: #0b0d10; }
/* 画布：接近纯黑 + 极细外框，让图片本身成为视觉中心 */
QLabel#imageLabel {
    background-color: #07090c;
    border: 1px solid #1e252f;
    border-radius: 12px;
    color: #64707d;
    font-size: 13px;
    letter-spacing: 1px;
}
QFrame[frameShape="4"] { color: #3b4552; }
QFrame[frameShape="5"] { color: #232830; }

/* ================= 右侧工具面板(卡片) ================= */
QWidget#widget {
    background-color: #14171c;
    border: 1px solid #232830;
    border-radius: 14px;
}
QWidget#widget QWidget { background: transparent; }
/* 固定标签宽度，让输入框/按钮整齐对齐(标签里原有的对齐空格会被裁掉) */
QWidget#widget QLabel { color: #9dabb8; min-width: 104px; max-width: 104px; }
QLabel#label_sec_adjust, QLabel#label_sec_transform {
    color: #67e8f9;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 2px;
    min-width: 0;
    max-width: 16777215px;
    padding: 6px 0 0 2px;
}
QWidget#widget QLineEdit { min-width: 78px; max-width: 92px; }
QWidget#widget QPushButton { min-width: 46px; max-width: 64px; }

/* ================= 状态栏 ================= */
QStatusBar {
    background-color: #101318;
    border-top: 1px solid #1c222b;
    color: #8b96a5;
}
QStatusBar::item { border: none; }
QStatusBar QLabel {
    color: #8b96a5;
    padding: 1px 10px;
    font-size: 12px;
}
QLabel#statusInfoLabel {
    color: #93a3b4;
    font-family: "Consolas", "Microsoft YaHei UI", monospace;
    font-size: 12px;
}
QLabel#statusInfoLabel:hover { color: #67e8f9; }
QStatusBar QLabel#statusInfoLabel:first-child { }

/* ================= 滚动条 ================= */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical {
    background: #2b323c;
    border-radius: 4px;
    min-height: 32px;
}
QScrollBar::handle:vertical:hover { background: #3b4653; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal {
    background: #2b323c;
    border-radius: 4px;
    min-width: 32px;
}
QScrollBar::handle:horizontal:hover { background: #3b4653; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

/* ================= 其它 ================= */
QCheckBox { spacing: 8px; }
QProgressBar {
    border: 1px solid #232830;
    border-radius: 5px;
    background: #0e1116;
    text-align: center;
    height: 8px;
}
QProgressBar::chunk { background: #22d3ee; border-radius: 4px; }
)");
}

inline QString LightStyleSheet()
{
    return QString::fromUtf8(R"(
QWidget {
    background-color: #f6f7f9;
    color: #1c2430;
    font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: 13px;
}
QMainWindow::separator { background: #dfe3e8; width: 1px; height: 1px; }
QToolTip {
    background-color: #ffffff; color: #1c2430;
    border: 1px solid #d6dbe2; border-radius: 6px; padding: 6px 10px;
}
QMenuBar {
    background-color: #ffffff;
    border: none;
    border-bottom: 1px solid #e7eaee;
    padding: 3px 8px;
}
QMenuBar::item {
    background: transparent; padding: 7px 13px;
    border-radius: 6px; margin: 0 1px; color: #354252;
}
QMenuBar::item:selected { background-color: #eef4fa; color: #0b7285; }
QMenu {
    background-color: #ffffff; border: 1px solid #dfe3e8;
    border-radius: 10px; padding: 7px;
}
QMenu::item { padding: 8px 30px 8px 20px; border-radius: 6px; margin: 1px 3px; }
QMenu::item:selected { background-color: #e3f4f8; color: #0b7285; }
QMenu::separator { height: 1px; background: #e7eaee; margin: 6px 10px; }
QToolBar {
    background-color: #ffffff; border: none;
    border-bottom: 1px solid #e7eaee; padding: 6px 10px; spacing: 4px;
}
QToolBar QToolButton {
    background: transparent; border: 1px solid transparent;
    border-radius: 7px; padding: 6px 11px; color: #3c4a59;
}
QToolBar QToolButton:hover { background-color: #f0f4f8; border-color: #dfe3e8; }
QToolBar QToolButton:pressed { background-color: #dff3f8; border-color: #9edbe6; color: #0b7285; }
QToolBar::separator { background: #e7eaee; width: 1px; margin: 4px 8px; }
/* ================= 文档标签栏 ================= */
QTabBar#documentTabBar { background: transparent; }
QTabBar#documentTabBar::tab {
    background: #ffffff;
    border: 1px solid #dfe3e8;
    border-bottom: none;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    padding: 5px 12px;
    margin-right: 3px;
    color: #6b7785;
    min-width: 88px;
}
QTabBar#documentTabBar::tab:hover { background: #f2f6fa; color: #354252; }
QTabBar#documentTabBar::tab:selected {
    background: #eef8fb;
    border-color: #22b8cf;
    color: #0b7285;
}
QTabBar#documentTabBar QToolButton#tabCloseButton {
    background: transparent;
    border: none;
    padding: 0;
    min-width: 14px;
    max-width: 14px;
    color: #77828d;
    font-size: 12px;
}
QTabBar#documentTabBar QToolButton#tabCloseButton:hover { color: #0b7285; }
QPushButton {
    background-color: #ffffff;
    border: 1px solid #d9dee5;
    border-radius: 8px;
    padding: 6px 14px;
    min-width: 54px;
    min-height: 20px;
    color: #1c2430;
}
QPushButton:hover { background-color: #f2f6fa; border-color: #c4cdd7; }
QPushButton:pressed { background-color: #dff3f8; border-color: #22b8cf; color: #0b7285; }
QPushButton:disabled { color: #a7b0ba; background-color: #f2f4f6; border-color: #e7eaee; }
QPushButton:focus { border-color: #22b8cf; }
QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #ffffff;
    border: 1px solid #d9dee5;
    border-radius: 7px;
    padding: 5px 9px;
    min-height: 20px;
    color: #1c2430;
    selection-background-color: #cbeef5;
    selection-color: #0b3b44;
}
QLineEdit:hover, QSpinBox:hover { border-color: #bcc7d2; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #22b8cf; }
QComboBox {
    background-color: #ffffff; border: 1px solid #d9dee5; border-radius: 7px;
    padding: 6px 10px; min-width: 88px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #ffffff; border: 1px solid #dfe3e8; border-radius: 8px;
    selection-background-color: #e3f4f8; selection-color: #0b7285; padding: 4px; outline: none;
}
QSlider::groove:horizontal { height: 4px; background: #e2e7ec; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #22b8cf; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #ffffff; border: 1px solid #22b8cf; width: 12px; height: 12px;
    margin: -5px 0; border-radius: 7px;
}
QGroupBox {
    border: 1px solid #e2e7ec; border-radius: 10px; margin-top: 15px;
    padding: 12px 12px 10px 12px; background-color: #ffffff;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left; left: 12px;
    padding: 0 6px; color: #0b7285; font-weight: 600;
}
QListWidget, QListView, QTreeWidget, QTableWidget {
    background-color: #ffffff; border: 1px solid #dfe3e8; border-radius: 8px;
    padding: 4px; outline: none; color: #354252;
}
QListWidget::item { padding: 5px 8px; border-radius: 5px; margin: 1px 2px; }
QListWidget::item:hover { background-color: #f2f6fa; }
QListWidget::item:selected { background-color: #e3f4f8; color: #0b7285; }
QWidget#widget QListWidget#historyList {
    background-color: #ffffff; min-height: 120px; padding: 4px;
    selection-background-color: #e3f4f8; selection-color: #0b7285;
}
QWidget#widget QLabel#label_layers_hint {
    color: #8a96a4; font-size: 12px;
    min-width: 0; max-width: 16777215px; padding: 8px 0;
}
QDialog { background-color: #f6f7f9; }

QLabel#imageLabel {
    background-color: #eceff3;
    border: 1px solid #d9dee5;
    border-radius: 12px;
    color: #8a96a4;
    font-size: 13px;
    letter-spacing: 1px;
}
QWidget#widget {
    background-color: #ffffff;
    border: 1px solid #e2e7ec;
    border-radius: 14px;
}
QWidget#widget QWidget { background: transparent; }
QWidget#widget QLabel { color: #5b6876; min-width: 104px; max-width: 104px; }
QLabel#label_sec_adjust, QLabel#label_sec_transform {
    color: #0b7285; font-size: 12px; font-weight: 600;
    letter-spacing: 2px; min-width: 0; max-width: 16777215px; padding: 6px 0 0 2px;
}
QWidget#widget QLineEdit { min-width: 78px; max-width: 92px; }
QWidget#widget QPushButton { min-width: 46px; max-width: 64px; }
QStatusBar { background-color: #ffffff; border-top: 1px solid #e7eaee; color: #6b7785; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: #6b7785; padding: 1px 10px; font-size: 12px; }
QLabel#statusInfoLabel {
    color: #5a6774;
    font-family: "Consolas", "Microsoft YaHei UI", monospace;
    font-size: 12px;
}
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #ccd3db; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: #b0b9c3; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #ccd3db; border-radius: 4px; min-width: 32px; }
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
