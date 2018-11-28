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
using DomainIndex = std::uint64_t; /** A virtual index attributed to each domain */

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
	// Constructors
	MCDomain(DomainIndex const index, la::avdecc::UniqueIdentifier const mediaClockMaster = la::avdecc::UniqueIdentifier::getUninitializedUniqueIdentifier(), la::avdecc::entity::model::SamplingRate const samplingRate = la::avdecc::entity::model::getNullSamplingRate()) noexcept;

	// Getters
	DomainIndex getDomainIndex() const noexcept;
	la::avdecc::UniqueIdentifier getMediaClockDomainMaster() const noexcept;
	la::avdecc::entity::model::SamplingRate getDomainSamplingRate() const noexcept;

	// Setters
	void setMediaClockDomainMaster(la::avdecc::UniqueIdentifier const entityId) noexcept;
	void setDomainSamplingRate(la::avdecc::entity::model::SamplingRate const samplingRate) noexcept;

	// Other methods
	QString getDisplayName() const noexcept;

private:
	DomainIndex _domainIndex{ 0u }; /** Index of the domain */
	la::avdecc::UniqueIdentifier _mediaClockMasterId{}; /** ID of the clock master for the domain */
	la::avdecc::entity::model::SamplingRate _samplingRate{ la::avdecc::entity::model::getNullSamplingRate() }; /** Sampling rate for the whole domain */
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
	using Mappings = std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>, la::avdecc::UniqueIdentifier::hash>;
	using Domains = std::unordered_map<DomainIndex, MCDomain>;
	using Errors = std::unordered_map<la::avdecc::UniqueIdentifier, McDeterminationError, la::avdecc::UniqueIdentifier::hash>;

	MCEntityDomainMapping() noexcept;
	MCEntityDomainMapping(Mappings&& mappings, Domains&& domains, Errors&& errors) noexcept;

	std::optional<DomainIndex> const findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier mediaClockMasterId) noexcept;

	Mappings const& getEntityMediaClockMasterMappings() const noexcept;
	Domains const& getMediaClockDomains() const noexcept;
	Errors const& getEntityMcErrors() const noexcept;

private:
	Mappings _entityMediaClockMasterMappings{};
	Domains _mediaClockDomains{};
	Errors _entityMcErrors{};
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
};

constexpr bool operator!(McDeterminationError const error)
{
	return error == McDeterminationError::NoError;
}

} // namespace mediaClock
} // namespace avdecc
