/* pinentrydialog.cpp - A (not yet) secure Qt 4 dialog for PIN entry.
 * Copyright (C) 2002, 2008 Klarälvdalens Datakonsult AB (KDAB)
 * Copyright 2007 Ingo Klöcker
 * Copyright 2016 Intevation GmbH
 * Copyright (C) 2021 g10 Code GmbH
 *
 * Written by Steffen Hansen <steffen@klaralvdalens-datakonsult.se>.
 * Modified by Andre Heinecke <aheinecke@intevation.de>
 * Software engineering by Ingo Klöcker <dev@ingo-kloecker.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "pinentrydialog.h"

#include "capslock.h"
#include "pinlineedit.h"

#include <QGridLayout>
#include <QProgressBar>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>
#include <QPainter>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QLineEdit>
#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#if QT_VERSION >= 0x050700
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif
#endif

void raiseWindow(QWidget *w)
{
#ifdef Q_OS_WIN
#if QT_VERSION >= 0x050700
    QWindowsWindowFunctions::setWindowActivationBehavior(
            QWindowsWindowFunctions::AlwaysActivateWindow);
#endif
#endif
    w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    w->activateWindow();
    w->raise();
}

QPixmap icon(QStyle::StandardPixmap which)
{
    QPixmap pm = qApp->windowIcon().pixmap(48, 48);

    if (which != QStyle::SP_CustomBase) {
        const QIcon ic = qApp->style()->standardIcon(which);
        QPainter painter(&pm);
        const int emblemSize = 22;
        painter.drawPixmap(pm.width() - emblemSize, 0,
                           ic.pixmap(emblemSize, emblemSize));
    }

    return pm;
}

void PinEntryDialog::slotTimeout()
{
    _timed_out = true;
    reject();
}

PinEntryDialog::PinEntryDialog(QWidget *parent, const char *name,
                               int timeout, bool modal, bool enable_quality_bar,
                               const QString &repeatString,
                               const QString &visibilityTT,
                               const QString &hideTT)
    : QDialog(parent),
      mRepeat(NULL),
      _grabbed(false),
      _disable_echo_allowed(true),
      mVisibilityTT(visibilityTT),
      mHideTT(hideTT),
      mVisiActionEdit(NULL),
      mGenerateActionEdit(NULL),
      mVisiCB(NULL),
      mFormattedPassphraseCB(NULL),
      mFormattedPassphraseHint(NULL),
      mFormattedPassphraseHintSpacer(NULL),
      mCapsLockHint(NULL)
{
    _timed_out = false;

    if (modal) {
        setWindowModality(Qt::ApplicationModal);
    }

    _icon = new QLabel(this);
    _icon->setPixmap(icon());

    _error = new QLabel(this);
    QPalette pal;
    pal.setColor(QPalette::WindowText, Qt::red);
    _error->setPalette(pal);
    _error->hide();

    mCapsLockHint = new QLabel(this);
    mCapsLockHint->setPalette(pal);
    mCapsLockHint->setAlignment(Qt::AlignCenter);
    mCapsLockHint->setVisible(false);

    _desc = new QLabel(this);
    _desc->hide();

    _prompt = new QLabel(this);
    _prompt->hide();

    _edit = new PinLineEdit(this);
    _edit->setMaxLength(256);
    _edit->setMinimumWidth(_edit->fontMetrics().averageCharWidth()*20 + 48);
    _edit->setEchoMode(QLineEdit::Password);

    _prompt->setBuddy(_edit);

    if (!repeatString.isNull()) {
        mRepeat = new PinLineEdit(this);
    }

    if (enable_quality_bar) {
        _quality_bar_label = new QLabel(this);
        _quality_bar_label->setAlignment(Qt::AlignVCenter);
        _quality_bar = new QProgressBar(this);
        _quality_bar->setAlignment(Qt::AlignCenter);
        _have_quality_bar = true;
    } else {
        _have_quality_bar = false;
    }

    mFormattedPassphraseCB = new QCheckBox{this};

    QDialogButtonBox *const buttons = new QDialogButtonBox(this);
    buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    _ok = buttons->button(QDialogButtonBox::Ok);
    _cancel = buttons->button(QDialogButtonBox::Cancel);

    _ok->setDefault(true);

    if (style()->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons)) {
        _ok->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
        _cancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    }

    if (timeout > 0) {
        _timer = new QTimer(this);
        connect(_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
        _timer->start(timeout * 1000);
    } else {
        _timer = NULL;
    }

    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_edit, SIGNAL(textChanged(QString)),
            this, SLOT(updateQuality(QString)));
    connect(_edit, SIGNAL(textChanged(QString)),
            this, SLOT(textChanged(QString)));
    connect(_edit, SIGNAL(backspacePressed()),
            this, SLOT(onBackspace()));

    auto *const mainLayout = new QVBoxLayout{this};

    auto *const hbox = new QHBoxLayout;

    hbox->addWidget(_icon, 0, Qt::AlignVCenter | Qt::AlignLeft);

    auto *const grid = new QGridLayout;
    int row = 1;
    grid->addWidget(_error, row++, 1, 1, 2);
    grid->addWidget(_desc,  row++, 1, 1, 2);
    grid->addWidget(mCapsLockHint, row++, 1, 1, 2);
    grid->addWidget(_prompt, row, 1);
    grid->addWidget(_edit, row++, 2);

    mFormattedPassphraseHintSpacer = new QLabel;
    mFormattedPassphraseHintSpacer->setVisible(false);
    mFormattedPassphraseHint = new QLabel;
    mFormattedPassphraseHint->setVisible(false);
    grid->addWidget(mFormattedPassphraseHintSpacer, row, 1);
    grid->addWidget(mFormattedPassphraseHint, row++, 2);

    if (mRepeat) {
        mRepeat->setMaxLength(256);
        mRepeat->setEchoMode(QLineEdit::Password);
        connect(mRepeat, SIGNAL(textChanged(QString)),
                this, SLOT(textChanged(QString)));
        QLabel *repeatLabel = new QLabel(repeatString);
        repeatLabel->setBuddy(mRepeat);
        grid->addWidget(repeatLabel, row, 1);
        grid->addWidget(mRepeat, row++, 2);
    }
    if (enable_quality_bar) {
        grid->addWidget(_quality_bar_label, row, 1);
        grid->addWidget(_quality_bar, row++, 2);
    }
    /* Set up the show password action */
    const QIcon visibilityIcon = QIcon::fromTheme(QLatin1String("visibility"));
    const QIcon hideIcon = QIcon::fromTheme(QLatin1String("hint"));
    const QIcon generateIcon = QIcon::fromTheme(QLatin1String("password-generate"));
