/*
* Copyright (C) 2017-2019, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "helper.hpp"
#include "controllerManager.hpp"
#include <la/avdecc/utils.hpp>
#include <cctype>

namespace avdecc
{
namespace helper
{
QString protocolInterfaceTypeName(la::avdecc::protocol::ProtocolInterface::Type const& protocolInterfaceType)
{
	switch (protocolInterfaceType)
	{
		case la::avdecc::protocol::ProtocolInterface::Type::PCap:
			return "PCap";
		case la::avdecc::protocol::ProtocolInterface::Type::MacOSNative:
			return "MacOS Native";
		case la::avdecc::protocol::ProtocolInterface::Type::Proxy:
			return "Proxy";
		case la::avdecc::protocol::ProtocolInterface::Type::Virtual:
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

QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept
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

		if (la::avdecc::utils::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
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

		if (la::avdecc::utils::hasFlag(entity.getEntityCapabilities(), la::avdecc::entity::EntityCapabilities::AemSupported))
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

QString descriptorTypeToString(la::avdecc::entity::model::DescriptorType const& descriptorType) noexcept
{
	return QString::fromStdString(la::avdecc::entity::model::descriptorTypeToString(descriptorType));
}

QString acquireStateToString(la::avdecc::controller::model::AcquireState const& acquireState, la::avdecc::UniqueIdentifier const& owningController) noexcept
{
	switch (acquireState)
	{
		case la::avdecc::controller::model::AcquireState::Undefined:
			return "Undefined";
		case la::avdecc::controller::model::AcquireState::NotSupported:
			return "Not Supported";
		case la::avdecc::controller::model::AcquireState::NotAcquired:
			return "Not Acquired";
		case la::avdecc::controller::model::AcquireState::TryAcquire:
			return "Try Acquire";
		case la::avdecc::controller::model::AcquireState::Acquired:
			return "Acquired";
		case la::avdecc::controller::model::AcquireState::AcquiredByOther:
		{
			auto text = QString{ "Acquired by " };
			auto& controllerManager = avdecc::ControllerManager::getInstance();
			auto const& controllerEntity = controllerManager.getControlledEntity(owningController);
			if (controllerEntity)
			{
				text += smartEntityName(*controllerEntity);
			}
			else
			{
				text += uniqueIdentifierToString(owningController);
			}
			return text;
		}
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString lockStateToString(la::avdecc::controller::model::LockState const& lockState, la::avdecc::UniqueIdentifier const& lockingController) noexcept
{
	switch (lockState)
	{
		case la::avdecc::controller::model::LockState::Undefined:
			return "Undefined";
		case la::avdecc::controller::model::LockState::NotSupported:
			return "Not Supported";
		case la::avdecc::controller::model::LockState::NotLocked:
			return "Not Locked";
		case la::avdecc::controller::model::LockState::TryLock:
			return "Try Lock";
		case la::avdecc::controller::model::LockState::Locked:
			return "Locked";
		case la::avdecc::controller::model::LockState::LockedByOther:
		{
			auto text = QString{ "Locked by " };
			auto& controllerManager = avdecc::ControllerManager::getInstance();
			auto const& controllerEntity = controllerManager.getControlledEntity(lockingController);
			if (controllerEntity)
			{
				text += smartEntityName(*controllerEntity);
			}
			else
			{
				text += uniqueIdentifierToString(lockingController);
			}
			return text;
		}
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString samplingRateToString(la::avdecc::entity::model::StreamFormatInfo::SamplingRate const& samplingRate) noexcept
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

QString streamFormatToString(la::avdecc::entity::model::StreamFormatInfo const& format) noexcept
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

QString clockSourceToString(la::avdecc::controller::model::ClockSourceNode const& node) noexcept
{
	auto const* const descriptor = node.staticModel;

	return avdecc::helper::clockSourceTypeToString(descriptor->clockSourceType) + ", " + avdecc::helper::descriptorTypeToString(descriptor->clockSourceLocationType) + ":" + QString::number(descriptor->clockSourceLocationIndex);
}

inline void concatenateFlags(QString& flags, QString const& flag) noexcept
{
	if (!flags.isEmpty())
		flags += " | ";
	flags += flag;
}

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags) noexcept
{
	QString str;

	if (flags == la::avdecc::entity::AvbInterfaceFlags::None)
		return "None";

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::GptpGrandmasterSupported))
		concatenateFlags(str, "GptpGrandmasterSupported");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInterfaceFlags::SrpSupported))
		concatenateFlags(str, "SrpSupported");

	return str;
}

QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags) noexcept
{
	QString str;

	if (flags == la::avdecc::entity::AvbInfoFlags::None)
		return "None";

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::AsCapable))
		concatenateFlags(str, "AS Capable");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::GptpEnabled))
		concatenateFlags(str, "Gptp Enabled");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::AvbInfoFlags::SrpEnabled))
		concatenateFlags(str, "Srp Enabled");

	return str;
}

QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags) noexcept
{
	QString str;

	if (flags == la::avdecc::entity::ClockSourceFlags::None)
		return "None";

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::ClockSourceFlags::StreamID))
		concatenateFlags(str, "Stream");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::ClockSourceFlags::LocalID))
		concatenateFlags(str, "Local");

	return str;
}

QString flagsToString(la::avdecc::entity::PortFlags const flags) noexcept
{
	QString str;

	if (flags == la::avdecc::entity::PortFlags::None)
		return "None";

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::PortFlags::ClockSyncSource))
		concatenateFlags(str, "ClockSyncSource");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::PortFlags::AsyncSampleRateConv))
		concatenateFlags(str, "AsyncSampleRateConv");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::PortFlags::SyncSampleRateConv))
		concatenateFlags(str, "SyncSampleRateConv");

	return str;
}

QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::ClassB))
		concatenateFlags(str, "ClassB");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::FastConnect))
		concatenateFlags(str, "FastConnect");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::SavedState))
		concatenateFlags(str, "SavedState");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::StreamingWait))
		concatenateFlags(str, "StreamingWait");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::SupportsEncrypted))
		concatenateFlags(str, "SupportsEncrypted");
	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlags::EncryptedPdu))
		concatenateFlags(str, "EncryptedPdu");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString flagsToString(la::avdecc::entity::StreamInfoFlagsEx const flags) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(flags, la::avdecc::entity::StreamInfoFlagsEx::Registering))
		concatenateFlags(str, "Registering");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString flagsToString(la::avdecc::protocol::MvuFeaturesFlags const flags) noexcept
{
	QString str;

	if ((flags & la::avdecc::protocol::MvuFeaturesFlags::Redundancy) == la::avdecc::protocol::MvuFeaturesFlags::Redundancy)
		concatenateFlags(str, "Redundancy");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString probingStatusToString(la::avdecc::entity::model::ProbingStatus const status) noexcept
{
	switch (status)
	{
		case la::avdecc::entity::model::ProbingStatus::Disabled:
			return "Disabled";
		case la::avdecc::entity::model::ProbingStatus::Passive:
			return "Passive";
		case la::avdecc::entity::model::ProbingStatus::Active:
			return "Active";
		case la::avdecc::entity::model::ProbingStatus::Completed:
			return "Completed";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

QString capabilitiesToString(la::avdecc::entity::EntityCapabilities const caps) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::EfuMode))
		concatenateFlags(str, "EfuMode");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AddressAccessSupported))
		concatenateFlags(str, "AddressAccessSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::GatewayEntity))
		concatenateFlags(str, "GatewayEntity");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemSupported))
		concatenateFlags(str, "AemSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::LegacyAvc))
		concatenateFlags(str, "LegacyAvc");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AssociationIDSupported))
		concatenateFlags(str, "AssociationIDSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::VendorUniqueSupported))
		concatenateFlags(str, "VendorUniqueSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::ClassASupported))
		concatenateFlags(str, "ClassASupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::ClassBSupported))
		concatenateFlags(str, "ClassBSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemAuthenticationSupported))
		concatenateFlags(str, "AemAuthenticationSupported");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemAuthenticationRequired))
		concatenateFlags(str, "AemAuthenticationRequired");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::EntityCapabilities::AemPersistentAcquireSupported))
		concatenateFlags(str, "AemPersistentAcquireSupported");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::TalkerCapabilities const caps) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::OtherSource))
		concatenateFlags(str, "OtherSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::ControlSource))
		concatenateFlags(str, "ControlSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::MediaClockSource))
		concatenateFlags(str, "MediaClockSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::SmpteSource))
		concatenateFlags(str, "SmpteSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::MidiSource))
		concatenateFlags(str, "MidiSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::AudioSource))
		concatenateFlags(str, "AudioSource");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::TalkerCapabilities::VideoSource))
		concatenateFlags(str, "VideoSource");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ListenerCapabilities const caps) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::OtherSink))
		concatenateFlags(str, "OtherSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::ControlSink))
		concatenateFlags(str, "ControlSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::MediaClockSink))
		concatenateFlags(str, "MediaClockSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::SmpteSink))
		concatenateFlags(str, "SmpteSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::MidiSink))
		concatenateFlags(str, "MidiSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::AudioSink))
		concatenateFlags(str, "AudioSink");
	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ListenerCapabilities::VideoSink))
		concatenateFlags(str, "VideoSink");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ControllerCapabilities const caps) noexcept
{
	QString str;

	if (la::avdecc::utils::hasFlag(caps, la::avdecc::entity::ControllerCapabilities::Implemented))
		concatenateFlags(str, "Implemented");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString clockSourceTypeToString(la::avdecc::entity::model::ClockSourceType const type) noexcept
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

QString audioClusterFormatToString(la::avdecc::entity::model::AudioClusterFormat const format) noexcept
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

QString memoryObjectTypeToString(la::avdecc::entity::model::MemoryObjectType const type) noexcept
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
			return "Png Entity";
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

QString certificationVersionToString(std::uint32_t const certificationVersion) noexcept
{
	return QString("%1.%2.%3.%4").arg(certificationVersion >> 24 & 0xFF).arg(certificationVersion >> 16 & 0xFF).arg(certificationVersion >> 8 & 0xFF).arg(certificationVersion & 0xFF);
}

QString loggerLayerToString(la::avdecc::logger::Layer const layer) noexcept
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

QString loggerLevelToString(la::avdecc::logger::Level const& level) noexcept
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

QString toUpperCamelCase(std::string const& text) noexcept
{
#pragma message("TODO: Use a regex, if possible")
	auto output = std::string{};

	auto shouldUpperCase = true;
	for (auto const c : text)
	{
		if (c == '_')
		{
			output.push_back(' ');
			shouldUpperCase = true;
		}
		else if (shouldUpperCase)
		{
			output.push_back(std::toupper(c));
			shouldUpperCase = false;
		}
		else
		{
			output.push_back(std::tolower(c));
		}
	}

	return QString::fromStdString(output);
}

QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	static auto s_oui24ToName = std::unordered_map<std::uint32_t, QString>{};
	static auto s_oui36ToName = std::unordered_map<std::uint64_t, QString>{};

	// Map is empty, load it
	if (s_oui24ToName.empty())
	{
		// Right now statically build the map, later use a resource file generated from https://standards.ieee.org/develop/regauth/oui/oui.csv
		s_oui24ToName.emplace(std::make_pair(0x001B92, "l-acoustics"));
		s_oui24ToName.emplace(std::make_pair(0x001CF7, "AudioScience"));
		s_oui24ToName.emplace(std::make_pair(0xB4994C, "Texas Instruments"));
		s_oui24ToName.emplace(std::make_pair(0x3CC0C6, "d&b audiotechnik GmbH"));
		s_oui24ToName.emplace(std::make_pair(0x001CAB, "Meyer Sound Laboratories, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x0C4DE9, "Apple"));
		s_oui24ToName.emplace(std::make_pair(0x0090E5, "TEKNEMA, INC."));
		s_oui24ToName.emplace(std::make_pair(0x0001F2, "Mark of the Unicorn, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xD0699E, "LUMINEX Lighting Control Equipment"));
		s_oui24ToName.emplace(std::make_pair(0xCC46D6, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x58AC78, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x00107B, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x00906D, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x0090BF, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x005080, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0xF4CFE2, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x501CBF, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x88F031, "Cisco Systems, Inc"));
		s_oui24ToName.emplace(std::make_pair(0x508789, "Cisco Systems, Inc")); // Too many cisco OUI-24 to manually add them
		s_oui24ToName.emplace(std::make_pair(0x00A07E, "AVID TECHNOLOGY, INC."));
		s_oui24ToName.emplace(std::make_pair(0xD88466, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x000496, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xB85001, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x5C0E8B, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xB4C799, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x7467F7, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x00E02B, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x949B2C, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xA4EA8E, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xFC0A81, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xB42D56, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x000130, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0xF46E95, "Extreme Networks, Inc."));
		s_oui24ToName.emplace(std::make_pair(0x784501, "Biamp Systems"));
	}

	// First search in OUI-24
	{
		auto const nameIt = s_oui24ToName.find(entityID.getVendorID<std::uint32_t>());
		if (nameIt != s_oui24ToName.end())
		{
			return nameIt->second;
		}
	}

	// Then search in OUI-36
	{
		auto const nameIt = s_oui36ToName.find(entityID.getVendorID<std::uint64_t>());
		if (nameIt != s_oui36ToName.end())
		{
			return nameIt->second;
		}
	}

	// If not found, convert to hex string
	return toHexQString(entityID.getVendorID<std::uint32_t>(), true, true);
}

bool isStreamConnected(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::Connected) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

bool isStreamFastConnecting(la::avdecc::UniqueIdentifier const talkerID, la::avdecc::controller::model::StreamOutputNode const* const talkerNode, la::avdecc::controller::model::StreamInputNode const* const listenerNode) noexcept
{
	return (listenerNode->dynamicModel->connectionState.state == la::avdecc::controller::model::StreamConnectionState::State::FastConnecting) && (listenerNode->dynamicModel->connectionState.talkerStream.entityID == talkerID) && (listenerNode->dynamicModel->connectionState.talkerStream.streamIndex == talkerNode->descriptorIndex);
}

} // namespace helper
} // namespace avdecc
