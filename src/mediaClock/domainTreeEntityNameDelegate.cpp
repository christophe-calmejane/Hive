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

#include "domainTreeEntityNameDelegate.hpp"
#include "ui_domainTreeEntityNameDelegate.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "mediaClock/domainTreeModel.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

// **************************************************************
// class MediaClockTableEntityNameDelegateImpl
// **************************************************************
/**
* @brief    Private implemenation of the tree view delegate for entities.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* This delegate is used to display entity rows.
*/
class DomainTreeEntityNameDelegateImpl final : private Ui::EntityNameDelegate
{
public:
	/**
	* Constructor.
	* Sets up the ui.
	* Removes the background from the widget.
	*/
	DomainTreeEntityNameDelegateImpl(::DomainTreeEntityNameDelegate* parent)
	{
		// Link UI
		m_parent = parent;
		setupUi(parent);

		QPalette pal;
		pal.setColor(QPalette::Window, Qt::transparent);
		m_parent->setPalette(pal);
	}

	/**
	* Gets the left labl of the delegates view.
	*/
	QLabel* getLabelLeft()
	{
		return labelLeft;
	}

	/**
	* Gets the right labl of the delegates view.
	*/
	QLabel* getLabelRight()
	{
		return labelRight;
	}

private:
	::DomainTreeEntityNameDelegate* m_parent;
};

/**
* Constructor.
*/
DomainTreeEntityNameDelegate::DomainTreeEntityNameDelegate(QWidget* parent)
	: QWidget(parent)
	, _pImpl(new DomainTreeEntityNameDelegateImpl(this))
{
}

/**
* Destructor.
*/
DomainTreeEntityNameDelegate::~DomainTreeEntityNameDelegate() noexcept
{
	delete _pImpl;
}

/**
* Gets the left labl of the delegates view.
*/
QLabel* DomainTreeEntityNameDelegate::getLabelLeft()
{
	return _pImpl->getLabelLeft();
}

/**
* Gets the right labl of the delegates view.
*/
QLabel* DomainTreeEntityNameDelegate::getLabelRight()
{
	return _pImpl->getLabelRight();
}
