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

#include "connectionMatrix/legendDialog.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "internals/config.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QScreen>

namespace connectionMatrix
{
using DrawFunction = std::function<void(QPainter*, QRect const&, Model::IntersectionData::Capabilities const&)>;

// We use a widget over a simple pixmap so that the device pixel ratio is handled automatically
class CapabilitiesLabel : public QLabel
{
public:
	CapabilitiesLabel(DrawFunction const& drawFunction, Model::IntersectionData::Capabilities const& capabilities, QWidget* parent = nullptr)
		: QLabel{ parent }
		, _drawFunction{ drawFunction }
		, _capabilities{ capabilities }
	{
		setFixedSize(20, 20);
	}

private:
	virtual void paintEvent(QPaintEvent* event) override
	{
		Q_UNUSED(event);

		QPainter painter{ this };
		_drawFunction(&painter, rect(), _capabilities);
	}

private:
	DrawFunction const _drawFunction;
	Model::IntersectionData::Capabilities const _capabilities;
};

LegendDialog::LegendDialog(QWidget* parent)
	: QDialog{ parent }
{
	setWindowTitle(hive::internals::applicationShortName + " - " + "Connection matrix legend");

	using Sections = std::vector<std::tuple<QString, DrawFunction, Model::IntersectionData::Capabilities>>;

	// Add section
	auto const addSection = [this](QString const& title, Sections const& sections)
	{
		auto* sectionGroupBox = new QGroupBox{ title, this };
		sectionGroupBox->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);

		auto* sectionLayout = new QGridLayout{ sectionGroupBox };

		for (auto const& section : sections)
		{
			auto const& sectionTitle = std::get<0>(section);
			auto const& sectionDrawFunction = std::get<1>(section);
			auto const& sectionCapabilities = std::get<2>(section);

			auto const row = sectionLayout->rowCount();

			auto* capabilitiesLabel = new CapabilitiesLabel{ sectionDrawFunction, sectionCapabilities, sectionGroupBox };
			sectionLayout->addWidget(capabilitiesLabel, row, 0);

			auto* descriptionLabel = new QLabel{ sectionTitle, sectionGroupBox };
			sectionLayout->addWidget(descriptionLabel, row, 1);
		}

		_layout.addWidget(sectionGroupBox);
	};

	auto drawEntityConnectionSummary = std::bind(&paintHelper::drawEntityConnectionSummary, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	auto drawStreamConnectionStatus = std::bind(&paintHelper::drawStreamConnectionStatus, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	auto drawIndividualRedundantStreamStatus = std::bind(&paintHelper::drawIndividualRedundantStreamStatus, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

	Sections shapeSections = {
		{ "Entity connection summary (Not working yet)", drawEntityConnectionSummary, {} },
		{ "Connection status for a Simple or Redundant stream", drawStreamConnectionStatus, {} },
		{ "Connection status for the individual stream of a Redundant Stream Pair", drawIndividualRedundantStreamStatus, {} },
	};

	Sections colorCodeSections = {
		{ "Connectable without detectable error", drawStreamConnectionStatus, {} },
		{ "Connectable but incompatible AVB domain", drawStreamConnectionStatus, {} },
		{ "Connectable but incompatible stream format", drawStreamConnectionStatus, {} },
		{ "Connectable but at least one Network Interface is down", drawStreamConnectionStatus, {} },
		{ "Connectable Redundant Stream Pair but at least one error detected", drawStreamConnectionStatus, {} },

		{ "Connected and no detectable error found", drawStreamConnectionStatus, {} },
		{ "Connected but incompatible AVB domain", drawStreamConnectionStatus, {} },
		{ "Connected but incompatible stream format", drawStreamConnectionStatus, {} },
		{ "Connected but Network Interface is down", drawStreamConnectionStatus, {} },
		{ "Partially connected Redundant Stream Pair", drawStreamConnectionStatus, {} },
		{ "Redundant Stream Pair connected but at least one error detected", drawStreamConnectionStatus, {} },
	};

	addSection("Shapes", shapeSections);
	addSection("Color codes", colorCodeSections);

	connect(&_closeButton, &QPushButton::clicked, this, &QDialog::accept);
	_layout.addWidget(&_closeButton);
}

} // namespace connectionMatrix
