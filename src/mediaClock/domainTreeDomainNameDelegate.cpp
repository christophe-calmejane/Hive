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

#include "domainTreeDomainNameDelegate.hpp"
#include "ui_domainTreeDomainNameDelegate.h"
#include "internals/config.hpp"
#include "settingsManager/settings.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "mediaClock/domainTreeModel.hpp"

#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

// **************************************************************
// class MediaClockTableDomainEditDelegateImpl
// **************************************************************
/**
* @brief    Private implemenation of the tree view delegate for domains.
* [@author  Marius Erlen]
* [@date    2018-09-21]
*
* This delegate is used to display domain rows.
*/
class DomainTreeDomainEditDelegateImpl final : private Ui::MediaClockDomainTreeDelegate
{
public:
	/**
	* Constructor.
	* Sets up the ui.
	* Removes the background from the widget.
	*/
	DomainTreeDomainEditDelegateImpl(::DomainTreeDomainEditDelegate* parent)
	{
		// Link UI
		m_widget = parent;
		setupUi(parent);
		QPalette pal;
		pal.setColor(QPalette::Window, Qt::transparent);
		m_widget->setPalette(pal);
	}

	/**
	* Gets the left labl of the delegates view.
	*/
	QLabel* getLabel()
	{
		return label;
	}

	/**
	* Gets the comboBox of the delegates view.
	*/
	QComboBox* getComboBox()
	{
		return comboBox;
	}

private:
	DomainTreeDomainEditDelegate* m_widget;
};

/**
* Constructor.
*/
DomainTreeDomainEditDelegate::DomainTreeDomainEditDelegate(QWidget* parent)
	: QWidget(parent)
	, _pImpl(new DomainTreeDomainEditDelegateImpl(this))
{
}

/**
* Destructor.
*/
DomainTreeDomainEditDelegate::~DomainTreeDomainEditDelegate() noexcept
{
	delete _pImpl;
}

/**
* Gets the left labl of the delegates view.
*/
QLabel* DomainTreeDomainEditDelegate::getLabel()
{
	return _pImpl->getLabel();
}

/**
* Gets the comboBox of the delegates view.
*/
QComboBox* DomainTreeDomainEditDelegate::getComboBox()
{
	return _pImpl->getComboBox();
}
