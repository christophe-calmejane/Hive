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

#include <QWidget>
#include <QLabel>
#include <QComboBox>


class DomainTreeDomainEditDelegateImpl;

// **************************************************************
// class DomainTreeDomainEditDelegate
// **************************************************************
/**
* @brief    The ui of the tree view delegate for domains.
* [@author  Marius Erlen]
* [@date    2018-10-22]
*/
class DomainTreeDomainEditDelegate : public QWidget
{
	Q_OBJECT
public:
	DomainTreeDomainEditDelegate(QWidget* parent = nullptr);
	virtual ~DomainTreeDomainEditDelegate() noexcept;

	// Deleted compiler auto-generated methods
	DomainTreeDomainEditDelegate(DomainTreeDomainEditDelegate&&) = delete;
	DomainTreeDomainEditDelegate(DomainTreeDomainEditDelegate const&) = delete;
	DomainTreeDomainEditDelegate& operator=(DomainTreeDomainEditDelegate const&) = delete;
	DomainTreeDomainEditDelegate& operator=(DomainTreeDomainEditDelegate&&) = delete;

	QLabel* getLabel();
	QComboBox* getComboBox();

private:
	DomainTreeDomainEditDelegateImpl* _pImpl{ nullptr };
};
