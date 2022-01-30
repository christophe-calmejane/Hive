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

#include "settingsManager/settings.hpp"
#include "internals/config.hpp"
#include "newsFeed.hpp"

#include <nlohmann/json.hpp>
#include <la/avdecc/utils.hpp>

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <atomic>
#include <optional>
#include <cstdint>

using json = nlohmann::json;

class NewsFeedImpl final : public NewsFeed
{
public:
	NewsFeedImpl() noexcept
	{
		// Connect signals
		connect(&_webCtrlRelease, &QNetworkAccessManager::finished, this,
			[this](QNetworkReply* const reply)
			{
				if (reply->error() == QNetworkReply::NoError)
				{
					auto const& buffer = QString(reply->readAll()).trimmed();

					try
					{
						auto const j = json::parse(buffer.toStdString());
						auto buffer = std::string{};

						auto const serverTimestamp = j.at("serverTimestamp").get<std::uint64_t>();
						auto const news = j.at("news").get<std::string>();
						emit newsAvailable(QString::fromStdString(news), serverTimestamp);
					}
					catch (...)
					{
					}
				}
				reply->deleteLater();
				_checkInProgress = false;
			});
	}

	~NewsFeedImpl() noexcept {}

	// Deleted compiler auto-generated methods
	NewsFeedImpl(NewsFeedImpl const&) = delete;
	NewsFeedImpl(NewsFeedImpl&&) = delete;
	NewsFeedImpl& operator=(NewsFeedImpl const&) = delete;
	NewsFeedImpl& operator=(NewsFeedImpl&&) = delete;

private:
	// NewsFeed overrides
	virtual void checkForNews(std::uint64_t const lastCheckTime) noexcept override
	{
		if (!_checkInProgress)
		{
			QNetworkRequest request{ QUrl{ QString{ "%1?lastCheckTime=%2&buildNumber=%3" }.arg(hive::internals::newsFeedUrl.c_str()).arg(lastCheckTime).arg(hive::internals::buildNumber) } };
			_checkInProgress = true;
			_webCtrlRelease.get(request);
		}
	}

	// Private methods
	template<typename T>
	std::optional<T> getOptional(json const& j, std::string const& name)
	{
		try
		{
			return j.at(name).get<T>();
		}
		catch (...)
		{
		}
		return {};
	}

	// Private members
	std::atomic_bool _checkInProgress{ false };
	QNetworkAccessManager _webCtrlRelease{};
};

NewsFeed::NewsFeed() noexcept {}

NewsFeed& NewsFeed::getInstance() noexcept
{
	static NewsFeedImpl s_newsFeed{};

	return s_newsFeed;
}
