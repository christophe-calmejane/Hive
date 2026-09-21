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

#include "journalAnalysisPrivate.hpp"

#include <hive/modelsLibrary/helper.hpp>

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace hive
{
namespace modelsLibrary
{
namespace
{
using EntityID = JournalAnalysis::EntityID;
using Plane = JournalAnalysis::Plane;
using Results = JournalAnalysis::Results;

/** Renders the Markdown report of an analysis. */
class Renderer final
{
public:
	Renderer(JournalAnalysis::Private const& analysis, EventJournal::Session const& session, JournalAnalysis::ReportOptions const& options)
		: _analysis{ analysis }
		, _results{ analysis.results }
		, _session{ session }
		, _options{ options }
	{
		// Times are shown without the date when the session fits in one day
		auto const first = QDateTime::fromMSecsSinceEpoch(_results.firstTimestamp, _options.timeZone);
		auto const last = QDateTime::fromMSecsSinceEpoch(_results.lastTimestamp, _options.timeZone);
		_singleDay = first.date() == last.date();
		// Members of every rack, for the labels
		for (auto const& [entityID, entity] : _analysis.topology.entities)
		{
			for (auto const& [index, interface] : entity.interfaces)
			{
				auto const plane = static_cast<Plane>(index);
				if (auto const rack = _analysis.rackOf(entityID, plane))
				{
					_rackMembers[std::make_pair(*rack, plane)].push_back(entityID);
				}
			}
		}
		for (auto& [key, members] : _rackMembers)
		{
			std::sort(members.begin(), members.end(),
				[this](EntityID const lhs, EntityID const rhs)
				{
					return name(lhs) < name(rhs);
				});
		}
	}

	QString render()
	{
		renderHeader();
		renderMarkers();
		renderSessionShape();
		renderBreakdown();
		renderTopology();
		renderUnlocks();
		renderClockDomain();
		renderBothPlanes();
		renderOtherFamilies();
		renderStreamData();
		renderMethod();
		return _out;
	}

private:
	// Formatting helpers
	QString dateTime(qint64 const timestamp) const
	{
		return QDateTime::fromMSecsSinceEpoch(timestamp, _options.timeZone).toString("yyyy-MM-dd HH:mm:ss");
	}

	QString time(qint64 const timestamp) const
	{
		return QDateTime::fromMSecsSinceEpoch(timestamp, _options.timeZone).toString(_singleDay ? "HH:mm:ss" : "MM-dd HH:mm:ss");
	}

	/** Time of an event, as a link to it in the analyser. */
	QString eventTime(JournalAnalysis::EventIndex const eventIndex) const
	{
		if (eventIndex >= _session.events.size())
		{
			return {};
		}
		return link(time(_session.events[eventIndex].timestamp), eventIndex);
	}

	QString link(QString const& label, JournalAnalysis::EventIndex const eventIndex) const
	{
		if (!_options.eventLinks)
		{
			return label;
		}
		return QString{ "[%1](event:%2)" }.arg(label).arg(eventIndex);
	}

	static QString duration(qint64 const ms)
	{
		if (ms < 1000)
		{
			return QString{ "%1 ms" }.arg(ms);
		}
		if (ms < 60000)
		{
			return QString{ "%1 s" }.arg(QString::number(static_cast<double>(ms) / 1000.0, 'f', 2));
		}
		if (ms < 3600000)
		{
			auto const minutes = ms / 60000;
			auto const seconds = (ms % 60000 + 500) / 1000;
			return seconds > 0 ? QString{ "%1 min %2 s" }.arg(minutes).arg(seconds) : QString{ "%1 min" }.arg(minutes);
		}
		auto const hours = ms / 3600000;
		auto const minutes = (ms % 3600000 + 30000) / 60000;
		return minutes > 0 ? QString{ "%1 h %2 min" }.arg(hours).arg(minutes) : QString{ "%1 h" }.arg(hours);
	}

	static QString optionalDuration(std::optional<qint64> const& ms)
	{
		return ms ? duration(*ms) : QString{ "unknown" };
	}

	/** "a to b", or "a" when both bounds are equal. */
	static QString range(qint64 const minMs, qint64 const maxMs)
	{
		return minMs == maxMs ? duration(minMs) : QString{ "%1 to %2" }.arg(duration(minMs), duration(maxMs));
	}

	static QString percent(std::size_t const part, std::size_t const total)
	{
		if (total == 0u)
		{
			return "0 %";
		}
		return QString{ "%1 %" }.arg(QString::number(100.0 * static_cast<double>(part) / static_cast<double>(total), 'f', 1));
	}

	QString name(EntityID const entityID) const
	{
		return _analysis.entityName(entityID);
	}

	static QString identifier(EntityID const entityID)
	{
		return helper::uniqueIdentifierToString(entityID);
	}

	static QString identifier(std::optional<EntityID> const& entityID)
	{
		return entityID ? identifier(*entityID) : QString{ "none" };
	}

	static QString planeName(Plane const plane)
	{
		return JournalAnalysis::planeName(plane);
	}

	QString planesText(std::vector<Plane> const& planes) const
	{
		if (planes.empty())
		{
			return "none at stream level";
		}
		auto names = QStringList{};
		for (auto const plane : planes)
		{
			names.push_back(planeName(plane));
		}
		return names.size() > 1 ? QString{ "Both (%1)" }.arg(names.join(", ")) : names.front();
	}

	/** Compacts a list of entity names sharing a prefix ("LA7.16 62", "LA7.16 63" gives "LA7.16 62/63"). */
	static QString compactNames(QStringList const& names)
	{
		if (names.isEmpty())
		{
			return {};
		}
		if (names.size() == 1)
		{
			return names.front();
		}
		auto prefix = names.front();
		for (auto const& entry : names)
		{
			auto length = 0;
			while (length < prefix.size() && length < entry.size() && prefix[length] == entry[length])
			{
				++length;
			}
			prefix.truncate(length);
		}
		auto const lastSpace = prefix.lastIndexOf(' ');
		if (lastSpace <= 0)
		{
			return names.join(", ");
		}
		prefix.truncate(lastSpace + 1);
		auto remainders = QStringList{};
		for (auto const& entry : names)
		{
			auto const remainder = entry.mid(prefix.size());
			if (remainder.isEmpty())
			{
				return names.join(", ");
			}
			remainders.push_back(remainder);
		}
		return prefix + remainders.join('/');
	}

	QString entitiesLabel(JournalAnalysis::EntityIDs const& entities, std::size_t const limit = 8u) const
	{
		auto names = QStringList{};
		for (auto const entityID : entities)
		{
			names.push_back(name(entityID));
		}
		if (static_cast<std::size_t>(names.size()) <= limit)
		{
			return compactNames(names);
		}
		auto const shown = names.mid(0, static_cast<qsizetype>(limit));
		return QString{ "%1 and %2 more" }.arg(compactNames(shown)).arg(names.size() - shown.size());
	}

	/** Label of a rack: the names of its members (the entity itself for a rack of one), else the switch identity. */
	QString rackLabel(EntityID const rack, Plane const plane) const
	{
		if (auto const it = _rackMembers.find(std::make_pair(rack, plane)); it != _rackMembers.end() && !it->second.empty())
		{
			return entitiesLabel(it->second, 6u);
		}
		return identifier(rack);
	}

	// Markdown helpers
	void heading(int const level, QString const& text)
	{
		_out += QString{ "%1 %2\n\n" }.arg(QString{ level, '#' }, text);
	}

	void paragraph(QString const& text)
	{
		_out += text + "\n\n";
	}

	void bullet(QString const& text)
	{
		_out += "* " + text + "\n";
	}

	void endList()
	{
		_out += "\n";
	}

	static QString cell(QString text)
	{
		return text.replace('|', "\\|").replace('\n', ' ');
	}

	void table(QStringList const& headers, std::vector<QStringList> const& rows)
	{
		auto line = QStringList{};
		for (auto const& header : headers)
		{
			line.push_back(cell(header));
		}
		_out += "| " + line.join(" | ") + " |\n";
		line.clear();
		for (auto i = 0; i < headers.size(); ++i)
		{
			line.push_back("---");
		}
		_out += "| " + line.join(" | ") + " |\n";
		for (auto const& row : rows)
		{
			line.clear();
			for (auto const& value : row)
			{
				line.push_back(cell(value));
			}
			_out += "| " + line.join(" | ") + " |\n";
		}
		_out += "\n";
	}

	/** Key and value table (the header tables of the reports). */
	void keyValues(std::vector<std::pair<QString, QString>> const& entries)
	{
		_out += "| | |\n|---|---|\n";
		for (auto const& [key, value] : entries)
		{
			_out += QString{ "| %1 | %2 |\n" }.arg(cell(key), cell(value));
		}
		_out += "\n";
	}

	// Sections
	void renderHeader()
	{
		auto const& metadata = _session.metadata;
		heading(1, _options.title.isEmpty() ? QString{ "Analysis of the Event Journal" } : _options.title);
		auto const offset = QDateTime::fromMSecsSinceEpoch(_results.firstTimestamp, _options.timeZone).offsetFromUtc();
		auto const sign = offset < 0 ? '-' : '+';
		auto const offsetText = QString{ "UTC%1%2" }.arg(sign).arg(std::abs(offset) / 3600);
		paragraph(QString{ "Computed by the Journal Analyser from the events of the journal (the interpretation is the analyst's). All times are local (%1, the journal stores UTC)." }.arg(offsetText));

		auto entries = std::vector<std::pair<QString, QString>>{};
		entries.emplace_back("Journal window", QString{ "%1 to %2" }.arg(dateTime(_results.firstTimestamp), dateTime(_results.lastTimestamp)));
		entries.emplace_back("Duration", duration(_results.lastTimestamp - _results.firstTimestamp));
		auto recorder = QString{ "Hive %1 on %2" }.arg(metadata.value("hive_version", "?"), metadata.value("computer_name", "?"));
		if (metadata.contains("secondary_interface_name"))
		{
			recorder += QString{ ", redundant controller (Primary \"%1\", Secondary \"%2\")" }.arg(metadata.value("interface_name"), metadata.value("secondary_interface_name"));
		}
		else if (metadata.contains("interface_name"))
		{
			recorder += QString{ ", interface \"%1\"" }.arg(metadata.value("interface_name"));
		}
		entries.emplace_back("Recorded by", recorder);
		entries.emplace_back("Schema version", QString::number(_session.schemaVersion()));
		if (metadata.contains("recording_level_at_start"))
		{
			entries.emplace_back("Recording level at start", metadata.value("recording_level_at_start"));
		}
		entries.emplace_back("Total events", QString::number(_session.events.size()));
		auto entitiesText = QString::number(_results.entitiesCount);
		if (!_results.controllers.empty())
		{
			entitiesText += QString{ " (other controllers: %1)" }.arg(entitiesLabel(_results.controllers));
		}
		entries.emplace_back("Entities", entitiesText);
		if (_results.isInterrupted)
		{
			entries.emplace_back("Interrupted", "the last event is not 'Recording stopped': the session was interrupted (crash or forced quit), the last events may be missing");
		}
		keyValues(entries);
	}

	void renderMarkers()
	{
		if (_results.markers.empty())
		{
			return;
		}
		heading(2, "Markers");
		paragraph("What the operator saw or did, the ground truth to align the findings with.");
		auto rows = std::vector<QStringList>{};
		for (auto const index : _results.markers)
		{
			auto const& event = _session.events[index];
			rows.push_back({ eventTime(index), event.summary });
		}
		table({ "Time", "Marker" }, rows);
	}

	void renderSessionShape()
	{
		heading(2, "Session shape");
		bullet(QString{ "%1 entities online within the first minute, %2 in the whole session." }.arg(_results.entitiesOnlineAtStart).arg(_results.entitiesCount));
		for (auto const& wave : _results.waves)
		{
			bullet(QString{ "%1: %2 entities went %3 within %4 (%5)." }.arg(eventTime(wave.events.front())).arg(wave.entities.size()).arg(wave.isOnline ? "online" : "offline").arg(duration(std::max<qint64>(wave.endTimestamp - wave.timestamp, 1))).arg(entitiesLabel(wave.entities)));
		}
		for (auto const index : _results.sessionEvents)
		{
			auto const& event = _session.events[index];
			if (event.summary == "Recording started" || event.summary == "Recording stopped")
			{
				continue;
			}
			bullet(QString{ "%1: %2." }.arg(eventTime(index), event.summary));
		}
		for (auto const index : _results.controllerInterfaceEvents)
		{
			auto const& event = _session.events[index];
			bullet(QString{ "%1: %2 (%3), a controller side event." }.arg(eventTime(index), event.summary, event.subject));
		}
		if (_results.heartbeats > 0u)
		{
			auto text = QString{ "%1 heartbeats" }.arg(_results.heartbeats);
			if (_results.heartbeatIntervalMs)
			{
				text += QString{ " every %1" }.arg(duration(*_results.heartbeatIntervalMs));
			}
			if (_results.heartbeatGaps.empty())
			{
				text += ", no gap: the controller was alive for the whole session.";
			}
			else
			{
				text += QString{ ", %1 gaps (the controller was frozen or stopped, do not read an absence of events during them as a quiet network):" }.arg(_results.heartbeatGaps.size());
			}
			bullet(text);
			for (auto const& gap : _results.heartbeatGaps)
			{
				bullet(QString{ "  gap of %1 from %2." }.arg(duration(gap.durationMs), eventTime(gap.events.front())));
			}
			if (!_results.heartbeatLatencyOutliers.empty())
			{
				auto times = QStringList{};
				for (auto const index : _results.heartbeatLatencyOutliers)
				{
					times.push_back(QString{ "%1 (%2 ms)" }.arg(eventTime(index)).arg(_analysis.infos[index].value));
				}
				bullet(QString{ "Event loop latency of %1 ms or more at %2: the controller was overloaded, the timestamps of those periods are late." }.arg(_analysis.options.heartbeatLatencyThresholdMs).arg(times.join(", ")));
			}
		}
		endList();

		if (!_results.dropOuts.empty())
		{
			auto refreshes = std::size_t{ 0u };
			auto rows = std::vector<QStringList>{};
			for (auto const& dropOut : _results.dropOuts)
			{
				if (dropOut.isRefresh)
				{
					++refreshes;
					continue;
				}
				if (rows.size() < _analysis.options.listingSize)
				{
					rows.push_back({ eventTime(dropOut.events.front()), name(dropOut.entityID), dropOut.durationMs ? duration(*dropOut.durationMs) : QString{ "never came back" } });
				}
			}
			auto text = QString{ "%1 entities dropped out (offline then back) outside of the waves" }.arg(_results.dropOuts.size() - refreshes);
			if (refreshes > 0u)
			{
				text += QString{ ", plus %1 re-enumerations requested by the operator (not network events)" }.arg(refreshes);
			}
			if (rows.size() < _results.dropOuts.size() - refreshes)
			{
				text += QString{ "; the first %1" }.arg(rows.size());
			}
			paragraph(text + ":");
			if (!rows.empty())
			{
				table({ "Time", "Entity", "Offline for" }, rows);
			}
		}
	}

	void renderBreakdown()
	{
		heading(2, "Event breakdown");
		auto rows = std::vector<QStringList>{};
		auto sorted = std::vector<std::pair<std::pair<EventJournal::Category, EventJournal::Severity>, std::size_t>>{ _results.categoryCounts.begin(), _results.categoryCounts.end() };
		std::sort(sorted.begin(), sorted.end(),
			[](auto const& lhs, auto const& rhs)
			{
				return lhs.second > rhs.second;
			});
		for (auto const& [key, count] : sorted)
		{
			rows.push_back({ EventJournal::categoryToString(key.first), EventJournal::severityToString(key.second), QString::number(count) });
		}
		table({ "Category", "Severity", "Count" }, rows);

		auto families = std::vector<std::pair<QString, std::size_t>>{ _results.familyCounts.begin(), _results.familyCounts.end() };
		std::sort(families.begin(), families.end(),
			[](auto const& lhs, auto const& rhs)
			{
				return lhs.second > rhs.second;
			});
		rows.clear();
		for (auto const& [family, count] : families)
		{
			if (rows.size() >= 25u)
			{
				break;
			}
			rows.push_back({ family, QString::number(count) });
		}
		table({ "Event", "Count" }, rows);

		if (!_results.absentCategories.empty())
		{
			auto names = QStringList{};
			for (auto const category : _results.absentCategories)
			{
				names.push_back(EventJournal::categoryToString(category));
			}
			paragraph(QString{ "Absent categories: %1. What is absent matters as much as what is present (no grandmaster change, no link event, no packet loss counter is important information), as long as the journal was recorded by a version that had the family." }.arg(names.join(", ")));
		}
	}

	void renderTopology()
	{
		auto const& topology = _analysis.topology;
		if (!topology.hasAsPaths())
		{
			heading(2, "Topology");
			paragraph("Unknown: the journal has no 'Entity is online' details with the gPTP AsPath (schema version 1) and no network state file was loaded. The rack incidents, storms, runs and rankings per rack and branch need it (File > Load Network State).");
			return;
		}
		heading(2, "Topology");
		paragraph("From the gPTP AsPath of every interface (the last hop before the entity is its rack switch, the one before is its branch switch; an entity plugged directly into a branch switch is a rack of its own).");
		// Grandmasters per plane
		auto grandmasters = std::map<Plane, std::map<EntityID, std::size_t>>{};
		auto branches = std::map<std::pair<Plane, std::optional<EntityID>>, std::set<EntityID>>{};
		for (auto const& [entityID, entity] : topology.entities)
		{
			for (auto const& [index, interface] : entity.interfaces)
			{
				auto const plane = static_cast<Plane>(index);
				if (interface.grandmasterID)
				{
					++grandmasters[plane][interface.grandmasterID];
				}
				if (auto const rack = _analysis.rackOf(entityID, plane))
				{
					branches[std::make_pair(plane, _analysis.branchOf(entityID, plane))].insert(*rack);
				}
			}
		}
		for (auto const& [plane, counts] : grandmasters)
		{
			auto parts = QStringList{};
			for (auto const& [grandmaster, count] : counts)
			{
				parts.push_back(QString{ "%1 (%2 interfaces)" }.arg(identifier(grandmaster)).arg(count));
			}
			bullet(QString{ "%1 grandmaster: %2." }.arg(planeName(plane), parts.join(", ")));
		}
		endList();
		auto rows = std::vector<QStringList>{};
		for (auto const& [key, racks] : branches)
		{
			auto labels = QStringList{};
			for (auto const rack : racks)
			{
				labels.push_back(rackLabel(rack, key.first));
			}
			rows.push_back({ planeName(key.first), identifier(key.second), QString::number(racks.size()), labels.join("; ") });
		}
		table({ "Plane", "Branch switch", "Racks", "Racks (entities)" }, rows);
		if (_results.asPathChanges > 0u)
		{
			paragraph(QString{ "%1 'gPTP AsPath changed' events: the mapping changed during the session (a re-patch), the table shows the paths at the end of it." }.arg(_results.asPathChanges));
		}
	}

	void renderUnlocks()
	{
		heading(2, "Media clock unlocks at stream level");
		if (_results.unlockEvents == 0u)
		{
			paragraph("No stream level media unlock event ('Media unlocked' or the 'Media Unlocked' error counter).");
			return;
		}
		auto const& options = _analysis.options;
		auto perPlane = [](std::map<Plane, std::size_t> const& counts)
		{
			auto parts = QStringList{};
			for (auto const& [plane, count] : counts)
			{
				parts.push_back(QString{ "%1 %2" }.arg(planeName(plane)).arg(count));
			}
			return parts.join(" / ");
		};
		auto entries = std::vector<std::pair<QString, QString>>{};
		entries.emplace_back("Unlock events", QString{ "%1 (%2)" }.arg(_results.unlockEvents).arg(perPlane(_results.unlockEventsPerPlane)));
		entries.emplace_back(QString{ "Entity and plane hits (same entity, same plane, within %1)" }.arg(duration(options.hitWindowMs)), QString{ "%1 (%2)" }.arg(_results.hits.size()).arg(perPlane(_results.hitsPerPlane)));
		if (_analysis.topology.hasAsPaths())
		{
			entries.emplace_back(QString{ "Rack and plane incidents (entities of a rack, same plane, within %1)" }.arg(duration(options.hitWindowMs)), QString{ "%1 (%2), %3 of them involving every entity of the rack" }.arg(_results.incidents.size()).arg(perPlane(_results.incidentsPerPlane)).arg(_results.fullRackIncidents));
			entries.emplace_back("Of which inside the system wide events", QString::number(_results.systemWideIncidents));
			entries.emplace_back("Background incidents", QString::number(_results.incidents.size() - _results.systemWideIncidents));
			entries.emplace_back(QString{ "Storms (incidents of a rack and plane less than %1 apart)" }.arg(duration(options.stormGapMs)), QString::number(_results.storms.size()));
			entries.emplace_back(QString{ "Runs (storms of a branch and plane less than %1 apart)" }.arg(duration(options.runGapMs)), QString::number(_results.runs.size()));
		}
		auto const hours = std::max<qint64>(_results.lastTimestamp - _results.firstTimestamp, 1) / 3600000.0;
		entries.emplace_back("Hits per hour", QString::number(static_cast<double>(_results.hits.size()) / hours, 'f', 1));
		keyValues(entries);

		renderSystemWideEvents();
		renderRuns();
		renderRackRanking();
		renderEntityRanking();
	}

	void renderSystemWideEvents()
	{
		heading(3, "System wide events");
		auto const& options = _analysis.options;
		if (_results.systemWideEvents.empty())
		{
			paragraph(QString{ "None: no moment where at least %1 % of the entities reporting hits (%2 at least) were hit within %3 of each other%4." }.arg(static_cast<int>(options.systemWideMinShare * 100.0)).arg(options.systemWideMinEntities).arg(duration(options.systemWideGapMs)).arg(_analysis.topology.hasAsPaths() ? " across several racks" : ""));
			return;
		}
		paragraph(QString{ "Moments where many entities were hit within %1 of each other (stream level unlocks and clock domain unlocks), across several racks: the origin is above the racks (grandmaster, media clock talker, or a control action). The entities spared and the branches partially hit localize it (a redundant installation counts one rack switch per plane)." }.arg(duration(options.systemWideGapMs)));
		auto rows = std::vector<QStringList>{};
		for (auto const& event : _results.systemWideEvents)
		{
			auto clockText = QString::number(event.clockDomainEntities.size());
			if (event.minClockDomainUnlockMs)
			{
				clockText += QString{ " (%1)" }.arg(range(*event.minClockDomainUnlockMs, *event.maxClockDomainUnlockMs));
			}
			auto accompanying = QStringList{};
			if (event.grandmasterChanges > 0u)
			{
				accompanying.push_back(QString{ "%1 grandmaster changes" }.arg(event.grandmasterChanges));
			}
			if (event.linkEvents > 0u)
			{
				accompanying.push_back(QString{ "%1 link events" }.arg(event.linkEvents));
			}
			if (event.packetLossEvents > 0u)
			{
				accompanying.push_back(QString{ "%1 packet loss counters" }.arg(event.packetLossEvents));
			}
			if (event.streamDataLosses > 0u)
			{
				accompanying.push_back(QString{ "%1 stream data losses" }.arg(event.streamDataLosses));
			}
			auto const racksText = _analysis.topology.hasAsPaths() ? QString{ "%1 rack switches, %2 branches" }.arg(event.racks).arg(event.branches) : QString{ "unknown" };
			rows.push_back({ eventTime(event.events.front()), duration(std::max<qint64>(event.endTimestamp - event.timestamp, 1)), planesText(event.planes), QString{ "%1 (%2)" }.arg(event.entities.size()).arg(entitiesLabel(event.entities, 6u)), racksText, clockText, accompanying.isEmpty() ? QString{ "none (a timing discontinuity, not a connectivity loss)" } : accompanying.join(", ") });
		}
		table({ "Time", "Span", "Plane", "Entities hit", "Switches", "Clock domain unlocks", "Accompanied by" }, rows);
		for (auto const& event : _results.systemWideEvents)
		{
			auto parts = QStringList{};
			for (auto const& share : event.branchShares)
			{
				parts.push_back(QString{ "%1 %2: %3 of %4" }.arg(planeName(share.plane), identifier(share.branchSwitch)).arg(share.hit).arg(share.total));
			}
			auto text = QString{ "%1: " }.arg(eventTime(event.events.front()));
			if (!parts.isEmpty())
			{
				text += QString{ "branches hit (entities hit of the entities of the branch reporting hits in the session): %1. " }.arg(parts.join(", "));
			}
			text += event.spared.empty() ? QString{ "No entity spared." } : QString{ "Spared: %1." }.arg(entitiesLabel(event.spared, 12u));
			bullet(text);
		}
		endList();
	}

	void renderRuns()
	{
		if (!_analysis.topology.hasAsPaths())
		{
			return;
		}
		heading(3, "Runs: storms walking the ports of a branch switch");
		auto const& options = _analysis.options;
		if (_results.runs.empty())
		{
			paragraph("No storm outside of the system wide events.");
			return;
		}
		paragraph(QString{ "Background storms (outside of the system wide events) grouped per branch switch and plane when less than %1 apart. The sweeps (runs visiting several racks) are listed with the racks in order of their main storm: a fixed non numeric order repeated at every sweep is the physical port order of the switch, and a stable period per switch is the signature of a scheduled process." }.arg(duration(options.runGapMs)));
		auto rows = std::vector<QStringList>{};
		for (auto const& run : _results.runs)
		{
			if (run.racks.size() < 2u)
			{
				continue;
			}
			auto racks = QStringList{};
			auto minVisit = std::optional<qint64>{};
			auto maxVisit = std::optional<qint64>{};
			for (auto const& visit : run.racks)
			{
				racks.push_back(rackLabel(visit.rackSwitch, run.plane));
				auto const span = visit.endTimestamp - visit.timestamp;
				minVisit = minVisit ? std::min(*minVisit, span) : span;
				maxVisit = maxVisit ? std::max(*maxVisit, span) : span;
			}
			rows.push_back({ QString{ "%1 to %2" }.arg(eventTime(run.events.front()), time(run.endTimestamp)), planeName(run.plane), identifier(run.branchSwitch), racks.join(" > "), range(*minVisit, *maxVisit), QString::number(run.storms), QString::number(run.incidents) });
		}
		if (rows.empty())
		{
			paragraph("No sweep: every background storm stayed on one rack.");
		}
		else
		{
			table({ "Sweep", "Plane", "Branch", "Racks in order of appearance", "Time per rack", "Storms", "Incidents" }, rows);
		}

		rows.clear();
		for (auto const& series : _results.runSeries)
		{
			auto durations = std::vector<qint64>{};
			for (auto const runIndex : series.sweeps)
			{
				durations.push_back(_results.runs[runIndex].endTimestamp - _results.runs[runIndex].timestamp);
			}
			std::sort(durations.begin(), durations.end());
			auto const periodText = series.minPeriodMs ? range(*series.minPeriodMs, *series.maxPeriodMs) : QString{ "n/a" };
			auto const durationText = durations.empty() ? QString{ "n/a" } : range(durations.front(), durations.back());
			auto orderText = QString{ "n/a" };
			if (series.sweeps.size() == 1u)
			{
				orderText = "single sweep";
			}
			else if (series.sweeps.size() > 1u)
			{
				orderText = series.sameOrderSweeps == series.sweeps.size() ? QString{ "same order every sweep" } : QString{ "same order in %1 of %2 sweeps" }.arg(series.sameOrderSweeps).arg(series.sweeps.size());
			}
			rows.push_back({ planeName(series.plane), identifier(series.branchSwitch), QString::number(series.sweeps.size()), QString::number(series.runs.size() - series.sweeps.size()), periodText, durationText, orderText });
		}
		table({ "Plane", "Branch", "Sweeps", "Isolated storms", "Period", "Sweep duration", "Port order" }, rows);
	}

	void renderRackRanking()
	{
		if (!_analysis.topology.hasAsPaths() || _results.rackRanking.empty())
		{
			return;
		}
		heading(3, "Rack and branch ranking");
		auto const& options = _analysis.options;
		auto rows = std::vector<QStringList>{};
		for (auto const& rack : _results.rackRanking)
		{
			if (rows.size() >= options.rankingSize)
			{
				break;
			}
			rows.push_back({ planeName(rack.plane), QString::number(rack.incidents), QString::number(rack.backgroundIncidents), identifier(rack.rackSwitch), identifier(rack.branchSwitch), entitiesLabel(rack.entities, 6u) });
		}
		table({ "Plane", "Incidents", "Background", "Rack switch", "Branch switch", "Entities" }, rows);
		rows.clear();
		for (auto const& branch : _results.branchRanking)
		{
			rows.push_back({ planeName(branch.plane), identifier(branch.branchSwitch), QString::number(branch.incidents), QString::number(branch.backgroundIncidents), QString::number(branch.racks) });
		}
		table({ "Plane", "Branch switch", "Incidents", "Background", "Racks hit" }, rows);
		paragraph("A rack far above the others under the same branch is a link problem (its uplink, SFP, fiber); a whole branch above the others is the branch switch or its cycle.");
	}

	void renderEntityRanking()
	{
		if (_results.entityRanking.empty())
		{
			return;
		}
		heading(3, "Entity ranking");
		auto const& options = _analysis.options;
		auto rows = std::vector<QStringList>{};
		auto const hasTopology = _analysis.topology.hasAsPaths();
		for (auto const& entry : _results.entityRanking)
		{
			if (rows.size() >= options.rankingSize)
			{
				break;
			}
			auto perPlane = QStringList{};
			for (auto const& [plane, count] : entry.perPlane)
			{
				perPlane.push_back(QString{ "%1 %2" }.arg(planeName(plane)).arg(count));
			}
			auto row = QStringList{ name(entry.entityID), QString::number(entry.total), perPlane.join(" / "), QString::number(entry.clockDomainUnlocks) };
			if (hasTopology)
			{
				auto racks = QStringList{};
				for (auto const& [plane, count] : entry.perPlane)
				{
					if (auto const rack = _analysis.rackOf(entry.entityID, plane))
					{
						racks.push_back(QString{ "%1 %2" }.arg(planeName(plane), identifier(*rack)));
					}
				}
				row.push_back(racks.join(" / "));
			}
			rows.push_back(row);
		}
		auto headers = QStringList{ "Entity", "Hits", "Per plane", "Clock domain unlocks" };
		if (hasTopology)
		{
			headers.push_back("Rack switch");
		}
		table(headers, rows);
		auto const& last = _results.entityRanking.back();
		paragraph(QString{ "%1 entities with at least one hit or clock domain unlock; median %2 hits per entity, minimum %3, maximum %4. An outlier explained by its rack or branch is a network position, an outlier alone in its rack is a device property." }.arg(_results.entityRanking.size()).arg(_results.medianEntityHits).arg(last.total).arg(_results.entityRanking.front().total));
	}

	void renderClockDomain()
	{
		heading(2, "Clock domain");
		if (_results.clockDomainIntervals.empty())
		{
			paragraph("No clock domain unlock ('Media clock unlocked').");
			return;
		}
		auto const& options = _analysis.options;
		auto const& stats = _results.clockDomainStats;
		auto entries = std::vector<std::pair<QString, QString>>{};
		entries.emplace_back("Clock domain unlocks", QString::number(_results.clockDomainIntervals.size()));
		entries.emplace_back("With a known duration", QString{ "%1 (%2 inferred from the counters or never relocked)" }.arg(stats.count).arg(stats.unknown));
		if (stats.count > 0u)
		{
			entries.emplace_back("Unlock duration", QString{ "median %1, minimum %2, maximum %3, total %4" }.arg(duration(stats.medianMs), duration(stats.minMs), duration(stats.maxMs), duration(stats.totalMs)));
		}
		auto const& companions = _results.clockDomainCompanions;
		auto const total = _results.clockDomainIntervals.size();
		entries.emplace_back(QString{ "Accompanying stream unlocks (same entity, within %1)" }.arg(duration(options.accompanyingWindowMs)), QString{ "Primary only %1 (%2), Secondary only %3 (%4), both planes %5 (%6), none %7 (%8)" }.arg(companions.primaryOnly).arg(percent(companions.primaryOnly, total)).arg(companions.secondaryOnly).arg(percent(companions.secondaryOnly, total)).arg(companions.bothPlanes).arg(percent(companions.bothPlanes, total)).arg(companions.none).arg(percent(companions.none, total)));
		entries.emplace_back("Stream hits accompanied by a clock domain unlock", QString{ "%1 of %2 (%3)" }.arg(_results.hitsWithClockDomainUnlock).arg(_results.hits.size()).arg(percent(_results.hitsWithClockDomainUnlock, _results.hits.size())));
		keyValues(entries);
		if (companions.primaryOnly + companions.secondaryOnly > companions.bothPlanes)
		{
			paragraph("Clock domains unlock when a single plane glitches: the media clock stream pair failover is not seamless on these devices (worth raising with the firmware team), but the audio path is not what threatens the show as long as the other plane stays clean.");
		}
		if (!_results.longestClockDomainIntervals.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const index : _results.longestClockDomainIntervals)
			{
				auto const& interval = _results.clockDomainIntervals[index];
				rows.push_back({ eventTime(interval.startEvent), name(interval.entityID), interval.subject, duration(*interval.durationMs) });
			}
			table({ "Time", "Entity", "Clock domain", "Duration" }, rows);
		}
		if (_results.systemWideEventsWithClockDomainUnlocks > 0u)
		{
			auto recurring = JournalAnalysis::EntityIDs{};
			auto once = std::size_t{ 0u };
			for (auto const& entry : _results.clockDomainGroup)
			{
				if (entry.events >= 2u)
				{
					recurring.push_back(entry.entityID);
				}
				else
				{
					++once;
				}
			}
			auto text = QString{ "%1 of the system wide events unlocked clock domains. " }.arg(_results.systemWideEventsWithClockDomainUnlocks);
			if (!recurring.empty())
			{
				text += QString{ "%1 entities lose their clock domain at two or more of them (%2)" }.arg(recurring.size()).arg(entitiesLabel(recurring, 40u));
				text += once > 0u ? QString{ ", %1 at one only. " }.arg(once) : QString{ ". " };
				text += "A fixed subset unlocking every time is a per device property (audio patch, clock source, firmware) rather than a network position: check what those entities have in common.";
			}
			else
			{
				text += QString{ "%1 entities unlocked at one of them, none at several." }.arg(once);
			}
			paragraph(text);
		}

		// Stream level pairs
		if (!_results.streamIntervals.empty())
		{
			heading(3, "Per stream unlock durations");
			auto rows = std::vector<QStringList>{};
			for (auto const& [plane, planeStats] : _results.streamStatsPerPlane)
			{
				rows.push_back({ planeName(plane), QString::number(planeStats.count + planeStats.unknown), QString::number(planeStats.unknown), planeStats.count > 0u ? duration(planeStats.medianMs) : QString{ "n/a" }, planeStats.count > 0u ? duration(planeStats.minMs) : QString{ "n/a" }, planeStats.count > 0u ? duration(planeStats.maxMs) : QString{ "n/a" } });
			}
			table({ "Plane", "Unlocks", "Inferred or open", "Median", "Minimum", "Maximum" }, rows);
			if (_results.bothPlanesUnlocked.empty())
			{
				paragraph("No period where a primary and a secondary stream of the same entity were unlocked at the same time: no audible loss from the media clock, as far as the counters tell.");
			}
			else
			{
				paragraph(QString{ "%1 periods where a primary and a secondary stream of the same entity were unlocked at the same time (the audible ones):" }.arg(_results.bothPlanesUnlocked.size()));
				rows.clear();
				for (auto const& interval : _results.bothPlanesUnlocked)
				{
					if (rows.size() >= options.listingSize)
					{
						break;
					}
					rows.push_back({ link(time(interval.timestamp), interval.startEvent), name(interval.entityID), interval.subject, duration(*interval.durationMs) });
				}
				table({ "Time", "Entity", "Streams", "Overlap" }, rows);
			}
		}
	}

	void renderBothPlanes()
	{
		heading(2, "Both planes");
		if (_results.hits.empty())
		{
			paragraph("No hit.");
			return;
		}
		auto const& options = _analysis.options;
		auto entries = std::vector<std::pair<QString, QString>>{};
		entries.emplace_back("Entity and plane hits", QString::number(_results.hits.size()));
		entries.emplace_back("Primary only", QString::number(_results.primaryOnlyHits));
		entries.emplace_back("Secondary only", QString::number(_results.secondaryOnlyHits));
		auto background = std::size_t{ 0u };
		for (auto const& pair : _results.bothPlanesPairs)
		{
			if (!pair.isSystemWide)
			{
				++background;
			}
		}
		entries.emplace_back(QString{ "Both planes on the same entity within %1" }.arg(duration(options.bothPlanesWindowMs)), QString{ "%1 pairs (%2 of the hits), %3 inside the system wide events, %4 in the background" }.arg(_results.bothPlanesPairs.size()).arg(percent(2u * _results.bothPlanesPairs.size(), _results.hits.size())).arg(_results.bothPlanesPairs.size() - background).arg(background));
		if (_results.bothPlanesMedianDelayMs)
		{
			entries.emplace_back("Delay between the two planes", QString{ "median %1, minimum %2, maximum %3; %4 pairs below %5 (truly simultaneous, a common source), the others sequential" }.arg(duration(*_results.bothPlanesMedianDelayMs), duration(*_results.bothPlanesMinDelayMs), duration(*_results.bothPlanesMaxDelayMs)).arg(_results.simultaneousPairs).arg(duration(options.simultaneousMs)));
		}
		keyValues(entries);
		if (background > 0u)
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& pair : _results.bothPlanesPairs)
			{
				if (pair.isSystemWide)
				{
					continue;
				}
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ eventTime(pair.events.front()), name(pair.entityID), duration(pair.delayMs) });
			}
			paragraph("Background pairs (outside of the system wide events):");
			table({ "Time", "Entity", "Delay" }, rows);
		}
	}

	void renderOtherFamilies()
	{
		heading(2, "Other error families");
		auto const& options = _analysis.options;

		heading(3, "Grandmaster changes");
		if (_results.grandmasterChanges == 0u)
		{
			paragraph(QString{ "None%1: what a healthy installation looks like." }.arg(_results.grandmasterCounterIncreases > 0u ? QString{ " observed by the controller, but %1 'GPTP GM Changed' counter increases (the grandmaster changed and came back between two notifications)" }.arg(_results.grandmasterCounterIncreases) : QString{}));
		}
		else
		{
			paragraph(QString{ "%1 grandmaster changes in %2 storms (changes less than %3 apart). A grandmaster change is a time discontinuity for the whole segment that lost its Announce messages; intermediate switches winning the election means the BMCA priorities are not locked down." }.arg(_results.grandmasterChanges).arg(_results.grandmasterStorms.size()).arg(duration(options.grandmasterStormGapMs)));
			auto rows = std::vector<QStringList>{};
			for (auto const& storm : _results.grandmasterStorms)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				auto flips = QStringList{};
				for (auto const& flip : storm.flips)
				{
					if (flips.size() >= 4)
					{
						break;
					}
					flips.push_back(QString{ "%1 to %2 (%3)" }.arg(identifier(flip.from), identifier(flip.to)).arg(flip.count));
				}
				rows.push_back({ eventTime(storm.events.front()), duration(std::max<qint64>(storm.endTimestamp - storm.timestamp, 1)), planesText(storm.planes), QString::number(storm.changes), QString{ "%1 (%2)" }.arg(storm.entities.size()).arg(entitiesLabel(storm.entities, 6u)), flips.join("; "), QString{ "%1 packet loss counters, %2 clock domain unlocks" }.arg(storm.packetLossEvents).arg(storm.clockDomainUnlocks) });
			}
			table({ "Time", "Span", "Plane", "Changes", "Entities", "Flips", "Accompanied by" }, rows);
			if (_results.grandmasterCounterIncreases > 0u)
			{
				paragraph(QString{ "Plus %1 'GPTP GM Changed' counter increases (some of them the changes above, the others changes that came back between two notifications)." }.arg(_results.grandmasterCounterIncreases));
			}
		}

		heading(3, "Link");
		if (_results.linkIntervals.empty())
		{
			paragraph("No link down on the entities.");
		}
		else
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& interval : _results.linkIntervals)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ eventTime(interval.startEvent), name(interval.entityID), interval.subject, interval.isInferred ? QString{ "inferred from the counters (came back between two notifications)" } : optionalDuration(interval.durationMs) });
			}
			paragraph(QString{ "%1 link downs:" }.arg(_results.linkIntervals.size()));
			table({ "Time", "Entity", "Interface", "Down for" }, rows);
		}
		if (!_results.nullPropagationDelayIntervals.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& interval : _results.nullPropagationDelayIntervals)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ eventTime(interval.startEvent), name(interval.entityID), interval.subject, interval.isClosed ? optionalDuration(interval.durationMs) : QString{ "still null at the end of the session" } });
			}
			paragraph(QString{ "%1 links whose gPTP propagation delay dropped to zero (a null delay is only expected on a link to a bridge embedded in the same unit, and such a link never measures one: a measurement anomaly of the peer delay mechanism, the same family as the null delays the Network Graph flags):" }.arg(_results.nullPropagationDelayIntervals.size()));
			table({ "Time", "Entity", "Interface", "Null for" }, rows);
		}

		heading(3, "Packet loss counters");
		if (_results.packetLossPerCounter.empty())
		{
			paragraph("None: as far as the counters tell, not a single packet was lost (the timing family above is a separate problem).");
		}
		else
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& [counter, count] : _results.packetLossPerCounter)
			{
				auto perPlane = QStringList{};
				for (auto const& [key, planeCount] : _results.packetLossPerCounterAndPlane)
				{
					if (key.first == counter)
					{
						perPlane.push_back(QString{ "%1 %2" }.arg(planeName(key.second)).arg(planeCount));
					}
				}
				rows.push_back({ counter, QString::number(count), perPlane.join(" / ") });
			}
			table({ "Counter", "Increases", "Per plane" }, rows);
			rows.clear();
			for (auto const& entry : _results.packetLossCounts)
			{
				if (rows.size() >= options.rankingSize)
				{
					break;
				}
				rows.push_back({ entry.name, name(entry.entityID), entry.subject, QString::number(entry.count), eventTime(entry.events.front()) });
			}
			table({ "Counter", "Entity", "Stream", "Increases", "First" }, rows);
		}

		heading(3, "Controller side");
		auto lines = QStringList{};
		for (auto const& entry : _results.controllerCounts)
		{
			lines.push_back(QString{ "%1 '%2' increases on %3 entities%4" }.arg(entry.count).arg(entry.name).arg(entry.entities).arg(entry.plane != JournalAnalysis::UnknownPlane ? QString{ " (%1 interface)" }.arg(planeName(entry.plane)) : QString{}));
		}
		if (_results.transportErrors > 0u)
		{
			lines.push_back(QString{ "%1 transport errors on the controller interfaces" }.arg(_results.transportErrors));
		}
		for (auto const& [plane, count] : _results.unsolicitedLossesPerInterface)
		{
			lines.push_back(QString{ "%1 entities no longer received on the %2 interface (unsolicited notifications lost)" }.arg(count).arg(planeName(plane)));
		}
		for (auto const& [plane, count] : _results.offlineOnOneInterface)
		{
			lines.push_back(QString{ "%1 entities offline on the %2 interface only" }.arg(count).arg(planeName(plane)));
		}
		if (_results.redundancyWarnings > 0u)
		{
			lines.push_back(QString{ "%1 redundancy warnings (the redundant interfaces appear to be connected to the same network)" }.arg(_results.redundancyWarnings));
		}
		if (_results.latencyErrors > 0u)
		{
			lines.push_back(QString{ "%1 stream latency errors" }.arg(_results.latencyErrors));
		}
		if (lines.isEmpty())
		{
			paragraph("Nothing: no controller side counter, no redundancy warning, no latency error.");
		}
		else
		{
			for (auto const& line : lines)
			{
				bullet(line + ".");
			}
			endList();
			paragraph("The controller side counters and the per interface losses tell about the controller computer's own links as much as about the network: an interface far above the other points at that NIC or its link.");
		}

		heading(3, "Operator actions and connection changes");
		if (_results.operatorActions.empty() && _results.connectionChanges == 0u)
		{
			paragraph("None: nobody touched the patch or the devices during the session, as far as the journal tells (older journals do not record these).");
		}
		else
		{
			paragraph(QString{ "%1 stream connection changes and %2 operator actions (acquire, lock, refresh, identify, acknowledged counters, and the configuration changes of the user level)%3:" }.arg(_results.connectionChanges).arg(_results.operatorActions.size()).arg(_results.operatorActions.size() > options.listingSize ? QString{ "; the first %1" }.arg(options.listingSize) : QString{}));
			auto rows = std::vector<QStringList>{};
			for (auto const index : _results.operatorActions)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				auto const& event = _session.events[index];
				rows.push_back({ eventTime(index), name(event.entityID), event.subject, event.summary });
			}
			if (!rows.empty())
			{
				table({ "Time", "Entity", "Subject", "Action" }, rows);
			}
		}
	}

	void renderStreamData()
	{
		heading(2, "Stream data losses");
		if (!_results.hasStreamDataEvents)
		{
			paragraph("No stream data event: the journal predates the rule, or no connected stream lost frames (only meaningful when 'Stream data monitoring started' events exist, which are diagnostic level).");
			return;
		}
		auto const& options = _analysis.options;
		auto covered = std::size_t{ 0u };
		auto references = std::map<std::pair<Plane, qint64>, std::size_t>{};
		for (auto const& stream : _results.monitoredStreams)
		{
			if (stream.isCovered)
			{
				++covered;
				++references[std::make_pair(stream.plane, stream.expectedIncrement)];
			}
		}
		auto entries = std::vector<std::pair<QString, QString>>{};
		entries.emplace_back("Covered streams (monitoring started)", QString::number(covered));
		if (!references.empty())
		{
			auto parts = QStringList{};
			for (auto const& [key, count] : references)
			{
				parts.push_back(QString{ "%1 plane: %2 frames per second on %3 streams" }.arg(planeName(key.first)).arg(key.second).arg(count));
			}
			entries.emplace_back("Learned references", parts.join("; "));
		}
		if (covered > 0u)
		{
			entries.emplace_back("Connected streams not covered", _results.uncoveredStreams.empty() ? QString{ "none" } : QString{ "%1 (their absence of loss events means nothing)" }.arg(_results.uncoveredStreams.size()));
		}
		auto perPlane = QStringList{};
		for (auto const& [plane, count] : _results.streamDataLossesPerPlane)
		{
			perPlane.push_back(QString{ "%1 %2" }.arg(planeName(plane)).arg(count));
		}
		entries.emplace_back("Loss events", QString{ "%1 (%2)" }.arg(_results.streamDataLosses).arg(perPlane.join(" / ")));
		if (_results.streamDataLosses > 0u)
		{
			auto histogram = QStringList{};
			auto const bounds = std::vector<std::pair<qint64, QString>>{ { 0, "under 100 ms" }, { 100, "100 to 249 ms" }, { 250, "250 to 499 ms" }, { 500, "500 to 749 ms" }, { 750, "750 ms and more" } };
			for (auto const& [bound, label] : bounds)
			{
				if (auto const it = _results.missingMsHistogram.find(bound); it != _results.missingMsHistogram.end())
				{
					histogram.push_back(QString{ "%1: %2" }.arg(label).arg(it->second));
				}
			}
			entries.emplace_back("Missing time per event", histogram.join(", ") + " (a cluster just under the threshold means noise, a spread of small values means real cuts)");
			entries.emplace_back(QString{ "Losses within %1 of a media unlock of the same stream" }.arg(duration(options.accompanyingWindowMs)), QString{ "%1 of %2 (the incident cuts the frames; unlocks alone mean a clock disturbance)" }.arg(_results.lossesWithMediaUnlock).arg(_results.streamDataLosses));
			entries.emplace_back("Both planes of the same entity within the same second", QString{ "%1 pairs (the only signal of an audible loss)" }.arg(_results.streamDataBothPlanes.size()));
			entries.emplace_back(QString{ "Long interruptions (elapsed time over %1)" }.arg(duration(options.longInterruptionMs)), QString::number(_results.longInterruptions.size()));
		}
		keyValues(entries);

		if (!_results.uncoveredStreams.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& stream : _results.uncoveredStreams)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ name(stream.entityID), QString::number(stream.streamIndex), planeName(stream.plane), stream.talker ? QString{ "%1 (output stream %2)" }.arg(name(stream.talker->entityID)).arg(stream.talker->streamIndex) : QString{} });
			}
			paragraph("Connected streams not covered:");
			table({ "Entity", "Input stream", "Plane", "Talker" }, rows);
		}
		if (_results.streamDataLosses > 0u)
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& stream : _results.monitoredStreams)
			{
				if (stream.losses == 0u || rows.size() >= options.rankingSize)
				{
					break;
				}
				rows.push_back({ name(stream.entityID), stream.subject, planeName(stream.plane), QString::number(stream.losses), duration(stream.totalMissingMs), stream.talker ? name(stream.talker->entityID) : QString{}, eventTime(stream.events.front()) });
			}
			table({ "Entity", "Stream", "Plane", "Losses", "Missing", "Talker", "First" }, rows);
		}
		if (!_results.streamDataBothPlanes.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& pair : _results.streamDataBothPlanes)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ eventTime(pair.events.front()), name(pair.entityID), duration(pair.delayMs) });
			}
			paragraph("Losses on both planes of the same entity within the same second:");
			table({ "Time", "Entity", "Delay" }, rows);
		}
		if (!_results.longInterruptions.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& interruption : _results.longInterruptions)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				rows.push_back({ eventTime(interruption.resumeEvent), name(interruption.entityID), interruption.subject, duration(interruption.elapsedMs), interruption.startEvent ? eventTime(*interruption.startEvent) : QString{ "not journaled" } });
			}
			paragraph("Long interruptions (the resume edge, with the silence duration and the start edge when the journal has it):");
			table({ "Resumed at", "Entity", "Stream", "Silence", "Started at" }, rows);
		}
		if (!_results.streamDataIncidents.empty())
		{
			auto rows = std::vector<QStringList>{};
			for (auto const& incident : _results.streamDataIncidents)
			{
				if (rows.size() >= options.listingSize)
				{
					break;
				}
				auto localization = QString{};
				if (incident.listeners.size() == 1u)
				{
					localization = "single listener: its own link";
				}
				else if (incident.commonHop)
				{
					localization = QString{ "common hop %1" }.arg(identifier(*incident.commonHop));
					localization += incident.sparedBehindHop.empty() ? QString{ ", every listener behind it was hit" } : QString{ ", %1 listeners behind it were not hit (%2)" }.arg(incident.sparedBehindHop.size()).arg(entitiesLabel(incident.sparedBehindHop, 4u));
				}
				else
				{
					localization = "topology unknown";
				}
				rows.push_back({ eventTime(incident.events.front()), planeName(incident.plane), QString{ "%1 (output stream %2)" }.arg(name(incident.talker.entityID)).arg(incident.talker.streamIndex), QString{ "%1 of %2 (%3)" }.arg(incident.listeners.size()).arg(incident.talkerListeners).arg(entitiesLabel(incident.listeners, 4u)), localization });
			}
			paragraph(QString{ "Incidents (the listeners of one talker stream hit within %1): all the listeners of a talker hit together points at the talker or its uplink, one listener alone at its own link, and the deepest switch common to the listeners hit names the switch or port that dropped the frames." }.arg(duration(options.hitWindowMs)));
			table({ "Time", "Plane", "Talker", "Listeners hit", "Localization" }, rows);
		}
	}

	void renderMethod()
	{
		heading(2, "How this analysis was produced");
		auto const& options = _analysis.options;
		paragraph("Computed by the Journal Analyser from the events of the journal only, with the following rules:");
		bullet("Stream events are attributed to a plane from the AVB interface of the stream (the topology) or, failing that, from the naming of the stream (Primary / Secondary); entities are mapped to their rack and branch switch from the gPTP AsPath of the 'Entity is online' details or of a loaded network state file.");
		bullet(QString{ "Hits: media unlock events of the same entity and plane less than %1 apart. Incidents: hits of the entities of the same rack and plane less than %1 apart. Storms: incidents of the same rack and plane less than %2 apart. Runs: storms of the same branch and plane less than %3 apart, racks in order of first appearance." }.arg(duration(options.hitWindowMs), duration(options.stormGapMs), duration(options.runGapMs)));
		bullet(QString{ "System wide events: entities touched (stream level or clock domain unlock) less than %1 apart, at least %2 % of the entities reporting hits in the session (%3 at least) across several racks, extended by the hits of the same entities within %4 of the end (the relock wave); their incidents are excluded from the storms and runs." }.arg(duration(options.systemWideGapMs)).arg(static_cast<int>(options.systemWideMinShare * 100.0)).arg(options.systemWideMinEntities).arg(duration(options.systemWideTailMs)));
		bullet(QString{ "Both planes: a primary and a secondary hit of the same entity less than %1 apart, simultaneous below %2. Accompanying events: within %3 of each other on the same entity (or stream)." }.arg(duration(options.bothPlanesWindowMs), duration(options.simultaneousMs), duration(options.accompanyingWindowMs)));
		bullet(QString{ "Durations: an error event paired with the recovered event of the same entity and subject; pairs inferred from the counters have no duration. Grandmaster storms: changes less than %1 apart. Waves: entities online (or offline) less than %2 apart, %3 at least. Long interruptions: stream data losses whose elapsed time exceeds %4." }.arg(duration(options.grandmasterStormGapMs), duration(options.waveGapMs)).arg(options.waveMinEntities).arg(duration(options.longInterruptionMs)));
		bullet("Event order is the order in which notifications reached the controller (a few ms of jitter): no causality is inferred from the order of events less than about 100 ms apart, and a journal recorded before an event family existed has no figure for it.");
		endList();
	}

	JournalAnalysis::Private const& _analysis;
	Results const& _results;
	EventJournal::Session const& _session;
	JournalAnalysis::ReportOptions const& _options;
	bool _singleDay{ true };
	std::map<std::pair<EntityID, Plane>, JournalAnalysis::EntityIDs> _rackMembers{};
	QString _out{};
};
} // namespace

QString JournalAnalysis::markdownReport(EventJournal::Session const& session, ReportOptions const& options) const
{
	if (session.events.empty())
	{
		return QString{ "# %1\n\nThe journal has no event.\n" }.arg(options.title.isEmpty() ? QString{ "Analysis of the Event Journal" } : options.title);
	}
	return Renderer{ *_pImpl, session, options }.render();
}

QString JournalAnalysis::markdownReport(EventJournal::Session const& session) const
{
	return markdownReport(session, ReportOptions{});
}

} // namespace modelsLibrary
} // namespace hive
