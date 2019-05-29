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

#pragma once

#include "settingsManager.hpp"
#include "profiles/profiles.hpp"
#include "toolkit/material/colorPalette.hpp"
#include <la/avdecc/internals/protocolInterface.hpp>
#include <la/avdecc/utils.hpp>

namespace settings
{
// Settings with a default initial value
static SettingsManager::SettingDefault LastLaunchedVersion = { "LastLaunchedVersion", "" };
static SettingsManager::SettingDefault UserProfile = { "userProfile", la::avdecc::utils::to_integral(profiles::ProfileType::None) };

// General settings
static SettingsManager::SettingDefault AutomaticPNGDownloadEnabled = { "avdecc/general/enableAutomaticPNGDownload", false };
static SettingsManager::SettingDefault TransposeConnectionMatrix = { "avdecc/general/transposeConnectionMatrix", false };
static SettingsManager::SettingDefault AutomaticCheckForUpdates = { "avdecc/general/enableAutomaticCheckForUpdates", true };
static SettingsManager::SettingDefault CheckForBetaVersions = { "avdecc/general/enableCheckForBetaVersions", false };
static SettingsManager::SettingDefault ThemeColorIndex = { "avdecc/general/themeColorIndex", qt::toolkit::material::color::Palette::index(qt::toolkit::material::color::DefaultColor) };

// Network settings
static SettingsManager::SettingDefault ProtocolType = { "avdecc/network/protocolType", la::avdecc::utils::to_integral(la::avdecc::protocol::ProtocolInterface::Type::None) };
static SettingsManager::SettingDefault InterfaceTypeEthernet = { "avdecc/network/interfaceType/ethernet", true };
static SettingsManager::SettingDefault InterfaceTypeWiFi = { "avdecc/network/interfaceType/wifi", false };

// Controller settings
static SettingsManager::SettingDefault AemCacheEnabled = { "avdecc/controller/enableAemCache", false };

// Settings with no default initial value (no need to register with the SettingsManager) - Not allowed to call registerSettingObserver for those
static SettingsManager::Setting InterfaceID = { "interfaceID" };
static SettingsManager::Setting ControllerDynamicHeaderViewState = { "controllerDynamicHeaderView/state" };
static SettingsManager::Setting LoggerDynamicHeaderViewState = { "loggerDynamicHeaderView/state" };
static SettingsManager::Setting EntityInspectorState = { "entityInspector/state" };
static SettingsManager::Setting SplitterState = { "splitter/state" };
static SettingsManager::Setting MainWindowGeometry = { "mainWindow/geometry" };
static SettingsManager::Setting MainWindowState = { "mainWindow/state" };

} // namespace settings
