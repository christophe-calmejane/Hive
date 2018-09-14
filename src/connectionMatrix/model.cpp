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

#ifndef ENABLE_AVDECC_FEATURE_REDUNDANCY
#error "Hive requires Redundancy Feature to be enabled in AVDECC Library"
#endif // ENABLE_AVDECC_FEATURE_REDUNDANCY

namespace connectionMatrix
{

class HeaderItem : public QStandardItem
{
public:
	HeaderItem(Model::NodeType const nodeType, la::avdecc::UniqueIdentifier const& entityID)
		: _nodeType{nodeType}
		, _entityID{entityID}
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
				else if (_nodeType == Model::NodeType::InputStream)
				{
					auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
					return avdecc::helper::objectName(controlledEntity.get(), streamNode);
				}
				else if (_nodeType == Model::NodeType::OutputStream)
				{
					auto const& streamNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
					return avdecc::helper::objectName(controlledEntity.get(), streamNode);
				}
			}
		}
		
		return QStandardItem::data(role);
	}
	
private:
	Model::NodeType const _nodeType;
	la::avdecc::UniqueIdentifier const _entityID;
	la::avdecc::entity::model::StreamIndex _streamIndex{static_cast<la::avdecc::entity::model::StreamIndex>(-1)};
	la::avdecc::controller::model::VirtualIndex _redundantIndex{static_cast<la::avdecc::controller::model::VirtualIndex>(-1)};
	std::int32_t _redundantStreamOrder{-1};
};

bool isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

bool isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::FastConnecting) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

Model::ConnectionCapabilities computeConnectionCapabilities(HeaderItem* talkerItem, HeaderItem* listenerItem)
{
	auto const talkerEntityID{talkerItem->entityID()};
	auto const listenerEntityID{listenerItem->entityID()};
	
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
					auto const connected = isStreamConnected(talkerEntityID, redundantTalkerStreamNode, redundantListenerStreamNode);
					atLeastOneConnected |= connected;
					allConnected &= connected;
					allCompatibleFormat &= computeFormatCompatible(*redundantTalkerStreamNode, *redundantListenerStreamNode);
					allDomainCompatible &= computeDomainCompatible();
					++talkerIt;
					++listenerIt;
				}

				return computeCapabilities(atLeastOneConnected ? ConnectState::Connected : ConnectState::NotConnected, allConnected, allCompatibleFormat, allDomainCompatible);
			}
			else if ((talkerNodeType == Model::NodeType::OutputStream && listenerNodeType == Model::NodeType::InputStream)
							 || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantOutputStream)
							 || (talkerNodeType ==Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInput)
							 || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream))
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
				auto const areConnected = isStreamConnected(talkerEntityID, talkerNode, listenerNode);
				auto const fastConnecting = isStreamFastConnecting(talkerEntityID, talkerNode, listenerNode);
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

	return Model::ConnectionCapabilities::None;
}

class ConnectionItem : public QStandardItem
{
public:
	virtual QVariant data(int role) const override
	{
		auto* talkerItem = static_cast<HeaderItem*>(model()->verticalHeaderItem(index().row()));
		auto* listenerItem = static_cast<HeaderItem*>(model()->horizontalHeaderItem(index().column()));
		
		if (role == Model::TalkerNodeTypeRole)
		{
			return QVariant::fromValue(talkerItem->nodeType());
		}
		else if (role == Model::ListenerNodeTypeRole)
		{
			return QVariant::fromValue(listenerItem->nodeType());
		}
		if (role == Model::TalkerRedundantStreamOrderRole)
		{
			return QVariant::fromValue(talkerItem->redundantStreamOrder());
		}
		else if (role == Model::ListenerRedundantStreamOrderRole)
		{
			return QVariant::fromValue(listenerItem->redundantStreamOrder());
		}
		else if (role == Model::ConnectionCapabilitiesRole)
		{
			return QVariant::fromValue(computeConnectionCapabilities(talkerItem, listenerItem));
		}
		
		return QStandardItem::data(role);
	}
};

class ModelPrivate : public QObject
{
	Q_OBJECT
public:
	ModelPrivate(Model* q)
	: q_ptr{q}
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
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);
		if (controlledEntity && AVDECC_ASSERT_WITH_RET(!controlledEntity->gotFatalEnumerationError(), "An entity should not be set online if it had an enumeration error"))
		{
			auto const& entityNode = controlledEntity->getEntityNode();
			auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			
			auto const previousRowCount{q_ptr->rowCount()};
			auto const previousColumnCount{q_ptr->columnCount()};

			// Talker

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
			{
				auto* entityItem = new HeaderItem(Model::NodeType::Entity, entityID);
				q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), entityItem);
				
				for (auto const& output : configurationNode.streamOutputs)
				{
					auto* streamItem = new HeaderItem{Model::NodeType::OutputStream, entityID};
					streamItem->setStreamIndex(output.first);
					q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), streamItem);
				}
				
				// Create new connection items
				for (auto column = 0; column < q_ptr->columnCount(); ++column)
				{
					for (auto row = previousRowCount; row < q_ptr->rowCount(); ++row)
					{
						auto* connectionItem = new ConnectionItem;
						q_ptr->setItem(row, column, connectionItem);
					}
				}
			}

			// Listener

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				auto* entityItem = new HeaderItem{Model::NodeType::Entity, entityID};
				q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), entityItem);
				
				for (auto const& output : configurationNode.streamInputs)
				{
					auto* streamItem = new HeaderItem{Model::NodeType::InputStream, entityID};
					streamItem->setStreamIndex(output.first);
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), streamItem);
				}
				
				// Create new connection cells
				for (auto row = 0; row < q_ptr->rowCount(); ++row)
				{
					for (auto column = previousColumnCount; column < q_ptr->columnCount(); ++column)
					{
						auto* connectionItem = new ConnectionItem;
						q_ptr->setItem(row, column, connectionItem);
					}
				}
			}
		}
	}
	
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
	}
	
	Q_SLOT void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
	{
	}
	
	Q_SLOT void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state)
	{
	}
	
	Q_SLOT void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
	}
	
	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
	}
	
	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
	}
	
	Q_SLOT void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
	}

private:
	Model* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(Model);
};
	
Model::Model(QObject* parent)
	: QStandardItemModel(parent)
	, d_ptr{new ModelPrivate{this}}
{
}
	
Model::~Model()
{
	delete d_ptr;
}

} // namespace connectionMatrix

#include "model.moc"
