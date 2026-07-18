/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "networkGraphView.hpp"
#include "networkGraphPane.hpp"

#include "settingsManager/settings.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace
{
QString networkName(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::MilanVersion const& milanVersion)
{
	if (auto const interfaceType = hive::modelsLibrary::helper::redundantInterfaceType(avbInterfaceIndex, milanVersion))
	{
		return hive::modelsLibrary::helper::interfaceTypeName(*interfaceType);
	}
	return QString{ "Network %1" }.arg(avbInterfaceIndex);
}

NetworkGraphPane::LayoutMode layoutModeFromAggregated(bool const aggregated)
{
	return aggregated ? NetworkGraphPane::LayoutMode::AggregatedBySwitch : NetworkGraphPane::LayoutMode::Detailed;
}

// The native flat style paints no background for the checked state, making it impossible to tell which
// checkable button is active: use an explicit palette driven style (valid in both light and dark themes)
auto const ToolbarButtonStyle = QStringLiteral("QPushButton { border: none; background: transparent; padding: 3px; }"
																							 "QPushButton:hover { background-color: palette(midlight); border-radius: 4px; }"
																							 "QPushButton:pressed { background-color: palette(mid); border-radius: 4px; }"
																							 "QPushButton:checked { background-color: palette(highlight); color: palette(highlighted-text); border-radius: 4px; }");

QFrame* createToolbarSeparator(QWidget* parent)
{
	auto* const separator = new QFrame{ parent };
	separator->setFrameShape(QFrame::VLine);
	separator->setFrameShadow(QFrame::Sunken);
	return separator;
}
} // namespace

NetworkGraphView::NetworkGraphView(QWidget* parent)
	: QWidget{ parent }
{
	_tabWidget = new QTabWidget{ this };
	_tabWidget->setDocumentMode(true);

	// Layout choice (radio like buttons), persisted in the settings
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	auto const aggregatedLayout = settings->getValue(settings::NetworkGraph_AggregatedLayout.name).toBool();
	_detailedLayoutButton.setToolTip("Detailed layout: one node per device");
	_aggregatedLayoutButton.setToolTip("Aggregated layout: entities grouped inside the bridge they are attached to");
	_detailedLayoutButton.setCheckable(true);
	_aggregatedLayoutButton.setCheckable(true);
	auto* const layoutModeGroup = new QButtonGroup{ this };
	layoutModeGroup->addButton(&_detailedLayoutButton);
	layoutModeGroup->addButton(&_aggregatedLayoutButton);
	_detailedLayoutButton.setChecked(!aggregatedLayout);
	_aggregatedLayoutButton.setChecked(aggregatedLayout);

	_relayoutButton.setToolTip("Re-layout the graph");
	_fitButton.setToolTip("Zoom to fit");
	_clearHighlightButton.setToolTip("Clear stream highlight (Esc)");
	// Highlighted (checked) when a stream highlight is active, showing the button has an effect. The checked
	// state is entirely driven by the panes highlightChanged signal, the click toggle is always overridden.
	_clearHighlightButton.setCheckable(true);
	_streamInfoButton.setToolTip("Show/Hide link information");
	_streamInfoButton.setCheckable(true);
	_streamInfoButton.setChecked(true);

	for (auto* const button : { &_relayoutButton, &_fitButton, &_clearHighlightButton, &_detailedLayoutButton, &_aggregatedLayoutButton, &_streamInfoButton })
	{
		button->setStyleSheet(ToolbarButtonStyle);
	}

	// Three sections: graph actions, layout choice, link information visibility
	auto* const toolbarLayout = new QHBoxLayout{};
	toolbarLayout->setContentsMargins(2, 2, 2, 2);
	toolbarLayout->addWidget(&_relayoutButton);
	toolbarLayout->addWidget(&_fitButton);
	toolbarLayout->addWidget(&_clearHighlightButton);
	toolbarLayout->addWidget(createToolbarSeparator(this));
	toolbarLayout->addWidget(&_detailedLayoutButton);
	toolbarLayout->addWidget(&_aggregatedLayoutButton);
	toolbarLayout->addWidget(createToolbarSeparator(this));
	toolbarLayout->addWidget(&_streamInfoButton);
	toolbarLayout->addStretch();
	toolbarLayout->addWidget(&_statsLabel);

	auto* const layout = new QVBoxLayout{ this };
	layout->setContentsMargins(2, 2, 2, 2);
	layout->addLayout(toolbarLayout);
	layout->addWidget(_tabWidget);

	connect(&_topologyModel, &hive::modelsLibrary::NetworkTopologyModel::topologyChanged, this,
		[this]()
		{
			rebuildPanes();
		});
	connect(_tabWidget, &QTabWidget::currentChanged, this,
		[this](int)
		{
			refreshStats();
			refreshClearHighlightButton();
		});
	connect(&_relayoutButton, &QPushButton::clicked, this,
		[this]()
		{
			if (auto* const pane = currentPane())
			{
				pane->relayout();
			}
		});
	connect(&_fitButton, &QPushButton::clicked, this,
		[this]()
		{
			if (auto* const pane = currentPane())
			{
				pane->fitToContents();
			}
		});
	connect(&_clearHighlightButton, &QPushButton::clicked, this,
		[this]()
		{
			if (auto* const pane = currentPane())
			{
				pane->clearHighlight();
			}
			// Cancel the click toggle: the checked state only reflects the actual highlight state
			refreshClearHighlightButton();
		});
	connect(&_streamInfoButton, &QPushButton::toggled, this,
		[this](bool const checked)
		{
			_streamInfoButton.setText(checked ? "label" : "label_off");
			for (auto const& [avbInterfaceIndex, pane] : _panes)
			{
				pane->setShowStreamInfo(checked);
			}
		});
	// The two layout buttons being mutually exclusive, observing the 'aggregated' one is enough
	connect(&_aggregatedLayoutButton, &QPushButton::toggled, this,
		[this](bool const checked)
		{
			auto* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
			settings->setValue(settings::NetworkGraph_AggregatedLayout.name, checked);
			for (auto const& [avbInterfaceIndex, pane] : _panes)
			{
				pane->setLayoutMode(layoutModeFromAggregated(checked));
			}
		});

	auto* const escapeShortcut = new QShortcut{ QKeySequence{ Qt::Key_Escape }, this };
	escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
	connect(escapeShortcut, &QShortcut::activated, this,
		[this]()
		{
			if (auto* const pane = currentPane())
			{
				pane->clearHighlight();
			}
		});
}

