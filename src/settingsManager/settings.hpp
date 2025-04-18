/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "settingsManager.hpp"
#include "profiles/profiles.hpp"
#include "internals/config.hpp"

#include <la/avdecc/internals/protocolInterface.hpp>
#include <la/avdecc/utils.hpp>
#include <QtMate/material/colorPalette.hpp>

namespace settings
{
static constexpr auto ViewSettingsCurrentVersion = 5;

// Settings with a default initial value
static SettingsManager::SettingDefault LastLaunchedVersion = { "LastLaunchedVersion", "" };
static SettingsManager::SettingDefault UserProfile = { "userProfile", la::avdecc::utils::to_integral(profiles::ProfileType::None) };
static SettingsManager::SettingDefault LastCheckTime = { "LastCheckTime", QVariant::fromValue(std::uint64_t{ 0u }) };

// General settings
static SettingsManager::SettingDefault General_AutomaticPNGDownloadEnabled = { "avdecc/general/enableAutomaticPNGDownload", false };
static SettingsManager::SettingDefault General_AutomaticCheckForUpdates = { "avdecc/general/enableAutomaticCheckForUpdates", true };
static SettingsManager::SettingDefault General_CheckForBetaVersions = { "avdecc/general/enableCheckForBetaVersions", false };
static SettingsManager::SettingDefault General_ThemeColorIndex = { "avdecc/general/themeColorIndex", qtMate::material::color::Palette::index(qtMate::material::color::DefaultColor) };

// Connection matrix settings
static SettingsManager::SettingDefault ConnectionMatrix_Transpose = { "avdecc/connectionMatrix/transpose", false };
static SettingsManager::SettingDefault ConnectionMatrix_ChannelMode = { "avdecc/connectionMatrix/channelMode", false };
static SettingsManager::SettingDefault ConnectionMatrix_AlwaysShowArrowTip = { "avdecc/connectionMatrix/alwaysShowArrowTip", false };
static SettingsManager::SettingDefault ConnectionMatrix_AlwaysShowArrowEnd = { "avdecc/connectionMatrix/alwaysShowArrowEnd", false };
static SettingsManager::SettingDefault ConnectionMatrix_ShowMediaLockedDot = { "avdecc/connectionMatrix/showMediaLockedDot", true };
static SettingsManager::SettingDefault ConnectionMatrix_AllowCRFAudioConnection = { "avdecc/connectionMatrix/allowCRFAudioConnection", false };
static SettingsManager::SettingDefault ConnectionMatrix_CollapsedByDefault = { "avdecc/connectionMatrix/collapsedByDefault", true };
static SettingsManager::SettingDefault ConnectionMatrix_ShowEntitySummary = { "avdecc/connectionMatrix/showEntitySummary", true };

// Network settings
static SettingsManager::SettingDefault Network_ProtocolType = { "avdecc/network/protocolType", la::avdecc::utils::to_integral(la::avdecc::protocol::ProtocolInterface::Type::None) };
static SettingsManager::SettingDefault Network_InterfaceTypeEthernet = { "avdecc/network/interfaceType/ethernet", true };
static SettingsManager::SettingDefault Network_InterfaceTypeWiFi = { "avdecc/network/interfaceType/wifi", false };

// Controller settings
static SettingsManager::SettingDefault Controller_DiscoveryDelay = { "avdecc/controller/discoveryDelay", 0 };
static SettingsManager::SettingDefault Controller_AemCacheEnabled = { "avdecc/controller/enableAemCache", false };
static SettingsManager::SettingDefault Controller_FastEnumerationEnabled = { "avdecc/controller/enableFastEnumeration", false }; // Requires Controller_AemCacheEnabled
static SettingsManager::SettingDefault Controller_FullStaticModelEnabled = { "avdecc/controller/fullStaticModel", false };
static SettingsManager::SettingDefault Controller_AdvertisingEnabled = { "avdecc/controller/enableAdvertising", true };
#ifdef DEBUG
static SettingsManager::SettingDefault Controller_ControllerSubID = { "avdecc/controller/controllerSubID_Debug", (hive::internals::majorVersion * 100) + (hive::internals::minorVersion * 10) + 1 + (hive::internals::marketingDigits > 2u ? 0x8000 : 0) };
#else // !DEBUG
static SettingsManager::SettingDefault Controller_ControllerSubID = { "avdecc/controller/controllerSubID", (hive::internals::majorVersion * 100) + (hive::internals::minorVersion * 10) + (hive::internals::marketingDigits > 2u ? 0x8000 : 0) };
#endif // DEBUG

// Settings with no default initial value (no need to register with the SettingsManager) - Not allowed to call registerSettingObserver for those
static SettingsManager::Setting InterfaceID = { "interfaceID" };
static SettingsManager::Setting ViewSettingsVersion = { "viewSettingsVersion" }; // Must match ViewSettingsCurrentVersion
static SettingsManager::Setting ControllerDynamicHeaderViewState = { "controllerDynamicHeaderView/state" };
static SettingsManager::Setting LoggerDynamicHeaderViewState = { "loggerDynamicHeaderView/state" };
static SettingsManager::Setting EntityInspectorState = { "entityInspector/state" };
static SettingsManager::Setting SplitterState = { "splitter/state" };
static SettingsManager::Setting MainWindowGeometry = { "mainWindow/geometry" };
static SettingsManager::Setting MainWindowState = { "mainWindow/state" };

} // namespace settings
