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

#pragma once

#include "aecpCommandComboBox.hpp"
#include "avdecc/helper.hpp"
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>

#include <QInputDialog>

using StreamFormatComboBox_t = la::avdecc::entity::model::StreamFormat;

Q_DECLARE_METATYPE(StreamFormatComboBox_t)

class StreamFormatComboBox final : public AecpCommandComboBox<StreamFormatComboBox_t>
{
public:
	StreamFormatComboBox(QWidget* parent = nullptr)
		: AecpCommandComboBox<StreamFormatComboBox_t>{ parent }
	{
		// Handle index change
		setIndexChangedHandler(
			[this](StreamFormatComboBox_t const& streamFormat)
			{
				auto format = streamFormat;
				auto streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
				if (streamFormatInfo->isUpToChannelsCount())
				{
					bool ok{ false };
					auto const channelCount = QInputDialog::getInt(this, "Number of channels", "Count", 1, 1, streamFormatInfo->getChannelsCount(), 1, &ok);

					if (ok)
					{
						format = streamFormatInfo->getAdaptedStreamFormat(channelCount);
					}
					else
					{
						format = getCurrentData();
					}
				}
				return format;
			});
	}

	void setCurrentStreamFormat(StreamFormatComboBox_t const& streamFormat)
	{
		setCurrentData(streamFormat);
	}

	void setStreamFormats(Data const& streamFormats) noexcept
	{
		AecpCommandComboBox<StreamFormatComboBox_t>::setAllData(streamFormats,
			[](StreamFormatComboBox_t const& streamFormat)
			{
				auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
				return avdecc::helper::streamFormatToString(*streamFormatInfo);
			});
	}

	StreamFormatComboBox_t const& getCurrentStreamFormat() const noexcept
	{
		return getCurrentData();
	}

private:
	using AecpCommandComboBox<StreamFormatComboBox_t>::setIndexChangedHandler;
	using AecpCommandComboBox<StreamFormatComboBox_t>::setAllData;
	using AecpCommandComboBox<StreamFormatComboBox_t>::getCurrentData;

	virtual void setCurrentData(StreamFormatComboBox_t const& data) noexcept override
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals so setCurrentText do not trigger "currentIndexChanged"

		auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(data);
		auto const streamFormatString = avdecc::helper::streamFormatToString(*streamFormatInfo);

		// If the previous format is not part of the data, it was added as a temporary value and we have to remove it
		if (_data.count(_previousData) == 0)
		{
			auto const index = findData(QVariant::fromValue(_previousData));

			if (index != -1)
			{
				removeItem(index);
			}
		}

		// If the new format is not part of the data, we have to add it as a temporary value
		if (_data.count(data) == 0)
		{
			addItem(streamFormatString, QVariant::fromValue(data));

			auto const index = findData(QVariant::fromValue(data));

			QFont font;
			font.setItalic(true);

			setItemData(index, font, Qt::FontRole);
		}

		_previousData = data;
		setCurrentText(streamFormatString);
	}
};
