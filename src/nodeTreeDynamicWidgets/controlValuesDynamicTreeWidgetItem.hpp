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

#include "avdecc/helper.hpp"
#include "avdecc/stringValidator.hpp"
#include "aecpCommandComboBox.hpp"
#include "aecpCommandTextEntry.hpp"
#include "aecpCommandSpinBox.hpp"
#include "aecpCommandSlider.hpp"
#include "avdecc/hiveLogItems.hpp"

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/internals/entityModelControlValuesTraits.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/helper.hpp>

#include <QObject>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSignalBlocker>

#include <map>
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

/** Linear Values - Clause 7.3.5.2.1 */
template<class StaticValueType, class DynamicValueType>
class LinearControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	using value_size = typename DynamicValueType::control_value_details_traits::size_type;
	static constexpr auto UseSpinBox = std::conditional_t < std::is_integral_v<value_size> && sizeof(value_size) <= 4, std::true_type, std::false_type > ();
	static constexpr auto UseSlider = std::false_type::value;
	using WidgetType = std::conditional_t<UseSpinBox, AecpCommandSpinBox<value_size>, std::conditional_t<UseSlider, AecpCommandSlider<value_size>, AecpCommandComboBox<value_size>>>;

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
					// Do not use a widget for read only values (especially not a QLabel), the 'selected' state of the item will not be applied
					item->setText(1, QString::number(val.currentValue));
					_items[valNumber] = item;
				}
				else
				{
					auto* widget = new WidgetType{};
					parent->setItemWidget(item, 1, widget);

					auto const range = static_cast<value_size>(staticVal.maximum - staticVal.minimum);
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

					if constexpr (UseSpinBox)
					{
						widget->setRangeAndStep(staticVal.minimum, staticVal.maximum, staticVal.step);
					}
					else if constexpr (UseSlider)
					{
						widget->setRangeAndStep(staticVal.minimum, staticVal.maximum, staticVal.step);
						widget->setTickPosition(QSlider::TicksBothSides);
					}
					else
					{
						auto data = typename std::remove_pointer_t<decltype(widget)>::Data{};
						for (auto i = size_t{ 0u }; i < stepsCount; ++i)
						{
							auto const possibleValue = static_cast<value_size>(staticVal.minimum + i * staticVal.step);
							data.insert(possibleValue);
						}
						widget->setAllData(data,
							[](auto const& value)
							{
								return QString::number(value);
							});
					}

					// Send changes
					widget->setDataChangedHandler(
						[this, widget](auto const& previousValue, auto const& newValue)
						{
							sendControlValues(widget, previousValue);
						});

					_widgets[valNumber] = widget;
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
	void sendControlValues(WidgetType* const changedWidget, value_size const previousValue) noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};

			for (auto const [valNumber, item] : _widgets)
			{
				auto const* widget = static_cast<WidgetType const*>(item);
				auto value = typename DynamicValueType::value_type{};
				value.currentValue = widget->getCurrentData();

				values.addValue(std::move(value));
			}

			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) },
				[this, changedWidget](la::avdecc::UniqueIdentifier const /*entityID*/)
				{
					changedWidget->setEnabled(false);
				},
				[this, changedWidget, previousValue](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
				{
					QMetaObject::invokeMethod(this,
						[this, changedWidget, previousValue, status]()
						{
							if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
							{
								changedWidget->setCurrentData(previousValue);

								QMessageBox::warning(changedWidget, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
							}
							changedWidget->setEnabled(true);
						});
				});
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != (_items.size() + _widgets.size()))
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Dynamic count mismatch");
				return;
			}

			try
			{
				auto const dynamicValues = controlValues.getValues<DynamicValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop
				auto valNumber = size_t{ 0u };
				for (auto const& val : dynamicValues.getValues())
				{
					if (_isReadOnly)
					{
						if (auto const itemIt = _items.find(valNumber); itemIt != _items.end())
						{
							auto* const item = itemIt->second;
							item->setText(1, QString::number(val.currentValue));
						}
						else
						{
							LOG_HIVE_WARN(QString("Failed to update ControlValue n°%1: Item not found").arg(valNumber));
						}
					}
					else
					{
						if (auto const widgetIt = _widgets.find(valNumber); widgetIt != _widgets.end())
						{
							auto* const widget = static_cast<WidgetType*>(widgetIt->second);
							widget->setCurrentData(val.currentValue);
						}
						else
						{
							LOG_HIVE_WARN(QString("Failed to update ControlValue n°%1: Widget not found").arg(valNumber));
						}
					}

					++valNumber;
				}
			}
			catch (std::invalid_argument const&)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor values doesn't seem valid");
			}
			catch (...)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor was validated, this should not throw");
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	std::map<size_t, QTreeWidgetItem*> _items{};
	std::map<size_t, QWidget*> _widgets{};
};

