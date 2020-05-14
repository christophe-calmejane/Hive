/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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

// Public Headers
#include "hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp"
#include "hive/widgetModelsLibrary/entityLogoCache.hpp"
#include "hive/widgetModelsLibrary/errorItemDelegate.hpp"
#include "hive/widgetModelsLibrary/imageItemDelegate.hpp"
#include "hive/widgetModelsLibrary/networkInterfacesListModel.hpp"
#include "hive/widgetModelsLibrary/painterHelper.hpp"
#include "hive/widgetModelsLibrary/qtUserRoles.hpp"
#include "hive/widgetModelsLibrary/widgetModelsLibrary.hpp"

// Other Headers
#include <QtMate/material/color.hpp>
#include <QtMate/material/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/networkInterfacesModel.hpp>
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/logger.hpp>
#include <la/avdecc/networkInterfaceHelper.hpp>
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
#include <Qt>
#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QMetaType>
#include <QMimeData>
#include <QObject>
#include <QPainter>
#include <QScopedPointer>
#include <QSharedMemory>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QThread>
#include <QVariant>
#include <QVector>
#include <QtGlobal>

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
