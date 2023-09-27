/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "avdecc/helper.hpp"
#include "settingsManager/settings.hpp"
#include "nodeVisitor.hpp"
#include "entityInspector.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/widgetModelsLibrary/qtUserRoles.hpp>

#include <QHeaderView>
#include <QMenu>

#include <unordered_set>

// Base node item
class NodeItem : public QObject, public QTreeWidgetItem
{
	using QObject::parent;

public:
	enum class ErrorBit
	{
		// Entity Level
		EntityStatistics = 1u << 0,
		EntityRedundancyWarning = 1u << 1,
		// StreamInput Level
		StreamInputCounter = 1u << 2,
		StreamInputLatency = 1u << 3,
		// Control Level
		ControlValueOutOfBounds = 1u << 4,
	};
	using ErrorBits = la::avdecc::utils::EnumBitfield<ErrorBit>;

	bool isVirtual() const noexcept
	{
		return _isVirtual;
	}

	la::avdecc::entity::model::DescriptorType descriptorType() const
	{
		return _descriptorType;
	}

	la::avdecc::entity::model::DescriptorIndex descriptorIndex() const
	{
		return _descriptorIndex;
	}

	void setErrorBit(ErrorBit const errorBit, bool const isError)
	{
		if (isError)
		{
			_errorBits.set(errorBit);
		}
		else
		{
			_errorBits.reset(errorBit);
		}

		auto const inError = hasError();
		setData(0, la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ErrorRole), inError);

		// Also update the parent node
		if (auto* parent = static_cast<NodeItem*>(QTreeWidgetItem::parent()))
		{
			if (parent->isVirtual())
			{
				parent->updateHasError();
			}
		}
	}

	bool hasError() const noexcept
	{
		return !_errorBits.empty();
	}

	void setErrorBits(ErrorBits const errorBits) noexcept
	{
		_errorBits = errorBits;

		auto const inError = hasError();
		setData(0, la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ErrorRole), inError);
	}

	ErrorBits getErrorBits() const noexcept
	{
		return _errorBits;
	}

protected:
	NodeItem(bool const isVirtual, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QString const& name)
		: _isVirtual{ isVirtual }
		, _descriptorType{ descriptorType }
		, _descriptorIndex{ descriptorIndex }
	{
		setData(0, Qt::DisplayRole, name);
	}

	virtual void updateHasError() {}

private:
	bool const _isVirtual{ false };
	la::avdecc::entity::model::DescriptorType const _descriptorType{ la::avdecc::entity::model::DescriptorType::Invalid };
	la::avdecc::entity::model::DescriptorIndex const _descriptorIndex{ 0u };
	ErrorBits _errorBits{};
};

// EntityModelNode
class EntityModelNodeItem : public NodeItem
{
public:
	EntityModelNodeItem(la::avdecc::controller::model::EntityModelNode const* node, QString const& name)
		: NodeItem{ false, node->descriptorType, node->descriptorIndex, name }
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
		: NodeItem{ true, node->descriptorType, static_cast<la::avdecc::entity::model::DescriptorIndex>(node->virtualIndex), name }
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
		auto errorBits = ErrorBits{};
		for (auto i = 0; i < childCount(); ++i)
		{
			auto const* item = static_cast<NodeItem const*>(child(i));
			if (item)
			{
				errorBits |= item->getErrorBits();
			}
		}

		setErrorBits(errorBits);
	}
};

class ControlledEntityTreeWidgetPrivate : public QObject, public la::avdecc::controller::model::EntityModelVisitor, private settings::SettingsManager::Observer
{
public:
	struct NodeIdentifier
	{
		la::avdecc::entity::model::ConfigurationIndex configurationIndex{ 0u };
		la::avdecc::entity::model::DescriptorType type{ la::avdecc::entity::model::DescriptorType::Invalid };
		la::avdecc::entity::model::DescriptorIndex index{ 0u };
		bool isVirtual{ false };

		NodeIdentifier() noexcept = default;

		NodeIdentifier(la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const type, la::avdecc::entity::model::DescriptorIndex const index) noexcept
			: configurationIndex(configurationIndex)
			, type(type)
			, index(index)
			, isVirtual(false)
		{
		}

