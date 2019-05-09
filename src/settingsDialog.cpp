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

#include "settingsDialog.hpp"
#include "ui_settingsDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include "settingsManager/settings.hpp"
#include "entityLogoCache.hpp"
#include "toolkit/material/colorPalette.hpp"

class SettingsDialogImpl final : public Ui::SettingsDialog
{
public:
	SettingsDialogImpl(::SettingsDialog* parent)
	{
		// Link UI
		setupUi(parent);

		// Initialize settings (blocking signals)
		auto& settings = settings::SettingsManager::getInstance();

		// Automatic PNG Download
		{
			QSignalBlocker lock(automaticPNGDownloadCheckBox);
			automaticPNGDownloadCheckBox->setChecked(settings.getValue(settings::AutomaticPNGDownloadEnabled.name).toBool());
		}

		// Transpose Connection Matrix
		{
			QSignalBlocker lock(transposeConnectionMatrixCheckBox);
			transposeConnectionMatrixCheckBox->setChecked(settings.getValue(settings::TransposeConnectionMatrix.name).toBool());
		}

		// Automatic Check For Updates
		{
			QSignalBlocker lock(automaticCheckForUpdatesCheckBox);
			automaticCheckForUpdatesCheckBox->setChecked(settings.getValue(settings::AutomaticCheckForUpdates.name).toBool());
		}

		// Theme Color
		{
			QSignalBlocker lock(themeColorComboBox);
			themeColorComboBox->setModel(&_themeColorModel);
			themeColorComboBox->setModelColumn(_themeColorModel.index(qt::toolkit::material::color::DefaultShade));
			themeColorComboBox->setCurrentIndex(settings.getValue(settings::ThemeColorIndex.name).toInt());
		}

		// Check For Beta Updates
		{
			QSignalBlocker lock(checkForBetaVersionsCheckBox);
			checkForBetaVersionsCheckBox->setChecked(settings.getValue(settings::CheckForBetaVersions.name).toBool());
			auto const enabled = automaticCheckForUpdatesCheckBox->isChecked();
			checkForBetaVersionsLabel->setEnabled(enabled);
			checkForBetaVersionsCheckBox->setEnabled(enabled);
		}

		// AEM Cache
		{
			QSignalBlocker lock(enableAEMCacheCheckBox);
			enableAEMCacheCheckBox->setChecked(settings.getValue(settings::AemCacheEnabled.name).toBool());
		}
	}

private:
	qt::toolkit::material::color::Palette _themeColorModel;
};

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent)
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
	settings.setValue(settings::AutomaticPNGDownloadEnabled.name, checked);
}

void SettingsDialog::on_clearLogoCacheButton_clicked()
{
	auto& logoCache{ EntityLogoCache::getInstance() };
	logoCache.clear();
}

void SettingsDialog::on_transposeConnectionMatrixCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::TransposeConnectionMatrix.name, checked);
}

void SettingsDialog::on_automaticCheckForUpdatesCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::AutomaticCheckForUpdates.name, checked);

	_pImpl->checkForBetaVersionsLabel->setEnabled(checked);
	_pImpl->checkForBetaVersionsCheckBox->setEnabled(checked);
}

void SettingsDialog::on_checkForBetaVersionsCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::CheckForBetaVersions.name, checked);
}

void SettingsDialog::on_themeColorComboBox_currentIndexChanged(int index)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::ThemeColorIndex.name, index);
}

void SettingsDialog::on_enableAEMCacheCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();
	settings.setValue(settings::AemCacheEnabled.name, checked);
}
