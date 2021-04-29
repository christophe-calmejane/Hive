/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QDialog>

class SettingsDialogImpl;
class SettingsDialog : public QDialog
{
	Q_OBJECT
public:
	SettingsDialog(QWidget* parent = nullptr);
	virtual ~SettingsDialog() noexcept;

	// Deleted compiler auto-generated methods
	SettingsDialog(SettingsDialog&&) = delete;
	SettingsDialog(SettingsDialog const&) = delete;
	SettingsDialog& operator=(SettingsDialog const&) = delete;
	SettingsDialog& operator=(SettingsDialog&&) = delete;

private:
	// General
	Q_SLOT void on_automaticPNGDownloadCheckBox_toggled(bool checked);
	Q_SLOT void on_clearLogoCacheButton_clicked();
	Q_SLOT void on_automaticCheckForUpdatesCheckBox_toggled(bool checked);
	Q_SLOT void on_checkForBetaVersionsCheckBox_toggled(bool checked);
	Q_SLOT void on_themeColorComboBox_currentIndexChanged(int index);

	// Connection Matrix
	Q_SLOT void on_transposeConnectionMatrixCheckBox_toggled(bool checked);
	Q_SLOT void on_alwaysShowArrowTipConnectionMatrixCheckBox_toggled(bool checked);
	Q_SLOT void on_alwaysShowArrowEndConnectionMatrixCheckBox_toggled(bool checked);
	Q_SLOT void on_showMediaLockedDotCheckBox_toggled(bool checked);

	// Controller
	Q_SLOT void on_discoveryDelayLineEdit_returnPressed();
	Q_SLOT void on_enableAEMCacheCheckBox_toggled(bool checked);
	Q_SLOT void on_fullAEMEnumerationCheckBox_toggled(bool checked);

	// Network
	Q_SLOT void on_protocolComboBox_currentIndexChanged(int index);

	SettingsDialogImpl* _pImpl{ nullptr };
};
