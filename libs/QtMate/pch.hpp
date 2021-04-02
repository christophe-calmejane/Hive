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

// Public Headers
#include "QtMate/material/color.hpp"
#include "QtMate/material/colorPalette.hpp"
#include "QtMate/material/helper.hpp"
#include "QtMate/graph/connection.hpp"
#include "QtMate/graph/inputSocket.hpp"
#include "QtMate/graph/node.hpp"
#include "QtMate/graph/outputSocket.hpp"
#include "QtMate/graph/socket.hpp"
#include "QtMate/graph/type.hpp"
#include "QtMate/graph/view.hpp"
#include "QtMate/widgets/comboBox.hpp"
#include "QtMate/widgets/dynamicHeaderView.hpp"
#include "QtMate/widgets/flatIconButton.hpp"
#include "QtMate/widgets/tableView.hpp"
#include "QtMate/widgets/textEntry.hpp"
#include "QtMate/widgets/tickableMenu.hpp"

// Qt Headers
#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QStyledItemDelegate>
#include <QTableView>

// STL Headers
#include <cassert>
#include <memory>
#include <unordered_map>
#include <unordered_set>
