#include "lockcontent.h"

#include "src/widgets/controlwidget.h"
#include "sessionbasemodel.h"
#include "userframe.h"
#include "src/widgets/shutdownwidget.h"
#include "src/widgets/virtualkbinstance.h"
#include "src/widgets/logowidget.h"
#include "src/widgets/timewidget.h"
#include "userlogininfo.h"
#include "userloginwidget.h"
#include "userframelist.h"

#include <QMouseEvent>
#include <DDBusSender>

LockContent::LockContent(SessionBaseModel *const model, QWidget *parent)
    : SessionBaseWindow(parent)
    , m_model(model)
    , m_virtualKB(nullptr)
    , m_translator(new QTranslator)
    , m_userLoginInfo(new UserLoginInfo(model))
    , m_wmInter(new com::deepin::wm("com.deepin.wm", "/com/deepin/wm", QDBusConnection::sessionBus(), this))
    , m_greeterBackgroundPath(QString())
    , m_desktopBackgroundPath(QString())
{
    QTimer::singleShot(0, this, [ = ] {
        auto user = model->currentUser();
        if (user != nullptr) {
            updateGreeterBackgroundPath(user->greeterBackgroundPath());
            updateDesktopBackgroundPath(user->desktopBackgroundPath());
        }
    });

    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

    m_timeWidget = new TimeWidget();
    setCenterTopWidget(m_timeWidget);
    // 处理时间制跳转策略，获取到时间制再显示时间窗口
    m_timeWidget->setVisible(false);

    m_shutdownFrame = new ShutdownWidget;
    m_shutdownFrame->setModel(model);

    m_logoWidget = new LogoWidget;
    setLeftBottomWidget(m_logoWidget);

    m_controlWidget = new ControlWidget;
    setRightBottomWidget(m_controlWidget);

    switch (model->currentType()) {
    case SessionBaseModel::AuthType::LockType:
        setMPRISEnable(model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode);
        break;
    default:
        break;
    }

    // init connect
    connect(model, &SessionBaseModel::currentUserChanged, this, &LockContent::onCurrentUserChanged);
    connect(m_controlWidget, &ControlWidget::requestSwitchUser, this, [ = ] {
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::UserMode) return;
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
    });
    connect(m_controlWidget, &ControlWidget::requestShutdown, this, [ = ] {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PowerMode);
    });
    connect(m_controlWidget, &ControlWidget::requestSwitchVirtualKB, this, &LockContent::toggleVirtualKB);
    connect(m_userLoginInfo, &UserLoginInfo::requestAuthUser, this, &LockContent::requestAuthUser);
    connect(m_userLoginInfo, &UserLoginInfo::hideUserFrameList, this, &LockContent::restoreMode);
    connect(m_userLoginInfo, &UserLoginInfo::requestSwitchUser, this, &LockContent::requestSwitchToUser);
    connect(m_userLoginInfo, &UserLoginInfo::switchToCurrentUser, this, &LockContent::restoreMode);
    connect(m_userLoginInfo, &UserLoginInfo::requestSetLayout, this, &LockContent::requestSetLayout);
    connect(m_userLoginInfo, &UserLoginInfo::unlockActionFinish, this, [&]{
        emit unlockActionFinish();
    });
    connect(m_shutdownFrame, &ShutdownWidget::abortOperation, this, [ = ] {
            restoreMode();
    });

    if (m_model->currentType() == SessionBaseModel::LockType) {
        // 锁屏，点击关机，需要提示“输入密码以关机”。登录不需要这个提示
        connect(m_shutdownFrame, &ShutdownWidget::abortOperation, m_userLoginInfo, [ = ] {
            if (m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode) {
                m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
            }
        });
    }

    connect(model, &SessionBaseModel::onStatusChanged, this, &LockContent::onStatusChanged);

    //在锁屏显示时，启动onborad进程，锁屏结束时结束onboard进程
    auto initVirtualKB = [&](bool hasvirtualkb) {
        if (hasvirtualkb && !m_virtualKB) {
            connect(&VirtualKBInstance::Instance(), &VirtualKBInstance::initFinished, this, [&] {
                m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
                m_controlWidget->setVirtualKBVisible(true);
            }, Qt::QueuedConnection);
            VirtualKBInstance::Instance().init();
        } else {
            VirtualKBInstance::Instance().stopVirtualKBProcess();
            m_virtualKB = nullptr;
            m_controlWidget->setVirtualKBVisible(false);
        }
    };

    connect(model, &SessionBaseModel::hasVirtualKBChanged, this, initVirtualKB, Qt::QueuedConnection);
    connect(model, &SessionBaseModel::onUserListChanged, this, &LockContent::onUserListChanged);
    connect(model, &SessionBaseModel::userListLoginedChanged, this, &LockContent::onUserListChanged);
    connect(model, &SessionBaseModel::authFinished, this, &LockContent::restoreMode);
    connect(model, &SessionBaseModel::switchUserFinished, this, [ = ] {
        QTimer::singleShot(100, this, [ = ] {
            emit LockContent::restoreMode();
        });
    });

    QTimer::singleShot(0, this, [ = ] {
        onCurrentUserChanged(model->currentUser());
        onUserListChanged(model->isServerModel() ? model->logindUser() : model->userList());
    });

    connect(m_wmInter, &__wm::WorkspaceSwitched, this, &LockContent::currentWorkspaceChanged);
}

