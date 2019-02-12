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

#include "connectionMatrix/headerItem.hpp"
#include "avdecc/helper.hpp"

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)
Q_DECLARE_METATYPE(connectionMatrix::HeaderItem::RelativeParentIndex)

namespace connectionMatrix
{
HeaderItem::HeaderItem(Model::NodeType const nodeType, la::avdecc::UniqueIdentifier const& entityID)
	: _nodeType{ nodeType }
	, _entityID{ entityID }
{
}

Model::NodeType HeaderItem::nodeType() const
{
	return _nodeType;
}

la::avdecc::UniqueIdentifier const& HeaderItem::entityID() const
{
	return _entityID;
}

void HeaderItem::setStreamNodeInfo(la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
{
	_streamIndex = streamIndex;
	_avbInterfaceIndex = avbInterfaceIndex;
}

la::avdecc::entity::model::StreamIndex HeaderItem::streamIndex() const
{
	return _streamIndex;
}

la::avdecc::entity::model::AvbInterfaceIndex HeaderItem::avbInterfaceIndex() const
{
	return _avbInterfaceIndex;
}

void HeaderItem::setRedundantIndex(la::avdecc::controller::model::VirtualIndex const redundantIndex)
{
	_redundantIndex = redundantIndex;
}

la::avdecc::controller::model::VirtualIndex HeaderItem::redundantIndex() const
{
	return _redundantIndex;
}

void HeaderItem::setRedundantStreamOrder(std::int32_t const redundantStreamOrder)
{
	_redundantStreamOrder = redundantStreamOrder;
}

std::int32_t HeaderItem::redundantStreamOrder() const
{
	return _redundantStreamOrder;
}

void HeaderItem::setRelativeParentIndex(std::int32_t const relativeParentIndex)
{
	_relativeParentIndex = relativeParentIndex;
}

HeaderItem::RelativeParentIndex HeaderItem::relativeParentIndex() const
{
	return _relativeParentIndex;
}

void HeaderItem::setChildrenCount(std::int32_t const childrenCount)
{
	_childrenCount = childrenCount;
}

std::int32_t HeaderItem::childrenCount() const
{
	return _childrenCount;
}

void HeaderItem::setStreamMap(StreamMap const& streamMap)
{
	_streamMap = streamMap;
}

HeaderItem::StreamMap const& HeaderItem::streamMap() const
{
	return _streamMap;
}

void HeaderItem::setInterfaceMap(InterfaceMap const& interfaceMap)
{
	_interfaceMap = interfaceMap;
}

HeaderItem::InterfaceMap const& HeaderItem::interfaceMap() const
{
	return _interfaceMap;
}

QVariant HeaderItem::data(int role) const
{
	if (role == Model::NodeTypeRole)
	{
		return QVariant::fromValue(_nodeType);
	}
	else if (role == Model::EntityIDRole)
	{
		return QVariant::fromValue(_entityID);
	}
	else if (role == Model::StreamIndexRole)
	{
		return QVariant::fromValue(_streamIndex);
	}
	else if (role == Model::RedundantIndexRole)
	{
		return QVariant::fromValue(_redundantIndex);
	}
	else if (role == Model::RedundantStreamOrderRole)
	{
		return QVariant::fromValue(_redundantStreamOrder);
	}
	else if (role == Model::StreamWaitingRole)
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		if (auto controlledEntity = manager.getControlledEntity(_entityID))
		{
			auto const& entityNode = controlledEntity->getEntityNode();

			if (_nodeType == Model::NodeType::OutputStream)
			{
				return !controlledEntity->isStreamOutputRunning(entityNode.dynamicModel->currentConfiguration, _streamIndex);
			}
			else if (_nodeType == Model::NodeType::InputStream)
			{
				return !controlledEntity->isStreamInputRunning(entityNode.dynamicModel->currentConfiguration, _streamIndex);
			}
		}
	}
	else if (role == Model::RelativeParentIndexRole)
	{
		return QVariant::fromValue(_relativeParentIndex);
	}
	else if (role == Model::ChildrenCountRole)
	{
		return QVariant::fromValue(_childrenCount);
	}
	else if (role == Qt::DisplayRole || role == Model::FilterRole)
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		if (auto controlledEntity = manager.getControlledEntity(_entityID))
		{
			auto const& entityNode = controlledEntity->getEntityNode();

			if (_nodeType == Model::NodeType::Entity || role == Model::FilterRole)
			{
				if (entityNode.dynamicModel->entityName.empty())
				{
					return avdecc::helper::uniqueIdentifierToString(_entityID);
				}
				else
				{
					return QString::fromStdString(entityNode.dynamicModel->entityName);
				}
			}
			else if (_nodeType == Model::NodeType::InputStream || _nodeType == Model::NodeType::RedundantInputStream)
			{
				auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
				return avdecc::helper::objectName(controlledEntity.get(), streamNode);
			}
			else if (_nodeType == Model::NodeType::OutputStream || _nodeType == Model::NodeType::RedundantOutputStream)
			{
				auto const& streamNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
				return avdecc::helper::objectName(controlledEntity.get(), streamNode);
			}
			else if (_nodeType == Model::NodeType::RedundantInput)
			{
				return QString{ "Redundant Stream Input %1" }.arg(QString::number(_redundantIndex));
			}
			else if (_nodeType == Model::NodeType::RedundantOutput)
			{
				return QString{ "Redundant Stream Output %1" }.arg(QString::number(_redundantIndex));
			}
		}
	}

	return QStandardItem::data(role);
}

} // namespace connectionMatrix
