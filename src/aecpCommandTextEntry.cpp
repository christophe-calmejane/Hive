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

#include "aecpCommandTextEntry.hpp"

class AecpCommandTextEntryPrivate : public QObject
{
public:
	AecpCommandTextEntryPrivate(AecpCommandTextEntry* q, la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex)
		: q_ptr(q)
		, _entityID{ entityID }
		, _commandType{ commandType }
		, _descriptorIndex{ descriptorIndex }
	{
		auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
		connect(&manager, &hive::modelsLibrary::ControllerManager::beginAecpCommand, this,
			[this](la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex)
			{
				if (entityID != _entityID)
				{
					return;
				}

				if (commandType != _commandType)
				{
					return;
				}

				if (descriptorIndex != _descriptorIndex)
				{
					return;
				}

				q_ptr->setEnabled(false);
			});

		connect(&manager, &hive::modelsLibrary::ControllerManager::endAecpCommand, this,
			[this](la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
			{
				if (entityID != _entityID)
				{
					return;
				}

				if (commandType != _commandType)
				{
					return;
				}

				if (descriptorIndex != _descriptorIndex)
				{
					return;
				}

				if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
				{
					// Something went wrong, restore the previous index
					QSignalBlocker lock{ q_ptr };
					q_ptr->setText(_previousText);
				}

				q_ptr->setEnabled(true);
			});
		connect(q, &qtMate::widgets::TextEntry::validated, this,
			[this](QString const& oldText, QString const& newText)
			{
				// Our last chance to get the previous text before it's sent
				_previousText = oldText;
			});
	}

protected:
	AecpCommandTextEntry* const q_ptr{ nullptr };
	Q_DECLARE_PUBLIC(AecpCommandTextEntry);

	la::avdecc::UniqueIdentifier const _entityID;
	hive::modelsLibrary::ControllerManager::AecpCommandType const _commandType;
	la::avdecc::entity::model::DescriptorIndex _descriptorIndex{};

	QString _previousText{};
};

AecpCommandTextEntry::AecpCommandTextEntry(la::avdecc::UniqueIdentifier const entityID, hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, QString const& text, std::optional<QValidator*> validator, QWidget* parent)
	: qtMate::widgets::TextEntry{ text, validator, parent }
	, d_ptr(new AecpCommandTextEntryPrivate(this, entityID, commandType, descriptorIndex))
{
}

AecpCommandTextEntry::~AecpCommandTextEntry()
{
	delete d_ptr;
}
