/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "nodeTreeWidget.hpp"
#include "avdecc/hiveLogItems.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/stringValidator.hpp"
#include "nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/controlValuesDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/discoveredInterfacesTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp"
#include "nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp"
#include "counters/entityCountersTreeWidgetItem.hpp"
#include "counters/avbInterfaceCountersTreeWidgetItem.hpp"
#include "counters/clockDomainCountersTreeWidgetItem.hpp"
#include "counters/streamInputCountersTreeWidgetItem.hpp"
#include "counters/streamOutputCountersTreeWidgetItem.hpp"
#include "statistics/entityStatisticsTreeWidgetItem.hpp"
#include "firmwareUploadDialog.hpp"
#include "aecpCommandComboBox.hpp"
#include "aecpCommandTextEntry.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/internals/entityModelControlValuesTraits.hpp>
#include <la/avdecc/logger.hpp>
#include <QtMate/widgets/textEntry.hpp>
#include <QtMate/widgets/comboBox.hpp>
#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/widgetModelsLibrary/entityLogoCache.hpp>
#include <hive/widgetModelsLibrary/painterHelper.hpp>

#include <QListWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>

#include <vector>
#include <utility>
#include <algorithm>

Q_DECLARE_METATYPE(la::avdecc::UniqueIdentifier)

class Label : public QWidget
{
	Q_OBJECT
	static constexpr int size{ 16 };
	static constexpr int halfSize{ size / 2 };

public:
	Label(QWidget* parent = nullptr)
		: QWidget{ parent }
		, _backgroundPixmap{ size, size }
	{
		QColor evenColor{ 0x5E5E5E };
		QColor oddColor{ 0xE5E5E5 };

		evenColor.setAlpha(96);
		oddColor.setAlpha(96);

		_backgroundPixmap.fill(Qt::transparent);

		QPainter painter{ &_backgroundPixmap };
		painter.fillRect(_backgroundPixmap.rect(), evenColor);
		painter.fillRect(QRect{ 0, 0, halfSize, halfSize }, oddColor);
		painter.fillRect(QRect{ halfSize, halfSize, halfSize, halfSize }, oddColor);

		connect(&_downloadButton, &QPushButton::clicked, this, &Label::clicked);

		_layout.addWidget(&_downloadButton);
	}

	Q_SIGNAL void clicked();

	void setImage(QImage const& image)
	{
		_image = image;
		_downloadButton.setVisible(image.isNull());
		repaint();
	}

protected:
	virtual void paintEvent(QPaintEvent* event) override
	{
		if (_image.isNull())
		{
			QWidget::paintEvent(event);
		}
		else
		{
			QPainter painter{ this };
			painter.fillRect(rect(), QBrush{ _backgroundPixmap });
			hive::widgetModelsLibrary::painterHelper::drawCentered(&painter, rect(), _image);
		}
	}

private:
	QHBoxLayout _layout{ this };
	QPushButton _downloadButton{ "Click to Download", this };
	QPixmap _backgroundPixmap;
	QImage _image;
};

class NodeTreeWidgetPrivate : public QObject, public NodeVisitor
{
	Q_OBJECT
public:
	class VisitControlValues
	{
	public:
		virtual void visitStaticControlValues(NodeTreeWidgetPrivate* self, la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, QTreeWidgetItem* const item, la::avdecc::entity::model::ControlNodeStaticModel const& /*staticModel*/, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/) noexcept
		{
			AVDECC_ASSERT(false, "Should not be there. Missing specialization?");
			self->addTextItem(item, "Values", "Not supported (but should be), please report this bug");
		}
		virtual void visitDynamicControlValues(QTreeWidget* const tree, la::avdecc::UniqueIdentifier const /*entityID*/, la::avdecc::entity::model::ControlIndex const /*controlIndex*/, la::avdecc::entity::model::ControlNodeStaticModel const& /*staticModel*/, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/) noexcept
		{
			AVDECC_ASSERT(false, "Should not be there. Missing specialization?");
			auto* dynamicItem = new QTreeWidgetItem(tree);
			dynamicItem->setText(0, "Dynamic Info");
			dynamicItem->setText(1, "Not supported (but should be), please report this bug");
		}
	};
	using VisitControlValuesDispatchTable = std::unordered_map<la::avdecc::entity::model::ControlValueType::Type, std::unique_ptr<VisitControlValues>>;

	NodeTreeWidgetPrivate(NodeTreeWidget* q)
		: q_ptr(q)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::controllerOffline, this, &NodeTreeWidgetPrivate::controllerOffline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOnline, this, &NodeTreeWidgetPrivate::entityOnline);
		connect(&controllerManager, &hive::modelsLibrary::ControllerManager::entityOffline, this, &NodeTreeWidgetPrivate::entityOffline);

		connect(q_ptr, &NodeTreeWidget::itemClicked, this, &NodeTreeWidgetPrivate::itemClicked);
	}

	Q_SLOT void controllerOffline()
	{
		Q_Q(NodeTreeWidget);

		q->setNode(la::avdecc::UniqueIdentifier{}, false, {});
		q->clearSelection();
	}

	Q_SLOT void entityOnline(la::avdecc::UniqueIdentifier const /*entityID*/) {}

	Q_SLOT void entityOffline(la::avdecc::UniqueIdentifier const entityID)
	{
		if (_controlledEntityID == entityID)
		{
			Q_Q(NodeTreeWidget);
			q->setNode(la::avdecc::UniqueIdentifier{}, false, {});
			q->clearSelection();
		}
	}

	Q_SLOT void itemClicked(QTreeWidgetItem* item)
	{
		auto const type = static_cast<NodeTreeWidget::TreeWidgetItemType>(item->type());

		if (type == NodeTreeWidget::TreeWidgetItemType::StreamInputCounter)
		{
			auto* inputStreamItem = static_cast<StreamInputCounterTreeWidgetItem*>(item);
			auto const streamIndex = inputStreamItem->streamIndex();
			auto const flag = inputStreamItem->counterValidFlag();

			hive::modelsLibrary::ControllerManager::getInstance().clearStreamInputCounterValidFlags(_controlledEntityID, streamIndex, flag);
		}
		else if (type == NodeTreeWidget::TreeWidgetItemType::EntityStatistic)
		{
			auto* entityItem = static_cast<EntityStatisticTreeWidgetItem*>(item);
			auto const flag = entityItem->counterFlag();

			hive::modelsLibrary::ControllerManager::getInstance().clearStatisticsCounterValidFlags(_controlledEntityID, flag);
		}
	}

	void setNode(la::avdecc::UniqueIdentifier const entityID, bool const isActiveConfiguration, AnyNode const& node)
	{
		Q_Q(NodeTreeWidget);

		q->clear();

		_controlledEntityID = entityID;

		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		if (controlledEntity && node.getNode().has_value())
		{
			NodeVisitor::accept(this, controlledEntity.get(), isActiveConfiguration, node);
		}

		q->expandAll();
	}

