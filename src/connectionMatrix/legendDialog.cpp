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

namespace connectionMatrix
{
// We use a widget over a simple pixmap so that the device pixel ratio is handled automatically
class CapabilitiesLabel : public QLabel
{
public:
	CapabilitiesLabel(Model::IntersectionData::Type const type, Model::IntersectionData::State const& state, Model::IntersectionData::Flags const& flags, QWidget* parent = nullptr)
		: QLabel{ parent }
		, _type{ type }
		, _state{ state }
		, _flags{ flags }
	{
		setFixedSize(19, 19);
	}

private:
	virtual void paintEvent(QPaintEvent* event) override
	{
		Q_UNUSED(event);
		QPainter painter{ this };
		paintHelper::drawCapabilities(&painter, rect(), _type, _state, _flags);
	}

private:
	Model::IntersectionData::Type const _type;
	Model::IntersectionData::State const _state;
	Model::IntersectionData::Flags const _flags;
};

LegendDialog::LegendDialog(QWidget* parent)
	: QDialog{ parent }
{
	setWindowTitle(hive::internals::applicationShortName + " - " + "Connection matrix legend");

	using Section = std::tuple<QString, Model::IntersectionData::Type, Model::IntersectionData::State, Model::IntersectionData::Flags>;
	using Sections = std::vector<Section>;

	// Add section
	auto const addSection = [this](QString const& title, Sections const& sections)
	{
		auto* sectionGroupBox = new QGroupBox{ title, this };
		sectionGroupBox->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);

		auto* sectionLayout = new QGridLayout{ sectionGroupBox };

		for (auto const& section : sections)
		{
			auto const& sectionTitle = std::get<0>(section);
			auto const& sectionType = std::get<1>(section);
			auto const& sectionState = std::get<2>(section);
			auto const& sectionFlags = std::get<3>(section);

			auto const row = sectionLayout->rowCount();

			auto* capabilitiesLabel = new CapabilitiesLabel{ sectionType, sectionState, sectionFlags, sectionGroupBox };
			sectionLayout->addWidget(capabilitiesLabel, row, 0);

			auto* descriptionLabel = new QLabel{ sectionTitle, sectionGroupBox };
			sectionLayout->addWidget(descriptionLabel, row, 1);
		}

		_layout.addWidget(sectionGroupBox);
	};

	Sections shapeSections = {
		{ "Entity connection summary (Not working yet)", Model::IntersectionData::Type::Entity_Entity, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{} },
		{ "Connection status for a Simple or Redundant stream", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{} },
		{ "Connection status for the individual stream of a Redundant Stream Pair", Model::IntersectionData::Type::RedundantStream_RedundantStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{} },
	};

	Sections colorCodeSections = {
		{ "Connectable without detectable error", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{} },
		{ "Connectable but incompatible AVB domain", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongDomain } },
		{ "Connectable but incompatible stream format", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongFormat } },
		{ "Connectable but at least one Network Interface is down", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::InterfaceDown } },
		{ "Connectable Redundant Stream Pair but at least one error detected", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{} },

		{ "Connected and no detectable error found", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{} },
		{ "Connected but incompatible AVB domain", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongDomain } },
		{ "Connected but incompatible stream format", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongFormat } },
		{ "Connected but Network Interface is down", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::InterfaceDown } },
		{ "Partially connected Redundant Stream Pair", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{} },
		{ "Redundant Stream Pair connected but at least one error detected", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{} },
	};

	addSection("Shapes", shapeSections);
	addSection("Color codes", colorCodeSections);

	connect(&_closeButton, &QPushButton::clicked, this, &QDialog::accept);
	_layout.addWidget(&_closeButton);
}

} // namespace connectionMatrix
