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

#include "connectionMatrix/legendDialog.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/paintHelper.hpp"
#include "internals/config.hpp"

#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QApplication>

namespace connectionMatrix
{
// We use a widget over a simple pixmap so that the device pixel ratio is handled automatically
class HeaderArrowLabel : public QLabel
{
public:
	HeaderArrowLabel(QColor const& color, Qt::Orientation const orientation, bool const isTransposed, QWidget* parent = nullptr)
		: QLabel{ parent }
		, _color{ color }
		, _orientation{ orientation }
		, _isTransposed{ isTransposed }
	{
		setFixedSize(20, 20);
	}

private:
	virtual void paintEvent(QPaintEvent* event) override
	{
		Q_UNUSED(event);
		QPainter painter{ this };
		auto const path = paintHelper::buildHeaderArrowPath(rect(), _orientation, _isTransposed, false, false, 3, 10, 5);
		painter.fillPath(path, _color);
	}

private:
	QColor const _color{};
	Qt::Orientation const _orientation{ Qt::Orientation::Horizontal };
	bool const _isTransposed{ false };
};

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
		paintHelper::drawCapabilities(&painter, rect(), _type, _state, _flags, true, true);
	}

private:
	Model::IntersectionData::Type const _type;
	Model::IntersectionData::State const _state;
	Model::IntersectionData::Flags const _flags;
};

