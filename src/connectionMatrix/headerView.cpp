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

#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "avdecc/mappingsHelper.hpp"
#include <QtMate/material/color.hpp>

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QPainter>
#include <QContextMenuEvent>
#include <QMenu>

#include <optional>

#if ENABLE_CONNECTION_MATRIX_DEBUG
#	include <QDebug>
#endif

#ifdef Q_CC_MSVC
#	pragma warning(push)
#	pragma warning(disable : 4127) // "conditional expression is constant" due to QVector compilation warning
#endif

namespace connectionMatrix
{
HeaderView::HeaderView(bool const isListenersHeader, Qt::Orientation const orientation, QWidget* parent)
	: QHeaderView{ orientation, parent }
	, _isListenersHeader(isListenersHeader)
{
	setSectionResizeMode(QHeaderView::Fixed);
	setSectionsClickable(true);

	int const size{ 20 };
	setMaximumSectionSize(size);
	setMinimumSectionSize(size);
	setDefaultSectionSize(size);

	setAttribute(Qt::WA_Hover);

	connect(this, &QHeaderView::sectionClicked, this, &HeaderView::handleSectionClicked);
}

bool HeaderView::isListenersHeader() const noexcept
{
	return _isTransposed ? !_isListenersHeader : _isListenersHeader;
}

void HeaderView::setAlwaysShowArrowTip(bool const show)
{
	_alwaysShowArrowTip = show;
	update();
}
void HeaderView::setAlwaysShowArrowEnd(bool const show)
{
	_alwaysShowArrowEnd = show;
	update();
}

void HeaderView::setTransposed(bool const isTransposed)
{
	_isTransposed = isTransposed;
	update();
}

void HeaderView::setCollapsedByDefault(bool const collapsedByDefault)
{
	_collapsedByDefault = collapsedByDefault;
	update();
}

void HeaderView::setColor(qtMate::material::color::Name const name)
{
	_colorName = name;
	update();
}

QVector<HeaderView::SectionState> HeaderView::saveSectionState() const
{
	return _sectionState;
}

void HeaderView::restoreSectionState(QVector<SectionState> const& sectionState)
{
	if (!AVDECC_ASSERT_WITH_RET(sectionState.count() == count(), "invalid count"))
	{
		_sectionState = {};
		return;
	}

	_sectionState = sectionState;

	for (auto section = 0; section < count(); ++section)
	{
		updateSectionVisibility(section);
	}
}

void HeaderView::setFilterPattern(QRegularExpression const& pattern)
{
	_pattern = pattern;
	applyFilterPattern();
}

void HeaderView::expandAll()
{
	for (auto section = 0; section < count(); ++section)
	{
		_sectionState[section].expanded = true;
		_sectionState[section].visible = true;

		updateSectionVisibility(section);
	}

	applyFilterPattern();
}

void HeaderView::collapseAll()
{
	auto* model = static_cast<Model*>(this->model());

	for (auto section = 0; section < count(); ++section)
	{
		auto* node = model->node(section, orientation());

		if (node->type() == Node::Type::Entity)
		{
			_sectionState[section].expanded = false;
			_sectionState[section].visible = true;
		}
		else
		{
			_sectionState[section].expanded = false;
			_sectionState[section].visible = false;
		}

		updateSectionVisibility(section);
	}

	applyFilterPattern();
}

void HeaderView::handleSectionClicked(int logicalIndex)
{
	auto* model = static_cast<Model*>(this->model());
	auto* node = model->node(logicalIndex, orientation());

	if (!AVDECC_ASSERT_WITH_RET(node, "invalid node"))
	{
		return;
	}

	if (node->childrenCount() == 0)
	{
		return;
	}

	// Toggle the section expand state
	auto const expanded = !_sectionState[logicalIndex].expanded;
	_sectionState[logicalIndex].expanded = expanded;

#if ENABLE_CONNECTION_MATRIX_DEBUG
	qDebug() << logicalIndex << "is now" << (expanded ? "expanded" : "collapsed");
#endif

	// Update hierarchy visibility
	auto const update = [=](Node* node)
	{
		auto const section = model->section(node, orientation());

		// Do not affect the current node
		if (section == logicalIndex)
		{
			return;
		}

		if (auto* parent = node->parent())
		{
			auto const parentSection = model->section(parent, orientation());
			_sectionState[section].visible = expanded && _sectionState[parentSection].expanded;

			updateSectionVisibility(section);
		}
	};

	model->accept(node, update, true);
}

void HeaderView::handleSectionInserted(QModelIndex const& /*parent*/, int first, int last)
{
	auto const it = std::next(std::begin(_sectionState), first);
	_sectionState.insert(it, last - first + 1, {});

	for (auto section = first; section <= last; ++section)
	{
		auto* model = static_cast<Model*>(this->model());
		auto* node = model->node(section, orientation());

		if (AVDECC_ASSERT_WITH_RET(node, "Node should not be null"))
		{
			auto expanded = true;
			auto visible = true;

			switch (node->type())
			{
				case Node::Type::Entity:
					// Currently don't collapse in Channel mode (Because summary is not supported)
					if (model->mode() == Model::Mode::Channel)
					{
						expanded = true;
					}
					else
					{
						expanded = !_collapsedByDefault;
					}
					break;
				case Node::Type::RedundantOutput:
				case Node::Type::RedundantInput:
					visible = !_collapsedByDefault;
					expanded = false;
					break;
				case Node::Type::RedundantOutputStream:
				case Node::Type::RedundantInputStream:
					visible = false;
					break;
				case Node::Type::OutputStream:
				case Node::Type::InputStream:
					visible = !_collapsedByDefault;
					break;
				case Node::Type::OutputChannel:
				case Node::Type::InputChannel:
					// Currently don't hide in Channel mode (Because summary is not supported)
					visible = true;
					break;
				default:
					break;
			}

			_sectionState[section] = { expanded, visible };
			updateSectionVisibility(section);
		}
	}

#if ENABLE_CONNECTION_MATRIX_DEBUG
	qDebug() << "handleSectionInserted" << _sectionState.count();
#endif
}

void HeaderView::handleSectionRemoved(QModelIndex const& /*parent*/, int first, int last)
{
	_sectionState.remove(first, last - first + 1);

#if ENABLE_CONNECTION_MATRIX_DEBUG
	qDebug() << "handleSectionRemoved" << _sectionState.count();
#endif
}

void HeaderView::handleModelReset()
{
	_sectionState.clear();
}

void HeaderView::updateSectionVisibility(int const logicalIndex)
{
	if (!AVDECC_ASSERT_WITH_RET(logicalIndex >= 0 && logicalIndex < _sectionState.count(), "invalid index"))
	{
		return;
	}

	if (_sectionState[logicalIndex].visible)
	{
		showSection(logicalIndex);
	}
	else
	{
		hideSection(logicalIndex);
	}
}

void HeaderView::applyFilterPattern()
{
	auto* model = static_cast<Model*>(this->model());

	auto const showVisitor = [=](Node* node)
	{
		auto const section = model->section(node, orientation());
		updateSectionVisibility(section); // Conditional update
	};

	auto const hideVisitor = [=](Node* node)
	{
		auto const section = model->section(node, orientation());
		hideSection(section); // Hide section no matter what
	};

	for (auto section = 0; section < count(); ++section)
	{
		auto* node = model->node(section, orientation());
		if (node->type() == Node::Type::Entity)
		{
			auto const matches = !node->name().contains(_pattern);

			if (!matches)
			{
				model->accept(node, showVisitor);
			}
			else
			{
				model->accept(node, hideVisitor);
			}
		}
	}
}

void HeaderView::setModel(QAbstractItemModel* model)
{
	if (this->model())
	{
		disconnect(this->model());
	}

	if (!AVDECC_ASSERT_WITH_RET(dynamic_cast<Model*>(model), "invalid pointer kind"))
	{
		return;
	}

	QHeaderView::setModel(model);

	if (model)
	{
		if (orientation() == Qt::Vertical)
		{
			connect(model, &QAbstractItemModel::rowsInserted, this, &HeaderView::handleSectionInserted);
			connect(model, &QAbstractItemModel::rowsRemoved, this, &HeaderView::handleSectionRemoved);
		}
		else
		{
			connect(model, &QAbstractItemModel::columnsInserted, this, &HeaderView::handleSectionInserted);
			connect(model, &QAbstractItemModel::columnsRemoved, this, &HeaderView::handleSectionRemoved);
		}

		connect(model, &QAbstractItemModel::modelReset, this, &HeaderView::handleModelReset);
	}
}

QSize HeaderView::sizeHint() const
{
	if (orientation() == Qt::Horizontal)
	{
		return { defaultSectionSize(), 200 };
	}
	else
	{
		return { 200, defaultSectionSize() };
	}
}

void HeaderView::paintSection(QPainter* painter, QRect const& rect, int logicalIndex) const
{
	auto* model = static_cast<Model*>(this->model());
	auto const orientation = this->orientation();
	auto* node = model->node(logicalIndex, orientation);

	if (!node)
	{
		return;
	}

	auto backgroundColor = QColor{};
	auto foregroundColor = QColor{};
	auto foregroundErrorColor = QColor{};
	auto nodeLevel{ 0 };

	auto const nodeType = node->type();
	// First pass for Bar Color
	switch (nodeType)
	{
		case Node::Type::OfflineOutputStream:
			backgroundColor = Qt::black;
			foregroundColor = Qt::white;
			foregroundErrorColor = Qt::red;
			break;
		case Node::Type::Entity:
			backgroundColor = qtMate::material::color::value(_colorName, qtMate::material::color::Shade::Shade900);
			foregroundColor = qtMate::material::color::foregroundValue(_colorName, qtMate::material::color::Shade::Shade900);
			foregroundErrorColor = qtMate::material::color::foregroundErrorColorValue(_colorName, qtMate::material::color::Shade::Shade900);
			break;
		case Node::Type::RedundantInput:
		case Node::Type::RedundantOutput:
		case Node::Type::InputStream:
		case Node::Type::OutputStream:
		case Node::Type::InputChannel:
		case Node::Type::OutputChannel:
			backgroundColor = qtMate::material::color::value(_colorName, qtMate::material::color::Shade::Shade600);
			foregroundColor = qtMate::material::color::foregroundValue(_colorName, qtMate::material::color::Shade::Shade600);
			foregroundErrorColor = qtMate::material::color::foregroundErrorColorValue(_colorName, qtMate::material::color::Shade::Shade600);
			nodeLevel = 1;
			break;
		case Node::Type::RedundantInputStream:
		case Node::Type::RedundantOutputStream:
			backgroundColor = qtMate::material::color::value(_colorName, qtMate::material::color::Shade::Shade300);
			foregroundColor = qtMate::material::color::foregroundValue(_colorName, qtMate::material::color::Shade::Shade300);
			foregroundErrorColor = qtMate::material::color::foregroundErrorColorValue(_colorName, qtMate::material::color::Shade::Shade300);
			nodeLevel = 2;
			break;
		default:
			AVDECC_ASSERT(false, "NodeType not handled");
			return;
	}

	// Second pass for Arrow Color
	auto arrowColor = std::optional<QColor>{ std::nullopt };
	switch (nodeType)
	{
		case Node::Type::RedundantInput:
		{
			auto const state = static_cast<RedundantNode const&>(*node).lockedState();
			if (state == Node::TriState::False)
			{
				arrowColor = foregroundErrorColor;
			}
			else if (state == Node::TriState::True)
			{
				arrowColor = backgroundColor;
			}
			break;
		}
		case Node::Type::InputStream:
		case Node::Type::RedundantInputStream:
		{
			auto const state = static_cast<StreamNode const&>(*node).lockedState();
			if (state == Node::TriState::False)
			{
				arrowColor = foregroundErrorColor;
			}
			else if (state == Node::TriState::True)
			{
				arrowColor = backgroundColor;
			}
			break;
		}
		case Node::Type::RedundantOutput:
		{
			if (static_cast<RedundantNode const&>(*node).isStreaming())
			{
				arrowColor = backgroundColor;
			}
			break;
		}
		case Node::Type::OutputStream:
		case Node::Type::RedundantOutputStream:
		{
			if (static_cast<StreamNode const&>(*node).isStreaming())
			{
				arrowColor = backgroundColor;
			}
			break;
		}
		default:
			break;
	}

	auto isSelected = false;

	if (orientation == Qt::Horizontal)
	{
		isSelected = selectionModel()->isColumnSelected(logicalIndex, {});
	}
	else
	{
		isSelected = selectionModel()->isRowSelected(logicalIndex, {});
	}

	if (isSelected)
	{
		backgroundColor = qtMate::material::color::complementaryValue(_colorName, qtMate::material::color::Shade::Shade600);
		foregroundColor = qtMate::material::color::foregroundComplementaryValue(_colorName, qtMate::material::color::Shade::Shade600);
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	auto const arrowSize{ 10 };
	auto const arrowOffset{ 20 * nodeLevel };

	// Draw the main background arrow
	painter->fillPath(paintHelper::buildHeaderArrowPath(rect, orientation, _isTransposed, _alwaysShowArrowTip, _alwaysShowArrowEnd, arrowOffset, arrowSize, 0), backgroundColor);

	// Draw the small arrow, if needed
	if (arrowColor)
	{
		auto path = paintHelper::buildHeaderArrowPath(rect, orientation, _isTransposed, _alwaysShowArrowTip, _alwaysShowArrowEnd, arrowOffset, arrowSize, 5);
		if (orientation == Qt::Horizontal)
		{
			path.translate(0, 10);
		}
		else
		{
			path.translate(10, 0);
		}

		painter->fillPath(path, *arrowColor);
	}

	painter->translate(rect.topLeft());

	auto textLeftOffset = 0;
	auto textRightOffset = 0;
	auto r = QRect(0, 0, rect.width(), rect.height());
	if (orientation == Qt::Horizontal)
	{
		r.setWidth(rect.height());
		r.setHeight(rect.width());

		painter->rotate(-90);
		painter->translate(-r.width(), 0);

		r.translate(arrowOffset, 0);

		textLeftOffset = arrowSize;
		textRightOffset = _isTransposed ? (_alwaysShowArrowEnd ? arrowSize : 0) : (_alwaysShowArrowTip ? arrowSize : 0);
	}
	else
	{
		textLeftOffset = _isTransposed ? (_alwaysShowArrowTip ? arrowSize : 0) : (_alwaysShowArrowEnd ? arrowSize : 0);
		textRightOffset = arrowSize;
	}

	auto const padding{ 2 };
	auto textRect = r.adjusted(padding + textLeftOffset, 0, -(padding + textRightOffset + arrowOffset), 0);

	auto const elidedText = painter->fontMetrics().elidedText(node->name(), Qt::ElideMiddle, textRect.width());

	if (node->isEntityNode())
	{
		if (!static_cast<EntityNode*>(node)->isRegisteredUnsol())
		{
			painter->setPen(foregroundErrorColor);
		}
		else
		{
			painter->setPen(foregroundColor);
		}
	}
	else if (node->isStreamNode())
	{
		if (!static_cast<StreamNode*>(node)->isRunning())
		{
			painter->setPen(foregroundErrorColor);
		}
		else
		{
			painter->setPen(foregroundColor);
		}
	}
	else
	{
		painter->setPen(foregroundColor);
	}

	auto const isSelectedEntity = model->headerData(logicalIndex, orientation, Model::SelectedEntityRole).toBool();
	auto font = painter->font();
	if (isSelectedEntity)
	{
		font.setBold(isSelectedEntity);
		font.setPointSize(font.pointSize() + 1);
	}
	painter->setFont(font);

	painter->drawText(textRect, Qt::AlignVCenter, elidedText);
	painter->restore();
}

void HeaderView::contextMenuEvent(QContextMenuEvent* event)
{
	auto const logicalIndex = logicalIndexAt(event->pos());
	if (logicalIndex < 0)
	{
		return;
	}

	auto* model = static_cast<Model*>(this->model());
	auto* node = model->node(logicalIndex, orientation());

	if (!AVDECC_ASSERT_WITH_RET(node, "invalid node"))
	{
		return;
	}

	auto addHeaderAction = [](QMenu& menu, QString const& text)
	{
		auto* action = menu.addAction(text);
		auto font = action->font();
		font.setBold(true);
		action->setFont(font);
		action->setEnabled(false);
		return action;
	};

	auto addAction = [](QMenu& menu, QString const& text, bool enabled)
	{
		auto* action = menu.addAction(text);
		action->setEnabled(enabled);
		return action;
	};

	auto getStreamNodeFromNode = [](la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::controller::model::EntityNode const& entityNode, connectionMatrix::Node const* node, la::avdecc::entity::model::StreamIndex streamIndex)
	{
		auto const isOutputStream = node->type() == Node::Type::OutputStream || node->type() == Node::Type::RedundantOutputStream;
		if (isOutputStream)
		{
			return static_cast<la::avdecc::controller::model::StreamNode const*>(&controlledEntity.getStreamOutputNode(entityNode.dynamicModel->currentConfiguration, streamIndex));
		}
		else if (node->type() == Node::Type::InputStream || node->type() == Node::Type::RedundantInputStream)
		{
			return static_cast<la::avdecc::controller::model::StreamNode const*>(&controlledEntity.getStreamInputNode(entityNode.dynamicModel->currentConfiguration, streamIndex));
		}
		else
		{
			return static_cast<la::avdecc::controller::model::StreamNode const*>(nullptr);
		}
	};

	struct MapSupTypeIndex
	{
		bool supportsDynamicMappings{ false };
		la::avdecc::entity::model::DescriptorType streamPortType{ la::avdecc::entity::model::DescriptorType::Invalid };
		la::avdecc::entity::model::StreamPortIndex streamPortIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
		la::avdecc::entity::model::StreamIndex streamIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	};
	auto findMappingsSupportTypeIndexForDescriptor = [](la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::DescriptorType const& descriptorType)
	{
		// Currently returning the first audio unit - TODO: If we have devices with multiple audio units, we shall probably offer to choose which audio unit to edit
		auto mti = MapSupTypeIndex{};
		try
		{
			if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortInputs)
					{
						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							mti.supportsDynamicMappings = true;
							mti.streamPortType = la::avdecc::entity::model::DescriptorType::StreamPortInput;
							mti.streamPortIndex = spi;
							break;
						}
					}
				}
			}
			else if (descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortOutputs)
					{
						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							mti.supportsDynamicMappings = true;
							mti.streamPortType = la::avdecc::entity::model::DescriptorType::StreamPortOutput;
							mti.streamPortIndex = spi;
							break;
						}
					}
				}
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
		}

		return mti;
	};
	auto findMappingsSupportTypeIndexForStreamNode = [](la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::controller::model::StreamNode const& streamNode)
	{
		auto mti = MapSupTypeIndex{};

		auto const isValidStreamFormat = [](auto const& streamNode)
		{
			auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode.dynamicModel->streamFormat);

			return sfi->getChannelsCount() > 0;
		};

		try
		{
			if (streamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					// Search which StreamPort has the same ClockDomain than passed StreamNode
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortInputs)
					{
						if (streamPortNode.staticModel->clockDomainIndex == streamNode.staticModel->clockDomainIndex)
						{
							if (streamPortNode.staticModel->hasDynamicAudioMap)
							{
								auto const& streamInputNode = static_cast<la::avdecc::controller::model::StreamInputNode const&>(streamNode);
								auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamInputNode.dynamicModel->streamFormat);

								mti.supportsDynamicMappings = isValidStreamFormat(streamInputNode);
								mti.streamPortType = la::avdecc::entity::model::DescriptorType::StreamPortInput;
								mti.streamPortIndex = spi;
								// Don't allow StreamInput to be configured individually to prevent user errors
							}
							break;
						}
					}
				}
			}
			else if (streamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					// Search which StreamPort has the same ClockDomain than passed StreamNode
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortOutputs)
					{
						if (streamPortNode.staticModel->clockDomainIndex == streamNode.staticModel->clockDomainIndex)
						{
							if (streamPortNode.staticModel->hasDynamicAudioMap)
							{
								auto const& streamOutputNode = static_cast<la::avdecc::controller::model::StreamOutputNode const&>(streamNode);
								auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamOutputNode.dynamicModel->streamFormat);

								mti.supportsDynamicMappings = isValidStreamFormat(streamOutputNode);
								mti.streamPortType = la::avdecc::entity::model::DescriptorType::StreamPortOutput;
								mti.streamPortIndex = spi;
								mti.streamIndex = streamOutputNode.descriptorIndex;
							}
							break;
						}
					}
				}
			}
		}
		catch (la::avdecc::controller::ControlledEntity::Exception const&)
		{
			// one of the given parameters is invalid.
		}

		return mti;
	};

	try
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const entityID = node->entityID();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			auto const& entityNode = controlledEntity->getEntityNode();
			la::avdecc::controller::model::StreamNode const* streamNode{ nullptr };
			la::avdecc::entity::model::StreamIndex streamIndex = la::avdecc::entity::model::getInvalidDescriptorIndex();

			QMenu menu;
			addHeaderAction(menu, "Entity: " + hive::modelsLibrary::helper::smartEntityName(*controlledEntity));

			if (node->isStreamNode())
			{
				streamIndex = static_cast<StreamNode*>(node)->streamIndex();
				streamNode = getStreamNodeFromNode(*controlledEntity, entityNode, node, streamIndex);

				if (!AVDECC_ASSERT_WITH_RET(streamNode, "invalid node"))
				{
					return;
				}

				addHeaderAction(menu, "Stream: " + node->name());

				menu.addSeparator();

				auto const isRunning = static_cast<StreamNode*>(node)->isRunning();
				auto* startStreamingAction = addAction(menu, "Start Streaming", !isRunning);
				auto* stopStreamingAction = addAction(menu, "Stop Streaming", isRunning);

				menu.addSeparator();

				auto mti = findMappingsSupportTypeIndexForStreamNode(*controlledEntity, *streamNode);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings...", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec)
				controlledEntity.reset();

				if (auto* action = menu.exec(event->globalPos()))
				{
					if (action == startStreamingAction)
					{
						if (node->type() == Node::Type::OutputStream || node->type() == Node::Type::RedundantOutputStream)
						{
							manager.startStreamOutput(entityID, streamIndex);
						}
						else
						{
							manager.startStreamInput(entityID, streamIndex);
						}
					}
					else if (action == stopStreamingAction)
					{
						if (node->type() == Node::Type::OutputStream || node->type() == Node::Type::RedundantOutputStream)
						{
							manager.stopStreamOutput(entityID, streamIndex);
						}
						else
						{
							manager.stopStreamInput(entityID, streamIndex);
						}
					}
					else if (action == editMappingsAction)
					{
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, mti.streamIndex);
					}
				}
			}
			else if (node->isRedundantNode())
			{
				if (static_cast<RedundantNode*>(node)->childrenCount() > 0)
				{
					auto const* connectionMatrixNode = static_cast<StreamNode const*>(static_cast<RedundantNode*>(node)->childAt(0));
					streamIndex = connectionMatrixNode->streamIndex();
					streamNode = getStreamNodeFromNode(*controlledEntity, entityNode, connectionMatrixNode, streamIndex);
				}

				if (!AVDECC_ASSERT_WITH_RET(streamNode, "invalid node"))
				{
					return;
				}

				addHeaderAction(menu, "Stream: " + node->name());

				menu.addSeparator();

				auto mti = findMappingsSupportTypeIndexForStreamNode(*controlledEntity, *streamNode);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings...", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec)
				controlledEntity.reset();

				if (auto* action = menu.exec(event->globalPos()))
				{
					if (action == editMappingsAction)
					{
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, mti.streamIndex);
					}
				}
			}
			else if (node->isEntityNode())
			{
				menu.addSeparator();

				auto mti = findMappingsSupportTypeIndexForDescriptor(*controlledEntity, isListenersHeader() ? la::avdecc::entity::model::DescriptorType::StreamInput : la::avdecc::entity::model::DescriptorType::StreamOutput);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings...", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec)
				controlledEntity.reset();

				if (auto* action = menu.exec(event->globalPos()))
				{
					if (action == editMappingsAction)
					{
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, mti.streamIndex);
					}
				}
			}
		}
	}
	catch (...)
	{
	}
}

void HeaderView::mouseMoveEvent(QMouseEvent* event)
{
	if (orientation() == Qt::Horizontal)
	{
		auto const column = logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
		selectionModel()->select(model()->index(0, column), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
	}
	else
	{
		auto const row = logicalIndexAt(static_cast<QMouseEvent*>(event)->pos());
		selectionModel()->select(model()->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}

	QHeaderView::mouseMoveEvent(event);
}

void HeaderView::mouseDoubleClickEvent(QMouseEvent* event)
{
	// Swallow double clicks and transform them into normal mouse press events
	mousePressEvent(event);
}

void HeaderView::leaveEvent(QEvent* event)
{
	selectionModel()->clearSelection();

	QHeaderView::leaveEvent(event);
}

void HeaderView::handleEditMappingsClicked(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::DescriptorType const streamPortType, la::avdecc::entity::model::StreamPortIndex const streamPortIndex, la::avdecc::entity::model::StreamIndex const streamIndex)
{
	avdecc::mappingsHelper::showMappingsEditor(this, entityID, streamPortType, streamPortIndex, streamIndex);
}

} // namespace connectionMatrix

#ifdef Q_CC_MSVC
#	pragma warning(pop)
#endif
