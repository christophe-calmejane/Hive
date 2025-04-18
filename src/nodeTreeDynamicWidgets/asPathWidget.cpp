/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "asPathWidget.hpp"

#include <hive/modelsLibrary/helper.hpp>

AsPathWidget::AsPathWidget(la::avdecc::UniqueIdentifier const clockID, QString const& vendorName, QWidget* parent)
	: QWidget(parent)
{
	_layout.setContentsMargins(0, 0, 0, 0);

	_layout.addWidget(&_clockIDLabel, 1);
	_layout.addWidget(&_vendorNameLabel, 2);

	_clockIDLabel.setText(hive::modelsLibrary::helper::uniqueIdentifierToString(clockID));
	_vendorNameLabel.setText(vendorName);
}