		NodeIdentifier(la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const type, la::avdecc::controller::model::VirtualIndex const index) noexcept
			: configurationIndex(configurationIndex)
			, type(type)
			, index(index)
			, isVirtual(true)
		{
		}

		constexpr bool operator==(NodeIdentifier const& other) const noexcept
		{
			return configurationIndex == other.configurationIndex && type == other.type && index == other.index && isVirtual == other.isVirtual;
		}

		/** Hash functor to be used for std::hash */
		struct hash
		{
			std::size_t operator()(NodeIdentifier const& id) const
			{
				// Bit 21-31 for ConfigurationIndex (11 bits)
				// Bit 7-20 for DescriptorIndex (14 bits)
				// Bit 1-6 for DescriptorType (6 bits)
				// Bit 0 for Kind (1 bit)
				return ((static_cast<std::size_t>(id.configurationIndex) & 0x7fff) << 21) | ((static_cast<std::size_t>(id.index) & 0x3fff) << 7) | ((static_cast<std::size_t>(id.type) & 0x3f) << 1) | (static_cast<std::size_t>(id.isVirtual) & 0x1);

				//return (static_cast<std::size_t>(id.type) << 16) | (static_cast<std::size_t>(id.index & 0x7fff) << 1) | static_cast<std::size_t>(id.isVirtual);
			}
		};
	};

	using NodeIdentifierSet = std::unordered_set<NodeIdentifier, NodeIdentifier::hash>;

