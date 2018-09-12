/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "connectionMatrix.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "internals/config.hpp"
#include <la/avdecc/utils.hpp>

#include <QApplication>
#include <QPainter>
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QLayout>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>

namespace connectionMatrix
{

void ConnectionMatrixHeaderDelegate::paintSection(QPainter* painter, QRect const& rect, int const logicalIndex, QHeaderView* headerView, qt::toolkit::MatrixModel::Node* node)
{
	auto const orientation = headerView->orientation();
	auto* model = headerView->model();

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	std::function<int(qt::toolkit::MatrixModel::Node*, int)> countDepth;
	countDepth = [&countDepth](qt::toolkit::MatrixModel::Node* node, int const depth)
	{
		if (node->parent == nullptr)
			return depth;
		return countDepth(node->parent, depth + 1);
	};

	QBrush backgroundBrush{};
	auto const arrowSize{ 10 };
	auto const depth = countDepth(node, 0);
	auto const arrowOffset{ 25 * depth };
	switch (depth)
	{
		case 0:
			backgroundBrush = QColor{ "#4A148C" };
			break;
		case 1:
			backgroundBrush = QColor{ "#7B1FA2" };
			break;
		case 2:
			backgroundBrush = QColor{ "#BA68C8" };
			break;
		default:
			backgroundBrush = QColor{ "#808080" };
			break;
	}

	auto highlighted{false};

	QPainterPath path;
	if (orientation == Qt::Horizontal)
	{
		path.moveTo(rect.topLeft());
		path.lineTo(rect.bottomLeft() - QPoint{ 0, arrowSize + arrowOffset });
		path.lineTo(rect.center() + QPoint{ 0, rect.height() / 2 - arrowOffset });
		path.lineTo(rect.bottomRight() - QPoint{ 0, arrowSize + arrowOffset });
		path.lineTo(rect.topRight());

		highlighted = headerView->selectionModel()->isColumnSelected(logicalIndex, {});
	}
	else
	{
		path.moveTo(rect.topLeft());
		path.lineTo(rect.topRight() - QPoint{ arrowSize + arrowOffset, 0 });
		path.lineTo(rect.center() + QPoint{ rect.width() / 2 - arrowOffset, 0 });
		path.lineTo(rect.bottomRight() - QPoint{ arrowSize + arrowOffset, 0 });
		path.lineTo(rect.bottomLeft());

		highlighted = headerView->selectionModel()->isRowSelected(logicalIndex, {});
	}

	if (highlighted)
	{
		backgroundBrush = QColor{ "#007ACC" };
	}

	painter->fillPath(path, backgroundBrush);
	painter->translate(rect.topLeft());

	auto r = QRect(0, 0, rect.width(), rect.height());

	if (orientation == Qt::Horizontal)
	{
		r.setWidth(rect.height());
		r.setHeight(rect.width());

		painter->rotate(-90);
		painter->translate(-r.width(), 0);

		r.translate(arrowSize + arrowOffset, 0);
	}

	auto const padding{ 4 };
	auto textRect = r.adjusted(padding, 0, -(padding + arrowSize + arrowOffset), 0);

	auto const text = model->headerData(logicalIndex, orientation).toString();
	auto const elidedText = painter->fontMetrics().elidedText(text, Qt::ElideMiddle, textRect.width());

	auto const isStreamingWait = model->headerData(logicalIndex, orientation, ConnectionMatrixModel::StreamWaitingRole).toBool();
	if (isStreamingWait)
	{
		painter->setPen(Qt::red);
	}
	else
	{
		painter->setPen(Qt::white);
	}

	painter->drawText(textRect, Qt::AlignVCenter, elidedText);
	painter->restore();
}

/* ************************************************************ */
/* ConnectionMatrixModel                                        */
/* ************************************************************ */
class ConnectionMatrixModel::ConnectionMatrixModelPrivate : public QObject
{
	Q_OBJECT
public:
	using Entities = std::set<la::avdecc::UniqueIdentifier>;

	ConnectionMatrixModelPrivate(ConnectionMatrixModel* q);

	bool isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept;
	bool isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept;
	ConnectionCapabilities connectionCapabilities(UserData const& talkerStream, UserData const& listenerStream) const noexcept;
	//int countConnectedStreams(UserData*) const;

private:
	// Slots for avdecc::ControllerManager signals
	Q_SLOT void controllerOffline();
	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning);
	Q_SLOT void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state);
	Q_SLOT void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat);
	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain);
	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID);
	Q_SLOT void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex);

	// Private methods
	void addEntity(bool const orientationIsRow, la::avdecc::UniqueIdentifier const entityID);
	void removeEntity(bool const orientationIsRow, la::avdecc::UniqueIdentifier const entityID);
	static inline bool areUserDataEqual(QVariant const& lhs, QVariant const& rhs)
	{
		auto const& lhsData = lhs.value<UserData>();
		auto const& rhsData = rhs.value<UserData>();
		return lhsData == rhsData;
	}

	// Private members
	Entities _talkers{};
	Entities _listeners{};

	ConnectionMatrixModel * const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ConnectionMatrixModel);
};

