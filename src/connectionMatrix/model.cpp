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
#include <QDebug>

namespace connectionMatrix
{

class EntityHeaderItem : public QStandardItem
{
public:
	EntityHeaderItem(la::avdecc::UniqueIdentifier const& entityID)
		: _entityID{entityID}
	{
	}

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			if (auto controlledEntity = manager.getControlledEntity(_entityID))
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				if (entityNode.dynamicModel->entityName.empty())
				{
					return avdecc::helper::uniqueIdentifierToString(_entityID);
				}
				else
				{
					return QString::fromStdString(entityNode.dynamicModel->entityName);
				}
			}
		}
		else if (role == Model::HeaderTypeRole)
		{
			return 0;
		}

		return QStandardItem::data(role);
	}


protected:
	la::avdecc::UniqueIdentifier const _entityID;
};

class StreamHeaderItem : public QStandardItem
{
public:
	StreamHeaderItem(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex)
		: _entityID{entityID}
		, _streamIndex{streamIndex}
	{
	}

	virtual QVariant data(int role) const override
	{
		if (role == Model::HeaderTypeRole)
		{
			return 1;
		}

		return QStandardItem::data(role);
	}

protected:
	la::avdecc::UniqueIdentifier const _entityID;
	la::avdecc::entity::model::StreamIndex const _streamIndex;
};

class OutputStreamHeaderItem : public StreamHeaderItem
{
public:
	using StreamHeaderItem::StreamHeaderItem;

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			if (auto controlledEntity = manager.getControlledEntity(_entityID))
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& streamNode = controlledEntity->getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
				return avdecc::helper::objectName(controlledEntity.get(), streamNode);
			}
		}

		return StreamHeaderItem::data(role);
	}
};

class InputStreamHeaderItem : public StreamHeaderItem
{
public:
	using StreamHeaderItem::StreamHeaderItem;

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			if (auto controlledEntity = manager.getControlledEntity(_entityID))
			{
				auto const& entityNode = controlledEntity->getEntityNode();
				auto const& streamNode = controlledEntity->getStreamInputNode(entityNode.dynamicModel->currentConfiguration, _streamIndex);
				return avdecc::helper::objectName(controlledEntity.get(), streamNode);
			}
		}

		return StreamHeaderItem::data(role);
	}
};

class ConnectionItem : public QStandardItem
{
public:
#if 0
	ConnectionItem(la::avdecc::UniqueIdentifier const& talkerID, la::avdecc::entity::model::StreamIndex const talkerStreamIndex, la::avdecc::UniqueIdentifier const& listenerID, la::avdecc::entity::model::StreamIndex const listenerStreamIndex)
		: _talkerID{talkerID}
		, _talkerStreamIndex{talkerStreamIndex}
		, _listenerID{listenerID}
		, _listenerStreamIndex{listenerStreamIndex}
	{
	}
#endif

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			return QString{"%1, %2"}.arg(index().row()).arg(index().column());
		}
		else if (role == Qt::BackgroundColorRole)
		{
			return QColor{Qt::lightGray};
		}

		return QStandardItem::data(role);
	}

protected:
#if 0
	la::avdecc::UniqueIdentifier const _talkerID;
	la::avdecc::entity::model::StreamIndex const _talkerStreamIndex;
	la::avdecc::UniqueIdentifier const _listenerID;
	la::avdecc::entity::model::StreamIndex const _listenerStreamIndex;
#endif
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
	
	struct EntityInfo
	{
		std::uint32_t section{0};
		std::uint32_t streamCount{0};
	};
	
	std::unordered_map<la::avdecc::UniqueIdentifier::value_type, EntityInfo> _talkers;
	std::unordered_map<la::avdecc::UniqueIdentifier::value_type, EntityInfo> _listeners;
	
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

			// Talker header items

			EntityInfo talkerInfo;
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
			{
				talkerInfo.section = q_ptr->rowCount();
				talkerInfo.streamCount = configurationNode.streamOutputs.size();

				q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), new EntityHeaderItem{entityID});

				for (auto streamIndex = 0u; streamIndex < talkerInfo.streamCount; ++streamIndex)
				{
					q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), new OutputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
				}

				_talkers.insert(std::make_pair(entityID, talkerInfo));
			}

			// Listener header items

			EntityInfo listenerInfo;
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				listenerInfo.section = q_ptr->columnCount();
				listenerInfo.streamCount = configurationNode.streamInputs.size();

				q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new EntityHeaderItem{entityID});

				for (auto streamIndex = 0u; streamIndex < listenerInfo.streamCount; ++streamIndex)
				{
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new InputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
				}

				_listeners.insert(std::make_pair(entityID, listenerInfo));
			}

			// Connection items

			// Add new items for existing rows
			for (auto row = 0u; row < talkerInfo.section; ++row)
			{
			}

			// Add new items for existing columns
			for (auto column = 0u; column < listenerInfo.section; ++column)
			{
			}

			// Add new items
			for (auto subRow = 0u; subRow < talkerInfo.streamCount + 1; ++subRow)
			{
				auto const row = talkerInfo.section + subRow;
				for (auto subColumn = 0u; subColumn < listenerInfo.streamCount + 1; ++subColumn)
				{
					auto const column = listenerInfo.section + subColumn;
					q_ptr->setItem(row, column, new ConnectionItem);
				}
			}
		}
	}
	
	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		// Talker
		{
			auto const it = _talkers.find(entityID.getValue());
			if (it != std::end(_talkers))
			{
				auto const& info{it->second};
				q_ptr->removeRows(info.streamCount, info.streamCount + 1);
			}
		}

		// Listener
		{
			auto const it = _listeners.find(entityID.getValue());
			if (it != std::end(_listeners))
			{
				auto const& info{it->second};
				q_ptr->removeColumns(info.streamCount, info.streamCount + 1);
			}
		}
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
	Model * const q_ptr{ nullptr };
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
