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

#include "controlledEntityTreeWidget.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"

#include "nodeVisitor.hpp"

#include <QHeaderView>
#include <QMenu>

#include <unordered_set>

enum class Kind
{
	None,
	EntityModelNode,
	VirtualNode,
};

// Base node item
class NodeItem : public QObject, public QTreeWidgetItem
{
	using QObject::parent;

public:
	Kind kind() const
	{
		return _kind;
	}

	la::avdecc::entity::model::DescriptorType descriptorType() const
	{
		return _descriptorType;
	}

	la::avdecc::entity::model::DescriptorIndex descriptorIndex() const
	{
		return _descriptorIndex;
	}

	void setHasError(bool const hasError)
	{
		_hasError = hasError;
		setForeground(0, _hasError ? Qt::red : Qt::black);

		// Also update the parent node
		if (auto* parent = static_cast<NodeItem*>(QTreeWidgetItem::parent()))
		{
			if (parent->kind() == Kind::VirtualNode)
			{
				parent->updateHasError();
			}
		}
	}

	bool hasError() const
	{
		return _hasError;
	}

protected:
	NodeItem(Kind const kind, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QString const& name)
		: _kind{ kind }
		, _descriptorType{ descriptorType }
		, _descriptorIndex{ descriptorIndex }
	{
		setData(0, Qt::DisplayRole, name);
	}

	virtual void updateHasError() {}

private:
	la::avdecc::entity::model::DescriptorType const _descriptorType{ la::avdecc::entity::model::DescriptorType::Invalid };
	la::avdecc::entity::model::DescriptorIndex const _descriptorIndex{ 0u };
	Kind const _kind{ Kind::None };
	bool _hasError{ false };
};

// EntityModelNode
class EntityModelNodeItem : public NodeItem
{
public:
	EntityModelNodeItem(la::avdecc::controller::model::EntityModelNode const* node, QString const& name)
		: NodeItem{ Kind::EntityModelNode, node->descriptorType, node->descriptorIndex, name }
	{
	}
};

// VirtualNodeItem
class VirtualNodeItem : public NodeItem
{
public:
	enum class VirtualDescriptorType
	{
		Unknown,
		RedundantStreamInput,
		RedundantStreamOutput,
	};

	VirtualNodeItem(la::avdecc::controller::model::VirtualNode const* node, QString const& name)
		: NodeItem{ Kind::VirtualNode, node->descriptorType, static_cast<la::avdecc::entity::model::DescriptorIndex>(node->virtualIndex), name }
	{
	}

	VirtualDescriptorType virtualDescriptorType() const
	{
		switch (descriptorType())
		{
			case la::avdecc::entity::model::DescriptorType::StreamInput:
				return VirtualDescriptorType::RedundantStreamInput;
			case la::avdecc::entity::model::DescriptorType::StreamOutput:
				return VirtualDescriptorType::RedundantStreamOutput;
			default:
				return VirtualDescriptorType::Unknown;
		}
	}

protected:
	virtual void updateHasError() override
	{
		auto hasError = false;
		for (auto i = 0; i < childCount(); ++i)
		{
			auto const* item = static_cast<NodeItem const*>(child(i));
			if (item && item->hasError())
			{
				hasError = true;
				break;
			}
		}

		setHasError(hasError);
	}
};

class ControlledEntityTreeWidgetPrivate : public QObject, public la::avdecc::controller::model::EntityModelVisitor
{
public:
	struct NodeIdentifier
	{
		la::avdecc::entity::model::DescriptorType type{ la::avdecc::entity::model::DescriptorType::Invalid };
		la::avdecc::entity::model::DescriptorIndex index{ 0u };
		Kind kind{ Kind::None };

		constexpr bool operator==(NodeIdentifier const& other) const noexcept
		{
			return type == other.type && index == other.index && kind == other.kind;
		}

		/** Hash functor to be used for std::hash */
		struct hash
		{
			std::size_t operator()(NodeIdentifier const& id) const
			{
				// We use 16 bits for the descriptor type, 15 bits for the index and 1 for the kind
				return (static_cast<std::size_t>(id.type) << 16) | (static_cast<std::size_t>(id.index & 0x7fff) << 1) | static_cast<std::size_t>(id.kind);
			}
		};
	};

	using NodeIdentifierSet = std::unordered_set<NodeIdentifier, NodeIdentifier::hash>;

