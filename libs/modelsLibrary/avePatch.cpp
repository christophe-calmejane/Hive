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

#include "avePatch.hpp"

namespace hive
{
namespace modelsLibrary
{
namespace avePatch
{
namespace
{
// Key names of the la_avdecc JSON serializer (la::avdecc::entity::model::keyName is not part of the public API, the values are checked by the unit tests against the serializer output)
constexpr auto EntityDescriptorPointer = "/entity_model/entity_descriptor";
constexpr auto ConfigurationDescriptorsKey = "configuration_descriptors";
constexpr auto AudioUnitDescriptorsKey = "audio_unit_descriptors";
constexpr auto StreamInputDescriptorsKey = "stream_input_descriptors";
constexpr auto StreamOutputDescriptorsKey = "stream_output_descriptors";
constexpr auto JackInputDescriptorsKey = "jack_input_descriptors";
constexpr auto JackOutputDescriptorsKey = "jack_output_descriptors";
constexpr auto AvbInterfaceDescriptorsKey = "avb_interface_descriptors";
constexpr auto ClockSourceDescriptorsKey = "clock_source_descriptors";
constexpr auto MemoryObjectDescriptorsKey = "memory_object_descriptors";
constexpr auto StreamPortInputDescriptorsKey = "stream_port_input_descriptors";
constexpr auto StreamPortOutputDescriptorsKey = "stream_port_output_descriptors";
constexpr auto AudioClusterDescriptorsKey = "audio_cluster_descriptors";
constexpr auto AudioMapDescriptorsKey = "audio_map_descriptors";
constexpr auto ControlDescriptorsKey = "control_descriptors";
constexpr auto ClockDomainDescriptorsKey = "clock_domain_descriptors";
constexpr auto TimingDescriptorsKey = "timing_descriptors";
constexpr auto PtpInstanceDescriptorsKey = "ptp_instance_descriptors";
constexpr auto PtpPortDescriptorsKey = "ptp_port_descriptors";
constexpr auto DynamicModelKey = "dynamic";

/** Returns the pointer of a descriptor stored in the array of its parent: the serializer writes the descriptors of a parent from its base index up, so the position is the index minus the base. */
template<typename NodeMap>
std::optional<QString> childPointer(QString const& parentPointer, char const* const arrayKey, NodeMap const& nodes, la::avdecc::entity::model::DescriptorIndex const baseIndex, la::avdecc::entity::model::DescriptorIndex const descriptorIndex) noexcept
{
	if (descriptorIndex < baseIndex || nodes.count(descriptorIndex) == 0)
	{
		return std::nullopt;
	}
	return QString{ "%1/%2/%3" }.arg(parentPointer, arrayKey).arg(descriptorIndex - baseIndex);
}

/** Returns the pointer of a control, searching the parents that can hold controls (the configuration itself, audio units, stream ports, jacks, PTP instances). */
std::optional<QString> controlPointer(QString const& configurationPointer, la::avdecc::controller::model::ConfigurationNode const& configurationNode, la::avdecc::entity::model::ControlIndex const controlIndex) noexcept
{
	if (auto const pointer = childPointer(configurationPointer, ControlDescriptorsKey, configurationNode.controls, 0u, controlIndex))
	{
		return pointer;
	}
	for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
	{
		auto const audioUnitPointer = QString{ "%1/%2/%3" }.arg(configurationPointer, AudioUnitDescriptorsKey).arg(audioUnitIndex);
		if (auto const pointer = childPointer(audioUnitPointer, ControlDescriptorsKey, audioUnitNode.controls, audioUnitNode.staticModel.baseControl, controlIndex))
		{
			return pointer;
		}
		for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortInputs)
		{
			if (auto const streamPortPointer = childPointer(audioUnitPointer, StreamPortInputDescriptorsKey, audioUnitNode.streamPortInputs, audioUnitNode.staticModel.baseStreamInputPort, streamPortIndex))
			{
				if (auto const pointer = childPointer(*streamPortPointer, ControlDescriptorsKey, streamPortNode.controls, streamPortNode.staticModel.baseControl, controlIndex))
				{
					return pointer;
				}
			}
		}
		for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortOutputs)
		{
			if (auto const streamPortPointer = childPointer(audioUnitPointer, StreamPortOutputDescriptorsKey, audioUnitNode.streamPortOutputs, audioUnitNode.staticModel.baseStreamOutputPort, streamPortIndex))
			{
				if (auto const pointer = childPointer(*streamPortPointer, ControlDescriptorsKey, streamPortNode.controls, streamPortNode.staticModel.baseControl, controlIndex))
				{
					return pointer;
				}
			}
		}
	}
	for (auto const& [jackIndex, jackNode] : configurationNode.jackInputs)
	{
		if (auto const pointer = childPointer(QString{ "%1/%2/%3" }.arg(configurationPointer, JackInputDescriptorsKey).arg(jackIndex), ControlDescriptorsKey, jackNode.controls, jackNode.staticModel.baseControl, controlIndex))
		{
			return pointer;
		}
	}
	for (auto const& [jackIndex, jackNode] : configurationNode.jackOutputs)
	{
		if (auto const pointer = childPointer(QString{ "%1/%2/%3" }.arg(configurationPointer, JackOutputDescriptorsKey).arg(jackIndex), ControlDescriptorsKey, jackNode.controls, jackNode.staticModel.baseControl, controlIndex))
		{
			return pointer;
		}
	}
	for (auto const& [ptpInstanceIndex, ptpInstanceNode] : configurationNode.ptpInstances)
	{
		if (auto const pointer = childPointer(QString{ "%1/%2/%3" }.arg(configurationPointer, PtpInstanceDescriptorsKey).arg(ptpInstanceIndex), ControlDescriptorsKey, ptpInstanceNode.controls, ptpInstanceNode.staticModel.baseControl, controlIndex))
		{
			return pointer;
		}
	}
	return std::nullopt;
}

/** Returns the pointer of a stream port of an audio unit. */
std::optional<QString> streamPortPointer(QString const& configurationPointer, la::avdecc::controller::model::ConfigurationNode const& configurationNode, bool const isInput, la::avdecc::entity::model::StreamPortIndex const streamPortIndex) noexcept
{
	for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
	{
		auto const audioUnitPointer = QString{ "%1/%2/%3" }.arg(configurationPointer, AudioUnitDescriptorsKey).arg(audioUnitIndex);
		auto const pointer = isInput ? childPointer(audioUnitPointer, StreamPortInputDescriptorsKey, audioUnitNode.streamPortInputs, audioUnitNode.staticModel.baseStreamInputPort, streamPortIndex) : childPointer(audioUnitPointer, StreamPortOutputDescriptorsKey, audioUnitNode.streamPortOutputs, audioUnitNode.staticModel.baseStreamOutputPort, streamPortIndex);
		if (pointer)
		{
			return pointer;
		}
	}
	return std::nullopt;
}

/** Returns the pointer of an audio cluster or an audio map (the given members of the stream port node and of its static model), searching the stream ports of every audio unit. */
template<typename NodeMap, typename BaseIndexType>
std::optional<QString> streamPortChildPointer(QString const& configurationPointer, la::avdecc::controller::model::ConfigurationNode const& configurationNode, char const* const arrayKey, NodeMap la::avdecc::controller::model::StreamPortNode::*const nodesField, BaseIndexType la::avdecc::entity::model::StreamPortNodeStaticModel::*const baseIndexField, la::avdecc::entity::model::DescriptorIndex const descriptorIndex) noexcept
{
	for (auto const& [audioUnitIndex, audioUnitNode] : configurationNode.audioUnits)
	{
		for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortInputs)
		{
			if (auto const portPointer = streamPortPointer(configurationPointer, configurationNode, true, streamPortIndex))
			{
				if (auto const pointer = childPointer(*portPointer, arrayKey, streamPortNode.*nodesField, streamPortNode.staticModel.*baseIndexField, descriptorIndex))
				{
					return pointer;
				}
			}
		}
		for (auto const& [streamPortIndex, streamPortNode] : audioUnitNode.streamPortOutputs)
		{
			if (auto const portPointer = streamPortPointer(configurationPointer, configurationNode, false, streamPortIndex))
			{
				if (auto const pointer = childPointer(*portPointer, arrayKey, streamPortNode.*nodesField, streamPortNode.staticModel.*baseIndexField, descriptorIndex))
				{
					return pointer;
				}
			}
		}
	}
	return std::nullopt;
}
} // namespace

