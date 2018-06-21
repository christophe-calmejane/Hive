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

#include "controlledEntityTreeWidget.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"

#include "nodeVisitor.hpp"

#include <QHeaderView>
#include <QMenu>

class TreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
};

class ControlledEntityTreeWidgetPrivate : public QObject, public la::avdecc::controller::model::EntityModelVisitor
{
public:
	ControlledEntityTreeWidgetPrivate(ControlledEntityTreeWidget* q)
		: q_ptr(q)
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();

		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ControlledEntityTreeWidgetPrivate::controllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ControlledEntityTreeWidgetPrivate::entityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ControlledEntityTreeWidgetPrivate::entityOffline);
	}

	Q_SLOT void controllerOffline()
	{
		Q_Q(ControlledEntityTreeWidget);

		q->setControlledEntityID(la::avdecc::getUninitializedIdentifier());
		q->clearSelection();

		_entityExpandedStates.clear();
	}

	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
		loadCurrentControlledEntity();
	}

	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		_entityExpandedStates.erase(entityID);

		Q_Q(ControlledEntityTreeWidget);
		q->clearSelection();
	}

	void saveExpandedState()
	{
		// Build expanded state
		NodeExpandedStates states;

		Q_Q(ControlledEntityTreeWidget);
		for (auto const& kv : _map)
		{
			auto const& node{kv.first};
			auto const& item{kv.second};
			states.insert({node, q->isItemExpanded(item)});
		}

		// Save expanded state for previous EntityID
		_entityExpandedStates[_controlledEntityID] = states;
	}

	void restoreExpandedState()
	{
		Q_Q(ControlledEntityTreeWidget);

		// Load expanded state if any
		auto const statesIt = _entityExpandedStates.find(_controlledEntityID);
		if (statesIt != _entityExpandedStates.end())
		{
			// Restore expanded state
			auto const& states = statesIt->second;
			for (auto const& kv : _map)
			{
				auto const& node{kv.first};
				auto const& item{kv.second};
				try
				{
					q->setItemExpanded(item, states.at(node));
				}
				catch (...)
				{
				}
			}
		}
	}

	void loadCurrentControlledEntity()
	{
		Q_Q(ControlledEntityTreeWidget);

		q->clear();
		_map.clear();

		if (!la::avdecc::isValidUniqueIdentifier(_controlledEntityID))
			return;

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_controlledEntityID);

		if (controlledEntity)
		{
			controlledEntity->accept(this);
		}

		// Restore expanded state for new EntityID
		restoreExpandedState();
	}

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_controlledEntityID == entityID)
		{
			return;
		}

		Q_Q(ControlledEntityTreeWidget);

		if (la::avdecc::isValidUniqueIdentifier(_controlledEntityID))
		{
			saveExpandedState();
		}

		_controlledEntityID = entityID;

		loadCurrentControlledEntity();
	}

	la::avdecc::UniqueIdentifier controlledEntityID() const
	{
		return _controlledEntityID;
	}

	void customContextMenuRequested(QPoint const& pos)
	{
		Q_Q(ControlledEntityTreeWidget);

		auto const item = static_cast<TreeWidgetItem*>(q->itemAt(pos));
		if (!item)
		{
			return;
		}

		auto const* node = find(item);
		if (!node)
		{
			return;
		}

		if (node->descriptorType == la::avdecc::entity::model::DescriptorType::Configuration)
		{
			auto const* configurationNode = static_cast<la::avdecc::controller::model::ConfigurationNode const*>(node);

			QMenu menu;

			auto* setAsCurrentConfigurationAction = menu.addAction("Set As Current Configuration");
			menu.addSeparator();
			menu.addAction("Cancel");

			setAsCurrentConfigurationAction->setEnabled(!configurationNode->dynamicModel->isActiveConfiguration);

			if (auto* action = menu.exec(q->mapToGlobal(pos)))
			{
				if (action == setAsCurrentConfigurationAction)
				{
					avdecc::ControllerManager::getInstance().setConfiguration(_controlledEntityID, configurationNode->descriptorIndex);
				}
			}
		}
	}