#if QT_VERSION >= 0x050200
    if (!generateIcon.isNull()) {
        mGenerateActionEdit = _edit->addAction(generateIcon,
                                               QLineEdit::LeadingPosition);
        mGenerateActionEdit->setToolTip(mGenerateTT);
        connect(mGenerateActionEdit, SIGNAL(triggered()), this, SLOT(generatePin()));
    }
    if (!visibilityIcon.isNull() && !hideIcon.isNull()) {
        mVisiActionEdit = _edit->addAction(visibilityIcon, QLineEdit::TrailingPosition);
        mVisiActionEdit->setVisible(false);
        mVisiActionEdit->setToolTip(mVisibilityTT);
        connect(mVisiActionEdit, SIGNAL(triggered()), this, SLOT(toggleVisibility()));
    } else
#endif
    {
        if (!mVisibilityTT.isNull()) {
            mVisiCB = new QCheckBox(mVisibilityTT);
            connect(mVisiCB, SIGNAL(toggled(bool)), this, SLOT(toggleVisibility()));
            grid->addWidget(mVisiCB, row++, 1, 1, 2, Qt::AlignLeft);
        }
    }

    mFormattedPassphraseCB->setVisible(false);
    mFormattedPassphraseCB->setEnabled(false);
    connect(mFormattedPassphraseCB, SIGNAL(toggled(bool)), this, SLOT(toggleFormattedPassphrase()));
    grid->addWidget(mFormattedPassphraseCB, row++, 1, 1, 2);

    hbox->addLayout(grid, 1);

    mainLayout->addLayout(hbox);
    mainLayout->addStretch(1);
    mainLayout->addWidget(buttons);

    connect(qApp, SIGNAL(focusChanged(QWidget *, QWidget *)),
            this, SLOT(focusChanged(QWidget *, QWidget *)));
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
            this, SLOT(checkCapsLock()));

    checkCapsLock();

#if QT_VERSION >= 0x050000
    /* This is mostly an issue on Windows where this results
       in the pinentry popping up nicely with an animation and
       comes to front. It is not ifdefed for Windows only since
       window managers on Linux like KWin can also have this
       result in an animation when the pinentry is shown and
       not just popping it up.
    */
    setWindowState(Qt::WindowMinimized);
    QTimer::singleShot(0, this, [this] () {
        raiseWindow (this);
    });
#else
    activateWindow();
    raise();
#endif
}

void PinEntryDialog::keyReleaseEvent(QKeyEvent *event)
{
    QDialog::keyReleaseEvent(event);
    checkCapsLock();
}

void PinEntryDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    _edit->setFocus();
}