	ControlledEntityTreeWidgetPrivate(ControlledEntityTreeWidget* q)
		: q_ptr(q)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::controllerOffline, this, &ControlledEntityTreeWidgetPrivate::controllerOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOnline, this, &ControlledEntityTreeWidgetPrivate::entityOnline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &ControlledEntityTreeWidgetPrivate::entityOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::streamInputErrorCounterChanged, this, &ControlledEntityTreeWidgetPrivate::streamInputErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::statisticsErrorCounterChanged, this, &ControlledEntityTreeWidgetPrivate::statisticsErrorCounterChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::redundancyWarningChanged, this, &ControlledEntityTreeWidgetPrivate::redundancyWarningChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::streamInputLatencyErrorChanged, this, &ControlledEntityTreeWidgetPrivate::handleStreamInputLatencyErrorChanged);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::controlCurrentValueOutOfBoundsChanged, this, &ControlledEntityTreeWidgetPrivate::handleControlCurrentValueOutOfBoundsChanged);

		// Configure settings observers
		auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
		settings->registerSettingObserver(settings::Controller_FullStaticModelEnabled.name, this);
	}

	~ControlledEntityTreeWidgetPrivate()
	{
		// Remove settings observers
		auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
		settings->unregisterSettingObserver(settings::Controller_FullStaticModelEnabled.name, this);
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

	Q_SLOT void streamInputErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, hive::modelsLibrary::ControllerManager::StreamInputErrorCounters const& errorCounters)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ _currentConfigurationIndex, la::avdecc::entity::model::DescriptorType::StreamInput, descriptorIndex }))
		{
			item->setErrorBit(NodeItem::ErrorBit::StreamInputCounter, !errorCounters.empty());
		}
	}

	Q_SLOT void statisticsErrorCounterChanged(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::StatisticsErrorCounters const& errorCounters)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ _currentConfigurationIndex, la::avdecc::entity::model::DescriptorType::Entity, la::avdecc::entity::model::DescriptorIndex{ 0u } }))
		{
			item->setErrorBit(NodeItem::ErrorBit::EntityStatistics, !errorCounters.empty());
		}
	}

	Q_SLOT void redundancyWarningChanged(la::avdecc::UniqueIdentifier const entityID, bool const isRedundancyWarning)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ _currentConfigurationIndex, la::avdecc::entity::model::DescriptorType::Entity, la::avdecc::entity::model::DescriptorIndex{ 0u } }))
		{
			item->setErrorBit(NodeItem::ErrorBit::EntityRedundancyWarning, isRedundancyWarning);
		}
	}

	Q_SLOT void handleStreamInputLatencyErrorChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, bool const isLatencyError)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ _currentConfigurationIndex, la::avdecc::entity::model::DescriptorType::StreamInput, streamIndex }))
		{
			item->setErrorBit(NodeItem::ErrorBit::StreamInputLatency, isLatencyError);
		}
	}

	Q_SLOT void handleControlCurrentValueOutOfBoundsChanged(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, bool const isValueOutOfBounds)
	{
		if (entityID != _controlledEntityID)
		{
			return;
		}

		if (auto* item = findItem({ _currentConfigurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex }))
		{
			item->setErrorBit(NodeItem::ErrorBit::ControlValueOutOfBounds, isValueOutOfBounds);
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
			if (item->isExpanded())
			{
				expandedNodes.insert(id);
			}
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

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(_controlledEntityID);

		if (controlledEntity)
		{
			controlledEntity->accept(this, _displayFullModel);
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

	void showSetDescriptorAsCurrentMenu(QPoint const& pos, NodeItem const* /*item*/, QString const& actionText, bool const isEnabled, std::function<void()> const& onActionTriggered)
	{
		Q_Q(ControlledEntityTreeWidget);

		QMenu menu;

		auto* setAsCurrentAction = menu.addAction(actionText);
		setAsCurrentAction->setEnabled(isEnabled);

		menu.addSeparator();
		menu.addAction("Cancel");

		if (auto* action = menu.exec(q->mapToGlobal(pos)))
		{
			if (action == setAsCurrentAction)
			{
				onActionTriggered();
			}
		}
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
		switch (nodeIdentifier.type)
		{
			case la::avdecc::entity::model::DescriptorType::Configuration:
			{
				auto const& anyNode = item->data(0, la::avdecc::utils::to_integral(EntityInspector::RoleInfo::NodeType)).value<AnyNode>().getNode();
				auto const* configurationNode = std::any_cast<la::avdecc::controller::model::ConfigurationNode const*>(anyNode);
				auto const isEnabled = !configurationNode->dynamicModel.isActiveConfiguration;

				showSetDescriptorAsCurrentMenu(pos, item, "Set As Current Configuration", isEnabled,
					[this, configurationIndex = nodeIdentifier.index]()
					{
						hive::modelsLibrary::ControllerManager::getInstance().setConfiguration(_controlledEntityID, configurationIndex);
					});
				break;
			}
			case la::avdecc::entity::model::DescriptorType::ClockSource:
			{
				if (auto* parentItem = static_cast<NodeItem*>(item->QTreeWidgetItem::parent()))
				{
					auto const parentNodeIdentifier = findNodeIdentifier(parentItem);
					if (parentNodeIdentifier.type == la::avdecc::entity::model::DescriptorType::ClockDomain)
					{
						auto const& anyNode = parentItem->data(0, la::avdecc::utils::to_integral(EntityInspector::RoleInfo::NodeType)).value<AnyNode>().getNode();
						auto const* clockDomainNode = std::any_cast<la::avdecc::controller::model::ClockDomainNode const*>(anyNode);

						auto const clockDomainIndex = clockDomainNode->descriptorIndex;
						auto const clockSourceIndex = nodeIdentifier.index;
						auto const isEnabled = clockDomainNode->dynamicModel.clockSourceIndex != clockSourceIndex;

						showSetDescriptorAsCurrentMenu(pos, item, "Set As Current Clock Source", isEnabled,
							[this, clockDomainIndex, clockSourceIndex]()
							{
								hive::modelsLibrary::ControllerManager::getInstance().setClockSource(_controlledEntityID, clockDomainIndex, clockSourceIndex);
							});
					}
				}
				break;
			}
			default:
				break;
		}
	}

private:
	// settings::SettingsManager::Observer overrides
	virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept override
	{
		if (name == settings::Controller_FullStaticModelEnabled.name)
		{
			_displayFullModel = value.toBool();
		}
	}

	template<typename NodeType>
	NodeIdentifier makeIdentifier([[maybe_unused]] la::avdecc::entity::model::ConfigurationIndex const configurationIndex, [[maybe_unused]] NodeType const* node) const noexcept
	{
		if constexpr (std::is_base_of_v<la::avdecc::controller::model::EntityModelNode, NodeType>)
		{
			return { configurationIndex, node->descriptorType, static_cast<la::avdecc::controller::model::EntityModelNode const*>(node)->descriptorIndex };
		}
		else if constexpr (std::is_base_of_v<la::avdecc::controller::model::VirtualNode, NodeType>)
		{
			return { configurationIndex, node->descriptorType, static_cast<la::avdecc::controller::model::VirtualNode const*>(node)->virtualIndex };
		}
		else
		{
			AVDECC_ASSERT(false, "if constexpr not handled");
			return {};
		}
	}

	template<typename ParentNodeType, typename NodeType>
	NodeItem* addItem(la::avdecc::entity::model::ConfigurationIndex const configurationIndex, ParentNodeType const* parent, NodeType const* node, QString const& name) noexcept
	{
		auto isActiveConfiguration = _currentConfigurationIndex == configurationIndex;
		// Special case for EntityNode, which always is ActiveConfiguration
		if constexpr (std::is_same_v<la::avdecc::controller::model::EntityNode, NodeType>)
		{
			isActiveConfiguration = true;
		}

		auto parentConfigurationIndex = configurationIndex;
		// Special case for ConfigurationNode, which parent uses ConfigurationIndex 0
		if constexpr (std::is_same_v<la::avdecc::controller::model::ConfigurationNode, NodeType>)
		{
			parentConfigurationIndex = la::avdecc::entity::model::ConfigurationIndex{ 0u };
		}

		NodeItem* item = nullptr;

		if constexpr (std::is_base_of_v<la::avdecc::controller::model::EntityModelNode, NodeType>)
		{
			item = new EntityModelNodeItem{ static_cast<la::avdecc::controller::model::EntityModelNode const*>(node), name };
			_identifierToNodeItem.insert({ makeIdentifier(configurationIndex, node), item });
		}
		else if constexpr (std::is_base_of_v<la::avdecc::controller::model::VirtualNode, NodeType>)
		{
			item = new VirtualNodeItem{ static_cast<la::avdecc::controller::model::VirtualNode const*>(node), name };
			_identifierToNodeItem.insert({ makeIdentifier(configurationIndex, node), item });
		}
		else
		{
			AVDECC_ASSERT(false, "if constexpr not handled");
		}

		// Store the node inside the item
		auto const anyNode = AnyNode(node);
		item->setData(0, la::avdecc::utils::to_integral(EntityInspector::RoleInfo::NodeType), QVariant::fromValue(anyNode));
		item->setData(0, la::avdecc::utils::to_integral(EntityInspector::RoleInfo::IsActiveConfiguration), isActiveConfiguration);

		if (parent)
		{
			auto* parentItem = findItem(makeIdentifier(parentConfigurationIndex, parent));
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

	QString genDescriptorName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::LocalizedStringReference const& localizedDescription, QString const& name)
	{
		auto objName = hive::modelsLibrary::helper::localizedString(*controlledEntity, configurationIndex, localizedDescription);

		// Only use name for current configuration (and all ConfigurationDescriptors)
		if ((descriptorType == la::avdecc::entity::model::DescriptorType::Configuration || configurationIndex == controlledEntity->getEntityNode().dynamicModel.currentConfiguration) && !name.isEmpty())
		{
			objName = name;
		}

		return QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(descriptorType), QString::number(descriptorIndex), objName);
	}
	QString genDescriptorName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::LocalizedStringReference const& localizedDescription, la::avdecc::entity::model::AvdeccFixedString const& name)
	{
		auto qName = QString::fromStdString(name.data());
		return genDescriptorName(controlledEntity, configurationIndex, descriptorType, descriptorIndex, localizedDescription, qName);
	}
	void updateName(NodeItem* item, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::model::LocalizedStringReference const& localizedDescription, QString const& name)
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		// Filter configuration, we currently expand nodes only for current configuration
		if (controlledEntity && configurationIndex == controlledEntity->getEntityNode().dynamicModel.currentConfiguration)
		{
			auto descName = genDescriptorName(controlledEntity.get(), configurationIndex, descriptorType, descriptorIndex, localizedDescription, name);
			item->setData(0, Qt::DisplayRole, descName);
		}
	}

	// la::avdecc::controller::model::EntityModelVisitor overrides
	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::EntityNode const& node) noexcept override
	{
		_currentConfigurationIndex = node.dynamicModel.currentConfiguration;

		auto genName = [type = node.descriptorType](QString const& name)
		{
			return QString("%1: %2").arg(avdecc::helper::descriptorTypeToString(type), name);
		};
		// Use Index 0 as ConfigurationIndex for the Entity Descriptor
		auto* item = addItem<la::avdecc::controller::model::Node const*>(la::avdecc::entity::model::ConfigurationIndex{ 0u }, nullptr, &node, genName(node.dynamicModel.entityName.data()));

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();

		// Init ErrorBits
		{
			// Statistics
			auto const errorCounters = manager.getStatisticsCounters(_controlledEntityID);
			item->setErrorBit(NodeItem::ErrorBit::EntityStatistics, !errorCounters.empty());

			// Redundancy Warning
			auto const redundancyWarning = manager.getDiagnostics(_controlledEntityID).redundancyWarning;
			item->setErrorBit(NodeItem::ErrorBit::EntityRedundancyWarning, redundancyWarning);
		}

		connect(&manager, &hive::modelsLibrary::ControllerManager::entityNameChanged, item,
			[genName, item](la::avdecc::UniqueIdentifier const /*entityID*/, QString const& entityName)
			{
				auto name = genName(entityName);
				item->setData(0, Qt::DisplayRole, name);
			});

		Q_Q(ControlledEntityTreeWidget);
		item->setExpanded(true);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::EntityNode const* const parent, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		auto descName = genDescriptorName(controlledEntity, node.descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(node.descriptorIndex, parent, &node, descName);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::configurationNameChanged, item,
			[this, item, node](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName)
			{
				if (entityID == _controlledEntityID && configurationIndex == node.descriptorIndex)
				{
					auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
					auto controlledEntity = manager.getControlledEntity(entityID);
					if (controlledEntity)
					{
						auto descName = genDescriptorName(controlledEntity.get(), configurationIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, configurationName);
						item->setData(0, Qt::DisplayRole, descName);
					}
				}
			});

		if (node.dynamicModel.isActiveConfiguration)
		{
			// Set the ActiveRole
			item->setData(0, la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ActiveRole), true);

			Q_Q(ControlledEntityTreeWidget);
			item->setExpanded(true);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::audioUnitNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& audioUnitName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::AudioUnit == node.descriptorType && audioUnitIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioUnit, audioUnitIndex, node.staticModel.localizedDescription, audioUnitName);
				}
			});
	}

	template<typename ParentNodeType>
	void processStreamInputNode(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, ParentNodeType const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept
	{
		auto const name = genDescriptorName(controlledEntity, configurationIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(configurationIndex, parent, &node, name);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		// Init ErrorBits
		{
			// StreamInput Counters
			auto const errorCounters = manager.getStreamInputErrorCounters(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::StreamInputCounter, !errorCounters.empty());

			// StreamInput Latency
			auto const errorLatency = manager.getStreamInputLatencyError(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::StreamInputLatency, errorLatency);
		}

		connect(&manager, &hive::modelsLibrary::ControllerManager::streamNameChanged, item,
			[this, item, node, confIndex = configurationIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && descriptorType == node.descriptorType && streamIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, descriptorType, streamIndex, node.staticModel.localizedDescription, streamName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		// Only show non-redundant streams when the parent is Configuration
		if (!node.isRedundant)
		{
			processStreamInputNode(controlledEntity, parent->descriptorIndex, parent, node);
		}
	}

	template<typename ParentNodeType>
	void processStreamOutputNode(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, ParentNodeType const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept
	{
		auto const name = genDescriptorName(controlledEntity, configurationIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(configurationIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::streamNameChanged, item,
			[this, item, node, confIndex = configurationIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && descriptorType == node.descriptorType && streamIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, descriptorType, streamIndex, node.staticModel.localizedDescription, streamName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		// Only show non-redundant streams when the parent is Configuration
		if (!node.isRedundant)
		{
			processStreamOutputNode(controlledEntity, parent->descriptorIndex, parent, node);
		}
	}

	template<typename NodeType>
	void processJackNode(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, NodeType const& node) noexcept
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::jackNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::JackIndex const jackIndex, QString const& jackName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && descriptorType == node.descriptorType && jackIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, descriptorType, jackIndex, node.staticModel.localizedDescription, jackName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::JackInputNode const& node) noexcept override
	{
		processJackNode(controlledEntity, parent, node);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::JackOutputNode const& node) noexcept override
	{
		processJackNode(controlledEntity, parent, node);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::JackNode const* const parent, la::avdecc::controller::model::ControlNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, grandParent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(grandParent->descriptorIndex, parent, &node, name);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		// Init ErrorBits
		{
			// ControlValue OutOfBounds
			auto const valueOutOfBounds = manager.getControlValueOutOfBounds(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::ControlValueOutOfBounds, valueOutOfBounds);
		}

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlNameChanged, item,
			[this, item, node, confIndex = grandParent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::Control == node.descriptorType && controlIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex, node.staticModel.localizedDescription, controlName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::avbInterfaceNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& avbInterfaceName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::AvbInterface == node.descriptorType && avbInterfaceIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AvbInterface, avbInterfaceIndex, node.staticModel.localizedDescription, avbInterfaceName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const /*parent*/, la::avdecc::controller::model::ClockSourceNode const& /*node*/) noexcept override
	{
		// Ignored, we'll show the ClockSourceNode in the ClockDomainNode
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::memoryObjectNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& memoryObjectName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::MemoryObject == node.descriptorType && memoryObjectIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::MemoryObject, memoryObjectIndex, node.staticModel.localizedDescription, memoryObjectName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::LocaleNode const& node) noexcept override
	{
		auto const name = QString("%1.%2: %3").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex), node.staticModel.localeID.data());
		addItem(parent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::LocaleNode const* const parent, la::avdecc::controller::model::StringsNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(grandParent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::AudioUnitNode const* const parent, la::avdecc::controller::model::StreamPortInputNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(grandParent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::AudioUnitNode const* const parent, la::avdecc::controller::model::StreamPortOutputNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(grandParent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandGrandParent, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const parent, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, grandGrandParent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(grandGrandParent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::audioClusterNameChanged, item,
			[this, item, node, confIndex = grandGrandParent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::AudioCluster == node.descriptorType && audioClusterIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::AudioCluster, audioClusterIndex, node.staticModel.localizedDescription, audioClusterName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const grandGrandParent, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const parent, la::avdecc::controller::model::AudioMapNode const& node) noexcept override
	{
		auto const name = QString("%1.%2").arg(avdecc::helper::descriptorTypeToString(node.descriptorType), QString::number(node.descriptorIndex));
		addItem(grandGrandParent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandGrandParent, la::avdecc::controller::model::AudioUnitNode const* const /*grandParent*/, la::avdecc::controller::model::StreamPortNode const* const parent, la::avdecc::controller::model::ControlNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, grandGrandParent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(grandGrandParent->descriptorIndex, parent, &node, name);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		// Init ErrorBits
		{
			// ControlValue OutOfBounds
			auto const valueOutOfBounds = manager.getControlValueOutOfBounds(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::ControlValueOutOfBounds, valueOutOfBounds);
		}

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlNameChanged, item,
			[this, item, node, confIndex = grandGrandParent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::Control == node.descriptorType && controlIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex, node.staticModel.localizedDescription, controlName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::AudioUnitNode const* const parent, la::avdecc::controller::model::ControlNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, grandParent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(grandParent->descriptorIndex, parent, &node, name);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		// Init ErrorBits
		{
			// ControlValue OutOfBounds
			auto const valueOutOfBounds = manager.getControlValueOutOfBounds(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::ControlValueOutOfBounds, valueOutOfBounds);
		}

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlNameChanged, item,
			[this, item, node, confIndex = grandParent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::Control == node.descriptorType && controlIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex, node.staticModel.localizedDescription, controlName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::ControlNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		// Init ErrorBits
		{
			// ControlValue OutOfBounds
			auto const valueOutOfBounds = manager.getControlValueOutOfBounds(_controlledEntityID, node.descriptorIndex);
			item->setErrorBit(NodeItem::ErrorBit::ControlValueOutOfBounds, valueOutOfBounds);
		}

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::Control == node.descriptorType && controlIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::Control, controlIndex, node.staticModel.localizedDescription, controlName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, parent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(parent->descriptorIndex, parent, &node, name);

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockDomainNameChanged, item,
			[this, item, node, confIndex = parent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& clockDomainName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::ClockDomain == node.descriptorType && clockDomainIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockDomain, clockDomainIndex, node.staticModel.localizedDescription, clockDomainName);
				}
			});
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::ClockDomainNode const* const parent, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override
	{
		auto const name = genDescriptorName(controlledEntity, grandParent->descriptorIndex, node.descriptorType, node.descriptorIndex, node.staticModel.localizedDescription, node.dynamicModel.objectName);
		auto* item = addItem(grandParent->descriptorIndex, parent, &node, name);
		auto const isCurrentConfiguration = grandParent->descriptorIndex == controlledEntity->getEntityNode().dynamicModel.currentConfiguration;

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockSourceNameChanged, item,
			[this, item, node, confIndex = grandParent->descriptorIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& clockSourceName)
			{
				if (entityID == _controlledEntityID && confIndex == configurationIndex && la::avdecc::entity::model::DescriptorType::ClockSource == node.descriptorType && clockSourceIndex == node.descriptorIndex)
				{
					updateName(item, entityID, configurationIndex, la::avdecc::entity::model::DescriptorType::ClockSource, clockSourceIndex, node.staticModel.localizedDescription, clockSourceName);
				}
			});

		connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockSourceChanged, item,
			[this, item, node, parentIndex = parent->descriptorIndex, isCurrentConfiguration](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex)
			{
				if (isCurrentConfiguration && entityID == _controlledEntityID && clockDomainIndex == parentIndex)
				{
					// Set the ActiveRole
					auto const isCurrentClockSource = node.descriptorIndex == clockSourceIndex;
					item->setData(0, la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ActiveRole), isCurrentClockSource);
				}
			});

		if (isCurrentConfiguration)
		{
			// Set the ActiveRole
			auto const isCurrentClockSource = node.descriptorIndex == parent->dynamicModel.clockSourceIndex;
			item->setData(0, la::avdecc::utils::to_integral(hive::widgetModelsLibrary::QtUserRoles::ActiveRole), isCurrentClockSource);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::RedundantStreamInputNode const& node) noexcept override
	{
		auto const name = QString("REDUNDANT_INPUT.%1: %2").arg(QString::number(node.virtualIndex), QString::fromStdString(node.virtualName));
		addItem(parent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, la::avdecc::controller::model::ConfigurationNode const* const parent, la::avdecc::controller::model::RedundantStreamOutputNode const& node) noexcept override
	{
		auto const name = QString("REDUNDANT_OUTPUT.%1: %2").arg(QString::number(node.virtualIndex), QString::fromStdString(node.virtualName));
		addItem(parent->descriptorIndex, parent, &node, name);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::RedundantStreamNode const* const parent, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		processStreamInputNode(controlledEntity, grandParent->descriptorIndex, parent, node);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const* const grandParent, la::avdecc::controller::model::RedundantStreamNode const* const parent, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		processStreamOutputNode(controlledEntity, grandParent->descriptorIndex, parent, node);
	}

private:
	ControlledEntityTreeWidget* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(ControlledEntityTreeWidget);

	la::avdecc::UniqueIdentifier _controlledEntityID{};
	la::avdecc::entity::model::ConfigurationIndex _currentConfigurationIndex{ 0u };
	bool _displayFullModel{ false };

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
