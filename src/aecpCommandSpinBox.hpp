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

#pragma once

#include <hive/modelsLibrary/controllerManager.hpp>

#include <QSpinBox>
#include <QString>

#include <functional>
#include <vector>
#include <set>
#include <string>

template<typename DataType>
class AecpCommandSpinBox : public QSpinBox
{
public:
	using DataChangedHandler = std::function<void(DataType const& previousData, DataType const& newData)>;
	using Data = std::set<DataType>;
	using AecpBeginCommandHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID)>;
	using AecpResultHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;

	static_assert(std::is_integral_v<DataType> && sizeof(DataType) <= 4, "AecpCommandSpinBox only support integral types up to 32 bits");

	AecpCommandSpinBox(QWidget* parent = nullptr)
		: QSpinBox{ parent }
		, _parent{ parent }
	{
		// Send changes
		connect(this, QOverload<int>::of(&QSpinBox::valueChanged), this,
			[this](int value)
			{
				auto newData = static_cast<DataType>(value);
				// Save previous data before it's changed
				auto const previous = _previousData;
				// Update to new data
				setCurrentData(newData);
				// If new data is different than previous one, call handler
				if (previous != newData)
				{
					if (_dataChangedHandler)
					{
						_dataChangedHandler(previous, newData);
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
		setValue(static_cast<int>(data));
	}

	void setRangeAndStep(DataType const minimum, DataType const maximum, std::uint32_t const step) noexcept
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals
		setRange(static_cast<int>(minimum), static_cast<int>(maximum));
		setSingleStep(static_cast<int>(step));
	}

	DataType const& getCurrentData() const noexcept
	{
		return _previousData;
	}

	AecpBeginCommandHandler getBeginCommandHandler(hive::modelsLibrary::ControllerManager::AecpCommandType const commandType) noexcept
	{
		return [this](la::avdecc::UniqueIdentifier const entityID)
		{
			setEnabled(false);
		};
	}

	AecpResultHandler getResultHandler(hive::modelsLibrary::ControllerManager::AecpCommandType const commandType, DataType const& previousData) noexcept
	{
		return [this, commandType, previousData](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
		{
			QMetaObject::invokeMethod(this,
				[this, commandType, previousData, status]()
				{
					if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
					{
						setCurrentData(previousData);

						QMessageBox::warning(_parent, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(commandType) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
					}
					setEnabled(true);
				});
		};
	}

protected:
	using QSpinBox::setMinimum;
	using QSpinBox::setMaximum;
	using QSpinBox::setRange;
	using QSpinBox::setSingleStep;
	using QSpinBox::valueChanged;
	QWidget* _parent{ nullptr };
	Data _data{};
	DataType _previousData{};
	DataChangedHandler _dataChangedHandler{};
};
