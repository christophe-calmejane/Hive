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

#include <QHBoxLayout>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace
{
QString networkName(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex)
{
	// Milan redundancy defines AVB interface index 0 as the primary network and index 1 as the secondary network
	switch (avbInterfaceIndex)
	{
		case 0u:
			return "Primary";
		case 1u:
			return "Secondary";
		default:
			return QString{ "Network %1" }.arg(avbInterfaceIndex);
	}
}
} // namespace

NetworkGraphView::NetworkGraphView(QWidget* parent)
	: QWidget{ parent }
{
	_tabWidget = new QTabWidget{ this };
	_tabWidget->setDocumentMode(true);

	_relayoutButton.setToolTip("Re-layout the graph");
	_fitButton.setToolTip("Zoom to fit");
	_clearHighlightButton.setToolTip("Clear stream highlight (Esc)");
	_streamInfoButton.setToolTip("Show/hide stream bandwidth and latency information");
	_streamInfoButton.setCheckable(true);
	_streamInfoButton.setChecked(true);

	auto* const toolbarLayout = new QHBoxLayout{};
	toolbarLayout->setContentsMargins(2, 2, 2, 2);
	toolbarLayout->addWidget(&_relayoutButton);
	toolbarLayout->addWidget(&_fitButton);
	toolbarLayout->addWidget(&_clearHighlightButton);
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
			_tabWidget->insertTab(static_cast<int>(networkIndex), pane, networkName(network.avbInterfaceIndex));
			pane->selectEntity(_selectedEntityID);
			pane->setShowStreamInfo(_streamInfoButton.isChecked());
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
}