ConnectionMatrixModel::ConnectionMatrixModelPrivate::ConnectionMatrixModelPrivate(ConnectionMatrixModel* q) : q_ptr(q)
{
	auto& controllerManager = avdecc::ControllerManager::getInstance();
	connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::controllerOffline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityOnline);
	connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityOffline);
	connect(&controllerManager, &avdecc::ControllerManager::streamRunningChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamRunningChanged);
	connect(&controllerManager, &avdecc::ControllerManager::streamConnectionChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamConnectionChanged);
	connect(&controllerManager, &avdecc::ControllerManager::streamFormatChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamFormatChanged);
	connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::gptpChanged);
	connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityNameChanged);
	connect(&controllerManager, &avdecc::ControllerManager::streamNameChanged, this, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamNameChanged);
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::controllerOffline()
{
	if (q_ptr)
		q_ptr->clearModel();
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityOnline(la::avdecc::UniqueIdentifier const entityID)
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);
	if (controlledEntity && AVDECC_ASSERT_WITH_RET(!controlledEntity->gotFatalEnumerationError(), "An entity should not be set online if it had an enumeration error"))
	{
		if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
		{
			_talkers.insert(entityID);
			addEntity(true, entityID);
		}
		if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
		{
			_listeners.insert(entityID);
			addEntity(false, entityID);
		}
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityOffline(la::avdecc::UniqueIdentifier const entityID)
{
	if (_talkers.erase(entityID) != 0)
		removeEntity(true, entityID);
	if (_listeners.erase(entityID) != 0)
		removeEntity(false, entityID);
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
{
	if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
	{
		// Refresh header for specified listener input stream
		auto const index = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::InputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (index != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Orientation::Horizontal, index, index);
		}
	}
	else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
	{
		// Refresh header for specified talker output stream
		auto const index = q_ptr->rowForUserData(QVariant::fromValue(UserData{ UserData::Type::OutputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (index != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Orientation::Vertical, index, index);
		}
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state)
{
	auto const listenerID = state.listenerStream.entityID;
	auto const listenerIndex = state.listenerStream.streamIndex;

	// Refresh whole column for specified listener single stream
	{
		auto const columnIndex = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::InputStreamNode, listenerID, listenerIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (columnIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(0, columnIndex, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(q_ptr->rowCount({}), columnIndex, q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}

	// Refresh whole column for specified listener redundant stream
	//{
	//	auto const columnIndex = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::RedundantInputStreamNode, listenerID, listenerIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
	//
	//	if (columnIndex != -1)
	//	{
	//		auto const topLeftIndex = q_ptr->createIndex(0, columnIndex, q_ptr);
	//		auto const bottomRightIndex = q_ptr->createIndex(q_ptr->rowCount({}), columnIndex, q_ptr);
	//
	//		emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
	//	}
	//}

	// Refresh whole column for specified listener (UserData::Type::EntityNode only)
	{
		auto const columnIndex = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::EntityNode, listenerID }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (columnIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(0, columnIndex, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(q_ptr->rowCount({}), columnIndex, q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
{
	if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
	{
		// Refresh whole column for specified listener single stream
		auto const columnIndex = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::InputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (columnIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(0, columnIndex, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(q_ptr->rowCount({}), columnIndex, q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}
	else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
	{
		// Refresh whole row for specified talker single stream
		auto const rowIndex = q_ptr->rowForUserData(QVariant::fromValue(UserData{ UserData::Type::OutputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (rowIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(rowIndex, 0, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(rowIndex, q_ptr->columnCount({}), q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}
	else
	{
		AVDECC_ASSERT(false, "DescriptorType should be StreamInput or StreamOutput");
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
{
	auto const toCompare = QVariant::fromValue(UserData{ UserData::Type::EntityNode, entityID });

	// Refresh whole columns for specified listener (all UserData::Type for that entity)
	{
		auto const result = q_ptr->columnAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const columnIndex = result.first;
		if (columnIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(0, columnIndex, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(q_ptr->rowCount({}), columnIndex + q_ptr->countChildren(result.second), q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}

	// Refresh whole rows for specified talker (all UserData::Type for that entity)
	{
		auto const result = q_ptr->rowAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const rowIndex = result.first;
		if (rowIndex != -1)
		{
			auto const topLeftIndex = q_ptr->createIndex(rowIndex, 0, q_ptr);
			auto const bottomRightIndex = q_ptr->createIndex(rowIndex + q_ptr->countChildren(result.second), q_ptr->columnCount({}), q_ptr);

			emit q_ptr->dataChanged(topLeftIndex, bottomRightIndex, { Qt::DisplayRole });
		}
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::entityNameChanged(la::avdecc::UniqueIdentifier const entityID)
{
	auto const toCompare = QVariant::fromValue(UserData{ UserData::Type::EntityNode, entityID });

	{
		auto const result = q_ptr->columnAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const columnIndex = result.first;
		if (columnIndex != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Horizontal, columnIndex, columnIndex);
		}
	}

	{
		auto const result = q_ptr->rowAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const rowIndex = result.first;
		if (rowIndex != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Vertical, rowIndex, rowIndex);
		}
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
{
	Q_UNUSED(configurationIndex);

	if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
	{
		auto const columnIndex = q_ptr->columnForUserData(QVariant::fromValue(UserData{ UserData::Type::InputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (columnIndex != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Vertical, columnIndex, columnIndex);
		}
	}
	else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
	{
		auto const rowIndex = q_ptr->rowForUserData(QVariant::fromValue(UserData{ UserData::Type::OutputStreamNode, entityID, streamIndex }), &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);

		if (rowIndex != -1)
		{
			emit q_ptr->headerDataChanged(Qt::Horizontal, rowIndex, rowIndex);
		}
	}
	else
	{
		AVDECC_ASSERT(false, "DescriptorType should be StreamInput or StreamOutput");
	}
}

bool ConnectionMatrixModel::ConnectionMatrixModelPrivate::isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

bool ConnectionMatrixModel::ConnectionMatrixModelPrivate::isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) const noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::FastConnecting) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

ConnectionCapabilities ConnectionMatrixModel::ConnectionMatrixModelPrivate::connectionCapabilities(UserData const& talkerStream, UserData const& listenerStream) const noexcept
{
	if (talkerStream.entityID == listenerStream.entityID)
		return ConnectionCapabilities::None;

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto talkerEntity = manager.getControlledEntity(talkerStream.entityID);
		auto listenerEntity = manager.getControlledEntity(listenerStream.entityID);
		if (talkerEntity && listenerEntity)
		{
			auto const& talkerEntityNode = talkerEntity->getEntityNode();
			auto const& talkerEntityInfo = talkerEntity->getEntity();
			auto const& listenerEntityNode = listenerEntity->getEntityNode();
			auto const& listenerEntityInfo = listenerEntity->getEntity();

			auto const computeFormatCompatible = [](la::avdecc::controller::model::StreamOutputNode const& talkerNode, la::avdecc::controller::model::StreamInputNode const& listenerNode)
			{
				return la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerNode.dynamicModel->currentFormat, talkerNode.dynamicModel->currentFormat);
			};
			auto const computeDomainCompatible = [&talkerEntityInfo, &listenerEntityInfo]()
			{
				// TODO: Incorrect computation, must be based on the AVBInterface for the stream
				return listenerEntityInfo.getGptpGrandmasterID() == talkerEntityInfo.getGptpGrandmasterID();
			};
			enum class ConnectState
			{
				NotConnected = 0,
				FastConnecting,
				Connected,
			};
			auto const computeCapabilities = [](ConnectState const connectState, bool const areAllConnected, bool const isFormatCompatible, bool const isDomainCompatible)
			{
				auto caps{ ConnectionCapabilities::Connectable };

				if (!isDomainCompatible)
					caps |= ConnectionCapabilities::WrongDomain;

				if (!isFormatCompatible)
					caps |= ConnectionCapabilities::WrongFormat;

				if (connectState != ConnectState::NotConnected)
				{
					if (areAllConnected)
						caps |= ConnectionCapabilities::Connected;
					else if (connectState == ConnectState::FastConnecting)
						caps |= ConnectionCapabilities::FastConnecting;
					else
						caps |= ConnectionCapabilities::PartiallyConnected;
				}

				return caps;
			};

			// Special case for both redundant nodes
			if (talkerStream.type == UserData::Type::RedundantOutputNode && listenerStream.type == UserData::Type::RedundantInputNode)
			{
				// Check if all redundant streams are connected
				auto const& talkerRedundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStream.redundantIndex);
				auto const& listenerRedundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStream.redundantIndex);
				// TODO: Maybe someday handle the case for more than 2 streams for redundancy
				AVDECC_ASSERT(talkerRedundantNode.redundantStreams.size() == listenerRedundantNode.redundantStreams.size(), "More than 2 redundant streams in the set");
				auto talkerIt = talkerRedundantNode.redundantStreams.begin();
				auto const* const talkerStreamNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(talkerIt->second);
				auto listenerIt = listenerRedundantNode.redundantStreams.begin();
				auto const* const listenerStreamNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(listenerIt->second);
				auto atLeastOneConnected{ false };
				auto allConnected{ true };
				auto allCompatibleFormat{ true };
				auto allDomainCompatible{ true };
				for (auto idx = 0u; idx < talkerRedundantNode.redundantStreams.size(); ++idx)
				{
					auto const* const redundantTalkerStreamNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(talkerIt->second);
					auto const* const redundantListenerStreamNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(listenerIt->second);
					auto const connected = isStreamConnected(talkerStream.entityID, redundantTalkerStreamNode, redundantListenerStreamNode);
					atLeastOneConnected |= connected;
					allConnected &= connected;
					allCompatibleFormat &= computeFormatCompatible(*redundantTalkerStreamNode, *redundantListenerStreamNode);
					allDomainCompatible &= computeDomainCompatible();
					++talkerIt;
					++listenerIt;
				}

				return computeCapabilities(atLeastOneConnected ? ConnectState::Connected : ConnectState::NotConnected, allConnected, allCompatibleFormat, allDomainCompatible);
			}
			else if ((talkerStream.type == UserData::Type::OutputStreamNode && listenerStream.type == UserData::Type::InputStreamNode)
							 || (talkerStream.type == UserData::Type::RedundantOutputStreamNode && listenerStream.type == UserData::Type::RedundantInputStreamNode)
							 || (talkerStream.type == UserData::Type::RedundantOutputNode && listenerStream.type == UserData::Type::RedundantInputStreamNode)
							 || (talkerStream.type == UserData::Type::RedundantOutputStreamNode && listenerStream.type == UserData::Type::RedundantInputNode))
			{
				la::avdecc::controller::model::StreamOutputNode const* talkerNode{ nullptr };
				la::avdecc::controller::model::StreamInputNode const* listenerNode{ nullptr };

				// If we have the redundant node, use the talker redundant stream associated with the listener redundant stream
				if (talkerStream.type == UserData::Type::RedundantOutputNode)
				{
					auto const& redundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStream.redundantIndex);
					if (listenerStream.redundantStreamOrder >= static_cast<std::int32_t>(redundantNode.redundantStreams.size()))
						throw la::avdecc::controller::ControlledEntity::Exception(la::avdecc::controller::ControlledEntity::Exception::Type::InvalidDescriptorIndex, "Invalid redundant stream index");
					decltype(listenerStream.redundantStreamOrder) idx{ 0 };
					auto it = redundantNode.redundantStreams.begin();
					while (idx != listenerStream.redundantStreamOrder)
					{
						++it;
						++idx;
					}
					talkerNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(it->second);
					AVDECC_ASSERT(talkerNode->isRedundant, "Stream is not redundant");
				}
				else
				{
					talkerNode = &talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStream.streamIndex);
				}
				// If we have the redundant node, use the listener redundant stream associated with the talker redundant stream
				if (listenerStream.type == UserData::Type::RedundantInputNode)
				{
					auto const& redundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStream.redundantIndex);
					if (talkerStream.redundantStreamOrder >= static_cast<std::int32_t>(redundantNode.redundantStreams.size()))
						throw la::avdecc::controller::ControlledEntity::Exception(la::avdecc::controller::ControlledEntity::Exception::Type::InvalidDescriptorIndex, "Invalid redundant stream index");
					decltype(talkerStream.redundantStreamOrder) idx{ 0 };
					auto it = redundantNode.redundantStreams.begin();
					while (idx != talkerStream.redundantStreamOrder)
					{
						++it;
						++idx;
					}
					listenerNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(it->second);
					AVDECC_ASSERT(listenerNode->isRedundant, "Stream is not redundant");
				}
				else
				{
					listenerNode = &listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStream.streamIndex);
				}

				// Get connected state
				auto const areConnected = isStreamConnected(talkerStream.entityID, talkerNode, listenerNode);
				auto const fastConnecting = isStreamFastConnecting(talkerStream.entityID, talkerNode, listenerNode);
				auto const connectState = areConnected ? ConnectState::Connected : (fastConnecting ? ConnectState::FastConnecting : ConnectState::NotConnected);

				// Get stream format compatibility
				auto const isFormatCompatible = computeFormatCompatible(*talkerNode, *listenerNode);

				// Get domain compatibility
				auto const isDomainCompatible = computeDomainCompatible();

				return computeCapabilities(connectState, areConnected, isFormatCompatible, isDomainCompatible);
			}
		}
	}
	catch (...)
	{
	}

	return ConnectionCapabilities::None;
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::addEntity(bool const orientationIsRow, la::avdecc::UniqueIdentifier const entityID)
{
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);
	if (controlledEntity)
	{
		auto const& entityNode = controlledEntity->getEntityNode();
		auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);

		// Initialize orientation-based dispatch variables
		std::function<void(int)> beginInsertFunction;
		std::function<std::pair<QModelIndex, Node&>(QModelIndex)> addFunction;
		std::function<void()> endInsertFunction;
		std::map<la::avdecc::controller::model::VirtualIndex, la::avdecc::controller::model::RedundantStreamNode> const* redundantStreamsList{ nullptr };
		std::map<la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::StreamNode const*> streamsList{};
		UserData::Type singleStreamType{ UserData::Type::None };
		UserData::Type redundantNodeType{ UserData::Type::None };
		UserData::Type redundantStreamType{ UserData::Type::None };
		if (orientationIsRow)
		{
			beginInsertFunction = std::bind(&ConnectionMatrixModel::beginAppendRows, q_ptr, QModelIndex{}, std::placeholders::_1);
			addFunction = std::bind(&ConnectionMatrixModel::appendRow, q_ptr, std::placeholders::_1);
			endInsertFunction = std::bind(&ConnectionMatrixModel::endAppendRows, q_ptr);
			redundantStreamsList = &configurationNode.redundantStreamOutputs;
			for (auto const& s : configurationNode.streamOutputs)
			{
				streamsList.insert(std::make_pair(s.first, &s.second));
			}
			singleStreamType = UserData::Type::OutputStreamNode;
			redundantNodeType = UserData::Type::RedundantOutputNode;
			redundantStreamType = UserData::Type::RedundantOutputStreamNode;
		}
		else
		{
			beginInsertFunction = std::bind(&ConnectionMatrixModel::beginAppendColumns, q_ptr, QModelIndex{}, std::placeholders::_1);
			addFunction = std::bind(&ConnectionMatrixModel::appendColumn, q_ptr, std::placeholders::_1);
			endInsertFunction = std::bind(&ConnectionMatrixModel::endAppendColumns, q_ptr);
			redundantStreamsList = &configurationNode.redundantStreamInputs;
			for (auto const& s : configurationNode.streamInputs)
			{
				streamsList.insert(std::make_pair(s.first, &s.second));
			}
			singleStreamType = UserData::Type::InputStreamNode;
			redundantNodeType = UserData::Type::RedundantInputNode;
			redundantStreamType = UserData::Type::RedundantInputStreamNode;
		}

		// Lambda to run through the index to add
		auto const runThroughEntity = [redundantStreamsList, &streamsList](std::function<void()> entityAction, std::function<void(la::avdecc::controller::model::VirtualIndex)> redundantNodeAction, std::function<void(la::avdecc::entity::model::StreamIndex, la::avdecc::controller::model::VirtualIndex, std::int32_t)> redundantStreamAction, std::function<void(la::avdecc::entity::model::StreamIndex)> singleStreamAction)
		{
			// Entity action
			entityAction();

			// Run through redundant streams
			for (auto const& redundantStreamKV : *redundantStreamsList)
			{
				auto const redundantIndex = redundantStreamKV.first;
				auto const& redundantNode = redundantStreamKV.second;

				// Redundant node action
				redundantNodeAction(redundantIndex);

				// Run through streams of the redundant node
				std::int32_t redundantStreamOrder{ 0 };
				for (auto const& streamKV : redundantNode.redundantStreams)
				{
					auto const streamIndex = streamKV.first;
					auto const& streamNode = streamKV.second;

					// Redundant stream action
					redundantStreamAction(streamIndex, redundantIndex, redundantStreamOrder);
					++redundantStreamOrder;
				}
			}

			// Run through single streams
			for (auto const& streamKV : streamsList)
			{
				auto const streamIndex = streamKV.first;
				auto const * const streamNode = streamKV.second;

				if (!streamNode->isRedundant)
				{
					singleStreamAction(streamIndex);
				}
			}
		};

		// First run, count the number of index we'll need
		auto countIndex{ 0u };
		runThroughEntity([&countIndex]() // Entity action
		{
			++countIndex;
		}, [&countIndex](la::avdecc::controller::model::VirtualIndex const redundantIndex) // Redundant node action
		{
			++countIndex;
		}, [&countIndex](la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::VirtualIndex const redundantIndex, std::int32_t const redundantStreamOrder) // Redundant stream action
		{
			++countIndex;
		}, [&countIndex](la::avdecc::entity::model::StreamIndex const streamIndex) // Single stream action
		{
			++countIndex;
		});

		// Lambda to add a stream
		auto const addNode = [this, entityID, addFunction](UserData::Type const userType, QModelIndex const& rootIndex, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::VirtualIndex const redundantIndex, std::int32_t const redundantStreamOrder)
		{
			auto streamResult = addFunction(rootIndex);
			auto const strIndex = streamResult.first;
			auto& strNode = streamResult.second;
			strNode.userData = QVariant::fromValue(UserData{ userType, entityID, streamIndex, redundantIndex, redundantStreamOrder });
			// Do not expand redundant nodes
			if (userType == UserData::Type::RedundantInputNode || userType == UserData::Type::RedundantOutputNode)
			{
				strNode.isExpanded = false;
			}
			return strIndex;
		};

		// Begin insert
		beginInsertFunction(countIndex);

		// Second run, actually add the nodes
		QModelIndex rootIndex{};
		QModelIndex redundantModelIndex{};

		runThroughEntity([entityID, &addFunction, &rootIndex]() // Entity action
		{
			auto rootResult = addFunction({});
			rootIndex = rootResult.first;
			auto& rootNode = rootResult.second;
			rootNode.userData = QVariant::fromValue(UserData{ UserData::Type::EntityNode, entityID });
		}, [&addNode, &redundantModelIndex, redundantNodeType, &rootIndex](la::avdecc::controller::model::VirtualIndex const redundantIndex) // Redundant node action
		{
			redundantModelIndex = addNode(redundantNodeType, rootIndex, la::avdecc::entity::model::StreamIndex(-1), redundantIndex, std::int32_t(-1));
		}, [&addNode, &redundantModelIndex, redundantStreamType](la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::controller::model::VirtualIndex const redundantIndex, std::int32_t const redundantStreamOrder) // Redundant stream action
		{
			addNode(redundantStreamType, redundantModelIndex, streamIndex, redundantIndex, redundantStreamOrder);
		}, [&addNode, &rootIndex, singleStreamType](la::avdecc::entity::model::StreamIndex const streamIndex) // Single stream action
		{
			addNode(singleStreamType, rootIndex, streamIndex, la::avdecc::controller::model::VirtualIndex(-1), std::int32_t(-1));
		});

		// End insert
		endInsertFunction();
	}
}

void ConnectionMatrixModel::ConnectionMatrixModelPrivate::removeEntity(bool const orientationIsRow, la::avdecc::UniqueIdentifier const entityID)
{
	auto const toCompare = QVariant::fromValue(UserData{ UserData::Type::EntityNode, entityID });

	if (orientationIsRow)
	{
		auto const result = q_ptr->rowAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const index = result.first;
		if (index != -1)
			q_ptr->removeRows(index, q_ptr->countChildren(result.second) + 1);
	}
	else
	{
		auto const result = q_ptr->columnAndNodeForUserData(toCompare, &ConnectionMatrixModel::ConnectionMatrixModelPrivate::areUserDataEqual);
		auto const index = result.first;
		if (index != -1)
			q_ptr->removeColumns(index, q_ptr->countChildren(result.second) + 1);
	}
}

ConnectionMatrixModel::ConnectionMatrixModel(QObject* parent)
	: MatrixModel(parent)
	, d_ptr(new ConnectionMatrixModelPrivate(this))
{
}

ConnectionMatrixModel::~ConnectionMatrixModel() {
	delete d_ptr;
}

QVariant ConnectionMatrixModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	// Early return - Optimization
	if (role != Qt::DisplayRole && role <= Qt::UserRole)
		return MatrixModel::headerData(section, orientation, role);

	Node const* node{ nullptr };

	if (orientation == Qt::Orientation::Vertical)
		node = nodeAtRow(section);
	else
		node = nodeAtColumn(section);

	auto const& userData = *static_cast<UserData const*>(node->userData.constData());
	auto& manager = avdecc::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(userData.entityID);
	if (controlledEntity)
	{
		try // TODO: Try-catch everywhere we get a getControlledEntity, because it might go offline while requesting it
		{
			auto const& entityNode = controlledEntity->getEntityNode();

			switch (role)
			{
				case Qt::DisplayRole:
				{
					switch (userData.type)
					{
						case UserData::Type::EntityNode:
						{
							if (entityNode.dynamicModel->entityName.empty())
								return avdecc::helper::uniqueIdentifierToString(userData.entityID);
							else
								return QString::fromStdString(entityNode.dynamicModel->entityName.str());
						}
						case UserData::Type::InputStreamNode:
						case UserData::Type::RedundantInputStreamNode:
						{
							auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, userData.streamIndex);
							return avdecc::helper::objectName(controlledEntity.get(), streamNode);
						}
						case UserData::Type::OutputStreamNode:
						case UserData::Type::RedundantOutputStreamNode:
						{
							auto const& streamNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, userData.streamIndex);
							return avdecc::helper::objectName(controlledEntity.get(), streamNode);
						}
						case UserData::Type::RedundantInputNode:
							return QString("Redundant Stream Input " + QString::number(userData.redundantIndex));
						case UserData::Type::RedundantOutputNode:
							return QString("Redundant Stream Output " + QString::number(userData.redundantIndex));
						default:
							return "Unknown";
					}
					break;
				}
				case StreamWaitingRole:
				{
					if (userData.type == UserData::Type::InputStreamNode)
					{
						return !controlledEntity->isStreamInputRunning(entityNode.dynamicModel->currentConfiguration, userData.streamIndex);
					}
					else if (userData.type == UserData::Type::OutputStreamNode)
					{
						return !controlledEntity->isStreamOutputRunning(entityNode.dynamicModel->currentConfiguration, userData.streamIndex);
					}
					return false;
					break;
				}
				default:
					AVDECC_ASSERT(false, "Unhandlded case - Don't forget the 'early return' at the start of this function");
			}
		}
		catch (...)
		{
		}
	}

	return MatrixModel::headerData(section, orientation, role);
}

/* ************************************************************ */
/* ConnectionMatrixItemDelegate                                 */
/* ************************************************************ */
void ConnectionMatrixItemDelegate::paint(QPainter* painter, QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	// Highlighted background if needed
	if (option.state & QStyle::State_Selected)
	{
		painter->fillRect(option.rect, option.palette.highlight());
	}

	auto const* const model = static_cast<ConnectionMatrixModel const*>(index.model());
	auto const& talkerNode = model->nodeAtRow(index.row());
	auto const& listenerNode = model->nodeAtColumn(index.column());
	auto const& talkerData = talkerNode->userData.value<UserData>();
	auto const& listenerData = listenerNode->userData.value<UserData>();

	// Entity row or column
	if (talkerData.type == UserData::Type::EntityNode || listenerData.type == UserData::Type::EntityNode)
	{
		drawEntityNoConnection(painter, option.rect);
	}
	else
	{
		// If index is a cross of 2 redundant streams, only the diagonal is connectable
		if (talkerData.type == UserData::Type::RedundantOutputStreamNode && listenerData.type == UserData::Type::RedundantInputStreamNode && talkerData.redundantStreamOrder != listenerData.redundantStreamOrder)
		{
			return;
		}
		auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

		if (caps == ConnectionCapabilities::None)
			return;

		auto const isRedundant = !((talkerData.type == UserData::Type::RedundantOutputNode && listenerData.type == UserData::Type::RedundantInputNode)
															 || (talkerData.type == UserData::Type::OutputStreamNode && listenerData.type == UserData::Type::InputStreamNode));

		if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connected))
		{
			if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainConnectedStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatConnectedStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawConnectedStream(painter, option.rect, isRedundant);
			}
		}
		else if (la::avdecc::hasFlag(caps, ConnectionCapabilities::FastConnecting))
		{
			if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainFastConnectingStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatFastConnectingStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawFastConnectingStream(painter, option.rect, isRedundant);
			}
		}
		else if (la::avdecc::hasFlag(caps, ConnectionCapabilities::PartiallyConnected))
		{
			drawPartiallyConnectedRedundantNode(painter, option.rect);
		}
		else
		{
			if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongDomain))
			{
				drawWrongDomainNotConnectedStream(painter, option.rect, isRedundant);
			}
			else if (la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongFormat))
			{
				drawWrongFormatNotConnectedStream(painter, option.rect, isRedundant);
			}
			else
			{
				drawNotConnectedStream(painter, option.rect, isRedundant);
			}
		}
	}
}

QSize ConnectionMatrixItemDelegate::sizeHint(QStyleOptionViewItem const& option, QModelIndex const& index) const
{
	return QSize();
}

class ConnectionMatrixLegend : public QWidget
{
	Q_OBJECT
public:
	ConnectionMatrixLegend(ConnectionMatrixView& parent)
		: QWidget(&parent)
		, _parent(parent)
	{
		// Layout widgets
		_layout.addWidget(&_buttonContainer, 0, 0);
		_layout.addWidget(&_horizontalPlaceholder, 1, 0);
		_layout.addWidget(&_verticalPlaceholder, 0, 1);
		_layout.setSpacing(2);

		_buttonContainer.setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		_buttonContainerLayout.addWidget(&_button);

		_layout.setRowStretch(0, 1);
		_layout.setRowStretch(1, 0);

		_layout.setColumnStretch(0, 1);
		_layout.setColumnStretch(1, 0);

		_horizontalPlaceholder.setFixedHeight(20);
		_verticalPlaceholder.setFixedWidth(20);

		// Connect button
		connect(&_button, &QPushButton::clicked, this, [this]()
		{
			QDialog dialog;
			QVBoxLayout layout{ &dialog };

			using DrawFunctionType = std::function<void(QPainter*, QRect const&)>;
			auto const separatorDrawFunction = [](QPainter*, QRect const&)
			{
			};

			std::vector<std::pair<DrawFunctionType, QString>> drawFunctions{
				{ static_cast<DrawFunctionType>(std::bind(&drawEntityNoConnection, std::placeholders::_1, std::placeholders::_2)), "Entity connection summary (Not working yet)" },
				{ static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Possible connection for a Simple Stream or Redundant Stream Pair" },
				{ static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, true)), "Possible connection for a Single Stream of a Redundant Stream Pair" },
				{ static_cast<DrawFunctionType>(separatorDrawFunction), "" },
				{ static_cast<DrawFunctionType>(std::bind(&drawNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable without error" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongDomainNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable but incompatible AVB domain" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongFormatNotConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connectable but incompatible stream format" },
				{ static_cast<DrawFunctionType>(std::bind(&drawConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongDomainConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected but incompatible AVB domain" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongFormatConnectedStream, std::placeholders::_1, std::placeholders::_2, false)), "Connected but incompatible stream format" },
				{ static_cast<DrawFunctionType>(std::bind(&drawPartiallyConnectedRedundantNode, std::placeholders::_1, std::placeholders::_2, false)), "Partially connected Redundant Stream Pair" },
				{ static_cast<DrawFunctionType>(std::bind(&drawFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongDomainFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect (incompatible AVB domain)" },
				{ static_cast<DrawFunctionType>(std::bind(&drawWrongFormatFastConnectingStream, std::placeholders::_1, std::placeholders::_2, false)), "Listener trying to fast connect (incompatible stream format)" },
			};
			class IconDrawer : public QWidget
			{
			public:
				IconDrawer(DrawFunctionType const& func)
					: _drawFunction(func)
				{
				}
			private:
				virtual void paintEvent(QPaintEvent*) override
				{
					QPainter painter(this);
					auto const rect = geometry();
					_drawFunction(&painter, QRect(0, 0, rect.width(), rect.height()));
				}
				DrawFunctionType const& _drawFunction;
			};

			for (auto& drawFunction : drawFunctions)
			{
				auto* hlayout = new QHBoxLayout{ &dialog };
				auto* icon = new IconDrawer{ drawFunction.first };
				icon->setFixedSize(_parent.horizontalHeader()->defaultSectionSize(), _parent.verticalHeader()->defaultSectionSize());
				hlayout->addWidget(icon);
				auto* label = new QLabel{ drawFunction.second };
				auto font = label->font();
				//font.setBold(true);
				font.setStyleStrategy(QFont::PreferAntialias);
				label->setFont(font);
				hlayout->addWidget(label);
				layout.addLayout(hlayout);
			}

			QPushButton closeButton{ "Close" };
			connect(&closeButton, &QPushButton::clicked, &dialog, [&dialog]()
			{
				dialog.accept();
			});
			layout.addWidget(&closeButton);

			dialog.setWindowTitle(hive::internals::applicationShortName + " - " + "Connection matrix legend");
			// Run dialog
			dialog.exec();
		});
	}

	Q_SLOT void updateSize()
	{
		setGeometry(0, 0, _parent.verticalHeader()->width(), _parent.horizontalHeader()->height());
	}

	virtual void paintEvent(QPaintEvent*) override
	{
		QPainter painter(this);

		// Whole section
		{
			painter.fillRect(geometry(), QColor("#F5F5F5"));
		}

		// Horizontal section
		{
			painter.save();

			//painter.drawRect(_horizontalPlaceholder.geometry());

			painter.setRenderHint(QPainter::Antialiasing, true);
			auto font = painter.font();
			font.setBold(true);
			painter.setFont(font);

			QTextOption options;
			options.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			painter.drawText(_horizontalPlaceholder.geometry(), "Talkers", options);

			painter.restore();
		}

		// Vertical section
		{
			painter.save();

			//painter.drawRect(_verticalPlaceholder.geometry());

			auto rect = _verticalPlaceholder.geometry();
			painter.translate(rect.bottomLeft());
			painter.rotate(-90);
			auto const drawRect = QRect(0, 0, rect.height(), rect.width());

			painter.setRenderHint(QPainter::Antialiasing, true);
			auto font = painter.font();
			font.setBold(true);
			painter.setFont(font);

			QTextOption options;
			options.setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			painter.drawText(drawRect, "Listeners", options);

			painter.restore();
		}
	}

private:
	ConnectionMatrixView const& _parent;
	QGridLayout _layout{ this };
	QWidget _buttonContainer{ this };
	QVBoxLayout _buttonContainerLayout{ &_buttonContainer };
	QPushButton _button{ "Show Legend", &_buttonContainer };
	QWidget _horizontalPlaceholder{ this };
	QWidget _verticalPlaceholder{ this };
};

/* ************************************************************ */
/* ConnectionMatrixView                                         */
/* ************************************************************ */
ConnectionMatrixView::ConnectionMatrixView(QWidget* parent)
{
	setCornerButtonEnabled(false);
	setMouseTracking(true);

	// Configure highlight color
	auto p = palette();
	p.setColor(QPalette::Highlight, 0xf3e5f5);
	setPalette(p);

	auto* legend = new ConnectionMatrixLegend{ *this };

	_connectionMatrixItemDelegate = std::make_unique<ConnectionMatrixItemDelegate>();
	setItemDelegate(_connectionMatrixItemDelegate.get());

	_connectionMatrixHeaderDelegate = std::make_unique<ConnectionMatrixHeaderDelegate>();

	// Configure table headers
	connect(verticalHeader(), &QHeaderView::geometriesChanged, legend, &ConnectionMatrixLegend::updateSize);
	connect(verticalHeader(), &QHeaderView::customContextMenuRequested, this, &ConnectionMatrixView::onHeaderCustomContextMenuRequested);
	verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	verticalHeader()->setAttribute(Qt::WA_Hover);
	verticalHeader()->installEventFilter(this);
	setVerticalHeaderDelegate(_connectionMatrixHeaderDelegate.get());

	connect(horizontalHeader(), &QHeaderView::geometriesChanged, legend, &ConnectionMatrixLegend::updateSize);
	connect(horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &ConnectionMatrixView::onHeaderCustomContextMenuRequested);
	horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	horizontalHeader()->setAttribute(Qt::WA_Hover);
	horizontalHeader()->installEventFilter(this);
	setHorizontalHeaderDelegate(_connectionMatrixHeaderDelegate.get());

	//
	_connectionMatrixModel = std::make_unique<ConnectionMatrixModel>(this);
	setModel(_connectionMatrixModel.get());

	//
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &ConnectionMatrixView::customContextMenuRequested, this, [this](QPoint const& pos)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();

			auto const index = indexAt(pos);

			if (!index.isValid())
				return;

			auto const* const model = static_cast<ConnectionMatrixModel const*>(index.model());
			auto const& talkerNode = model->nodeAtRow(index.row());
			auto const& listenerNode = model->nodeAtColumn(index.column());
			auto const& talkerData = talkerNode->userData.value<UserData>();
			auto const& listenerData = listenerNode->userData.value<UserData>();
			if ((talkerData.type == UserData::Type::OutputStreamNode && listenerData.type == UserData::Type::InputStreamNode)
					|| (talkerData.type == UserData::Type::RedundantOutputStreamNode && listenerData.type == UserData::Type::RedundantInputStreamNode))
			{
					auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

#pragma message("TODO: Call haveCompatibleFormats(talker, listener)")
				if (caps != ConnectionCapabilities::None && la::avdecc::hasFlag(caps, ConnectionCapabilities::WrongFormat))
				{
					QMenu menu;

					auto* matchTalkerAction = menu.addAction("Match formats using Talker");
					auto* matchListenerAction = menu.addAction("Match formats using Listener");
					menu.addSeparator();
					menu.addAction("Cancel");

#pragma message("TODO: setEnabled() based on format compatibility -> If talker can be set from listener, and vice versa.")
					matchTalkerAction->setEnabled(true);
					matchListenerAction->setEnabled(true);

					if (auto* action = menu.exec(viewport()->mapToGlobal(pos)))
					{
						if (action == matchTalkerAction)
						{
							auto talkerEntity = manager.getControlledEntity(talkerData.entityID);
							if (talkerEntity)
							{
								auto const& talkerEntityNode = talkerEntity->getEntityNode();
								auto const& talkerStreamNode = talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerData.streamIndex);
								manager.setStreamInputFormat(listenerData.entityID, listenerData.streamIndex, talkerStreamNode.dynamicModel->currentFormat);
							}
						}
						else if (action == matchListenerAction)
						{
							auto listenerEntity = manager.getControlledEntity(listenerData.entityID);
							if (listenerEntity)
							{
								auto const& listenerEntityNode = listenerEntity->getEntityNode();
								auto const& listenerStreamNode = listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerData.streamIndex);
								manager.setStreamOutputFormat(talkerData.entityID, talkerData.streamIndex, listenerStreamNode.dynamicModel->currentFormat);
							}
						}
					}
				}
			}
			else if (talkerData.type == UserData::Type::RedundantOutputNode && listenerData.type == UserData::Type::RedundantInputNode)
			{
				// TODO
			}
		}
		catch (...)
		{
		}
	});

	//

	connect(this, &ConnectionMatrixView::clicked, this, [this](QModelIndex const& index)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();

			auto const* const model = static_cast<ConnectionMatrixModel const*>(index.model());
			auto const& talkerNode = model->nodeAtRow(index.row());
			auto const& listenerNode = model->nodeAtColumn(index.column());
			auto const& talkerData = talkerNode->userData.value<UserData>();
			auto const& listenerData = listenerNode->userData.value<UserData>();

			if ((talkerData.type == UserData::Type::OutputStreamNode && listenerData.type == UserData::Type::InputStreamNode)
					|| (talkerData.type == UserData::Type::RedundantOutputStreamNode && listenerData.type == UserData::Type::RedundantInputStreamNode))
			{
				auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

				if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connectable))
				{
					if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connected))
					{
						manager.disconnectStream(talkerData.entityID, talkerData.streamIndex, listenerData.entityID, listenerData.streamIndex);
					}
					else
					{
						manager.connectStream(talkerData.entityID, talkerData.streamIndex, listenerData.entityID, listenerData.streamIndex);
					}
				}
			}
			else if (talkerData.type == UserData::Type::RedundantOutputNode && listenerData.type == UserData::Type::RedundantInputNode)
			{
				auto const caps = model->d_ptr->connectionCapabilities(talkerData, listenerData);

				bool doConnect{ false };
				bool doDisconnect{ false };

				if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connectable))
				{
					if (la::avdecc::hasFlag(caps, ConnectionCapabilities::Connected))
						doDisconnect = true;
					else
						doConnect = true;
				}

				auto talkerEntity = manager.getControlledEntity(talkerData.entityID);
				auto listenerEntity = manager.getControlledEntity(listenerData.entityID);
				if (talkerEntity && listenerEntity)
				{
					auto const& talkerEntityNode = talkerEntity->getEntityNode();
					auto const& talkerEntityInfo = talkerEntity->getEntity();
					auto const& listenerEntityNode = listenerEntity->getEntityNode();
					auto const& listenerEntityInfo = listenerEntity->getEntity();

					auto const& talkerRedundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerData.redundantIndex);
					auto const& listenerRedundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerData.redundantIndex);
					// TODO: Maybe someday handle the case for more than 2 streams for redundancy
					AVDECC_ASSERT(talkerRedundantNode.redundantStreams.size() == listenerRedundantNode.redundantStreams.size(), "More than 2 redundant streams in the set");
					auto talkerIt = talkerRedundantNode.redundantStreams.begin();
					auto listenerIt = listenerRedundantNode.redundantStreams.begin();
					auto atLeastOneConnected{ false };
					auto allConnected{ true };
					auto allCompatibleFormat{ true };
					auto allDomainCompatible{ true };
					for (auto idx = 0u; idx < talkerRedundantNode.redundantStreams.size(); ++idx)
					{
						auto const* const talkerStreamNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(talkerIt->second);
						auto const* const listenerStreamNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(listenerIt->second);
						auto const areConnected = model->d_ptr->isStreamConnected(talkerData.entityID, talkerStreamNode, listenerStreamNode);
						if (doConnect && !areConnected)
						{
							manager.connectStream(talkerData.entityID, talkerStreamNode->descriptorIndex, listenerData.entityID, listenerStreamNode->descriptorIndex);
						}
						else if (doDisconnect && areConnected)
						{
							manager.disconnectStream(talkerData.entityID, talkerStreamNode->descriptorIndex, listenerData.entityID, listenerStreamNode->descriptorIndex);
						}
						++talkerIt;
						++listenerIt;
					}
				}
			}
		}
		catch (...)
		{
		}
	});
}

void ConnectionMatrixView::mouseMoveEvent(QMouseEvent* event)
{
	qt::toolkit::MatrixTreeView::mouseMoveEvent(event);

	auto const index = indexAt(static_cast<QMouseEvent*>(event)->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows|QItemSelectionModel::Columns);
}

bool ConnectionMatrixView::eventFilter(QObject* object, QEvent* event)
{
	if (event->type() == QEvent::Leave)
	{
		selectionModel()->clearSelection();
	}
	else if (event->type() == QEvent::HoverMove)
	{
		if (object == verticalHeader())
		{
			auto const row = verticalHeader()->logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
			selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Rows);
		}
		else if (object == horizontalHeader())
		{
			auto const column = horizontalHeader()->logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
			selectionModel()->select(model()->index(0, column), QItemSelectionModel::ClearAndSelect|QItemSelectionModel::Columns);
		}
	}

	return qt::toolkit::MatrixTreeView::eventFilter(object, event);
}

