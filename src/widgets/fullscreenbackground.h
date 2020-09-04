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

#ifndef FULLSCREENBACKGROUND_H
#define FULLSCREENBACKGROUND_H

#include <QWidget>
#include <QPointer>
#include <QTimer>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QVariantAnimation>

#include <com_deepin_daemon_imageeffect.h>

using ImageEffectInter = com::deepin::daemon::ImageEffect;

class FullscreenBackground : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool contentVisible READ contentVisible WRITE setContentVisible NOTIFY contentVisibleChanged)

public:
    enum DisplayMode {
        CopyMode = 1,
        ExpandMode = 2
    };

    explicit FullscreenBackground(QWidget *parent = nullptr);
    ~FullscreenBackground();
    bool contentVisible() const;

public slots:
    void updateBackground();
    void updateBackground(const QString &file);
    void setScreen(QScreen *screen);
    void setContentVisible(bool contentVisible);
    void setIsBlackMode(bool is_black);
    void setIsHibernateMode();

signals:
    void contentVisibleChanged(bool contentVisible);

protected:
    void setContent(QWidget * const w);
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *e) override;

private:
    void paintEvent(QPaintEvent *e) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    const QPixmap pixmapHandle(const QPixmap &pixmap);
    // 通过指定文件路径获取背景图片，默认共享内存获取，失败则从文件或取
    const QPixmap getPixmapByPath(const QString &path);
    void loadPixmap();
    void releasePixmap();

private:
    void updateScreen(QScreen *screen);
    QString getBlurBackground(const QString &file);
    void updateGeometry();
    using QWidget::setGeometry;
    using QWidget::resize;
    using QWidget::move;

    bool m_pixmapLoaded;
    QPixmap m_background;
    QPixmap m_fakeBackground;
    QPixmap m_backgroundCache;
    QPixmap m_fakeBackgroundCache;
    QString m_backgroundPath;
    QString m_fakePath;
    QPointer<QWidget> m_content;
    QVariantAnimation *m_fadeOutAni;
    QScreen *m_screen = nullptr;
    ImageEffectInter *m_imageEffectInter = nullptr;
    bool m_primaryShowFinished = false;
    bool m_isBlackMode = false;
    bool m_isHibernateMode = false;
};

#endif // FULLSCREENBACKGROUND_H