private:
	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::EntityNode const& node) noexcept override
	{
		createIdItem(&node);
		createAccessItem(controlledEntity);

		auto const& entity = *controlledEntity;

		Q_Q(NodeTreeWidget);

		// Names
		{
			auto* nameItem = new QTreeWidgetItem(q);
			nameItem->setText(0, "Names");

			addEditableTextItem(nameItem, "Entity Name", hive::modelsLibrary::helper::entityName(entity), hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityName, la::avdecc::entity::model::DescriptorIndex{ 0u }, {});
			addEditableTextItem(nameItem, "Group Name", hive::modelsLibrary::helper::groupName(entity), hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityGroupName, la::avdecc::entity::model::DescriptorIndex{ 0u }, {});
		}

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const staticModel = node.staticModel;
			auto const* const dynamicModel = node.dynamicModel;

			// Currently, use the getEntity() information, but maybe in the future the controller will have the information in its static/dynamic model
			{
				auto const& e = entity.getEntity();
				auto const talkerCaps = e.getTalkerCapabilities();
				auto const listenerCaps = e.getListenerCapabilities();
				auto const ctrlCaps = e.getControllerCapabilities();

				addTextItem(descriptorItem, "Entity Model ID", hive::modelsLibrary::helper::uniqueIdentifierToString(e.getEntityModelID()));
				addFlagsItem(descriptorItem, "Talker Capabilities", la::avdecc::utils::forceNumeric(talkerCaps.value()), avdecc::helper::capabilitiesToString(talkerCaps));
				addTextItem(descriptorItem, "Talker Max Sources", QString::number(e.getTalkerStreamSources()));
				addFlagsItem(descriptorItem, "Listener Capabilities", la::avdecc::utils::forceNumeric(listenerCaps.value()), avdecc::helper::capabilitiesToString(listenerCaps));
				addTextItem(descriptorItem, "Listener Max Sinks", QString::number(e.getListenerStreamSinks()));
				addFlagsItem(descriptorItem, "Controller Capabilities", la::avdecc::utils::forceNumeric(ctrlCaps.value()), avdecc::helper::capabilitiesToString(ctrlCaps));
				addTextItem(descriptorItem, "Identify Control Index", e.getIdentifyControlIndex() ? QString::number(*e.getIdentifyControlIndex()) : QString("Not Set"));
			}

			addTextItem(descriptorItem, "Vendor Name", hive::modelsLibrary::helper::localizedString(entity, staticModel->vendorNameString));
			addTextItem(descriptorItem, "Model Name", hive::modelsLibrary::helper::localizedString(entity, staticModel->modelNameString));
			addTextItem(descriptorItem, "Firmware Version", dynamicModel->firmwareVersion.data());
			addTextItem(descriptorItem, "Serial Number", dynamicModel->serialNumber.data());

			addTextItem(descriptorItem, "Configuration Count", node.configurations.size());
		}

		// Milan Info
		if (entity.getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			auto* milanInfoItem = new QTreeWidgetItem(q);
			milanInfoItem->setText(0, "Milan Info");

			auto const milanInfo = *entity.getMilanInfo();

			addTextItem(milanInfoItem, "Protocol Version", QString::number(milanInfo.protocolVersion));
			addFlagsItem(milanInfoItem, "Features", la::avdecc::utils::forceNumeric(milanInfo.featuresFlags.value()), avdecc::helper::flagsToString(milanInfo.featuresFlags));
			addTextItem(milanInfoItem, "Certification Version", avdecc::helper::certificationVersionToString(milanInfo.certificationVersion));
		}

		// Discovery information
		{
			auto* discoveredInterfacesItem = new DiscoveredInterfacesTreeWidgetItem(_controlledEntityID, entity.getEntity().getInterfacesInformation(), q);
			discoveredInterfacesItem->setText(0, "Discovered Interfaces");
		}

		// Dynamic model
		{
			auto* dynamicItem = new QTreeWidgetItem(q);
			dynamicItem->setText(0, "Dynamic Info");

			auto const& e = entity.getEntity();
			auto const entityCaps = e.getEntityCapabilities();
			addFlagsItem(dynamicItem, "Entity Capabilities", la::avdecc::utils::forceNumeric(entityCaps.value()), avdecc::helper::capabilitiesToString(entityCaps));
			if (entityCaps.test(la::avdecc::entity::EntityCapability::AssociationIDSupported))
			{
				addEditableTextItem(dynamicItem, "Association ID", e.getAssociationID() ? hive::modelsLibrary::helper::uniqueIdentifierToString(*e.getAssociationID()) : QString(""), hive::modelsLibrary::ControllerManager::AecpCommandType::SetAssociationID, la::avdecc::entity::model::DescriptorIndex{ 0u }, {});
			}
			else
			{
				addTextItem(dynamicItem, "Association ID", e.getAssociationID() ? hive::modelsLibrary::helper::uniqueIdentifierToString(*e.getAssociationID()) : QString("Not Set"));
			}

			auto* currentConfigurationItem = new QTreeWidgetItem(dynamicItem);
			currentConfigurationItem->setText(0, "Current Configuration");

			auto* configurationComboBox = new AecpCommandComboBox<la::avdecc::entity::model::ConfigurationIndex>();
			auto configurations = std::remove_pointer_t<decltype(configurationComboBox)>::Data{};
			for (auto const& it : node.configurations)
			{
				configurations.insert(it.first);
			}
			configurationComboBox->setAllData(configurations,
				[this](auto const& configurationIndex)
				{
					auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
					auto const controlledEntity = manager.getControlledEntity(_controlledEntityID);

					if (controlledEntity)
					{
						try
						{
							auto const& entity = *controlledEntity;
							auto const& configurationNode = entity.getConfigurationNode(configurationIndex);
							return QString::number(configurationIndex) + ": " + hive::modelsLibrary::helper::configurationName(&entity, configurationNode);
						}
						catch (...)
						{
							// Ignore exception
						}
					}
					return QString::number(configurationIndex);
				});

			q->setItemWidget(currentConfigurationItem, 1, configurationComboBox);

			// Send changes
			configurationComboBox->setDataChangedHandler(
				[this, configurationComboBox](auto const& previousConfiguration, auto const& newConfiguration)
				{
					hive::modelsLibrary::ControllerManager::getInstance().setConfiguration(_controlledEntityID, newConfiguration, configurationComboBox->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetConfiguration), configurationComboBox->getResultHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetConfiguration, previousConfiguration));
				});

			// Update now
			configurationComboBox->setCurrentData(node.dynamicModel->currentConfiguration);
		}

		// Counters (if supported by the entity)
		if (node.dynamicModel->counters && !node.dynamicModel->counters->empty())
		{
			auto* countersItem = new EntityCountersTreeWidgetItem(_controlledEntityID, *node.dynamicModel->counters, q);
			countersItem->setText(0, "Counters");
		}

		// Statistics
		{
			auto* statisticsItem = new EntityStatisticsTreeWidgetItem(_controlledEntityID, entity.getAecpRetryCounter(), entity.getAecpTimeoutCounter(), entity.getAecpUnexpectedResponseCounter(), entity.getAecpResponseAverageTime(), entity.getAemAecpUnsolicitedCounter(), entity.getEnumerationTime(), q);
			statisticsItem->setText(0, "Statistics");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::ConfigurationNode const& node) noexcept override
	{
		createIdItem(&node);
		// Always want to display dynamic information for configurations
		createNameItem(controlledEntity, true, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetConfigurationName, node.descriptorIndex, node.descriptorIndex);
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::AudioUnitNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioUnitName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto* dynamicItem = new AudioUnitDynamicTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.staticModel, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::StreamInputNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorType, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "AVB Interface Index", model->avbInterfaceIndex);
			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto* dynamicItem = new StreamDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, node.dynamicModel, nullptr, q);
			dynamicItem->setText(0, "Dynamic Info");
		}

		// Counters (if supported by the entity)
		if (isActiveConfiguration && node.descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput && node.dynamicModel->counters && !node.dynamicModel->counters->empty())
		{
			auto* countersItem = new StreamInputCountersTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.dynamicModel->connectionInfo.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected, *node.dynamicModel->counters, q);
			countersItem->setText(0, "Counters");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::StreamOutputNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorType, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "AVB Interface Index", model->avbInterfaceIndex);
			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto* dynamicItem = new StreamDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, nullptr, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}

		// Counters (if supported by the entity)
		if (isActiveConfiguration && node.descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput && node.dynamicModel->counters && !node.dynamicModel->counters->empty())
		{
			auto* countersItem = new StreamOutputCountersTreeWidgetItem(_controlledEntityID, node.descriptorIndex, *node.dynamicModel->counters, q);
			countersItem->setText(0, "Counters");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::AvbInterfaceNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetAvbInterfaceName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "MAC Address", la::networkInterface::NetworkInterfaceHelper::macAddressToString(model->macAddress, true));
			addFlagsItem(descriptorItem, "Flags", la::avdecc::utils::forceNumeric(model->interfaceFlags.value()), avdecc::helper::flagsToString(model->interfaceFlags));
			addTextItem(descriptorItem, "Clock Identity", hive::modelsLibrary::helper::uniqueIdentifierToString(model->clockIdentity));
			addTextItem(descriptorItem, "Priority 1", hive::modelsLibrary::helper::toHexQString(model->priority1, true, true));
			addTextItem(descriptorItem, "Clock Class", hive::modelsLibrary::helper::toHexQString(model->clockClass, true, true));
			addTextItem(descriptorItem, "Offset Scaled Log Variance", hive::modelsLibrary::helper::toHexQString(model->offsetScaledLogVariance, true, true));
			addTextItem(descriptorItem, "Clock Accuracy", hive::modelsLibrary::helper::toHexQString(model->clockAccuracy, true, true));
			addTextItem(descriptorItem, "Priority 2", hive::modelsLibrary::helper::toHexQString(model->priority2, true, true));
			addTextItem(descriptorItem, "Domain Number", hive::modelsLibrary::helper::toHexQString(model->domainNumber, true, true));
			addTextItem(descriptorItem, "Log Sync Interval", hive::modelsLibrary::helper::toHexQString(model->logSyncInterval, true, true));
			addTextItem(descriptorItem, "Log Announce Interval", hive::modelsLibrary::helper::toHexQString(model->logAnnounceInterval, true, true));
			addTextItem(descriptorItem, "Log Delay Interval", hive::modelsLibrary::helper::toHexQString(model->logPDelayInterval, true, true));
			addTextItem(descriptorItem, "Port Number", hive::modelsLibrary::helper::toHexQString(model->portNumber, true, true));
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto linkStatus = controlledEntity->getAvbInterfaceLinkStatus(node.descriptorIndex);
			auto* dynamicItem = new AvbInterfaceDynamicTreeWidgetItem(_controlledEntityID, node.descriptorIndex, node.dynamicModel, linkStatus, q);
			dynamicItem->setText(0, "Dynamic Info");
		}

		// Counters (if supported by the entity)
		if (isActiveConfiguration && node.dynamicModel->counters && !node.dynamicModel->counters->empty())
		{
			auto* countersItem = new AvbInterfaceCountersTreeWidgetItem(_controlledEntityID, node.descriptorIndex, *node.dynamicModel->counters, q);
			countersItem->setText(0, "Counters");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::ClockSourceNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSourceName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model (and dynamic read-only part)
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;
			auto const* const dynamicModel = node.dynamicModel;

			addTextItem(descriptorItem, "Clock Source Type", avdecc::helper::clockSourceTypeToString(model->clockSourceType));
			addFlagsItem(descriptorItem, "Flags", la::avdecc::utils::forceNumeric(dynamicModel->clockSourceFlags.value()), avdecc::helper::flagsToString(dynamicModel->clockSourceFlags));

			addTextItem(descriptorItem, "Clock Source Identifier", hive::modelsLibrary::helper::uniqueIdentifierToString(dynamicModel->clockSourceIdentifier));
			addTextItem(descriptorItem, "Clock Source Location Type", avdecc::helper::descriptorTypeToString(model->clockSourceLocationType));
			addTextItem(descriptorItem, "Clock Source Location Index", model->clockSourceLocationIndex);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::LocaleNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Number of String Descriptors", model->numberOfStringDescriptors);
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::StringsNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			if (model)
			{
				auto idx = 0u;
				for (auto const& str : model->strings)
				{
					addTextItem(descriptorItem, QString("String %1").arg(idx), str.str());
					++idx;
				}
			}
			else
			{
				addTextItem(descriptorItem, ("Not retrieved from entity"), "");
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, bool const isActiveConfiguration, la::avdecc::controller::model::StreamPortNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Clock Domain Index", model->clockDomainIndex);
			addFlagsItem(descriptorItem, "Flags", la::avdecc::utils::forceNumeric(model->portFlags.value()), avdecc::helper::flagsToString(model->portFlags));
			addTextItem(descriptorItem, "Supports Dynamic Mapping", model->hasDynamicAudioMap ? "Yes" : "No");
		}

		// Dynamic model
		auto const hasAtLeastOneDynamicInfo = node.staticModel->hasDynamicAudioMap;
		if (isActiveConfiguration && hasAtLeastOneDynamicInfo)
		{
			auto* dynamicItem = new StreamPortDynamicTreeWidgetItem(_controlledEntityID, node.descriptorType, node.descriptorIndex, node.staticModel, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::AudioClusterNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioClusterName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			addTextItem(descriptorItem, "Signal Type", avdecc::helper::descriptorTypeToString(model->signalType));
			addTextItem(descriptorItem, "Signal Index", model->signalIndex);

			addTextItem(descriptorItem, "Signal Output", model->signalOutput);
			addTextItem(descriptorItem, "Path Latency", model->pathLatency);
			addTextItem(descriptorItem, "Block Latency", model->blockLatency);
			addTextItem(descriptorItem, "Channel Count", model->channelCount);
			addTextItem(descriptorItem, "Format", avdecc::helper::audioClusterFormatToString(model->format));
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::AudioMapNode const& node) noexcept override
	{
		createIdItem(&node);

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;

			auto* mappingsIndexItem = new QTreeWidgetItem(descriptorItem);
			mappingsIndexItem->setText(0, "Mappings");

			auto* listWidget = new QListWidget;

			q->setItemWidget(mappingsIndexItem, 1, listWidget);

			for (auto const& mapping : model->mappings)
			{
				listWidget->addItem(QString("%1.%2 > %3.%4").arg(mapping.streamIndex).arg(mapping.streamChannel).arg(mapping.clusterOffset).arg(mapping.clusterChannel));
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::ControlNode const& node) noexcept override
	{
		static auto s_Dispatch = VisitControlValuesDispatchTable{};
		if (s_Dispatch.empty())
		{
			// Create the dispatch table
			createControlValuesDispatchTable(s_Dispatch);
		}

		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetControlName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);
		auto const* const staticModel = node.staticModel;
		auto const* const dynamicModel = node.dynamicModel;
		auto const valueType = staticModel->controlValueType.getType();

		if (!staticModel->values)
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Error");
			descriptorItem->setText(1, "Invalid Descriptor");
			return;
		}

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			addTextItem(descriptorItem, "Signal Type", avdecc::helper::descriptorTypeToString(staticModel->signalType));
			addTextItem(descriptorItem, "Signal Index", staticModel->signalIndex);
			addTextItem(descriptorItem, "Signal Output", staticModel->signalOutput);

			addTextItem(descriptorItem, "Block Latency", staticModel->blockLatency);
			addTextItem(descriptorItem, "Control Latency", staticModel->controlLatency);
			addTextItem(descriptorItem, "Control Domain", staticModel->controlDomain);
			addTextItem(descriptorItem, "Auto Reset Time (usec)", staticModel->resetTime);

			addTextItem(descriptorItem, "Control Type", avdecc::helper::controlTypeToString(staticModel->controlType));
			addTextItem(descriptorItem, "Values Type", avdecc::helper::controlValueTypeToString(valueType));
			addTextItem(descriptorItem, "Values Writable", staticModel->controlValueType.isReadOnly() ? "False" : "True");
			addTextItem(descriptorItem, "Values Valid", staticModel->controlValueType.isUnknown() ? "False" : "True");
			addTextItem(descriptorItem, "Values Count", dynamicModel->values.size());

			// Display static values
			if (auto const& it = s_Dispatch.find(valueType); it != s_Dispatch.end())
			{
				it->second->visitStaticControlValues(this, controlledEntity, descriptorItem, *staticModel, *dynamicModel);
			}
			else
			{
				addTextItem(descriptorItem, "Values", "Value Type Not Supported");
			}
		}

		// Dynamic model
		{
			// Display static values
			if (auto const& it = s_Dispatch.find(valueType); it != s_Dispatch.end())
			{
				it->second->visitDynamicControlValues(q, _controlledEntityID, node.descriptorIndex, *staticModel, *dynamicModel);
			}
			else
			{
				auto* descriptorItem = new QTreeWidgetItem(q);
				descriptorItem->setText(0, "Dynamic Info");
				descriptorItem->setText(0, "Value Type Not Supported");
			}
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::ClockDomainNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockDomainName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);
		auto const* const model = node.staticModel;
		auto const* const dynamicModel = node.dynamicModel;

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			addTextItem(descriptorItem, "Clock Sources count", model->clockSources.size());
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto* dynamicItem = new QTreeWidgetItem(q);
			dynamicItem->setText(0, "Dynamic Info");

			auto* currentSourceItem = new QTreeWidgetItem(dynamicItem);
			currentSourceItem->setText(0, "Current Clock Source");

			auto* sourceComboBox = new AecpCommandComboBox<la::avdecc::entity::model::ClockSourceIndex>();
			auto clockSources = std::remove_pointer_t<decltype(sourceComboBox)>::Data{};
			for (auto const sourceIndex : model->clockSources)
			{
				clockSources.insert(sourceIndex);
				/*try
				{
					auto const& clockSourceNode = controlledEntity->getClockSourceNode(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, sourceIndex);
					auto const name = QString::number(sourceIndex) + ": '" + hive::modelsLibrary::helper::objectName(controlledEntity, clockSourceNode) + "' (" + avdecc::helper::clockSourceToString(clockSourceNode) + ")";
					sourceComboBox->addItem(name, QVariant::fromValue(sourceIndex));
				}
				catch (...)
				{
				}*/
			}
			sourceComboBox->setAllData(clockSources,
				[this](auto const& sourceIndex)
				{
					auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
					auto const controlledEntity = manager.getControlledEntity(_controlledEntityID);

					if (controlledEntity)
					{
						try
						{
							auto const& entity = *controlledEntity;
							auto const& clockSourceNode = entity.getClockSourceNode(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, sourceIndex);
							return QString::number(sourceIndex) + ": " + hive::modelsLibrary::helper::objectName(&entity, clockSourceNode) + " (" + avdecc::helper::clockSourceToString(clockSourceNode) + ")";
						}
						catch (...)
						{
							// Ignore exception
						}
					}
					return QString::number(sourceIndex);
				});

			q->setItemWidget(currentSourceItem, 1, sourceComboBox);

			// Send changes
			sourceComboBox->setDataChangedHandler(
				[this, sourceComboBox, clockDomainIndex = node.descriptorIndex](auto const& previousSourceIndex, auto const& newSourceIndex)
				{
					hive::modelsLibrary::ControllerManager::getInstance().setClockSource(_controlledEntityID, clockDomainIndex, newSourceIndex, sourceComboBox->getBeginCommandHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource), sourceComboBox->getResultHandler(hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSource, previousSourceIndex));
				});

			// Listen for changes
			connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockSourceChanged, sourceComboBox,
				[this, streamType = node.descriptorType, domainIndex = node.descriptorIndex, sourceComboBox](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const sourceIndex)
				{
					if (entityID == _controlledEntityID && clockDomainIndex == domainIndex)
					{
						sourceComboBox->setCurrentData(sourceIndex);
					}
				});

			// Update now
			sourceComboBox->setCurrentData(dynamicModel->clockSourceIndex);
		}

		// Counters (if supported by the entity)
		if (isActiveConfiguration && node.dynamicModel->counters && !node.dynamicModel->counters->empty())
		{
			auto* countersItem = new ClockDomainCountersTreeWidgetItem(_controlledEntityID, node.descriptorIndex, *node.dynamicModel->counters, q);
			countersItem->setText(0, "Counters");
		}
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, bool const /*isActiveConfiguration*/, la::avdecc::controller::model::RedundantStreamNode const& /*node*/) noexcept override
	{
		//createIdItem(&node);
		//createNameItem(controlledEntity, isActiveConfiguration, node.clockDomainDescriptor, hive::modelsLibrary::ControllerManager::CommandType::None, {}); // SetName not supported yet
	}

	virtual void visit(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const isActiveConfiguration, la::avdecc::controller::model::MemoryObjectNode const& node) noexcept override
	{
		createIdItem(&node);
		createNameItem(controlledEntity, isActiveConfiguration, node, hive::modelsLibrary::ControllerManager::AecpCommandType::SetMemoryObjectName, node.descriptorIndex, std::make_tuple(controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex));

		Q_Q(NodeTreeWidget);

		// Static model
		{
			auto* descriptorItem = new QTreeWidgetItem(q);
			descriptorItem->setText(0, "Static Info");

			auto const* const model = node.staticModel;
			addTextItem(descriptorItem, "Memory object type", avdecc::helper::memoryObjectTypeToString(model->memoryObjectType));
			addTextItem(descriptorItem, "Target descriptor type", avdecc::helper::descriptorTypeToString(model->targetDescriptorType));
			addTextItem(descriptorItem, "Target descriptor index", model->targetDescriptorIndex);
			addTextItem(descriptorItem, "Start address", hive::modelsLibrary::helper::toHexQString(model->startAddress, false, true));
			addTextItem(descriptorItem, "Maximum length", hive::modelsLibrary::helper::toHexQString(model->maximumLength, false, true));

			// Check and add ImageItem, if this MemoryObject is a supported image type
			checkAddImageItem(descriptorItem, "Preview", model->memoryObjectType);

			// Check and add FirmwareUpload, if this MemoryObject is a supported firmware type
			checkAddFirmwareItem(descriptorItem, "Firmware", model->memoryObjectType, node.descriptorIndex, model->startAddress, model->maximumLength);
		}

		// Dynamic model
		if (isActiveConfiguration)
		{
			auto* dynamicItem = new MemoryObjectDynamicTreeWidgetItem(_controlledEntityID, controlledEntity->getEntityNode().dynamicModel->currentConfiguration, node.descriptorIndex, node.dynamicModel, q);
			dynamicItem->setText(0, "Dynamic Info");
		}
	}

