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

#include "connectionMatrix/model.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#	error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

namespace connectionMatrix
{
class HeaderItem : public QStandardItem
{
public:
	using StreamMap = std::unordered_map<la::avdecc::entity::model::StreamIndex, std::int32_t>;

	HeaderItem(Model::NodeType const nodeType, la::avdecc::UniqueIdentifier const& entityID)
		: _nodeType{ nodeType }
		, _entityID{ entityID }
	{
	}

	Model::NodeType nodeType() const
	{
		return _nodeType;
	}

	la::avdecc::UniqueIdentifier const& entityID() const
	{
		return _entityID;
	}

	void setStreamIndex(la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		_streamIndex = streamIndex;
	}

	la::avdecc::entity::model::StreamIndex streamIndex() const
	{
		return _streamIndex;
	}

	void setRedundantIndex(la::avdecc::controller::model::VirtualIndex const redundantIndex)
	{
		_redundantIndex = redundantIndex;
	}

	la::avdecc::controller::model::VirtualIndex redundantIndex() const
	{
		return _redundantIndex;
	}

	void setRedundantStreamOrder(std::int32_t const redundantStreamOrder)
	{
		_redundantStreamOrder = redundantStreamOrder;
	}

	std::int32_t redundantStreamOrder() const
	{
		return _redundantStreamOrder;
	}

	void setParentIndex(std::int32_t const parentIndex)
	{
		_parentIndex = parentIndex;
	}

	std::int32_t parentIndex() const
	{
		return _parentIndex;
	}

	void setChildrenCount(std::int32_t const childrenCount)
	{
		_childrenCount = childrenCount;
	}

	std::int32_t childrenCount() const
	{
		return _childrenCount;
	}

	void setStreamMap(StreamMap const& streamMap)
	{
		_streamMap = streamMap;
	}

	StreamMap const& streamMap() const
	{
		return _streamMap;
	}

