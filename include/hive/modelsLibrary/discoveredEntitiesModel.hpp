/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/utils.hpp>

#include <QAbstractTableModel>
#include <QString>

#include <optional>
#include <type_traits>
#include <string>
#include <memory>
#include <map>
#include <set>

namespace hive
{
namespace modelsLibrary
{
class DiscoveredEntitiesAbstractTableModel;

/** DiscoveredEntities model itself (core engine) */
class DiscoveredEntitiesModel
{
public:
	enum class ProtocolCompatibility
	{
		NotCompliant,
		IEEE,
		Milan,
		MilanCertified,
		MilanWarning,
		MilanRedundant,
		MilanCertifiedRedundant,
		MilanWarningRedundant,
		Misbehaving,
	};

	enum class ExclusiveAccessState
	{
		NoAccess = 0, /**< Device is not exclusively accessed */
		NotSupported = 1, /**< Device does not support exclusive access */
		AccessOther = 2, /**< Device is exclusively accessed by another controller */
		AccessSelf = 3, /**< Device is exclusively accessed by us */
	};

	enum class ClockDomainLockedState
	{
		Unknown = 0, /**< Unknown state */
		Unlocked = 1, /**< Not Locked */
		Locked = 2, /**< Locked */
	};

	struct ExclusiveAccessInfo
	{
		ExclusiveAccessState state{ ExclusiveAccessState::NoAccess };
		la::avdecc::UniqueIdentifier exclusiveID{};
		QString tooltip{};
	};

	struct GptpInfo
	{
		std::optional<la::avdecc::UniqueIdentifier> grandmasterID{};
		std::optional<std::uint8_t> domainNumber{};
	};

	struct MediaClockReference
	{
		la::avdecc::controller::model::MediaClockChain mcChain{};
		QString referenceIDString{};
		QString referenceStatus{};
		bool isError{ false };
	};

	struct ClockDomainInfo
	{
		ClockDomainLockedState state{ ClockDomainLockedState::Unknown };
		QString tooltip{ "Not reported by the entity" };
	};

	struct Entity
	{
		// Static information
		la::avdecc::UniqueIdentifier entityID{};
		bool isAemSupported{ false };
		bool hasAnyConfigurationTree{ false };
		bool isVirtual{ false };
		bool areUnsolicitedNotificationsSupported{ false };
		la::avdecc::UniqueIdentifier entityModelID{};
		std::optional<QString> firmwareVersion{};
		std::optional<la::avdecc::entity::model::MemoryObjectIndex> firmwareUploadMemoryIndex{ std::nullopt };
		std::optional<la::avdecc::entity::model::MilanInfo> milanInfo{};
		std::map<la::avdecc::entity::model::AvbInterfaceIndex, la::networkInterface::MacAddress> macAddresses{};

