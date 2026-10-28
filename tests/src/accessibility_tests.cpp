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

/**
* @file accessibility_tests.cpp
* @author Christophe Calmejane
* @brief Validates the accessibility information the Hive widgets expose through QAccessibleInterface (what the platform accessibility backends, and so the assistive technologies and UI automation tools, get).
* @details The assertions target the accessible interfaces, not the widget properties. The test process installs no translator: the
*          expected texts are retrieved with QCoreApplication::translate using the same contexts than the widgets, so the tests keep
*          passing whatever the translation in use, and never depend on the display language of the machine.
*/

#include "accessibilityAudit.hpp"

#include <gtest/gtest.h>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <hive/modelsLibrary/eventJournal.hpp>
#include <QtMate/widgets/flatIconButton.hpp>

#include "aboutDialog.hpp"
#include "avdecc/mcDomainManager.hpp"
#include "connectionMatrix/legendDialog.hpp"
#include "connectionMatrix/model.hpp"
#include "connectionMatrix/view.hpp"
#include "deviceDetailsDialog.hpp"
#include "diagnostics/compatibilityChangeEventsDialog.hpp"
#include "discoveredEntities/view.hpp"
#include "discoveredEntitiesView.hpp"
#include "entityInspector.hpp"
#include "eventJournal/eventJournalView.hpp"
#include "firmwareUploadDialog.hpp"
#include "loggerView.hpp"
#include "mappingMatrix.hpp"
#include "mediaClock/mediaClockManagementDialog.hpp"
#include "multiFirmwareUpdateDialog.hpp"
#include "networkGraph/networkGraphView.hpp"
#include "profiles/profileSelectionDialog.hpp"
#include "settingsDialog.hpp"
#include "settingsManager/settings.hpp"
#include "settingsManager/settingsManager.hpp"

#include <QAccessible>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTimeEdit>
#include <QDeadlineTimer>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QWidget>
#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 4127) // Disable conditional expression is constant
#endif
#include <QTest>
#ifdef _WIN32
#	pragma warning(pop)
#endif

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
// A simple network state: a talker and a listener (with a full entity model), one stream connected
constexpr auto NetworkStateFile = "data/connectionMatrix/1-Normal_Normal-ConnectedNoError_WrongFormat.json";
// A network state with the gPTP information required to compute a network topology
constexpr auto NetworkTopologyStateFile = "data/networkTopology/1-ZeroPropagationDelay_GrandmasterAdjacency.json";

QString tr(char const* const context, char const* const text)
{
	return QCoreApplication::translate(context, text, nullptr);
}

QAccessibleInterface* accessibleInterface(QObject* const object)
{
	return QAccessible::queryAccessibleInterface(object);
}

QString accessibleName(QObject* const object)
{
	auto* const iface = accessibleInterface(object);
	return iface ? iface->text(QAccessible::Name) : QString{};
}

QString accessibleDescription(QObject* const object)
{
	auto* const iface = accessibleInterface(object);
	return iface ? iface->text(QAccessible::Description) : QString{};
}

QStringList actionNames(QObject* const object)
{
	auto* const iface = accessibleInterface(object);
	auto* const actions = iface ? iface->actionInterface() : nullptr;
	return actions ? actions->actionNames() : QStringList{};
}

// Checks the accessible interface of a control has a label relation with a label displaying the given text
bool isLabeledBy(QObject* const control, QString const& labelText)
{
	auto* const iface = accessibleInterface(control);
	if (iface == nullptr)
	{
		return false;
	}
	auto const relations = iface->relations(QAccessible::Label);
	return std::any_of(relations.begin(), relations.end(),
		[&labelText](auto const& relation)
		{
			return relation.first != nullptr && relation.first->role() == QAccessible::StaticText && relation.first->text(QAccessible::Name) == labelText;
		});
}

// Finds a control the way a UI automation tool does: by the name its accessible interface exposes (the labels, whose name is their text, are skipped)
template<class WidgetType = QWidget>
WidgetType* findByAccessibleName(QWidget* const root, QString const& name)
{
	for (auto* const widget : root->findChildren<WidgetType*>())
	{
		if (!qobject_cast<QLabel*>(widget) && accessibleName(widget) == name)
		{
			return widget;
		}
	}
	return nullptr;
}

// Hides the widget it was created for when destroyed (declare it after the widget)
class ShownWidget final
{
public:
	explicit ShownWidget(QWidget& widget) noexcept
		: _widget{ widget }
	{
	}
	~ShownWidget() noexcept
	{
		// Some widgets can't be destroyed while visible (eg. LoggerView: its header view member is destroyed before the table view still using it), the application hides its windows before destroying them
		_widget.hide();
	}

	ShownWidget(ShownWidget const&) = delete;
	ShownWidget(ShownWidget&&) = delete;
	ShownWidget& operator=(ShownWidget const&) = delete;
	ShownWidget& operator=(ShownWidget&&) = delete;

private:
	QWidget& _widget;
};

// Shows a widget without putting it on screen (so the visibility states are realistic), it is hidden when the returned object is destroyed
[[nodiscard]] std::unique_ptr<ShownWidget> showOffscreen(QWidget& widget)
{
	widget.setAttribute(Qt::WA_DontShowOnScreen);
	widget.show();
	QTest::qWait(10); // Flush Qt EventLoop
	return std::make_unique<ShownWidget>(widget);
}

void expectNoAuditIssue(QWidget& widget, accessibilityAudit::Exceptions const& exceptions = {})
{
	auto const issues = accessibilityAudit::audit(&widget, exceptions);
	EXPECT_TRUE(issues.empty()) << "Accessibility audit of '" << widget.windowTitle().toStdString() << "' (" << widget.metaObject()->className() << ") found " << issues.size() << " issue(s):\n" << accessibilityAudit::toString(issues);
}

// Verifies the role, name and actions of a button found by its accessible name
void expectButton(QWidget* const root, QString const& name, QAccessible::Role const role, QString const& action)
{
	auto* const button = findByAccessibleName<QAbstractButton>(root, name);
	ASSERT_NE(nullptr, button) << "No button named '" << name.toStdString() << "'";
	auto* const iface = accessibleInterface(button);
	ASSERT_NE(nullptr, iface);
	EXPECT_TRUE(iface->isValid());
	EXPECT_EQ(role, iface->role()) << name.toStdString();
	// Qt lists no action for a disabled control
	if (button->isEnabled())
	{
		EXPECT_TRUE(actionNames(button).contains(action)) << name.toStdString() << " actions: " << actionNames(button).join(",").toStdString();
	}
	else
	{
		EXPECT_TRUE(iface->state().disabled) << name.toStdString();
	}
}

