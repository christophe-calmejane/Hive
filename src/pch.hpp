/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

// Private Headers
#include "aboutDialog.hpp"
#include "activeNetworkInterfacesModel.hpp"
#include "aecpCommandComboBox.hpp"
#include "controlledEntityTreeWidget.hpp"
#include "defaults.hpp"
#include "deviceDetailsChannelTableModel.hpp"
#include "deviceDetailsDialog.hpp"
#include "entityInspector.hpp"
#include "firmwareUploadDialog.hpp"
#include "loggerView.hpp"
#include "mainWindow.hpp"
#include "mappingMatrix.hpp"
#include "multiFirmwareUpdateDialog.hpp"
#include "networkInterfaceTypeModel.hpp"
#include "nodeTreeWidget.hpp"
#include "nodeVisitor.hpp"
#include "settingsDialog.hpp"
#include "windowsNpfHelper.hpp"
#include "avdecc/channelConnectionManager.hpp"
#include "avdecc/commandChain.hpp"
#include "avdecc/controllerModel.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "avdecc/loggerModel.hpp"
#include "avdecc/mappingsHelper.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "avdecc/stringValidator.hpp"
#include "connectionMatrix/cornerWidget.hpp"
#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/legendDialog.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "connectionMatrix/view.hpp"
#include "counters/avbInterfaceCountersTreeWidgetItem.hpp"
#include "counters/clockDomainCountersTreeWidgetItem.hpp"
#include "counters/entityCountersTreeWidgetItem.hpp"
#include "counters/streamInputCountersTreeWidgetItem.hpp"
#include "counters/streamOutputCountersTreeWidgetItem.hpp"
#include "mediaClock/domainTreeDomainNameDelegate.hpp"
#include "mediaClock/domainTreeEntityNameDelegate.hpp"
#include "mediaClock/domainTreeModel.hpp"
#include "mediaClock/mediaClockManagementDialog.hpp"
#include "mediaClock/unassignedListModel.hpp"
#include "nodeTreeDynamicWidgets/asPathWidget.hpp"
#include "nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp"
#include "profiles/profileSelectionDialog.hpp"
#include "profiles/profiles.hpp"
#include "settingsManager/settings.hpp"
#include "sparkleHelper/sparkleHelper.hpp"
#include "statistics/entityStatisticsTreeWidgetItem.hpp"

// QtMate Library
#include <QtMate/graph/inputSocket.hpp>
#include <QtMate/graph/node.hpp>
#include <QtMate/graph/outputSocket.hpp>
#include <QtMate/graph/view.hpp>
#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>
#include <QtMate/material/helper.hpp>
#include <QtMate/widgets/comboBox.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/flatIconButton.hpp>
#include <QtMate/widgets/textEntry.hpp>
#include <QtMate/widgets/tickableMenu.hpp>

// Models Library
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/networkInterfacesModel.hpp>
#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>

// Widget Models Library
#include <hive/widgetModelsLibrary/entityLogoCache.hpp>
#include <hive/widgetModelsLibrary/errorItemDelegate.hpp>
#include <hive/widgetModelsLibrary/imageItemDelegate.hpp>
#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>
#include <hive/widgetModelsLibrary/painterHelper.hpp>


// Other Headers
#include <la/networkInterfaceHelper/networkInterfaceHelper.hpp>
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/logger.hpp>
#include <la/avdecc/utils.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/entityModel.hpp>
#include <la/avdecc/internals/entityModelTypes.hpp>
#include <la/avdecc/internals/logItems.hpp>
#include <la/avdecc/internals/protocolInterface.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>
#include <la/avdecc/internals/uniqueIdentifier.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntityModel.hpp>
#include <la/avdecc/controller/internals/logItems.hpp>

// Qt Headers
#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QApplication>
#include <QBrush>
#include <QByteArray>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRect>
#include <QScopedPointer>
#include <QScrollBar>
#include <QSettings>
#include <QSharedMemory>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QSplashScreen>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextBrowser>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QToolTip>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QValidator>
#include <QVariant>
#include <QVector>
#include <QWidget>
#include <QtGlobal>
#include <QtWidgets>

// STL Headers
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
