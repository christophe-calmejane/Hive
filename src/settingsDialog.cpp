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

#include "settingsDialog.hpp"
#include "ui_settingsDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include "settingsManager/settings.hpp"
#include "entityLogoCache.hpp"
#include "toolkit/material/colorPalette.hpp"
#include "toolkit/tickableMenu.hpp"
#include "networkInterfaceTypeModel.hpp"

Q_DECLARE_METATYPE(la::avdecc::protocol::ProtocolInterface::Type)

class SettingsDialogImpl final : public Ui::SettingsDialog
{
public:
	SettingsDialogImpl(::SettingsDialog* parent)
	{
		// Link UI
		setupUi(parent);

		// Additional UI initialization
		if (auto* closeButton = buttonBox->button(QDialogButtonBox::Close))
		{
			closeButton->setDefault(false);
			closeButton->setAutoDefault(false);
		}
		discoveryDelayLineEdit->setValidator(new QIntValidator{ 0, 999, discoveryDelayLineEdit });

		// Initialize settings (blocking signals)
		loadGeneralSettings();
		loadConnectionMatrixSettings();
		loadControllerSettings();
		loadNetworkSettings();
	}

private:
	void loadGeneralSettings()
	{
		auto& settings = settings::SettingsManager::getInstance();

		// Automatic PNG Download
		{
			auto const lock = QSignalBlocker{ automaticPNGDownloadCheckBox };
			automaticPNGDownloadCheckBox->setChecked(settings.getValue(settings::General_AutomaticPNGDownloadEnabled.name).toBool());
		}

		// Automatic Check For Updates
		{
			auto const lock = QSignalBlocker{ automaticCheckForUpdatesCheckBox };
			automaticCheckForUpdatesCheckBox->setChecked(settings.getValue(settings::General_AutomaticCheckForUpdates.name).toBool());
		}

		// Theme Color
		{
			auto const lock = QSignalBlocker{ themeColorComboBox };
			themeColorComboBox->setModel(&_themeColorModel);
			themeColorComboBox->setModelColumn(_themeColorModel.index(qt::toolkit::material::color::DefaultShade));
			themeColorComboBox->setCurrentIndex(settings.getValue(settings::General_ThemeColorIndex.name).toInt());
		}
	}

	void loadConnectionMatrixSettings()
	{
		auto& settings = settings::SettingsManager::getInstance();

		// Transpose
		{
			auto const lock = QSignalBlocker{ transposeConnectionMatrixCheckBox };
			transposeConnectionMatrixCheckBox->setChecked(settings.getValue(settings::ConnectionMatrix_Transpose.name).toBool());
		}

		// Always Show Arrow Tip
		{
			auto const lock = QSignalBlocker{ alwaysShowArrowTipConnectionMatrixCheckBox };
			alwaysShowArrowTipConnectionMatrixCheckBox->setChecked(settings.getValue(settings::ConnectionMatrix_AlwaysShowArrowTip.name).toBool());
		}

		// Always Show Arrow End
		{
			auto const lock = QSignalBlocker{ alwaysShowArrowEndConnectionMatrixCheckBox };
			alwaysShowArrowEndConnectionMatrixCheckBox->setChecked(settings.getValue(settings::ConnectionMatrix_AlwaysShowArrowEnd.name).toBool());
		}
	}

	void loadControllerSettings()
	{
		auto& settings = settings::SettingsManager::getInstance();

		// Check For Beta Updates
		{
			auto const lock = QSignalBlocker{ checkForBetaVersionsCheckBox };
			checkForBetaVersionsCheckBox->setChecked(settings.getValue(settings::General_CheckForBetaVersions.name).toBool());
			auto const enabled = automaticCheckForUpdatesCheckBox->isChecked();
			checkForBetaVersionsLabel->setEnabled(enabled);
			checkForBetaVersionsCheckBox->setEnabled(enabled);
		}

		// Discovery Delay
		{
			auto const lock = QSignalBlocker{ discoveryDelayLineEdit };
			discoveryDelayLineEdit->setText(settings.getValue(settings::Controller_DiscoveryDelay.name).toString());
		}

		// AEM Cache
		{
			auto const lock = QSignalBlocker{ enableAEMCacheCheckBox };
			enableAEMCacheCheckBox->setChecked(settings.getValue(settings::Controller_AemCacheEnabled.name).toBool());
		}

		// Full Static Model
		{
			auto const lock = QSignalBlocker{ fullAEMEnumerationCheckBox };
			fullAEMEnumerationCheckBox->setChecked(settings.getValue(settings::Controller_FullStaticModelEnabled.name).toBool());
		}
	}

