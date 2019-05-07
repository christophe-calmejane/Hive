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

#include "connectionMatrix/view.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/node.hpp"
#include "connectionMatrix/headerView.hpp"
#include "connectionMatrix/itemDelegate.hpp"
#include "connectionMatrix/cornerWidget.hpp"
#include "avdecc/controllerManager.hpp"
#include "avdecc/helper.hpp"
#include "avdecc/hiveLogItems.hpp"

#include <QMouseEvent>
#include <QMessageBox>

namespace connectionMatrix
{
View::View(QWidget* parent)
	: QTableView{ parent }
	, _model{ std::make_unique<Model>() }
	, _horizontalHeaderView{ std::make_unique<HeaderView>(Qt::Horizontal, this) }
	, _verticalHeaderView{ std::make_unique<HeaderView>(Qt::Vertical, this) }
	, _itemDelegate{ std::make_unique<ItemDelegate>(this) }
	, _cornerWidget{ std::make_unique<CornerWidget>(this) }
{
	setModel(_model.get());
	setHorizontalHeader(_horizontalHeaderView.get());
	setVerticalHeader(_verticalHeaderView.get());
	setItemDelegate(_itemDelegate.get());

	setSelectionMode(QAbstractItemView::NoSelection);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setMouseTracking(true);

	// Take care of the top left corner widget
	setCornerButtonEnabled(false);
	stackUnder(_cornerWidget.get());

	// Apply filter when needed
	connect(_cornerWidget.get(), &CornerWidget::filterChanged, this, &View::onFilterChanged);

	connect(_cornerWidget.get(), &CornerWidget::horizontalExpandClicked, _horizontalHeaderView.get(), &HeaderView::expandAll);
	connect(_cornerWidget.get(), &CornerWidget::horizontalCollapseClicked, _horizontalHeaderView.get(), &HeaderView::collapseAll);

	connect(_cornerWidget.get(), &CornerWidget::verticalExpandClicked, _verticalHeaderView.get(), &HeaderView::expandAll);
	connect(_cornerWidget.get(), &CornerWidget::verticalCollapseClicked, _verticalHeaderView.get(), &HeaderView::collapseAll);

	// Make sure the corner widgets fits the available space
	auto const updateCornerWidgetGeometry = [this]()
	{
		_cornerWidget->setGeometry(0, 0, verticalHeader()->width(), horizontalHeader()->height());
	};

	connect(_verticalHeaderView.get(), &QHeaderView::geometriesChanged, this, updateCornerWidgetGeometry);
	connect(_horizontalHeaderView.get(), &QHeaderView::geometriesChanged, this, updateCornerWidgetGeometry);

	// Handle click on the table
	connect(this, &QTableView::clicked, this, &View::onIntersectionClicked);

	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.registerSettingObserver(settings::TransposeConnectionMatrix.name, this);
}

View::~View()
{
	// Configure settings observers
	auto& settings = settings::SettingsManager::getInstance();
	settings.unregisterSettingObserver(settings::TransposeConnectionMatrix.name, this);
}

void View::onIntersectionClicked(QModelIndex const& index)
{
	auto const& intersectionData = _model->intersectionData(index);

	auto const talkerID = intersectionData.talker->entityID();
	auto const listenerID = intersectionData.listener->entityID();

	auto& manager = avdecc::ControllerManager::getInstance();

	switch (intersectionData.type)
	{
		// Use SmartConnection algorithm
		case Model::IntersectionData::Type::RedundantStream_RedundantStream:
		case Model::IntersectionData::Type::RedundantStream_SingleStream:
		case Model::IntersectionData::Type::SingleStream_SingleStream:
		case Model::IntersectionData::Type::Redundant_Redundant:
		case Model::IntersectionData::Type::Redundant_RedundantStream:
		case Model::IntersectionData::Type::Redundant_SingleStream:
		{
			if (intersectionData.smartConnectableStreams.empty())
			{
				QMessageBox::information(this, "", "Couldn't detect a Stream of the Redundant Pair on the same AVB domain than the other Entity, cannot automatically change the stream connection.\n\nPlease expand the Redundant Pair and manually choose desired stream.");
			}
			else
			{
				auto doConnect{ false };
				auto doDisconnect{ false };

				if (intersectionData.state == Model::IntersectionData::State::NotConnected || intersectionData.state == Model::IntersectionData::State::PartiallyConnected)
				{
					doConnect = true;
				}
				else
				{
					doDisconnect = true;
				}

				auto* talker = static_cast<RedundantNode*>(intersectionData.talker);
				auto* listener = static_cast<RedundantNode*>(intersectionData.listener);

				for (auto const& connectableStream : intersectionData.smartConnectableStreams)
				{
					auto const talkerStream = la::avdecc::entity::model::StreamIdentification{ talkerID, connectableStream.talkerStreamIndex };
					auto const areConnected = connectableStream.isConnected || connectableStream.isFastConnecting;

					if (doConnect && !areConnected)
					{
						manager.connectStream(talkerID, connectableStream.talkerStreamIndex, listenerID, connectableStream.listenerStreamIndex);
					}
					else if (doDisconnect && areConnected)
					{
						manager.disconnectStream(talkerID, connectableStream.talkerStreamIndex, listenerID, connectableStream.listenerStreamIndex);
					}
					else
					{
						LOG_HIVE_TRACE(QString("connectionMatrix::View::onClicked: Neither connecting nor disconnecting: doConnect=%1 doDisconnect=%2 areConnected=%3").arg(doConnect).arg(doDisconnect).arg(areConnected));
					}
				}
			}
			break;
		}

		default:
			break;
	}
}

void View::onFilterChanged(QString const& filter)
{
	applyFilterPattern(QRegExp{ filter });
}

void View::applyFilterPattern(QRegExp const& pattern)
{
	_verticalHeaderView->setFilterPattern(pattern);
	_horizontalHeaderView->setFilterPattern(pattern);
}

void View::mouseMoveEvent(QMouseEvent* event)
{
	auto const index = indexAt(event->pos());
	selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Columns);
	QTableView::mouseMoveEvent(event);
}

void View::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::TransposeConnectionMatrix.name)
	{
		auto const transposed = value.toBool();

		auto const verticalSectionState = _verticalHeaderView->saveSectionState();
		auto const horizontalSectionState = _horizontalHeaderView->saveSectionState();

		_model->setTransposed(transposed);
		_cornerWidget->setTransposed(transposed);

		_verticalHeaderView->restoreSectionState(horizontalSectionState);
		_horizontalHeaderView->restoreSectionState(verticalSectionState);

		applyFilterPattern(QRegExp{ _cornerWidget->filterText() });
	}
}

} // namespace connectionMatrix