	ControlledEntityTreeWidgetPrivate(ControlledEntityTreeWidget* q)
		: q_ptr(q)
	{
		auto& controllerManager = avdecc::ControllerManager::getInstance();

		connect(&controllerManager, &avdecc::ControllerManager::controllerOffline, this, &ControlledEntityTreeWidgetPrivate::controllerOffline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOnline, this, &ControlledEntityTreeWidgetPrivate::entityOnline);
		connect(&controllerManager, &avdecc::ControllerManager::entityOffline, this, &ControlledEntityTreeWidgetPrivate::entityOffline);
		connect(&controllerManager, &avdecc::ControllerManager::streamInputErrorCounterChanged, this, &ControlledEntityTreeWidgetPrivate::streamInputErrorCounterChanged);
	}

	Q_SLOT void controllerOffline()
	{
		Q_Q(ControlledEntityTreeWidget);

		q->setControlledEntityID(la::avdecc::UniqueIdentifier{});
		q->clearSelection();
	}

	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_controlledEntityID != entityID)
		{
			return;
		}

		loadCurrentControlledEntity();
	}

	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		// The current entity went offline, clear everything
		if (_controlledEntityID == entityID)
		{
			Q_Q(ControlledEntityTreeWidget);
			q->clearSelection();
		}
	}

	Q_SLOT void streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::StreamInputCounterValidFlags const flags)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ la::avdecc::entity::model::DescriptorType::StreamInput, descriptorIndex }))
		{
			item->setHasError(!flags.empty());
		}
	}

	void saveUserTreeWidgetState()
	{
		// Build expanded state
		auto currentNode = NodeIdentifier{};
		auto expandedNodes = NodeIdentifierSet{};

		Q_Q(ControlledEntityTreeWidget);
		for (auto const& [id, item] : _identifierToNodeItem)
		{
			if (item == q->currentItem())
			{
				currentNode = id;
			}

			// Put only the expanded nodes in the set
			if (q->isItemExpanded(item))
			{
				expandedNodes.insert(id);
			}
		}

		if (_controlledEntityID.getValue() == 0x001B92FFFE01B930)
		{
			_userTreeWidgetStates[la::avdecc::UniqueIdentifier{ 0x001B92FFFF01B930 }] = { currentNode, expandedNodes };
		}
		if (_controlledEntityID.getValue() == 0x001B92FFFF01B930)
		{
			_userTreeWidgetStates[la::avdecc::UniqueIdentifier{ 0x001B92FFFE01B930 }] = { currentNode, expandedNodes };
		}

		// Save expanded state for previous EntityID
		_userTreeWidgetStates[_controlledEntityID] = { currentNode, std::move(expandedNodes) };
	}

	void restoreUserTreeWidgetState()
	{
		Q_Q(ControlledEntityTreeWidget);

		auto nodeSelected = false;
		auto const it = _userTreeWidgetStates.find(_controlledEntityID);
		if (it != std::end(_userTreeWidgetStates))
		{
			auto const& userTreeWidgetState = it->second;
			for (auto const& id : userTreeWidgetState.expandedNodes)
			{
				if (auto* nodeItem = findItem(id))
				{
					nodeItem->setExpanded(true);
				}
			}

			if (auto* selectedItem = findItem(userTreeWidgetState.currentNode))
			{
				auto const index = q->indexFromItem(selectedItem);
				q->setCurrentIndex(index);
				nodeSelected = true;
			}
		}

		// First time we see this entity or model changed
		if (!nodeSelected)
		{
			// Select the first node, which is always Entity Descriptor
			q->setCurrentIndex(q->model()->index(0, 0));
		}
	}

	void loadCurrentControlledEntity()
	{
		Q_Q(ControlledEntityTreeWidget);

		q->clear();
		_identifierToNodeItem.clear();

		if (!_controlledEntityID)
		{
			return;
		}

		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_controlledEntityID);

		if (controlledEntity)
		{
			controlledEntity->accept(this);
		}

		// Restore expanded state for new EntityID
		restoreUserTreeWidgetState();
	}

	void setControlledEntityID(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_controlledEntityID == entityID)
		{
			return;
		}

		Q_Q(ControlledEntityTreeWidget);

		if (_controlledEntityID)
		{
			saveUserTreeWidgetState();
		}

		_controlledEntityID = entityID;

		loadCurrentControlledEntity();
	}

	la::avdecc::UniqueIdentifier controlledEntityID() const
	{
		return _controlledEntityID;
	}

	NodeItem* findItem(NodeIdentifier const& nodeIdentifier) const
	{
		auto const it = _identifierToNodeItem.find(nodeIdentifier);
		return it != std::end(_identifierToNodeItem) ? it->second : nullptr;
	}

	NodeIdentifier findNodeIdentifier(NodeItem const* item) const
	{
		auto const it = std::find_if(std::begin(_identifierToNodeItem), std::end(_identifierToNodeItem),
			[item](auto const& kv)
			{
				return kv.second == item;
			});
		assert(it != std::end(_identifierToNodeItem));
		return it->first;
	}

	void customContextMenuRequested(QPoint const& pos)
	{
		Q_Q(ControlledEntityTreeWidget);

		auto const* item = static_cast<NodeItem*>(q->itemAt(pos));
		if (!item)
		{
			return;
		}

		auto const nodeIdentifier = findNodeIdentifier(item);
		if (nodeIdentifier.type == la::avdecc::entity::model::DescriptorType::Configuration)
		{
			auto const& anyNode = item->data(0, Qt::UserRole).value<AnyNode>().getNode();
			auto const* configurationNode = std::any_cast<la::avdecc::controller::model::ConfigurationNode const*>(anyNode);

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
	NodeIdentifier makeIdentifier(la::avdecc::controller::model::Node const* node, Kind const kind) const noexcept
	{
		return { node->descriptorType, static_cast<la::avdecc::controller::model::EntityModelNode const*>(node)->descriptorIndex, kind };
	}

	template<typename T>
	NodeItem* addItem(la::avdecc::controller::model::Node const* parent, Kind const parentKind, T const* node, QString const& name) noexcept
	{
		NodeItem* item = nullptr;

		if constexpr (std::is_base_of_v<la::avdecc::controller::model::EntityModelNode, T>)
		{
			item = new EntityModelNodeItem{ static_cast<la::avdecc::controller::model::EntityModelNode const*>(node), name };
			_identifierToNodeItem.insert({ makeIdentifier(node, Kind::EntityModelNode), item });
		}
		else if constexpr (std::is_base_of_v<la::avdecc::controller::model::VirtualNode, T>)
		{
			item = new VirtualNodeItem{ static_cast<la::avdecc::controller::model::VirtualNode const*>(node), name };
			_identifierToNodeItem.insert({ makeIdentifier(node, Kind::VirtualNode), item });
		}
		else
		{
			AVDECC_ASSERT(false, "if constexpr not handled");
		}

		// Store the node inside the item
		auto const anyNode = AnyNode(node);
		item->setData(0, Qt::UserRole, QVariant::fromValue(anyNode));

		if (parent)
		{
			auto* parentItem = findItem(makeIdentifier(parent, parentKind));
			assert(parentItem);
			parentItem->addChild(item);
		}
		else
		{
			Q_Q(ControlledEntityTreeWidget);
			q->addTopLevelItem(item);
		}

		return item;
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::EntityNode const& node) noexcept override
	{
		auto genName = [type = node.descriptorType](QString const& name)
		{
			return QString("%1: %2").arg(avdecc::helper::descriptorTypeToString(type), name);
		};
		auto* item = addItem(nullptr, Kind::EntityModelNode, &node, genName(node.dynamicModel->entityName.data()));

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::entityNameChanged, item,
			[genName, item](la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
			{
				auto name = genName(entityName);
				item->setData(0, Qt::DisplayRole, name);
			});

		Q_Q(ControlledEntityTreeWidget);
		q->setItemExpanded(item, true);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::EntityNode const* const parent, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		auto genName = [type = node.descriptorType, index = node.descriptorIndex](QString const& name)
		{
			return QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(type), QString::number(index), name);
		};
		auto* item = addItem(parent, Kind::EntityModelNode, &node, genName(avdecc::helper::configurationName(controlledEntity, node)));

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::configurationNameChanged, item,
			[this, genName, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& /*configurationName*/)
			{
				if (entityID == _controlledEntityID && configurationIndex == node.descriptorIndex)
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
	void updateName(NodeItem* item, Node const& node, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex)
	{
		if (entityID == _controlledEntityID && descriptorType == node.descriptorType && descriptorIndex == node.descriptorIndex)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			auto controlledEntity = manager.getControlledEntity(entityID);

			// Filter configuration, we currently expand nodes only for current configuration
			if (controlledEntity && configurationIndex == controlledEntity->getEntityNode().dynamicModel->currentConfiguration)
			{
				auto name = genName(controlledEntity.get(), node);
				item->setData(0, Qt::DisplayRole, name);
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::audioUnitNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& /*audioUnitName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioUnit, audioUnitIndex);
			});
	}

	void processStreamInputNode(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, Kind const parentKind, la::avdecc::controller::model::StreamInputNode const& node) noexcept
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, parentKind, &node, name);

		auto& manager = avdecc::ControllerManager::getInstance();
		auto const flags = manager.getStreamInputErrorCounterFlags(_controlledEntityID, node.descriptorIndex);
		item->setHasError(!flags.empty());

		connect(&manager, &avdecc::ControllerManager::streamNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& /*streamName*/)
			{
				updateName(item, node, entityID, configurationIndex, descriptorType, streamIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		// Only show non-redundant streams when the parent is Configuration
		if (!node.isRedundant)
		{
			processStreamInputNode(controlledEntity, parent, Kind::EntityModelNode, node);
		}
	}

	void processStreamOutputNode(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::Node const* const parent, Kind const parentKind, la::avdecc::controller::model::StreamOutputNode const& node) noexcept
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, parentKind, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::streamNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& /*streamName*/)
			{
				updateName(item, node, entityID, configurationIndex, descriptorType, streamIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		// Only show non-redundant streams when the parent is Configuration
		if (!node.isRedundant)
		{
			processStreamOutputNode(controlledEntity, parent, Kind::EntityModelNode, node);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::avbInterfaceNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& /*avbInterfaceName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::LocaleNode const& node) noexcept override
	{
		auto const name = QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex), node.staticModel->localeID.data());
		addItem(parent, Kind::EntityModelNode, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::LocaleNode const* const parent, la::avdecc::controller::model::StringsNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, Kind::EntityModelNode, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::AudioUnitNode const* const parent, la::avdecc::controller::model::StreamPortNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, Kind::EntityModelNode, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandGrandParent*/, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const parent, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::audioClusterNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& /*audioClusterName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioCluster, audioClusterIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandGrandParent*/, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const parent, la::avdecc::controller::model::AudioMapNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(parent, Kind::EntityModelNode, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::clockDomainNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& /*clockDomainName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::ClockDomainNode const* const parent, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::clockSourceNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& /*clockSourceName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockSource, clockSourceIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override
	{
		auto const name = genName(controlledEntity, node);
		auto* item = addItem(parent, Kind::EntityModelNode, &node, name);

		connect(&avdecc::ControllerManager::getInstance(), &avdecc::ControllerManager::memoryObjectNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& /*memoryObjectName*/)
			{
				updateName(item, node, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::MemoryObject, memoryObjectIndex);
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::RedundantStreamNode const& node) noexcept override
	{
		auto const name = QString("REDUNDANT_%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.virtualIndex));
		addItem(parent, Kind::EntityModelNode, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::RedundantStreamNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		processStreamInputNode(controlledEntity, parent, Kind::VirtualNode, node);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const /*grandParent*/, la::avdecc::controller::model::RedundantStreamNode const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		processStreamOutputNode(controlledEntity, parent, Kind::VirtualNode, node);
	}

private:
	ControlledEntityTreeWidget* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControlledEntityTreeWidget);

	la::avdecc::UniqueIdentifier _controlledEntityID{};

	// Quick access node item by node identifier
	std::unordered_map<NodeIdentifier, NodeItem*, NodeIdentifier::hash> _identifierToNodeItem;

	struct UserTreeWidgetState
	{
		NodeIdentifier currentNode{};
		NodeIdentifierSet expandedNodes{};
	};

	// Global map that stores UserTreeWidgetState by entity ID (Stored in the class so each instance keeps its own cache)
	std::unordered_map<la::avdecc::UniqueIdentifier, UserTreeWidgetState, la::avdecc::UniqueIdentifier::hash> _userTreeWidgetStates{};
};

ControlledEntityTreeWidget::ControlledEntityTreeWidget(QWidget* parent)
	: QTreeWidget(parent)
	, d_ptr(new ControlledEntityTreeWidgetPrivate(this))
{
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	header()->hide();

	setContextMenuPolicy(Qt::CustomContextMenu);

	connect(this, &QTreeWidget::customContextMenuRequested, this,
		[this](QPoint const& pos)
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
