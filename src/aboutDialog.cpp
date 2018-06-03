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

#include "aboutDialog.hpp"
#include "ui_aboutDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

class AboutDialogImpl final : private Ui::AboutDialog
{
public:
	AboutDialogImpl(::AboutDialog* parent)
	{
		// Link UI
		setupUi(parent);

		// Build avdecc libraries compile options string
		std::string avdeccOptions{};
		for (auto const& info : la::avdecc::getCompileOptionsInfo())
		{
			if (!avdeccOptions.empty())
				avdeccOptions += "+";
			avdeccOptions += info.shortName;
		}
		std::string avdeccControllerOptions{};
		for (auto const& info : la::avdecc::controller::getCompileOptionsInfo())
		{
			if (!avdeccControllerOptions.empty())
				avdeccControllerOptions += "+";
			avdeccControllerOptions += info.shortName;
		}

		// Configure text
		auto const configuredText = aboutLabel->text()
			.arg(hive::internals::applicationLongName)
			.arg(hive::internals::versionString)
			.arg(hive::internals::buildArchitecture)
			.arg(hive::internals::buildConfiguration)
			.arg(la::avdecc::getVersion().c_str())
			.arg(avdeccOptions.c_str())
			.arg(la::avdecc::controller::getVersion().c_str())
			.arg(avdeccControllerOptions.c_str())
			.arg(hive::internals::authors)
			.arg(hive::internals::projectURL);
		aboutLabel->setText(configuredText);
	}
};

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent), _pImpl(new AboutDialogImpl(this))
{
	setWindowTitle(QCoreApplication::applicationName() + " - Version " + QCoreApplication::applicationVersion());
}

AboutDialog::~AboutDialog() noexcept
{
	delete _pImpl;
}
