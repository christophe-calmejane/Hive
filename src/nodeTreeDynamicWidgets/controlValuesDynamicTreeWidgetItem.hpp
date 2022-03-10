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

#include "avdecc/helper.hpp"
#include "avdecc/stringValidator.hpp"
#include "aecpCommandComboBox.hpp"
#include "aecpCommandTextEntry.hpp"
#include "avdecc/hiveLogItems.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/internals/entityModelControlValuesTraits.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSignalBlocker>

#include <vector>
#include <cstring> // std::memcpy

class ControlValuesDynamicTreeWidgetItem : public QObject, public QTreeWidgetItem
{
public:
	ControlValuesDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel, QTreeWidget* parent = nullptr);

protected:
	la::avdecc::UniqueIdentifier const _entityID{};
	la::avdecc::entity::model::ControlIndex const _controlIndex{ 0u };

private:
	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept = 0;
};

/** Linear Values */
template<class StaticValueType, class DynamicValueType>
class LinearControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	using value_size = typename DynamicValueType::control_value_details_traits::size_type;
	using ComboboxType = AecpCommandComboBox<value_size>;

public:
	LinearControlValuesDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel, QTreeWidget* parent = nullptr)
		: ControlValuesDynamicTreeWidgetItem{ entityID, controlIndex, staticModel, dynamicModel, parent }
	{
		_isReadOnly = staticModel.controlValueType.isReadOnly();

		try
		{
			auto const staticValues = staticModel.values.getValues<StaticValueType>();
			auto const dynamicValues = dynamicModel.values.getValues<DynamicValueType>();
			auto const staticCount = staticValues.countValues();
			auto const dynamicCount = dynamicValues.countValues();
			if (staticCount != dynamicCount)
			{
				LOG_HIVE_WARN("ControlValues not valid: Static/Dynamic count mismatch");
				return;
			}

			auto valNumber = size_t{ 0u };
			for (auto const& val : dynamicValues.getValues())
			{
				auto const& staticVal = staticValues.getValues()[valNumber];

				auto* valueItem = new QTreeWidgetItem(this);
				valueItem->setText(0, QString("Value %1").arg(valNumber));

				auto* item = new QTreeWidgetItem(valueItem);
				item->setText(0, "Current Value");
				if (_isReadOnly)
				{
					auto* label = new QLabel(QString::number(val.currentValue));
					parent->setItemWidget(item, 1, label);
					_widgets.push_back(label);
				}
				else
				{
					auto* comboBox = new ComboboxType();
					parent->setItemWidget(item, 1, comboBox);

					auto const range = static_cast<size_t>(staticVal.maximum - staticVal.minimum);
					auto stepsCount = size_t{ 1 };
					if (staticVal.step != value_size{ 0 })
					{
						stepsCount += static_cast<size_t>(range / staticVal.step);
						if constexpr (std::is_integral_v<value_size>)
						{
							if ((range % staticVal.step) != 0)
							{
								// This should probably be detected by the AVDECC library
								LOG_HIVE_WARN("ControlValues not valid: Range not evenly divisible by Step");
							}
						}
					}
					auto data = typename std::remove_pointer_t<decltype(comboBox)>::Data{};
					for (auto i = size_t{ 0u }; i < stepsCount; ++i)
					{
						auto const possibleValue = static_cast<value_size>(staticVal.minimum + i * staticVal.step);
						data.insert(possibleValue);
					}
					comboBox->setAllData(data,
						[](auto const& value)
						{
							return QString::number(value);
						});

					// Send changes
					comboBox->setDataChangedHandler(
						[this, comboBox](auto const& previousValue, auto const& newValue)
						{
							sendControlValues(comboBox, previousValue);
						});

					_widgets.push_back(comboBox);
				}

				++valNumber;
			}

			_isValid = true;

			// Update now
			updateValues(dynamicModel.values);
		}
		catch (...)
		{
		}
	}