void LockContent::onCurrentUserChanged(std::shared_ptr<User> user)
{
    if (user.get() == nullptr) return; // if dbus is async

    // set user language
    qApp->removeTranslator(m_translator);
    const QString locale { user->locale().split(".").first() };
    m_translator->load("/usr/share/dde-session-shell/translations/dde-session-shell_" + QLocale(locale.isEmpty() ? "en_US" : locale).name());
    qApp->installTranslator(m_translator);

    m_logoWidget->updateLocale(user->locale());
    m_timeWidget->updateLocale(user->locale());

    for (auto connect : m_currentUserConnects) {
        m_user.get()->disconnect(connect);
    }

    m_currentUserConnects.clear();

    m_user = user;

    std::shared_ptr<NativeUser> nativeUser = std::dynamic_pointer_cast<NativeUser>(user);
    UserInter *userInter = nullptr;
    if (!nativeUser) {
        m_timeWidget->setVisible(true);
        qDebug() << "trans to NativeUser user failed, user:" << user->metaObject()->className();
    } else {
        userInter = nativeUser->getUserInter();
    }

    m_currentUserConnects << connect(user.get(), &User::greeterBackgroundPathChanged, this, &LockContent::updateGreeterBackgroundPath, Qt::UniqueConnection)
                          << connect(user.get(), &User::desktopBackgroundPathChanged, this, &LockContent::updateDesktopBackgroundPath, Qt::UniqueConnection)
                          << connect(user.get(), &User::use24HourFormatChanged, this, &LockContent::updateTimeFormat, Qt::UniqueConnection)
                          << connect(userInter, &UserInter::WeekdayFormatChanged, m_timeWidget, &TimeWidget::setWeekdayFormatType)
                          << connect(userInter, &UserInter::ShortDateFormatChanged, m_timeWidget, &TimeWidget::setShortDateFormat)
                          << connect(userInter, &UserInter::ShortTimeFormatChanged, m_timeWidget, &TimeWidget::setShortTimeFormat);

    //lixin
    m_userLoginInfo->setUser(user);

    //TODO: refresh blur image
    QTimer::singleShot(0, this, [ = ] {
        if (userInter) {
            userInter->weekdayFormat();
            userInter->shortDateFormat();
            userInter->shortTimeFormat();
        }

        // 异步刷新界面时间格式
        user->is24HourFormat();

        //获取用户后，刷新界面背景图片
        m_user->greeterBackgroundPath();
        m_user->desktopBackgroundPath();
    });

    m_logoWidget->updateLocale(m_user->locale());
}

void LockContent::pushPasswordFrame()
{
    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
        m_model->setAbortConfirm(false);
    } else {
        restoreMode();
        setCenterContent(m_userLoginInfo->getUserLoginWidget());
    }

    // hide keyboardlayout widget
    m_userLoginInfo->hideKBLayout();
}

void LockContent::pushUserFrame()
{
    if(m_model->isServerModel())
        m_controlWidget->setUserSwitchEnable(false);
    //设置用户列表大小为中间区域的大小
    UserFrameList * userFrameList = m_userLoginInfo->getUserFrameList();
    userFrameList->setFixedSize(getCenterContentSize());
    setCenterContent(m_userLoginInfo->getUserFrameList());
}

void LockContent::pushConfirmFrame()
{
    setCenterContent(m_userLoginInfo->getUserLoginWidget());
    m_model->setAbortConfirm(true);
}