LegendDialog::LegendDialog(qtMate::material::color::Name const& colorName, bool const isTransposed, QWidget* parent)
	: QDialog{ parent }
{
	// Configure settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->registerSettingObserver(settings::ConnectionMatrix_ShowMediaLockedDot.name, this);

	// Configure UI
	setWindowTitle(hive::internals::applicationShortName + " - " + "Connection Matrix Legend");

	using Section = std::tuple<QString, Model::IntersectionData::Type, Model::IntersectionData::State, Model::IntersectionData::Flags, bool>;
	using Sections = std::vector<Section>;

	// Add section helper lambda
	auto const addSection = [this](QString const& title, Sections const& sections)
	{
		auto* sectionGroupBox = new QGroupBox{ title, this };
		sectionGroupBox->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);

		auto* sectionLayout = new QGridLayout{ sectionGroupBox };

		for (auto const& [sectionTitle, sectionType, sectionState, sectionFlags, isMediaLockedDot] : sections)
		{
			if (!isMediaLockedDot || _drawMediaLockedDot)
			{
				auto const row = sectionLayout->rowCount();

				auto* capabilitiesLabel = new CapabilitiesLabel{ sectionType, sectionState, sectionFlags, sectionGroupBox };
				sectionLayout->addWidget(capabilitiesLabel, row, 0);

				auto* descriptionLabel = new QLabel{ sectionTitle, sectionGroupBox };
				sectionLayout->addWidget(descriptionLabel, row, 1);
			}
		}

		_layout.addWidget(sectionGroupBox);
	};

	Sections shapeSections = {
		{ "Entity connection summary (Not working yet)", Model::IntersectionData::Type::Entity_Entity, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{}, false },
		{ "Connection status for a Simple stream", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{}, false },
		{ "Redundant Stream Pair connection summary", Model::IntersectionData::Type::Redundant_Redundant, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{}, false },
		{ "Connection status for the individual stream of a Redundant Stream Pair", Model::IntersectionData::Type::RedundantStream_RedundantStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{}, false },
	};

	Sections colorCodeSections = {
		{ "Connectable without detectable error", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{}, false },
		{ "Connectable but incompatible AVB domain", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongDomain }, false },
		{ "Connectable but Listener stream format must be changed to match Talker's current one", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongFormatPossible }, false },
		{ "Connectable but no compatible Listener stream format exists to match Talker's current one", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongFormatImpossible }, false },
		{ "Connectable but at least one Network Interface is down", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::NotConnected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::InterfaceDown }, false },

		{ "Connected and no detectable error found", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{}, false },
		{ "Connected and Media Locked (Milan Only)", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::MediaLocked }, true },
		{ "Connected but incompatible AVB domain", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongDomain }, false },
		{ "Connected but incompatible stream format", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::WrongFormatPossible }, false },
		{ "Connected but at least one Network Interface is down", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::InterfaceDown }, false },
		{ "Connected but Talker not detected on the Network (probably Offline)", Model::IntersectionData::Type::OfflineOutputStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{}, false },
		{ "Connected and Media Locked but Talker not detected on the Network (but Online)", Model::IntersectionData::Type::OfflineOutputStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::MediaLocked }, true },
		{ "Connected but MSRP Latency exceeds Presentation Time", Model::IntersectionData::Type::SingleStream_SingleStream, Model::IntersectionData::State::Connected, Model::IntersectionData::Flags{ Model::IntersectionData::Flag::LatencyError }, true },

		{ "Partially connected Redundant Stream Pair", Model::IntersectionData::Type::Redundant_Redundant, Model::IntersectionData::State::PartiallyConnected, Model::IntersectionData::Flags{}, false },
	};

	// Add a section for the Arrows
	{
		auto* sectionGroupBox = new QGroupBox{ "Header Small Arrows (Milan devices only)", this };
		sectionGroupBox->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);

		auto* sectionLayout = new QGridLayout{ sectionGroupBox };

		auto const arrowColor = qtMate::material::color::value(colorName, qtMate::material::color::Shade::Shade600);
		auto const errorArrowColor = qtMate::material::color::foregroundErrorColorValue(colorName, qtMate::material::color::Shade::Shade600);
		auto const talkerOrientation = isTransposed ? Qt::Orientation::Horizontal : Qt::Orientation::Vertical;
		auto const listenerOrientation = isTransposed ? Qt::Orientation::Vertical : Qt::Orientation::Horizontal;

		// Output Stream "isStreaming"
		{
			auto const row = sectionLayout->rowCount();
			auto* arrowLabel = new HeaderArrowLabel{ arrowColor, talkerOrientation, isTransposed };
			sectionLayout->addWidget(arrowLabel, row, 0);

			auto* descriptionLabel = new QLabel{ "[Output Stream Only] Currently Streaming", sectionGroupBox };
			sectionLayout->addWidget(descriptionLabel, row, 1);
		}

		// Input Stream "lockedState == false"
		{
			auto const row = sectionLayout->rowCount();
			auto* arrowLabel = new HeaderArrowLabel{ errorArrowColor, listenerOrientation, isTransposed };
			sectionLayout->addWidget(arrowLabel, row, 0);

			auto* descriptionLabel = new QLabel{ "[Input Stream Only] Connected but not Media Locked", sectionGroupBox };
			sectionLayout->addWidget(descriptionLabel, row, 1);
		}

		// Input Stream "lockedState == true"
		{
			auto const row = sectionLayout->rowCount();
			auto* arrowLabel = new HeaderArrowLabel{ arrowColor, listenerOrientation, isTransposed };
			sectionLayout->addWidget(arrowLabel, row, 0);

			auto* descriptionLabel = new QLabel{ "[Input Stream Only] Connected and Media Locked", sectionGroupBox };
			sectionLayout->addWidget(descriptionLabel, row, 1);
		}

		_layout.addWidget(sectionGroupBox);
	}

	// Add a section for the Shapes
	addSection("Intersection Shapes", shapeSections);

	// Add a section for the Colors
	addSection("Intersection Color codes", colorCodeSections);

	// Add a Close Button
	connect(&_closeButton, &QPushButton::clicked, this, &QDialog::accept);
	_layout.addWidget(&_closeButton);
}

LegendDialog::~LegendDialog()
{
	// Remove settings observers
	auto const* const settings = qApp->property(settings::SettingsManager::PropertyName).value<settings::SettingsManager*>();
	settings->unregisterSettingObserver(settings::ConnectionMatrix_ShowMediaLockedDot.name, this);
}

void LegendDialog::onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept
{
	if (name == settings::ConnectionMatrix_ShowMediaLockedDot.name)
	{
		auto const drawDot = value.toBool();
		_drawMediaLockedDot = drawDot;
	}
}

} // namespace connectionMatrix