void PinEntryDialog::setDescription(const QString &txt)
{
    _desc->setVisible(!txt.isEmpty());
    _desc->setText(txt);
#ifndef QT_NO_ACCESSIBILITY
    _desc->setAccessibleDescription(txt);
#endif
    _icon->setPixmap(icon());
    setError(QString());
}

QString PinEntryDialog::description() const
{
    return _desc->text();
}

void PinEntryDialog::setError(const QString &txt)
{
    if (!txt.isNull()) {
        _icon->setPixmap(icon(QStyle::SP_MessageBoxCritical));
    }
    _error->setText(txt);
#ifndef QT_NO_ACCESSIBILITY
    _error->setAccessibleDescription(txt);
#endif
    _error->setVisible(!txt.isEmpty());
}

QString PinEntryDialog::error() const
{
    return _error->text();
}

void PinEntryDialog::setPin(const QString &txt)
{
    _edit->setPin(txt);
}

QString PinEntryDialog::pin() const
{
    return _edit->pin();
}

void PinEntryDialog::setPrompt(const QString &txt)
{
    _prompt->setText(txt);
    _prompt->setVisible(!txt.isEmpty());
    if (txt.contains("PIN"))
      _disable_echo_allowed = false;
}

QString PinEntryDialog::prompt() const
{
    return _prompt->text();
}

void PinEntryDialog::setOkText(const QString &txt)
{
    _ok->setText(txt);
#ifndef QT_NO_ACCESSIBILITY
    _ok->setAccessibleDescription(txt);
#endif
    _ok->setVisible(!txt.isEmpty());
}

void PinEntryDialog::setCancelText(const QString &txt)
{
    _cancel->setText(txt);
#ifndef QT_NO_ACCESSIBILITY
    _cancel->setAccessibleDescription(txt);
#endif
    _cancel->setVisible(!txt.isEmpty());
}

void PinEntryDialog::setQualityBar(const QString &txt)
{
    if (_have_quality_bar) {
        _quality_bar_label->setText(txt);
#ifndef QT_NO_ACCESSIBILITY
        _quality_bar_label->setAccessibleDescription(txt);
#endif
    }
}

void PinEntryDialog::setQualityBarTT(const QString &txt)
{
    if (_have_quality_bar) {
        _quality_bar->setToolTip(txt);
    }
}

void PinEntryDialog::setGenpinLabel(const QString &txt)
{
    if (!mGenerateActionEdit) {
        return;
    }
    if (txt.isEmpty()) {
        mGenerateActionEdit->setVisible(false);
    } else {
        mGenerateActionEdit->setText(txt);
        mGenerateActionEdit->setVisible(true);
    }
}

void PinEntryDialog::setGenpinTT(const QString &txt)
{
    if (mGenerateActionEdit) {
        mGenerateActionEdit->setToolTip(txt);
    }
}

void PinEntryDialog::setCapsLockHint(const QString &txt)
{
    mCapsLockHint->setText(txt);
}

void PinEntryDialog::setFormattedPassphrase(const PinEntryDialog::FormattedPassphraseOptions &options)
{
    mFormattedPassphraseCB->setText(options.label);
    mFormattedPassphraseCB->setToolTip(QLatin1String("<html>") + options.tooltip.toHtmlEscaped() + QLatin1String("</html>"));
    mFormattedPassphraseHint->setText(QLatin1String("<html>") + options.hint.toHtmlEscaped() + QLatin1String("</html>"));

    mFormattedPassphraseCB->setVisible(options.mode != FormattedPassphraseHidden);
    mFormattedPassphraseCB->setEnabled(options.mode == FormattedPassphraseOff || options.mode == FormattedPassphraseOn);
    mFormattedPassphraseCB->setChecked(options.mode == FormattedPassphraseOn || options.mode == FormattedPassphraseForcedOn);
}

void PinEntryDialog::toggleFormattedPassphrase()
{
    const bool enableFormatting = mFormattedPassphraseCB->isChecked() && _edit->echoMode() == QLineEdit::Normal;
    _edit->setFormattedPassphrase(enableFormatting);
    if (mRepeat) {
        mRepeat->setFormattedPassphrase(enableFormatting);
        const bool hintAboutToBeHidden = mFormattedPassphraseHint->isVisible() && !enableFormatting;
        if (hintAboutToBeHidden) {
            // set hint spacer to current height of hint label before hiding the hint
            mFormattedPassphraseHintSpacer->setMinimumHeight(mFormattedPassphraseHint->height());
            mFormattedPassphraseHintSpacer->setVisible(true);
        } else if (enableFormatting) {
            mFormattedPassphraseHintSpacer->setVisible(false);
        }
        mFormattedPassphraseHint->setVisible(enableFormatting);
    }
}

void PinEntryDialog::onBackspace()
{
    if (_disable_echo_allowed) {
        _edit->setEchoMode(QLineEdit::NoEcho);
        if (mRepeat) {
            mRepeat->setEchoMode(QLineEdit::NoEcho);
        }
    }
}

