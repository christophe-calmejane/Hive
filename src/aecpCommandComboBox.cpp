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

#include "aecpCommandComboBox.hpp"

class AecpCommandComboBoxPrivate : public QObject
{
public:
	AecpCommandComboBoxPrivate(AecpCommandComboBox* q, la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType commandType)
		: q_ptr(q)
		, _entityID(entityID)
		, _commandType(commandType)
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&manager, &hive::modelsLibrary::ControllerManager::endAecpCommand, this,
			[this](la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType commandType, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
			{
				if (entityID != _entityID)
				{
					return;
				}

				if (commandType != _commandType)
				{
					return;
				}

				if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
				{
					// Something went wrong, restore the previous index
					QSignalBlocker lock{ q_ptr };
					q_ptr->setCurrentIndex(_previousIndex);
				}
				else
				{
					// Keep everything synchronized
					_previousIndex = q_ptr->currentIndex();
				}
			});
	}

protected:
	AecpCommandComboBox* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(AecpCommandComboBox);

	la::avdecc::UniqueIdentifier const _entityID;
	hive::modelsLibrary::ControllerManager::AecpCommandType const _commandType;

	int _previousIndex{ -1 };
};

AecpCommandComboBox::AecpCommandComboBox(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType commandType, QWidget* parent)
	: ComboBox(parent)
	, d_ptr(new AecpCommandComboBoxPrivate(this, entityID, commandType))
{
}

AecpCommandComboBox::~AecpCommandComboBox()
{
	delete d_ptr;
}

void AecpCommandComboBox::showPopup()
{
	// Our last chance to get the currently selected index before the user changes it
	d_ptr->_previousIndex = currentIndex();
	ComboBox::showPopup();
}