private:
	void sendControlValues(ComboboxType* const changedComboxbox, value_size const previousValue) noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};

			for (auto const* item : _widgets)
			{
				auto const* comboBox = static_cast<ComboboxType const*>(item);
				auto value = typename DynamicValueType::value_type{};
				value.currentValue = comboBox->getCurrentData();

				values.addValue(std::move(value));
			}

			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) },
				[this, changedComboxbox, previousValue](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
				{
					QMetaObject::invokeMethod(this,
						[this, changedComboxbox, previousValue, status]()
						{
							if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
							{
								changedComboxbox->setCurrentData(previousValue);

								QMessageBox::warning(changedComboxbox, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
							}
							changedComboxbox->setEnabled(true);
						});
				});
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != _widgets.size())
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Static/Dynamic count mismatch");
				return;
			}

			auto const dynamicValues = controlValues.getValues<DynamicValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop
			auto valNumber = size_t{ 0u };
			for (auto const& val : dynamicValues.getValues())
			{
				if (_isReadOnly)
				{
					auto* label = static_cast<QLabel*>(_widgets[valNumber]);
					label->setText(QString::number(val.currentValue));
				}
				else
				{
					auto* comboBox = static_cast<ComboboxType*>(_widgets[valNumber]);
					comboBox->setCurrentData(val.currentValue);
				}

				++valNumber;
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	std::vector<QWidget*> _widgets{};
};

/** Array Values */
template<class StaticValueType, class DynamicValueType>
class ArrayControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	using value_size = typename DynamicValueType::control_value_details_traits::size_type;

public:
	ArrayControlValuesDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel, QTreeWidget* parent = nullptr)
		: ControlValuesDynamicTreeWidgetItem{ entityID, controlIndex, staticModel, dynamicModel, parent }
	{
		_isReadOnly = staticModel.controlValueType.isReadOnly();

		try
		{
			auto const& staticVal = staticModel.values.getValues<StaticValueType>();
			auto const dynamicValues = dynamicModel.values.getValues<DynamicValueType>();
			auto const dynamicCount = dynamicValues.countValues();

			auto valNumber = size_t{ 0u };
			for (auto const& val : dynamicValues.currentValues)
			{
				auto* valueItem = new QTreeWidgetItem(this);
				valueItem->setText(0, QString("Value %1").arg(valNumber));

				auto* item = new QTreeWidgetItem(valueItem);
				item->setText(0, "Current Value");
				if (_isReadOnly)
				{
					auto* label = new QLabel(QString::number(val));
					parent->setItemWidget(item, 1, label);
					_widgets.push_back(label);
				}
				else
				{
					auto* comboBox = new AecpCommandComboBoxOLD(entityID, hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl, controlIndex);
					parent->setItemWidget(item, 1, comboBox);

					auto const range = static_cast<size_t>(staticVal.maximum - staticVal.minimum);
					auto stepsCount = size_t{ 1 };
					if (staticVal.step != value_size{ 0 })
					{
						stepsCount += static_cast<size_t>(range / staticVal.step);
						if constexpr (std::is_integral_v<value_size>)
						{
							if ((range % staticVal.step) != 0)
							{
								// This should probably be detected by the AVDECC library
								LOG_HIVE_WARN("ControlValues not valid: Range not evenly divisible by Step");
							}
						}
					}
					for (auto i = size_t{ 0u }; i < stepsCount; ++i)
					{
						auto const possibleValue = static_cast<value_size>(staticVal.minimum + i * staticVal.step);
						comboBox->addItem(QString::number(possibleValue), QVariant::fromValue(possibleValue));
					}

					// Send changes
					connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
						[this]()
						{
							sendControlValues();
						});

					_widgets.push_back(comboBox);
				}

				++valNumber;
			}

			_isValid = true;

			// Update now
			updateValues(dynamicModel.values);
		}
		catch (...)
		{
		}
	}

