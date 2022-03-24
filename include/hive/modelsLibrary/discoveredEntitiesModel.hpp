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

#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/utils.hpp>

#include <QAbstractTableModel>
#include <QString>

#include <optional>
#include <type_traits>
#include <string>
#include <memory>
#include <map>

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
		MilanRedundant,
		MilanCertifiedRedundant,
		Misbehaving,
	};

	struct GptpInfo
	{
		std::optional<la::avdecc::UniqueIdentifier> grandmasterID;
		std::optional<std::uint8_t> domainNumber;
	};
	struct Entity
	{
		// Static information
		la::avdecc::UniqueIdentifier entityID{};
		bool isAemSupported{ false };
		la::avdecc::UniqueIdentifier entityModelID{};
		std::optional<QString> firmwareVersion{};
		std::optional<la::avdecc::entity::model::MemoryObjectIndex> firmwareUploadMemoryIndex{ std::nullopt };
		std::optional<la::avdecc::entity::model::MilanInfo> milanInfo{};

		// Dynamic information
		QString name{};
		QString groupName{};
		bool isSubscribedToUnsol{ false };
		ProtocolCompatibility protocolCompatibility{ ProtocolCompatibility::NotCompliant };
		la::avdecc::entity::EntityCapabilities entityCapabilities{};
		la::avdecc::controller::model::AcquireState acquireState{ la::avdecc::controller::model::AcquireState::Undefined };
		la::avdecc::UniqueIdentifier owningController{};
		la::avdecc::controller::model::LockState lockState{ la::avdecc::controller::model::LockState::Undefined };
		la::avdecc::UniqueIdentifier lockingController{};
		std::map<la::avdecc::entity::model::AvbInterfaceIndex, GptpInfo> gptpInfo{};
		std::optional<la::avdecc::UniqueIdentifier> associationID{};
	};

	using Model = DiscoveredEntitiesAbstractTableModel;

	DiscoveredEntitiesModel(Model* const model, QObject* parent = nullptr);
	virtual ~DiscoveredEntitiesModel();

	std::optional<std::reference_wrapper<Entity const>> entity(std::size_t const index) const noexcept;
	std::optional<std::reference_wrapper<Entity const>> entity(la::avdecc::UniqueIdentifier const& entityID) const noexcept;
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
		GptpInfo = 1u << 9,
		AssociationID = 1u << 10,
	};
	using ChangedInfoFlags = la::avdecc::utils::EnumBitfield<ChangedInfoFlag>;

	/** Flag for error counter that changed */
	enum class ChangedErrorCounterFlag : std::uint32_t
	{
		Statistics = 1u << 0,
		StreamInputCounters = 1u << 1,
		StreamInputLatency = 1u << 2,
	};
	using ChangedErrorCounterFlags = la::avdecc::utils::EnumBitfield<ChangedErrorCounterFlag>;

	// Notifications for DiscoveredEntitiesModel changes
	virtual void entityInfoChanged(std::size_t const /*index*/, hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& /*entity*/, ChangedInfoFlags const /*changedInfoFlags*/) noexcept {}
	virtual void identificationStarted(std::size_t const /*index*/) noexcept {}
	virtual void identificationStopped(std::size_t const /*index*/) noexcept {}
	virtual void entityErrorCountersChanged(std::size_t const /*index*/, ChangedErrorCounterFlags const /*changedErrorCounterFlags*/) noexcept {}

private:
	friend class DiscoveredEntitiesModel;
};

} // namespace modelsLibrary
} // namespace hive
