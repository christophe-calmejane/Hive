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

#include <QDebug>

namespace connectionMatrix
{
	
class StandardItem : public QStandardItem
{
public:
	StandardItem()
	{
		qDebug() << '+' << this;
	}
	~StandardItem()
	{
		qDebug() << '-' << this;
	}
	virtual QVariant data(int role) const override
	{
		if (role == Qt::BackgroundColorRole)
		{
			return QColor{Qt::red};
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
	
	struct EntityInfo
	{
		int section{0};
		int streamCount{0};
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
			
			auto const prevRowCount{q_ptr->rowCount()};
			auto const prevColumnCount{q_ptr->columnCount()};
			
			auto newRowCount{prevRowCount};
			auto newColumnCount{prevColumnCount};
			
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
			{
				EntityInfo info;
				info.section = _talkers.size();
				info.streamCount = configurationNode.streamOutputs.size();
				_talkers.insert(std::make_pair(entityID.getValue(), info));
				
				newRowCount += info.streamCount + 1;
			}
			
			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				EntityInfo info;
				info.section = _listeners.size();
				info.streamCount = configurationNode.streamInputs.size();
				_listeners.insert(std::make_pair(entityID.getValue(), info));
				
				newColumnCount += info.streamCount + 1;
			}
			
			q_ptr->setRowCount(newRowCount);
			q_ptr->setColumnCount(newColumnCount);

			// Add missing column items for each new row
			for (auto row = prevRowCount; row < newRowCount; ++row)
			{
				for (auto column = 0; column < prevColumnCount; ++column)
				{
					q_ptr->setItem(row, column, new StandardItem);
				}
			}
			
			for (auto column = prevColumnCount; column < newColumnCount; ++column)
			{
				for (auto row = 0; row < prevRowCount; ++row)
				{
					q_ptr->setItem(row, column, new StandardItem);
				}
			}
			
			emit q_ptr->dataChanged(q_ptr->index(0, 0), q_ptr->index(newRowCount, newColumnCount));
			
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
