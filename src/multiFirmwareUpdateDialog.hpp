/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <vector>
#include <tuple>

#include <QDialog>
#include <QString>

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>
#include <QtMate/widgets/dynamicHeaderView.hpp>

namespace Ui
{
class MultiFirmwareUpdateDialog;
}


class Model;

class MultiFirmwareUpdateDialog : public QDialog
{
	Q_OBJECT

public:
	MultiFirmwareUpdateDialog(QWidget* parent = nullptr);
	~MultiFirmwareUpdateDialog();

private:
	enum class ItemRole
	{
		EntityID = Qt::UserRole + 1,
		DescriptorIndex,
		MemoryObjectAddress,
		EntityName,
	};

	void startFirmwareUpdate();


	Q_SLOT void handleContinueButtonClicked();

	Q_SLOT void onItemSelectionChanged(QItemSelection const& selected, QItemSelection const& deselected);

private:
	Ui::MultiFirmwareUpdateDialog* _ui{ nullptr };
	Model* _model{ nullptr };
};
