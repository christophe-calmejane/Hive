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

#include "helper.hpp"

#include <hive/modelsLibrary/helper.hpp>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <la/avdecc/utils.hpp>

#include <QMessageBox>

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
		case la::avdecc::controller::model::AcquireState::AcquireInProgress:
			return "Acquire In Progress";
		case la::avdecc::controller::model::AcquireState::Acquired:
			return "Acquired";
		case la::avdecc::controller::model::AcquireState::AcquiredByOther:
		{
			auto text = QString{ "Acquired by " };
			auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const& controllerEntity = controllerManager.getControlledEntity(owningController);
			if (controllerEntity)
			{
				text += hive::modelsLibrary::helper::smartEntityName(*controllerEntity);
			}
			else
			{
				text += hive::modelsLibrary::helper::uniqueIdentifierToString(owningController);
			}
			return text;
		}
		case la::avdecc::controller::model::AcquireState::ReleaseInProgress:
			return "Release In Progress";
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
		case la::avdecc::controller::model::LockState::LockInProgress:
			return "Lock In Progress";
		case la::avdecc::controller::model::LockState::Locked:
			return "Locked";
		case la::avdecc::controller::model::LockState::LockedByOther:
		{
			auto text = QString{ "Locked by " };
			auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
			auto const& controllerEntity = controllerManager.getControlledEntity(lockingController);
			if (controllerEntity)
			{
				text += hive::modelsLibrary::helper::smartEntityName(*controllerEntity);
			}
			else
			{
				text += hive::modelsLibrary::helper::uniqueIdentifierToString(lockingController);
			}
			return text;
		}
		case la::avdecc::controller::model::LockState::UnlockInProgress:
			return "Unlock In Progress";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return {};
	}
}

