/*
* Copyright (C) 2017-2026, Emilien Vallot, Christophe Calmejane and other contributors

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

#include "compatibilityChangeEventsDialog.hpp"

#include <QCoreApplication>
#include <QDialog>
#include <QTableView>
#include <QAbstractTableModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QDateTime>
#include <QVariant>

class CompatibilityChangeEventsTableModel : public QAbstractTableModel
{
public:
	enum class Column
	{
		Timestamp = 0,
		PreviousCompatibility,
		NewCompatibility,
		SpecClause,
		Message,
		Count
	};

	explicit CompatibilityChangeEventsTableModel(la::avdecc::controller::ControlledEntity::CompatibilityChangedEvents const& events, QObject* parent = nullptr)
		: QAbstractTableModel{ parent }
		, _events{ events }
	{
	}

	// QAbstractTableModel overrides
	int rowCount(QModelIndex const& parent = QModelIndex()) const override
	{
		Q_UNUSED(parent)
		return static_cast<int>(_events.size());
	}

	int columnCount(QModelIndex const& parent = QModelIndex()) const override
	{
		Q_UNUSED(parent)
		return static_cast<int>(Column::Count);
	}

	QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const override
	{
		if (!index.isValid() || index.row() >= static_cast<int>(_events.size()))
		{
			return QVariant();
		}

		auto const& event = _events[index.row()];
		auto const column = static_cast<Column>(index.column());

		if (role == Qt::DisplayRole)
		{
			switch (column)
			{
				case Column::Timestamp:
				{
					auto const time_t = std::chrono::system_clock::to_time_t(event.timestamp);
					return QDateTime::fromSecsSinceEpoch(time_t).toString("yyyy-MM-dd hh:mm:ss");
				}
				case Column::PreviousCompatibility:
					return QString::fromStdString(compatibilityToString(event.previousFlags, event.previousMilanVersion));
				case Column::NewCompatibility:
					return QString::fromStdString(compatibilityToString(event.newFlags, event.newMilanVersion));
				case Column::SpecClause:
					return QString::fromStdString(event.specClause);
				case Column::Message:
					return QString::fromStdString(event.message);
				default:
					break;
			}
		}

		return QVariant();
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
	{
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		{
			auto const column = static_cast<Column>(section);
			switch (column)
			{
				case Column::Timestamp:
					return "Timestamp";
				case Column::PreviousCompatibility:
					return "Previous Compatibility";
				case Column::NewCompatibility:
					return "New Compatibility";
				case Column::SpecClause:
					return "Spec Clause";
				case Column::Message:
					return "Message";
				default:
					break;
			}
		}

		return QVariant();
	}

private:
	std::string compatibilityToString(la::avdecc::controller::ControlledEntity::CompatibilityFlags const& flags, la::avdecc::entity::model::MilanVersion const& milanVersion) const
	{
		std::vector<std::string> flagStrings;

		if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221))
		{
			flagStrings.push_back("IEEE17221");
		}
		if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Milan))
		{
			// Display Milan version instead of just "Milan"
			auto milanVersionStr = static_cast<std::string>(milanVersion);
			if (!milanVersionStr.empty() && milanVersionStr != "0.0.0.0")
			{
				flagStrings.push_back("Milan v" + milanVersionStr);
			}
			else
			{
				flagStrings.push_back("Milan");
			}
		}
		if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::IEEE17221Warning))
		{
			flagStrings.push_back("IEEE17221Warning");
		}
		if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::MilanWarning))
		{
			flagStrings.push_back("MilanWarning");
		}
		if (flags.test(la::avdecc::controller::ControlledEntity::CompatibilityFlag::Misbehaving))
		{
			flagStrings.push_back("Misbehaving");
		}

		if (flagStrings.empty())
		{
			return "None";
		}

		std::string result;
		for (size_t i = 0; i < flagStrings.size(); ++i)
		{
			if (i > 0)
				result += ", ";
			result += flagStrings[i];
		}

		return result;
	}

	la::avdecc::controller::ControlledEntity::CompatibilityChangedEvents _events{};
};

class CompatibilityChangeEventsDialog::CompatibilityChangeEventsDialogImpl : public QWidget
{
public:
	CompatibilityChangeEventsDialogImpl(QString const& title, la::avdecc::controller::ControlledEntity::CompatibilityChangedEvents const& events, CompatibilityChangeEventsDialog* parent)
		: _parent{ parent }
		, _tableModel{ new CompatibilityChangeEventsTableModel(events, this) }
		, _tableView{ new QTableView(this) }
		, _closeButton{ new QPushButton("Close", this) }
	{
		setupUI(title);
	}

private:
	void setupUI(QString const& title)
	{
		// Set window title
		_parent->setWindowTitle(title);
		_parent->resize(1000, 400);

		// Setup table view
		_tableView->setAccessibleName(QCoreApplication::translate("CompatibilityChangeEventsDialog", "Compatibility Change Events", nullptr));
		_tableView->setModel(_tableModel);
		_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
		_tableView->setAlternatingRowColors(true);
		_tableView->setSortingEnabled(false);
		_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

		// Auto resize columns to contents
		_tableView->horizontalHeader()->setStretchLastSection(true);
		_tableView->resizeColumnsToContents();

		// Setup layout
		auto* mainLayout = new QVBoxLayout(_parent);
		mainLayout->addWidget(_tableView);

		auto* buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();
		buttonLayout->addWidget(_closeButton);
		mainLayout->addLayout(buttonLayout);

		// Connect close button
		QObject::connect(_closeButton, &QPushButton::clicked, _parent, &QDialog::accept);
	}

	CompatibilityChangeEventsDialog* _parent;
	CompatibilityChangeEventsTableModel* _tableModel;
	QTableView* _tableView;
	QPushButton* _closeButton;
};

CompatibilityChangeEventsDialog::CompatibilityChangeEventsDialog(QString const& title, la::avdecc::controller::ControlledEntity::CompatibilityChangedEvents const& events, QWidget* parent)
	: QDialog{ parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint }
{
	_pImpl = new CompatibilityChangeEventsDialogImpl(title, events, this);
}

CompatibilityChangeEventsDialog::~CompatibilityChangeEventsDialog() noexcept
{
	delete _pImpl;
}
