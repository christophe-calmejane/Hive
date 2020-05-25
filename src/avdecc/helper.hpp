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

#include <QString>
#include <QObject>
#include <QIcon>
#include <functional>
#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/streamFormatInfo.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/logger.hpp>

namespace avdecc
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

QString protocolInterfaceTypeName(la::avdecc::protocol::ProtocolInterface::Type const& protocolInterfaceType);

QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier);

QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept;
template<class NodeType>
QString objectName(la::avdecc::controller::ControlledEntity const* const controlledEntity, NodeType const& node) noexcept
{
	return node.dynamicModel->objectName.empty() ? controlledEntity->getLocalizedString(node.staticModel->localizedDescription).data() : node.dynamicModel->objectName.data();
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

QString descriptorTypeToString(la::avdecc::entity::model::DescriptorType const& descriptorType) noexcept;
QString acquireStateToString(la::avdecc::controller::model::AcquireState const& acquireState, la::avdecc::UniqueIdentifier const& owningController) noexcept;
QString lockStateToString(la::avdecc::controller::model::LockState const& lockState, la::avdecc::UniqueIdentifier const& lockingController) noexcept;

QString samplingRateToString(la::avdecc::entity::model::StreamFormatInfo::SamplingRate const& samplingRate) noexcept;
QString streamFormatToString(la::avdecc::entity::model::StreamFormatInfo const& format) noexcept;
QString clockSourceToString(la::avdecc::controller::model::ClockSourceNode const& node) noexcept;

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::PortFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags) noexcept;
QString flagsToString(la::avdecc::entity::StreamInfoFlagsEx const flags) noexcept;
QString flagsToString(la::avdecc::entity::MilanInfoFeaturesFlags const flags) noexcept;

QString probingStatusToString(la::avdecc::entity::model::ProbingStatus const status) noexcept;

QString capabilitiesToString(la::avdecc::entity::EntityCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::TalkerCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::ListenerCapabilities const caps) noexcept;
QString capabilitiesToString(la::avdecc::entity::ControllerCapabilities const caps) noexcept;

QString clockSourceTypeToString(la::avdecc::entity::model::ClockSourceType const type) noexcept;
QString audioClusterFormatToString(la::avdecc::entity::model::AudioClusterFormat const format) noexcept;

QString memoryObjectTypeToString(la::avdecc::entity::model::MemoryObjectType const type) noexcept;

QString certificationVersionToString(std::uint32_t const certificationVersion) noexcept;

QString loggerLayerToString(la::avdecc::logger::Layer const layer) noexcept;
QString loggerLevelToString(la::avdecc::logger::Level const& level) noexcept;

QString toUpperCamelCase(std::string const& text) noexcept;

QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept;

QIcon interfaceTypeIcon(la::avdecc::networkInterface::Interface::Type const type) noexcept;

} // namespace helper
} // namespace avdecc
