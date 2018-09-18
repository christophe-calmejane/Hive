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
					return QString{"Redundant Stream Input %1"}.arg(QString::number(_redundantIndex));
				}
				else if (_nodeType == Model::NodeType::RedundantOutput)
				{
					return QString{"Redundant Stream Output %1"}.arg(QString::number(_redundantIndex));
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

Model::ConnectionCapabilities computeConnectionCapabilities(HeaderItem const* talkerItem, HeaderItem const* listenerItem)
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
					auto const connected = avdecc::helper::isStreamConnected(talkerEntityID, redundantTalkerStreamNode, redundantListenerStreamNode);
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
							 || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInputStream)
							 || (talkerNodeType ==Model::NodeType::RedundantOutput && listenerNodeType == Model::NodeType::RedundantInputStream)
							 || (talkerNodeType == Model::NodeType::RedundantOutputStream && listenerNodeType == Model::NodeType::RedundantInput))
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
	Model::ConnectionCapabilities _capabilities{Model::ConnectionCapabilities::None};
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
			if (!la::avdecc::hasFlag(controlledEntity->getEntity().getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
			{
				return;
			}

			auto const& entityNode = controlledEntity->getEntityNode();
			auto const& configurationNode = controlledEntity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			
			auto const previousRowCount{q_ptr->rowCount()};
			auto const previousColumnCount{q_ptr->columnCount()};

			// Talker

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
			{
				auto* entityItem = new HeaderItem(Model::NodeType::Entity, entityID);
				q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), entityItem);
				
				// Redundant streams
				for (auto const& output : configurationNode.redundantStreamOutputs)
				{
					auto const& redundantIndex{output.first};
					auto const& redundantNode{output.second};
					
					auto* redundantItem = new HeaderItem(Model::NodeType::RedundantOutput, entityID);
					redundantItem->setRedundantIndex(redundantIndex);
					q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), redundantItem);

					std::int32_t redundantStreamOrder{0};
					for (auto const& streamKV : redundantNode.redundantStreams)
					{
						auto const& streamIndex{streamKV.first};
						
						auto* redundantStreamItem = new HeaderItem(Model::NodeType::RedundantOutputStream, entityID);
						redundantStreamItem->setStreamIndex(streamIndex);
						redundantStreamItem->setRedundantIndex(redundantIndex);
						redundantStreamItem->setRedundantStreamOrder(redundantStreamOrder);
						q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), redundantStreamItem);
						
						++redundantStreamOrder;
					}
				}
				
				// Single streams
				for (auto const& output : configurationNode.streamOutputs)
				{
					auto const& streamIndex{output.first};
					auto const& streamNode{output.second};
					
					if (!streamNode.isRedundant)
					{
						auto* streamItem = new HeaderItem{Model::NodeType::OutputStream, entityID};
						streamItem->setStreamIndex(streamIndex);
						q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), streamItem);
					}
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
				
				auto const talkerInfo = talkerSectionInfo(entityID);
				if (talkerInfo.first >= 0)
				{
					talkerDataChanged(talkerInfo);
				}
			}

			// Listener

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				auto* entityItem = new HeaderItem{Model::NodeType::Entity, entityID};
				q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), entityItem);
				
				// Redundant streams
				for (auto const& input : configurationNode.redundantStreamInputs)
				{
					auto const& redundantIndex{input.first};
					auto const& redundantNode{input.second};
					
					auto* redundantItem = new HeaderItem(Model::NodeType::RedundantInput, entityID);
					redundantItem->setRedundantIndex(redundantIndex);
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), redundantItem);
					
					std::int32_t redundantStreamOrder{0};
					for (auto const& streamKV : redundantNode.redundantStreams)
					{
						auto const& streamIndex{streamKV.first};
						
						auto* redundantStreamItem = new HeaderItem(Model::NodeType::RedundantInputStream, entityID);
						redundantStreamItem->setStreamIndex(streamIndex);
						redundantStreamItem->setRedundantIndex(redundantIndex);
						redundantStreamItem->setRedundantStreamOrder(redundantStreamOrder);
						q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), redundantStreamItem);
						
						++redundantStreamOrder;
					}
				}
				
				// Single streams
				for (auto const& input : configurationNode.streamInputs)
				{
					auto const& streamIndex{input.first};
					auto const& streamNode{input.second};
					
					if (!streamNode.isRedundant)
					{
						auto* streamItem = new HeaderItem{Model::NodeType::InputStream, entityID};
						streamItem->setStreamIndex(streamIndex);
						q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), streamItem);
					}
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
				
				auto const listenerInfo = listenerSectionInfo(entityID);
				if (listenerInfo.first >= 0)
				{
					listenerDataChanged(listenerInfo);
				}
			}
		}
	}
	
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		// Talker
		auto const talkerInfo = talkerSectionInfo(entityID);
		if (talkerInfo.first >= 0)
		{
			q_ptr->removeRows(talkerInfo.first, talkerInfo.second);
		}
	
		// Listener
		auto const listenerInfo = listenerSectionInfo(entityID);
		if (listenerInfo.first >= 0)
		{
			q_ptr->removeColumns(listenerInfo.first, listenerInfo.second);
		}
	}
	
	Q_SLOT void streamRunningChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isRunning)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			// Refresh header for specified talker output stream
			auto const talkerInfo = talkerSectionInfo(entityID);
			if (talkerInfo.first >= 0)
			{
				emit q_ptr->headerDataChanged(Qt::Vertical, talkerInfo.first + 1 + streamIndex, talkerInfo.first + 1 + streamIndex);
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			// Refresh header for specified listener input stream
			auto const listenerInfo = listenerSectionInfo(entityID);
			if (listenerInfo.first >= 0)
			{
				emit q_ptr->headerDataChanged(Qt::Horizontal, listenerInfo.first + 1 + streamIndex, listenerInfo.first + 1 + streamIndex);
			}
		}
	}
	
	Q_SLOT void streamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& state)
	{
		auto const entityID = state.listenerStream.entityID;
		auto const streamIndex = state.listenerStream.streamIndex;
		
		auto const listenerInfo = listenerSectionInfo(entityID);
		if (listenerInfo.first >= 0)
		{
			// Refresh whole column for specified listener single stream
			listenerDataChanged({listenerInfo.first, 0});
			
			// Refresh whole column for specified listener redundant stream
			// TODO
			
			// Refresh whole column for specified listener
			for (auto column = listenerInfo.first; column < listenerInfo.first + listenerInfo.second; ++column)
			{
				auto* item = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
				if (item->entityID() == entityID && item->streamIndex() == streamIndex)
				{
					listenerDataChanged({column, 1});
					break;
				}
			}
		}
	}
	
	Q_SLOT void streamFormatChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			// Refresh whole row for specified talker single stream
			auto const talkerInfo = talkerSectionInfo(entityID);
			if (talkerInfo.first >= 0)
			{
				for (auto row = talkerInfo.first; row < talkerInfo.first + talkerInfo.second; ++row)
				{
					auto* item = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
					if (item->nodeType() == Model::NodeType::OutputStream && item->streamIndex() == streamIndex)
					{
						talkerDataChanged({row, 1});
					}
				}
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			// Refresh whole column for specified listener single stream
			auto const listenerInfo = listenerSectionInfo(entityID);
			if (listenerInfo.first >= 0)
			{
				for (auto column = listenerInfo.first; column < listenerInfo.first + listenerInfo.second; ++column)
				{
					auto* item = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
					if (item->nodeType() == Model::NodeType::InputStream && item->streamIndex() == streamIndex)
					{
						listenerDataChanged({column, 1});
					}
				}
			}
		}
	}
	
	Q_SLOT void gptpChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::UniqueIdentifier const grandMasterID, std::uint8_t const grandMasterDomain)
	{
		// Refresh whole rows for specified talker
		auto const talkerInfo = talkerSectionInfo(entityID);
		if (talkerInfo.first >= 0)
		{
			talkerDataChanged(talkerInfo);
		}

		// Refresh whole columns for specified listener
		auto const listenerInfo = listenerSectionInfo(entityID);
		if (listenerInfo.first >= 0)
		{
			listenerDataChanged(listenerInfo);
		}
	}
	
	Q_SLOT void entityNameChanged(la::avdecc::UniqueIdentifier const entityID)
	{
		auto const talkerInfo = talkerSectionInfo(entityID);
		if (talkerInfo.first >= 0)
		{
			emit q_ptr->headerDataChanged(Qt::Vertical, talkerInfo.first, talkerInfo.first);
		}

		auto const listenerInfo = listenerSectionInfo(entityID);
		if (listenerInfo.first >= 0)
		{
			emit q_ptr->headerDataChanged(Qt::Horizontal, listenerInfo.first, listenerInfo.first);
		}
	}
	
	Q_SLOT void streamNameChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex)
	{
		if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
		{
			auto const talkerInfo = talkerSectionInfo(entityID);
			if (talkerInfo.first >= 0)
			{
				for (auto row = talkerInfo.first; row < talkerInfo.first + talkerInfo.second; ++row)
				{
					auto* item = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
					if (item->entityID() == entityID && item->streamIndex() == streamIndex)
					{
						emit q_ptr->headerDataChanged(Qt::Vertical, row, row);
						break;
					}
				}
			}
		}
		else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
		{
			auto const listenerInfo = listenerSectionInfo(entityID);
			if (listenerInfo.first >= 0)
			{
				for (auto column = listenerInfo.first; column < listenerInfo.first + listenerInfo.second; ++column)
				{
					auto* item = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
					if (item->entityID() == entityID && item->streamIndex() == streamIndex)
					{
						emit q_ptr->headerDataChanged(Qt::Horizontal, column, column);
						break;
					}
				}
			}
		}
	}
	
	// Returns the a pair representing the pos of the entity and its "children" count including itself
	QPair<int, int> talkerSectionInfo(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto pos{-1};
		auto count{0};
		
		for (auto row = 0; row < q_ptr->rowCount(); ++row)
		{
			auto* startItem = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
			if (startItem->entityID() == entityID)
			{
				if (!count)
				{
					pos = row;
				}
				
				++count;
			}
			else if (count)
			{
				break;
			}
		}
		
		return {pos, count};
	}
	
	// Returns the a pair representing the pos of the entity and its "children" count including itself
	QPair<int, int> listenerSectionInfo(la::avdecc::UniqueIdentifier const& entityID) const
	{
		auto pos{-1};
		auto count{0};
		
		for (auto column = 0; column < q_ptr->columnCount(); ++column)
		{
			auto* startItem = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
			if (startItem->entityID() == entityID)
			{
				if (!count)
				{
					pos = column;
				}
				
				++count;
			}
			else if (count)
			{
				break;
			}
		}
		
		return {pos, count};
	}
	
	void talkerDataChanged(QPair<int, int> const& talkerInfo)
	{
		auto const topLeft = q_ptr->createIndex(talkerInfo.first, 0);
		auto const bottomRight = q_ptr->createIndex(talkerInfo.first + talkerInfo.second, q_ptr->columnCount());
		
		updateCapabilities(topLeft, bottomRight);
		
		emit q_ptr->dataChanged(topLeft, bottomRight);
	}
	
	void listenerDataChanged(QPair<int, int> const& listenerInfo)
	{
		auto const topLeft = q_ptr->createIndex(0, listenerInfo.first);
		auto const bottomRight = q_ptr->createIndex(q_ptr->rowCount(), listenerInfo.first + listenerInfo.second);
		
		updateCapabilities(topLeft, bottomRight);
		
		emit q_ptr->dataChanged(topLeft, bottomRight);
	}
	
	void updateCapabilities(QModelIndex const& topLeft, QModelIndex const& bottomRight)
	{
		for (auto row = topLeft.row(); row < bottomRight.row(); ++row)
		{
			auto const* talkerItem = static_cast<HeaderItem*>(q_ptr->verticalHeaderItem(row));
			for (auto column = topLeft.column(); column < bottomRight.column(); ++column)
			{
				auto const* listenerItem = static_cast<HeaderItem*>(q_ptr->horizontalHeaderItem(column));
				auto const capabilities = computeConnectionCapabilities(talkerItem, listenerItem);
				
				q_ptr->item(row, column)->setData(QVariant::fromValue(capabilities), Model::ConnectionCapabilitiesRole);
			}
		}
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
