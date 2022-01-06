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

#include "aboutDialog.hpp"
#include "ui_aboutDialog.h"
#include "internals/config.hpp"
#include <la/avdecc/avdecc.hpp>
#include <la/avdecc/controller/avdeccController.hpp>

#include <QObject>
#include <QPushButton>
#include <QTextBrowser>
#include <QFile>

extern "C"
{
#include <mkdio.h>
}

class AboutDialogImpl final : public QWidget, private Ui::AboutDialog
{
public:
	AboutDialogImpl(::AboutDialog* parent)
		: _parent(parent)
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
		auto const configuredText = aboutLabel->text().arg(hive::internals::applicationLongName).arg(hive::internals::versionString).arg(hive::internals::buildArchitecture).arg(hive::internals::buildConfiguration).arg(la::avdecc::getVersion().c_str()).arg(avdeccOptions.c_str()).arg(la::avdecc::controller::getVersion().c_str()).arg(avdeccControllerOptions.c_str()).arg(hive::internals::authors).arg(hive::internals::projectURL).arg(qVersion());
		aboutLabel->setText(configuredText);

		connect(legalNotices, &QPushButton::clicked, this, &AboutDialogImpl::on_legalNotices_clicked);
	}

private:
	Q_SLOT void on_legalNotices_clicked()
	{
		// Create dialog popup
		auto dialog = QDialog{ _parent };
		auto layout = QVBoxLayout{ &dialog };
		auto view = QTextBrowser{};
		layout.addWidget(&view);
		dialog.setWindowTitle(hive::internals::applicationShortName + " - Legal Notices");
		dialog.resize(800, 600);
		auto closeButton = QPushButton{ "Close" };
		connect(&closeButton, &QPushButton::clicked, &dialog,
			[&dialog]()
			{
				dialog.accept();
			});
		layout.addWidget(&closeButton);

		view.setContextMenuPolicy(Qt::NoContextMenu);
		view.setOpenExternalLinks(true);
		auto noticesFile = QFile(":/legal_notices.md");
		if (noticesFile.open(QIODevice::ReadOnly))
		{
			auto content = QString(noticesFile.readAll()).toLocal8Bit();
			auto* mmiot = mkd_string(content.data(), content.size(), 0);
			if (mmiot == nullptr)
				return;
			std::unique_ptr<MMIOT, std::function<void(MMIOT*)>> scopedMmiot{ mmiot, [](MMIOT* ptr)
				{
					if (ptr != nullptr)
						mkd_cleanup(ptr);
				} };

			if (mkd_compile(mmiot, 0) == 0)
				return;

			char* docPointer{ nullptr };
			auto const docLength = mkd_document(mmiot, &docPointer);
			if (docLength == 0)
				return;

			view.setHtml(QString::fromUtf8(docPointer, docLength));

			// Run dialog
			dialog.exec();
		}
	}

	::AboutDialog* _parent{ nullptr };
};

AboutDialog::AboutDialog(QWidget* parent)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
	, _pImpl(new AboutDialogImpl(this))
{
	setWindowTitle(QCoreApplication::applicationName() + " - Version " + QCoreApplication::applicationVersion());
	layout()->setSizeConstraint(QLayout::SetFixedSize);
	setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
}

AboutDialog::~AboutDialog() noexcept
{
	delete _pImpl;
}
