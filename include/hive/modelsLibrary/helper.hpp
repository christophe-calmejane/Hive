/*
* Copyright (C) 2017-2020, Emilien Vallot, Christophe Calmejane and other contributors

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
#include <QString>

#include <sstream>
#include <functional>
#include <ios>
#include <iomanip>

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
QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept;
QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier);
QString localizedString(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::LocalizedStringReference const stringReference) noexcept;
QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept;

template<class NodeType>
QString objectName(la::avdecc::controller::ControlledEntity const* const controlledEntity, NodeType const& node) noexcept
{
	if (node.dynamicModel->objectName.empty())
	{
		return localizedString(*controlledEntity, node.staticModel->localizedDescription);
	}

	return node.dynamicModel->objectName.data();
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
QString groupName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString outputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept;
QString inputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept;
QString redundantOutputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;
QString redundantInputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept;

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