void NetworkGraphView::selectEntity(la::avdecc::UniqueIdentifier const entityID)
{
	if (entityID == _selectedEntityID)
	{
		return;
	}
	_selectedEntityID = entityID;
	for (auto const& [avbInterfaceIndex, pane] : _panes)
	{
		pane->selectEntity(entityID);
	}
}

NetworkGraphPane* NetworkGraphView::currentPane() const
{
	auto const tabIndex = _tabWidget->currentIndex();
	if (tabIndex >= 0 && static_cast<std::size_t>(tabIndex) < _panes.size())
	{
		return _panes[static_cast<std::size_t>(tabIndex)].second;
	}
	return nullptr;
}

void NetworkGraphView::refreshStats()
{
	if (auto* const pane = currentPane())
	{
		_statsLabel.setText(pane->statsText());
	}
	else
	{
		_statsLabel.clear();
	}
}

void NetworkGraphView::refreshClearHighlightButton()
{
	auto* const pane = currentPane();
	_clearHighlightButton.setChecked(pane != nullptr && pane->hasHighlight());
}

void NetworkGraphView::rebuildPanes()
{
	auto const& networks = _topologyModel.networks();
	auto const previousNetworkIndex = currentPane() ? std::optional<la::avdecc::entity::model::AvbInterfaceIndex>{ _panes[static_cast<std::size_t>(_tabWidget->currentIndex())].first } : std::nullopt;

	// Synchronize the tabs with the networks (networks are ordered by AVB interface index and rarely change)
	// Remove panes whose network disappeared
	for (auto paneIndex = static_cast<int>(_panes.size()) - 1; paneIndex >= 0; --paneIndex)
	{
		auto const avbInterfaceIndex = _panes[static_cast<std::size_t>(paneIndex)].first;
		auto const stillExists = std::any_of(networks.begin(), networks.end(),
			[avbInterfaceIndex](auto const& network)
			{
				return network.avbInterfaceIndex == avbInterfaceIndex;
			});
		if (!stillExists)
		{
			auto* const pane = _panes[static_cast<std::size_t>(paneIndex)].second;
			_tabWidget->removeTab(paneIndex);
			_panes.erase(_panes.begin() + paneIndex);
			pane->deleteLater();
		}
	}

	// Create/update panes, keeping them ordered by AVB interface index (same order than the networks vector)
	for (auto networkIndex = std::size_t{ 0u }; networkIndex < networks.size(); ++networkIndex)
	{
		auto const& network = networks[networkIndex];
		if (networkIndex >= _panes.size() || _panes[networkIndex].first != network.avbInterfaceIndex)
		{
			auto* const pane = new NetworkGraphPane{ this };
			connect(pane, &NetworkGraphPane::highlightChanged, this,
				[this](bool const)
				{
					refreshClearHighlightButton();
				});
			connect(pane, &NetworkGraphPane::entitySelectionChanged, this,
				[this](la::avdecc::UniqueIdentifier const entityID)
				{
					if (entityID != _selectedEntityID)
					{
						_selectedEntityID = entityID;
						// Propagate to the other panes so the same entity is selected everywhere
						for (auto const& [avbInterfaceIndex, otherPane] : _panes)
						{
							otherPane->selectEntity(entityID);
						}
						emit entitySelectionChanged(entityID);
					}
				});
			_panes.insert(_panes.begin() + static_cast<std::ptrdiff_t>(networkIndex), std::make_pair(network.avbInterfaceIndex, pane));
			_tabWidget->insertTab(static_cast<int>(networkIndex), pane, networkName(network.avbInterfaceIndex, network.milanVersion));
			pane->selectEntity(_selectedEntityID);
			pane->setShowStreamInfo(_streamInfoButton.isChecked());
			pane->setLayoutMode(layoutModeFromAggregated(_aggregatedLayoutButton.isChecked()));
		}
		_panes[networkIndex].second->setTopology(network.topology);
	}

	// Restore the previously displayed network when possible
	if (previousNetworkIndex)
	{
		for (auto paneIndex = std::size_t{ 0u }; paneIndex < _panes.size(); ++paneIndex)
		{
			if (_panes[paneIndex].first == *previousNetworkIndex)
			{
				_tabWidget->setCurrentIndex(static_cast<int>(paneIndex));
				break;
			}
		}
	}

	refreshStats();
	refreshClearHighlightButton();
}