private:
	la::avdecc::controller::model::Node const* find(TreeWidgetItem const* item)
	{
		for (auto const& it : _map)
		{
			if (it.second == item)
			{
				return it.first;
			}
		}

		return nullptr;
	}

	TreeWidgetItem* find(la::avdecc::controller::model::Node const* const node)
	{
		return node ? _map.at(node) : nullptr;
	}

	template<typename T>
	TreeWidgetItem* addItem(la::avdecc::controller::model::Node const* parent, T const* node, QString const& name) noexcept
	{
		auto* item = new TreeWidgetItem;

		item->setData(0, Qt::DisplayRole, name);

		auto const anyNode = AnyNode(node);
		item->setData(0, Qt::UserRole, QVariant::fromValue(anyNode));

		if (auto* parentItem = find(parent))
		{
			parentItem->addChild(item);
		}
		else
		{
			Q_Q(ControlledEntityTreeWidget);
			q->addTopLevelItem(item);
		}

		_map.insert({ node, item });

		return item;
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::EntityNode const& node) noexcept override
	{
		auto genName = [type = node.descriptorType](QString const& name)
		{
			return QString("%1: %2").arg(avdecc::helper::descriptorTypeToString(type), name);
		};
		auto* item = addItem(parent, &node, genName(node.dynamicModel->entityName.data()));

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::entityNameChanged, item, [genName, item](la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
		{
			auto name = genName(entityName);
			item->setData(0, Qt::DisplayRole, name);
		});

		Q_Q(ControlledEntityTreeWidget);
		q->setItemExpanded(item, true);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		auto genName = [type = node.descriptorType, index = node.descriptorIndex](QString const& name)
		{
			return QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(type), QString::number(index), name);
		};
		auto* item = addItem(parent, &node, genName(avdecc::helper::configurationName(controlledEntity, node)));

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::configurationNameChanged, item, [genName, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& /*configurationName*/)
		{
			if (configurationIndex == node.descriptorIndex)
			{
				auto& manager = avdecc::ControllerManager::getInstance();
				auto controlledEntity = manager.getControlledEntity(entityID);

				if (controlledEntity)
				{
					auto name = genName(avdecc::helper::configurationName(controlledEntity.get(), node));
					item->setData(0, Qt::DisplayRole, name);
				}
			}
		});

		if (node.dynamicModel->isActiveConfiguration)
		{
			QFont boldFont;
			boldFont.setBold(true);
			item->setData(0, Qt::FontRole, boldFont);

			Q_Q(ControlledEntityTreeWidget);
			q->setItemExpanded(item, true);
		}
	}

	template<class Node>
	QString genName(la::avdecc::controller::ControlledEntity const* const controlledEntity, Node const& node)
	{
		return QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex), avdecc::helper::objectName(controlledEntity, node));
	}
	template<class Node>
	void updateName(TreeWidgetItem* item, Node const& node, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex)
	{
		if (descriptorType == node.descriptorType && descriptorIndex == node.descriptorIndex)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			if (controlledEntity && configurationIndex == controlledEntity->getEntityNode().dynamicModel->currentConfiguration)
			{
				auto name = genName(controlledEntity.get(), node);
				item->setData(0, Qt::DisplayRole, name);
			}
		}
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		// Do not show redundant streams that have Configuration as direct parent
		if (!node.isRedundant || parent->descriptorType != la::avdecc::entity::model::DescriptorType::Configuration)
		{
			auto const name = genName(controlledEntity, node);
			auto* item = addItem(parent, &node, name);

			connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamNameChanged, item, [this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& /*streamName*/)
			{
				updateName(item, node, entityID, configurationIndex, descriptorType, streamIndex);
			});
		}
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		// Do not show redundant streams that have Configuration as direct parent
		if (!node.isRedundant || parent->descriptorType != la::avdecc::entity::model::DescriptorType::Configuration)
		{
			auto const name = genName(controlledEntity, node);
			auto* item = addItem(parent, &node, name);

			connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamNameChanged, item, [this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& /*streamName*/)
			{
				updateName(item, node, entityID, configurationIndex, descriptorType, streamIndex);
			});
		}
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override
	{
		// Only add ClockSource nodes that are not direct parent of Configuration (use aliases only)
		if (parent->descriptorType != la::avdecc::entity::model::DescriptorType::Configuration)
		{
			auto const name = genName(controlledEntity, node);
			addItem(parent, &node, name);
		}
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::LocaleNode const& node) noexcept override
	{
		auto const name = QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex), node.staticModel->localeID.data());
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StringsNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::StreamPortNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::AudioMapNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::RedundantStreamNode const& node) noexcept override
	{
		auto const name = QString("REDUNDANT_%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.virtualIndex));
		addItem(parent, &node, name);
	}

	void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		addItem(parent, &node, name);
	}

private:
	ControlledEntityTreeWidget * const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControlledEntityTreeWidget);

	la::avdecc::UniqueIdentifier _controlledEntityID{ la::avdecc::getUninitializedIdentifier() };
	std::unordered_map<la::avdecc::controller::model::Node const*, TreeWidgetItem*> _map;

	using NodeExpandedStates = std::unordered_map<la::avdecc::controller::model::Node const*, bool>;
	std::unordered_map<la::avdecc::UniqueIdentifier, NodeExpandedStates> _entityExpandedStates;
};

ControlledEntityTreeWidget::ControlledEntityTreeWidget(QWidget* parent)
	: QTreeWidget(parent)
	, d_ptr(new ControlledEntityTreeWidgetPrivate(this))
{
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	header()->hide();

	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, &QTreeWidget::customContextMenuRequested, this, [this](QPoint const& pos)
	{
		Q_D(ControlledEntityTreeWidget);
		d->customContextMenuRequested(pos);
	});
}

ControlledEntityTreeWidget::~ControlledEntityTreeWidget()
{
	delete d_ptr;
}

void ControlledEntityTreeWidget::setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
{
	Q_D(ControlledEntityTreeWidget);
	d->setControlledEntityID(entityID);
}

la::avdecc::UniqueIdentifier ControlledEntityTreeWidget::controlledEntityID() const
{
	Q_D(const ControlledEntityTreeWidget);
	return d->controlledEntityID();
}
