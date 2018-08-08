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

#include "helper.hpp"
#include <la/avdecc/utils.hpp>

namespace avdecc
{
namespace helper
{
QString protocolInterfaceTypeName(la::avdecc::EndStation::ProtocolInterfaceType const& protocolInterfaceType)
{
	switch (protocolInterfaceType)
	{
		case la::avdecc::EndStation::ProtocolInterfaceType::None:
			return "None";
		case la::avdecc::EndStation::ProtocolInterfaceType::PCap:
			return "PCap";
		case la::avdecc::EndStation::ProtocolInterfaceType::MacOSNative:
			return "MacOS Native";
		case la::avdecc::EndStation::ProtocolInterfaceType::Proxy:
			return "Proxy";
		case la::avdecc::EndStation::ProtocolInterfaceType::Virtual:
			return "Virtual";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier)
{
	return toHexQString(identifier.getValue(), true, true);
}

QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node)
{
	return node.dynamicModel->objectName.empty() ? controlledEntity->getLocalizedString(node.descriptorIndex, node.staticModel->localizedDescription).data() : node.dynamicModel->objectName.data();
}

QString smartEntityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	QString name;

	name = entityName(controlledEntity);

	if (name.isEmpty())
		name = uniqueIdentifierToString(controlledEntity.getEntity().getEntityID());

