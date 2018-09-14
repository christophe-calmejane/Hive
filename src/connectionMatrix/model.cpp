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
	HeaderItem(la::avdecc::UniqueIdentifier const& entityID)
		: _entityID{entityID}
	{
	}

protected:
	la::avdecc::UniqueIdentifier const _entityID;
};

class EntityHeaderItem : public HeaderItem
{
public:
	using HeaderItem::HeaderItem;

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

		return HeaderItem::data(role);
	}
};

class StreamHeaderItem : public HeaderItem
{
public:
	StreamHeaderItem(la::avdecc::UniqueIdentifier const& entityID, la::avdecc::entity::model::StreamIndex const streamIndex)
		: HeaderItem{entityID}
		, _streamIndex{streamIndex}
	{
	}

protected:
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
		else if (role == Model::HeaderTypeRole)
		{
			return 1;
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
		else if (role == Model::HeaderTypeRole)
		{
			return 1;
		}

		return StreamHeaderItem::data(role);
	}
};

class RedundantOutputStreamHeaderItem : public StreamHeaderItem
{
public:
	using StreamHeaderItem::StreamHeaderItem;

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			return "RedundantOutputStream";
		}
		else if (role == Model::HeaderTypeRole)
		{
			return 2;
		}

		return StreamHeaderItem::data(role);
	}
};

class RedundantInputStreamHeaderItem : public StreamHeaderItem
{
public:
	using StreamHeaderItem::StreamHeaderItem;

	virtual QVariant data(int role) const override
	{
		if (role == Qt::DisplayRole)
		{
			return "RedundantInputStream";
		}
		else if (role == Model::HeaderTypeRole)
		{
			return 2;
		}

		return StreamHeaderItem::data(role);
	}
};

class ConnectionItem : public QStandardItem
{
public:
	using QStandardItem::QStandardItem;
	
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

			// Talker header items

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getTalkerCapabilities(), la::avdecc::entity::TalkerCapabilities::Implemented))
			{
				q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), new EntityHeaderItem{entityID});

				for (auto streamIndex = 0u; streamIndex < configurationNode.streamOutputs.size(); ++streamIndex)
				{
					q_ptr->setVerticalHeaderItem(q_ptr->rowCount(), new OutputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
				}
				
				for (auto streamIndex = 0u; streamIndex < configurationNode.redundantStreamInputs.size(); ++streamIndex)
				{
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new RedundantInputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
				}
			}

			// Listener header items

			if (la::avdecc::hasFlag(controlledEntity->getEntity().getListenerCapabilities(), la::avdecc::entity::ListenerCapabilities::Implemented))
			{
				q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new EntityHeaderItem{entityID});

				for (auto streamIndex = 0u; streamIndex < configurationNode.streamInputs.size(); ++streamIndex)
				{
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new InputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
				}

				for (auto streamIndex = 0u; streamIndex < configurationNode.redundantStreamOutputs.size(); ++streamIndex)
				{
					q_ptr->setHorizontalHeaderItem(q_ptr->columnCount(), new RedundantOutputStreamHeaderItem{entityID, static_cast<la::avdecc::entity::model::StreamIndex>(streamIndex)});
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