	virtual QVariant data(int role) const override
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
		else if (role == Model::ParentIndexRole)
		{
			return QVariant::fromValue(_parentIndex);
		}
		else if (role == Model::ChildrenCountRole)
		{
			return QVariant::fromValue(_childrenCount);
		}
		else if (role == Qt::DisplayRole)
		{
			auto& manager = avdecc::ControllerManager::getInstance();

			if (auto controlledEntity = manager.getControlledEntity(_entityID))
			{
				auto const& entityNode = controlledEntity->getEntityNode();

				if (_nodeType == Model::NodeType::Entity)
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

private:
	Model::NodeType const _nodeType;
	la::avdecc::UniqueIdentifier const _entityID;
	la::avdecc::entity::model::StreamIndex _streamIndex{ static_cast<la::avdecc::entity::model::StreamIndex>(-1) };
	la::avdecc::controller::model::VirtualIndex _redundantIndex{ static_cast<la::avdecc::controller::model::VirtualIndex>(-1) };
	std::int32_t _redundantStreamOrder{ -1 };
	std::int32_t _parentIndex{ -1 };
	std::int32_t _childrenCount{ 0 };
	StreamMap _streamMap{};
};

Model::ConnectionCapabilities computeConnectionCapabilities(HeaderItem const* talkerItem, HeaderItem const* listenerItem)
{
	auto const talkerEntityID{ talkerItem->entityID() };
	auto const listenerEntityID{ listenerItem->entityID() };

	if (talkerEntityID == listenerEntityID)
	{
		return Model::ConnectionCapabilities::None;
	}

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();

		auto talkerEntity = manager.getControlledEntity(talkerEntityID);
		auto listenerEntity = manager.getControlledEntity(listenerEntityID);

		if (talkerEntity && listenerEntity)
		{
			auto const talkerNodeType = talkerItem->nodeType();
			auto const talkerStreamIndex = talkerItem->streamIndex();
			auto const talkerRedundantIndex = talkerItem->redundantIndex();
			auto const talkerRedundantStreamOrder = talkerItem->redundantStreamOrder();
			auto const& talkerEntityNode = talkerEntity->getEntityNode();
			auto const& talkerEntityInfo = talkerEntity->getEntity();

			auto const listenerNodeType = listenerItem->nodeType();
			auto const listenerStreamIndex = listenerItem->streamIndex();
			auto const listenerRedundantIndex = listenerItem->redundantIndex();
			auto const listenerRedundantStreamOrder = listenerItem->redundantStreamOrder();
			auto const& listenerEntityNode = listenerEntity->getEntityNode();
			auto const& listenerEntityInfo = listenerEntity->getEntity();

			auto const computeFormatCompatible = [](la::avdecc::controller::model::StreamOutputNode const& talkerNode, la::avdecc::controller::model::StreamInputNode const& listenerNode)
			{
				return la::avdecc::entity::model::StreamFormatInfo::isListenerFormatCompatibleWithTalkerFormat(listenerNode.dynamicModel->currentFormat, talkerNode.dynamicModel->currentFormat);
			};

			auto const computeDomainCompatible = [&talkerEntity, &listenerEntity, &talkerEntityNode, &listenerEntityNode](auto const talkerAvbInterfaceIndex, auto const listenerAvbInterfaceIndex)
			{
				try
				{
					// Get the AvbInterface associated to the streams
					auto const& talkerAvbInterfaceNode = talkerEntity->getAvbInterfaceNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerAvbInterfaceIndex);
					auto const& listenerAvbInterfaceNode = listenerEntity->getAvbInterfaceNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerAvbInterfaceIndex);

					// Check both have the same grandmaster
					return talkerAvbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID == listenerAvbInterfaceNode.dynamicModel->avbInfo.gptpGrandmasterID;
				}
				catch (...)
				{
					return false;
				}
			};

			enum class ConnectState
			{
				NotConnected = 0,
				FastConnecting,
				Connected,
			};

			auto const computeCapabilities = [](ConnectState const connectState, bool const areAllConnected, bool const isFormatCompatible, bool const isDomainCompatible)
			{
				auto caps{ Model::ConnectionCapabilities::Connectable };

				if (!isDomainCompatible)
					caps |= Model::ConnectionCapabilities::WrongDomain;

				if (!isFormatCompatible)
					caps |= Model::ConnectionCapabilities::WrongFormat;

				if (connectState != ConnectState::NotConnected)
				{
					if (areAllConnected)
						caps |= Model::ConnectionCapabilities::Connected;
					else if (connectState == ConnectState::FastConnecting)
						caps |= Model::ConnectionCapabilities::FastConnecting;
					else
						caps |= Model::ConnectionCapabilities::PartiallyConnected;
				}

				return caps;
			};

			// Special case for both redundant nodes
			if (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput)
			{
				// Check if all redundant streams are connected
				auto const& talkerRedundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerRedundantIndex);
				auto const& listenerRedundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerRedundantIndex);
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
					auto const connected = avdecc::helper::isStreamConnected(talkerEntityID, redundantTalkerStreamNode, redundantListenerStreamNode);
					atLeastOneConnected |= connected;
					allConnected &= connected;
					allCompatibleFormat &= computeFormatCompatible(*redundantTalkerStreamNode, *redundantListenerStreamNode);
					allDomainCompatible &= computeDomainCompatible(redundantTalkerStreamNode->staticModel->avbInterfaceIndex, redundantListenerStreamNode->staticModel->avbInterfaceIndex);
					++talkerIt;
					++listenerIt;
				}

