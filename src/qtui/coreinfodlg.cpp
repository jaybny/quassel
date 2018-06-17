/***************************************************************************
 *   Copyright (C) 2005-2018 by the Quassel Project                        *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "coreinfodlg.h"

#include <QMessageBox>

#include "bufferwidget.h"
#include "client.h"
#include "icon.h"

CoreInfoDlg::CoreInfoDlg(QWidget *parent) : QDialog(parent) {
    ui.setupUi(this);
    CoreInfo *coreInfo = Client::coreInfo();
    connect(coreInfo, SIGNAL(coreDataChanged(const QVariantMap &)), this, SLOT(coreInfoChanged(const QVariantMap &)));

    coreInfoChanged(coreInfo->coreData());

    // Warning icon
    ui.coreUnsupportedIcon->setPixmap(icon::get("dialog-warning").pixmap(16));

    updateUptime();
    startTimer(1000);
}


void CoreInfoDlg::coreInfoChanged(const QVariantMap &coreInfo) {
    ui.labelCoreVersion->setText(coreInfo["quasselVersion"].toString());
    ui.labelCoreVersionDate->setText(coreInfo["quasselBuildDate"].toString()); // "BuildDate" for compatibility
    ui.labelClientCount->setNum(coreInfo["sessionConnectedClients"].toInt());

    auto coreSessionSupported = false;
    auto ids = _widgets.keys();
    for (const auto &peerData : coreInfo["sessionConnectedClientData"].toList()) {
        coreSessionSupported = true;

        auto peerMap = peerData.toMap();
        int peerId = peerMap["id"].toInt();

        ids.removeAll(peerId);

        bool isNew = false;
        CoreSessionWidget *coreSessionWidget = _widgets[peerId];
        if (coreSessionWidget == nullptr) {
            coreSessionWidget = new CoreSessionWidget(ui.coreSessionScrollContainer);
            isNew = true;
        }
        coreSessionWidget->setData(peerMap);
        if (isNew) {
            _widgets[peerId] = coreSessionWidget;
            // Add this to the end of the session list, but before the default layout stretch item.
            // The layout stretch item should never be removed, so count should always be >= 1.
            ui.coreSessionContainer->insertWidget(ui.coreSessionContainer->count() - 1,
                                                  coreSessionWidget, 0, Qt::AlignTop);
            connect(coreSessionWidget, SIGNAL(disconnectClicked(int)), this, SLOT(disconnectClicked(int)));
        }
    }

    for (const auto &key : ids) {
        delete _widgets[key];
        _widgets.remove(key);
    }

    ui.coreSessionScrollArea->setVisible(coreSessionSupported);

    // Hide the information bar when core sessions are supported
    ui.coreUnsupportedWidget->setVisible(!coreSessionSupported);
}


void CoreInfoDlg::updateUptime() {
    CoreInfo *coreInfo = Client::coreInfo();
    if (coreInfo) {
        QDateTime startTime = coreInfo->at("startTime").toDateTime();

        int64_t uptime = startTime.secsTo(QDateTime::currentDateTime().toUTC());
        int64_t updays = uptime / 86400;
        uptime %= 86400;
        int64_t uphours = uptime / 3600;
        uptime %= 3600;
        int64_t upmins = uptime / 60;
        uptime %= 60;

        QString uptimeText = tr("%n Day(s)", "", updays) +
                             tr(" %1:%2:%3 (since %4)")
                                     .arg(uphours, 2, 10, QChar('0'))
                                     .arg(upmins, 2, 10, QChar('0'))
                                     .arg(uptime, 2, 10, QChar('0'))
                                     .arg(startTime.toLocalTime().toString(Qt::TextDate));
        ui.labelUptime->setText(uptimeText);
    }
}

void CoreInfoDlg::disconnectClicked(int peerId) {
    Client::kickClient(peerId);
}

void CoreInfoDlg::on_coreUnsupportedDetails_clicked()
{
    QMessageBox::warning(this,
                         tr("Active sessions unsupported"),
                         QString("<p><b>%1</b></p></br><p>%2</p>"
                                 ).arg(tr("Your Quassel core is too old to show active sessions"),
                                       tr("You need a Quassel core v0.13.0 or newer to view and "
                                          "disconnect other connected clients.")));
}