/** Selector Values - Clause 7.3.5.2.2 */
template<typename ValueType, class StaticValueType, class DynamicValueType>
class SelectorControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	using WidgetType = AecpCommandComboBox<ValueType>;

public:
	SelectorControlValuesDynamicTreeWidgetItem([[maybe_unused]] la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::ControlIndex const controlIndex, la::avdecc::entity::model::ControlNodeStaticModel const& staticModel, la::avdecc::entity::model::ControlNodeDynamicModel const& dynamicModel, QTreeWidget* parent = nullptr)
		: ControlValuesDynamicTreeWidgetItem{ entityID, controlIndex, staticModel, dynamicModel, parent }
	{
		_isReadOnly = staticModel.controlValueType.isReadOnly();

		try
		{
			auto const staticValue = staticModel.values.getValues<StaticValueType>();
			auto const dynamicValue = dynamicModel.values.getValues<DynamicValueType>();
			if (dynamicValue.countValues() != 1)
			{
				LOG_HIVE_WARN("ControlValues not valid: Dynamic count is not equal to 1");
				return;
			}

			auto* valueItem = new QTreeWidgetItem(this);
			valueItem->setText(0, "Current Value");

			if (_isReadOnly)
			{
				// Do not use a widget for read only values (especially not a QLabel), the 'selected' state of the item will not be applied
				valueItem->setText(1, QString::number(dynamicValue.currentValue));
				_item = valueItem;
			}
			else
			{
				auto* widget = new WidgetType{};
				parent->setItemWidget(valueItem, 1, widget);

				{
					auto data = typename std::remove_pointer_t<decltype(widget)>::Data{};
					for (auto const& option : staticValue.options)
					{
						data.insert(option);
					}
					widget->setAllData(data,
						[controlledEntity](auto const& value)
						{
							if constexpr (std::is_same_v<ValueType, la::avdecc::entity::model::LocalizedStringReference>)
							{
								return hive::modelsLibrary::helper::localizedString(*controlledEntity, value);
							}
							else
							{
								return QString::number(value);
							}
						});
				}

				// Send changes
				widget->setDataChangedHandler(
					[this, widget](auto const& previousValue, auto const& newValue)
					{
						sendControlValues(widget, previousValue);
					});

				_widget = widget;
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
	void sendControlValues(WidgetType* const changedWidget, ValueType const previousValue) noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};
			{
				auto const* widget = static_cast<WidgetType const*>(_widget);
				values.currentValue = static_cast<decltype(DynamicValueType::currentValue)>(widget->getCurrentData());
			}
			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) },
				[this, changedWidget](la::avdecc::UniqueIdentifier const /*entityID*/)
				{
					changedWidget->setEnabled(false);
				},
				[this, changedWidget, previousValue](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
				{
					QMetaObject::invokeMethod(this,
						[this, changedWidget, previousValue, status]()
						{
							if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
							{
								changedWidget->setCurrentData(previousValue);

								QMessageBox::warning(changedWidget, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl) + "<//>/ failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
							}
							changedWidget->setEnabled(true);
						});
				});
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != 1u)
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Dynamic count is not equal to 1");
				return;
			}

			try
			{
				auto const& dynamicValue = controlValues.getValues<DynamicValueType>();

				if (_isReadOnly)
				{
					_item->setText(1, QString::number(dynamicValue.currentValue));
				}
				else
				{
					auto* widget = static_cast<WidgetType*>(_widget);
					widget->setCurrentData(dynamicValue.currentValue);
				}
			}
			catch (std::invalid_argument const&)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor values doesn't seem valid");
			}
			catch (...)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor was validated, this should not throw");
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	QTreeWidgetItem* _item{ nullptr };
	QWidget* _widget{ nullptr };
};

