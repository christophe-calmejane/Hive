/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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
#include "avdecc/controllerManager.hpp"
#include "avdecc/mappingsHelper.hpp"
#include "toolkit/material/color.hpp"
#include <QPainter>
#include <QContextMenuEvent>
#include <QMenu>

#include <optional>

#if ENABLE_CONNECTION_MATRIX_DEBUG
#	include <QDebug>
#endif

#ifdef _WIN32
#	pragma warning(disable : 4127) // "conditional expression is constant" due to QVector compilation warning
#endif // _WIN32

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

void HeaderView::setColor(qt::toolkit::material::color::Name const name)
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

void HeaderView::setFilterPattern(QRegExp const& pattern)
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
				case Node::Type::RedundantOutput:
				case Node::Type::RedundantInput:
					expanded = false;
					break;
				case Node::Type::RedundantOutputStream:
				case Node::Type::RedundantInputStream:
					visible = false;
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
	auto* node = model->node(logicalIndex, orientation());

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
			backgroundColor = qt::toolkit::material::color::value(_colorName, qt::toolkit::material::color::Shade::Shade900);
			foregroundColor = qt::toolkit::material::color::foregroundValue(_colorName, qt::toolkit::material::color::Shade::Shade900);
			foregroundErrorColor = qt::toolkit::material::color::foregroundErrorColorValue(_colorName, qt::toolkit::material::color::Shade::Shade900);
			break;
		case Node::Type::RedundantInput:
		case Node::Type::RedundantOutput:
		case Node::Type::InputStream:
		case Node::Type::OutputStream:
		case Node::Type::InputChannel:
		case Node::Type::OutputChannel:
			backgroundColor = qt::toolkit::material::color::value(_colorName, qt::toolkit::material::color::Shade::Shade600);
			foregroundColor = qt::toolkit::material::color::foregroundValue(_colorName, qt::toolkit::material::color::Shade::Shade600);
			foregroundErrorColor = qt::toolkit::material::color::foregroundErrorColorValue(_colorName, qt::toolkit::material::color::Shade::Shade600);
			nodeLevel = 1;
			break;
		case Node::Type::RedundantInputStream:
		case Node::Type::RedundantOutputStream:
			backgroundColor = qt::toolkit::material::color::value(_colorName, qt::toolkit::material::color::Shade::Shade300);
			foregroundColor = qt::toolkit::material::color::foregroundValue(_colorName, qt::toolkit::material::color::Shade::Shade300);
			foregroundErrorColor = qt::toolkit::material::color::foregroundErrorColorValue(_colorName, qt::toolkit::material::color::Shade::Shade300);
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

	auto isSelected{ false };

	if (orientation() == Qt::Horizontal)
	{
		isSelected = selectionModel()->isColumnSelected(logicalIndex, {});
	}
	else
	{
		isSelected = selectionModel()->isRowSelected(logicalIndex, {});
	}

	if (isSelected)
	{
		backgroundColor = qt::toolkit::material::color::complementaryValue(_colorName, qt::toolkit::material::color::Shade::Shade600);
		foregroundColor = qt::toolkit::material::color::foregroundComplementaryValue(_colorName, qt::toolkit::material::color::Shade::Shade600);
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing);

	auto const arrowSize{ 10 };
	auto const arrowOffset{ 20 * nodeLevel };

	// Draw the main background arrow
	painter->fillPath(paintHelper::buildHeaderArrowPath(rect, orientation(), _isTransposed, _alwaysShowArrowTip, _alwaysShowArrowEnd, arrowOffset, arrowSize, 0), backgroundColor);

	// Draw the small arrow, if needed
	if (arrowColor)
	{
		auto path = paintHelper::buildHeaderArrowPath(rect, orientation(), _isTransposed, _alwaysShowArrowTip, _alwaysShowArrowEnd, arrowOffset, arrowSize, 5);
		if (orientation() == Qt::Horizontal)
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
	if (orientation() == Qt::Horizontal)
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

	if (node->isStreamNode() && !static_cast<StreamNode*>(node)->isRunning())
	{
		painter->setPen(foregroundErrorColor);
	}
	else
	{
		painter->setPen(foregroundColor);
	}

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

	struct MapsupTypeIndex
	{
		bool supportsDynamicMappings{ false };
		la::avdecc::entity::model::DescriptorType streamPortType{ la::avdecc::entity::model::DescriptorType::Invalid };
		la::avdecc::entity::model::StreamPortIndex streamPortIndex{ la::avdecc::entity::model::getInvalidDescriptorIndex() };
	};
	auto findMappingssupportTypeIndexForDescriptor = [](la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::DescriptorType const& descriptorType)
	{
		MapsupTypeIndex mti;
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
	auto findMappingssupportTypeIndexForStreamNode = [](la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::controller::model::StreamNode const& streamNode)
	{
		MapsupTypeIndex mti;
		try
		{
			if (streamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamInput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortInputs)
					{
						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							auto const& streamInputNode = static_cast<la::avdecc::controller::model::StreamInputNode const&>(streamNode);
							auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamInputNode.dynamicModel->streamFormat);

							mti.supportsDynamicMappings = (sfi->getChannelsCount() > 0);
							mti.streamPortType = la::avdecc::entity::model::DescriptorType::StreamPortInput;
							mti.streamPortIndex = spi;
							break;
						}
					}
				}
			}
			else if (streamNode.descriptorType == la::avdecc::entity::model::DescriptorType::StreamOutput)
			{
				for (auto const& [audioUnitIndex, audioUnitNode] : controlledEntity.getCurrentConfigurationNode().audioUnits)
				{
					for (auto const& [spi, streamPortNode] : audioUnitNode.streamPortOutputs)
					{
						if (streamPortNode.staticModel->hasDynamicAudioMap)
						{
							auto const& streamOutputNode = static_cast<la::avdecc::controller::model::StreamOutputNode const&>(streamNode);
							auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamOutputNode.dynamicModel->streamFormat);

							mti.supportsDynamicMappings = (sfi->getChannelsCount() > 0);
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

	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto const entityID = node->entityID();
		if (auto controlledEntity = manager.getControlledEntity(entityID))
		{
			auto const& entityNode = controlledEntity->getEntityNode();
			la::avdecc::controller::model::StreamNode const* streamNode{ nullptr };
			la::avdecc::entity::model::StreamIndex streamIndex = la::avdecc::entity::model::getInvalidDescriptorIndex();

			QMenu menu;
			addHeaderAction(menu, "Entity: " + avdecc::helper::smartEntityName(*controlledEntity));

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

				auto mti = findMappingssupportTypeIndexForStreamNode(*controlledEntity, *streamNode);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec()
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
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, streamIndex);
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

				auto mti = findMappingssupportTypeIndexForStreamNode(*controlledEntity, *streamNode);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec()
				controlledEntity.reset();

				if (auto* action = menu.exec(event->globalPos()))
				{
					if (action == editMappingsAction)
					{
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, streamIndex);
					}
				}
			}
			else if (node->isEntityNode())
			{
				menu.addSeparator();

				auto mti = findMappingssupportTypeIndexForDescriptor(*controlledEntity, isListenersHeader() ? la::avdecc::entity::model::DescriptorType::StreamInput : la::avdecc::entity::model::DescriptorType::StreamOutput);

				auto* editMappingsAction = addAction(menu, "Edit Dynamic Mappings", mti.supportsDynamicMappings);

				menu.addSeparator();

				// Release the controlled entity before starting a long operation (menu.exec()
				controlledEntity.reset();

				if (auto* action = menu.exec(event->globalPos()))
				{
					if (action == editMappingsAction)
					{
						handleEditMappingsClicked(entityID, mti.streamPortType, mti.streamPortIndex, la::avdecc::entity::model::getInvalidDescriptorIndex());
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
	try
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		auto controlledEntity = manager.getControlledEntity(entityID);

		if (controlledEntity)
		{
			auto const* const entity = controlledEntity.get();
			auto const& entityNode = entity->getEntityNode();
			auto const& configurationNode = entity->getConfigurationNode(entityNode.dynamicModel->currentConfiguration);
			mappingMatrix::Nodes outputs;
			mappingMatrix::Nodes inputs;
			mappingMatrix::Connections connections;
			avdecc::mappingsHelper::NodeMappings streamMappings;
			avdecc::mappingsHelper::NodeMappings clusterMappings;

			auto const isValidStream = [](auto const* const streamNode)
			{
				auto const sfi = la::avdecc::entity::model::StreamFormatInfo::create(streamNode->dynamicModel->streamFormat);
				auto const formatType = sfi->getType();

				if (formatType == la::avdecc::entity::model::StreamFormatInfo::Type::None || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::Unsupported || formatType == la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference)
					return false;

				return true;
			};

			if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
			{
				auto const& streamPortNode = entity->getStreamPortInputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamInputNode const*> streamNodes;

				if (streamIndex != la::avdecc::entity::model::getInvalidDescriptorIndex())
				{
					// Insert single StreamInput to list
					auto const& streamNode = configurationNode.streamInputs.at(streamIndex);
					if (!streamNode.isRedundant && isValidStream(&streamNode))
						streamNodes.push_back(&streamNode);

					// Insert single primary stream of a Redundant Set to list
					for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
					{
						auto const& redundantStreamNode = redundantStreamKV.second;
						auto const* const primRedundantStreamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);

						// we use the redundantStreams to check if our current streamIndex is either prim or sec of it. For mappings edit, we only use prim though.
						if (redundantStreamNode.redundantStreams.count(streamIndex) && isValidStream(primRedundantStreamNode))
						{
							streamNodes.push_back(primRedundantStreamNode);
							break;
						}
					}
				}
				else
				{
					// Build list of StreamInput
					for (auto const& streamKV : configurationNode.streamInputs)
					{
						auto const& streamNode = streamKV.second;
						if (!streamNode.isRedundant && isValidStream(&streamNode))
						{
							streamNodes.push_back(&streamNode);
						}
					}

					// Add primary stream of a Redundant Set
					for (auto const& redundantStreamKV : configurationNode.redundantStreamInputs)
					{
						auto const& redundantStreamNode = redundantStreamKV.second;
						auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
						if (isValidStream(streamNode))
						{
							streamNodes.push_back(streamNode);
						}
					}
				}

				// Build mappingMatrix vectors
				auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
				clusterMappings = std::move(clusterResult.first);
				inputs = std::move(clusterResult.second);
				auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
				streamMappings = std::move(streamResult.first);
				outputs = std::move(streamResult.second);
				connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
					[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
					{
						return std::make_pair(streamSlotID, clusterSlotID);
					});
			}
			else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
			{
				auto const& streamPortNode = entity->getStreamPortOutputNode(entityNode.dynamicModel->currentConfiguration, streamPortIndex);
				std::vector<la::avdecc::controller::model::StreamOutputNode const*> streamNodes;

				if (streamIndex != la::avdecc::entity::model::getInvalidDescriptorIndex())
				{
					// Insert single StreamOutput to list
					auto const& streamNode = configurationNode.streamOutputs.at(streamIndex);
					if (!streamNode.isRedundant && isValidStream(&streamNode))
						streamNodes.push_back(&streamNode);

					// Insert single primary stream of a Redundant Set to list
					for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
					{
						auto const& redundantStreamNode = redundantStreamKV.second;
						auto const* const primRedundantStreamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);

						// we use the redundantStreams to check if our current streamIndex is either prim or sec of it. For mappings edit, we only use prim though.
						if (redundantStreamNode.redundantStreams.count(streamIndex) && isValidStream(primRedundantStreamNode))
						{
							streamNodes.push_back(primRedundantStreamNode);
							break;
						}
					}
				}
				else
				{
					// Build list of StreamOutput
					for (auto const& streamKV : configurationNode.streamOutputs)
					{
						auto const& streamNode = streamKV.second;
						if (!streamNode.isRedundant && isValidStream(&streamNode))
						{
							streamNodes.push_back(&streamNode);
						}
					}

					// Add primary stream of a Redundant Set
					for (auto const& redundantStreamKV : configurationNode.redundantStreamOutputs)
					{
						auto const& redundantStreamNode = redundantStreamKV.second;
						auto const* const streamNode = static_cast<decltype(streamNodes)::value_type>(redundantStreamNode.primaryStream);
						if (isValidStream(streamNode))
						{
							streamNodes.push_back(streamNode);
						}
					}
				}

				// Build mappingMatrix vectors
				auto clusterResult = avdecc::mappingsHelper::buildClusterMappings(entity, streamPortNode);
				clusterMappings = std::move(clusterResult.first);
				outputs = std::move(clusterResult.second);
				auto streamResult = avdecc::mappingsHelper::buildStreamMappings(entity, streamNodes);
				streamMappings = std::move(streamResult.first);
				inputs = std::move(streamResult.second);
				connections = buildConnections(streamPortNode, streamNodes, streamMappings, clusterMappings,
					[](mappingMatrix::SlotID const streamSlotID, mappingMatrix::SlotID const clusterSlotID)
					{
						return std::make_pair(clusterSlotID, streamSlotID);
					});
			}
			else
			{
				AVDECC_ASSERT(false, "Should not happen");
			}

			if (!outputs.empty() && !inputs.empty())
			{
				auto smartName = avdecc::helper::smartEntityName(*entity);

				// Release the controlled entity before starting a long operation (dialog.exec)
				controlledEntity.reset();

				// Get exclusive access
				manager.requestExclusiveAccess(entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType::Lock,
					[this, streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections), entityID, streamPortType, streamPortIndex](auto const /*entityID*/, auto const status, auto&& token)
					{
						// Moving the token to the capture will effectively extend the lifetime of the token, keeping the entity locked until the lambda completes (meaning the dialog has been closed and mappings changed)
						QMetaObject::invokeMethod(this,
							[this, status, token = std::move(token), streamMappings = std::move(streamMappings), clusterMappings = std::move(clusterMappings), smartName = std::move(smartName), outputs = std::move(outputs), inputs = std::move(inputs), connections = std::move(connections), entityID, streamPortType, streamPortIndex]()
							{
								// Failed to get the exclusive access
								if (!status || !token)
								{
									// If the device does not support the exclusive access, still proceed
									if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotImplemented && status != la::avdecc::entity::ControllerEntity::AemCommandStatus::NotSupported)
									{
										QMessageBox::warning(nullptr, QString(""), QString("Failed to get Exclusive Access on %1:<br>%2").arg(smartName).arg(QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status))));
										return;
									}
								}

								// Create the dialog
								auto title = QString("%1 - %2.%3 Dynamic Mappings").arg(smartName).arg(avdecc::helper::descriptorTypeToString(streamPortType)).arg(streamPortIndex);
								auto dialog = mappingMatrix::MappingMatrixDialog{ title, outputs, inputs, connections };

								if (dialog.exec() == QDialog::Accepted)
								{
									if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortInput)
									{
										avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortInput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
									}
									else if (streamPortType == la::avdecc::entity::model::DescriptorType::StreamPortOutput)
									{
										avdecc::mappingsHelper::processNewConnections<la::avdecc::entity::model::DescriptorType::StreamPortOutput>(entityID, streamPortIndex, streamMappings, clusterMappings, connections, dialog.connections());
									}
								}
							});
					});
			}
		}
	}
	catch (...)
	{
	}
}

} // namespace connectionMatrix
