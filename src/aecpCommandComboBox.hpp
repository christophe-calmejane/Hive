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

#pragma once

#include <hive/modelsLibrary/controllerManager.hpp>
#include <QtMate/widgets/comboBox.hpp>

// ComboBox that watches an Aecp command result, restoring the previous index if the command fails
class AecpCommandComboBoxPrivate;
class AecpCommandComboBox : public qtMate::widgets::ComboBox
{
	Q_OBJECT

public:
	AecpCommandComboBox(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QWidget* parent = nullptr);
	~AecpCommandComboBox();

protected:
	virtual void showPopup() override;

private:
	AecpCommandComboBoxPrivate* const d_ptr{ nullptr };
	Q_DECLARE_PRIVATE(AecpCommandComboBox);
};