void PinEntryDialog::updateQuality(const QString &txt)
{
    int length;
    int percent;
    QPalette pal;

    if (_timer) {
        _timer->stop();
    }

    _disable_echo_allowed = false;

    if (!_have_quality_bar || !_pinentry_info) {
        return;
    }
    const QByteArray utf8_pin = txt.toUtf8();
    const char *pin = utf8_pin.constData();
    length = strlen(pin);
    percent = length ? pinentry_inq_quality(_pinentry_info, pin, length) : 0;
    if (!length) {
        _quality_bar->reset();
    } else {
        pal = _quality_bar->palette();
        if (percent < 0) {
            pal.setColor(QPalette::Highlight, QColor("red"));
            percent = -percent;
        } else {
            pal.setColor(QPalette::Highlight, QColor("green"));
        }
        _quality_bar->setPalette(pal);
        _quality_bar->setValue(percent);
    }
}

void PinEntryDialog::setPinentryInfo(pinentry_t peinfo)
{
    _pinentry_info = peinfo;
}

void PinEntryDialog::focusChanged(QWidget *old, QWidget *now)
{
    // Grab keyboard. It might be a little weird to do it here, but it works!
    // Previously this code was in showEvent, but that did not work in Qt4.
    if (!_pinentry_info || _pinentry_info->grab) {
        if (_grabbed && old && (old == _edit || old == mRepeat)) {
            old->releaseKeyboard();
            _grabbed = false;
        }
        if (!_grabbed && now && (now == _edit || now == mRepeat)) {
            now->grabKeyboard();
            _grabbed = true;
        }
    }
}

void PinEntryDialog::textChanged(const QString &text)
{
    Q_UNUSED(text);
    if (mRepeat && mRepeat->pin() == _edit->pin()) {
        _ok->setEnabled(true);
        _ok->setToolTip(QString());
    } else if (mRepeat) {
        _ok->setEnabled(false);
        _ok->setToolTip(mRepeatError);
    }

    if (mVisiActionEdit && sender() == _edit) {
        mVisiActionEdit->setVisible(!_edit->pin().isEmpty());
    }
    if (mGenerateActionEdit) {
        mGenerateActionEdit->setVisible(_edit->pin().isEmpty() &&
                                        _pinentry_info->genpin_label);
    }
}

void PinEntryDialog::generatePin()
{
    const char *pin = pinentry_inq_genpin(_pinentry_info);
    if (pin) {
        if (_edit->echoMode() == QLineEdit::Password) {
            toggleVisibility();
        }
        const auto pinStr = QString::fromUtf8(pin);
        _edit->setPin(pinStr);
        _edit->selectAll();
        mRepeat->setPin(pinStr);
        if (mFormattedPassphraseCB->isEnabled() && !mFormattedPassphraseCB->isChecked()) {
            mFormattedPassphraseCB->setChecked(true);
        }
    }
}

void PinEntryDialog::toggleVisibility()
{
    if (sender() != mVisiCB) {
        if (_edit->echoMode() == QLineEdit::Password) {
            mVisiActionEdit->setIcon(QIcon::fromTheme(QLatin1String("hint")));
            mVisiActionEdit->setToolTip(mHideTT);
            _edit->setEchoMode(QLineEdit::Normal);
            if (mRepeat) {
                mRepeat->setEchoMode(QLineEdit::Normal);
            }
        } else {
            mVisiActionEdit->setIcon(QIcon::fromTheme(QLatin1String("visibility")));
            mVisiActionEdit->setToolTip(mVisibilityTT);
            _edit->setEchoMode(QLineEdit::Password);
            if (mRepeat) {
                mRepeat->setEchoMode(QLineEdit::Password);
            }
        }
    } else {
        if (mVisiCB->isChecked()) {
            if (mRepeat) {
                mRepeat->setEchoMode(QLineEdit::Normal);
            }
            _edit->setEchoMode(QLineEdit::Normal);
        } else {
            if (mRepeat) {
                mRepeat->setEchoMode(QLineEdit::Password);
            }
            _edit->setEchoMode(QLineEdit::Password);
        }
    }
    toggleFormattedPassphrase();
}

QString PinEntryDialog::repeatedPin() const
{
    if (mRepeat) {
        return mRepeat->pin();
    }
    return QString();
}

bool PinEntryDialog::timedOut() const
{
    return _timed_out;
}

void PinEntryDialog::setRepeatErrorText(const QString &err)
{
    mRepeatError = err;
}

void PinEntryDialog::checkCapsLock()
{
    mCapsLockHint->setVisible(capsLockIsOn());
}

#include "pinentrydialog.moc"