void ConnectionMatrixView::onHeaderCustomContextMenuRequested(QPoint const& pos)
{
	auto* header = static_cast<QHeaderView*>(sender());

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto const index = header->logicalIndexAt(pos);

		if (index == -1)
			return;

		auto const* const m = static_cast<ConnectionMatrixModel const*>(model());

		auto const isInputStreamKind = header->orientation() == Qt::Orientation::Horizontal;
		qt::toolkit::MatrixModel::Node const* node{ nullptr };
		if (isInputStreamKind)
			node = m->nodeAtColumn(index);
		else
			node = m->nodeAtRow(index);
		if (node == nullptr)
			return;

		auto const& data = node->userData.value<connectionMatrix::UserData>();
		auto controlledEntity = manager.getControlledEntity(data.entityID);
		if (controlledEntity)
		{
			QMenu menu;

			auto const& entityNode = controlledEntity->getEntityNode();
			la::avdecc::controller::model::StreamNode const* streamNode{ nullptr };
			QString streamName{};
			bool isStreamRunning{ false };

			if (isInputStreamKind)
			{
				auto const& streamInputNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, data.streamIndex);
				streamName = avdecc::helper::objectName(controlledEntity.get(), streamInputNode);
				isStreamRunning = controlledEntity->isStreamInputRunning(entityNode.dynamicModel->currentConfiguration, data.streamIndex);
				streamNode = &streamInputNode;
			}
			else
			{
				auto const& streamOutputNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, data.streamIndex);
				streamName = avdecc::helper::objectName(controlledEntity.get(), streamOutputNode);
				isStreamRunning = controlledEntity->isStreamOutputRunning(entityNode.dynamicModel->currentConfiguration, data.streamIndex);
				streamNode = &streamOutputNode;
			}

			{
				auto* header = menu.addAction("Entity: " + avdecc::helper::smartEntityName(*controlledEntity));
				auto font = header->font();
				font.setBold(true);
				header->setFont(font);
				header->setEnabled(false);
			}
			{
				auto* header = menu.addAction("Stream: " + streamName);
				auto font = header->font();
				font.setBold(true);
				header->setFont(font);
				header->setEnabled(false);
			}
			menu.addSeparator();

			auto* startStreamingAction = menu.addAction("Start Streaming");
			auto* stopStreamingAction = menu.addAction("Stop Streaming");
			menu.addSeparator();
			menu.addAction("Cancel");

			startStreamingAction->setEnabled(!isStreamRunning);
			stopStreamingAction->setEnabled(isStreamRunning);

			// Release the controlled entity before starting a long operation (menu.exec)
			controlledEntity.reset();

			if (auto* action = menu.exec(header->mapToGlobal(pos)))
			{
				if (action == startStreamingAction)
				{
					if (isInputStreamKind)
					{
						manager.startStreamInput(data.entityID, data.streamIndex);
					}
					else
					{
						manager.startStreamOutput(data.entityID, data.streamIndex);
					}
				}
				else if (action == stopStreamingAction)
				{
					if (isInputStreamKind)
					{
						manager.stopStreamInput(data.entityID, data.streamIndex);
					}
					else
					{
						manager.stopStreamOutput(data.entityID, data.streamIndex);
					}
				}
			}
		}
	}
	catch (...)
	{
	}
}

