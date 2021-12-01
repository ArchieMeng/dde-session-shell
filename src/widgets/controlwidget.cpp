/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "controlwidget.h"
#include "sessionbasemodel.h"
#include "kblayoutlistview.h"

#include "mediawidget.h"
#include "modules_loader.h"
#include "tray_module_interface.h"

#include <DFloatingButton>
#include <DArrowRectangle>
#include <DPushButton>
#include <dimagebutton.h>

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QPainter>
#include <QWheelEvent>
#include <QMenu>

#define BUTTON_ICON_SIZE QSize(26,26)
#define BUTTON_SIZE QSize(52,52)

using namespace dss;

ControlWidget::ControlWidget(const SessionBaseModel *model, QWidget *parent)
    : QWidget(parent)
    , m_contextMenu(new QMenu(this))
    , m_tipsWidget(new DArrowRectangle(DArrowRectangle::ArrowDirection::ArrowBottom, this))
    , m_arrowRectWidget(new DArrowRectangle(DArrowRectangle::ArrowBottom, this))
    , m_kbLayoutListView(nullptr)
    , m_keyboardBtn(nullptr)
{
    setModel(model);
    initUI();
    initConnect();
}

void ControlWidget::setModel(const SessionBaseModel *model)
{
    m_model = model;
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &ControlWidget::setUser);
}

void ControlWidget::setUser(std::shared_ptr<User> user)
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connectionList))
        user.get()->disconnect(connection);

    m_connectionList.clear();
    m_connectionList << connect(user.get(), &User::keyboardLayoutChanged, this, &ControlWidget::setKeyboardType)
                     << connect(user.get(), &User::keyboardLayoutListChanged, this, &ControlWidget::setKeyboardTypeList);
}

void ControlWidget::initKeyboardLayoutList()
{
    /* 键盘布局选择菜单 */
    const QString language = m_model->currentUser()->keyboardLayout();
    m_kbLayoutListView = new KBLayoutListView(language, this);
    m_kbLayoutListView->setAccessibleName(QStringLiteral("KbLayoutlistview"));
    m_kbLayoutListView->updateButtonList(m_model->currentUser()->keyboardLayoutList());
    m_kbLayoutListView->setMinimumWidth(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH);
    m_kbLayoutListView->setMaximumSize(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH, DDESESSIONCC::LAYOUTBUTTON_HEIGHT * 7);
    m_kbLayoutListView->setFocusPolicy(Qt::NoFocus);

    const QStringList languageList = language.split(";");
    if (!languageList.isEmpty())
        static_cast<QAbstractButton *>(m_keyboardBtn)->setText(languageList.at(0));

    // 无特效模式时，让窗口圆角
    m_arrowRectWidget->setProperty("_d_radius_force", true);
    m_arrowRectWidget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    m_arrowRectWidget->setMargin(0);
    m_arrowRectWidget->setShadowBlurRadius(20);
    m_arrowRectWidget->setRadius(6);
    m_arrowRectWidget->setShadowYOffset(2);
    m_arrowRectWidget->setShadowXOffset(0);
    m_arrowRectWidget->setArrowWidth(18);
    m_arrowRectWidget->setArrowHeight(10);
    m_arrowRectWidget->setMinimumWidth(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH);
    m_arrowRectWidget->setMaximumSize(DDESESSIONCC::KEYBOARDLAYOUT_WIDTH, DDESESSIONCC::LAYOUTBUTTON_HEIGHT * 7);
    m_arrowRectWidget->setFocusPolicy(Qt::NoFocus);

    QPalette pal = m_arrowRectWidget->palette();
    pal.setColor(DPalette::Inactive, DPalette::Base, QColor(235, 235, 235, 0.05 * 255));
    setPalette(pal);

    m_arrowRectWidget->setContent(m_kbLayoutListView);
    connect(m_kbLayoutListView, &KBLayoutListView::itemClicked, this, &ControlWidget::setKeyboardType);
}

void ControlWidget::setVirtualKBVisible(bool visible)
{
    m_virtualKBBtn->setVisible(visible);
}

