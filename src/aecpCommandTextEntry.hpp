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

#include <hive/modelsLibrary/controllerManager.hpp>
#include <QtMate/widgets/textEntry.hpp>

#include <QMessageBox>
#include <QString>

#include <functional>

class AecpCommandTextEntry : public qtMate::widgets::TextEntry
{
public:
	using DataChangedHandler = std::function<void(QString const& previousData, QString const& newData)>;
	using DataType = QString;
	using AecpBeginCommandHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID)>;
	using MilanBeginCommandHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID)>;
	using AecpResultHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;
	using MilanResultHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::MvuCommandStatus const status)>;

	AecpCommandTextEntry(QString const& text, std::optional<QValidator*> validator = std::nullopt, QWidget* parent = nullptr)
		: qtMate::widgets::TextEntry{ text, validator, parent }
		, _parent{ parent }
	{
		// Send changes
		connect(this, &qtMate::widgets::TextEntry::validated, this,
			[this](QString const& oldText, QString const& newText)
			{
				// Update to new data
				setCurrentData(newText);
				// If new data is different than previous one, call handler
				if (oldText != newText)
				{
					if (_dataChangedHandler)
					{
						_dataChangedHandler(oldText, newText);
					}
				}
			});
	}

	void setDataChangedHandler(DataChangedHandler const& handler) noexcept
	{
		_dataChangedHandler = handler;
	}

	virtual void setCurrentData(DataType const& data) noexcept
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals
		_previousData = data;
		setText(data);
	}

	DataType const& getCurrentData() const noexcept
	{
		return _previousData;
	}

	template<class CommandType, class CommandHandlerType = std::conditional_t<std::is_same_v<CommandType, hive::modelsLibrary::ControllerManager::AecpCommandType>, AecpBeginCommandHandler, MilanBeginCommandHandler>>
	CommandHandlerType getBeginCommandHandler(CommandType const commandType) noexcept
	{
		return [this](la::avdecc::UniqueIdentifier const entityID)
		{
			setEnabled(false);
		};
	}

	template<class CommandType, class CommandStatus = std::conditional_t<std::is_same_v<CommandType, hive::modelsLibrary::ControllerManager::AecpCommandType>, la::avdecc::entity::ControllerEntity::AemCommandStatus, la::avdecc::entity::ControllerEntity::MvuCommandStatus>, class ResultHandlerType = std::conditional_t<std::is_same_v<CommandType, hive::modelsLibrary::ControllerManager::AecpCommandType>, AecpResultHandler, MilanResultHandler>>
	ResultHandlerType getResultHandler(CommandType const commandType, DataType const& previousData) noexcept
	{
		return [this, commandType, previousData](la::avdecc::UniqueIdentifier const entityID, CommandStatus const status)
		{
			QMetaObject::invokeMethod(this,
				[this, commandType, previousData, status]()
				{
					if (status != CommandStatus::Success)
					{
						setCurrentData(previousData);

						QMessageBox::warning(_parent, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
					}
					setEnabled(true);
				});
		};
	}

protected:
	using qtMate::widgets::TextEntry::setText;
	using qtMate::widgets::TextEntry::text;
	using qtMate::widgets::TextEntry::validated;
	QWidget* _parent{ nullptr };
	DataType _previousData{};
	DataChangedHandler _dataChangedHandler{};
};
