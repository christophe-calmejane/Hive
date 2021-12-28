/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>

class StreamFormatComboBox final : public AecpCommandComboBox
{
	Q_OBJECT
public:
	using StreamFormat = la::avdecc::entity::model::StreamFormat;
	using StreamFormats = std::set<StreamFormat>;

	StreamFormatComboBox(la::avdecc::UniqueIdentifier const entityID, QWidget* parent = nullptr);

	void setStreamFormats(StreamFormats const& streamFormats);
	void setCurrentStreamFormat(StreamFormat const& streamFormat);

	StreamFormats const& getStreamFormats() const noexcept;
	StreamFormat const& getCurrentStreamFormat() const noexcept;

	Q_SIGNAL void currentFormatChanged(la::avdecc::entity::model::StreamFormat const previousStreamFormat, la::avdecc::entity::model::StreamFormat const newStreamFormat);

private:
	StreamFormats _streamFormats{};
	StreamFormat _previousFormat{ 0 };
};
