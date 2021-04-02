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

#include <memory>
#include <QObject>
#include <QString>

class Updater : public QObject
{
	Q_OBJECT
public:
	static Updater& getInstance() noexcept;

	/** Force a check for new version */
	virtual void checkForNewVersion() noexcept = 0;

	/** Returns true if the Updater automatically checks for new versions */
	virtual bool isAutomaticCheckForNewVersion() const noexcept = 0;

	/* Updater signals */
	Q_SIGNAL void newReleaseVersionAvailable(QString version, QString downloadURL);
	Q_SIGNAL void newBetaVersionAvailable(QString version, QString downloadURL);
	Q_SIGNAL void checkFailed(QString reason);
};