void LockContent::pushShutdownFrame()
{
    QSize size = getCenterContentSize();
    m_shutdownFrame->setFixedSize(size);
    m_shutdownFrame->onStatusChanged(m_model->currentModeState());
    setCenterContent(m_shutdownFrame);
}

void LockContent::setMPRISEnable(const bool state)
{
    if (!m_mediaWidget) {
        m_mediaWidget = new MediaWidget;
        m_mediaWidget->initMediaPlayer();
    }

    m_mediaWidget->setVisible(state);
    setCenterBottomWidget(m_mediaWidget);
}

void LockContent::beforeUnlockAction(bool is_finish)
{
    m_userLoginInfo->beforeUnlockAction(is_finish);
}

void LockContent::onStatusChanged(SessionBaseModel::ModeStatus status)
{
    refreshBackground(status);
    refreshLayout(status);

    if(m_model->isServerModel())
        onUserListChanged(m_model->logindUser());
    switch (status) {
    case SessionBaseModel::ModeStatus::PasswordMode:
        restoreCenterContent();
        break;
    case SessionBaseModel::ModeStatus::ConfirmPasswordMode:
        pushConfirmFrame();
        break;
    case SessionBaseModel::ModeStatus::UserMode:
        pushUserFrame();
        break;
    case SessionBaseModel::ModeStatus::PowerMode:
    case SessionBaseModel::ModeStatus::ShutDownMode:
        pushShutdownFrame();
        break;
    default:
        break;
    }
}

void LockContent::mouseReleaseEvent(QMouseEvent *event)
{
    //在关机界面没有点击按钮直接点击界面时，直接隐藏关机界面
    if (m_model->currentModeState() == SessionBaseModel::ShutDownMode) {
        hideToplevelWindow();
    }

    pushPasswordFrame();

    return SessionBaseWindow::mouseReleaseEvent(event);
}

void LockContent::showEvent(QShowEvent *event)
{
    onStatusChanged(m_model->currentModeState());

    tryGrabKeyboard();
    return QFrame::showEvent(event);
}

void LockContent::hideEvent(QHideEvent *event)
{
    m_shutdownFrame->recoveryLayout();
    return QFrame::hideEvent(event);
}

void LockContent::resizeEvent(QResizeEvent *event)
{
    QTimer::singleShot(0, this, [ = ] {
        if (m_virtualKB && m_virtualKB->isVisible())
        {
            updateVirtualKBPosition();
        }
    });

    return QFrame::resizeEvent(event);
}

void LockContent::restoreCenterContent()
{
    auto current_user = m_model->currentUser();
    if (current_user != nullptr && current_user->isLock()) {
        current_user->onLockTimeOut();
    }
    if ((m_model->powerAction() == SessionBaseModel::RequireShutdown) ||
        (m_model->powerAction() == SessionBaseModel::RequireRestart)) {
        m_model->setCurrentModeState(SessionBaseModel::ModeStatus::ConfirmPasswordMode);
    } else {
        restoreMode();
    }

    setCenterContent(m_userLoginInfo->getUserLoginWidget());
}

void LockContent::restoreMode()
{
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
}

void LockContent::updateGreeterBackgroundPath(const QString &path)
{
    m_greeterBackgroundPath = path;

    if (m_greeterBackgroundPath.isEmpty()) {
        return;
    }

    if (m_model->currentModeState() != SessionBaseModel::ModeStatus::ShutDownMode) {
        emit requestBackground(m_greeterBackgroundPath);
    }
}

void LockContent::updateDesktopBackgroundPath(const QString &path)
{
    m_desktopBackgroundPath = path;

    if (m_desktopBackgroundPath.isEmpty()) {
        return;
    }

    if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
        emit requestBackground(m_desktopBackgroundPath);
    }
}

void LockContent::updateTimeFormat(bool use24)
{
    if (m_user != nullptr) {
        m_timeWidget->updateLocale(m_user->locale());
        m_timeWidget->set24HourFormat(use24);
        m_timeWidget->setVisible(true);
    }
}

void LockContent::toggleVirtualKB()
{
    if (!m_virtualKB) {
        VirtualKBInstance::Instance();
        QTimer::singleShot(500, this, [ = ] {
            m_virtualKB = VirtualKBInstance::Instance().virtualKBWidget();
            qDebug() << "init virtualKB over." << m_virtualKB;
            toggleVirtualKB();
        });
        return;
    }

    m_virtualKB->setParent(this);
    m_virtualKB->raise();
    m_userLoginInfo->getUserLoginWidget()->setPassWordEditFocus();

    updateVirtualKBPosition();
    m_virtualKB->setVisible(!m_virtualKB->isVisible());
}

