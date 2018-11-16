/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <memory>
#include <QObject>
#include <QMap>


namespace avdecc
{
namespace mediaClock
{
using DomainIndex = std::uint64_t;


// **************************************************************
// class MCDomain
// **************************************************************
/**
* @brief    Data container for domain data.
* [@author  Marius Erlen]
* [@date    2018-10-14]
*/
class MCDomain
{
public:
	DomainIndex domainIndex() const;
	void setDomainIndex(DomainIndex domainIndex);
	QString displayName() const;
	la::avdecc::UniqueIdentifier mediaClockDomainMaster() const;
	void setMediaClockDomainMaster(la::avdecc::UniqueIdentifier entityId);
	la::avdecc::entity::model::SamplingRate domainSamplingRate() const;
	void setDomainSamplingRate(la::avdecc::entity::model::SamplingRate samplingRate);

protected:
	int _domainIndex;
	la::avdecc::UniqueIdentifier _mcMaster;
	la::avdecc::entity::model::SamplingRate _domainSamplingRate;
};

// **************************************************************
// class MCEntityDomainMapping
// **************************************************************
/**
* @brief    Data container for mc domains and assigned entities.
* [@author  Marius Erlen]
* [@date    2018-10-14]
*/
class MCEntityDomainMapping
{
public:
	const DomainIndex findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier const mediaClockMasterId) noexcept;

	std::map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>>& entityMediaClockMasterMappings();
	std::map<DomainIndex, MCDomain>& mediaClockDomains();

protected:
	std::map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>> _entityMediaClockMasterMappings;
	std::map<DomainIndex, MCDomain> _mediaClockDomains;
};

// **************************************************************
// class MCDomainManager
// **************************************************************
/**
* @brief    This class provides helper methods to determine and track
*					media clock connections.
* [@author  Marius Erlen]
* [@date    2018-10-04]
*/
class MCDomainManager : public QObject
{
	Q_OBJECT

public:
	enum class Error
	{
		NoError,
		Recursive,
		UnknownEntity,
	};

	static MCDomainManager& getInstance() noexcept;

	/* media clock management helper functions */
	virtual const std::pair<la::avdecc::UniqueIdentifier, Error> getMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept = 0;
	virtual const MCEntityDomainMapping createMediaClockDomainModel() noexcept = 0;
	Q_SIGNAL void mediaClockConnectionsUpdate(std::vector<la::avdecc::UniqueIdentifier> entityIds);

protected:
	std::set<la::avdecc::UniqueIdentifier> _entities;
	MCEntityDomainMapping _currentMCDomainMapping;
};

constexpr bool operator!(MCDomainManager::Error const status)
{
	return status == MCDomainManager::Error::NoError;
}

} // namespace mediaClock
} // namespace avdecc
