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

/**
* @file helper_tests.cpp
* @author Christophe Calmejane
*/

#include <gtest/gtest.h>
#include <avdecc/helper.hpp>
#include <hive/modelsLibrary/helper.hpp>

// Reserved bits (out of specification device, or newer specification) must be displayed with their value
TEST(Helper, FlagsToStringUnknownBit)
{
	auto flags = la::avdecc::entity::StreamInfoFlagsEx{};
	flags.assign(0x00040001u);
	EXPECT_EQ(QString{ "Registering | Unknown 0x00040000" }, avdecc::helper::flagsToString(flags));
}

TEST(Helper, FlagsToStringAvbInterfaceFlags)
{
	auto const flags = la::avdecc::entity::AvbInterfaceFlags{ la::avdecc::entity::AvbInterfaceFlag::GptpGrandmasterSupported, la::avdecc::entity::AvbInterfaceFlag::GptpSupported, la::avdecc::entity::AvbInterfaceFlag::SrpSupported, la::avdecc::entity::AvbInterfaceFlag::FqtssNotSupported, la::avdecc::entity::AvbInterfaceFlag::ScheduledTrafficSupported, la::avdecc::entity::AvbInterfaceFlag::CanListenToSelf, la::avdecc::entity::AvbInterfaceFlag::CanListenToOtherSelf };
	EXPECT_EQ(QString{ "GptpGrandmasterSupported | GptpSupported | SrpSupported | FqtssNotSupported | ScheduledTrafficSupported | CanListenToSelf | CanListenToOtherSelf" }, avdecc::helper::flagsToString(flags));
}

TEST(Helper, FlagsToStringAvbInfoFlags)
{
	auto const flags = la::avdecc::entity::AvbInfoFlags{ la::avdecc::entity::AvbInfoFlag::AsCapable, la::avdecc::entity::AvbInfoFlag::AvtpDown, la::avdecc::entity::AvbInfoFlag::AvtpDownValid };
	EXPECT_EQ(QString{ "AS Capable | Avtp Down | Avtp Down Valid" }, avdecc::helper::flagsToString(flags));
}

TEST(Helper, CapabilitiesToStringUnknownBit)
{
	auto caps = la::avdecc::entity::ControllerCapabilities{};
	caps.assign(0x00000003u);
	EXPECT_EQ(QString{ "Implemented | Unknown 0x00000002" }, avdecc::helper::capabilitiesToString(caps));
}

// Values not defined by the specification must be displayed with their value
TEST(Helper, EnumToStringUnknownValue)
{
	EXPECT_EQ(QString{ "Unknown 0x1234" }, avdecc::helper::jackTypeToString(static_cast<la::avdecc::entity::model::JackType>(0x1234)));
	EXPECT_EQ(QString{ "Unknown 0x00F0" }, avdecc::helper::clockSourceTypeToString(static_cast<la::avdecc::entity::model::ClockSourceType>(0x00F0)));
	EXPECT_EQ(QString{ "Unknown 0x7F" }, avdecc::helper::audioClusterFormatToString(static_cast<la::avdecc::entity::model::AudioClusterFormat>(0x7F)));
	EXPECT_EQ(QString{ "Unknown 0x42" }, avdecc::helper::msrpFailureCodeToString(static_cast<la::avdecc::entity::model::MsrpFailureCode>(0x42)));
	EXPECT_EQ(QString{ "Unknown 0x07" }, avdecc::helper::probingStatusToString(static_cast<la::avdecc::entity::model::ProbingStatus>(0x07)));
	EXPECT_EQ(QString{ "Unknown 0x0100" }, avdecc::helper::memoryObjectTypeToString(static_cast<la::avdecc::entity::model::MemoryObjectType>(0x0100)));
}

// An unknown vendor is displayed as its OUI-24, which is 6 hex digits wide (not zero filled to the 8 digits of the integer type holding it)
TEST(Helper, VendorNameUnknownOui)
{
	// Locally administered OUI (never assigned by the IEEE), with a leading zero digit
	EXPECT_EQ(QString{ "0x0A1234" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x0A1234FFFE000001u }));
}

// Every OUI size is resolved, whether the identifier is a native EUI-64 or an EUI-48 encapsulated in an EUI-64 (0xFFFE inserted after its first 3 octets, like most gPTP clock identities and many EntityIDs)
TEST(Helper, VendorNameAllOuiSizes)
{
	// OUI-24 (MA-L)
	EXPECT_EQ(QString{ "L-Acoustics" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x001B92FFFE050E61u }));
	// OUI-28 (MA-M): native, then encapsulated (MAC address 48:0B:B2:D8:00:00)
	EXPECT_EQ(QString{ "M2Lab" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x480BB2D000000001u }));
	EXPECT_EQ(QString{ "M2Lab" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x480BB2FFFED80000u }));
	// OUI-36 (MA-S): native, then encapsulated (MAC address 00:50:C2:9C:91:23)
	EXPECT_EQ(QString{ "Luminex" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x0050C29C90000001u }));
	EXPECT_EQ(QString{ "Luminex" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x0050C2FFFE9C9123u }));
	// Another OUI-28 of the same OUI-24 block (MAC address 48:0B:B2:A0:00:00), not registered: falls back to the OUI-24
	EXPECT_EQ(QString{ "0x480BB2" }, hive::modelsLibrary::helper::getVendorName(la::avdecc::UniqueIdentifier{ 0x480BB2FFFEA00000u }));
}
