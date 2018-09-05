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

#include <la/avdecc/controller/internals/avdeccControlledEntity.hpp>
#include "memoryObjectUploadWidget.hpp"

#include <QWidget>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QDebug>

class MemoryObjectUploadWidget : public QWidget
{
	Q_OBJECT
public:
	MemoryObjectUploadWidget(QWidget *parent = nullptr);
	MemoryObjectUploadWidget(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorIndex const descriptorIndex, std::uint64_t const address, QWidget *parent = nullptr);
	~MemoryObjectUploadWidget();

private:
	QHBoxLayout* _uploadMainLayout;

	QVBoxLayout* _stateLayout;
	QVBoxLayout* _progressLayout;
	QVBoxLayout* _fileLayout;
	QVBoxLayout* _actionLayout;

	QLabel* _stateLabel;
	QLabel* _currentUploadState;

	QLabel* _progressLabel;
	QProgressBar* _progressBar;

	QLabel* _fileLabel;
	QHBoxLayout* _fileHLayout;
	QLabel* _filePathLabel;
	QPushButton _fileSelect{ "Select file", this };

	QPushButton _actionStartButton{ "Start upload", this };
	QPushButton _actionAbortButton{ "Abort upload", this };

	QString _selectedFile;
	la::avdecc::UniqueIdentifier _targetEntityID;
	la::avdecc::entity::model::DescriptorIndex _descriptorIndex;
	std::uint64_t _address;
	
	void fileSelectClicked();
	void uploadClicked();
	void abortClicked();

	Q_SLOT void progressUpdate(la::avdecc::UniqueIdentifier const targetEntityID, la::avdecc::entity::model::DescriptorType descriptorType, la::avdecc::entity::model::DescriptorIndex descriptorIndex, std::uint16_t operationId, std::uint16_t percentComplete);
};
