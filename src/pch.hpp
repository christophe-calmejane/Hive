/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/logger.hpp>
#include <la/avdecc/networkInterfaceHelper.hpp>
#include <la/avdecc/utils.hpp>
#include <la/avdecc/internals/uniqueIdentifier.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntityModel.hpp>

#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHash>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QObject>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScopedPointer>
#include <QScrollBar>
#include <QSettings>
#include <QSharedMemory>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QSplashScreen>
#include <QSplitter>
#include <QStandardPaths>
#include <QString>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextBrowser>
#include <QThread>
#include <QTreeWidget>
#include <QVariant>
#include <QVector>
#include <QtGlobal>
#include <QtWidgets>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mkdio.h>
#include <mutex>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