QString entityDescriptorPointer() noexcept
{
	return EntityDescriptorPointer;
}

QString configurationPointer(la::avdecc::entity::model::ConfigurationIndex const configurationIndex) noexcept
{
	return QString{ "%1/%2/%3" }.arg(EntityDescriptorPointer, ConfigurationDescriptorsKey).arg(configurationIndex);
}

QString statePointer() noexcept
{
	return "/state";
}

QString diagnosticsPointer() noexcept
{
	return "/diagnostics";
}

QString compatibilityFlagsPointer() noexcept
{
	return "/compatibility_flags";
}

QString milanCompatibilityVersionPointer() noexcept
{
	return "/milan_compatibility_version";
}

QString entityCapabilitiesPointer() noexcept
{
	return "/adp_information/common/entity_capabilities";
}

std::optional<QString> descriptorPointer(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex) noexcept
{
	using DescriptorType = la::avdecc::entity::model::DescriptorType;

	if (descriptorType == DescriptorType::Entity)
	{
		return entityDescriptorPointer();
	}
	if (descriptorType == DescriptorType::Configuration)
	{
		return configurationPointer(descriptorIndex);
	}
	try
	{
		auto const& configurationNode = controlledEntity.getConfigurationNode(configurationIndex);
		auto const configPointer = configurationPointer(configurationIndex);
		switch (descriptorType)
		{
			case DescriptorType::AudioUnit:
				return childPointer(configPointer, AudioUnitDescriptorsKey, configurationNode.audioUnits, 0u, descriptorIndex);
			case DescriptorType::StreamInput:
				return childPointer(configPointer, StreamInputDescriptorsKey, configurationNode.streamInputs, 0u, descriptorIndex);
			case DescriptorType::StreamOutput:
				return childPointer(configPointer, StreamOutputDescriptorsKey, configurationNode.streamOutputs, 0u, descriptorIndex);
			case DescriptorType::JackInput:
				return childPointer(configPointer, JackInputDescriptorsKey, configurationNode.jackInputs, 0u, descriptorIndex);
			case DescriptorType::JackOutput:
				return childPointer(configPointer, JackOutputDescriptorsKey, configurationNode.jackOutputs, 0u, descriptorIndex);
			case DescriptorType::AvbInterface:
				return childPointer(configPointer, AvbInterfaceDescriptorsKey, configurationNode.avbInterfaces, 0u, descriptorIndex);
			case DescriptorType::ClockSource:
				return childPointer(configPointer, ClockSourceDescriptorsKey, configurationNode.clockSources, 0u, descriptorIndex);
			case DescriptorType::MemoryObject:
				return childPointer(configPointer, MemoryObjectDescriptorsKey, configurationNode.memoryObjects, 0u, descriptorIndex);
			case DescriptorType::StreamPortInput:
				return streamPortPointer(configPointer, configurationNode, true, descriptorIndex);
			case DescriptorType::StreamPortOutput:
				return streamPortPointer(configPointer, configurationNode, false, descriptorIndex);
			case DescriptorType::AudioCluster:
				return streamPortChildPointer(configPointer, configurationNode, AudioClusterDescriptorsKey, &la::avdecc::controller::model::StreamPortNode::audioClusters, &la::avdecc::entity::model::StreamPortNodeStaticModel::baseCluster, descriptorIndex);
			case DescriptorType::AudioMap:
				return streamPortChildPointer(configPointer, configurationNode, AudioMapDescriptorsKey, &la::avdecc::controller::model::StreamPortNode::audioMaps, &la::avdecc::entity::model::StreamPortNodeStaticModel::baseMap, descriptorIndex);
			case DescriptorType::Control:
				return controlPointer(configPointer, configurationNode, descriptorIndex);
			case DescriptorType::ClockDomain:
				return childPointer(configPointer, ClockDomainDescriptorsKey, configurationNode.clockDomains, 0u, descriptorIndex);
			case DescriptorType::Timing:
				return childPointer(configPointer, TimingDescriptorsKey, configurationNode.timings, 0u, descriptorIndex);
			case DescriptorType::PtpInstance:
				return childPointer(configPointer, PtpInstanceDescriptorsKey, configurationNode.ptpInstances, 0u, descriptorIndex);
			case DescriptorType::PtpPort:
				for (auto const& [ptpInstanceIndex, ptpInstanceNode] : configurationNode.ptpInstances)
				{
					if (auto const pointer = childPointer(QString{ "%1/%2/%3" }.arg(configPointer, PtpInstanceDescriptorsKey).arg(ptpInstanceIndex), PtpPortDescriptorsKey, ptpInstanceNode.ptpPorts, ptpInstanceNode.staticModel.basePtpPort, descriptorIndex))
					{
						return pointer;
					}
				}
				return std::nullopt;
			default:
				// Locales and strings have no dynamic model, the other descriptor types are not serialized
				return std::nullopt;
		}
	}
	catch (...)
	{
		// Unknown configuration, or entity without an entity model
		return std::nullopt;
	}
}

std::optional<QString> dynamicModelPointer(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::DescriptorType const descriptorType, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, char const* const key) noexcept
{
	if (auto const pointer = descriptorPointer(controlledEntity, configurationIndex, descriptorType, descriptorIndex))
	{
		return QString{ "%1/%2/%3" }.arg(*pointer, DynamicModelKey, key);
	}
	return std::nullopt;
}

} // namespace avePatch
} // namespace modelsLibrary
} // namespace hive
