/***************************************************************************
 *   Copyright (C) 2005-08 by the Quassel Project                          *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "client.h"
#include "buffersyncer.h"
#include "bufferview.h"
#include "networkmodel.h"
#include "network.h"

#include "uisettings.h"

#include "global.h"

/*****************************************
* The TreeView showing the Buffers
*****************************************/
// Please be carefull when reimplementing methods which are used to inform the view about changes to the data
// to be on the safe side: call QTreeView's method aswell
BufferView::BufferView(QWidget *parent) : QTreeView(parent) {
  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showContextMenu(const QPoint &)));
}

void BufferView::init() {
  setIndentation(10);
  header()->setContextMenuPolicy(Qt::ActionsContextMenu);
  hideColumn(1);
  hideColumn(2);
  expandAll();

  setAnimated(true);

#ifndef QT_NO_DRAGANDDROP
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
#endif

  setSortingEnabled(true);
  sortByColumn(0, Qt::AscendingOrder);
#ifndef Q_WS_QWS
  // this is a workaround to not join channels automatically... we need a saner way to navigate for qtopia anyway though,
  // such as mark first, activate at second click...
  connect(this, SIGNAL(activated(QModelIndex)), this, SLOT(joinChannel(QModelIndex)));
#else
  connect(this, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(joinChannel(QModelIndex)));  // Qtopia uses single click for activation
#endif
}

void BufferView::setFilteredModel(QAbstractItemModel *model, BufferViewFilter::Modes mode, QList<NetworkId> nets) {
  BufferViewFilter *filter = new BufferViewFilter(model, mode, nets);
  setModel(filter);
  connect(this, SIGNAL(removeBuffer(const QModelIndex &)), filter, SLOT(removeBuffer(const QModelIndex &)));
}

void BufferView::setModel(QAbstractItemModel *model) {
  delete selectionModel();
  QTreeView::setModel(model);
  init();

  // remove old Actions
  QList<QAction *> oldactions = header()->actions();
  foreach(QAction *action, oldactions) {
    header()->removeAction(action);
    action->deleteLater();
  }

  QString sectionName;
  QAction *showSection;
  for(int i = 1; i < model->columnCount(); i++) {
    sectionName = (model->headerData(i, Qt::Horizontal, Qt::DisplayRole)).toString();
    showSection = new QAction(sectionName, header());
    showSection->setCheckable(true);
    showSection->setChecked(!isColumnHidden(i));
    showSection->setProperty("column", i);
    connect(showSection, SIGNAL(toggled(bool)), this, SLOT(toggleHeader(bool)));
    header()->addAction(showSection);
  }
  
}

void BufferView::joinChannel(const QModelIndex &index) {
  BufferInfo::Type bufferType = (BufferInfo::Type)index.data(NetworkModel::BufferTypeRole).value<int>();

  if(bufferType != BufferInfo::ChannelBuffer)
    return;

  BufferInfo bufferInfo = index.data(NetworkModel::BufferInfoRole).value<BufferInfo>();
  
  Client::userInput(bufferInfo, QString("/JOIN %1").arg(bufferInfo.bufferName()));
}

void BufferView::keyPressEvent(QKeyEvent *event) {
  if(event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
    event->accept();
    QModelIndex index = selectionModel()->selectedIndexes().first();
    if(index.isValid()) {
      emit removeBuffer(index);
    }
  }
  QTreeView::keyPressEvent(event);
}

// ensure that newly inserted network nodes are expanded per default
void BufferView::rowsInserted(const QModelIndex & parent, int start, int end) {
  QTreeView::rowsInserted(parent, start, end);
  if(model()->rowCount(parent) == 1 && parent.data(NetworkModel::ItemTypeRole) == NetworkModel::NetworkItemType
     && (Global::SPUTDEV || parent.data(NetworkModel::ItemActiveRole) == true)) {
    // without updating the parent the expand will have no effect... Qt Bug?
    update(parent);
    expand(parent);
  }
}