static inline void drawCircle(QPainter* painter, QRect const& rect)
{
	painter->drawEllipse(rect.adjusted(3, 3, -3, -3));
}

static inline void drawLozenge(QPainter* painter, QRect const& rect)
{
	auto r = rect.adjusted(4, 4, -4, -4);
	painter->translate(r.center());
	r.moveCenter({});
	painter->rotate(45.0);
	painter->drawRect(r);
}

static inline void drawSquare(QPainter* painter, QRect const& rect)
{
	painter->drawRect(rect.adjusted(3, 3, -3, -3));
}

static inline void drawEntitySummaryFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 1));
	painter->setBrush(color);
	drawSquare(painter, rect);

	painter->restore();
}

static inline void drawConnectedStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(Qt::black, 2));
	painter->setBrush(color);
	drawCircle(painter, rect);

	painter->restore();
}

static inline void drawNotConnectedStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 2));
	painter->setBrush(color);
	drawCircle(painter, rect);

	painter->restore();
}

static inline void drawFastConnectingStreamFigure(QPainter* painter, QRect const& rect, QColor const& colorConnected, QColor const& colorNotConnected)
{
	constexpr auto startAngle = 90;

	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 2));
	painter->setBrush(colorConnected);
	painter->drawPie(rect.adjusted(3, 3, -3, -3), startAngle * 16, 180 * 16);
	painter->setBrush(colorNotConnected);
	painter->drawPie(rect.adjusted(3, 3, -3, -3), (startAngle + 180) * 16, 180 * 16);

	painter->restore();
}

