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

#include "loggerView.hpp"
#include "avdecc/helper.hpp"

#include <QScrollBar>
#include <QFileDialog>
#include <QStandardPaths>
#include <QShortcut>

class AutoScrollBar : public QScrollBar {
public:
	AutoScrollBar(QWidget* parent)
	: AutoScrollBar(Qt::Vertical, parent)
	{
	}

	AutoScrollBar(Qt::Orientation orientation, QWidget* parent)
	: QScrollBar(orientation, parent)
	{
		_bufferedMaximum = maximum();

		connect(this, &QScrollBar::rangeChanged, this, [this](int, int max)
		{
			if (value() == _bufferedMaximum) {
				setValue(max);
			}

			_bufferedMaximum = max;
		});
	}

private:
	int _bufferedMaximum{0};
};

const std::vector<la::avdecc::logger::Layer> loggerLayers
{
	la::avdecc::logger::Layer::Generic,
	la::avdecc::logger::Layer::Serialization,
	la::avdecc::logger::Layer::ProtocolInterface,
	la::avdecc::logger::Layer::AemPayload,
	la::avdecc::logger::Layer::ControllerEntity,
	la::avdecc::logger::Layer::ControllerStateMachine,
	la::avdecc::logger::Layer::Controller,
	la::avdecc::logger::Layer::FirstUserLayer,
};

const std::vector<la::avdecc::logger::Level> loggerLevels
{
	la::avdecc::logger::Level::Trace,
	la::avdecc::logger::Level::Debug,
	la::avdecc::logger::Level::Info,
	la::avdecc::logger::Level::Warn,
	la::avdecc::logger::Level::Error,
};

LoggerView::LoggerView(QWidget* parent)
	: QWidget(parent)
{
	setupUi(this);

	auto* logger = &la::avdecc::logger::Logger::getInstance();

#ifndef NDEBUG
	logger->setLevel(la::avdecc::logger::Level::Trace);
#else
	logger->setLevel(la::avdecc::logger::Level::Info);
#endif

	tableView->setModel(&_loggerModel);
	tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	tableView->setSelectionMode(QAbstractItemView::SingleSelection);
	tableView->setVerticalScrollBar(new AutoScrollBar{Qt::Vertical, this});

	_dynamicHeaderView.setHighlightSections(false);
	_dynamicHeaderView.setStretchLastSection(true);
	_dynamicHeaderView.setMandatorySection(3);

	tableView->setHorizontalHeader(&_dynamicHeaderView);

	tableView->setColumnWidth(0, 160);
	tableView->setColumnWidth(1, 120);
	tableView->setColumnWidth(2, 90);

	_layerFilterProxyModel.setSourceModel(&_loggerModel);
	_levelFilterProxyModel.setSourceModel(&_layerFilterProxyModel);
	_searchFilterProxyModel.setSourceModel(&_levelFilterProxyModel);
	tableView->setModel(&_searchFilterProxyModel);

	connect(actionClear, &QAction::triggered, &_loggerModel, &avdecc::LoggerModel::clear);
	connect(actionSave, &QAction::triggered, this, [this]()
	{
		auto const filename = QFileDialog::getSaveFileName(this, "Save As..", QString("%1/%2.txt").arg(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).arg(qAppName()), "*.txt");
		_loggerModel.save(filename);
	});

	connect(actionSearch, &QAction::triggered, this, [this]()
	{
		auto const pattern = searchLineEdit->text();
		_searchFilterProxyModel.setFilterKeyColumn(3);
		_searchFilterProxyModel.setFilterRegExp(pattern);
		_searchFilterProxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
	});

	auto* searchShortcut = new QShortcut{QKeySequence::Find, this};
	connect(searchShortcut, &QShortcut::activated, this, [this]()
	{
		searchLineEdit->setFocus(Qt::MouseFocusReason);
	});

	auto* saveShortcut = new QShortcut{QKeySequence::Save, this};
	connect(saveShortcut, &QShortcut::activated, actionSave, &QAction::trigger);

	createLayerFilterButton();
	createLevelFilterButton();
}

qt::toolkit::DynamicHeaderView* LoggerView::header() const
{
	return const_cast<qt::toolkit::DynamicHeaderView*>(&_dynamicHeaderView);
}

void LoggerView::createLayerFilterButton()
{
	for (auto const& layer : loggerLayers)
	{
		auto* action = _layerFilterMenu.addAction(avdecc::helper::loggerLayerToString(layer));
		action->setCheckable(true);
		action->setChecked(true);
	}

	_layerFilterMenu.addSeparator();
	_layerFilterMenu.addAction("All");
	_layerFilterMenu.addAction("None");

	layerFilterButton->setMenu(&_layerFilterMenu);

	connect(&_layerFilterMenu, &QMenu::triggered, this, [this](QAction* action)
	{
		// All & None are non checkable
		if (!action->isCheckable())
		{
			auto const checked = action->text() == "All";

			QSignalBlocker lock(&_layerFilterMenu);
			for (auto* a : _layerFilterMenu.actions())
			{
				if (a->isCheckable())
				{
					a->setChecked(checked);
				}
			}
		}

		QStringList layerList;
		for (auto* a : _layerFilterMenu.actions())
		{
			if (a->isCheckable() && a->isChecked())
			{
				layerList << a->text();
			}
		}

		if (layerList.empty())
		{
			// Invalid filter
			layerList << "---";
		}

		// Update the filter
		_layerFilterProxyModel.setFilterKeyColumn(1);
		_layerFilterProxyModel.setFilterRegExp(layerList.join('|'));
	});
}

void LoggerView::createLevelFilterButton()
{
	for (auto const& level : loggerLevels)
	{
		auto* action = _levelFilterMenu.addAction(avdecc::helper::loggerLevelToString(level));
		action->setCheckable(true);
		action->setChecked(true);
	}

	_levelFilterMenu.addSeparator();
	_levelFilterMenu.addAction("All");
	_levelFilterMenu.addAction("None");

	levelFilterButton->setMenu(&_levelFilterMenu);

	connect(&_levelFilterMenu, &QMenu::triggered, this, [this](QAction* action)
	{
		// All & None are non checkable
		if (!action->isCheckable())
		{
			auto const checked = action->text() == "All";

			QSignalBlocker lock(&_levelFilterMenu);
			for (auto* a : _levelFilterMenu.actions())
			{
				if (a->isCheckable())
				{
					a->setChecked(checked);
				}
			}
		}

		QStringList levelList;
		for (auto* a : _levelFilterMenu.actions())
		{
			if (a->isCheckable() && a->isChecked())
			{
				levelList << a->text();
			}
		}

		if (levelList.empty())
		{
			// Invalid filter
			levelList << "---";
		}

		// Update the filter
		_levelFilterProxyModel.setFilterKeyColumn(2);
		_levelFilterProxyModel.setFilterRegExp(levelList.join('|'));
	});
}