QString samplingRateToString(la::avdecc::entity::model::SamplingRate const& samplingRate) noexcept
{
	auto const freq = static_cast<std::uint32_t>(samplingRate.getNominalSampleRate());
	if (freq != 0)
	{
		if (freq < 1000)
		{
			return QString("%1Hz").arg(freq);
		}
		else
		{
			// Round to nearest integer but keep one decimal part
			auto const freqRounded = freq / 1000;
			auto const freqDecimal = (freq % 1000) / 100;
			if (freqDecimal == 0)
			{
				return QString("%1kHz").arg(freqRounded);
			}
			else
			{
				return QString("%1.%2kHz").arg(freqRounded).arg(freqDecimal);
			}
		}
	}
	return "Unknown";
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

QString flagsToString(la::avdecc::entity::JackFlags const flags) noexcept
{
	QString str;

	if (flags.empty())
		return "None";

	if (flags.test(la::avdecc::entity::JackFlag::ClockSyncSource))
		concatenateFlags(str, "ClockSyncSource");
	if (flags.test(la::avdecc::entity::JackFlag::Captive))
		concatenateFlags(str, "Captive");

	return str;
}

QString flagsToString(la::avdecc::entity::AvbInterfaceFlags const flags) noexcept
{
	QString str;

	if (flags.empty())
		return "None";

	if (flags.test(la::avdecc::entity::AvbInterfaceFlag::GptpGrandmasterSupported))
		concatenateFlags(str, "GptpGrandmasterSupported");
	if (flags.test(la::avdecc::entity::AvbInterfaceFlag::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (flags.test(la::avdecc::entity::AvbInterfaceFlag::SrpSupported))
		concatenateFlags(str, "SrpSupported");

	return str;
}

QString flagsToString(la::avdecc::entity::AvbInfoFlags const flags) noexcept
{
	QString str;

	if (flags.empty())
		return "None";

	if (flags.test(la::avdecc::entity::AvbInfoFlag::AsCapable))
		concatenateFlags(str, "AS Capable");
	if (flags.test(la::avdecc::entity::AvbInfoFlag::GptpEnabled))
		concatenateFlags(str, "Gptp Enabled");
	if (flags.test(la::avdecc::entity::AvbInfoFlag::SrpEnabled))
		concatenateFlags(str, "Srp Enabled");

	return str;
}

QString flagsToString(la::avdecc::entity::ClockSourceFlags const flags) noexcept
{
	QString str;

	if (flags.empty())
		return "None";

	if (flags.test(la::avdecc::entity::ClockSourceFlag::StreamID))
		concatenateFlags(str, "Stream");
	if (flags.test(la::avdecc::entity::ClockSourceFlag::LocalID))
		concatenateFlags(str, "Local");

	return str;
}

QString flagsToString(la::avdecc::entity::PortFlags const flags) noexcept
{
	QString str;

	if (flags.empty())
		return "None";

	if (flags.test(la::avdecc::entity::PortFlag::ClockSyncSource))
		concatenateFlags(str, "ClockSyncSource");
	if (flags.test(la::avdecc::entity::PortFlag::AsyncSampleRateConv))
		concatenateFlags(str, "AsyncSampleRateConv");
	if (flags.test(la::avdecc::entity::PortFlag::SyncSampleRateConv))
		concatenateFlags(str, "SyncSampleRateConv");

	return str;
}

QString flagsToString(la::avdecc::entity::StreamInfoFlags const flags) noexcept
{
	QString str;

	if (flags.test(la::avdecc::entity::StreamInfoFlag::ClassB))
		concatenateFlags(str, "ClassB");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::FastConnect))
		concatenateFlags(str, "FastConnect");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::SavedState))
		concatenateFlags(str, "SavedState");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::StreamingWait))
		concatenateFlags(str, "StreamingWait");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::SupportsEncrypted))
		concatenateFlags(str, "SupportsEncrypted");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::EncryptedPdu))
		concatenateFlags(str, "EncryptedPdu");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::TalkerFailed))
		concatenateFlags(str, "TalkerFailed");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::StreamVlanIDValid))
		concatenateFlags(str, "StreamVlanIDValid");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::Connected))
		concatenateFlags(str, "Connected");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::MsrpFailureValid))
		concatenateFlags(str, "MsrpFailureValid");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::StreamDestMacValid))
		concatenateFlags(str, "StreamDestMacValid");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::MsrpAccLatValid))
		concatenateFlags(str, "MsrpAccLatValid");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::StreamIDValid))
		concatenateFlags(str, "StreamIDValid");
	if (flags.test(la::avdecc::entity::StreamInfoFlag::StreamFormatValid))
		concatenateFlags(str, "StreamFormatValid");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString flagsToString(la::avdecc::entity::StreamInfoFlagsEx const flags) noexcept
{
	QString str;

	if (flags.test(la::avdecc::entity::StreamInfoFlagEx::Registering))
		concatenateFlags(str, "Registering");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString flagsToString(la::avdecc::entity::MilanInfoFeaturesFlags const flags) noexcept
{
	QString str;

	if (flags.test(la::avdecc::entity::MilanInfoFeaturesFlag::Redundancy))
		concatenateFlags(str, "Redundancy");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString msrpFailureCodeToString(la::avdecc::entity::model::MsrpFailureCode const msrpFailureCode) noexcept
{
	switch (msrpFailureCode)
	{
		case la::avdecc::entity::model::MsrpFailureCode::NoFailure:
			return "No Failure";
		case la::avdecc::entity::model::MsrpFailureCode::InsufficientBandwidth:
			return "Insufficient Bandwidth";
		case la::avdecc::entity::model::MsrpFailureCode::InsufficientResources:
			return "Insufficient Resources";
		case la::avdecc::entity::model::MsrpFailureCode::InsufficientTrafficClassBandwidth:
			return "Insufficient Traffic Class Bandwidth";
		case la::avdecc::entity::model::MsrpFailureCode::StreamIDInUse:
			return "Stream ID In Use";
		case la::avdecc::entity::model::MsrpFailureCode::StreamDestinationAddressInUse:
			return "Stream Destination Address In Use";
		case la::avdecc::entity::model::MsrpFailureCode::StreamPreemptedByHigherRank:
			return "Stream Preempted By Higher Rank";
		case la::avdecc::entity::model::MsrpFailureCode::LatencyHasChanged:
			return "Latency Has Changed";
		case la::avdecc::entity::model::MsrpFailureCode::EgressPortNotAVBCapable:
			return "Egress Port Not AVB Capable";
		case la::avdecc::entity::model::MsrpFailureCode::UseDifferentDestinationAddress:
			return "Use Different Destination Address";
		case la::avdecc::entity::model::MsrpFailureCode::OutOfMSRPResources:
			return "Out Of MSRP Resources";
		case la::avdecc::entity::model::MsrpFailureCode::OutOfMMRPResources:
			return "Out Of MMRP Resources";
		case la::avdecc::entity::model::MsrpFailureCode::CannotStoreDestinationAddress:
			return "Cannot Store Destination Address";
		case la::avdecc::entity::model::MsrpFailureCode::PriorityIsNotAnSRClass:
			return "Priority Is Not An SR Class";
		case la::avdecc::entity::model::MsrpFailureCode::MaxFrameSizeTooLarge:
			return "Max Frame Size Too Large";
		case la::avdecc::entity::model::MsrpFailureCode::MaxFanInPortsLimitReached:
			return "Max Fan In Ports Limit Reached";
		case la::avdecc::entity::model::MsrpFailureCode::FirstValueChangedForStreamID:
			return "First Value Changed For Stream ID";
		case la::avdecc::entity::model::MsrpFailureCode::VlanBlockedOnEgress:
			return "Vlan Blocked On Egress";
		case la::avdecc::entity::model::MsrpFailureCode::VlanTaggingDisabledOnEgress:
			return "Vlan Tagging Disabled On Egress";
		case la::avdecc::entity::model::MsrpFailureCode::SrClassPriorityMismatch:
			return "SR Class Priority Mismatch";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
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

	if (caps.test(la::avdecc::entity::EntityCapability::EfuMode))
		concatenateFlags(str, "EfuMode");
	if (caps.test(la::avdecc::entity::EntityCapability::AddressAccessSupported))
		concatenateFlags(str, "AddressAccessSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::GatewayEntity))
		concatenateFlags(str, "GatewayEntity");
	if (caps.test(la::avdecc::entity::EntityCapability::AemSupported))
		concatenateFlags(str, "AemSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::LegacyAvc))
		concatenateFlags(str, "LegacyAvc");
	if (caps.test(la::avdecc::entity::EntityCapability::AssociationIDSupported))
		concatenateFlags(str, "AssociationIDSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::VendorUniqueSupported))
		concatenateFlags(str, "VendorUniqueSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::ClassASupported))
		concatenateFlags(str, "ClassASupported");
	if (caps.test(la::avdecc::entity::EntityCapability::ClassBSupported))
		concatenateFlags(str, "ClassBSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::GptpSupported))
		concatenateFlags(str, "GptpSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::AemAuthenticationSupported))
		concatenateFlags(str, "AemAuthenticationSupported");
	if (caps.test(la::avdecc::entity::EntityCapability::AemAuthenticationRequired))
		concatenateFlags(str, "AemAuthenticationRequired");
	if (caps.test(la::avdecc::entity::EntityCapability::AemPersistentAcquireSupported))
		concatenateFlags(str, "AemPersistentAcquireSupported");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::TalkerCapabilities const caps) noexcept
{
	QString str;

	if (caps.test(la::avdecc::entity::TalkerCapability::Implemented))
		concatenateFlags(str, "Implemented");
	if (caps.test(la::avdecc::entity::TalkerCapability::OtherSource))
		concatenateFlags(str, "OtherSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::ControlSource))
		concatenateFlags(str, "ControlSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::MediaClockSource))
		concatenateFlags(str, "MediaClockSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::SmpteSource))
		concatenateFlags(str, "SmpteSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::MidiSource))
		concatenateFlags(str, "MidiSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::AudioSource))
		concatenateFlags(str, "AudioSource");
	if (caps.test(la::avdecc::entity::TalkerCapability::VideoSource))
		concatenateFlags(str, "VideoSource");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ListenerCapabilities const caps) noexcept
{
	QString str;

	if (caps.test(la::avdecc::entity::ListenerCapability::Implemented))
		concatenateFlags(str, "Implemented");
	if (caps.test(la::avdecc::entity::ListenerCapability::OtherSink))
		concatenateFlags(str, "OtherSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::ControlSink))
		concatenateFlags(str, "ControlSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::MediaClockSink))
		concatenateFlags(str, "MediaClockSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::SmpteSink))
		concatenateFlags(str, "SmpteSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::MidiSink))
		concatenateFlags(str, "MidiSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::AudioSink))
		concatenateFlags(str, "AudioSink");
	if (caps.test(la::avdecc::entity::ListenerCapability::VideoSink))
		concatenateFlags(str, "VideoSink");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString capabilitiesToString(la::avdecc::entity::ControllerCapabilities const caps) noexcept
{
	QString str;

	if (caps.test(la::avdecc::entity::ControllerCapability::Implemented))
		concatenateFlags(str, "Implemented");

	if (str.isEmpty())
		str = "None";
	return str;
}

QString jackTypeToString(la::avdecc::entity::model::JackType const type) noexcept
{
	switch (type)
	{
		case la::avdecc::entity::model::JackType::Speaker:
			return "Speaker";
		case la::avdecc::entity::model::JackType::Headphone:
			return "Headphone";
		case la::avdecc::entity::model::JackType::AnalogMicrophone:
			return "Analog Microphone";
		case la::avdecc::entity::model::JackType::Spdif:
			return "S/PDIF";
		case la::avdecc::entity::model::JackType::Adat:
			return "ADAT";
		case la::avdecc::entity::model::JackType::Tdif:
			return "TDIF";
		case la::avdecc::entity::model::JackType::Madi:
			return "MADI";
		case la::avdecc::entity::model::JackType::UnbalancedAnalog:
			return "Generic Unbalanced Analog";
		case la::avdecc::entity::model::JackType::BalancedAnalog:
			return "Generic Balanced Analog";
		case la::avdecc::entity::model::JackType::Digital:
			return "Generic Digital";
		case la::avdecc::entity::model::JackType::Midi:
			return "MIDI";
		case la::avdecc::entity::model::JackType::AesEbu:
			return "AES/EBU";
		case la::avdecc::entity::model::JackType::CompositeVideo:
			return "Composite Video";
		case la::avdecc::entity::model::JackType::SVhsVideo:
			return "S-VHS Video";
		case la::avdecc::entity::model::JackType::ComponentVideo:
			return "Component Video";
		case la::avdecc::entity::model::JackType::Dvi:
			return "DVI";
		case la::avdecc::entity::model::JackType::Hdmi:
			return "HDMI";
		case la::avdecc::entity::model::JackType::Udi:
			return "UDI";
		case la::avdecc::entity::model::JackType::DisplayPort:
			return "DisplayPort";
		case la::avdecc::entity::model::JackType::Antenna:
			return "Antenna";
		case la::avdecc::entity::model::JackType::AnalogTuner:
			return "Analog Tuner";
		case la::avdecc::entity::model::JackType::Ethernet:
			return "Non-AVB Ethernet";
		case la::avdecc::entity::model::JackType::Wifi:
			return "Non-AVB Wi-Fi";
		case la::avdecc::entity::model::JackType::Usb:
			return "USB";
		case la::avdecc::entity::model::JackType::Pci:
			return "PCI";
		case la::avdecc::entity::model::JackType::PciE:
			return "PCI-Express";
		case la::avdecc::entity::model::JackType::Scsi:
			return "SCSI";
		case la::avdecc::entity::model::JackType::Ata:
			return "ATA";
		case la::avdecc::entity::model::JackType::Imager:
			return "Camera Imager";
		case la::avdecc::entity::model::JackType::Ir:
			return "Infra-Red";
		case la::avdecc::entity::model::JackType::Thunderbolt:
			return "Thunderbolt";
		case la::avdecc::entity::model::JackType::Sata:
			return "SATA";
		case la::avdecc::entity::model::JackType::SmpteLtc:
			return "SMPTE Linear Time Code";
		case la::avdecc::entity::model::JackType::DigitalMicrophone:
			return "Digital Microphone";
		case la::avdecc::entity::model::JackType::AudioMediaClock:
			return "Audio Media Clock";
		case la::avdecc::entity::model::JackType::VideoMediaClock:
			return "Video Media Clock";
		case la::avdecc::entity::model::JackType::GnssClock:
			return "Global Navigation Satellite System Clock";
		case la::avdecc::entity::model::JackType::Pps:
			return "Pulse Per Second";
		case la::avdecc::entity::model::JackType::Expansion:
			return "Expansion";
		default:
			AVDECC_ASSERT(false, "Not handled!");
	}
	return "Unknown";
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

QString controlTypeToString(la::avdecc::entity::model::ControlType const& controlType) noexcept
{
	auto const vendorID = controlType.getVendorID();
	if (vendorID == la::avdecc::entity::model::StandardControlTypeVendorID)
	{
		switch (static_cast<la::avdecc::entity::model::StandardControlType>(controlType.getValue()))
		{
			case la::avdecc::entity::model::StandardControlType::Enable:
				return "Enable";
			case la::avdecc::entity::model::StandardControlType::Identify:
				return "Identify";
			case la::avdecc::entity::model::StandardControlType::Mute:
				return "Mute";
			case la::avdecc::entity::model::StandardControlType::Invert:
				return "Invert";
			case la::avdecc::entity::model::StandardControlType::Gain:
				return "Gain";
			case la::avdecc::entity::model::StandardControlType::Attenuate:
				return "Attenuate";
			case la::avdecc::entity::model::StandardControlType::Delay:
				return "Delay";
			case la::avdecc::entity::model::StandardControlType::SrcMode:
				return "Sample Rate Converter Mode";
			case la::avdecc::entity::model::StandardControlType::Snapshot:
				return "Snapshot";
			case la::avdecc::entity::model::StandardControlType::PowLineFreq:
				return "Power Line Frequency";
			case la::avdecc::entity::model::StandardControlType::PowerStatus:
				return "Power Status";
			case la::avdecc::entity::model::StandardControlType::FanStatus:
				return "Fan Status";
			case la::avdecc::entity::model::StandardControlType::Temperature:
				return "Temperature";
			case la::avdecc::entity::model::StandardControlType::Altitude:
				return "Altitude";
			case la::avdecc::entity::model::StandardControlType::AbsoluteHumidity:
				return "Absolute Humidity";
			case la::avdecc::entity::model::StandardControlType::RelativeHumidity:
				return "Relative Humidity";
			case la::avdecc::entity::model::StandardControlType::Orientation:
				return "Orientation";
			case la::avdecc::entity::model::StandardControlType::Velocity:
				return "Velocity";
			case la::avdecc::entity::model::StandardControlType::Acceleration:
				return "Acceleration";
			case la::avdecc::entity::model::StandardControlType::FilterResponse:
				return "Filter Response";
			case la::avdecc::entity::model::StandardControlType::Panpot:
				return "Stereo Pan Position";
			case la::avdecc::entity::model::StandardControlType::Phantom:
				return "Phantom Power";
			case la::avdecc::entity::model::StandardControlType::AudioScale:
				return "Audio Scale";
			case la::avdecc::entity::model::StandardControlType::AudioMeters:
				return "Audio Meters";
			case la::avdecc::entity::model::StandardControlType::AudioSpectrum:
				return "Audio Spectrum";
			case la::avdecc::entity::model::StandardControlType::ScanningMode:
				return "Video Scanning Mode";
			case la::avdecc::entity::model::StandardControlType::AutoExpMode:
				return "Auto Exposure Mode";
			case la::avdecc::entity::model::StandardControlType::AutoExpPrio:
				return "Auto Exposure Priority";
			case la::avdecc::entity::model::StandardControlType::ExpTime:
				return "Exposure Time";
			case la::avdecc::entity::model::StandardControlType::Focus:
				return "Focus";
			case la::avdecc::entity::model::StandardControlType::FocusAuto:
				return "Focus Automatic";
			case la::avdecc::entity::model::StandardControlType::Iris:
				return "Iris";
			case la::avdecc::entity::model::StandardControlType::Zoom:
				return "Zoom";
			case la::avdecc::entity::model::StandardControlType::Privacy:
				return "Privacy";
			case la::avdecc::entity::model::StandardControlType::Backlight:
				return "Backlight Compensation";
			case la::avdecc::entity::model::StandardControlType::Brightness:
				return "Brightness";
			case la::avdecc::entity::model::StandardControlType::Contrast:
				return "Contrast";
			case la::avdecc::entity::model::StandardControlType::Hue:
				return "Hue";
			case la::avdecc::entity::model::StandardControlType::Saturation:
				return "Saturation";
			case la::avdecc::entity::model::StandardControlType::Sharpness:
				return "Sharpness";
			case la::avdecc::entity::model::StandardControlType::Gamma:
				return "Gamma";
			case la::avdecc::entity::model::StandardControlType::WhiteBalTemp:
				return "White Balance Temperature";
			case la::avdecc::entity::model::StandardControlType::WhiteBalTempAuto:
				return "White Balance Temperature Auto";
			case la::avdecc::entity::model::StandardControlType::WhiteBalComp:
				return "White Balance Components";
			case la::avdecc::entity::model::StandardControlType::WhiteBalCompAuto:
				return "White Balance Components Auto";
			case la::avdecc::entity::model::StandardControlType::DigitalZoom:
				return "Digital Zoom";
			case la::avdecc::entity::model::StandardControlType::MediaPlaylist:
				return "Media Playlist";
			case la::avdecc::entity::model::StandardControlType::MediaPlaylistName:
				return "Media Playlist Name";
			case la::avdecc::entity::model::StandardControlType::MediaDisk:
				return "Media Disk";
			case la::avdecc::entity::model::StandardControlType::MediaDiskName:
				return "Media Disk Name";
			case la::avdecc::entity::model::StandardControlType::MediaTrack:
				return "Media Track";
			case la::avdecc::entity::model::StandardControlType::MediaTrackName:
				return "Media Track Name";
			case la::avdecc::entity::model::StandardControlType::MediaSpeed:
				return "Media Speed";
			case la::avdecc::entity::model::StandardControlType::MediaSamplePosition:
				return "Media Sample Position";
			case la::avdecc::entity::model::StandardControlType::MediaPlaybackTransport:
				return "Media Playback Transport";
			case la::avdecc::entity::model::StandardControlType::MediaRecordTransport:
				return "Media Record Transport";
			case la::avdecc::entity::model::StandardControlType::Frequency:
				return "Frequency";
			case la::avdecc::entity::model::StandardControlType::Modulation:
				return "Modulation";
			case la::avdecc::entity::model::StandardControlType::Polarization:
				return "Polarization";
			default:
				AVDECC_ASSERT(false, "Not handled!");
				return "Unknown Standard Control Type";
		}
	}
	else
	{
		return "Vendor: " + QString::fromStdString(la::avdecc::utils::toHexString<std::uint32_t, 6>(vendorID, true, true)) + " Value: " + QString::fromStdString(la::avdecc::utils::toHexString<std::uint64_t, 10>(controlType.getVendorValue(), true, true));
	}
}

QString controlValueTypeToString(la::avdecc::entity::model::ControlValueType::Type const controlValueType) noexcept
{
	switch (controlValueType)
	{
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt8:
			return "Linear Int 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt8:
			return "Linear UInt 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt16:
			return "Linear Int 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt16:
			return "Linear UInt 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt32:
			return "Linear Int 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt32:
			return "Linear UTnt 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearInt64:
			return "Linear Int 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearUInt64:
			return "Linear UInt 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearFloat:
			return "Linear Float";
		case la::avdecc::entity::model::ControlValueType::Type::ControlLinearDouble:
			return "Linear Double";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorInt8:
			return "Selector Int 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorUInt8:
			return "Selector UInt 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorInt16:
			return "Selector Int 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorUInt16:
			return "Selector UInt 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorInt32:
			return "Selector Int 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorUInt32:
			return "Selector UInt 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorInt64:
			return "Selector Int 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorUInt64:
			return "Selector UInt 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorFloat:
			return "Selector Float";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorDouble:
			return "Selector Double";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSelectorString:
			return "Selector String";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt8:
			return "Array Int 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt8:
			return "Array UInt 8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt16:
			return "Array Int 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt16:
			return "Array UInt 16";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt32:
			return "Array Int 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt32:
			return "Array UInt 32";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayInt64:
			return "Array Int 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayUInt64:
			return "Array UInt 64";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayFloat:
			return "Array Float";
		case la::avdecc::entity::model::ControlValueType::Type::ControlArrayDouble:
			return "Array Double";
		case la::avdecc::entity::model::ControlValueType::Type::ControlUtf8:
			return "UTF8";
		case la::avdecc::entity::model::ControlValueType::Type::ControlBodePlot:
			return "Bode Plot";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSmpteTime:
			return "SMPTE Time";
		case la::avdecc::entity::model::ControlValueType::Type::ControlSampleRate:
			return "Sample Rate";
		case la::avdecc::entity::model::ControlValueType::Type::ControlGptpTime:
			return "gPTP Time";
		case la::avdecc::entity::model::ControlValueType::Type::ControlVendor:
			return "Vendor";
		case la::avdecc::entity::model::ControlValueType::Type::Expansion:
			return "Expansion";
		default:
			AVDECC_ASSERT(false, "Not handled!");
			return "Unknown";
	}
}

QString controlValueUnitToString(la::avdecc::entity::model::ControlValueUnit::Unit const controlValueUnit) noexcept
{
	switch (controlValueUnit)
	{
		case la::avdecc::entity::model::ControlValueUnit::Unit::Unitless:
			return "Unitless";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Count:
			return "Count";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Percent:
			return "Percent";
		case la::avdecc::entity::model::ControlValueUnit::Unit::FStop:
			return "fstop";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Seconds:
			return "Seconds";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Minutes:
			return "Minutes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Hours:
			return "Hours";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Days:
			return "Days";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Months:
			return "Months";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Years:
			return "Years";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Samples:
			return "Samples";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Frames:
			return "Frames";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Hertz:
			return "Hertz";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Semitones:
			return "Semitones";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Cents:
			return "Cents";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Octaves:
			return "Octaves";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Fps:
			return "FPS";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Metres:
			return "Metres";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Kelvin:
			return "Kelvin";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Grams:
			return "Grams";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Volts:
			return "Volts";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbv:
			return "dBV";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbu:
			return "dBu";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Amps:
			return "Amps";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Watts:
			return "Watts";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbm:
			return "dBm";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbw:
			return "dBW";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Pascals:
			return "Pascals";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Bits:
			return "Bits";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Bytes:
			return "Bytes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::KibiBytes:
			return "KibiBytes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MebiBytes:
			return "MebiBytes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::GibiBytes:
			return "GibiBytes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::TebiBytes:
			return "TebiBytes";
		case la::avdecc::entity::model::ControlValueUnit::Unit::BitsPerSec:
			return "Bits Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::BytesPerSec:
			return "Bytes Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::KibiBytesPerSec:
			return "KibiBytes Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MebiBytesPerSec:
			return "MebiBytes Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::GibiBytesPerSec:
			return "GibiBytes Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::TebiBytesPerSec:
			return "TebiBytes Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Candelas:
			return "Candelas";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Joules:
			return "Joules";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Radians:
			return "Radians";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Newtons:
			return "Newtons";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Ohms:
			return "Ohms";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MetresPerSec:
			return "Metres Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::RadiansPerSec:
			return "Radians Per Sec";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MetresPerSecSquared:
			return "Metres Per Sec Squared";
		case la::avdecc::entity::model::ControlValueUnit::Unit::RadiansPerSecSquared:
			return "Radians Per Sec Squared";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Teslas:
			return "Teslas";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Webers:
			return "Webers";
		case la::avdecc::entity::model::ControlValueUnit::Unit::AmpsPerMetre:
			return "Amps Per Metre";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MetresSquared:
			return "Metres Squared";
		case la::avdecc::entity::model::ControlValueUnit::Unit::MetresCubed:
			return "Metres Cubed";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Litres:
			return "Litres";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Db:
			return "dB";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbPeak:
			return "dB Peak";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbRms:
			return "dB RMS";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbfs:
			return "dBFS";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbfsPeak:
			return "dBFS Peak";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbfsRms:
			return "dBFS RMS";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Dbtp:
			return "dBTP";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbSplA:
			return "dB(A) SPL";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbZ:
			return "dB(Z)";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbSplC:
			return "dB(C) SPL";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbSpl:
			return "dB SPL";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Lu:
			return "LU";
		case la::avdecc::entity::model::ControlValueUnit::Unit::Lufs:
			return "LUFS";
		case la::avdecc::entity::model::ControlValueUnit::Unit::DbA:
			return "dB(A)";
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
			return "Protocol Interface";
		case la::avdecc::logger::Layer::AemPayload:
			return "AemPayload";
		case la::avdecc::logger::Layer::Entity:
			return "Entity";
		case la::avdecc::logger::Layer::ControllerEntity:
			return "Controller Entity";
		case la::avdecc::logger::Layer::ControllerStateMachine:
			return "Controller State Machine";
		case la::avdecc::logger::Layer::Controller:
			return "Controller";
		case la::avdecc::logger::Layer::JsonSerializer:
			return "Json Serializer";
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

QString generateDumpSourceString(QString const& shortName, QString const& version) noexcept
{
	static auto s_DumpSource = QString{ "%1 v%2 using L-Acoustics AVDECC Controller v%3" }.arg(shortName).arg(version).arg(la::avdecc::controller::getVersion().c_str());

	return s_DumpSource;
}

bool isValidEntityModelID(la::avdecc::UniqueIdentifier const entityModelID) noexcept
{
	if (entityModelID)
	{
		auto const [vendorID, deviceID, modelID] = la::avdecc::entity::model::splitEntityModelID(entityModelID);
		return vendorID != 0x00000000 && vendorID != 0x00FFFFFF;
	}
	return false;
}

bool isEntityModelComplete(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto controlledEntity = manager.getControlledEntity(entityID);

	if (controlledEntity)
	{
		return controlledEntity->isEntityModelValidForCaching();
	}

	return true;
}

void smartChangeInputStreamFormat(QWidget* const parent, bool const autoRemoveMappings, la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::model::StreamIndex const streamIndex, la::avdecc::entity::model::StreamFormat const streamFormat, QObject* context, std::function<void(hive::modelsLibrary::CommandsExecutor::ExecutorResult const result)> const& handler) noexcept
{
	AVDECC_ASSERT(context != nullptr, "context must not be nullptr");

	auto& manager = hive::modelsLibrary::ControllerManager::getInstance();
	auto const entity = manager.getControlledEntity(entityID);
	if (entity)
	{
		// Check if we need to remove mappings
		try
		{
			auto const invalidMappings = entity->getStreamPortInputInvalidAudioMappingsForStreamFormat(streamIndex, streamFormat);
			if (!invalidMappings.empty() && parent != nullptr && !autoRemoveMappings)
			{
				auto result = QMessageBox::warning(parent, "", "One or more StreamInput mapping will be invalid once the format is changed.\nAutomatically remove invalid one(s)?", QMessageBox::StandardButton::Abort | QMessageBox::StandardButton::Yes, QMessageBox::StandardButton::Yes);
				if (result == QMessageBox::StandardButton::Abort)
				{
					la::avdecc::utils::invokeProtectedHandler(handler, hive::modelsLibrary::CommandsExecutor::ExecutorResult{ hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::Aborted });
					return;
				}
			}
			manager.createCommandsExecutor(entityID, !invalidMappings.empty(),
				[parent, streamIndex, streamFormat, context, handler, &invalidMappings](hive::modelsLibrary::CommandsExecutor& executor)
				{
					for (auto const& [streamPortIndex, mappings] : invalidMappings)
					{
						executor.addAemCommand(&hive::modelsLibrary::ControllerManager::removeStreamPortInputAudioMappings, streamPortIndex, mappings);
					}
					executor.addAemCommand(&hive::modelsLibrary::ControllerManager::setStreamInputFormat, streamIndex, streamFormat);
					context->connect(&executor, &hive::modelsLibrary::CommandsExecutor::executionComplete, context,
						[parent, handler](hive::modelsLibrary::CommandsExecutor::ExecutorResult const result)
						{
							if (parent != nullptr)
							{
								switch (result.getResult())
								{
									case hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::Success:
									case hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::Aborted: // No need for a message when aborted
										break;
									case hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::UnknownEntity:
										QMessageBox::warning(parent, "", "Failed to change Stream Format:<br> Unknown Entity");
										break;
									case hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::AemError:
										QMessageBox::warning(parent, "", "Failed to change Stream Format:<br>" + QString::fromStdString(la::avdecc::entity::ControllerEntity::statusToString(result.getAemStatus())));
										break;
									default:
										QMessageBox::warning(parent, "", "Failed to change Stream Format:<br> Unknown Error");
										break;
								}
							}
							la::avdecc::utils::invokeProtectedHandler(handler, result);
						});
				});
		}
		catch (...)
		{
			la::avdecc::utils::invokeProtectedHandler(handler, hive::modelsLibrary::CommandsExecutor::ExecutorResult{ hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::InternalError });
		}
	}
	else
	{
		la::avdecc::utils::invokeProtectedHandler(handler, hive::modelsLibrary::CommandsExecutor::ExecutorResult{ hive::modelsLibrary::CommandsExecutor::ExecutorResult::Result::UnknownEntity });
	}
}

} // namespace helper
} // namespace avdecc