static inline void drawConnectedRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(Qt::black, 1));
	painter->setBrush(color);
	drawLozenge(painter, rect);

	painter->restore();
}

static inline void drawNotConnectedRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& color)
{
	painter->save();

	painter->setRenderHint(QPainter::Antialiasing, true);

	painter->setPen(QPen(QColor("#9E9E9E"), 1));
	painter->setBrush(color);
	drawLozenge(painter, rect);

	painter->restore();
}

static inline void drawFastConnectingRedundantStreamFigure(QPainter* painter, QRect const& rect, QColor const& colorConnected, QColor const& colorNotConnected)
{
	drawNotConnectedRedundantStreamFigure(painter, rect, colorNotConnected); // TODO: Try to draw a lozenge split in 2, like the circle
}

static inline QColor getConnectedColor()
{
	return QColor("#4CAF50");
}
static inline QColor getConnectedWrongDomainColor()
{
	return QColor("#B71C1C");
}

static inline QColor getConnectedWrongFormatColor()
{
	return QColor("#FFD600");
}

static inline QColor getPartiallyConnectedColor()
{
	return QColor("#2196F3");
}

static inline QColor getNotConnectedColor()
{
	return QColor("#F5F5F5");
}

static inline QColor getNotConnectedWrongDomainColor()
{
	return QColor("#FFCDD2");
}