class Accessibility_F : public ::testing::Test
{
public:
	virtual void SetUp() override
	{
		// Files (eg. the journal sessions) must not pollute the user's directories
		QStandardPaths::setTestModeEnabled(true);
		ASSERT_TRUE(_settingsDir.isValid());

		// Settings stored in a temporary file (never the ones of the user), registered like the application does
		qApp->setProperty(settings::SettingsManager::PropertyName, QVariant::fromValue(_settings.get()));
		for (auto const& setting : { settings::LastLaunchedVersion, settings::General_AutomaticPNGDownloadEnabled, settings::General_AutomaticCheckForUpdates, settings::General_CheckForBetaVersions, settings::General_ThemeColorIndex, settings::ConnectionMatrix_Transpose, settings::ConnectionMatrix_ChannelMode, settings::ConnectionMatrix_AlwaysShowArrowTip, settings::ConnectionMatrix_AlwaysShowArrowEnd, settings::ConnectionMatrix_ShowMediaLockedDot, settings::ConnectionMatrix_AllowCRFAudioConnection, settings::ConnectionMatrix_CollapsedByDefault, settings::ConnectionMatrix_ShowEntitySummary, settings::NetworkGraph_AggregatedLayout, settings::EventJournal_DiagnosticLevel, settings::Network_ProtocolType, settings::Network_InterfaceTypeEthernet, settings::Network_InterfaceTypeWiFi,
					 settings::Controller_DiscoveryDelay, settings::Controller_AemCacheEnabled, settings::Controller_FastEnumerationEnabled, settings::Controller_FullStaticModelEnabled, settings::Controller_AdvertisingEnabled, settings::Controller_ControllerSubID })
		{
			_settings->registerSetting(setting);
		}

		// The journal session started when the controller comes online survives the controller reloads, and so does its recording level:
		// keep them as the journal tests expect them (diagnostic level), whatever the order the test suites run in (the journal view restores the level from the settings)
		hive::modelsLibrary::EventJournal::getInstance().setRecordingLevel(hive::modelsLibrary::EventJournal::Level::Diagnostic);
		_settings->setValue(settings::EventJournal_DiagnosticLevel.name, true);

		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		try
		{
			controllerManager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
		}
		catch (la::avdecc::controller::Controller::Exception const&)
		{
			ASSERT_FALSE(true) << "Failed to create the controller";
		}
	}

	virtual void TearDown() override
	{
		hive::modelsLibrary::ControllerManager::getInstance().destroyController();
		QTest::qWait(10); // Flush Qt EventLoop
		qApp->setProperty(settings::SettingsManager::PropertyName, QVariant{});
	}

	void loadNetworkState(char const* const filePath = NetworkStateFile)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
		auto const [err, msg] = controllerManager.loadVirtualEntitiesFromJsonNetworkState(filePath, flags);
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load NetworkState file";
		QTest::qWait(50); // Flush Qt EventLoop
	}

	// Returns the identifiers of the loaded entities having an entity model
	std::vector<la::avdecc::UniqueIdentifier> aemEntities() const
	{
		auto entities = std::vector<la::avdecc::UniqueIdentifier>{};
		hive::modelsLibrary::ControllerManager::getInstance().foreachEntity(
			[&entities](la::avdecc::UniqueIdentifier const& entityID, la::avdecc::controller::ControlledEntity const& entity)
			{
				if (entity.getEntity().getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported) && entity.hasAnyConfiguration())
				{
					entities.push_back(entityID);
				}
			});
		return entities;
	}

	settings::SettingsManager& settings() noexcept
	{
		return *_settings;
	}

private:
	int _argc{ 0 };
	QApplication _app{ _argc, nullptr };
	QTemporaryDir _settingsDir{};
	settings::SettingsManager::UniquePointer _settings{ settings::SettingsManager::create(_settingsDir.filePath("settings.ini")) };
};
} // namespace

/* ************************************************************ */
/* Audit helper                                                 */
/* ************************************************************ */
// The audit must detect the problems it is designed for, otherwise the audits of the Hive windows prove nothing
TEST_F(Accessibility_F, Audit_DetectsKnownProblems)
{
	auto window = QWidget{};
	window.setWindowTitle("Audit");
	auto* const layout = new QFormLayout{ &window };

	auto* const iconButton = new qtMate::widgets::FlatIconButton{ "Material Icons", "delete_forever", &window };
	layout->addRow(iconButton);

	auto* const placeholderOnlyEdit = new QLineEdit{ &window };
	placeholderOnlyEdit->setPlaceholderText("Filter");
	layout->addRow(placeholderOnlyEdit);

	auto* const unbuddiedCheckBox = new QCheckBox{ &window };
	layout->addRow(new QLabel{ "Enable Something", &window }, unbuddiedCheckBox);

	auto* const repeatingButton = new QPushButton{ "Save", &window };
	repeatingButton->setToolTip("Save");
	layout->addRow(repeatingButton);

	auto* const technicalButton = new QPushButton{ &window };
	technicalButton->setObjectName("applyButton");
	technicalButton->setAccessibleName("applyButton");
	layout->addRow(technicalButton);

	auto* const firstDuplicate = new QPushButton{ "Expand", &window };
	auto* const secondDuplicate = new QPushButton{ "Expand", &window };
	layout->addRow(firstDuplicate);
	layout->addRow(secondDuplicate);

	auto* const exceptedCheckBox = new QCheckBox{ &window };
	layout->addRow(exceptedCheckBox);

	auto const shown = showOffscreen(window);

	auto const issues = accessibilityAudit::audit(&window, { { exceptedCheckBox, accessibilityAudit::Check::MissingName, "Testing the exceptions" } });
	auto const hasIssue = [&issues](accessibilityAudit::Check const check, QString const& className)
	{
		return std::any_of(issues.begin(), issues.end(),
			[check, &className](auto const& issue)
			{
				return issue.check == check && issue.className == className;
			});
	};
	auto const countIssues = [&issues](accessibilityAudit::Check const check)
	{
		return std::count_if(issues.begin(), issues.end(),
			[check](auto const& issue)
			{
				return issue.check == check;
			});
	};

	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::IconFontGlyphName, "qtMate::widgets::FlatIconButton")) << accessibilityAudit::toString(issues);
	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::MissingName, "QLineEdit")) << accessibilityAudit::toString(issues);
	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::MissingName, "QCheckBox")) << accessibilityAudit::toString(issues);
	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::UnlabeledFormField, "QCheckBox")) << accessibilityAudit::toString(issues);
	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::DescriptionSameAsName, "QPushButton")) << accessibilityAudit::toString(issues);
	EXPECT_TRUE(hasIssue(accessibilityAudit::Check::ObjectNameAsName, "QPushButton")) << accessibilityAudit::toString(issues);
	EXPECT_EQ(2, countIssues(accessibilityAudit::Check::DuplicateName)) << accessibilityAudit::toString(issues);
	// Only the unbuddied check box lacks a name, the excepted one is not reported
	EXPECT_EQ(2, countIssues(accessibilityAudit::Check::MissingName)) << accessibilityAudit::toString(issues);
}