public:
	QTreeWidgetItem* createIdItem(la::avdecc::controller::model::EntityModelNode const* node)
	{
		Q_Q(NodeTreeWidget);

		auto* idItem = new QTreeWidgetItem(q);
		idItem->setText(0, "Id");

		auto* descriptorTypeItem = new QTreeWidgetItem(idItem);
		descriptorTypeItem->setText(0, "Descriptor Type");
		descriptorTypeItem->setText(1, avdecc::helper::descriptorTypeToString(node->descriptorType));

		auto* descriptorIndexItem = new QTreeWidgetItem(idItem);
		descriptorIndexItem->setText(0, "Descriptor Index");
		descriptorIndexItem->setText(1, QString::number(node->descriptorIndex));

		return idItem;
	}

	QTreeWidgetItem* createAccessItem(la::avdecc::controller::ControlledEntity const* const controlledEntity)
	{
		Q_Q(NodeTreeWidget);
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

		auto* accessItem = new QTreeWidgetItem(q);
		accessItem->setText(0, "Exclusive Access");

		// Acquire State
		if (!controlledEntity->getCompatibilityFlags().test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			auto* acquireLabel = addChangingTextItem(accessItem, "Acquire State");
			auto const updateAcquireLabel = [this, acquireLabel](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::AcquireState const acquireState, la::avdecc::UniqueIdentifier const owningEntity)
			{
				if (entityID == _controlledEntityID)
					acquireLabel->setText(avdecc::helper::acquireStateToString(acquireState, owningEntity));
			};

			// Update text now
			updateAcquireLabel(_controlledEntityID, controlledEntity->getAcquireState(), controlledEntity->getOwningControllerID());

			// Listen for changes
			connect(&controllerManager, &hive::modelsLibrary::ControllerManager::acquireStateChanged, acquireLabel, updateAcquireLabel);
		}

		// Lock State
		{
			auto* lockLabel = addChangingTextItem(accessItem, "Lock State");
			auto const updateLockLabel = [this, lockLabel](la::avdecc::UniqueIdentifier const entityID, la::avdecc::controller::model::LockState const lockState, la::avdecc::UniqueIdentifier const lockingEntity)
			{
				if (entityID == _controlledEntityID)
					lockLabel->setText(avdecc::helper::lockStateToString(lockState, lockingEntity));
			};

			// Update text now
			updateLockLabel(_controlledEntityID, controlledEntity->getLockState(), controlledEntity->getLockingControllerID());

			// Listen for changes
			connect(&controllerManager, &hive::modelsLibrary::ControllerManager::lockStateChanged, lockLabel, updateLockLabel);
		}

		return accessItem;
	}

	template<class NodeType>
	QTreeWidgetItem* createNameItem(la::avdecc::controller::ControlledEntity const* const controlledEntity, bool const showDynamicInformation, NodeType const& node, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::any const& customData)
	{
		Q_Q(NodeTreeWidget);

		auto* nameItem = new QTreeWidgetItem(q);
		nameItem->setText(0, "Name");

		if (showDynamicInformation)
		{
			if (commandType != hive::modelsLibrary::ControllerManager::AecpCommandType::None)
			{
				addEditableTextItem(nameItem, "Name", node.dynamicModel->objectName.data(), commandType, descriptorIndex, customData);
			}
			else
			{
				addTextItem(nameItem, "Name", node.dynamicModel->objectName.data());
			}
		}
		else
		{
			nameItem->setText(1, "");
		}

		auto* localizedNameItem = new QTreeWidgetItem(nameItem);
		localizedNameItem->setText(0, "Localized Name");
		localizedNameItem->setText(1, hive::modelsLibrary::helper::localizedString(*controlledEntity, node.staticModel->localizedDescription));

		return nameItem;
	}

	/** A label (readonly) item */
	template<typename ValueType>
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, ValueType itemValue)
	{
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		item->setData(1, Qt::DisplayRole, std::move(itemValue));
	}
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, QString itemValue)
	{
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		item->setText(1, std::move(itemValue));
	}
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, std::string const& itemValue)
	{
		addTextItem(treeWidgetItem, std::move(itemName), QString::fromStdString(itemValue));
	}
	// Not sure why we need this overload but without it compilation fails on gcc
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, long const& itemValue)
	{
		addTextItem(treeWidgetItem, std::move(itemName), QVariant::fromValue(itemValue));
	}
	// Not sure why we need this overload but without it compilation fails on gcc
	void addTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, unsigned long const& itemValue)
	{
		addTextItem(treeWidgetItem, std::move(itemName), QVariant::fromValue(itemValue));
	}

	/** A flags item */
	template<typename IntegralValueType, typename = std::enable_if_t<std::is_arithmetic<IntegralValueType>::value>>
	void addFlagsItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, IntegralValueType flagsValue, QString flagsString)
	{
		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));
		setFlagsItemText(item, flagsValue, flagsString);
	}

	/** A changing (readonly) text item */
	QLabel* addChangingTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName)
	{
		Q_Q(NodeTreeWidget);

		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));

		auto* label = new QLabel;
		q->setItemWidget(item, 1, label);

		return label;
	}

	/** An editable text entry item */
	void addEditableTextItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, QString itemValue, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::any const& customData)
	{
		Q_Q(NodeTreeWidget);

		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, itemName);

		auto* textEntry = new AecpCommandTextEntry(_controlledEntityID, commandType, descriptorIndex, itemValue, avdecc::AvdeccStringValidator::getSharedInstance());

		q->setItemWidget(item, 1, textEntry);

		connect(textEntry, &AecpCommandTextEntry::validated, textEntry,
			[this, commandType, descriptorIndex, customData](QString const& oldText, QString const& newText)
			{
				// Send changes
				switch (commandType)
				{
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityName:
						hive::modelsLibrary::ControllerManager::getInstance().setEntityName(_controlledEntityID, newText);
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityGroupName:
						hive::modelsLibrary::ControllerManager::getInstance().setEntityGroupName(_controlledEntityID, newText);
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetConfigurationName:
						try
						{
							auto const configIndex = std::any_cast<la::avdecc::entity::model::ConfigurationIndex>(customData);
							hive::modelsLibrary::ControllerManager::getInstance().setConfigurationName(_controlledEntityID, configIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioUnitName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::AudioUnitIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const audioUnitIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setAudioUnitName(_controlledEntityID, configIndex, audioUnitIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::StreamIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const streamType = std::get<1>(customTuple);
							auto const streamIndex = std::get<2>(customTuple);
							if (streamType == la::avdecc::entity::model::DescriptorType::StreamInput)
								hive::modelsLibrary::ControllerManager::getInstance().setStreamInputName(_controlledEntityID, configIndex, streamIndex, newText);
							else if (streamType == la::avdecc::entity::model::DescriptorType::StreamOutput)
								hive::modelsLibrary::ControllerManager::getInstance().setStreamOutputName(_controlledEntityID, configIndex, streamIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAvbInterfaceName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::AvbInterfaceIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const avbInterfaceIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setAvbInterfaceName(_controlledEntityID, configIndex, avbInterfaceIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSourceName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClockSourceIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const clockSourceIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setClockSourceName(_controlledEntityID, configIndex, clockSourceIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetMemoryObjectName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::MemoryObjectIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const memoryObjectIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setMemoryObjectName(_controlledEntityID, configIndex, memoryObjectIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioClusterName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClusterIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const audioClusterIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setAudioClusterName(_controlledEntityID, configIndex, audioClusterIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetControlName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ControlIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const controlIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setControlName(_controlledEntityID, configIndex, controlIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockDomainName:
						try
						{
							auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClockDomainIndex>>(customData);
							auto const configIndex = std::get<0>(customTuple);
							auto const clockDomainIndex = std::get<1>(customTuple);
							hive::modelsLibrary::ControllerManager::getInstance().setClockDomainName(_controlledEntityID, configIndex, clockDomainIndex, newText);
						}
						catch (...)
						{
						}
						break;
					case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAssociationID:
						try
						{
							auto const associationID = static_cast<la::avdecc::UniqueIdentifier>(la::avdecc::utils::convertFromString<la::avdecc::UniqueIdentifier::value_type>(newText.toStdString().c_str()));
							hive::modelsLibrary::ControllerManager::getInstance().setAssociationID(_controlledEntityID, associationID);
						}
						catch (std::invalid_argument const& e)
						{
							QMessageBox::warning(q_ptr, "", QString("Cannot set Association ID: Invalid EID: %1").arg(e.what()));
						}
						break;
					default:
						break;
				}
			});

		// Listen for changes
		try
		{
			switch (commandType)
			{
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityName:
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::entityNameChanged, textEntry,
						[this, textEntry](la::avdecc::UniqueIdentifier const entityID, QString const& entityName)
						{
							if (entityID == _controlledEntityID)
								textEntry->setText(entityName);
						});
					break;
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetEntityGroupName:
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::entityGroupNameChanged, textEntry,
						[this, textEntry](la::avdecc::UniqueIdentifier const entityID, QString const& entityGroupName)
						{
							if (entityID == _controlledEntityID)
								textEntry->setText(entityGroupName);
						});
					break;
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetConfigurationName:
				{
					auto const configIndex = std::any_cast<la::avdecc::entity::model::ConfigurationIndex>(customData);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::configurationNameChanged, textEntry,
						[this, textEntry, configIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, QString const& configurationName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex)
								textEntry->setText(configurationName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioUnitName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::AudioUnitIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const audioUnitIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::audioUnitNameChanged, textEntry,
						[this, textEntry, configIndex, auIndex = audioUnitIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AudioUnitIndex const audioUnitIndex, QString const& audioUnitName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && audioUnitIndex == auIndex)
								textEntry->setText(audioUnitName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetStreamName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::DescriptorType, la::avdecc::entity::model::StreamIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const streamType = std::get<1>(customTuple);
					auto const streamIndex = std::get<2>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::streamNameChanged, textEntry,
						[this, textEntry, configIndex, streamType, strIndex = streamIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::StreamIndex const streamIndex, QString const& streamName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && descriptorType == streamType && streamIndex == strIndex)
								textEntry->setText(streamName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAvbInterfaceName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::AvbInterfaceIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const avbInterfaceIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::avbInterfaceNameChanged, textEntry,
						[this, textEntry, configIndex, aiIndex = avbInterfaceIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, QString const& avbInterfaceName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && avbInterfaceIndex == aiIndex)
								textEntry->setText(avbInterfaceName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockSourceName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClockSourceIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const clockSourceIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockSourceNameChanged, textEntry,
						[this, textEntry, configIndex, csIndex = clockSourceIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex, QString const& clockSourceName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && clockSourceIndex == csIndex)
								textEntry->setText(clockSourceName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetMemoryObjectName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::MemoryObjectIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const memoryObjectIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::memoryObjectNameChanged, textEntry,
						[this, textEntry, configIndex, moIndex = memoryObjectIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::MemoryObjectIndex const memoryObjectIndex, QString const& memoryObjectName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && memoryObjectIndex == moIndex)
								textEntry->setText(memoryObjectName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAudioClusterName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClusterIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const audioClusterIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::audioClusterNameChanged, textEntry,
						[this, textEntry, configIndex, acIndex = audioClusterIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClusterIndex const audioClusterIndex, QString const& audioClusterName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && audioClusterIndex == acIndex)
								textEntry->setText(audioClusterName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetControlName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ControlIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const controlIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::controlNameChanged, textEntry,
						[this, textEntry, configIndex, cdIndex = controlIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ControlIndex const controlIndex, QString const& controlName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && controlIndex == cdIndex)
								textEntry->setText(controlName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetClockDomainName:
				{
					auto const customTuple = std::any_cast<std::tuple<la::avdecc::entity::model::ConfigurationIndex, la::avdecc::entity::model::ClockDomainIndex>>(customData);
					auto const configIndex = std::get<0>(customTuple);
					auto const clockDomainIndex = std::get<1>(customTuple);
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::clockDomainNameChanged, textEntry,
						[this, textEntry, configIndex, cdIndex = clockDomainIndex](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, QString const& clockDomainName)
						{
							if (entityID == _controlledEntityID && configurationIndex == configIndex && clockDomainIndex == cdIndex)
								textEntry->setText(clockDomainName);
						});
					break;
				}
				case hive::modelsLibrary::ControllerManager::AecpCommandType::SetAssociationID:
				{
					connect(&hive::modelsLibrary::ControllerManager::getInstance(), &hive::modelsLibrary::ControllerManager::associationIDChanged, textEntry,
						[this, textEntry](la::avdecc::UniqueIdentifier const entityID, std::optional<la::avdecc::UniqueIdentifier> const& associationID)
						{
							if (entityID == _controlledEntityID)
							{
								textEntry->setText(associationID ? hive::modelsLibrary::helper::uniqueIdentifierToString(*associationID) : "");
							}
						});
					break;
				}
				default:
					break;
			}
		}
		catch (...)
		{
		}
	}

	void checkAddImageItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, la::avdecc::entity::model::MemoryObjectType const memoryObjectType)
	{
		auto type = hive::widgetModelsLibrary::EntityLogoCache::Type::None;
		switch (memoryObjectType)
		{
			case la::avdecc::entity::model::MemoryObjectType::PngEntity:
				type = hive::widgetModelsLibrary::EntityLogoCache::Type::Entity;
				break;
			case la::avdecc::entity::model::MemoryObjectType::PngManufacturer:
				type = hive::widgetModelsLibrary::EntityLogoCache::Type::Manufacturer;
				break;
			default:
				return;
		}

		auto const image = hive::widgetModelsLibrary::EntityLogoCache::getInstance().getImage(_controlledEntityID, type);

		Q_Q(NodeTreeWidget);

		auto* item = new QTreeWidgetItem(treeWidgetItem);
		item->setText(0, std::move(itemName));

		auto* label = new Label;
		label->setFixedHeight(96);
		label->setImage(image);
		q->setItemWidget(item, 1, label);

		connect(label, &Label::clicked, label,
			[this, requestedType = type]()
			{
				hive::widgetModelsLibrary::EntityLogoCache::getInstance().getImage(_controlledEntityID, requestedType, true);
			});

		connect(&hive::widgetModelsLibrary::EntityLogoCache::getInstance(), &hive::widgetModelsLibrary::EntityLogoCache::imageChanged, label,
			[this, label, requestedType = type](const la::avdecc::UniqueIdentifier entityID, const hive::widgetModelsLibrary::EntityLogoCache::Type type)
			{
				if (entityID == _controlledEntityID && type == requestedType)
				{
					auto const image = hive::widgetModelsLibrary::EntityLogoCache::getInstance().getImage(_controlledEntityID, type);
					label->setImage(image);
				}
			});
	}

	void checkAddFirmwareItem(QTreeWidgetItem* const treeWidgetItem, QString itemName, la::avdecc::entity::model::MemoryObjectType const memoryObjectType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const baseAddress, std::uint64_t const maximumLength)
	{
		if (memoryObjectType == la::avdecc::entity::model::MemoryObjectType::FirmwareImage)
		{
			Q_Q(NodeTreeWidget);

			auto* item = new QTreeWidgetItem(treeWidgetItem);
			item->setText(0, std::move(itemName));

			auto* uploadButton = new QPushButton("Upload New Firmware");
			connect(uploadButton, &QPushButton::clicked,
				[this, descriptorIndex, baseAddress, maximumLength]()
				{
					QString fileName = QFileDialog::getOpenFileName(q_ptr, "Choose Firmware File", "", "");

					if (!fileName.isEmpty())
					{
						// Open the file
						QFile file{ fileName };
						if (!file.open(QIODevice::ReadOnly))
						{
							QMessageBox::critical(q_ptr, "", "Failed to load firmware file");
							return;
						}

						// Read all data
						auto data = file.readAll();

						// Check length
						if (maximumLength != 0 && static_cast<decltype(maximumLength)>(data.size()) > maximumLength)
						{
							QMessageBox::critical(q_ptr, "", "firmware image file is too large for this entity");
							return;
						}

						// Start firmware upload dialog
						FirmwareUploadDialog dialog{ { data.constData(), static_cast<size_t>(data.count()) }, QFileInfo(fileName).fileName(), { { _controlledEntityID, descriptorIndex, baseAddress } }, q_ptr };
						dialog.exec();
					}
				});
			q->setItemWidget(item, 1, uploadButton);
		}
	}

	static void createControlValuesDispatchTable(VisitControlValuesDispatchTable& dispatchTable);

private:
	NodeTreeWidget* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(NodeTreeWidget);

	la::avdecc::UniqueIdentifier _controlledEntityID{};
};

/** Linear Values */
template<class StaticValueType, class DynamicValueType>
class VisitControlLinearValues final : public NodeTreeWidgetPrivate::VisitControlValues
{
	virtual void visitStaticControlValues(NodeTreeWidgetPrivate* self, la::avdecc::controller::ControlledEntity const* const controlledEntity, QTreeWidgetItem* const item, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/) noexcept override
	{
		auto valNumber = decltype(std::declval<decltype(staticModel.values)>().size()){ 0u };
		auto const linearValues = staticModel.values.getValues<StaticValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop

		for (auto const& val : linearValues.getValues())
		{
			auto* valueItem = new QTreeWidgetItem(item);
			valueItem->setText(0, QString("Value %1").arg(valNumber));

			self->addTextItem(valueItem, "Minimum", val.minimum);
			self->addTextItem(valueItem, "Maximum", val.maximum);
			self->addTextItem(valueItem, "Step", val.step);
			self->addTextItem(valueItem, "Default Value", val.defaultValue);
			self->addTextItem(valueItem, "Unit Type", ::avdecc::helper::controlValueUnitToString(val.unit.getUnit()));
			self->addTextItem(valueItem, "Unit Multiplier", val.unit.getMultiplier());
			auto* localizedNameItem = new QTreeWidgetItem(valueItem);
			localizedNameItem->setText(0, "Localized Name");
			localizedNameItem->setText(1, hive::modelsLibrary::helper::localizedString(*controlledEntity, val.localizedName));

			++valNumber;
		}
	}
	virtual void visitDynamicControlValues(QTreeWidget* const tree, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel) noexcept override
	{
		auto* dynamicItem = new LinearControlValuesDynamicTreeWidgetItem<StaticValueType, DynamicValueType>(entityID, controlIndex, staticModel, dynamicModel, tree);
		dynamicItem->setText(0, "Dynamic Info");
	}
};

/** Array Values */
template<typename SizeType, typename StaticValueType = la::avdecc::entity::model::ArrayValueStatic<SizeType>, typename DynamicValueType = la::avdecc::entity::model::ArrayValueDynamic<SizeType>>
class VisitControlArrayValues final : public NodeTreeWidgetPrivate::VisitControlValues
{
	virtual void visitStaticControlValues(NodeTreeWidgetPrivate* self, la::avdecc::controller::ControlledEntity const* const controlledEntity, QTreeWidgetItem* const item, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/) noexcept override
	{
		auto const& arrayValue = staticModel.values.getValues<StaticValueType>();

		self->addTextItem(item, "Minimum", arrayValue.minimum);
		self->addTextItem(item, "Maximum", arrayValue.maximum);
		self->addTextItem(item, "Step", arrayValue.step);
		self->addTextItem(item, "Default Value", arrayValue.defaultValue);
		self->addTextItem(item, "Unit Type", ::avdecc::helper::controlValueUnitToString(arrayValue.unit.getUnit()));
		self->addTextItem(item, "Unit Multiplier", arrayValue.unit.getMultiplier());
		auto* localizedNameItem = new QTreeWidgetItem(item);
		localizedNameItem->setText(0, "Localized Name");
		localizedNameItem->setText(1, hive::modelsLibrary::helper::localizedString(*controlledEntity, arrayValue.localizedName));
	}
	virtual void visitDynamicControlValues(QTreeWidget* const tree, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel) noexcept override
	{
		auto* dynamicItem = new ArrayControlValuesDynamicTreeWidgetItem<StaticValueType, DynamicValueType>(entityID, controlIndex, staticModel, dynamicModel, tree);
		dynamicItem->setText(0, "Dynamic Info");
	}
};

/** UTF-8 String Value */
class VisitControlUtf8Values final : public NodeTreeWidgetPrivate::VisitControlValues
{
public:
	virtual void visitStaticControlValues(NodeTreeWidgetPrivate* self, la::avdecc::controller::ControlledEntity const* const /*controlledEntity*/, QTreeWidgetItem* const item, la::avdecc::entity::model::ControlNodeStaticModel const& /*staticModel*/, la::avdecc::entity::model::ControlNodeDynamicModel const& /*dynamicModel*/) noexcept override
	{
		// Nothing to display
	}
	virtual void visitDynamicControlValues(QTreeWidget* const tree, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel) noexcept override
	{
		auto* dynamicItem = new UTF8ControlValuesDynamicTreeWidgetItem(entityID, controlIndex, staticModel, dynamicModel, tree);
		dynamicItem->setText(0, "Dynamic Info");
	}
};

void NodeTreeWidgetPrivate::createControlValuesDispatchTable(VisitControlValuesDispatchTable& dispatchTable)
{
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt8] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::int8_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::int8_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt8] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::uint8_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::uint8_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt16] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::int16_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::int16_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt16] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::uint16_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::uint16_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt32] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::int32_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::int32_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt32] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::uint32_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::uint32_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt64] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::int64_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::int64_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt64] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<std::uint64_t>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<std::uint64_t>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearFloat] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<float>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<float>>>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlLinearDouble] = std::make_unique<VisitControlLinearValues<la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueStatic<double>>, la::avdecc::entity::model::LinearValues<la::avdecc::entity::model::LinearValueDynamic<double>>>>();

	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt8] = std::make_unique<VisitControlArrayValues<std::int8_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt8] = std::make_unique<VisitControlArrayValues<std::uint8_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt16] = std::make_unique<VisitControlArrayValues<std::int16_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt16] = std::make_unique<VisitControlArrayValues<std::uint16_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt32] = std::make_unique<VisitControlArrayValues<std::int32_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt32] = std::make_unique<VisitControlArrayValues<std::uint32_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt64] = std::make_unique<VisitControlArrayValues<std::int64_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt64] = std::make_unique<VisitControlArrayValues<std::uint64_t>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayFloat] = std::make_unique<VisitControlArrayValues<float>>();
	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlArrayDouble] = std::make_unique<VisitControlArrayValues<double>>();

	dispatchTable[la::avdecc::entity::model::ControlValueType::Type::ControlUtf8] = std::make_unique<VisitControlUtf8Values>();
}

NodeTreeWidget::NodeTreeWidget(QWidget* parent)
	: QTreeWidget(parent)
	, d_ptr(new NodeTreeWidgetPrivate(this))
{
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::SingleSelection);
	header()->resizeSection(0, 200);
}

NodeTreeWidget::~NodeTreeWidget()
{
	delete d_ptr;
}

void NodeTreeWidget::setNode(la::avdecc::UniqueIdentifier const entityID, bool const isActiveConfiguration, AnyNode const& node)
{
	Q_D(NodeTreeWidget);
	d->setNode(entityID, isActiveConfiguration, node);
}

#include "nodeTreeWidget.moc"
