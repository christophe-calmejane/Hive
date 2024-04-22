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

#include <QInputDialog>

#include <chrono>
#include <optional>

using LatencyComboBox_t = std::tuple<std::chrono::nanoseconds, std::string, std::optional<bool>>; // Value to be sent to the device / Displayed value / Is custom value

Q_DECLARE_METATYPE(LatencyComboBox_t)

struct LatencyComboBoxCompare final
{
	bool operator()(LatencyComboBox_t const& lhs, LatencyComboBox_t const& rhs) const noexcept
	{
		// Check for custom values (we want them at the end of the combobox)
		auto const lhsCustom = std::get<2>(lhs);
		auto const rhsCustom = std::get<2>(rhs);
		auto const lhsIsCustom = lhsCustom && *lhsCustom;
		auto const rhsIsCustom = rhsCustom && *rhsCustom;

		// If lhs is custom and rhs is not, lhs is greater
		if (lhsIsCustom && !rhsIsCustom)
		{
			return false;
		}

		// If rhs is custom and lhs is not, rhs is greater
		if (!lhsIsCustom && rhsIsCustom)
		{
			return true;
		}

		// If both or none are custom, compare the values
		return std::get<0>(lhs) < std::get<0>(rhs);
	}
};
using LatencyComboBoxDataContainer = std::set<LatencyComboBox_t, LatencyComboBoxCompare>;
using BaseComboBoxType = AecpCommandComboBox<LatencyComboBox_t, LatencyComboBoxDataContainer>;

class LatencyComboBox final : public BaseComboBoxType
{
private:
public:
	LatencyComboBox(QWidget* parent = nullptr)
		: BaseComboBoxType{ parent }
	{
		// Handle index change
		setIndexChangedHandler(
			[this](LatencyComboBox_t const& latencyData)
			{
				auto latData = latencyData;
				auto const isCustomOpt = std::get<2>(latencyData);
				if (isCustomOpt && *isCustomOpt)
				{
					auto ok = false;
					auto const customValue = QInputDialog::getInt(this, "Latency (in nanoseconds)", "Count", 2000000, 1, 3000000, 1, &ok);

					if (ok)
					{
						auto const latency = std::chrono::nanoseconds{ customValue };
						latData = LatencyComboBox_t{ latency, labelFromLatency(latency), true };
					}
					else
					{
						latData = getCurrentData();
					}
				}
				return latData;
			});
	}

	void setCurrentLatencyData(LatencyComboBox_t const& latencyData)
	{
		setCurrentData(latencyData);
	}

	void setLatencyDatas(Data const& latencyDatas) noexcept
	{
		BaseComboBoxType::setAllData(latencyDatas,
			[](LatencyComboBox_t const& latencyData)
			{
				return QString::fromUtf8(std::get<1>(latencyData));
			});
	}

	LatencyComboBox_t const& getCurrentLatencyData() const noexcept
	{
		return getCurrentData();
	}

private:
	using BaseComboBoxType::setIndexChangedHandler;
	using BaseComboBoxType::setAllData;
	using BaseComboBoxType::getCurrentData;

	std::string labelFromLatency(std::chrono::nanoseconds const& latency) const noexcept
	{
		return std::to_string(latency.count()) + " nsec";
	}

	bool isStaticValue(std::chrono::nanoseconds const& latency) const noexcept
	{
		for (auto const& latencyData : getAllData())
		{
			if (std::get<0>(latencyData) == latency)
			{
				return true;
			}
		}
		return false;
	}

	virtual void setCurrentData(LatencyComboBox_t const& data) noexcept override
	{
		auto const lg = QSignalBlocker{ this }; // Block internal signals so setCurrentText do not trigger "currentIndexChanged"

		auto const previousLatency = std::get<0>(_previousData);
		auto const latency = std::get<0>(data);
		auto label = QString::fromUtf8(std::get<1>(data));
		auto newData = data;

		// If the previous value is not part of the static data, it was added as a temporary value and we have to remove it
		if (!isStaticValue(previousLatency))
		{
			auto const index = findData(QVariant::fromValue(_previousData));

			if (index != -1)
			{
				removeItem(index);
			}
		}

		// If the new value is not part of the static data, we have to add it as a temporary value
		if (!isStaticValue(latency))
		{
			auto const strLabel = labelFromLatency(latency);
			label = QString::fromUtf8(strLabel);
			newData = LatencyComboBox_t{ latency, strLabel, true };

			addItem(label, QVariant::fromValue(newData));
			auto const index = findData(QVariant::fromValue(newData));

			QFont font;
			font.setItalic(true);

			setItemData(index, font, Qt::FontRole);
		}

		_previousData = std::move(newData);
		setCurrentText(label);
	}
};
