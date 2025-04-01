/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#ifdef USE_SPARKLE
#	include "sparkleHelper/sparkleHelper.hpp"
#endif // USE_SPARKLE

// QtMate Library
#include <QtMate/flow/flowConnection.hpp>
#include <QtMate/flow/flowDefs.hpp>
#include <QtMate/flow/flowInput.hpp>
#include <QtMate/flow/flowLink.hpp>
#include <QtMate/flow/flowNode.hpp>
#include <QtMate/flow/flowOutput.hpp>
#include <QtMate/flow/flowScene.hpp>
#include <QtMate/flow/flowSceneDelegate.hpp>
#include <QtMate/flow/flowSocket.hpp>
#include <QtMate/flow/flowStyle.hpp>
#include <QtMate/flow/flowView.hpp>
#include <QtMate/material/color.hpp>
#include <QtMate/material/colorPalette.hpp>
#include <QtMate/material/helper.hpp>
#include <QtMate/widgets/autoSizeLabel.hpp>
#include <QtMate/widgets/comboBox.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>
#include <QtMate/widgets/flatIconButton.hpp>
#include <QtMate/widgets/headerViewSortSectionFilter.hpp>
#include <QtMate/widgets/tableView.hpp>
#include <QtMate/widgets/textEntry.hpp>
#include <QtMate/widgets/tickableMenu.hpp>

// Models Library
#include <hive/modelsLibrary/commandsExecutor.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/modelsLibrary.hpp>
#include <hive/modelsLibrary/networkInterfacesModel.hpp>

// Widget Models Library
#include <hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp>
#include <hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp>
#include <hive/widgetModelsLibrary/entityLogoCache.hpp>
#include <hive/widgetModelsLibrary/errorItemDelegate.hpp>
#include <hive/widgetModelsLibrary/imageItemDelegate.hpp>
#include <hive/widgetModelsLibrary/networkInterfacesListItemDelegate.hpp>
#include <hive/widgetModelsLibrary/networkInterfacesListModel.hpp>
#include <hive/widgetModelsLibrary/painterHelper.hpp>
#include <hive/widgetModelsLibrary/qtUserRoles.hpp>
#include <hive/widgetModelsLibrary/widgetModelsLibrary.hpp>

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
#include <QIODevice>
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
#include <QStringView>
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