/** Array Values - Clause 7.3.5.2.3 */
template<class StaticValueType, class DynamicValueType>
class ArrayControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	using value_size = typename DynamicValueType::control_value_details_traits::size_type;
	static constexpr auto UseSpinBox = std::conditional_t < std::is_integral_v<value_size> && sizeof(value_size) <= 4, std::true_type, std::false_type > (); // SpinBox only supports integral types up to 32 bits
	using WidgetType = std::conditional_t<UseSpinBox, AecpCommandSpinBox<value_size>, AecpCommandComboBox<value_size>>;

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
					// Do not use a widget for read only values (especially not a QLabel), the 'selected' state of the item will not be applied
					item->setText(1, QString::number(val));
					_items[valNumber] = item;
				}
				else
				{
					auto* widget = new WidgetType{};
					parent->setItemWidget(item, 1, widget);

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

					if constexpr (UseSpinBox)
					{
						widget->setRangeAndStep(staticVal.minimum, staticVal.maximum, staticVal.step);
					}
					else
					{
						auto data = typename std::remove_pointer_t<decltype(widget)>::Data{};
						for (auto i = size_t{ 0u }; i < stepsCount; ++i)
						{
							auto const possibleValue = static_cast<value_size>(staticVal.minimum + i * staticVal.step);
							data.insert(possibleValue);
						}
						widget->setAllData(data,
							[](auto const& value)
							{
								return QString::number(value);
							});
					}

					// Send changes
					widget->setDataChangedHandler(
						[this, widget](auto const& previousValue, auto const& newValue)
						{
							sendControlValues(widget, previousValue);
						});

					_widgets[valNumber] = widget;
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
	void sendControlValues(WidgetType* const changedWidget, value_size const previousValue) noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};

			for (auto const [valNumber, item] : _widgets)
			{
				auto const* widget = static_cast<WidgetType const*>(item);
				values.currentValues.push_back(widget->getCurrentData());
			}

			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) },
				[this, changedWidget](la::avdecc::UniqueIdentifier const /*entityID*/)
				{
					changedWidget->setEnabled(false);
				},
				[this, changedWidget, previousValue](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
				{
					QMetaObject::invokeMethod(this,
						[this, changedWidget, previousValue, status]()
						{
							if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
							{
								changedWidget->setCurrentData(previousValue);

								QMessageBox::warning(changedWidget, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
							}
							changedWidget->setEnabled(true);
						});
				});
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != (_items.size() + _widgets.size()))
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Dynamic count mismatch");
				return;
			}

			try
			{
				auto const dynamicValues = controlValues.getValues<DynamicValueType>(); // We have to store the copy or it will go out of scope if using it directly in the range-based loop
				auto valNumber = size_t{ 0u };
				for (auto const& val : dynamicValues.currentValues)
				{
					if (_isReadOnly)
					{
						if (auto const itemIt = _items.find(valNumber); itemIt != _items.end())
						{
							auto* const item = itemIt->second;
							item->setText(1, QString::number(val));
						}
						else
						{
							LOG_HIVE_WARN(QString("Failed to update ControlValue n°%1: Item not found").arg(valNumber));
						}
					}
					else
					{
						if (auto const widgetIt = _widgets.find(valNumber); widgetIt != _widgets.end())
						{
							auto* const widget = static_cast<WidgetType*>(widgetIt->second);
							widget->setCurrentData(val);
						}
						else
						{
							LOG_HIVE_WARN(QString("Failed to update ControlValue n°%1: Widget not found").arg(valNumber));
						}
					}

					++valNumber;
				}
			}
			catch (std::invalid_argument const&)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor values doesn't seem valid");
			}
			catch (...)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor was validated, this should not throw");
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	std::map<size_t, QTreeWidgetItem*> _items{};
	std::map<size_t, QWidget*> _widgets{};
};

