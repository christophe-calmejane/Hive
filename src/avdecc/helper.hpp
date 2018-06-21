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

#include <QString>
#include <QObject>
#include <functional>
#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/streamFormat.hpp>
#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include <la/avdecc/logger.hpp>

namespace avdecc
{
namespace helper
{

template<typename T>
QString toHexQString(T const v, bool const zeroFilled = false, bool const upper = false) noexcept
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
		stream << std::hex << la::avdecc::forceNumeric(v);
		return QString::fromStdString(stream.str());
	}
	catch (...)
	{
		return "[Invalid Conversion]";
	}
}

QString protocolInterfaceTypeName(la::avdecc::EndStation::ProtocolInterfaceType const& protocolInterfaceType);

QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier);

QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node);
template<class NodeType>
QString objectName(la::avdecc::controller::ControlledEntity const* const controlledEntity, NodeType const& node)
{
	return node.dynamicModel->objectName.empty() ? controlledEntity->getLocalizedString(node.staticModel->localizedDescription).data() : node.dynamicModel->objectName.data();
}

QString smartEntityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString entityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;
QString groupName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept;

QString descriptorTypeToString(la::avdecc::entity::model::DescriptorType const& descriptorType);
QString acquireStateToString(la::avdecc::controller::model::AcquireState const& acquireState);

QString samplingRateToString(la::avdecc::entity::model::StreamFormatInfo::SamplingRate const& samplingRate);
QString streamFormatToString(la::avdecc::entity::model::StreamFormatInfo const& format);
QString clockSourceToString(la::avdecc::controller::model::ClockSourceNode const& node);

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags);
QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags);
QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags);
QString flagsToString(la::avdecc::entity::PortFlags const flags);
QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags);

QString clockSourceTypeToString(la::avdecc::entity::model::ClockSourceType const type);
QString audioClusterFormatToString(la::avdecc::entity::model::AudioClusterFormat const format);

QString memoryObjectTypeToString(la::avdecc::entity::model::MemoryObjectType const type);

QString loggerLayerToString(la::avdecc::logger::Layer const layer);
QString loggerLevelToString(la::avdecc::logger::Level const& level);

} // namespace helper
} // namespace avdecc