void BufferView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
  QTreeView::dataChanged(topLeft, bottomRight);
  
  // determine how many items have been changed and if any of them is a networkitem
  // which just swichted from active to inactive or vice versa
  if(topLeft.data(NetworkModel::ItemTypeRole) != NetworkModel::NetworkItemType)
    return;

  for(int i = topLeft.row(); i <= bottomRight.row(); i++) {
    QModelIndex networkIdx = topLeft.sibling(topLeft.row(), 0);
    if(model()->rowCount(networkIdx) == 0)
      continue;

    bool isActive = networkIdx.data(NetworkModel::ItemActiveRole).toBool();
    if(Global::SPUTDEV) {
      if(isExpanded(networkIdx) != isActive) setExpanded(networkIdx, true);
    } else {
      if(isExpanded(networkIdx) != isActive)
        setExpanded(networkIdx, isActive);
    }
  }
}


void BufferView::toggleHeader(bool checked) {
  QAction *action = qobject_cast<QAction *>(sender());
  header()->setSectionHidden((action->property("column")).toInt(), !checked);
}

void BufferView::showContextMenu(const QPoint &pos) {
  QModelIndex index = indexAt(pos);
  if(!index.isValid()) return;
  QMenu contextMenu(this);
  QAction *connectNetAction = new QAction(tr("Connect"), this);
  QAction *disconnectNetAction = new QAction(tr("Disconnect"), this);
  QAction *joinChannelAction = new QAction(tr("Join Channel"), this);

  QAction *joinBufferAction = new QAction(tr("Join"), this);
  QAction *partBufferAction = new QAction(tr("Part"), this);
  QAction *removeBufferAction = new QAction(tr("Delete buffer"), this);

  QMenu *hideEventsMenu = new QMenu(tr("Hide Events"), this);
  QAction *hideJoinAction = hideEventsMenu->addAction(tr("Join Events"));
  QAction *hidePartAction = hideEventsMenu->addAction(tr("Part Events"));
  QAction *hideKillAction = hideEventsMenu->addAction(tr("Kill Events"));
  QAction *hideQuitAction = hideEventsMenu->addAction(tr("Quit Events"));
  QAction *hideModeAction = hideEventsMenu->addAction(tr("Mode Events"));
  hideJoinAction->setCheckable(true);
  hidePartAction->setCheckable(true);
  hideKillAction->setCheckable(true);
  hideQuitAction->setCheckable(true);
  hideModeAction->setCheckable(true);
  hideJoinAction->setEnabled(false);
  hidePartAction->setEnabled(false);
  hideKillAction->setEnabled(false);
  hideQuitAction->setEnabled(false);
  hideModeAction->setEnabled(false);

  QAction *ignoreListAction = new QAction(tr("Ignore list"), this);
  ignoreListAction->setEnabled(false);
  QAction *whoBufferAction = new QAction(tr("WHO"), this);

  if(index.data(NetworkModel::ItemTypeRole) == NetworkModel::NetworkItemType) {
    if(index.data(NetworkModel::ItemActiveRole).toBool()) {
      contextMenu.addAction(disconnectNetAction);
      contextMenu.addSeparator();
      contextMenu.addAction(joinChannelAction);
    } else {
      contextMenu.addAction(connectNetAction);
    }
  }

  BufferInfo bufferInfo = index.data(NetworkModel::BufferInfoRole).value<BufferInfo>();
  QString channelname = index.sibling(index.row(), 0).data().toString();

  if(index.data(NetworkModel::ItemTypeRole) == NetworkModel::BufferItemType) {
    if(bufferInfo.type() != BufferInfo::ChannelBuffer && bufferInfo.type() != BufferInfo::QueryBuffer) return;
    contextMenu.addAction(joinBufferAction);
    contextMenu.addAction(partBufferAction);
    contextMenu.addAction(removeBufferAction);
    contextMenu.addMenu(hideEventsMenu);
    contextMenu.addAction(ignoreListAction);
    contextMenu.addAction(whoBufferAction);

    if(bufferInfo.type() == BufferInfo::ChannelBuffer) {
      if(index.data(NetworkModel::ItemActiveRole).toBool()) {
        removeBufferAction->setEnabled(false);
        removeBufferAction->setToolTip("To delete the buffer, part the channel first.");
        joinBufferAction->setVisible(false);
        whoBufferAction->setVisible(false);
      } else {
        partBufferAction->setVisible(false);
      }
    } else {
      joinBufferAction->setVisible(false);
      partBufferAction->setVisible(false);
    }
  }

  QAction *result = contextMenu.exec(QCursor::pos());
  if(result == connectNetAction || result == disconnectNetAction) {
    const Network *network = Client::network(index.data(NetworkModel::NetworkIdRole).value<NetworkId>());
    if(!network) return;
    if(network->connectionState() == Network::Disconnected) 
      network->requestConnect();
    else 
      network->requestDisconnect();
  } else
  if(result == joinChannelAction) {
    // FIXME no QInputDialog in Qtopia
#ifndef Q_WS_QWS
    bool ok;
    QString channelName = QInputDialog::getText(this, tr("Join Channel"), 
                                                tr("Input channel name:"),QLineEdit::Normal,
                                                QDir::home().dirName(), &ok);

    if (ok && !channelName.isEmpty()) {
      BufferInfo bufferInfo = index.child(0,0).data(NetworkModel::BufferInfoRole).value<BufferInfo>();
      if(bufferInfo.isValid()) {
        Client::instance()->userInput(bufferInfo, QString("/J %1").arg(channelName));
      }
    }
#endif
  } else
  if(result == joinBufferAction) {
    Client::instance()->userInput(bufferInfo, QString("/JOIN %1").arg(channelname));
  } else
  if(result == partBufferAction) {
    Client::instance()->userInput(bufferInfo, QString("/PART %1").arg(channelname));
  } else
  if(result == removeBufferAction) {
    int res = QMessageBox::question(this, tr("Remove buffer permanently?"),
                                    tr("Do you want to delete the buffer \"%1\" permanently? This will delete all related data, including all backlog "
                                       "data, from the core's database!").arg(bufferInfo.bufferName()),
                                        QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if(res == QMessageBox::Yes) {
      Client::removeBuffer(bufferInfo.bufferId());
    } 
  } else 
  if(result == whoBufferAction) {
    Client::instance()->userInput(bufferInfo, QString("/WHO %1").arg(channelname));
  }
}

void BufferView::wheelEvent(QWheelEvent* event) {
  if(UiSettings().value("MouseWheelChangesBuffers", QVariant(true)).toBool() == (bool)(event->modifiers() & Qt::AltModifier))
    return QTreeView::wheelEvent(event);

  int rowDelta = ( event->delta() > 0 ) ? -1 : 1;
  QModelIndex currentIndex = selectionModel()->currentIndex();
  QModelIndex resultingIndex;
  if( model()->hasIndex(  currentIndex.row() + rowDelta, currentIndex.column(), currentIndex.parent() ) )
    {
      resultingIndex = currentIndex.sibling( currentIndex.row() + rowDelta, currentIndex.column() );
    }
    else //if we scroll into a the parent node...
      {
        QModelIndex parent = currentIndex.parent();
        QModelIndex aunt = parent.sibling( parent.row() + rowDelta, parent.column() );
        if( rowDelta == -1 )
	  resultingIndex = aunt.child( model()->rowCount( aunt ) - 1, 0 );
        else
	  resultingIndex = aunt.child( 0, 0 );
        if( !resultingIndex.isValid() )
	  return;
      }
  selectionModel()->setCurrentIndex( resultingIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows );
  selectionModel()->select( resultingIndex, QItemSelectionModel::ClearAndSelect );
  
}


QSize BufferView::sizeHint() const {
  return QSize(120, 50);
  
  if(!model())
    return QTreeView::sizeHint();

  if(model()->rowCount() == 0)
    return QSize(120, 50);

  int columnSize = 0;
  for(int i = 0; i < model()->columnCount(); i++) {
    if(!isColumnHidden(i))
      columnSize += sizeHintForColumn(i);
  }
  return QSize(columnSize, 50);
}
