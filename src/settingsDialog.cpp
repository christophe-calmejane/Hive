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

#include "settingsDialog.hpp"
#include "ui_settingsDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include "settingsManager/settings.hpp"
#include "entityLogoCache.hpp"

class SettingsDialogImpl final : private Ui::SettingsDialog
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

		// AEM Cache
		{
			QSignalBlocker lock(enableAEMCacheCheckBox);
			enableAEMCacheCheckBox->setChecked(settings.getValue(settings::AemCacheEnabled.name).toBool());
		}
	}
};

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent), _pImpl(new SettingsDialogImpl(this))
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
	auto& logoCache{EntityLogoCache::getInstance()};
	logoCache.clear();
}

void SettingsDialog::on_enableAEMCacheCheckBox_toggled(bool checked)
{
	auto& settings = settings::SettingsManager::getInstance();

	settings.setValue(settings::AemCacheEnabled.name, checked);
}