private:
	void sendControlValues() noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};

			for (auto const* item : _widgets)
			{
				auto const* comboBox = static_cast<AecpCommandComboBoxOLD const*>(item);
				values.currentValues.push_back(comboBox->currentData().value<value_size>());
			}

			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) });
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != _widgets.size())
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Static/Dynamic count mismatch");
				return;
			}

			auto const dynamicValues = controlValues.getValues<DynamicValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop
			auto valNumber = size_t{ 0u };
			for (auto const& val : dynamicValues.currentValues)
			{
				if (_isReadOnly)
				{
					auto* label = static_cast<QLabel*>(_widgets[valNumber]);
					label->setText(QString::number(val));
				}
				else
				{
					auto* comboBox = static_cast<AecpCommandComboBoxOLD*>(_widgets[valNumber]);

					QSignalBlocker const lg{ comboBox }; // Block internal signals so setCurrentIndex do not trigger "currentIndexChanged"
					auto const index = comboBox->findData(QVariant::fromValue(val));
					comboBox->setCurrentIndex(index);
				}

				++valNumber;
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	std::vector<QWidget*> _widgets{};
};

/** UTF-8 String Value */
class UTF8ControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	//using value_size = typename DynamicValueType::control_value_details_traits::size_type;
	using StaticValueType = la::avdecc::entity::model::UTF8StringValueStatic;
	using DynamicValueType = la::avdecc::entity::model::UTF8StringValueDynamic;

public:
	UTF8ControlValuesDynamicTreeWidgetItem(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel, QTreeWidget* parent = nullptr)
		: ControlValuesDynamicTreeWidgetItem{ entityID, controlIndex, staticModel, dynamicModel, parent }
	{
		_isReadOnly = staticModel.controlValueType.isReadOnly();

		try
		{
			auto const staticValues = staticModel.values.getValues<StaticValueType>();
			auto const dynamicValues = dynamicModel.values.getValues<DynamicValueType>();
			auto const staticCount = staticValues.countValues();
			auto const dynamicCount = dynamicValues.countValues();
			if (staticCount != dynamicCount)
			{
				LOG_HIVE_WARN("ControlValues not valid: Static/Dynamic count mismatch");
				return;
			}

			auto const text = QString::fromUtf8(reinterpret_cast<char const*>(dynamicValues.currentValue.data()));
			auto* item = new QTreeWidgetItem(this);
			item->setText(0, "Current Value");
			if (_isReadOnly)
			{
				auto label = new QLabel(text);
				parent->setItemWidget(item, 1, label);
				_widget = label;
			}
			else
			{
				auto* textEntry = new AecpCommandTextEntry(entityID, hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl, controlIndex, text, avdecc::ControlUTF8StringValidator::getSharedInstance());
				parent->setItemWidget(item, 1, textEntry);

				// Send changes
				connect(textEntry, &AecpCommandTextEntry::validated, textEntry,
					[this](QString const& /*oldText*/, QString const& /*newText*/)
					{
						sendControlValues();
					});

				_widget = textEntry;
			}

			_isValid = true;

			// Update now
			updateValues(dynamicModel.values);
		}
		catch (...)
		{
		}
	}

private:
	void sendControlValues() noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};

			auto const* textEntry = static_cast<AecpCommandTextEntry const*>(_widget);
			auto data = textEntry->text().toUtf8();
			std::memcpy(values.currentValue.data(), data.constData(), std::min(static_cast<size_t>(data.size()), values.currentValue.size()));
			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) });
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != 1u)
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Static/Dynamic count mismatch");
				return;
			}

			auto const dynamicValues = controlValues.getValues<DynamicValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop
			auto const text = QString::fromUtf8(reinterpret_cast<char const*>(dynamicValues.currentValue.data()));

			if (_isReadOnly)
			{
				auto* label = static_cast<QLabel*>(_widget);
				label->setText(text);
			}
			else
			{
				auto* textEntry = static_cast<AecpCommandTextEntry*>(_widget);

				QSignalBlocker const lg{ textEntry }; // Block internal signals
				textEntry->setText(text);
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	QWidget* _widget{};
};