				return computeCapabilities(atLeastOneConnected ? ConnectState::Connected : ConnectState::NotConnected, allConnected, allCompatibleFormat, allDomainCompatible);
			}
			else if ((talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream) || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream) || (talkerNodeType == Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInputStream) || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInput))
			{
				la::avdecc::controller::model::StreamOutputNode const* talkerNode{ nullptr };
				la::avdecc::controller::model::StreamInputNode const* listenerNode{ nullptr };

				// If we have the redundant node, use the talker redundant stream associated with the listener redundant stream
				if (talkerNodeType == Model::NodeType::RedundantOutput)
				{
					auto const& redundantNode = talkerEntity->getRedundantStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerRedundantIndex);
					if (listenerRedundantStreamOrder >= static_cast<std::int32_t>(redundantNode.redundantStreams.size()))
						throw la::avdecc::controller::ControlledEntity::Exception(la::avdecc::controller::ControlledEntity::Exception::Type::InvalidDescriptorIndex, "Invalid redundant stream index");
					std::remove_const<decltype(listenerRedundantStreamOrder)>::type idx{ 0 };
					auto it = redundantNode.redundantStreams.begin();
					while (idx != listenerRedundantStreamOrder)
					{
						++it;
						++idx;
					}
					talkerNode = static_cast<la::avdecc::controller::model::StreamOutputNode const*>(it->second);
					AVDECC_ASSERT(talkerNode->isRedundant, "Stream is not redundant");
				}
				else
				{
					talkerNode = &talkerEntity->getStreamOutputNode(talkerEntityNode.dynamicModel->currentConfiguration, talkerStreamIndex);
				}
				// If we have the redundant node, use the listener redundant stream associated with the talker redundant stream
				if (listenerNodeType == Model::NodeType::RedundantInput)
				{
					auto const& redundantNode = listenerEntity->getRedundantStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerRedundantIndex);
					if (talkerRedundantStreamOrder >= static_cast<std::int32_t>(redundantNode.redundantStreams.size()))
						throw la::avdecc::controller::ControlledEntity::Exception(la::avdecc::controller::ControlledEntity::Exception::Type::InvalidDescriptorIndex, "Invalid redundant stream index");
					std::remove_const<decltype(talkerRedundantStreamOrder)>::type idx{ 0 };
					auto it = redundantNode.redundantStreams.begin();
					while (idx != talkerRedundantStreamOrder)
					{
						++it;
						++idx;
					}
					listenerNode = static_cast<la::avdecc::controller::model::StreamInputNode const*>(it->second);
					AVDECC_ASSERT(listenerNode->isRedundant, "Stream is not redundant");
				}
				else
				{
					listenerNode = &listenerEntity->getStreamInputNode(listenerEntityNode.dynamicModel->currentConfiguration, listenerStreamIndex);
				}

				// Get connected state
				auto const areConnected = avdecc::helper::isStreamConnected(talkerEntityID, talkerNode, listenerNode);
				auto const fastConnecting = avdecc::helper::isStreamFastConnecting(talkerEntityID, talkerNode, listenerNode);
				auto const connectState = areConnected ? ConnectState::Connected : (fastConnecting ? ConnectState::FastConnecting : ConnectState::NotConnected);

				// Get stream format compatibility
				auto const isFormatCompatible = computeFormatCompatible(*talkerNode, *listenerNode);

				// Get domain compatibility
				auto const isDomainCompatible = computeDomainCompatible(talkerNode->staticModel->avbInterfaceIndex, listenerNode->staticModel->avbInterfaceIndex);

				return computeCapabilities(connectState, areConnected, isFormatCompatible, isDomainCompatible);
			}
		}
	}
	catch (...)
	{
	}

	return Model::ConnectionCapabilities::None;
}

class ConnectionItem : public QStandardItem
{
public:
	virtual void setData(QVariant const& value, int role) override
	{
		if (role == Model::ConnectionCapabilitiesRole)
		{
			_capabilities = value.value<Model::ConnectionCapabilities>();
		}
		else
		{
			QStandardItem::setData(value, role);
		}
	}
	virtual QVariant data(int role) const override
	{
		if (role == Model::ConnectionCapabilitiesRole)
		{
			return QVariant::fromValue(_capabilities);
		}

		return QStandardItem::data(role);
	}

private:
	Model::ConnectionCapabilities _capabilities{ Model::ConnectionCapabilities::None };
};