/* ************************************************************ */
/* Logger                                                       */
/* ************************************************************ */
TEST_F(Accessibility_F, LoggerView)
{
	auto view = LoggerView{};
	view.setWindowTitle("Logger");
	auto const shown = showOffscreen(view);

	// Filter field: named (the placeholder alone only provides a description), described, editable text
	auto* const filter = findByAccessibleName<QLineEdit>(&view, tr("LoggerView", "Log Message Filter"));
	ASSERT_NE(nullptr, filter);
	auto* const filterIface = accessibleInterface(filter);
	EXPECT_EQ(QAccessible::EditableText, filterIface->role());
	EXPECT_EQ(tr("LoggerView", "Regular expression matched against the log messages"), filterIface->text(QAccessible::Description));
	EXPECT_NE(nullptr, filterIface->textInterface());
	EXPECT_NE(nullptr, filterIface->editableTextInterface());
	EXPECT_TRUE(filterIface->state().editable);

	// Filter menus and icon only buttons
	expectButton(&view, tr("LoggerView", "Layer Filter"), QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, tr("LoggerView", "Level Filter"), QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, tr("LoggerView", "Clear Log"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	expectButton(&view, tr("LoggerView", "Save Log"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	// The tooltip is the description
	EXPECT_EQ(QString{ "Clear" }, accessibleDescription(findByAccessibleName(&view, tr("LoggerView", "Clear Log"))));

	// Messages table
	auto* const table = findByAccessibleName<QAbstractItemView>(&view, tr("LoggerView", "Log Messages"));
	ASSERT_NE(nullptr, table);
	EXPECT_EQ(QAccessible::Table, accessibleInterface(table)->role());
	EXPECT_NE(nullptr, accessibleInterface(table)->tableInterface());
	EXPECT_NE(nullptr, accessibleInterface(table)->selectionInterface());

	expectNoAuditIssue(view);
}

/* ************************************************************ */
/* Discovered Entities                                          */
/* ************************************************************ */
TEST_F(Accessibility_F, DiscoveredEntitiesView)
{
	auto view = DiscoveredEntitiesView{};
	view.setWindowTitle("Discovered Entities");
	view.setupView(hive::VisibilityDefaults{}, true);
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}
	auto const shown = showOffscreen(view);

	auto* const filter = findByAccessibleName<QLineEdit>(&view, tr("DiscoveredEntitiesView", "Entity Name Filter"));
	ASSERT_NE(nullptr, filter);
	EXPECT_EQ(QAccessible::EditableText, accessibleInterface(filter)->role());
	// The placeholder is exposed as the description
	EXPECT_EQ(tr("DiscoveredEntitiesView", "Entity Name Filter (RegEx)"), accessibleDescription(filter));

	expectButton(&view, tr("DiscoveredEntitiesView", "Clear Error Counters"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	EXPECT_EQ(tr("DiscoveredEntitiesView", "Clears the error counters of all the discovered entities"), accessibleDescription(findByAccessibleName(&view, tr("DiscoveredEntitiesView", "Clear Error Counters"))));
	expectButton(&view, "Link with Matrix Filter", QAccessible::CheckBox, QAccessibleActionInterface::toggleAction());

	// Entities table: the icon only columns expose a textual equivalent
	auto* const table = view.entitiesTableView();
	EXPECT_EQ(tr("DiscoveredEntitiesView", "Discovered Entities"), accessibleName(table));
	auto* const tableIface = accessibleInterface(table);
	ASSERT_NE(nullptr, tableIface);
	EXPECT_EQ(QAccessible::Table, tableIface->role());
	auto* const tableInterface = tableIface->tableInterface();
	ASSERT_NE(nullptr, tableInterface);
	ASSERT_GT(tableInterface->rowCount(), 0);

	auto const statusTexts = QStringList{ tr("DiscoveredEntitiesTableModel", "Error"), tr("DiscoveredEntitiesTableModel", "Warning"), tr("DiscoveredEntitiesTableModel", "Information"), tr("DiscoveredEntitiesTableModel", "No Issue") };
	for (auto row = 0; row < tableInterface->rowCount(); ++row)
	{
		// Status: the error level, not the sorting value the model displays
		auto* const statusCell = tableInterface->cellAt(row, static_cast<int>(discoveredEntities::View::ControllerModelEntityColumn_EntityStatus));
		ASSERT_NE(nullptr, statusCell);
		EXPECT_EQ(QAccessible::Cell, statusCell->role());
		EXPECT_TRUE(statusTexts.contains(statusCell->text(QAccessible::Name))) << "Status cell name: " << statusCell->text(QAccessible::Name).toStdString();

		// Compatibility, acquire and lock states: the same text than their tooltip
		for (auto const column : { discoveredEntities::View::ControllerModelEntityColumn_Compatibility, discoveredEntities::View::ControllerModelEntityColumn_AcquireState, discoveredEntities::View::ControllerModelEntityColumn_LockState })
		{
			auto* const cell = tableInterface->cellAt(row, static_cast<int>(column));
			ASSERT_NE(nullptr, cell);
			auto const tooltip = table->model()->index(row, static_cast<int>(column)).data(Qt::ToolTipRole).toString();
			EXPECT_FALSE(cell->text(QAccessible::Name).isEmpty()) << "Column " << column;
			EXPECT_EQ(tooltip, cell->text(QAccessible::Name)) << "Column " << column;
		}

		// Textual columns are unchanged
		auto* const idCell = tableInterface->cellAt(row, static_cast<int>(discoveredEntities::View::ControllerModelEntityColumn_EntityID));
		ASSERT_NE(nullptr, idCell);
		EXPECT_EQ(table->model()->index(row, static_cast<int>(discoveredEntities::View::ControllerModelEntityColumn_EntityID)).data().toString(), idCell->text(QAccessible::Name));
	}

	expectNoAuditIssue(view);
}

/* ************************************************************ */
/* Connection Matrix                                            */
/* ************************************************************ */
TEST_F(Accessibility_F, ConnectionMatrix_ControlsAndTransposition)
{
	auto view = connectionMatrix::View{};
	view.setWindowTitle("Connection Matrix");
	auto const shown = showOffscreen(view);

	EXPECT_EQ(tr("connectionMatrix::View", "Connection Matrix"), accessibleName(&view));
	EXPECT_EQ(QAccessible::Table, accessibleInterface(&view)->role());

	auto* const filter = findByAccessibleName<QLineEdit>(&view, tr("CornerWidget", "Entity Name Filter"));
	ASSERT_NE(nullptr, filter);
	EXPECT_EQ(QAccessible::EditableText, accessibleInterface(filter)->role());

	expectButton(&view, "Color Code Help", QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	expectButton(&view, tr("CornerWidget", "Remove All Connections"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	EXPECT_EQ(tr("CornerWidget", "Remove all active connections"), accessibleDescription(findByAccessibleName(&view, tr("CornerWidget", "Remove All Connections"))));

	// Talkers are rows by default: the buttons below the corner act on the talkers, the ones on its right on the listeners
	auto* const expandTalkers = findByAccessibleName<QAbstractButton>(&view, tr("CornerWidget", "Expand All Talkers"));
	auto* const collapseTalkers = findByAccessibleName<QAbstractButton>(&view, tr("CornerWidget", "Collapse All Talkers"));
	auto* const expandListeners = findByAccessibleName<QAbstractButton>(&view, tr("CornerWidget", "Expand All Listeners"));
	auto* const collapseListeners = findByAccessibleName<QAbstractButton>(&view, tr("CornerWidget", "Collapse All Listeners"));
	ASSERT_NE(nullptr, expandTalkers);
	ASSERT_NE(nullptr, collapseTalkers);
	ASSERT_NE(nullptr, expandListeners);
	ASSERT_NE(nullptr, collapseListeners);
	for (auto* const button : { expandTalkers, collapseTalkers, expandListeners, collapseListeners })
	{
		EXPECT_EQ(QAccessible::PushButton, accessibleInterface(button)->role());
		EXPECT_TRUE(actionNames(button).contains(QAccessibleActionInterface::pressAction()));
	}

	// Transposing swaps the headers: the same buttons now act on the other kind of entities, their names follow
	settings().setValue(settings::ConnectionMatrix_Transpose.name, true);
	QTest::qWait(10);
	EXPECT_EQ(tr("CornerWidget", "Expand All Listeners"), accessibleName(expandTalkers));
	EXPECT_EQ(tr("CornerWidget", "Collapse All Listeners"), accessibleName(collapseTalkers));
	EXPECT_EQ(tr("CornerWidget", "Expand All Talkers"), accessibleName(expandListeners));
	EXPECT_EQ(tr("CornerWidget", "Collapse All Talkers"), accessibleName(collapseListeners));

	settings().setValue(settings::ConnectionMatrix_Transpose.name, false);
	QTest::qWait(10);
	EXPECT_EQ(tr("CornerWidget", "Expand All Talkers"), accessibleName(expandTalkers));

	// Qt's table interface only exposes the cells and headers of an item view, not its child widgets (here the corner widget)
	expectNoAuditIssue(view, { { filter->parentWidget()->parentWidget(), accessibilityAudit::Check::NotReachable, "Child widgets of an item view are not exposed by Qt's accessible table interface" } });
}

TEST_F(Accessibility_F, ConnectionMatrix_Intersections)
{
	auto view = connectionMatrix::View{};
	view.setWindowTitle("Connection Matrix");
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}
	auto const shown = showOffscreen(view);

	auto* const model = static_cast<connectionMatrix::Model*>(view.model());
	auto* const tableInterface = accessibleInterface(&view)->tableInterface();
	ASSERT_NE(nullptr, tableInterface);

	auto const cellOf = [model, tableInterface](int const talkerSection, int const listenerSection) -> QAccessibleInterface*
	{
		auto const index = model->getIntersectionIndex(talkerSection, listenerSection);
		return tableInterface->cellAt(index.row(), index.column());
	};

	// Same intersections than the ConnectionMatrix_F.EntitySingleStream_NormalNormal tests
	// Talker - Stream0: connected and media locked
	{
		auto* const cell = cellOf(5, 1);
		ASSERT_NE(nullptr, cell);
		EXPECT_EQ(QAccessible::Cell, cell->role());
		EXPECT_EQ(tr("connectionMatrix::Model", "Connected"), cell->text(QAccessible::Name));
		EXPECT_TRUE(cell->text(QAccessible::Description).contains(tr("connectionMatrix::Model", "Media Locked"))) << cell->text(QAccessible::Description).toStdString();

		// The talker and listener are exposed by the header cells of the intersection
		auto* const cellInterface = cell->tableCellInterface();
		ASSERT_NE(nullptr, cellInterface);
		auto const rowHeaders = cellInterface->rowHeaderCells();
		auto const columnHeaders = cellInterface->columnHeaderCells();
		ASSERT_FALSE(rowHeaders.isEmpty());
		ASSERT_FALSE(columnHeaders.isEmpty());
		auto const index = model->getIntersectionIndex(5, 1);
		EXPECT_EQ(model->headerData(index.row(), Qt::Vertical).toString(), rowHeaders.first()->text(QAccessible::Name));
		EXPECT_EQ(model->headerData(index.column(), Qt::Horizontal).toString(), columnHeaders.first()->text(QAccessible::Name));
		EXPECT_FALSE(rowHeaders.first()->text(QAccessible::Name).isEmpty());
	}
	// Talker - Stream1: not connected, no error
	{
		auto* const cell = cellOf(5, 2);
		ASSERT_NE(nullptr, cell);
		EXPECT_EQ(tr("connectionMatrix::Model", "Not Connected"), cell->text(QAccessible::Name));
		EXPECT_TRUE(cell->text(QAccessible::Description).isEmpty()) << cell->text(QAccessible::Description).toStdString();
	}

	// Every intersection has a meaningful name (the cells are only painted)
	auto const stateTexts = QStringList{ tr("connectionMatrix::Model", "Connected"), tr("connectionMatrix::Model", "Not Connected"), tr("connectionMatrix::Model", "Fast Connecting"), tr("connectionMatrix::Model", "Partially Connected"), tr("connectionMatrix::Model", "Not Connectable") };
	for (auto row = 0; row < tableInterface->rowCount(); ++row)
	{
		for (auto column = 0; column < tableInterface->columnCount(); ++column)
		{
			auto* const cell = tableInterface->cellAt(row, column);
			ASSERT_NE(nullptr, cell);
			EXPECT_TRUE(stateTexts.contains(cell->text(QAccessible::Name))) << "Cell [" << row << "," << column << "]: '" << cell->text(QAccessible::Name).toStdString() << "'";
		}
	}
}

/* ************************************************************ */
/* Network Graph                                                */
/* ************************************************************ */
TEST_F(Accessibility_F, NetworkGraphView)
{
	auto view = NetworkGraphView{};
	view.setWindowTitle("Network Graph");
	loadNetworkState(NetworkTopologyStateFile);
	if (HasFatalFailure())
	{
		return;
	}
	auto const shown = showOffscreen(view);

	// The topology is computed on a throttled timer, the graph of a network is created with its tab
	auto const graphName = tr("NetworkGraphPane", "Network Topology Graph");
	auto const deadline = QDeadlineTimer{ 10000 };
	while (!deadline.hasExpired() && findByAccessibleName(&view, graphName) == nullptr)
	{
		QTest::qWait(50);
	}

	expectButton(&view, tr("NetworkGraphView", "Re-layout"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	expectButton(&view, tr("NetworkGraphView", "Zoom to Fit"), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	EXPECT_EQ(tr("NetworkGraphView", "Zooms and centers the graph so it is entirely visible"), accessibleDescription(findByAccessibleName(&view, tr("NetworkGraphView", "Zoom to Fit"))));

	// Toggle buttons: a stable name, the checked state exposes what the changing icon shows
	for (auto const* const name : { "Lock Layout", "Clear Stream Highlight", "Detailed Layout", "Aggregated Layout", "Show Link Information", "Propagation Delay in Meters" })
	{
		expectButton(&view, tr("NetworkGraphView", name), QAccessible::CheckBox, QAccessibleActionInterface::toggleAction());
		EXPECT_TRUE(accessibleInterface(findByAccessibleName(&view, tr("NetworkGraphView", name)))->state().checkable) << name;
	}
	auto* const lockButton = findByAccessibleName<QAbstractButton>(&view, tr("NetworkGraphView", "Lock Layout"));
	ASSERT_NE(nullptr, lockButton);
	EXPECT_TRUE(accessibleInterface(lockButton)->state().checked); // Locked by default
	lockButton->click();
	EXPECT_FALSE(accessibleInterface(lockButton)->state().checked);
	EXPECT_EQ(tr("NetworkGraphView", "Lock Layout"), accessibleName(lockButton)); // The glyph changed, not the name
	EXPECT_EQ(QString{ "lock_open" }, lockButton->text());

	// The layout buttons are exclusive
	auto* const detailedButton = findByAccessibleName<QAbstractButton>(&view, tr("NetworkGraphView", "Detailed Layout"));
	auto* const aggregatedButton = findByAccessibleName<QAbstractButton>(&view, tr("NetworkGraphView", "Aggregated Layout"));
	ASSERT_NE(nullptr, detailedButton);
	ASSERT_NE(nullptr, aggregatedButton);
	EXPECT_NE(accessibleInterface(detailedButton)->state().checked, accessibleInterface(aggregatedButton)->state().checked);

	// The propagation delay unit only matters while the link information is shown
	auto* const linkInfoButton = findByAccessibleName<QAbstractButton>(&view, tr("NetworkGraphView", "Show Link Information"));
	auto* const delayButton = findByAccessibleName<QAbstractButton>(&view, tr("NetworkGraphView", "Propagation Delay in Meters"));
	ASSERT_NE(nullptr, linkInfoButton);
	ASSERT_NE(nullptr, delayButton);
	EXPECT_FALSE(accessibleInterface(delayButton)->state().disabled);
	linkInfoButton->click();
	EXPECT_TRUE(accessibleInterface(delayButton)->state().disabled);

	// One graph per network, named, in a tab named after the network
	auto* const graph = findByAccessibleName(&view, graphName);
	ASSERT_NE(nullptr, graph) << "No network graph created from the network state";
	EXPECT_FALSE(accessibleDescription(graph).isEmpty());
	auto* const tabBar = view.findChild<QTabBar*>();
	ASSERT_NE(nullptr, tabBar);
	auto* const tabBarIface = accessibleInterface(tabBar);
	EXPECT_EQ(QAccessible::PageTabList, tabBarIface->role());
	ASSERT_GT(tabBarIface->childCount(), 0);
	EXPECT_EQ(QAccessible::PageTab, tabBarIface->child(0)->role());
	EXPECT_EQ(tabBar->tabText(0), tabBarIface->child(0)->text(QAccessible::Name));

	expectNoAuditIssue(view);
}

/* ************************************************************ */
/* Event Journal                                                */
/* ************************************************************ */
TEST_F(Accessibility_F, EventJournalView)
{
	auto view = EventJournalView{};
	view.setWindowTitle("Event Journal");
	auto const shown = showOffscreen(view);

	auto* const search = findByAccessibleName<QLineEdit>(&view, tr("EventJournalView", "Search Events"));
	ASSERT_NE(nullptr, search);
	EXPECT_EQ(QString{ "Search (regular expression)" }, accessibleDescription(search));

	expectButton(&view, tr("EventJournalView", "Severity Filter"), QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, tr("EventJournalView", "Category Filter"), QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, tr("EventJournalView", "Entity Filter"), QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, "Export...", QAccessible::ButtonMenu, QAccessibleActionInterface::showMenuAction());
	expectButton(&view, "Show Diagnostic", QAccessible::CheckBox, QAccessibleActionInterface::toggleAction());
	expectButton(&view, "From:", QAccessible::CheckBox, QAccessibleActionInterface::toggleAction());

	// Time range: value controls, disabled until their check box is checked
	auto* const fromTime = findByAccessibleName<QDateTimeEdit>(&view, tr("EventJournalView", "From Time"));
	auto* const toTime = findByAccessibleName<QDateTimeEdit>(&view, tr("EventJournalView", "To Time"));
	ASSERT_NE(nullptr, fromTime);
	ASSERT_NE(nullptr, toTime);
	EXPECT_EQ(QAccessible::SpinBox, accessibleInterface(fromTime)->role());
	EXPECT_NE(nullptr, accessibleInterface(fromTime)->valueInterface());
	EXPECT_TRUE(accessibleInterface(fromTime)->state().disabled);
	findByAccessibleName<QAbstractButton>(&view, "From:")->click();
	EXPECT_FALSE(accessibleInterface(fromTime)->state().disabled);

	auto* const table = findByAccessibleName<QAbstractItemView>(&view, tr("EventJournalView", "Events"));
	ASSERT_NE(nullptr, table);
	EXPECT_EQ(QAccessible::Table, accessibleInterface(table)->role());

	auto* const details = findByAccessibleName<QPlainTextEdit>(&view, tr("EventJournalView", "Event Details"));
	ASSERT_NE(nullptr, details);
	EXPECT_NE(nullptr, accessibleInterface(details)->textInterface());
	EXPECT_TRUE(accessibleInterface(details)->state().readOnly);

	// The timeline is a painted visualization: named and described, pointing to the table
	auto* const timeline = findByAccessibleName(&view, tr("EventJournalView", "Event Timeline"));
	ASSERT_NE(nullptr, timeline);
	EXPECT_EQ(tr("EventJournalView", "Graphical view of the filtered events over time, the same events are listed in the Events table"), accessibleDescription(timeline));

	expectNoAuditIssue(view);
}

/* ************************************************************ */
/* Entity Inspector                                             */
/* ************************************************************ */
TEST_F(Accessibility_F, EntityInspector)
{
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}
	auto const entities = aemEntities();
	ASSERT_FALSE(entities.empty());

	auto inspector = EntityInspector{};
	inspector.setControlledEntityID(entities.front());
	auto const shown = showOffscreen(inspector);

	auto* const entityModelTree = findByAccessibleName<QTreeWidget>(&inspector, tr("EntityInspector", "Entity Model"));
	auto* const propertiesTree = findByAccessibleName<QTreeWidget>(&inspector, tr("EntityInspector", "Descriptor Properties"));
	ASSERT_NE(nullptr, entityModelTree);
	ASSERT_NE(nullptr, propertiesTree);
	EXPECT_EQ(QAccessible::Tree, accessibleInterface(entityModelTree)->role());
	EXPECT_EQ(QAccessible::Tree, accessibleInterface(propertiesTree)->role());

	// The blank header sections of the properties are named for assistive technologies
	auto* const propertiesTable = accessibleInterface(propertiesTree)->tableInterface();
	ASSERT_NE(nullptr, propertiesTable);
	ASSERT_EQ(2, propertiesTable->columnCount());
	auto* const headerCell = accessibleInterface(propertiesTree)->child(0);
	ASSERT_NE(nullptr, headerCell);
	EXPECT_EQ(QAccessible::ColumnHeader, headerCell->role());
	EXPECT_EQ(tr("EntityInspector", "Property"), headerCell->text(QAccessible::Name));
	EXPECT_EQ(tr("EntityInspector", "Value"), accessibleInterface(propertiesTree)->child(1)->text(QAccessible::Name));

	// Display the entity descriptor: its editable properties are widgets named after their label
	ASSERT_GT(entityModelTree->topLevelItemCount(), 0);
	entityModelTree->setCurrentItem(entityModelTree->topLevelItem(0));
	QTest::qWait(10);
	auto* const entityNameEntry = findByAccessibleName<QLineEdit>(propertiesTree, "Entity Name");
	ASSERT_NE(nullptr, entityNameEntry) << "No editable entity name";
	EXPECT_EQ(QAccessible::EditableText, accessibleInterface(entityNameEntry)->role());
	EXPECT_NE(nullptr, accessibleInterface(entityNameEntry)->editableTextInterface());
	auto* const configurationComboBox = findByAccessibleName<QComboBox>(propertiesTree, "Current Configuration");
	ASSERT_NE(nullptr, configurationComboBox) << "No current configuration combo box";
	EXPECT_EQ(QAccessible::ComboBox, accessibleInterface(configurationComboBox)->role());

	// Qt's tree interface only exposes the cells of the tree, not the widgets displayed in them
	expectNoAuditIssue(inspector, { { propertiesTree, accessibilityAudit::Check::NotReachable, "Widgets displayed in the cells of an item view are not exposed by Qt's accessible tree interface" } });
}

/* ************************************************************ */
/* Dialogs                                                      */
/* ************************************************************ */
TEST_F(Accessibility_F, SettingsDialog)
{
	auto dialog = SettingsDialog{};
	auto const shown = showOffscreen(dialog);

	// Every check box, field and combo box of the forms is named after its visible label, which is related to it
	auto const expectLabeled = [&dialog](QString const& labelText)
	{
		auto* const field = findByAccessibleName(&dialog, labelText);
		ASSERT_NE(nullptr, field) << "No control named '" << labelText.toStdString() << "'";
		// Labeled by its label (and by its group box)
		EXPECT_TRUE(isLabeledBy(field, labelText)) << labelText.toStdString();
	};
	for (auto const* const label : { "Automatic Entity Logo Download", "Automatic Check For Updates", "Check For Beta Versions", "Accent Color", "Show Talkers On Top", "Always Show Arrow Tip", "Always Show Arrow End", "Show Media Locked State (Dot) In Intersection", "Allow CRF-Audio Connection", "Collapsed by Default", "Show Entity Summary", "Enable AEM Cache", "Enable Advertising", "Controller Sub ID (if possible)", "Full AEM Enumeration", "Discovery Delay (in sec, 0=manual)", "Enable Fast Enumeration", "Protocol" })
	{
		expectLabeled(tr("SettingsDialog", label));
	}
	EXPECT_EQ(QAccessible::CheckBox, accessibleInterface(findByAccessibleName(&dialog, tr("SettingsDialog", "Enable AEM Cache")))->role());
	EXPECT_EQ(QAccessible::EditableText, accessibleInterface(findByAccessibleName(&dialog, tr("SettingsDialog", "Discovery Delay (in sec, 0=manual)")))->role());
	EXPECT_EQ(QAccessible::ComboBox, accessibleInterface(findByAccessibleName(&dialog, tr("SettingsDialog", "Protocol")))->role());
	auto* const interfaceTypes = findByAccessibleName<QAbstractItemView>(&dialog, tr("SettingsDialog", "Interface Types"));
	ASSERT_NE(nullptr, interfaceTypes);
	EXPECT_EQ(QAccessible::List, accessibleInterface(interfaceTypes)->role());

	expectNoAuditIssue(dialog);
}

TEST_F(Accessibility_F, DeviceDetailsDialog)
{
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}
	auto const entities = aemEntities();
	ASSERT_FALSE(entities.empty());

	auto dialog = DeviceDetailsDialog{ entities.front() };
	auto const shown = showOffscreen(dialog);

	for (auto const* const label : { "Device Name", "Group Name" })
	{
		auto* const field = findByAccessibleName<QLineEdit>(&dialog, tr("DeviceDetailsDialog", label));
		ASSERT_NE(nullptr, field) << label;
		EXPECT_TRUE(isLabeledBy(field, tr("DeviceDetailsDialog", label))) << label;
	}
	EXPECT_NE(nullptr, findByAccessibleName<QComboBox>(&dialog, tr("DeviceDetailsDialog", "Configuration")));

	// Tabs
	auto* const tabWidget = dialog.findChild<QTabWidget*>("tabWidget");
	ASSERT_NE(nullptr, tabWidget);
	auto* const tabBar = accessibleInterface(tabWidget->tabBar());
	ASSERT_NE(nullptr, tabBar);
	EXPECT_EQ(QAccessible::PageTabList, tabBar->role());
	auto tabNames = QStringList{};
	for (auto i = 0; i < tabBar->childCount(); ++i)
	{
		if (tabBar->child(i)->role() == QAccessible::PageTab)
		{
			tabNames << tabBar->child(i)->text(QAccessible::Name);
		}
	}
	EXPECT_TRUE(tabNames.contains(tr("DeviceDetailsDialog", "General"))) << tabNames.join(",").toStdString();
	EXPECT_TRUE(tabNames.contains(tr("DeviceDetailsDialog", "Stream Format"))) << tabNames.join(",").toStdString();

	// Every tab content is audited
	for (auto tab = 0; tab < tabWidget->count(); ++tab)
	{
		tabWidget->setCurrentIndex(tab);
		QTest::qWait(10);
		// The delegates of the format and latency columns display persistent editors inside the cells
		expectNoAuditIssue(dialog, { { tabWidget, accessibilityAudit::Check::NotReachable, "Persistent editors displayed in the cells of an item view are not exposed by Qt's accessible table interface" } });
	}
	for (auto const* const name : { "Stream Latency", "Receive Channels", "Transmit Channels", "Input Stream Formats", "Output Stream Formats" })
	{
		auto* const table = findByAccessibleName<QAbstractItemView>(&dialog, tr("DeviceDetailsDialog", name));
		ASSERT_NE(nullptr, table) << name;
		EXPECT_EQ(QAccessible::Table, accessibleInterface(table)->role()) << name;
	}
	EXPECT_NE(nullptr, findByAccessibleName<QLineEdit>(&dialog, tr("DeviceDetailsDialog", "Custom Presentation Time")));
	EXPECT_NE(nullptr, findByAccessibleName<QComboBox>(&dialog, tr("DeviceDetailsDialog", "Predefined Presentation Time")));

	// The persistent editors displayed in the cells are named after the stream of their row
	auto const expectEditorsNamedAfterTheirStream = [&dialog](char const* const tableName, char const* const context, char const* const nameFormat)
	{
		auto* const table = findByAccessibleName<QAbstractItemView>(&dialog, tr("DeviceDetailsDialog", tableName));
		ASSERT_NE(nullptr, table) << tableName;
		auto const editors = table->findChildren<QComboBox*>();
		EXPECT_EQ(table->model()->rowCount(), editors.size()) << tableName;
		for (auto row = 0; row < table->model()->rowCount(); ++row)
		{
			auto const expectedName = tr(context, nameFormat).arg(table->model()->index(row, 0).data().toString());
			EXPECT_NE(nullptr, findByAccessibleName<QComboBox>(table, expectedName)) << tableName << ": no editor named '" << expectedName.toStdString() << "'";
		}
	};
	expectEditorsNamedAfterTheirStream("Input Stream Formats", "StreamFormatItemDelegate", "%1 Format");
	expectEditorsNamedAfterTheirStream("Output Stream Formats", "StreamFormatItemDelegate", "%1 Format");
	expectEditorsNamedAfterTheirStream("Stream Latency", "LatencyItemDelegate", "%1 Latency");
}

TEST_F(Accessibility_F, MediaClockManagementDialog)
{
	// Like the application does, create the domains manager before the entities come online
	avdecc::mediaClock::MCDomainManager::getInstance();
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}

	auto dialog = MediaClockManagementDialog{};
	auto const shown = showOffscreen(dialog);

	for (auto const* const name : { "Assign to Domain", "Unassign from Domain", "Add Domain", "Remove Domain", "Clear Domains" })
	{
		expectButton(&dialog, tr("MediaClockManagementDialog", name), QAccessible::PushButton, QAccessibleActionInterface::pressAction());
	}
	// Nothing is selected: only adding a domain and clearing them are possible
	EXPECT_TRUE(accessibleInterface(findByAccessibleName(&dialog, tr("MediaClockManagementDialog", "Assign to Domain")))->state().disabled);
	EXPECT_TRUE(accessibleInterface(findByAccessibleName(&dialog, tr("MediaClockManagementDialog", "Remove Domain")))->state().disabled);
	EXPECT_FALSE(accessibleInterface(findByAccessibleName(&dialog, tr("MediaClockManagementDialog", "Add Domain")))->state().disabled);
	EXPECT_EQ(tr("MediaClockManagementDialog", "Removes all the domains, their devices become unassigned"), accessibleDescription(findByAccessibleName(&dialog, tr("MediaClockManagementDialog", "Clear Domains"))));

	auto* const domains = findByAccessibleName<QAbstractItemView>(&dialog, tr("MediaClockManagementDialog", "Media Clock Domains"));
	ASSERT_NE(nullptr, domains);
	EXPECT_EQ(QAccessible::Tree, accessibleInterface(domains)->role());
	auto* const unassigned = findByAccessibleName<QAbstractItemView>(&dialog, tr("MediaClockManagementDialog", "Unassigned Devices"));
	ASSERT_NE(nullptr, unassigned);
	EXPECT_EQ(QAccessible::List, accessibleInterface(unassigned)->role());

	// The domains tree is painted by delegates from an internal pointer: its cells expose a textual equivalent
	auto* const model = domains->model();
	ASSERT_GT(model->rowCount() + unassigned->model()->rowCount(), 0);
	for (auto row = 0; row < model->rowCount(); ++row)
	{
		auto const domainIndex = model->index(row, 0);
		EXPECT_FALSE(domainIndex.data(Qt::AccessibleTextRole).toString().isEmpty()) << "Domain row " << row;
		for (auto entityRow = 0; entityRow < model->rowCount(domainIndex); ++entityRow)
		{
			EXPECT_FALSE(model->index(entityRow, 0, domainIndex).data(Qt::AccessibleTextRole).toString().isEmpty()) << "Entity row " << entityRow;
			auto const master = model->index(entityRow, 1, domainIndex).data(Qt::AccessibleTextRole).toString();
			EXPECT_TRUE(master == tr("DomainTreeModel", "Master") || master == tr("DomainTreeModel", "Not Master")) << master.toStdString();
		}
	}

	// The sampling rate of a domain is edited with a persistent editor displayed inside the cell
	expectNoAuditIssue(dialog, { { domains, accessibilityAudit::Check::NotReachable, "Persistent editors displayed in the cells of an item view are not exposed by Qt's accessible tree interface" } });
}

TEST_F(Accessibility_F, ProfileSelectionDialog)
{
	auto dialog = profiles::ProfileSelectionDialog{};
	auto const shown = showOffscreen(dialog);

	// Each profile card is a button named after the profile, described by its description, without child labels (the icon one displays a glyph name)
	auto* const advanced = findByAccessibleName(&dialog, "Advanced");
	ASSERT_NE(nullptr, advanced);
	auto* const iface = accessibleInterface(advanced);
	EXPECT_EQ(QAccessible::PushButton, iface->role());
	EXPECT_EQ(QString{ "Intended for advanced users and developers." }, iface->text(QAccessible::Description));
	EXPECT_EQ(0, iface->childCount());
	EXPECT_TRUE(actionNames(advanced).contains(QAccessibleActionInterface::pressAction()));
	EXPECT_NE(nullptr, findByAccessibleName(&dialog, "Standard (Default)"));
	EXPECT_NE(nullptr, findByAccessibleName(&dialog, "Matrix"));

	// The window lists the cards as its children
	auto* const dialogIface = accessibleInterface(&dialog);
	EXPECT_EQ(QAccessible::Dialog, dialogIface->role());
	EXPECT_EQ(QString{ "Please Choose The Default User Profile" }, dialogIface->text(QAccessible::Name));
	EXPECT_GE(dialogIface->indexOfChild(iface), 0);

	expectNoAuditIssue(dialog);

	// Pressing a card through the accessibility interface selects its profile
	iface->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
	EXPECT_EQ(QDialog::Accepted, dialog.result());
	EXPECT_EQ(profiles::ProfileType::Developer, dialog.selectedProfile());
}

TEST_F(Accessibility_F, OtherDialogs)
{
	loadNetworkState();
	if (HasFatalFailure())
	{
		return;
	}
	auto const entities = aemEntities();
	ASSERT_FALSE(entities.empty());

	{
		auto dialog = AboutDialog{};
		auto const shown = showOffscreen(dialog);
		expectButton(&dialog, "Legal Notices", QAccessible::PushButton, QAccessibleActionInterface::pressAction());
		expectNoAuditIssue(dialog);
	}
	{
		auto dialog = connectionMatrix::LegendDialog{ qtMate::material::color::DefaultColor, false };
		auto const shown = showOffscreen(dialog);
		expectNoAuditIssue(dialog);
	}
	{
		auto dialog = MultiFirmwareUpdateDialog{};
		auto const shown = showOffscreen(dialog);
		auto* const devices = findByAccessibleName<QAbstractItemView>(&dialog, tr("MultiFirmwareUpdateDialog", "Devices"));
		ASSERT_NE(nullptr, devices);
		EXPECT_EQ(QAccessible::Table, accessibleInterface(devices)->role());
		expectNoAuditIssue(dialog);
	}
	{
		auto dialog = FirmwareUploadDialog{ {}, "firmware.bin", { { entities.front(), la::avdecc::entity::model::DescriptorIndex{ 0u }, std::uint64_t{ 0u } } } };
		auto const shown = showOffscreen(dialog);
		EXPECT_NE(nullptr, findByAccessibleName<QAbstractItemView>(&dialog, tr("FirmwareUploadDialog", "Firmware Updates")));
		// The progress of each device is identified by the device name and state its label displays
		auto const progressBars = dialog.findChildren<QProgressBar*>();
		ASSERT_EQ(1, progressBars.size());
		auto* const progressIface = accessibleInterface(progressBars.first());
		EXPECT_EQ(QAccessible::ProgressBar, progressIface->role());
		EXPECT_TRUE(progressIface->text(QAccessible::Name).endsWith(": Waiting to start")) << progressIface->text(QAccessible::Name).toStdString();
		EXPECT_NE(nullptr, progressIface->valueInterface());
		expectNoAuditIssue(dialog, { { dialog.findChild<QAbstractItemView*>(), accessibilityAudit::Check::NotReachable, "Widgets displayed in the items of an item view are not exposed by Qt's accessible list interface" } });
	}
	{
		auto dialog = CompatibilityChangeEventsDialog{ "Compatibility Change Events", {} };
		auto const shown = showOffscreen(dialog);
		EXPECT_NE(nullptr, findByAccessibleName<QAbstractItemView>(&dialog, tr("CompatibilityChangeEventsDialog", "Compatibility Change Events")));
		expectNoAuditIssue(dialog);
	}
	{
		auto dialog = mappingMatrix::MappingMatrixDialog{ "Mappings", { { "Output", { "Out 1", "Out 2" } } }, { { "Input", { "In 1", "In 2" } } }, {} };
		auto const shown = showOffscreen(dialog);
		auto* const mappings = findByAccessibleName(&dialog, tr("MappingMatrix", "Mappings"));
		ASSERT_NE(nullptr, mappings);
		EXPECT_FALSE(accessibleDescription(mappings).isEmpty());
		expectButton(&dialog, "Apply", QAccessible::PushButton, QAccessibleActionInterface::pressAction());
		expectNoAuditIssue(dialog);
	}
}