void ControlWidget::initUI()
{
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 60, 0);
    m_mainLayout->setSpacing(26);
    m_mainLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);

    m_keyboardBtn = new DFloatingButton(this);
    m_keyboardBtn->setAccessibleName("KeyboardLayoutBtn");
    m_keyboardBtn->setFixedSize(BUTTON_SIZE);
    m_keyboardBtn->setAutoExclusive(true);
    m_keyboardBtn->setBackgroundRole(DPalette::Button);
    m_keyboardBtn->setAutoExclusive(true);
    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(QString());
    m_keyboardBtn->installEventFilter(this);

    // 给显示文字的按钮设置样式
    QPalette pal = m_keyboardBtn->palette();
    pal.setColor(QPalette::Window, QColor(Qt::red));
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    pal.setColor(QPalette::Active, QPalette::Highlight, QColor(Qt::white));
    m_keyboardBtn->setPalette(pal);

    m_virtualKBBtn = new DFloatingButton(this);
    m_virtualKBBtn->setAccessibleName("VirtualKeyboardBtn");
    m_virtualKBBtn->setIcon(QIcon::fromTheme(":/img/screen_keyboard_normal.svg"));
    m_virtualKBBtn->hide();
    m_virtualKBBtn->setIconSize(BUTTON_ICON_SIZE);
    m_virtualKBBtn->setFixedSize(BUTTON_SIZE);
    m_virtualKBBtn->setAutoExclusive(true);
    m_virtualKBBtn->setBackgroundRole(DPalette::Button);
    m_virtualKBBtn->installEventFilter(this);

    m_switchUserBtn = new DFloatingButton(this);
    m_switchUserBtn->setAccessibleName("SwitchUserBtn");
    m_switchUserBtn->setIcon(QIcon::fromTheme(":/img/bottom_actions/userswitch_normal.svg"));
    m_switchUserBtn->setIconSize(BUTTON_ICON_SIZE);
    m_switchUserBtn->setFixedSize(BUTTON_SIZE);
    m_switchUserBtn->setAutoExclusive(true);
    m_switchUserBtn->setBackgroundRole(DPalette::Button);
    m_switchUserBtn->installEventFilter(this);

    m_powerBtn = new DFloatingButton(this);
    m_powerBtn->setAccessibleName("PowerBtn");
    m_powerBtn->setIcon(QIcon(":/img/bottom_actions/shutdown_normal.svg"));
    m_powerBtn->setIconSize(BUTTON_ICON_SIZE);
    m_powerBtn->setFixedSize(BUTTON_SIZE);
    m_powerBtn->setAutoExclusive(true);
    m_powerBtn->setBackgroundRole(DPalette::Button);
    m_powerBtn->installEventFilter(this);

    m_btnList.append(m_keyboardBtn);
    m_btnList.append(m_virtualKBBtn);
    m_btnList.append(m_switchUserBtn);
    m_btnList.append(m_powerBtn);

    m_mainLayout->addWidget(m_keyboardBtn);
    m_mainLayout->addWidget(m_virtualKBBtn);
    m_mainLayout->addWidget(m_switchUserBtn);
    m_mainLayout->addWidget(m_powerBtn);

    QHash<QString, module::BaseModuleInterface *> modules = module::ModulesLoader::instance().moduleList();
    for (module::BaseModuleInterface *module : modules.values()) {
        addModule(module);
    }

    updateLayout();

    // 初始化键盘布局列表
    initKeyboardLayoutList();
}

void ControlWidget::initConnect()
{
    connect(&module::ModulesLoader::instance(), &module::ModulesLoader::moduleFound, this, &ControlWidget::addModule);
    connect(m_switchUserBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchUser);
    connect(m_powerBtn, &DFloatingButton::clicked, this, &ControlWidget::requestShutdown);
    connect(m_virtualKBBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchVirtualKB);
    connect(m_keyboardBtn, &DFloatingButton::clicked, this, &ControlWidget::setKBLayoutVisible);
}

