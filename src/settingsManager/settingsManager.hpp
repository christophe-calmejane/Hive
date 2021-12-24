/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QString>
#include <QVariant>
#include <la/avdecc/utils.hpp>

#include <memory>
#include <optional>

namespace settings
{
class SettingsManager
{
protected:
	using Subject = la::avdecc::utils::TypedSubject<struct SubjectTag, std::mutex>;

public:
	static constexpr auto PropertyName = "SettingsManager";
	using UniquePointer = std::unique_ptr<SettingsManager, void (*)(SettingsManager*)>;
	using Setting = QString;
	struct SettingDefault
	{
		Setting name{};
		QVariant initialValue{};
	};

	class Observer : public la::avdecc::utils::Observer<Subject>
	{
	public:
		virtual void onSettingChanged(settings::SettingsManager::Setting const& name, QVariant const& value) noexcept = 0;
	};

	/**
	* @brief Factory method to create a new SettingsManager.
	* @details Creates a new SettingsManager as a unique pointer.
	* @param[in] iniFilePath If specified, Settings file to use. Otherwise the default file will be used.
	* @return A new SettingsManager as a SettingsManager::UniquePointer.
	*/
	static UniquePointer create(std::optional<QString> const& iniFilePath)
	{
		auto deleter = [](SettingsManager* self)
		{
			self->destroy();
		};
		return UniquePointer(createRawSettingsManager(iniFilePath), deleter);
	}

	virtual void registerSetting(SettingDefault const& setting) noexcept = 0;

	template<typename ValueType = QVariant>
	void setValue(Setting const& name, ValueType&& value, Observer const* const dontNotifyObserver = nullptr) noexcept
	{
		if constexpr (std::is_enum_v<std::decay_t<ValueType>>)
		{
			// Enums are stored as integral values
			setValueInternal(name, static_cast<std::underlying_type_t<std::decay_t<ValueType>>>(value), dontNotifyObserver);
		}
		else
		{
			setValueInternal(name, std::forward<ValueType>(value), dontNotifyObserver);
		}
	}

	template<typename ValueType = QVariant>
	ValueType getValue(Setting const& name) const noexcept
	{
		if constexpr (std::is_enum_v<ValueType>)
		{
			// Enums are stored as integral values
			return static_cast<ValueType>(getValueInternal(name).value<std::underlying_type_t<ValueType>>());
		}
		else
		{
			return getValueInternal(name);
		}
	}

	virtual void registerSettingObserver(Setting const& name, Observer* const observer, bool const triggerFirstNotification = true) const noexcept = 0;
	virtual void unregisterSettingObserver(Setting const& name, Observer* const observer) const noexcept = 0;
	virtual void triggerSettingObserver(Setting const& name, Observer* const observer) const noexcept = 0;

	virtual QString getFilePath() const noexcept = 0;

	// Deleted compiler auto-generated methods
	SettingsManager(SettingsManager&&) = delete;
	SettingsManager(SettingsManager const&) = delete;
	SettingsManager& operator=(SettingsManager const&) = delete;
	SettingsManager& operator=(SettingsManager&&) = delete;

protected:
	/** Constructor */
	SettingsManager() noexcept = default;

	/** Destructor */
	virtual ~SettingsManager() noexcept = default;

	virtual void setValueInternal(Setting const& name, QVariant const& value, Observer const* const dontNotifyObserver) noexcept = 0;
	virtual QVariant getValueInternal(Setting const& name) const noexcept = 0;

private:
	/** Entry point */
	static SettingsManager* createRawSettingsManager(std::optional<QString> const& iniFilePath);

	/** Destroy method for COM-like interface */
	virtual void destroy() noexcept = 0;
};

} // namespace settings

Q_DECLARE_METATYPE(settings::SettingsManager*);
