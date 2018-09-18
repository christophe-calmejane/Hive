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

#pragma once

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include <vector>
#include <tuple>

#include <QDialog>
#include <QString>

namespace Ui
{
class FirmwareUploadDialog;
}

class FirmwareUploadDialog : public QDialog
{
	Q_OBJECT

public:
	using EntityInfo = std::tuple<la::avdecc::UniqueIdentifier, la::avdecc::entity::model::DescriptorIndex, std::uint64_t>;
	explicit FirmwareUploadDialog(la::avdecc::controller::Controller::DeviceMemoryBuffer&& firmwareData, QString const& name, std::vector<EntityInfo> entitiesToUpdate, QWidget *parent = nullptr);
	~FirmwareUploadDialog();
	
private:
	enum class ItemRole
	{
		EntityID = Qt::UserRole + 1,
		DescriptorIndex,
		MemoryObjectAddress,
		EntityName,
		OperationID,
		IsStoreOperation,
	};

	void scheduleUpload(EntityInfo const& entityInfo);
	
	Q_SLOT void on_startPushButton_clicked();
	Q_SLOT void on_abortPushButton_clicked();

private:
	Ui::FirmwareUploadDialog *_ui{ nullptr };
	la::avdecc::controller::Controller::DeviceMemoryBuffer _firmwareData{};
};