class ModelPrivate : public QObject
{
	Q_OBJECT
public:
	ModelPrivate(Model* q)
		: q_ptr{ q }
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();
		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ModelPrivate::controllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ModelPrivate::entityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ModelPrivate::entityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::streamRunningChanged, this, &ModelPrivate::streamRunningChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamConnectionChanged, this, &ModelPrivate::streamConnectionChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamFormatChanged, this, &ModelPrivate::streamFormatChanged);
		connect(&controllerManager, &avdecc::ControllerManager::gptpChanged, this, &ModelPrivate::gptpChanged);
		connect(&controllerManager, &avdecc::ControllerManager::entityNameChanged, this, &ModelPrivate::entityNameChanged);
		connect(&controllerManager, &avdecc::ControllerManager::streamNameChanged, this, &ModelPrivate::streamNameChanged);
	}

	// Slots for avdecc::ControllerManager signals

	Q_SLOT void controllerOffline()
	{
		q_ptr->clear();
	}

	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
		try
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);
			if (controlledEntity && AVDECC_ASSERT_WITH_RET(!controlledEntity->gotFatalEnumerationError(), "An entity should not be set online if it had an enumeration error"))
			{
				if (!la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
				{
					return;
				}

				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);

				auto const previousRowCount{ q_ptr->rowCount() };
				auto const previousColumnCount{ q_ptr->columnCount() };

				// Talker

				if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented) && !configurationNode.streamOutputs.empty())
				{
					std::int32_t streamMapIndex{ 0 };
					HeaderItem::StreamMap streamMap{};

					std::int32_t const entityItemIndex{ q_ptr->rowCount() };
					std::int32_t entityItemChildrenCount{ 0 };

					auto* entityItem = new HeaderItem(Model::NodeType::Entity, entityID);
					q_ptr->setVerticalHeaderItem(entityItemIndex, entityItem);

					// Redundant streams
					for (auto const& output : configurationNode.redundantStreamOutputs)
					{
						std::int32_t const redundantItemIndex{ q_ptr->rowCount() };
						std::int32_t redundantItemChildrenCount{ 0 };

						auto const& redundantIndex{ output.first };
						auto const& redundantNode{ output.second };

						auto* redundantItem = new HeaderItem(Model::NodeType::RedundantOutput, entityID);
						redundantItem->setParentIndex(entityItemIndex);
						redundantItem->setRedundantIndex(redundantIndex);
						q_ptr->setVerticalHeaderItem(redundantItemIndex, redundantItem);

						++entityItemChildrenCount;
						++streamMapIndex;

						std::int32_t redundantStreamOrder{ 0 };
						for (auto const& streamKV : redundantNode.redundantStreams)
						{
							std::int32_t const redundantStreamItemIndex{ q_ptr->rowCount() };

							auto const& streamIndex{ streamKV.first };
							streamMap.insert(std::make_pair(streamIndex, ++streamMapIndex));

							auto* redundantStreamItem = new HeaderItem(Model::NodeType::RedundantOutputStream, entityID);
							redundantStreamItem->setParentIndex(redundantItemIndex);
							redundantStreamItem->setStreamIndex(streamIndex);
							redundantStreamItem->setRedundantIndex(redundantIndex);
							redundantStreamItem->setRedundantStreamOrder(redundantStreamOrder);
							q_ptr->setVerticalHeaderItem(redundantStreamItemIndex, redundantStreamItem);

							++redundantStreamOrder;

							++redundantItemChildrenCount;
							++entityItemChildrenCount;
						}

						redundantItem->setChildrenCount(redundantItemChildrenCount);
					}

					// Single streams
					for (auto const& output : configurationNode.streamOutputs)
					{
						auto const& streamIndex{ output.first };
						auto const& streamNode{ output.second };

						if (!streamNode.isRedundant)
						{
							std::int32_t const streamItemIndex{ q_ptr->rowCount() };
							streamMap.insert(std::make_pair(streamIndex, ++streamMapIndex));

							auto* streamItem = new HeaderItem{ Model::NodeType::OutputStream, entityID };
							streamItem->setParentIndex(entityItemIndex);
							streamItem->setStreamIndex(streamIndex);
							q_ptr->setVerticalHeaderItem(streamItemIndex, streamItem);

							++entityItemChildrenCount;
						}
					}

					entityItem->setChildrenCount(entityItemChildrenCount);
					entityItem->setStreamMap(streamMap);

					// Create new connection items
					for (auto column = 0; column < q_ptr->columnCount(); ++column)
					{
						for (auto row = previousRowCount; row < q_ptr->rowCount(); ++row)
						{
							auto* connectionItem = new ConnectionItem;
							q_ptr->setItem(row, column, connectionItem);
						}
					}

					dataChanged(talkerIndex(entityID), false, true);
				}

				// Listener

				if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented) && !configurationNode.streamInputs.empty())
				{
					std::int32_t streamMapIndex{ 0 };
					HeaderItem::StreamMap streamMap{};

					std::int32_t const entityItemIndex{ q_ptr->columnCount() };
					std::int32_t entityItemChildrenCount{ 0 };

					auto* entityItem = new HeaderItem{ Model::NodeType::Entity, entityID };
					q_ptr->setHorizontalHeaderItem(entityItemIndex, entityItem);

					// Redundant streams
					for (auto const& input : configurationNode.redundantStreamInputs)
					{
						std::int32_t const redundantItemIndex{ q_ptr->columnCount() };
						std::int32_t redundantItemChildrenCount{ 0 };

						auto const& redundantIndex{ input.first };
						auto const& redundantNode{ input.second };

						auto* redundantItem = new HeaderItem(Model::NodeType::RedundantInput, entityID);
						redundantItem->setParentIndex(entityItemIndex);
						redundantItem->setRedundantIndex(redundantIndex);
						q_ptr->setHorizontalHeaderItem(redundantItemIndex, redundantItem);

						++entityItemChildrenCount;
						++streamMapIndex;

						std::int32_t redundantStreamOrder{ 0 };
						for (auto const& streamKV : redundantNode.redundantStreams)
						{
							std::int32_t const redundantStreamItemIndex{ q_ptr->columnCount() };

							auto const& streamIndex{ streamKV.first };
							streamMap.insert(std::make_pair(streamIndex, ++streamMapIndex));

							auto* redundantStreamItem = new HeaderItem(Model::NodeType::RedundantInputStream, entityID);
							redundantStreamItem->setParentIndex(redundantItemIndex);
							redundantStreamItem->setStreamIndex(streamIndex);
							redundantStreamItem->setRedundantIndex(redundantIndex);
							redundantStreamItem->setRedundantStreamOrder(redundantStreamOrder);
							q_ptr->setHorizontalHeaderItem(redundantStreamItemIndex, redundantStreamItem);

							++redundantStreamOrder;

							++redundantItemChildrenCount;
							++entityItemChildrenCount;
						}

						redundantItem->setChildrenCount(redundantItemChildrenCount);
					}

					// Single streams
					for (auto const& input : configurationNode.streamInputs)
					{
						auto const& streamIndex{ input.first };
						auto const& streamNode{ input.second };

						if (!streamNode.isRedundant)
						{
							std::int32_t const streamItemIndex{ q_ptr->columnCount() };
							streamMap.insert(std::make_pair(streamIndex, ++streamMapIndex));

							auto* streamItem = new HeaderItem{ Model::NodeType::InputStream, entityID };
							streamItem->setParentIndex(entityItemIndex);
							streamItem->setStreamIndex(streamIndex);
							q_ptr->setHorizontalHeaderItem(streamItemIndex, streamItem);

							++entityItemChildrenCount;
						}
					}

					entityItem->setChildrenCount(entityItemChildrenCount);
					entityItem->setStreamMap(streamMap);

					// Create new connection cells
					for (auto row = 0; row < q_ptr->rowCount(); ++row)
					{
						for (auto column = previousColumnCount; column < q_ptr->columnCount(); ++column)
						{
							auto* connectionItem = new ConnectionItem;
							q_ptr->setItem(row, column, connectionItem);
						}
					}

					dataChanged(listenerIndex(entityID), false, true);
				}
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// Ignore exception
		}
		catch (...)
		{
			// Uncaught exception
			AVDECC_ASSERT(false, "Uncaught exception");
		}
	}

	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		// Talker
		auto const row = talkerIndex(entityID).row();
		if (row != -1)
		{
			auto const childrenCount = q_ptr->headerData(row, Qt::Vertical, Model::ChildrenCountRole).value<std::int32_t>();
			q_ptr->removeRows(row, childrenCount + 1);
		}

		// Listener
		auto const column = listenerIndex(entityID).column();
		if (column != -1)
		{
			auto const childrenCount = q_ptr->headerData(column, Qt::Horizontal, Model::ChildrenCountRole).value<std::int32_t>();
			q_ptr->removeColumns(column, childrenCount + 1);
		}
	}

	Q_SLOT void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			// Refresh header for specified talker output stream
			auto const index = talkerStreamIndex(entityID, streamIndex).row();
			if (index != -1)
			{
				emit q_ptr->headerDataChanged(Qt::Vertical, index, index);
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			// Refresh header for specified listener input stream
			auto const index = listenerStreamIndex(entityID, streamIndex).row();
			if (index != -1)
			{
				emit q_ptr->headerDataChanged(Qt::Horizontal, index, index);
			}
		}
	}

	Q_SLOT void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state)
	{
		auto const entityID = state.listenerStream.entityID;
		auto const streamIndex = state.listenerStream.streamIndex;
		auto const index = listenerStreamIndex(entityID, streamIndex);

		// Refresh whole column for specified listener single stream and redundant stream if it exists and the listener itself (no need to refresh the talker)
		LOG_HIVE_DEBUG(QString("connectionMatrix::Model::streamConnectionChanged: ListenerID=%1 Index=%2 (Row=%3 Column=%4 and parents)").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex).arg(index.row()).arg(index.column()));
		dataChanged(index, true, false);
	}

	Q_SLOT void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		LOG_HIVE_DEBUG(QString("connectionMatrix::Model::streamFormatChanged: EntityID=%1 Index=%2").arg(avdecc::helper::uniqueIdentifierToString(entityID)).arg(streamIndex));

		dataChanged(talkerStreamIndex(entityID, streamIndex), true, false);
		dataChanged(listenerStreamIndex(entityID, streamIndex), true, false);
	}

	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		dataChanged(talkerIndex(entityID), true, true);
		dataChanged(listenerIndex(entityID), true, true);
	}

	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
		headerDataChanged(talkerIndex(entityID), false, false);
		headerDataChanged(listenerIndex(entityID), false, false);
	}

	Q_SLOT void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			headerDataChanged(talkerStreamIndex(entityID, streamIndex), true, false);
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			headerDataChanged(listenerStreamIndex(entityID, streamIndex), true, false);
		}
	}

	QModelIndex talkerIndex(la::avdecc::UniqueIdentifier const entityID) const
	{
		for (auto row = 0; row < q_ptr->rowCount(); ++row)
		{
			auto* item = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
			if (item->nodeType() == Model::NodeType::Entity && item->entityID() == entityID)
			{
				return q_ptr->createIndex(row, -1);
			}
		}

		return {};
	}

	QModelIndex talkerStreamIndex(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		for (auto row = 0; row < q_ptr->rowCount(); ++row)
		{
			auto* item = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
			if (item->nodeType() == Model::NodeType::Entity && item->entityID() == entityID)
			{
				auto const& streamMap = item->streamMap();
				auto const offset = streamMap.at(streamIndex);
				return q_ptr->createIndex(row + offset, -1);
			}
		}
		return {};
	}

	QModelIndex listenerIndex(la::avdecc::UniqueIdentifier const entityID) const
	{
		for (auto column = 0; column < q_ptr->columnCount(); ++column)
		{
			// Refresh whole rows for specified talker
			auto* item = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
			if (item->nodeType() == Model::NodeType::Entity && item->entityID() == entityID)
			{
				// Refresh whole columns for specified listener
				return q_ptr->createIndex(-1, column);
			}
		}

		return {};
	}

	QModelIndex listenerStreamIndex(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const
	{
		for (auto column = 0; column < q_ptr->columnCount(); ++column)
		{
			// Refresh talker header
			auto* item = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
			if (item->nodeType() == Model::NodeType::Entity && item->entityID() == entityID)
			{
				// Refresh listener header
				auto const& streamMap = item->streamMap();
				auto const offset = streamMap.at(streamIndex);
				return q_ptr->createIndex(-1, column + offset);
			}
		}

		return {};
	}

	void headerDataChanged(QModelIndex const& index, bool const andParents, bool const andChildren)
	{
		if (index.row() == -1 && index.column() == -1)
		{
			// Not found
			return;
		}

		if (index.column() == -1)
		{
			// Talker
			auto const section{ index.row() };

			emit q_ptr->headerDataChanged(Qt::Vertical, section, section);

			if (andParents)
			{
				auto const parentIndex = q_ptr->headerData(section, Qt::Vertical, Model::ParentIndexRole).value<std::int32_t>();
				if (parentIndex != -1)
				{
					headerDataChanged(q_ptr->createIndex(parentIndex, -1), andParents, false);
				}
			}

			if (andChildren)
			{
				auto const childrenCount = q_ptr->headerData(section, Qt::Vertical, Model::ChildrenCountRole).value<std::int32_t>();
				for (auto childrenIndex = 0; childrenIndex < childrenCount; ++childrenIndex)
				{
					headerDataChanged(q_ptr->createIndex(section + 1 + childrenIndex, -1), false, false);
				}
			}
		}
		else if (index.row() == -1)
		{
			// Listener
			auto const section{ index.column() };

			emit q_ptr->headerDataChanged(Qt::Horizontal, section, section);

			if (andParents)
			{
				auto const parentIndex = q_ptr->headerData(section, Qt::Horizontal, Model::ParentIndexRole).value<std::int32_t>();
				if (parentIndex != -1)
				{
					headerDataChanged(q_ptr->createIndex(-1, parentIndex), andParents, false);
				}
			}

			if (andChildren)
			{
				auto const childrenCount = q_ptr->headerData(section, Qt::Horizontal, Model::ChildrenCountRole).value<std::int32_t>();
				for (auto childrenIndex = 0; childrenIndex < childrenCount; ++childrenIndex)
				{
					headerDataChanged(q_ptr->createIndex(-1, section + 1 + childrenIndex), false, false);
				}
			}
		}
	}

	void dataChanged(QModelIndex const& index, bool const andParents, bool const andChildren)
	{
		if (index.row() == -1 && index.column() == -1)
		{
			// Not found
			return;
		}

		if (index.column() == -1)
		{
			// Talker
			auto const section{ index.row() };

			auto const topLeft = q_ptr->createIndex(section, 0);
			auto const bottomRight = q_ptr->createIndex(section, q_ptr->columnCount() - 1);

			updateIntersectionCapabilities(topLeft, bottomRight);
			emit q_ptr->dataChanged(topLeft, bottomRight, { Model::ConnectionCapabilitiesRole });

			if (andParents)
			{
				auto const parentIndex = q_ptr->headerData(section, Qt::Vertical, Model::ParentIndexRole).value<std::int32_t>();
				if (parentIndex != -1)
				{
					dataChanged(q_ptr->createIndex(parentIndex, -1), andParents, false);
				}
			}

			if (andChildren)
			{
				auto const childrenCount = q_ptr->headerData(section, Qt::Vertical, Model::ChildrenCountRole).value<std::int32_t>();
				auto const firstChildSection = section + 1;

				auto const childrenTopLeft = q_ptr->createIndex(firstChildSection, 0);
				auto const childrenBottomRight = q_ptr->createIndex(firstChildSection + childrenCount - 1, q_ptr->columnCount() - 1);

				updateIntersectionCapabilities(childrenTopLeft, childrenBottomRight);
				emit q_ptr->dataChanged(childrenTopLeft, childrenBottomRight, { Model::ConnectionCapabilitiesRole });
			}
		}
		else if (index.row() == -1)
		{
			// Listener
			auto const section{ index.column() };

			auto const topLeft = q_ptr->createIndex(0, section);
			auto const bottomRight = q_ptr->createIndex(q_ptr->rowCount() - 1, section);

			updateIntersectionCapabilities(topLeft, bottomRight);
			emit q_ptr->dataChanged(topLeft, bottomRight, { Model::ConnectionCapabilitiesRole });

			if (andParents)
			{
				auto const parentIndex = q_ptr->headerData(section, Qt::Horizontal, Model::ParentIndexRole).value<std::int32_t>();
				if (parentIndex != -1)
				{
					dataChanged(q_ptr->createIndex(-1, parentIndex), andParents, false);
				}
			}

			if (andChildren)
			{
				auto const childrenCount = q_ptr->headerData(section, Qt::Horizontal, Model::ChildrenCountRole).value<std::int32_t>();
				auto const firstChildSection = section + 1;

				auto const childrenTopLeft = q_ptr->createIndex(0, firstChildSection);
				auto const childrenBottomRight = q_ptr->createIndex(q_ptr->rowCount() - 1, firstChildSection + childrenCount - 1);

				updateIntersectionCapabilities(childrenTopLeft, childrenBottomRight);
				emit q_ptr->dataChanged(childrenTopLeft, childrenBottomRight, { Model::ConnectionCapabilitiesRole });
			}
		}
	}

	void updateIntersectionCapabilities(QModelIndex const& topLeft, QModelIndex const& bottomRight)
	{
		for (auto row = topLeft.row(); row <= bottomRight.row(); ++row)
		{
			auto const* talkerItem = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));

			if (talkerItem)
			{
				for (auto column = topLeft.column(); column <= bottomRight.column(); ++column)
				{
					auto const* listenerItem = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));

					if (listenerItem)
					{
						auto const capabilities = computeConnectionCapabilities(talkerItem, listenerItem);
						q_ptr->item(row, column)->setData(QVariant::fromValue(capabilities), Model::ConnectionCapabilitiesRole);
					}
				}
			}
		}
	}

private:
	Model* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(Model);
};

Model::Model(QObject* parent)
	: QStandardItemModel(parent)
	, d_ptr{ new ModelPrivate{ this } }
{
}

Model::~Model()
{
	delete d_ptr;
}

} // namespace connectionMatrix

#include "model.moc"
