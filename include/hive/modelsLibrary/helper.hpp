/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <la/avdecc/utils.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <hive/modelsLibrary/discoveredEntitiesModel.hpp>
#include <QString>

#include <sstream>
#include <functional>
#include <ios>
#include <iomanip>
#include <optional>
#include <cstdint>

namespace hive
{
namespace modelsLibrary
{
namespace helper
{
template<typename T>
inline QString toHexQString(T const v, bool const zeroFilled = false, bool const upper = false) noexcept
{
	static_assert(std::numeric_limits<T>::is_integer, "toHexQString requires an integer value");

	try
	{
		std::stringstream stream;
		stream << "0x";
		if (zeroFilled)
			stream << std::setfill('0') << std::setw(sizeof(T) * 2);
		if (upper)
			stream << std::uppercase;
		stream << std::hex << la::avdecc::utils::forceNumeric(v);
		return QString::fromStdString(stream.str());
	}
	catch (...)
	{
		return "[Invalid Conversion]";
	}
}

QString toUpperCamelCase(std::string const& text) noexcept;
/** Warms the getVendorName() OUI database cache up from a background thread (the parse is slow enough to noticeably freeze the caller otherwise). Call once at application startup. */
void warmUpVendorNamesCache() noexcept;
QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept;
QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier);
QString interfaceTypeName(la::avdecc::controller::InterfaceType const interfaceType) noexcept;
/** Maps an entity's AVB interface index to the redundant controller interface type (Primary/Secondary). Milan redundancy (Milan 1.0+) defines AVB interface index 0 as the primary network and index 1 as the secondary network; returns std::nullopt for any other case (non-Milan entity or non-redundant interface index). */
std::optional<la::avdecc::controller::InterfaceType> redundantInterfaceType(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::MilanVersion const& milanVersion) noexcept;
QString macAddressToString(la::networkInterface::MacAddress const& macAddress);
/** Converts a propagation delay (in nanoseconds) to a displayable string, either as a time ("530 ns", "1.23 microseconds") or as the estimated attached cable length based on the signal propagation speed in a copper cable (530 ns per 100 m), displayed as "< 5 m" below 5 meters (the estimate is too coarse there to show a value). */
QString propagationDelayToString(std::uint32_t const delayNsec, bool const asDistance) noexcept;
/** Converts a propagation delay (in nanoseconds) to a displayable string showing both the time and the estimated attached cable length, eg. "530 ns (100 m)". */
QString propagationDelayWithDistanceToString(std::uint32_t const delayNsec) noexcept;
QString localizedString(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::LocalizedStringReference const stringReference) noexcept;
QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept;

template<class NodeType>
QString objectName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, NodeType const& node) noexcept
{
	if (node.dynamicModel.objectName.empty())
	{
		return localizedString(*controlledEntity, configurationIndex, node.staticModel.localizedDescription);
	}

	return QString::fromStdString(node.dynamicModel.objectName);
}

bool constexpr isConnectedToTalker(la::avdecc::entity::model::StreamIdentification const& talkerStream, la::avdecc::entity::model::StreamInputConnectionInfo const& info) noexcept
{
	return info.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::Connected && info.talkerStream == talkerStream;
}

bool constexpr isFastConnectingToTalker(la::avdecc::entity::model::StreamIdentification const& talkerStream, la::avdecc::entity::model::StreamInputConnectionInfo const& info) noexcept
{
	return info.state == la::avdecc::entity::model::StreamInputConnectionInfo::State::FastConnecting && info.talkerStream == talkerStream;
}

QString entityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString smartEntityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString smartEntityName(hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& entity) noexcept;
QString groupName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString outputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept;
QString inputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept;
QString redundantOutputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;
QString redundantInputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;
QString getComputerName() noexcept;

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
