/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QTreeWidget>
#include "nodeDispatcher.hpp"
#include "avdecc/helper.hpp"

#include <hive/modelsLibrary/helper.hpp>

class NodeTreeWidgetPrivate;
class NodeTreeWidget : public QTreeWidget
{
	Q_OBJECT
public:
	enum class TreeWidgetItemType : int
	{
		EntityStatistic = QTreeWidgetItem::UserType + 1,
		StreamInputCounter,
	};

	NodeTreeWidget(QWidget* parent = nullptr);
	~NodeTreeWidget();

	void setNode(la::avdecc::UniqueIdentifier const entityID, bool const isActiveConfiguration, AnyNode const& node);

private:
	NodeTreeWidgetPrivate* d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(NodeTreeWidget)
};

template<typename IntegralValueType, typename = std::enable_if_t<std::is_arithmetic<IntegralValueType>::value>>
void setFlagsItemText(QTreeWidgetItem* const item, IntegralValueType flagsValue, QString flagsString)
{
	item->setText(1, QString("%1 (%2)").arg(hive::modelsLibrary::helper::toHexQString(flagsValue, true, true)).arg(flagsString));
	item->setData(1, Qt::ToolTipRole, flagsString);
}