	void loadNetworkSettings()
	{
		auto& settings = settings::SettingsManager::getInstance();

		// Protocol Interface
		{
			auto const lock = QSignalBlocker{ protocolComboBox };
			populateProtocolComboBox();

			auto const type = settings.getValue(settings::Network_ProtocolType.name).value<la::avdecc::protocol::ProtocolInterface::Type>();
			auto const index = protocolComboBox->findData(QVariant::fromValue(type));
			protocolComboBox->setCurrentIndex(index);
		}

		// Interface Types
		{
			interfaceTypeList->setModel(&_networkInterfaceTypeModel);
		}
	}

private:
	void populateProtocolComboBox()
	{
		const std::map<la::avdecc::protocol::ProtocolInterface::Type, QString> protocolInterfaceName{
			{ la::avdecc::protocol::ProtocolInterface::Type::PCap, "PCap" },
			{ la::avdecc::protocol::ProtocolInterface::Type::MacOSNative, "MacOS Native" },
			{ la::avdecc::protocol::ProtocolInterface::Type::Proxy, "Proxy" },
			{ la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Virtual" },
		};

		for (auto const& type : la::avdecc::protocol::ProtocolInterface::getSupportedProtocolInterfaceTypes())
		{
#ifndef DEBUG
			if (type == la::avdecc::protocol::ProtocolInterface::Type::Virtual)
				continue;
#endif // !DEBUG
			protocolComboBox->addItem(protocolInterfaceName.at(type), QVariant::fromValue(type));
		}
	}

private:
	qt::toolkit::material::color::Palette _themeColorModel;
	NetworkInterfaceTypeModel _networkInterfaceTypeModel;
};

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
	, _pImpl(new SettingsDialogImpl(this))
{
	setWindowTitle(QCoreApplication::applicationName() + " Settings");
	layout()->setSizeConstraint(QLayout::SetFixedSize);
	setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
}

SettingsDialog::~SettingsDialog() noexcept
{
	delete _pImpl;
}

void SettingsDialog::on_automaticPNGDownloadCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::General_AutomaticPNGDownloadEnabled.name, checked);
}

void SettingsDialog::on_clearLogoCacheButton_clicked()
{
	auto& logoCache{ EntityLogoCache::getInstance() };
	logoCache.clear();
}

void SettingsDialog::on_automaticCheckForUpdatesCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::General_AutomaticCheckForUpdates.name, checked);

	_pImpl->checkForBetaVersionsLabel->setEnabled(checked);
	_pImpl->checkForBetaVersionsCheckBox->setEnabled(checked);
}

void SettingsDialog::on_checkForBetaVersionsCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::General_CheckForBetaVersions.name, checked);
}

void SettingsDialog::on_themeColorComboBox_currentIndexChanged(int index)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::General_ThemeColorIndex.name, index);
}

void SettingsDialog::on_transposeConnectionMatrixCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::ConnectionMatrix_Transpose.name, checked);
}

void SettingsDialog::on_alwaysShowArrowTipConnectionMatrixCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::ConnectionMatrix_AlwaysShowArrowTip.name, checked);
}

void SettingsDialog::on_alwaysShowArrowEndConnectionMatrixCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::ConnectionMatrix_AlwaysShowArrowEnd.name, checked);
}

void SettingsDialog::on_discoveryDelayLineEdit_returnPressed()
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::Controller_DiscoveryDelay.name, _pImpl->discoveryDelayLineEdit->text());
}

void SettingsDialog::on_enableAEMCacheCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::Controller_AemCacheEnabled.name, checked);
}

void SettingsDialog::on_fullAEMEnumerationCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::Controller_FullStaticModelEnabled.name, checked);
}

void SettingsDialog::on_protocolComboBox_currentIndexChanged(int /*index*/)
{
	auto& settings = settings::SettingsManager::getInstance();
	auto const type = _pImpl->protocolComboBox->currentData().value<la::avdecc::protocol::ProtocolInterface::Type>();
	settings.setValue(settings::Network_ProtocolType.name, la::avdecc::utils::to_integral(type));
}
