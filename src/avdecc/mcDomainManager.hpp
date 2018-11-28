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
#include <optional>
#include <QObject>
#include <QMap>


namespace avdecc
{
namespace mediaClock
{
using DomainIndex = std::uint64_t;

enum class McDeterminationError
{
	NoError,
	NotSupported,
	Recursive,
	StreamNotConnected,
	ExternalClockSource,
	UnknownEntity,
};

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
	DomainIndex getDomainIndex() const noexcept;
	void setDomainIndex(DomainIndex domainIndex) noexcept;
	QString getDisplayName() const noexcept;
	la::avdecc::UniqueIdentifier getMediaClockDomainMaster() const noexcept;
	void setMediaClockDomainMaster(la::avdecc::UniqueIdentifier entityId) noexcept;
	la::avdecc::entity::model::SamplingRate getDomainSamplingRate() const noexcept;
	void setDomainSamplingRate(la::avdecc::entity::model::SamplingRate samplingRate) noexcept;

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
	std::optional<DomainIndex> const findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier mediaClockMasterId) noexcept;

	std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>, la::avdecc::UniqueIdentifier::hash>& getEntityMediaClockMasterMappings() noexcept;
	std::unordered_map<la::avdecc::UniqueIdentifier, McDeterminationError, la::avdecc::UniqueIdentifier::hash>& getEntityMcErrors() noexcept;
	std::unordered_map<DomainIndex, MCDomain>& getMediaClockDomains() noexcept;

protected:
protected:
	std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>, la::avdecc::UniqueIdentifier::hash> _entityMediaClockMasterMappings;
	std::unordered_map<la::avdecc::UniqueIdentifier, McDeterminationError, la::avdecc::UniqueIdentifier::hash> _entityMcErrors;
	std::unordered_map<DomainIndex, MCDomain> _mediaClockDomains;
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
	static MCDomainManager& getInstance() noexcept;

	/* media clock management helper functions */
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> getMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept = 0;
	virtual MCEntityDomainMapping createMediaClockDomainModel() noexcept = 0;
	Q_SIGNAL void mediaClockConnectionsUpdate(std::vector<la::avdecc::UniqueIdentifier> entityIds);
	Q_SIGNAL void mcMasterNameChanged(std::vector<la::avdecc::UniqueIdentifier> entityIds);

protected:
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> findMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept = 0;

protected:
	std::set<la::avdecc::UniqueIdentifier> _entities;
	MCEntityDomainMapping _currentMCDomainMapping;
};

constexpr bool operator!(McDeterminationError const error)
{
	return error == McDeterminationError::NoError;
}

} // namespace mediaClock
} // namespace avdecc