void LockContent::updateVirtualKBPosition()
{
    const QPoint point = mapToParent(QPoint((width() - m_virtualKB->width()) / 2, height() - m_virtualKB->height() - 50));
    m_virtualKB->move(point);
}

void LockContent::onUserListChanged(QList<std::shared_ptr<User> > list)
{
    const bool allowShowUserSwitchButton = m_model->allowShowUserSwitchButton();
    const bool alwaysShowUserSwitchButton = m_model->alwaysShowUserSwitchButton();
    bool haveLogindUser = true;

    if (m_model->isServerModel() && m_model->currentType() == SessionBaseModel::LightdmType) {
        haveLogindUser = !m_model->logindUser().isEmpty();
    }
    m_controlWidget->setUserSwitchEnable((alwaysShowUserSwitchButton ||
                                          (allowShowUserSwitchButton &&
                                           (list.size() > (m_model->isServerModel() ? 0 : 1)))) &&
                                         haveLogindUser);
}

void LockContent::tryGrabKeyboard()
{
#ifdef QT_DEBUG
    return;
#endif
    if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(true)) {
        m_failures = 0;
        return;
    } else {
        // 通过以下指令暂时解除grab状态, 在有右键弹窗时可以锁屏
        QProcess process;
        process.start("/usr/bin/setxkbmap", QStringList() << "-option" << "grab:break_actions");
        process.waitForFinished();
        process.start("/usr/bin/xdotool", QStringList() << "key" << "XF86Ungrab");
        process.waitForFinished();
    }

    m_failures++;

    if (m_failures == 15) {
        qDebug() << "Trying grabkeyboard has exceeded the upper limit. dde-lock will quit.";
        m_model->setLocked(false);

        DDBusSender()
                .service("org.freedesktop.Notifications")
                .path("/org/freedesktop/Notifications")
                .interface("org.freedesktop.Notifications")
                .method(QString("Notify"))
                .arg(tr("Lock Screen"))
                .arg(static_cast<uint>(0))
                .arg(QString(""))
                .arg(QString(""))
                .arg(tr("Failed to lock screen"))
                .arg(QStringList())
                .arg(QVariantMap())
                .arg(5000)
                .call();

        qApp->quit();
        return;
    }

    QTimer::singleShot(100, this, &LockContent::tryGrabKeyboard);
}

void LockContent::hideToplevelWindow()
{
    QWidgetList widgets = qApp->topLevelWidgets();
    for (QWidget *widget : widgets) {
        if (widget->isVisible()) {
            widget->hide();
        }
    }
}

void LockContent::currentWorkspaceChanged()
{
    QDBusPendingCall call = m_wmInter->GetCurrentWorkspaceBackgroundForMonitor(QGuiApplication::primaryScreen()->name());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [ = ] {
        if (!call.isError())
        {
            QDBusReply<QString> reply = call.reply();
            updateWallpaper(reply.value());
        } else
        {
            qWarning() << "get current workspace background error: " << call.error().message();
            updateWallpaper("/usr/share/backgrounds/deepin/desktop.jpg");
        }

        watcher->deleteLater();
    });
}

void LockContent::updateWallpaper(const QString &path)
{
    const QUrl url(path);
    QString wallpaper = path;
    if (url.isLocalFile()) {
        wallpaper = url.path();
    }

    updateDesktopBackgroundPath(wallpaper);
}

void LockContent::refreshBackground(SessionBaseModel::ModeStatus status)
{
    if (status == SessionBaseModel::ModeStatus::ShutDownMode) {
        emit requestBackground(m_desktopBackgroundPath);
    } else {
        emit requestBackground(m_greeterBackgroundPath);
    }
}

void LockContent::refreshLayout(SessionBaseModel::ModeStatus status)
{
    setTopFrameVisible(status != SessionBaseModel::ModeStatus::ShutDownMode);
    setBottomFrameVisible(status != SessionBaseModel::ModeStatus::ShutDownMode);
}

void LockContent::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape: {
        if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ConfirmPasswordMode) {
            m_model->setAbortConfirm(false);
        } else if (m_model->currentModeState() == SessionBaseModel::ModeStatus::ShutDownMode) {
            hideToplevelWindow();
        }
        break;
    }
    }
}