	return name;
}

QString entityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	try
	{
		auto const& entity = controlledEntity.getEntity();

		if (la::avdecc::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
		{
			return controlledEntity.getEntityNode().dynamicModel->entityName.data();
		}
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString groupName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	try
	{
		auto const& entity = controlledEntity.getEntity();

		if (la::avdecc::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
		{
			return controlledEntity.getEntityNode().dynamicModel->groupName.data();
		}
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString descriptorTypeToString(la::avdecc::entity::model::DescriptorType const& descriptorType)
{
	switch (descriptorType)
	{
		case la::avdecc::entity::model::DescriptorType::Entity:
			return "ENTITY";
		case la::avdecc::entity::model::DescriptorType::Configuration:
			return "CONFIGURATION";
		case la::avdecc::entity::model::DescriptorType::AudioUnit:
			return "AUDIO_UNIT";
		case la::avdecc::entity::model::DescriptorType::VideoUnit:
			return "VIDEO_UNIT";
		case la::avdecc::entity::model::DescriptorType::SensorUnit:
			return "SENSOR_UNIT";
		case la::avdecc::entity::model::DescriptorType::StreamInput:
			return "STREAM_INPUT";
		case la::avdecc::entity::model::DescriptorType::StreamOutput:
			return "STREAM_OUTPUT";
		case la::avdecc::entity::model::DescriptorType::JackInput:
			return "JACK_INPUT";
		case la::avdecc::entity::model::DescriptorType::JackOutput:
			return "JACK_OUTPUT";
		case la::avdecc::entity::model::DescriptorType::AvbInterface:
			return "AVB_INTERFACE";
		case la::avdecc::entity::model::DescriptorType::ClockSource:
			return "CLOCK_SOURCE";
		case la::avdecc::entity::model::DescriptorType::MemoryObject:
			return "MEMORY_OBJECT";
		case la::avdecc::entity::model::DescriptorType::Locale:
			return "LOCALE";
		case la::avdecc::entity::model::DescriptorType::Strings:
			return "STRINGS";
		case la::avdecc::entity::model::DescriptorType::StreamPortInput:
			return "STREAM_PORT_INPUT";
		case la::avdecc::entity::model::DescriptorType::StreamPortOutput:
			return "STREAM_PORT_OUTPUT";
		case la::avdecc::entity::model::DescriptorType::ExternalPortInput:
			return "EXTRENAL_PORT_INPUT";
		case la::avdecc::entity::model::DescriptorType::ExternalPortOutput:
			return "EXTRENAL_PORT_OUTPUT";
		case la::avdecc::entity::model::DescriptorType::InternalPortInput:
			return "INTERNAL_PORT_INPUT";
		case la::avdecc::entity::model::DescriptorType::InternalPortOutput:
			return "INTERNAL_PORT_OUTPUT";
		case la::avdecc::entity::model::DescriptorType::AudioCluster:
			return "AUDIO_CLUSTER";
		case la::avdecc::entity::model::DescriptorType::VideoCluster:
			return "VIDEO_CLUSTER";
		case la::avdecc::entity::model::DescriptorType::SensorCluster:
			return "SENSOR_CLUSTER";
		case la::avdecc::entity::model::DescriptorType::AudioMap:
			return "AUDIO_MAP";
		case la::avdecc::entity::model::DescriptorType::VideoMap:
			return "VIDEO_MAP";
		case la::avdecc::entity::model::DescriptorType::SensorMap:
			return "SENSOR_MAP";
		case la::avdecc::entity::model::DescriptorType::Control:
			return "CONTROL";
		case la::avdecc::entity::model::DescriptorType::SignalSelector:
			return "SIGNAL_SELECTOR";
		case la::avdecc::entity::model::DescriptorType::Mixer:
			return "MIXER";
		case la::avdecc::entity::model::DescriptorType::Matrix:
			return "MATRIX";
		case la::avdecc::entity::model::DescriptorType::MatrixSignal:
			return "MATRIX_SIGNAL";
		case la::avdecc::entity::model::DescriptorType::SignalSplitter:
			return "SIGNAL_SPLITTER";
		case la::avdecc::entity::model::DescriptorType::SignalCombiner:
			return "SIGNAL_COMBINER";
		case la::avdecc::entity::model::DescriptorType::SignalDemultiplexer:
			return "SIGNAL_DEMULTIPLEXER";
		case la::avdecc::entity::model::DescriptorType::SignalMultiplexer:
			return "SIGNAL_MULTIPLEXER";
		case la::avdecc::entity::model::DescriptorType::SignalTranscoder:
			return "SIGNAL_TRANSCODER";
		case la::avdecc::entity::model::DescriptorType::ClockDomain:
			return "CLOCK_DOMAIN";
		case la::avdecc::entity::model::DescriptorType::ControlBlock:
			return "CONTROL_BLOCK";
		case la::avdecc::entity::model::DescriptorType::Invalid:
			return "INVALID";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString acquireStateToString(la::avdecc::controller::model::AcquireState const& acquireState)
{
	switch (acquireState)
	{
		case la::avdecc::controller::model::AcquireState::Undefined:
			return "Undefined";
		case la::avdecc::controller::model::AcquireState::NotAcquired:
			return "Not Acquired";
		case la::avdecc::controller::model::AcquireState::TryAcquire:
			return "Try Acquire";
		case la::avdecc::controller::model::AcquireState::Acquired:
			return "Acquired";
		case la::avdecc::controller::model::AcquireState::AcquiredByOther:
			return "Acquired By Other";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString samplingRateToString(la::avdecc::entity::model::StreamFormatInfo::SamplingRate const& samplingRate)
{
	switch (samplingRate)
	{
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_8:
			return "8kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_16:
			return "16kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_32:
			return "32kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_44_1:
			return "44.1kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_48:
			return "48kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_88_2:
			return "88.2kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_96:
			return "96kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_176_4:
			return "176.4kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_192:
			return "192kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::kHz_24:
			return "24kHz";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::UserDefined:
			return "UserDefinedFreq";
		case la::avdecc::entity::model::StreamFormatInfo::SamplingRate::Unknown:
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString streamFormatToString(la::avdecc::entity::model::StreamFormatInfo const& format)
{
	QString fmtStr;

	auto const type = format.getType();
	switch (type)
	{
		case la::avdecc::entity::model::StreamFormatInfo::Type::None:
			fmtStr += "No format";
			break;
		case la::avdecc::entity::model::StreamFormatInfo::Type::IEC_61883_6:
		case la::avdecc::entity::model::StreamFormatInfo::Type::AAF:
		{
			if (type == la::avdecc::entity::model::StreamFormatInfo::Type::IEC_61883_6)
				fmtStr += "IEC 61883-6";
			else
				fmtStr += "AAF";
			fmtStr += ", " + samplingRateToString(format.getSamplingRate());
			fmtStr += ", ";
			switch (format.getSampleFormat())
			{
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Int8:
					fmtStr += "PCM-INT-8";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Int16:
					fmtStr += "PCM-INT-16";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Int24:
					fmtStr += "PCM-INT-24";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Int32:
					fmtStr += "PCM-INT-32";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Int64:
					fmtStr += "PCM-INT-64";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::FixedPoint32:
					fmtStr += "PCM-FIXED-32";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::FloatingPoint32:
					fmtStr += "PCM-FLOAT-32";
					break;
				case la::avdecc::entity::model::StreamFormatInfo::SampleFormat::Unknown:
					fmtStr += "UnknownSize";
					break;
			}
			fmtStr += ", " + QString(format.isUpToChannelsCount() ? "up to " : "") + QString::number(format.getChannelsCount()) + " channels";
			fmtStr += QString(format.useSynchronousClock() ? "" : " (Async)");
			break;
		}
		case la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference:
		{
			auto const& crfFormat = static_cast<la::avdecc::entity::model::StreamFormatInfoCRF const&>(format);
			fmtStr += "CRF";
			switch (crfFormat.getCRFType())
			{
				case la::avdecc::entity::model::StreamFormatInfoCRF::CRFType::User:
					fmtStr += " User";
					break;
				case la::avdecc::entity::model::StreamFormatInfoCRF::CRFType::AudioSample:
					fmtStr += " AudioSample";
					break;
				case la::avdecc::entity::model::StreamFormatInfoCRF::CRFType::MachineCycle:
					fmtStr += " MachineCycle";
					break;
				case la::avdecc::entity::model::StreamFormatInfoCRF::CRFType::Unknown:
					fmtStr += " UnknownType";
					break;
			}
			fmtStr += ", " + samplingRateToString(format.getSamplingRate());
			fmtStr += ", " + QString::number(crfFormat.getTimestampInterval()) + " intvl";
			fmtStr += ", " + QString::number(crfFormat.getTimestampsPerPdu()) + " ts/pdu";
			break;
		}
		default:
			fmtStr += "Unknown format type";
	}

	return fmtStr;
}

QString clockSourceToString(la::avdecc::controller::model::ClockSourceNode const& node)
{
	auto const* const descriptor = node.staticModel;

	return clockSourceTypeToString(descriptor->clockSourceType) + ", " + descriptorTypeToString(descriptor->clockSourceLocationType) + ":" + QString::number(descriptor->clockSourceLocationIndex);
}

inline void concatenateFlags(QString& flags, QString const& flag)
{
	if (!flags.isEmpty())
		flags += " | ";
	flags += flag;
}

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags)
{
	QString str;

	if (flags == la::avdecc::entity::AvbInterfaceFlags::None)
		return "None";

	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::GptpGrandmasterSupported))
		concatenateFlags(str, "GptpGrandmasterSupported");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::SrpSupported))
		concatenateFlags(str, "SrpSupported");

	return str;
}

QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags)
{
	QString str;

	if (flags == la::avdecc::entity::AvbInfoFlags::None)
		return "None";

	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::AsCapable))
		concatenateFlags(str, "AS Capable");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::GptpEnabled))
		concatenateFlags(str, "Gptp Enabled");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::SrpEnabled))
		concatenateFlags(str, "Srp Enabled");

	return str;
}

QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags)
{
	QString str;

	if (flags == la::avdecc::entity::ClockSourceFlags::None)
		return "None";

	if (la::avdecc::hasFlag(flags, la::avdecc::entity::ClockSourceFlags::StreamID))
		concatenateFlags(str, "Stream");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::ClockSourceFlags::LocalID))
		concatenateFlags(str, "Local");

	return str;
}

QString flagsToString(la::avdecc::entity::PortFlags const flags)
{
	QString str;

	if (flags == la::avdecc::entity::PortFlags::None)
		return "None";

	if (la::avdecc::hasFlag(flags, la::avdecc::entity::PortFlags::ClockSyncSource))
		concatenateFlags(str, "ClockSyncSource");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::PortFlags::AsyncSampleRateConv))
		concatenateFlags(str, "AsyncSampleRateConv");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::PortFlags::SyncSampleRateConv))
		concatenateFlags(str, "SyncSampleRateConv");

	return str;
}

QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags)
{
	QString str;

	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::ClassB))
		concatenateFlags(str, "ClassB");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::FastConnect))
		concatenateFlags(str, "FastConnect");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::SavedState))
		concatenateFlags(str, "SavedState");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::StreamingWait))
		concatenateFlags(str, "StreamingWait");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::SupportsEncrypted))
		concatenateFlags(str, "SupportsEncrypted");
	if (la::avdecc::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::EncryptedPdu))
		concatenateFlags(str, "EncryptedPdu");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::EntityCapabilities const caps)
{
	QString str;
	
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::EfuMode))
		concatenateFlags(str, "EfuMode");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AddressAccessSupported))
		concatenateFlags(str, "AddressAccessSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::GatewayEntity))
		concatenateFlags(str, "GatewayEntity");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemSupported))
		concatenateFlags(str, "AemSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::LegacyAvc))
		concatenateFlags(str, "LegacyAvc");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AssociationIDSupported))
		concatenateFlags(str, "AssociationIDSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::VendorUniqueSupported))
		concatenateFlags(str, "VendorUniqueSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::ClassASupported))
		concatenateFlags(str, "ClassASupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::ClassBSupported))
		concatenateFlags(str, "ClassBSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemAuthenticationSupported))
		concatenateFlags(str, "AemAuthenticationSupported");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemAuthenticationRequired))
		concatenateFlags(str, "AemAuthenticationRequired");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemPersistentAcquireSupported))
		concatenateFlags(str, "AemPersistentAcquireSupported");
	
	if (str.isEmpty())
		str = "None";
	return str;
}
	