void ControlWidget::addModule(module::BaseModuleInterface *module)
{
    if (module && module->type() != module::BaseModuleInterface::TrayType)
        return;

    module::TrayModuleInterface *trayModule = dynamic_cast<module::TrayModuleInterface *>(module);
    if (!trayModule)
        return;

    trayModule->init();

    FlotingButton *button = new FlotingButton(this);
    button->setIconSize(QSize(26, 26));
    button->setFixedSize(QSize(52, 52));
    button->setAutoExclusive(true);
    button->setBackgroundRole(DPalette::Button);

    if (QWidget *trayWidget = trayModule->itemWidget()) {
        trayWidget->setParent(this);
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->addWidget(trayWidget);

        button->setLayout(layout);
    } else {
        button->setIcon(QIcon(trayModule->icon()));
    }

    m_modules.insert(trayModule->key(), button);
    m_btnList.append(button);

    connect(button, &FlotingButton::requestShowMenu, this, [ = ] {
        const QString menuJson = trayModule->itemContextMenu();
        if (menuJson.isEmpty())
            return;

        QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
        if (jsonDocument.isNull())
            return;

        QJsonObject jsonMenu = jsonDocument.object();

        m_contextMenu->clear();
        QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
        for (auto item : jsonMenuItems) {
            QJsonObject itemObj = item.toObject();
            QAction *action = new QAction(itemObj.value("itemText").toString());
            action->setCheckable(itemObj.value("isCheckable").toBool());
            action->setChecked(itemObj.value("checked").toBool());
            action->setData(itemObj.value("itemId").toString());
            action->setEnabled(itemObj.value("isActive").toBool());
            m_contextMenu->addAction(action);
        }

        connect(m_contextMenu, &QMenu::triggered, this, [ = ] (QAction *action) {
            trayModule->invokedMenuItem( action->data().toString(), true);
        });
        m_contextMenu->exec(QCursor::pos());
    });

    connect(button, &FlotingButton::requestShowTips, this, [ = ] {
        if (trayModule->itemTipsWidget()) {
            m_tipsWidget->setContent(trayModule->itemTipsWidget());
            m_tipsWidget->show(mapToGlobal(button->pos()).x() + button->width() / 2,mapToGlobal(button->pos()).y());
        }
    });

    connect(button, &FlotingButton::requestHideTips, this, [ = ] {
        if (m_tipsWidget->getContent())
            m_tipsWidget->getContent()->setVisible(false);
        m_tipsWidget->hide();
    });

    connect(button, &DFloatingButton::clicked, this, [this, trayModule] {
        emit requestShowModule(trayModule->key());
    }, Qt::UniqueConnection);


    updateLayout();
}

void ControlWidget::removeModule(module::BaseModuleInterface *module)
{
    if (module->type() != module::BaseModuleInterface::TrayType)
        return;

    module::TrayModuleInterface *trayModule = dynamic_cast<module::TrayModuleInterface *>(module);

    QMap<QString, QWidget *>::const_iterator i = m_modules.constBegin();
    while (i != m_modules.constEnd()) {
        if (i.key() == trayModule->key()) {
            m_modules.remove(i.key());
            m_btnList.removeAll(dynamic_cast<DFloatingButton *>(i.value()));
            updateLayout();
            break;
        }
        ++i;
    }
}

void ControlWidget::updateLayout()
{
    QObjectList moduleWidgetList = m_mainLayout->children();
    for (QWidget *moduleWidget : m_modules.values()) {
        if (moduleWidgetList.contains(moduleWidget)) {
            moduleWidgetList.removeAll(moduleWidget);
        }
        m_mainLayout->insertWidget(0, moduleWidget);
    }
    for (QObject *moduleWidget : moduleWidgetList) {
        m_mainLayout->removeWidget(qobject_cast<QWidget *>(moduleWidget));
    }

    updateTapOrder();
}

void ControlWidget::showTips()
{
#ifndef SHENWEI_PLATFORM
    m_tipsAni->setStartValue(QPoint(m_tipWidget->width(), 0));
    m_tipsAni->setEndValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(0, 0);
#endif
}

