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

class StandardHeaderItem : public QStandardItem
{
public:
	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			auto const nodeType = QStandardItem::data(Model::NodeTypeRole).value<Model::NodeType>();
			auto const entityID = QStandardItem::data(Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
			
			auto& manager = avdecc::ControllerManager::getInstance();
			
			if (auto controlledEntity = manager.getControlledEntity(entityID))
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				
				if (nodeType == Model::NodeType::Entity)
				{
					if (entityNode.dynamicModel->entityName.empty())
					{
						return avdecc::helper::uniqueIdentifierToString(entityID);
					}
					else
					{
						return QString::fromStdString(entityNode.dynamicModel->entityName);
					}
				}
				else if (nodeType == Model::NodeType::InputStream)
				{
					auto const streamIndex = QStandardItem::data(Model::InputStreamIndexRole).toInt();
					auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, streamIndex);
					return avdecc::helper::objectName(controlledEntity.get(), streamNode);
				}
				else if (nodeType == Model::NodeType::OutputStream)
				{
					auto const streamIndex = QStandardItem::data(Model::OutputStreamIndexRole).toInt();
					auto const& streamNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, streamIndex);
					return avdecc::helper::objectName(controlledEntity.get(), streamNode);
				}
			}
		}
		
		return QStandardItem::data(role);
	}
};

Model::ConnectionCapabilities computeConnectionCapabilities(la::avdecc::UniqueIdentifier const& talkerID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const& listenerID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex)
{
	if (talkerID == listenerID)
	{
		return Model::ConnectionCapabilities::None;
	}
	
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		
		auto talkerEntity = manager.getControlledEntity(talkerID);
		auto listenerEntity = manager.getControlledEntity(listenerID);
		
		return Model::ConnectionCapabilities::Connected;
	}
	catch (...)
	{
		// Nope
	}
	
	return Model::ConnectionCapabilities::None;
}

class ConnectionItem : public QStandardItem
{
public:
	virtual QVariant data(int role) const override
	{
		if (role == Model::ConnectionCapabilitiesRole)
		{
			auto const talkerID = QStandardItem::data(Model::TalkerIDRole).value<la::avdecc::UniqueIdentifier>();
			auto const talkerStreamIndex = QStandardItem::data(Model::TalkerStreamIndexRole).toInt();
			auto const listenerID = QStandardItem::data(Model::ListenerIDRole).value<la::avdecc::UniqueIdentifier>();
			auto const listenerStreamIndex = QStandardItem::data(Model::ListenerStreamIndexRole).toInt();
			
			return QVariant::fromValue(computeConnectionCapabilities(talkerID, talkerStreamIndex, listenerID, listenerStreamIndex));
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
				auto* entityItem = new StandardHeaderItem;
				entityItem->setData(QVariant::fromValue(Model::NodeType::Entity), Model::NodeTypeRole);
				entityItem->setData(QVariant::fromValue(entityID), Model::EntityIDRole);
				q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), entityItem);
				
				for (auto streamIndex = 0u; streamIndex < configurationNode.streamOutputs.size(); ++streamIndex)
				{
					auto* streamItem = new StandardHeaderItem;
					streamItem->setData(QVariant::fromValue(Model::NodeType::OutputStream), Model::NodeTypeRole);
					streamItem->setData(QVariant::fromValue(entityID), Model::EntityIDRole);
					streamItem->setData(streamIndex, Model::OutputStreamIndexRole);
					q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), streamItem);
				}
				
				// Create new connection cells
				for (auto column = 0; column < q_ptr->columnCount(); ++column)
				{
					auto* listenerItem = q_ptr->horizontalHeaderItem(column);
					auto const listenerID = listenerItem->data(Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
					auto const listenerStreamIndex = listenerItem->data(Model::InputStreamIndexRole).toInt();
					
					for (auto row = previousRowCount; row < q_ptr->rowCount(); ++row)
					{
						auto const talkerID{entityID};
						auto const talkerStreamIndex{row - previousRowCount - 1 /* the entity */};
					
						auto* connectionItem = new ConnectionItem;
						connectionItem->setData(QVariant::fromValue(talkerID), Model::TalkerIDRole);
						connectionItem->setData(talkerStreamIndex, Model::TalkerStreamIndexRole);
						connectionItem->setData(QVariant::fromValue(listenerID), Model::ListenerIDRole);
						connectionItem->setData(listenerStreamIndex, Model::ListenerStreamIndexRole);
						q_ptr->setItem(row, column, connectionItem);
					}
				}
			}

			// Listener

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				auto* entityItem = new StandardHeaderItem;
				entityItem->setData(QVariant::fromValue(Model::NodeType::Entity), Model::NodeTypeRole);
				entityItem->setData(QVariant::fromValue(entityID), Model::EntityIDRole);
				q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), entityItem);
				
				for (auto streamIndex = 0u; streamIndex < configurationNode.streamInputs.size(); ++streamIndex)
				{
					auto* streamItem = new StandardHeaderItem;
					streamItem->setData(QVariant::fromValue(Model::NodeType::InputStream), Model::NodeTypeRole);
					streamItem->setData(QVariant::fromValue(entityID), Model::EntityIDRole);
					streamItem->setData(streamIndex, Model::InputStreamIndexRole);
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), streamItem);
				}
				
				// Create new connection cells
				for (auto row = 0; row < q_ptr->rowCount(); ++row)
				{
					auto* talkerItem = q_ptr->verticalHeaderItem(row);
					auto const talkerID = talkerItem->data(Model::EntityIDRole).value<la::avdecc::UniqueIdentifier>();
					auto const talkerStreamIndex = talkerItem->data(Model::InputStreamIndexRole).toInt();
					
					for (auto column = previousColumnCount; column < q_ptr->columnCount(); ++column)
					{
						auto const listenerID{entityID};
						auto const listenerStreamIndex{column - previousRowCount - 1 /* the entity */};
					
						auto* connectionItem = new ConnectionItem;
						connectionItem->setData(QVariant::fromValue(talkerID), Model::TalkerIDRole);
						connectionItem->setData(talkerStreamIndex, Model::TalkerStreamIndexRole);
						connectionItem->setData(QVariant::fromValue(listenerID), Model::ListenerIDRole);
						connectionItem->setData(listenerStreamIndex, Model::ListenerStreamIndexRole);
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