static inline QColor getNotConnectedWrongFormatColor()
{
	return QColor("#FFF9C4");
}

void drawConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedColor());
	else
		drawConnectedStreamFigure(painter, rect, getConnectedColor());
}

void drawWrongDomainConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedWrongDomainColor());
	else
		drawConnectedStreamFigure(painter, rect, getConnectedWrongDomainColor());
}

void drawWrongFormatConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawConnectedRedundantStreamFigure(painter, rect, getConnectedWrongFormatColor());
	else
		drawConnectedStreamFigure(painter, rect, getConnectedWrongFormatColor());
}

void drawFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedColor(), getNotConnectedColor());
	else
		drawFastConnectingStreamFigure(painter, rect, getConnectedColor(), getNotConnectedColor());
}

void drawWrongDomainFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedWrongDomainColor(), getNotConnectedWrongDomainColor());
	else
		drawFastConnectingStreamFigure(painter, rect, getConnectedWrongDomainColor(), getNotConnectedWrongDomainColor());
}

void drawWrongFormatFastConnectingStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawFastConnectingRedundantStreamFigure(painter, rect, getConnectedWrongFormatColor(), getNotConnectedWrongFormatColor());
	else
		drawFastConnectingStreamFigure(painter, rect, getConnectedWrongFormatColor(), getNotConnectedWrongFormatColor());
}

void drawNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedColor());
	else
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedColor());
}

void drawWrongDomainNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedWrongDomainColor());
	else
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedWrongDomainColor());
}

void drawWrongFormatNotConnectedStream(QPainter* painter, QRect const& rect, bool const isRedundant)
{
	if (isRedundant)
		drawNotConnectedRedundantStreamFigure(painter, rect, getNotConnectedWrongFormatColor());
	else
		drawNotConnectedStreamFigure(painter, rect, getNotConnectedWrongFormatColor());
}

void drawPartiallyConnectedRedundantNode(QPainter* painter, QRect const& rect, bool const)
{
	drawConnectedStreamFigure(painter, rect, getPartiallyConnectedColor());
}

void drawEntityNoConnection(QPainter* painter, QRect const& rect)
{
	drawEntitySummaryFigure(painter, rect, QColor("#EEEEEE"));
}

} // namespace connectionMatrix

#include "connectionMatrix.moc"
