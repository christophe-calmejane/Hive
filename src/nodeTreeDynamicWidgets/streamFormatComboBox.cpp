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

#include "streamFormatComboBox.hpp"
#include "avdecc/helper.hpp"

#include <QInputDialog>

Q_DECLARE_METATYPE(la::avdecc::entity::model::StreamFormat)

StreamFormatComboBox::StreamFormatComboBox(la::avdecc::UniqueIdentifier const entityID, QWidget* parent)
	: AecpCommandComboBox(entityID, avdecc::ControllerManager::AecpCommandType::SetStreamFormat, parent)
{
	// Send changes
	connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int /*index*/)
		{
			auto streamFormat = currentData().value<StreamFormat>();
			auto streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
			if (streamFormatInfo->isUpToChannelsCount())
			{
				bool ok{ false };
				auto const channelCount = QInputDialog::getInt(this, "Number of channels", "Count", 1, 1, streamFormatInfo->getChannelsCount(), 1, &ok);

				if (ok)
				{
					streamFormat = streamFormatInfo->getAdaptedStreamFormat(channelCount);
				}
				else
				{
					streamFormat = _previousFormat;
				}
			}

			setCurrentStreamFormat(streamFormat);
			emit currentFormatChanged(streamFormat);
		});
}

void StreamFormatComboBox::setStreamFormats(StreamFormats const& streamFormats)
{
	QSignalBlocker lock(this); // Block internal signals so clear and addItem do not trigger "currentIndexChanged"

	clear();

	_streamFormats = streamFormats;

	for (auto const& streamFormat : _streamFormats)
	{
		auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
		addItem(avdecc::helper::streamFormatToString(*streamFormatInfo), QVariant::fromValue(streamFormat));
	}
}

void StreamFormatComboBox::setCurrentStreamFormat(StreamFormat const& streamFormat)
{
	QSignalBlocker lock(this); // Block internal signals so setCurrentText do not trigger "currentIndexChanged"

	auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
	auto const streamFormatString = avdecc::helper::streamFormatToString(*streamFormatInfo);

	// The format is not present in the list?
	if (_streamFormats.count(_previousFormat) == 0)
	{
		auto const index = findData(QVariant::fromValue(_previousFormat));

		if (index != -1)
		{
			removeItem(index);
		}
	}

	// We try to add a custom format
	if (_streamFormats.count(streamFormat) == 0)
	{
		addItem(streamFormatString, QVariant::fromValue(streamFormat));

		auto const index = findData(QVariant::fromValue(streamFormat));

		QFont font;
		font.setBold(true);
		font.setItalic(true);

		setItemData(index, font, Qt::FontRole);
	}

	_previousFormat = streamFormat;
	setCurrentText(streamFormatString);
}
