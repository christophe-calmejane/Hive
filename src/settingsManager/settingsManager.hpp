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
	* @return A new SettingsManager as a SettingsManager::UniquePointer.
	*/
	static UniquePointer create()
	{
		auto deleter = [](SettingsManager* self)
		{
			self->destroy();
		};
		return UniquePointer(createRawSettingsManager(), deleter);
	}

	//static SettingsManager& getInstance() noexcept;

	virtual void registerSetting(SettingDefault const& setting) noexcept = 0;

	virtual void setValue(Setting const& name, QVariant const& value, Observer const* const dontNotifyObserver = nullptr) noexcept = 0;
	virtual QVariant getValue(Setting const& name) const noexcept = 0;

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

private:
	/** Entry point */
	static SettingsManager* createRawSettingsManager();

	/** Destroy method for COM-like interface */
	virtual void destroy() noexcept = 0;
};

Q_DECLARE_METATYPE(settings::SettingsManager*);

} // namespace settings