/** UTF-8 String Value - Clause 7.3.5.2.4 */
class UTF8ControlValuesDynamicTreeWidgetItem : public ControlValuesDynamicTreeWidgetItem
{
	//using value_size = typename DynamicValueType::control_value_details_traits::size_type;
	using StaticValueType = la::avdecc::entity::model::UTF8StringValueStatic;
	using DynamicValueType = la::avdecc::entity::model::UTF8StringValueDynamic;
	using WidgetType = AecpCommandTextEntry;

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
				// Do not use a widget for read only values (especially not a QLabel), the 'selected' state of the item will not be applied
				item->setText(1, text);
				_item = item;
			}
			else
			{
				auto* widget = new WidgetType{ text, avdecc::ControlUTF8StringValidator::getSharedInstance() };
				parent->setItemWidget(item, 1, widget);

				// Send changes
				widget->setDataChangedHandler(
					[this, widget](QString const& oldText, QString const& /*newText*/)
					{
						sendControlValues(widget, oldText);
					});

				_widget = widget;
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
	void sendControlValues(WidgetType* const changedWidget, QString const& previousValue) noexcept
	{
		if (AVDECC_ASSERT_WITH_RET(!_isReadOnly, "Should never call sendControlValues with read only values"))
		{
			auto values = DynamicValueType{};
			{
				auto const* widget = static_cast<WidgetType const*>(_widget);
				auto data = widget->getCurrentData().toUtf8();
				std::memcpy(values.currentValue.data(), data.constData(), std::min(static_cast<size_t>(data.size()), values.currentValue.size()));
			}
			hive::modelsLibrary::ControllerManager::getInstance().setControlValues(_entityID, _controlIndex, la::avdecc::entity::model::ControlValues{ std::move(values) },
				[this, changedWidget](la::avdecc::UniqueIdentifier const /*entityID*/)
				{
					changedWidget->setEnabled(false);
				},
				[this, changedWidget, previousValue](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
				{
					QMetaObject::invokeMethod(this,
						[this, changedWidget, previousValue, status]()
						{
							if (status != la::avdecc::entity::ControllerEntity::AemCommandStatus::Success)
							{
								changedWidget->setCurrentData(previousValue);

								QMessageBox::warning(changedWidget, "", "<i>" + hive::modelsLibrary::ControllerManager::typeToString(hive::modelsLibrary::ControllerManager::AecpCommandType::SetControl) + "</i> failed:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(status)));
							}
							changedWidget->setEnabled(true);
						});
				});
		}
	}

	virtual void updateValues(la::avdecc::entity::model::ControlValues const& controlValues) noexcept override
	{
		if (_isValid)
		{
			if (controlValues.size() != 1u)
			{
				// This should probably be detected by the AVDECC library
				LOG_HIVE_WARN("ControlValues update not valid: Dynamic count is not equal to 1");
				return;
			}

			try
			{
				auto const& dynamicValue = controlValues.getValues<DynamicValueType>();
				auto const text = QString::fromUtf8(reinterpret_cast<char const*>(dynamicValue.currentValue.data()));

				if (_isReadOnly)
				{
					_item->setText(1, text);
				}
				else
				{
					auto* widget = static_cast<WidgetType*>(_widget);
					widget->setCurrentData(text);
				}
			}
			catch (std::invalid_argument const&)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor values doesn't seem valid");
			}
			catch (...)
			{
				AVDECC_ASSERT(false, "Identify Control Descriptor was validated, this should not throw");
			}
		}
	}

	bool _isValid{ false };
	bool _isReadOnly{ false };
	QTreeWidgetItem* _item{ nullptr };
	QWidget* _widget{ nullptr };
};
