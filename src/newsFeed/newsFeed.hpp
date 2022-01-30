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

#include <memory>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>

class NewsFeed : public QObject
{
	Q_OBJECT
public:
	static NewsFeed& getInstance() noexcept;

	/** Force a check for news */
	virtual void checkForNews(std::uint64_t const lastCheckTime) noexcept = 0;

	/* Public signals */
	Q_SIGNAL void newsAvailable(QString const& news, std::uint64_t const serverTimestamp);

	// Deleted compiler auto-generated methods
	NewsFeed(NewsFeed const&) = delete;
	NewsFeed(NewsFeed&&) = delete;
	NewsFeed& operator=(NewsFeed const&) = delete;
	NewsFeed& operator=(NewsFeed&&) = delete;

protected:
	NewsFeed() noexcept;
};
