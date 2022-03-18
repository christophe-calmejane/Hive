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

#include <QMessageBox>
#include <QString>

#include <functional>
#include <vector>
#include <set>
#include <string>

template<typename DataType>
class AecpCommandComboBox : public qtMate::widgets::ComboBox
{
public:
	using IndexChangedHandler = std::function<DataType(DataType const& data)>;
	using DataChangedHandler = std::function<void(DataType const& previousData, DataType const& newData)>;
	using Data = std::set<DataType>;
	using DataToStringHandler = std::function<QString(DataType const& data)>;
	using AecpBeginCommandHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID)>;
	using AecpResultHandler = std::function<void(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)>;

	AecpCommandComboBox(QWidget* parent = nullptr)
		: qtMate::widgets::ComboBox{ parent }
		, _parent{ parent }
	{
		// Send changes
		connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
			[this](int /*index*/)
			{
				auto newData = currentData().template value<DataType>();
				// If we have a index changed delegate, call it
				if (_indexChangedHandler)
				{
					newData = _indexChangedHandler(newData);
				}
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

	void setIndexChangedHandler(IndexChangedHandler const& handler) noexcept
	{
		_indexChangedHandler = handler;
	}

	void setDataChangedHandler(DataChangedHandler const& handler) noexcept
	{
		_dataChangedHandler = handler;
	}

	virtual void setCurrentData(DataType const& data) noexcept
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals so setCurrentText do not trigger "currentIndexChanged"
		auto const index = findData(QVariant::fromValue(data));
		_previousData = data;
		setCurrentIndex(index);
		setCurrentText(_dataToStringHandler(data));
	}

	void setAllData(Data const& data, DataToStringHandler const& handler) noexcept
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals so clear and addItem do not trigger "currentIndexChanged"
		_dataToStringHandler = handler;
		clear();
		for (auto const& d : data)
		{
			auto const str = _dataToStringHandler(d);
			addItem(str, QVariant::fromValue(d));
		}
		_data = data;
	}

	void setAllData(std::vector<DataType> const& data, DataToStringHandler const& handler) noexcept
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals so clear and addItem do not trigger "currentIndexChanged"
		_dataToStringHandler = handler;
		clear();
		for (auto const& d : data)
		{
			auto const str = _dataToStringHandler(d);
			addItem(str, QVariant::fromValue(d));
			_data.insert(d);
		}
	}

	DataType const& getCurrentData() const noexcept
	{
		return _previousData;
	}

	Data const& getAllData() const noexcept
	{
		return _data;
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
	using QComboBox::addItem;
	using QComboBox::setCurrentIndex;
	using QComboBox::currentData;
	using QComboBox::currentIndexChanged;
	QWidget* _parent{ nullptr };
	Data _data{};
	DataType _previousData{};
	IndexChangedHandler _indexChangedHandler{};
	DataChangedHandler _dataChangedHandler{};
	DataToStringHandler _dataToStringHandler{};
};