void ControlWidget::hideTips()
{
#ifndef SHENWEI_PLATFORM
    //在退出动画时候会出现白边，+1
    m_tipsAni->setEndValue(QPoint(m_tipWidget->width() + 1, 0));
    m_tipsAni->setStartValue(QPoint());
    m_tipsAni->start();
#else
    m_sessionTip->move(m_tipWidget->width() + 1, 0);
#endif
}

void ControlWidget::setUserSwitchEnable(const bool visible)
{
    m_switchUserBtn->setVisible(visible);
    if (m_btnList.indexOf(m_switchUserBtn) == m_index) {
        m_index = 0;
    }
}

void ControlWidget::setSessionSwitchEnable(const bool visible)
{
    if (!visible) return;

    if (!m_sessionBtn) {
        m_sessionBtn = new DFloatingButton(this);
        m_sessionBtn->setIconSize(BUTTON_ICON_SIZE);
        m_sessionBtn->setFixedSize(BUTTON_SIZE);
        m_sessionBtn->setAutoExclusive(true);
        m_sessionBtn->setBackgroundRole(DPalette::Button);
#ifndef SHENWEI_PLATFORM
        m_sessionBtn->installEventFilter(this);
#else
        m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
        m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
        m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");

#endif

        m_mainLayout->insertWidget(1, m_sessionBtn);
        m_mainLayout->setAlignment(m_sessionBtn, Qt::AlignBottom);
        m_btnList.push_front(m_sessionBtn);

        connect(m_sessionBtn, &DFloatingButton::clicked, this, &ControlWidget::requestSwitchSession);
    }

    if (!m_tipWidget) {
        m_tipWidget = new QWidget;
        m_tipWidget->setAccessibleName("TipWidget");
        m_mainLayout->insertWidget(1, m_tipWidget);
        m_mainLayout->setAlignment(m_tipWidget, Qt::AlignCenter);
    }

    if (!m_sessionTip) {
        m_sessionTip = new QLabel(m_tipWidget);
        m_sessionTip->setAccessibleName("SessionTip");
        m_sessionTip->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_sessionTip->installEventFilter(this);

#ifndef SHENWEI_PLATFORM
        m_sessionTip->setStyleSheet("color:white;"
                                    "font-size:12px;");
#else
        QPalette pe;
        pe.setColor(QPalette::WindowText, Qt::white);
        m_sessionTip->setPalette(pe);
#endif

        QGraphicsDropShadowEffect *tipsShadow = new QGraphicsDropShadowEffect(m_sessionTip);
        tipsShadow->setColor(Qt::white);
        tipsShadow->setBlurRadius(4);
        tipsShadow->setOffset(0, 0);
        m_sessionTip->setGraphicsEffect(tipsShadow);

        m_sessionTip->move(m_tipWidget->width(), 0);
    }

#ifndef SHENWEI_PLATFORM
    if (!m_tipsAni) {
        m_tipsAni = new QPropertyAnimation(m_sessionTip, "pos", this);
    }
#endif

    updateTapOrder();
}

void ControlWidget::chooseToSession(const QString &session)
{
    if (m_sessionBtn && m_sessionTip) {
        qDebug() << "chosen session: " << session;
        if (session.isEmpty())
            return;

        m_sessionTip->setText(session);
        if (session == "deepin") {
            m_sessionTip->setText("X11");
        }

        m_sessionTip->adjustSize();
        //当session长度改变时，应该移到它的width来隐藏
        m_sessionTip->move(m_sessionTip->size().width() + 1, 0);
        const QString sessionId = session.toLower();
        QString normalIcon = QString(":/img/sessions/%1_indicator_normal.svg").arg(sessionId);
        if (sessionId == "deepin") {
            normalIcon = QString(":/img/sessions/%1_indicator_normal.svg").arg("x11");
        }

        if (QFile(normalIcon).exists()) {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(normalIcon));
#else
            m_sessionBtn->setProperty("normalIcon", normalIcon);
            m_sessionBtn->setProperty("hoverIcon", hoverIcon);
            m_sessionBtn->setProperty("checkedIcon", checkedIcon);
#endif
        } else {
#ifndef SHENWEI_PLATFORM
            m_sessionBtn->setIcon(QIcon::fromTheme(":/img/sessions/unknown_indicator_normal.svg"));
#else
            m_sessionBtn->setProperty("normalIcon", ":/img/sessions/unknown_indicator_normal.svg");
            m_sessionBtn->setProperty("hoverIcon", ":/img/sessions/unknown_indicator_hover.svg");
            m_sessionBtn->setProperty("checkedIcon", ":/img/sessions/unknown_indicator_press.svg");
#endif
        }
    }
}