QString capabilitiesToString(la::avdecc::entity::TalkerCapabilities const caps)
{
	QString str;
	
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::OtherSource))
		concatenateFlags(str, "OtherSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::ControlSource))
		concatenateFlags(str, "ControlSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::MediaClockSource))
		concatenateFlags(str, "MediaClockSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::SmpteSource))
		concatenateFlags(str, "SmpteSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::MidiSource))
		concatenateFlags(str, "MidiSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::AudioSource))
		concatenateFlags(str, "AudioSource");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::VideoSource))
		concatenateFlags(str, "VideoSource");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ListenerCapabilities const caps)
{
	QString str;
	
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::OtherSink))
		concatenateFlags(str, "OtherSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::ControlSink))
		concatenateFlags(str, "ControlSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::MediaClockSink))
		concatenateFlags(str, "MediaClockSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::SmpteSink))
		concatenateFlags(str, "SmpteSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::MidiSink))
		concatenateFlags(str, "MidiSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::AudioSink))
		concatenateFlags(str, "AudioSink");
	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::VideoSink))
		concatenateFlags(str, "VideoSink");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ControllerCapabilities const caps)
{
	QString str;

	if (la::avdecc::hasFlag(caps, la::avdecc::entity::ControllerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString clockSourceTypeToString(la::avdecc::entity::model::ClockSourceType const type)
{
	switch (type)
	{
		case la::avdecc::entity::model::ClockSourceType::Internal:
			return "Internal";
		case la::avdecc::entity::model::ClockSourceType::External:
			return "External";
		case la::avdecc::entity::model::ClockSourceType::InputStream:
			return "Input Stream";
		case la::avdecc::entity::model::ClockSourceType::Expansion:
			return "Expansion";
		default:
			AVDECC_ASSERT(false, "Not handled!");
	}
	return "Unknown";
}

QString audioClusterFormatToString(la::avdecc::entity::model::AudioClusterFormat const format)
{
	switch (format)
	{
		case la::avdecc::entity::model::AudioClusterFormat::Iec60958:
			return "IEC 60958";
		case la::avdecc::entity::model::AudioClusterFormat::Mbla:
			return "MBLA";
		case la::avdecc::entity::model::AudioClusterFormat::Midi:
			return "MIDI";
		case la::avdecc::entity::model::AudioClusterFormat::Smpte:
			return "SMPTE";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

QString memoryObjectTypeToString(la::avdecc::entity::model::MemoryObjectType const type)
{
	switch (type)
	{
		case la::avdecc::entity::model::MemoryObjectType::FirmwareImage:
			return "Firmware Image";
		case la::avdecc::entity::model::MemoryObjectType::VendorSpecific:
			return "Vendor Specific";
		case la::avdecc::entity::model::MemoryObjectType::CrashDump:
			return "Crash Dump";
		case la::avdecc::entity::model::MemoryObjectType::LogObject:
			return "Log Object";
		case la::avdecc::entity::model::MemoryObjectType::AutostartSettings:
			return "Autostart Settings";
		case la::avdecc::entity::model::MemoryObjectType::SnapshotSettings:
			return "Snapshot Settings";
		case la::avdecc::entity::model::MemoryObjectType::SvgManufacturer:
			return "Svg Manufacturer";
		case la::avdecc::entity::model::MemoryObjectType::SvgEntity:
			return "Svg Entity";
		case la::avdecc::entity::model::MemoryObjectType::SvgGeneric:
			return "Svg Generic";
		case la::avdecc::entity::model::MemoryObjectType::PngManufacturer:
			return "Png Manufacturer";
		case la::avdecc::entity::model::MemoryObjectType::PngEntity:
			return "PngEntity";
		case la::avdecc::entity::model::MemoryObjectType::PngGeneric:
			return "Png Generic";
		case la::avdecc::entity::model::MemoryObjectType::DaeManufacturer:
			return "Dae Manufacturer";
		case la::avdecc::entity::model::MemoryObjectType::DaeEntity:
			return "Dae Entity";
		case la::avdecc::entity::model::MemoryObjectType::DaeGeneric:
			return "Dae Generic";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

QString loggerLayerToString(la::avdecc::logger::Layer const layer)
{
	switch (layer)
	{
		case la::avdecc::logger::Layer::Generic:
			return "Generic";
		case la::avdecc::logger::Layer::Serialization:
			return "Serialization";
		case la::avdecc::logger::Layer::ProtocolInterface:
			return "ProtocolInterface";
		case la::avdecc::logger::Layer::AemPayload:
			return "AemPayload";
		case la::avdecc::logger::Layer::ControllerEntity:
			return "ControllerEntity";
		case la::avdecc::logger::Layer::ControllerStateMachine:
			return "ControllerStateMachine";
		case la::avdecc::logger::Layer::Controller:
			return "Controller";
		case la::avdecc::logger::Layer::FirstUserLayer:
			return "Hive";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

QString loggerLevelToString(la::avdecc::logger::Level const& level)
{
	switch (level)
	{
		case la::avdecc::logger::Level::Trace:
			return "Trace";
		case la::avdecc::logger::Level::Debug:
			return "Debug";
		case la::avdecc::logger::Level::Info:
			return "Info";
		case la::avdecc::logger::Level::Warn:
			return "Warning";
		case la::avdecc::logger::Level::Error:
			return "Error";
		case la::avdecc::logger::Level::None:
			return "None";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

} // namespace helper
} // namespace avdecc