		// Dynamic information
		QString name{}; /** Change triggers ChangedInfoFlag::Name */
		QString groupName{}; /** Change triggers ChangedInfoFlag::GroupName */
		bool isSubscribedToUnsol{ false }; /** Change triggers ChangedInfoFlag::SubscribedToUnsol */
		ProtocolCompatibility protocolCompatibility{ ProtocolCompatibility::NotCompliant }; /** Change triggers ChangedInfoFlag::Compatibility */
		la::avdecc::entity::EntityCapabilities entityCapabilities{}; /** Change triggers ChangedInfoFlag::EntityCapabilities */
		ExclusiveAccessInfo acquireInfo{}; /** Change triggers ChangedInfoFlag::AcquireState and/or ChangedInfoFlag::OwningController */
		ExclusiveAccessInfo lockInfo{}; /** Change triggers ChangedInfoFlag::LockState and/or ChangedInfoFlag::LockingController */
		std::map<la::avdecc::entity::model::AvbInterfaceIndex, GptpInfo> gptpInfo{}; /** Change triggers ChangedInfoFlag::GrandmasterID and/or ChangeInfoFlag::GPTPDomain */
		std::optional<la::avdecc::UniqueIdentifier> associationID{}; /** Change triggers ChangedInfoFlag::AssociationID */
		std::map<la::avdecc::entity::model::ClockDomainIndex, MediaClockReference> mediaClockReferences{}; /** Change triggers ChangedInfoFlag::MediaClockReferenceID and/or ChangedInfoFlag::MediaClockReferenceName */
		bool isIdentifying{ false }; /** Change triggers ChangedInfoFlag::Identification */
		bool hasStatisticsError{ false }; /** Change triggers ChangedInfoFlag::StatisticsError */
		bool hasRedundancyWarning{ false }; /** Change triggers ChangedInfoFlag::RedundancyWarning */
		ClockDomainInfo clockDomainInfo{}; /** Change triggers ChangedInfoFlag::ClockDomainLockState */
		std::set<la::avdecc::entity::model::StreamIndex> streamsWithErrorCounter{}; /** Change triggers ChangedInfoFlag::StreamInputCountersError */
		std::set<la::avdecc::entity::model::StreamIndex> streamsWithLatencyError{}; /** Change triggers ChangedInfoFlag::StreamInputLatencyError */

		// Methods
		bool hasAnyError() const noexcept
		{
			return hasStatisticsError || hasRedundancyWarning || !streamsWithErrorCounter.empty() || !streamsWithLatencyError.empty();
		}
	};

	using Model = DiscoveredEntitiesAbstractTableModel;

	DiscoveredEntitiesModel(Model* const model, QObject* parent = nullptr);
	virtual ~DiscoveredEntitiesModel();

	std::optional<std::reference_wrapper<Entity const>> entity(std::size_t const index) const noexcept;
	std::optional<std::reference_wrapper<Entity const>> entity(la::avdecc::UniqueIdentifier const& entityID) const noexcept;
	std::optional<std::size_t> indexOf(la::avdecc::UniqueIdentifier const& entityID) const noexcept;
	std::size_t entitiesCount() const noexcept;

private:
	class pImpl;
	std::unique_ptr<pImpl> _pImpl; //{ nullptr }; NSDMI for unique_ptr not supported by gcc (for incomplete type)
};

/** Qt Abstract TableModel Proxy for DiscoveredEntities */
class DiscoveredEntitiesAbstractTableModel : public QAbstractTableModel
{
public:
	/** Flag for info that changed */
	enum class ChangedInfoFlag : std::uint32_t
	{
		Name = 1u << 0,
		GroupName = 1u << 1,
		SubscribedToUnsol = 1u << 2,
		Compatibility = 1u << 3,
		EntityCapabilities = 1u << 4,
		AcquireState = 1u << 5,
		OwningController = 1u << 6,
		LockedState = 1u << 7,
		LockingController = 1u << 8,
		GrandmasterID = 1u << 9,
		GPTPDomain = 1u << 10,
		InterfaceIndex = 1u << 11,
		MacAddress = 1u << 12,
		AssociationID = 1u << 13,
		MediaClockReferenceID = 1u << 14,
		MediaClockReferenceName = 1u << 15,
		ClockDomainLockState = 1u << 16,
		Identification = 1u << 17,
		StatisticsError = 1u << 18,
		RedundancyWarning = 1u << 19,
		StreamInputCountersError = 1u << 20,
		StreamInputLatencyError = 1u << 21,
	};
	using ChangedInfoFlags = la::avdecc::utils::EnumBitfield<ChangedInfoFlag>;

	// Notifications for DiscoveredEntitiesModel changes
	virtual void entityInfoChanged(std::size_t const /*index*/, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& /*entity*/, ChangedInfoFlags const /*changedInfoFlags*/) noexcept {}

private:
	friend class DiscoveredEntitiesModel;
};

} // namespace modelsLibrary
} // namespace hive