void ControlWidget::leftKeySwitch()
{
    if (m_index == 0) {
        m_index = m_btnList.length() - 1;
    } else {
        --m_index;
    }

    for (int i = m_btnList.size(); i != 0; --i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

void ControlWidget::rightKeySwitch()
{
    if (m_index == m_btnList.size() - 1) {
        m_index = 0;
    } else {
        ++m_index;
    }

    for (int i = 0; i < m_btnList.size(); ++i) {
        int index = (m_index + i) % m_btnList.size();

        if (m_btnList[index]->isVisible()) {
            m_index = index;
            break;
        }
    }

    m_btnList.at(m_index)->setFocus();
}

void ControlWidget::setKBLayoutVisible()
{
    DFloatingButton *layoutButton = static_cast<DFloatingButton *>(sender());
    if (!layoutButton)
        return;

    QPoint pos = mapToGlobal(layoutButton->pos()) - QPoint((m_arrowRectWidget->width() - layoutButton->width())/2, m_arrowRectWidget->height());
    m_arrowRectWidget->setGeometry(QRect(pos, m_arrowRectWidget->size()));
    m_arrowRectWidget->setVisible(!m_arrowRectWidget->isVisible());
}

void ControlWidget::setKeyboardType(const QString &str)
{
    // 初始化当前键盘布局输入法类型
    QString currentText = str.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/"))
        currentText = currentText.split("/").last();

    static_cast<QAbstractButton *>(m_keyboardBtn)->setText(currentText);

    m_arrowRectWidget->hide();
    m_model->currentUser()->setKeyboardLayout(str);
}

void ControlWidget::setKeyboardTypeList(const QStringList &str)
{
    // 初始化当前键盘布局列表
    if (m_kbLayoutListView)
        m_kbLayoutListView->updateButtonList(str);
}

bool ControlWidget::eventFilter(QObject *watched, QEvent *event)
{
#ifndef SHENWEI_PLATFORM
    if (watched == m_sessionBtn) {
        if (event->type() == QEvent::Enter)
            showTips();
        else if (event->type() == QEvent::Leave)
            hideTips();
    }

    if (watched == m_sessionTip) {
        if (event->type() == QEvent::Resize) {
            m_tipWidget->setFixedSize(m_sessionTip->size());
        }
    }

    DFloatingButton *btn = dynamic_cast<DFloatingButton *>(watched);
    if (m_btnList.contains(btn)) {
        if (event->type() == QEvent::Enter) {
            m_index = m_btnList.indexOf(btn);
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return false;
}

void ControlWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        leftKeySwitch();
        break;
    case Qt::Key_Right:
        rightKeySwitch();
        break;
    default:
        break;
    }
}

void ControlWidget::updateTapOrder()
{
    // 找出所有按钮
    QList<DFloatingButton*> buttons;
    for(int i = 0; i < m_mainLayout->count(); ++i) {
        DFloatingButton *btn = qobject_cast<DFloatingButton *>(m_mainLayout->itemAt(i)->widget());
        if (btn)
            buttons.append(btn);
    }

    // 按从左到右的顺序设置tab order
    for (int i = 0; i < buttons.size(); ++i) {
        if ((i + 1) < buttons.size())
            setTabOrder(buttons[i], buttons[i + 1]);
    }
}
