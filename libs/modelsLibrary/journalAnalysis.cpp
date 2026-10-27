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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>

namespace hive
{
namespace modelsLibrary
{
namespace
{
using EntityID = JournalAnalysis::EntityID;
using Plane = JournalAnalysis::Plane;
using Kind = JournalAnalysis::Private::Kind;
using Info = JournalAnalysis::Private::Info;

/** Parses an identifier written by helper::uniqueIdentifierToString (0x prefixed hexadecimal), returning an invalid identifier on error. */
EntityID identifierFromString(QString const& text) noexcept
{
	auto ok = false;
	auto const value = text.toULongLong(&ok, 16);
	return ok ? EntityID{ value } : EntityID{};
}

/** Parses an identifier from a JSON string value (nlohmann), returning an invalid identifier on error. */
EntityID identifierFromJson(nlohmann::json const& value) noexcept
{
	if (!value.is_string())
	{
		return {};
	}
	return identifierFromString(QString::fromStdString(value.get<std::string>()));
}

/** Parses the number following a prefix in a subject ("Input Stream 12 (name)" gives 12 for the prefix "Input Stream "), or std::nullopt. */
std::optional<int> numberAfter(QString const& subject, QLatin1String const prefix) noexcept
{
	if (!subject.startsWith(prefix))
	{
		return std::nullopt;
	}
	auto end = prefix.size();
	while (end < subject.size() && subject[end].isDigit())
	{
		++end;
	}
	if (end == prefix.size())
	{
		return std::nullopt;
	}
	return subject.mid(prefix.size(), end - prefix.size()).toInt();
}

/** Returns the plane named by the interface type names used in the events ("Primary" / "Secondary"), or UnknownPlane. */
Plane planeFromInterfaceName(QString const& name) noexcept
{
	if (name.compare(QLatin1String{ "Primary" }, Qt::CaseInsensitive) == 0)
	{
		return 0;
	}
	if (name.compare(QLatin1String{ "Secondary" }, Qt::CaseInsensitive) == 0)
	{
		return 1;
	}
	return JournalAnalysis::UnknownPlane;
}

/** Returns the plane a stream name designates ("Audio Primary Stream 1", "Media Clock Secondary Input Stream"), or UnknownPlane: the fallback when the topology does not know the stream. */
Plane planeFromStreamName(QString const& name) noexcept
{
	auto const hasPrimary = name.contains(QLatin1String{ "primary" }, Qt::CaseInsensitive);
	auto const hasSecondary = name.contains(QLatin1String{ "secondary" }, Qt::CaseInsensitive);
	if (hasPrimary && !hasSecondary)
	{
		return 0;
	}
	if (hasSecondary && !hasPrimary)
	{
		return 1;
	}
	return JournalAnalysis::UnknownPlane;
}

/** Stream input error counters that are packet problems (loss, link down, talker stopped), as opposed to the timing family. */
bool isPacketLossCounter(QString const& counter) noexcept
{
	return counter == "Stream Interrupted" || counter == "Sequence Number Mismatch" || counter == "Late Timestamp" || counter == "Early Timestamp" || counter == "Unsupported Format";
}

/** Returns the counter name quoted at the beginning of a counter summary ("'Media Unlocked' error counter increased to 5"), or an empty string. */
QString quotedCounterName(QString const& summary) noexcept
{
	if (!summary.startsWith('\''))
	{
		return {};
	}
	auto const end = summary.indexOf('\'', 1);
	if (end < 0)
	{
		return {};
	}
	return summary.mid(1, end - 1);
}

/** Median of a sorted vector (0 when empty). */
qint64 medianOf(std::vector<qint64> const& sorted) noexcept
{
	if (sorted.empty())
	{
		return 0;
	}
	return sorted[sorted.size() / 2u];
}

/** Computes the distribution of the known durations of intervals. */
JournalAnalysis::DurationStats durationStats(std::vector<JournalAnalysis::Interval> const& intervals, std::function<bool(JournalAnalysis::Interval const&)> const& filter) noexcept
{
	auto stats = JournalAnalysis::DurationStats{};
	auto durations = std::vector<qint64>{};
	for (auto const& interval : intervals)
	{
		if (!filter(interval))
		{
			continue;
		}
		if (interval.durationMs)
		{
			durations.push_back(*interval.durationMs);
		}
		else
		{
			++stats.unknown;
		}
	}
	if (!durations.empty())
	{
		std::sort(durations.begin(), durations.end());
		stats.count = durations.size();
		stats.minMs = durations.front();
		stats.maxMs = durations.back();
		stats.medianMs = medianOf(durations);
		for (auto const duration : durations)
		{
			stats.totalMs += duration;
		}
	}
	return stats;
}

/** Appends an identifier to a vector if not already present (order of appearance lists). */
void appendUnique(JournalAnalysis::EntityIDs& list, EntityID const entityID) noexcept
{
	if (std::find(list.begin(), list.end(), entityID) == list.end())
	{
		list.push_back(entityID);
	}
}

/** Descriptor arrays of a configuration in an AVE document. */
constexpr auto EntityDescriptorPointer = "/entity_model/entity_descriptor";
constexpr auto EntityIDPointer = "/adp_information/common/entity_id";
constexpr auto ControllerCapabilitiesPointer = "/adp_information/common/controller_capabilities";

/** Returns the node at a JSON pointer, or a null json on error. */
nlohmann::json const& nodeAt(nlohmann::json const& document, char const* const pointer) noexcept
{
	static auto const s_null = nlohmann::json{};
	try
	{
		return document.at(nlohmann::json::json_pointer{ pointer });
	}
	catch (...)
	{
		return s_null;
	}
}

/** Returns the descriptor index of an entry of a descriptors array (the position, unless the informative index key says otherwise). */
la::avdecc::entity::model::DescriptorIndex descriptorIndexOf(nlohmann::json const& descriptor, std::size_t const position) noexcept
{
	if (auto const it = descriptor.find("_index (informative)"); it != descriptor.end() && it->is_number_unsigned())
	{
		return it->get<la::avdecc::entity::model::DescriptorIndex>();
	}
	return static_cast<la::avdecc::entity::model::DescriptorIndex>(position);
}
} // namespace

/* ************************************************************ */
/* Topology                                                     */
/* ************************************************************ */
bool JournalAnalysis::Topology::hasAsPaths() const noexcept
{
	for (auto const& [entityID, entity] : entities)
	{
		for (auto const& [index, interface] : entity.interfaces)
		{
			if (!interface.asPath.empty())
			{
				return true;
			}
		}
	}
	return false;
}

bool JournalAnalysis::Topology::Interface::hasInconsistentClockIdentity() const noexcept
{
	// The AsPath starts with the grandmaster: an interface whose AsPath has more than one element is not the grandmaster, so it cannot have the identity of the first element
	return asPath.size() >= 2u && clockIdentity && clockIdentity == asPath.front() && clockIdentity != asPath.back();
}

bool JournalAnalysis::Topology::Interface::hasInconsistentAsPath() const noexcept
{
	// The AsPath starts with the grandmaster
	return asPath.size() >= 2u && grandmasterID && asPath.front() != grandmasterID;
}

EntityID JournalAnalysis::Topology::clockIdentity(EntityID const entityID, Plane const plane) const noexcept
{
	if (plane < 0)
	{
		return {};
	}
	auto const entityIt = entities.find(entityID);
	if (entityIt == entities.end())
	{
		return {};
	}
	auto const interfaceIt = entityIt->second.interfaces.find(static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(plane));
	if (interfaceIt == entityIt->second.interfaces.end())
	{
		return {};
	}
	auto const& interface = interfaceIt->second;
	if (!interface.hasInconsistentClockIdentity())
	{
		return interface.clockIdentity;
	}
	// Most implementations end the AsPath with their own clock identity. For the ones that don't, the last element is their upstream neighbor: it is detected when that neighbor is another entity (the clock identity then remains unknown), but not when it is a switch
	auto const& lastClockIdentity = interface.asPath.back();
	for (auto const& [otherEntityID, otherEntity] : entities)
	{
		for (auto const& [otherIndex, otherInterface] : otherEntity.interfaces)
		{
			if (otherInterface.clockIdentity == lastClockIdentity)
			{
				return {};
			}
		}
	}
	return lastClockIdentity;
}

namespace
{
// Hops above an element of the up to date AsPaths of a plane: the elements before it in the AsPath of an interface (the one of the lowest EntityID, for a deterministic result)
struct UpstreamHops
{
	EntityID entityID{};
	std::vector<EntityID> hops{};
};
using UpstreamHopsMap = std::map<std::pair<EntityID, EntityID>, UpstreamHops>; /**< Key: grandmaster, AsPath element */

UpstreamHopsMap upstreamHopsOfPlane(JournalAnalysis::Topology const& topology, Plane const plane) noexcept
{
	auto result = UpstreamHopsMap{};
	for (auto const& [entityID, entity] : topology.entities)
	{
		auto const interfaceIt = entity.interfaces.find(static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(plane));
		if (interfaceIt == entity.interfaces.end() || !interfaceIt->second.grandmasterID || interfaceIt->second.hasInconsistentAsPath())
		{
			continue;
		}
		auto const& asPath = interfaceIt->second.asPath;
		for (auto position = std::size_t{ 1u }; position < asPath.size(); ++position)
		{
			auto& upstreamHops = result[std::make_pair(interfaceIt->second.grandmasterID, asPath[position])];
			if (upstreamHops.hops.empty() || entityID < upstreamHops.entityID)
			{
				upstreamHops = UpstreamHops{ entityID, std::vector<EntityID>{ asPath.begin(), asPath.begin() + static_cast<std::ptrdiff_t>(position) } };
			}
		}
	}
	return result;
}

// The AsPath of an interface without the entity's own clock identity
std::vector<EntityID> asPathHops(JournalAnalysis::Topology const& topology, EntityID const entityID, Plane const plane, JournalAnalysis::Topology::Interface const& interface) noexcept
{
	auto hops = interface.asPath;
	// Most implementations end the AsPath with the entity's own clock identity (the deduced one when the reported one is inconsistent: it would otherwise be taken as the switch the entity is plugged into)
	if (auto const ownClockIdentity = topology.clockIdentity(entityID, plane); !hops.empty() && ownClockIdentity && hops.back() == ownClockIdentity)
	{
		hops.pop_back();
	}
	return hops;
}

// An inconsistent AsPath (see Interface::hasInconsistentAsPath) misses the hops above its first element, as it was most probably captured before a grandmaster change (the network above that element changed since):
// they are taken from an up to date AsPath going through that element, so the hops can be compared with the other ones (branch switch, upstream switches, deepest switch common to the listeners of an incident)
void completeInconsistentHops(std::vector<EntityID>& hops, JournalAnalysis::Topology::Interface const& interface, UpstreamHopsMap const& upstreamHops) noexcept
{
	if (hops.empty())
	{
		return;
	}
	if (auto const it = upstreamHops.find(std::make_pair(interface.grandmasterID, hops.front())); it != upstreamHops.end())
	{
		hops.insert(hops.begin(), it->second.hops.begin(), it->second.hops.end());
	}
}
} // namespace

std::vector<EntityID> JournalAnalysis::Topology::hops(EntityID const entityID, Plane const plane) const noexcept
{
	if (plane < 0)
	{
		return {};
	}
	auto const entityIt = entities.find(entityID);
	if (entityIt == entities.end())
	{
		return {};
	}
	auto const interfaceIt = entityIt->second.interfaces.find(static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(plane));
	if (interfaceIt == entityIt->second.interfaces.end())
	{
		return {};
	}
	auto const& interface = interfaceIt->second;
	auto hops = asPathHops(*this, entityID, plane, interface);
	if (interface.hasInconsistentAsPath())
	{
		completeInconsistentHops(hops, interface, upstreamHopsOfPlane(*this, plane));
	}
	return hops;
}

std::vector<EntityID> JournalAnalysis::Topology::upstreamSwitches(Plane const plane) const noexcept
{
	if (plane < 0)
	{
		return {};
	}
	// Same hops than the ones of hops(), the up to date AsPaths of the plane being indexed once for all the interfaces
	auto const upstreamHops = upstreamHopsOfPlane(*this, plane);
	auto switches = std::set<EntityID>{};
	for (auto const& [entityID, entity] : entities)
	{
		auto const interfaceIt = entity.interfaces.find(static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(plane));
		if (interfaceIt == entity.interfaces.end())
		{
			continue;
		}
		auto path = asPathHops(*this, entityID, plane, interfaceIt->second);
		if (interfaceIt->second.hasInconsistentAsPath())
		{
			completeInconsistentHops(path, interfaceIt->second, upstreamHops);
		}
		for (auto i = std::size_t{ 0u }; i + 1u < path.size(); ++i)
		{
			switches.insert(path[i]);
		}
	}
	return std::vector<EntityID>{ switches.begin(), switches.end() };
}

std::optional<EntityID> JournalAnalysis::Topology::rackSwitch(EntityID const entityID, Plane const plane) const noexcept
{
	auto const path = hops(entityID, plane);
	if (path.empty())
	{
		return std::nullopt;
	}
	auto const upstream = upstreamSwitches(plane);
	if (std::find(upstream.begin(), upstream.end(), path.back()) != upstream.end())
	{
		// Plugged directly into a switch serving other racks: a rack of one, identified by the entity (its clock identity when known, since racks are clock identities)
		auto const ownClockIdentity = clockIdentity(entityID, plane);
		return ownClockIdentity ? ownClockIdentity : entityID;
	}
	return path.back();
}

std::optional<EntityID> JournalAnalysis::Topology::branchSwitch(EntityID const entityID, Plane const plane) const noexcept
{
	auto const path = hops(entityID, plane);
	if (path.empty())
	{
		return std::nullopt;
	}
	auto const upstream = upstreamSwitches(plane);
	if (std::find(upstream.begin(), upstream.end(), path.back()) != upstream.end())
	{
		return path.back();
	}
	if (path.size() < 2u)
	{
		return std::nullopt;
	}
	return path[path.size() - 2u];
}

Plane JournalAnalysis::Topology::streamPlane(EntityID const entityID, la::avdecc::entity::model::StreamIndex const streamIndex) const noexcept
{
	auto const entityIt = entities.find(entityID);
	if (entityIt == entities.end())
	{
		return UnknownPlane;
	}
	auto const streamIt = entityIt->second.streamInputs.find(streamIndex);
	if (streamIt == entityIt->second.streamInputs.end())
	{
		return UnknownPlane;
	}
	return streamIt->second.plane;
}

std::size_t JournalAnalysis::Topology::addSession(EventJournal::Session const& session) noexcept
{
	auto described = std::unordered_set<EntityID, EntityID::hash>{};
	for (auto const& event : session.events)
	{
		if (!event.entityID || event.details.isEmpty())
		{
			continue;
		}
		if (event.category == EventJournal::Category::Entity && event.summary == "Entity is online")
		{
			auto const details = QJsonDocument::fromJson(event.details.toUtf8()).object();
			auto& entity = entities[event.entityID];
			entity.name = event.entityName;
			entity.isController = details.value("is_controller").toBool(entity.isController);
			for (auto const& value : details.value("avb_interfaces").toArray())
			{
				auto const object = value.toObject();
				auto& interface = entity.interfaces[static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(object.value("index").toInt())];
				interface.clockIdentity = identifierFromString(object.value("clock_identity").toString());
				interface.grandmasterID = identifierFromString(object.value("grandmaster_id").toString());
				interface.asPath.clear();
				for (auto const& hop : object.value("as_path").toArray())
				{
					interface.asPath.push_back(identifierFromString(hop.toString()));
				}
			}
			for (auto const& value : details.value("clock_domains").toArray())
			{
				auto const object = value.toObject();
				if (object.contains("clock_source_stream_index"))
				{
					entity.clockSourceStreams[static_cast<la::avdecc::entity::model::ClockDomainIndex>(object.value("index").toInt())] = static_cast<la::avdecc::entity::model::StreamIndex>(object.value("clock_source_stream_index").toInt());
				}
			}
			for (auto const& value : details.value("stream_inputs").toArray())
			{
				auto const object = value.toObject();
				auto& stream = entity.streamInputs[static_cast<la::avdecc::entity::model::StreamIndex>(object.value("index").toInt())];
				if (object.contains("avb_interface_index"))
				{
					stream.plane = object.value("avb_interface_index").toInt();
				}
				else if (stream.plane == UnknownPlane)
				{
					stream.plane = planeFromStreamName(object.value("name").toString());
				}
				stream.talker = la::avdecc::entity::model::StreamIdentification{ identifierFromString(object.value("talker_entity_id").toString()), static_cast<la::avdecc::entity::model::StreamIndex>(object.value("talker_stream_index").toInt()) };
			}
			if (!details.value("avb_interfaces").toArray().isEmpty())
			{
				described.insert(event.entityID);
			}
		}
		else if (event.category == EventJournal::Category::Gptp && event.summary == "gPTP AsPath changed")
		{
			auto const index = numberAfter(event.subject, QLatin1String{ "AVB Interface " });
			auto const entityIt = entities.find(event.entityID);
			if (index && entityIt != entities.end())
			{
				auto const interfaceIt = entityIt->second.interfaces.find(static_cast<la::avdecc::entity::model::AvbInterfaceIndex>(*index));
				if (interfaceIt != entityIt->second.interfaces.end())
				{
					auto const details = QJsonDocument::fromJson(event.details.toUtf8()).object();
					interfaceIt->second.asPath.clear();
					for (auto const& hop : details.value("new_as_path").toArray())
					{
						interfaceIt->second.asPath.push_back(identifierFromString(hop.toString()));
					}
				}
			}
		}
		else if (event.category == EventJournal::Category::Connection && event.summary.startsWith(QLatin1String{ "Connected to talker" }))
		{
			auto const index = numberAfter(event.subject, QLatin1String{ "Input Stream " });
			if (index)
			{
				auto const details = QJsonDocument::fromJson(event.details.toUtf8()).object();
				auto& entity = entities[event.entityID];
				if (entity.name.isEmpty())
				{
					entity.name = event.entityName;
				}
				auto& stream = entity.streamInputs[static_cast<la::avdecc::entity::model::StreamIndex>(*index)];
				if (details.contains("avb_interface_index"))
				{
					stream.plane = details.value("avb_interface_index").toInt();
				}
				else if (stream.plane == UnknownPlane)
				{
					stream.plane = planeFromStreamName(event.subject);
				}
				stream.talker = la::avdecc::entity::model::StreamIdentification{ identifierFromString(details.value("talker_id").toString()), static_cast<la::avdecc::entity::model::StreamIndex>(details.value("talker_stream_index").toInt()) };
			}
		}
	}
	return described.size();
}

bool JournalAnalysis::Topology::addEntityDocument(nlohmann::json const& document) noexcept
{
	try
	{
		auto const entityID = identifierFromJson(nodeAt(document, EntityIDPointer));
		if (!entityID)
		{
			return false;
		}
		auto& entity = entities[entityID];
		auto const& entityDescriptor = nodeAt(document, EntityDescriptorPointer);
		if (auto const& name = nodeAt(entityDescriptor, "/dynamic/entity_name"); name.is_string())
		{
			entity.name = QString::fromStdString(name.get<std::string>());
		}
		if (auto const& capabilities = nodeAt(document, ControllerCapabilitiesPointer); capabilities.is_array())
		{
			for (auto const& capability : capabilities)
			{
				if (capability.is_string() && capability.get<std::string>() == "IMPLEMENTED")
				{
					entity.isController = true;
				}
			}
		}
		auto configurationIndex = std::size_t{ 0u };
		if (auto const& current = nodeAt(entityDescriptor, "/dynamic/current_configuration"); current.is_number_unsigned())
		{
			configurationIndex = current.get<std::size_t>();
		}
		auto const& configurations = nodeAt(entityDescriptor, "/configuration_descriptors");
		if (!configurations.is_array() || configurationIndex >= configurations.size())
		{
			return true;
		}
		auto const& configuration = configurations[configurationIndex];

		if (auto const& interfaces = nodeAt(configuration, "/avb_interface_descriptors"); interfaces.is_array())
		{
			for (auto position = std::size_t{ 0u }; position < interfaces.size(); ++position)
			{
				auto const& descriptor = interfaces[position];
				auto& interface = entity.interfaces[descriptorIndexOf(descriptor, position)];
				interface.clockIdentity = identifierFromJson(nodeAt(descriptor, "/dynamic/clock_identity"));
				interface.grandmasterID = identifierFromJson(nodeAt(descriptor, "/dynamic/gptp_grandmaster_id"));
				if (auto const& asPath = nodeAt(descriptor, "/dynamic/as_path"); asPath.is_array())
				{
					interface.asPath.clear();
					for (auto const& hop : asPath)
					{
						interface.asPath.push_back(identifierFromJson(hop));
					}
				}
			}
		}
		if (auto const& streams = nodeAt(configuration, "/stream_input_descriptors"); streams.is_array())
		{
			for (auto position = std::size_t{ 0u }; position < streams.size(); ++position)
			{
				auto const& descriptor = streams[position];
				auto& stream = entity.streamInputs[descriptorIndexOf(descriptor, position)];
				if (auto const& index = nodeAt(descriptor, "/static/avb_interface_index"); index.is_number_unsigned())
				{
					stream.plane = index.get<int>();
				}
				auto const& state = nodeAt(descriptor, "/dynamic/connection_state");
				if (state.is_string() && state.get<std::string>() == "CONNECTED")
				{
					auto const talkerID = identifierFromJson(nodeAt(descriptor, "/dynamic/connected_talker/entity_id"));
					auto const& talkerIndex = nodeAt(descriptor, "/dynamic/connected_talker/stream_index");
					if (talkerID && talkerIndex.is_number_unsigned())
					{
						stream.talker = la::avdecc::entity::model::StreamIdentification{ talkerID, talkerIndex.get<la::avdecc::entity::model::StreamIndex>() };
					}
				}
			}
		}
		auto const& clockSources = nodeAt(configuration, "/clock_source_descriptors");
		if (auto const& domains = nodeAt(configuration, "/clock_domain_descriptors"); domains.is_array() && clockSources.is_array())
		{
			for (auto position = std::size_t{ 0u }; position < domains.size(); ++position)
			{
				auto const& descriptor = domains[position];
				auto const& sourceIndex = nodeAt(descriptor, "/dynamic/clock_source_index");
				if (!sourceIndex.is_number_unsigned() || sourceIndex.get<std::size_t>() >= clockSources.size())
				{
					continue;
				}
				auto const& source = clockSources[sourceIndex.get<std::size_t>()];
				auto const& locationType = nodeAt(source, "/static/clock_source_location_type");
				auto const& locationIndex = nodeAt(source, "/static/clock_source_location_index");
				if (locationType.is_string() && locationType.get<std::string>() == "STREAM_INPUT" && locationIndex.is_number_unsigned())
				{
					entity.clockSourceStreams[descriptorIndexOf(descriptor, position)] = locationIndex.get<la::avdecc::entity::model::StreamIndex>();
				}
			}
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

std::size_t JournalAnalysis::Topology::addNetworkState(nlohmann::json const& document) noexcept
{
	auto count = std::size_t{ 0u };
	try
	{
		auto const it = document.find("entities");
		if (it == document.end() || !it->is_array())
		{
			return 0u;
		}
		for (auto const& entity : *it)
		{
			if (addEntityDocument(entity))
			{
				++count;
			}
		}
	}
	catch (...)
	{
		// Not a network state document
	}
	return count;
}

/* ************************************************************ */
/* JournalAnalysis                                              */
/* ************************************************************ */
QString JournalAnalysis::planeName(Plane const plane) noexcept
{
	switch (plane)
	{
		case 0:
			return "Primary";
		case 1:
			return "Secondary";
		case UnknownPlane:
			return "Unknown";
		default:
			return QString{ "Interface %1" }.arg(plane);
	}
}

JournalAnalysis::JournalAnalysis(EventJournal::Session const& session, Topology const& topology, Options const& options)
	: _pImpl{ std::make_unique<Private>(session, topology, options) }
{
}

JournalAnalysis::JournalAnalysis(EventJournal::Session const& session, Topology const& topology)
	: JournalAnalysis{ session, topology, Options{} }
{
}

JournalAnalysis::~JournalAnalysis() noexcept = default;

JournalAnalysis::Results const& JournalAnalysis::results() const noexcept
{
	return _pImpl->results;
}

JournalAnalysis::Topology const& JournalAnalysis::topology() const noexcept
{
	return _pImpl->topology;
}

JournalAnalysis::Options const& JournalAnalysis::options() const noexcept
{
	return _pImpl->options;
}

QString JournalAnalysis::entityName(EntityID const entityID) const noexcept
{
	return _pImpl->entityName(entityID);
}

Plane JournalAnalysis::eventPlane(EventJournal::Event const& event) const noexcept
{
	return _pImpl->eventPlane(event, Private::parse(event).detailsPlane);
}

/* ************************************************************ */
/* JournalAnalysis::Private                                     */
/* ************************************************************ */
JournalAnalysis::Private::Private(EventJournal::Session const& session, Topology const& topo, Options const& opts)
	: topology{ topo }
	, options{ opts }
{
	auto const& events = session.events;
	infos.reserve(events.size());
	for (auto const& event : events)
	{
		infos.push_back(parse(event));
	}
	// Names: the last one the entity had in the session (the topology completes the entities that never had an event)
	for (auto const& event : events)
	{
		if (event.entityID && !event.entityName.isEmpty())
		{
			names[event.entityID] = event.entityName;
		}
	}
	for (auto const& [entityID, entity] : topology.entities)
	{
		if (names.count(entityID) == 0u && !entity.name.isEmpty())
		{
			names[entityID] = entity.name;
		}
	}
	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		infos[i].plane = eventPlane(events[i], infos[i].detailsPlane);
	}

	if (events.empty())
	{
		return;
	}
	results.firstTimestamp = events.front().timestamp;
	results.lastTimestamp = events.back().timestamp;

	computeSessionShape(events);
	computeBreakdown(events);
	computeHits(events);
	computeIntervals(events);
	computeSystemWideEvents(events);
	computeIncidents(events);
	computeStorms();
	computeRuns();
	computeRankings();
	computeBothPlanes();
	computeClockDomain(events);
	computeOtherFamilies(events);
	computeStreamData(events);
}

QString JournalAnalysis::Private::entityName(EntityID const entityID) const noexcept
{
	if (auto const it = names.find(entityID); it != names.end())
	{
		return it->second;
	}
	return helper::uniqueIdentifierToString(entityID);
}

Info JournalAnalysis::Private::parse(EventJournal::Event const& event) noexcept
{
	using Category = EventJournal::Category;
	using Severity = EventJournal::Severity;
	using Level = EventJournal::Level;

	auto info = Info{};
	auto const details = [&event]()
	{
		return event.details.isEmpty() ? QJsonObject{} : QJsonDocument::fromJson(event.details.toUtf8()).object();
	};
	auto const& summary = event.summary;

	switch (event.category)
	{
		case Category::Session:
			if (summary == "Heartbeat")
			{
				info.kind = Kind::Heartbeat;
				auto const object = details();
				info.value = object.value("event_loop_latency_ms").toInteger();
				info.secondaryValue = object.value("entities_online").toInteger();
			}
			else if (summary == "Recording started")
			{
				info.kind = Kind::RecordingStarted;
			}
			else if (summary == "Recording stopped")
			{
				info.kind = Kind::RecordingStopped;
			}
			else if (summary == "Controller stopped")
			{
				info.kind = Kind::ControllerStopped;
			}
			else if (summary == "Controller restarted")
			{
				info.kind = Kind::ControllerRestarted;
			}
			else
			{
				info.kind = Kind::SessionOther;
			}
			break;
		case Category::Marker:
			info.kind = Kind::Marker;
			break;
		case Category::Entity:
			if (summary == "Entity is online")
			{
				info.kind = Kind::EntityOnline;
				info.flag = details().value("is_controller").toBool();
			}
			else if (summary == "Entity went offline")
			{
				info.kind = Kind::EntityOffline;
			}
			break;
		case Category::Control:
			info.kind = summary == "Entity refresh requested" ? Kind::RefreshRequested : Kind::OperatorAction;
			break;
		case Category::Configuration:
			info.kind = event.level == Level::User ? Kind::OperatorAction : Kind::ChangeTracking;
			break;
		case Category::Connection:
			info.kind = event.level == Level::User ? Kind::Connection : Kind::ChangeTracking;
			break;
		case Category::Counters:
			if (summary == "Media unlocked")
			{
				info.kind = Kind::MediaUnlock;
				info.flag = details().value("inferred").toBool();
				info.isPaired = true;
			}
			else if (summary == "Media locked")
			{
				if (event.severity == Severity::Recovered)
				{
					info.kind = Kind::MediaLock;
					info.flag = details().value("inferred").toBool();
				}
			}
			else if (summary == "Counters updated")
			{
				info.kind = Kind::ChangeTracking;
			}
			else if (summary.startsWith('\''))
			{
				info.name = quotedCounterName(summary);
				if (event.subject == "Statistics")
				{
					info.kind = Kind::ControllerCounter;
					info.detailsPlane = planeFromInterfaceName(details().value("interface").toString());
					// The unsolicited notification loss events tell how many notifications were actually lost (the counter itself only counts the detections)
					info.increment = details().value("lost_count").toInteger();
				}
				else if (event.severity == Severity::Error && info.name == "Media Unlocked")
				{
					// Journals recorded before the 'Media unlocked' / 'Media locked' pairs
					info.kind = Kind::MediaUnlock;
				}
				else if (event.severity == Severity::Error && isPacketLossCounter(info.name))
				{
					info.kind = Kind::PacketLoss;
				}
				else if (event.level == Level::Diagnostic)
				{
					info.kind = Kind::ChangeTracking;
				}
			}
			break;
		case Category::MediaClock:
			if (summary == "Media clock unlocked")
			{
				info.kind = Kind::ClockUnlock;
				info.flag = details().value("inferred").toBool();
			}
			else if (summary == "Media clock locked")
			{
				info.kind = Kind::ClockLock;
				info.flag = details().value("inferred").toBool();
			}
			break;
		case Category::Gptp:
			if (summary.startsWith(QLatin1String{ "gPTP grandmaster changed" }))
			{
				info.kind = Kind::GrandmasterChange;
				auto const object = details();
				info.from = identifierFromString(object.value("previous_grandmaster_id").toString());
				info.to = identifierFromString(object.value("new_grandmaster_id").toString());
			}
			else if (summary == "gPTP AsPath changed")
			{
				info.kind = Kind::AsPathChange;
			}
			else if (summary.startsWith(QLatin1String{ "'GPTP GM Changed'" }))
			{
				info.kind = Kind::GrandmasterCounter;
			}
			else if (summary.startsWith(QLatin1String{ "gPTP propagation delay became null" }))
			{
				info.kind = Kind::PropagationDelayNull;
			}
			else if (summary.startsWith(QLatin1String{ "gPTP propagation delay measured" }))
			{
				info.kind = Kind::PropagationDelayMeasured;
			}
			else if (event.level == Level::Diagnostic)
			{
				info.kind = Kind::ChangeTracking;
			}
			break;
		case Category::Link:
			if (!event.entityID)
			{
				info.kind = Kind::ControllerInterface;
			}
			else if (summary == "Link down")
			{
				info.kind = Kind::LinkDown;
				info.flag = details().value("inferred").toBool();
			}
			else if (summary == "Link up")
			{
				info.kind = Kind::LinkUp;
				info.flag = details().value("inferred").toBool();
			}
			break;
		case Category::Latency:
			if (event.severity == Severity::Error)
			{
				info.kind = Kind::Latency;
			}
			break;
		case Category::Redundancy:
			if (summary.startsWith(QLatin1String{ "Entity is offline on one redundant interface" }))
			{
				info.kind = Kind::OfflineOnOneInterface;
			}
			else if (summary.startsWith(QLatin1String{ "Entity is back online on the redundant interface" }))
			{
				info.kind = Kind::BackOnlineOnInterface;
			}
			else if (summary.startsWith(QLatin1String{ "No longer receiving unsolicited notifications" }))
			{
				info.kind = Kind::UnsolicitedLoss;
			}
			else if (summary.startsWith(QLatin1String{ "Receiving unsolicited notifications again" }))
			{
				info.kind = Kind::UnsolicitedBack;
			}
			else if (summary.startsWith(QLatin1String{ "Transport error" }))
			{
				info.kind = Kind::TransportError;
			}
			else if (summary.startsWith(QLatin1String{ "Redundancy warning" }))
			{
				info.kind = Kind::RedundancyWarning;
			}
			break;
		case Category::Streaming:
			if (event.severity == Severity::Error)
			{
				info.kind = Kind::StreamDataLoss;
				auto const object = details();
				info.value = object.value("missing_ms").toInteger();
				info.secondaryValue = object.value("elapsed_ms").toInteger();
				info.increment = object.value("increment").toInteger();
				info.expected = object.value("expected_increment").toInteger();
				info.from = identifierFromString(object.value("talker_id").toString());
				info.talkerStreamIndex = object.value("talker_stream_index").toInt(-1);
			}
			else if (summary == "Stream data monitoring started")
			{
				info.kind = Kind::StreamDataMonitoring;
				auto const object = details();
				info.expected = object.value("expected_increment").toInteger();
				info.from = identifierFromString(object.value("talker_id").toString());
				info.talkerStreamIndex = object.value("talker_stream_index").toInt(-1);
			}
			break;
		default:
			break;
	}
	return info;
}

Plane JournalAnalysis::Private::eventPlane(EventJournal::Event const& event, Plane const detailsPlane) const noexcept
{
	auto const& subject = event.subject;
	if (auto const index = numberAfter(subject, QLatin1String{ "AVB Interface " }))
	{
		return *index;
	}
	if (subject == "Primary Interface")
	{
		return 0;
	}
	if (subject == "Secondary Interface")
	{
		return 1;
	}
	if (auto const index = numberAfter(subject, QLatin1String{ "Input Stream " }))
	{
		if (auto const plane = topology.streamPlane(event.entityID, static_cast<la::avdecc::entity::model::StreamIndex>(*index)); plane != UnknownPlane)
		{
			return plane;
		}
		return planeFromStreamName(subject);
	}
	if (subject.startsWith(QLatin1String{ "Output Stream " }))
	{
		return planeFromStreamName(subject);
	}
	if (subject == "Statistics")
	{
		return detailsPlane;
	}
	return UnknownPlane;
}

std::optional<EntityID> JournalAnalysis::Private::rackOf(EntityID const entityID, Plane const plane) const noexcept
{
	auto const key = std::make_pair(entityID, plane);
	if (auto const it = rackCache.find(key); it != rackCache.end())
	{
		return it->second;
	}
	auto rack = topology.rackSwitch(entityID, plane);
	if (!rack && plane == UnknownPlane)
	{
		// A clock domain event has no plane: the entity's position on its first known interface
		if (auto const entityIt = topology.entities.find(entityID); entityIt != topology.entities.end() && !entityIt->second.interfaces.empty())
		{
			rack = topology.rackSwitch(entityID, static_cast<Plane>(entityIt->second.interfaces.begin()->first));
		}
	}
	rackCache[key] = rack;
	return rack;
}

std::optional<EntityID> JournalAnalysis::Private::branchOf(EntityID const entityID, Plane const plane) const noexcept
{
	auto const key = std::make_pair(entityID, plane);
	if (auto const it = branchCache.find(key); it != branchCache.end())
	{
		return it->second;
	}
	auto branch = topology.branchSwitch(entityID, plane);
	if (!branch && plane == UnknownPlane)
	{
		if (auto const entityIt = topology.entities.find(entityID); entityIt != topology.entities.end() && !entityIt->second.interfaces.empty())
		{
			branch = topology.branchSwitch(entityID, static_cast<Plane>(entityIt->second.interfaces.begin()->first));
		}
	}
	branchCache[key] = branch;
	return branch;
}

bool JournalAnalysis::Private::isInsideSystemWideEvent(qint64 const timestamp, qint64 const endTimestamp) const noexcept
{
	for (auto const& event : results.systemWideEvents)
	{
		if (timestamp <= event.endTimestamp + options.hitWindowMs && endTimestamp >= event.timestamp - options.hitWindowMs)
		{
			return true;
		}
	}
	return false;
}

void JournalAnalysis::Private::computeSessionShape(std::vector<EventJournal::Event> const& events)
{
	auto onlineEntities = std::unordered_set<EntityID, EntityID::hash>{};
	auto controllers = std::set<EntityID>{};
	auto heartbeatTimestamps = std::vector<qint64>{};
	auto heartbeatIndexes = EventIndexes{};

	// Waves of entities going online or offline together
	auto const buildWaves = [this, &events](Kind const kind, bool const isOnline)
	{
		auto current = std::optional<Wave>{};
		auto const flush = [this, &current]()
		{
			if (current && current->entities.size() >= options.waveMinEntities)
			{
				results.waves.push_back(*current);
			}
			current.reset();
		};
		for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
		{
			if (infos[i].kind != kind)
			{
				continue;
			}
			auto const& event = events[i];
			if (current && event.timestamp - current->endTimestamp > options.waveGapMs)
			{
				flush();
			}
			if (!current)
			{
				current = Wave{ event.timestamp, event.timestamp, isOnline, {}, {} };
			}
			current->endTimestamp = event.timestamp;
			appendUnique(current->entities, event.entityID);
			current->events.push_back(i);
		}
		flush();
	};
	buildWaves(Kind::EntityOnline, true);
	buildWaves(Kind::EntityOffline, false);
	std::sort(results.waves.begin(), results.waves.end(),
		[](Wave const& lhs, Wave const& rhs)
		{
			return lhs.timestamp < rhs.timestamp;
		});
	auto waveEvents = std::unordered_set<EventIndex>{};
	for (auto const& wave : results.waves)
	{
		if (!wave.isOnline)
		{
			waveEvents.insert(wave.events.begin(), wave.events.end());
		}
	}

	auto openDropOuts = std::unordered_map<EntityID, std::size_t, EntityID::hash>{};
	auto lastRefresh = std::unordered_map<EntityID, qint64, EntityID::hash>{};
	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		auto const& event = events[i];
		auto const& info = infos[i];
		switch (info.kind)
		{
			case Kind::EntityOnline:
				if (event.timestamp - results.firstTimestamp <= 60000 && onlineEntities.count(event.entityID) == 0u)
				{
					++results.entitiesOnlineAtStart;
				}
				onlineEntities.insert(event.entityID);
				if (info.flag)
				{
					controllers.insert(event.entityID);
				}
				if (auto const it = openDropOuts.find(event.entityID); it != openDropOuts.end())
				{
					auto& dropOut = results.dropOuts[it->second];
					dropOut.durationMs = event.timestamp - dropOut.timestamp;
					dropOut.events.push_back(i);
					openDropOuts.erase(it);
				}
				break;
			case Kind::EntityOffline:
				if (waveEvents.count(i) == 0u)
				{
					auto dropOut = DropOut{};
					dropOut.entityID = event.entityID;
					dropOut.timestamp = event.timestamp;
					dropOut.events.push_back(i);
					if (auto const it = lastRefresh.find(event.entityID); it != lastRefresh.end() && event.timestamp - it->second <= options.waveGapMs)
					{
						dropOut.isRefresh = true;
					}
					openDropOuts[event.entityID] = results.dropOuts.size();
					results.dropOuts.push_back(dropOut);
				}
				break;
			case Kind::RefreshRequested:
				lastRefresh[event.entityID] = event.timestamp;
				results.operatorActions.push_back(i);
				break;
			case Kind::Marker:
				results.markers.push_back(i);
				break;
			case Kind::RecordingStarted:
			case Kind::RecordingStopped:
			case Kind::ControllerStopped:
			case Kind::ControllerRestarted:
			case Kind::SessionOther:
				results.sessionEvents.push_back(i);
				break;
			case Kind::ControllerInterface:
				results.controllerInterfaceEvents.push_back(i);
				break;
			case Kind::Heartbeat:
				++results.heartbeats;
				heartbeatTimestamps.push_back(event.timestamp);
				heartbeatIndexes.push_back(i);
				if (info.value >= options.heartbeatLatencyThresholdMs)
				{
					results.heartbeatLatencyOutliers.push_back(i);
				}
				break;
			default:
				break;
		}
	}
	results.entitiesCount = onlineEntities.size();
	results.controllers.assign(controllers.begin(), controllers.end());
	results.isInterrupted = infos.back().kind != Kind::RecordingStopped;

	// Heartbeat continuity: a gap of more than twice the usual interval means the controller was frozen or stopped
	if (heartbeatTimestamps.size() >= 2u)
	{
		auto intervals = std::vector<qint64>{};
		for (auto i = std::size_t{ 1u }; i < heartbeatTimestamps.size(); ++i)
		{
			intervals.push_back(heartbeatTimestamps[i] - heartbeatTimestamps[i - 1u]);
		}
		auto sorted = intervals;
		std::sort(sorted.begin(), sorted.end());
		results.heartbeatIntervalMs = medianOf(sorted);
		for (auto i = std::size_t{ 0u }; i < intervals.size(); ++i)
		{
			if (intervals[i] > 2 * *results.heartbeatIntervalMs)
			{
				results.heartbeatGaps.push_back(HeartbeatGap{ heartbeatTimestamps[i], intervals[i], { heartbeatIndexes[i], heartbeatIndexes[i + 1u] } });
			}
		}
	}
}

void JournalAnalysis::Private::computeBreakdown(std::vector<EventJournal::Event> const& events)
{
	auto const trailingParenthesis = QRegularExpression{ " \\([^()]*\\)$" };
	auto present = std::set<EventJournal::Category>{};
	for (auto const& event : events)
	{
		++results.categoryCounts[std::make_pair(event.category, event.severity)];
		present.insert(event.category);

		auto family = QString{};
		if (event.category == EventJournal::Category::Marker)
		{
			family = "Marker";
		}
		else
		{
			family = event.summary;
			family.remove(trailingParenthesis);
			if (auto const colon = family.indexOf(QLatin1String{ ": " }); colon > 0)
			{
				family.truncate(colon);
			}
			if (auto const from = family.indexOf(QLatin1String{ " from " }); from > 0)
			{
				family.truncate(from);
			}
			if (auto const to = family.indexOf(QLatin1String{ " to " }); to > 0)
			{
				family.truncate(to);
			}
		}
		++results.familyCounts[family];
	}
	for (auto category = static_cast<int>(EventJournal::Category::Session); category <= static_cast<int>(EventJournal::Category::Marker); ++category)
	{
		if (present.count(static_cast<EventJournal::Category>(category)) == 0u)
		{
			results.absentCategories.push_back(static_cast<EventJournal::Category>(category));
		}
	}
}

void JournalAnalysis::Private::computeHits(std::vector<EventJournal::Event> const& events)
{
	auto lastHit = std::map<std::pair<EntityID, Plane>, std::size_t>{};
	auto lastClockHit = std::map<std::pair<EntityID, QString>, std::size_t>{};
	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		auto const& event = events[i];
		auto const& info = infos[i];
		if (info.kind == Kind::MediaUnlock)
		{
			++results.unlockEvents;
			++results.unlockEventsPerPlane[info.plane];
			auto const key = std::make_pair(event.entityID, info.plane);
			if (auto const it = lastHit.find(key); it != lastHit.end() && event.timestamp - results.hits[it->second].endTimestamp <= options.hitWindowMs)
			{
				auto& hit = results.hits[it->second];
				hit.endTimestamp = event.timestamp;
				hit.events.push_back(i);
			}
			else
			{
				lastHit[key] = results.hits.size();
				results.hits.push_back(Hit{ event.timestamp, event.timestamp, event.entityID, info.plane, { i }, false });
			}
		}
		else if (info.kind == Kind::ClockUnlock)
		{
			auto const key = std::make_pair(event.entityID, event.subject);
			if (auto const it = lastClockHit.find(key); it != lastClockHit.end() && event.timestamp - clockHits[it->second].endTimestamp <= options.hitWindowMs)
			{
				auto& hit = clockHits[it->second];
				hit.endTimestamp = event.timestamp;
				hit.events.push_back(i);
			}
			else
			{
				lastClockHit[key] = clockHits.size();
				clockHits.push_back(Hit{ event.timestamp, event.timestamp, event.entityID, UnknownPlane, { i }, true });
			}
		}
	}
	for (auto const& hit : results.hits)
	{
		++results.hitsPerPlane[hit.plane];
		++hitsPerEntityPlane[std::make_pair(hit.entityID, hit.plane)];
	}
}

void JournalAnalysis::Private::computeIntervals(std::vector<EventJournal::Event> const& events)
{
	// Pairs an error event with the recovered event that closes it, per entity and subject
	auto const pair = [this, &events](Kind const openKind, Kind const closeKind, bool const pairedOnly, std::vector<Interval>& intervals)
	{
		auto open = std::map<std::pair<EntityID, QString>, std::size_t>{};
		for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
		{
			auto const& event = events[i];
			auto const& info = infos[i];
			auto const key = std::make_pair(event.entityID, event.subject);
			if (info.kind == openKind && (!pairedOnly || info.isPaired))
			{
				auto interval = Interval{};
				interval.entityID = event.entityID;
				interval.subject = event.subject;
				interval.plane = info.plane;
				interval.timestamp = event.timestamp;
				interval.isInferred = info.flag;
				interval.startEvent = i;
				open[key] = intervals.size();
				intervals.push_back(interval);
			}
			else if (info.kind == closeKind)
			{
				if (auto const it = open.find(key); it != open.end())
				{
					auto& interval = intervals[it->second];
					interval.isClosed = true;
					interval.endEvent = i;
					interval.isInferred = interval.isInferred || info.flag;
					if (!interval.isInferred)
					{
						interval.durationMs = event.timestamp - interval.timestamp;
					}
					open.erase(it);
				}
			}
		}
	};
	pair(Kind::ClockUnlock, Kind::ClockLock, false, results.clockDomainIntervals);
	pair(Kind::MediaUnlock, Kind::MediaLock, true, results.streamIntervals);
	pair(Kind::LinkDown, Kind::LinkUp, false, results.linkIntervals);
	pair(Kind::PropagationDelayNull, Kind::PropagationDelayMeasured, false, results.nullPropagationDelayIntervals);
}

void JournalAnalysis::Private::computeSystemWideEvents(std::vector<EventJournal::Event> const& events)
{
	// Every entity touched, at stream level or at clock domain level, by ascending time
	auto touches = std::vector<Hit const*>{};
	touches.reserve(results.hits.size() + clockHits.size());
	for (auto const& hit : results.hits)
	{
		touches.push_back(&hit);
	}
	for (auto const& hit : clockHits)
	{
		touches.push_back(&hit);
	}
	std::stable_sort(touches.begin(), touches.end(),
		[](Hit const* const lhs, Hit const* const rhs)
		{
			return lhs->timestamp < rhs->timestamp;
		});

	// The population: the entities reporting at least one hit in the session
	auto population = std::set<EntityID>{};
	for (auto const* const touch : touches)
	{
		population.insert(touch->entityID);
	}
	auto const minEntities = std::max(options.systemWideMinEntities, static_cast<std::size_t>(std::ceil(options.systemWideMinShare * static_cast<double>(population.size()))));
	auto const hasTopology = topology.hasAsPaths();

	auto const flush = [this, &events, &population, &minEntities, &hasTopology](std::vector<Hit const*> const& cluster)
	{
		auto event = SystemWideEvent{};
		auto racks = std::set<EntityID>{};
		auto branches = std::set<EntityID>{};
		auto planes = std::set<Plane>{};
		auto hitPlanes = std::set<std::pair<EntityID, Plane>>{};
		event.timestamp = cluster.front()->timestamp;
		for (auto const* const touch : cluster)
		{
			event.endTimestamp = std::max(event.endTimestamp, touch->endTimestamp);
			appendUnique(event.entities, touch->entityID);
			event.events.insert(event.events.end(), touch->events.begin(), touch->events.end());
			if (touch->isClockDomain)
			{
				appendUnique(event.clockDomainEntities, touch->entityID);
			}
			else
			{
				planes.insert(touch->plane);
				hitPlanes.insert(std::make_pair(touch->entityID, touch->plane));
			}
			if (auto const rack = rackOf(touch->entityID, touch->plane))
			{
				racks.insert(*rack);
			}
			if (auto const branch = branchOf(touch->entityID, touch->plane))
			{
				branches.insert(*branch);
			}
		}
		if (event.entities.size() < minEntities || (hasTopology && racks.size() < 2u))
		{
			return;
		}
		std::sort(event.events.begin(), event.events.end());
		event.planes.assign(planes.begin(), planes.end());
		event.racks = racks.size();
		event.branches = branches.size();
		for (auto const entityID : population)
		{
			if (std::find(event.entities.begin(), event.entities.end(), entityID) == event.entities.end())
			{
				event.spared.push_back(entityID);
			}
		}
		// How completely each branch was hit, per plane, among the entities of the branch that reported hits on that plane in the session
		if (hasTopology)
		{
			auto shares = std::map<std::pair<std::optional<EntityID>, Plane>, SystemWideEvent::BranchShare>{};
			for (auto const& [key, count] : hitsPerEntityPlane)
			{
				auto const& [entityID, plane] = key;
				if (planes.count(plane) == 0u)
				{
					continue;
				}
				auto const branch = branchOf(entityID, plane);
				auto& share = shares[std::make_pair(branch, plane)];
				share.branchSwitch = branch;
				share.plane = plane;
				++share.total;
				if (hitPlanes.count(std::make_pair(entityID, plane)) > 0u)
				{
					++share.hit;
				}
			}
			for (auto const& [key, share] : shares)
			{
				event.branchShares.push_back(share);
			}
		}
		// Clock domain unlock durations and accompanying events
		for (auto const& interval : results.clockDomainIntervals)
		{
			if (interval.timestamp >= event.timestamp && interval.timestamp <= event.endTimestamp && interval.durationMs)
			{
				event.minClockDomainUnlockMs = event.minClockDomainUnlockMs ? std::min(*event.minClockDomainUnlockMs, *interval.durationMs) : *interval.durationMs;
				event.maxClockDomainUnlockMs = event.maxClockDomainUnlockMs ? std::max(*event.maxClockDomainUnlockMs, *interval.durationMs) : *interval.durationMs;
			}
		}
		auto const windowStart = event.timestamp - options.accompanyingWindowMs;
		auto const windowEnd = event.endTimestamp + options.accompanyingWindowMs;
		auto const first = std::lower_bound(events.begin(), events.end(), windowStart,
			[](EventJournal::Event const& lhs, qint64 const timestamp)
			{
				return lhs.timestamp < timestamp;
			});
		for (auto it = first; it != events.end() && it->timestamp <= windowEnd; ++it)
		{
			switch (infos[static_cast<std::size_t>(std::distance(events.begin(), it))].kind)
			{
				case Kind::GrandmasterChange:
					++event.grandmasterChanges;
					break;
				case Kind::LinkDown:
				case Kind::LinkUp:
					++event.linkEvents;
					break;
				case Kind::PacketLoss:
					++event.packetLossEvents;
					break;
				case Kind::StreamDataLoss:
					++event.streamDataLosses;
					break;
				default:
					break;
			}
		}
		results.systemWideEvents.push_back(std::move(event));
	};

	auto cluster = std::vector<Hit const*>{};
	auto clusterEnd = qint64{ 0 };
	for (auto const* const touch : touches)
	{
		if (!cluster.empty() && touch->timestamp - clusterEnd > options.systemWideGapMs)
		{
			flush(cluster);
			cluster.clear();
		}
		cluster.push_back(touch);
		clusterEnd = cluster.size() == 1u ? touch->endTimestamp : std::max(clusterEnd, touch->endTimestamp);
	}
	if (!cluster.empty())
	{
		flush(cluster);
	}

	// The tail of an event: the entities it hit reporting again shortly after (the relock wave, the second wave of a discontinuity) belong to it, not to the background
	auto memberTouches = std::unordered_set<Hit const*>{};
	for (auto const& event : results.systemWideEvents)
	{
		for (auto const* const touch : touches)
		{
			if (touch->timestamp >= event.timestamp && touch->timestamp <= event.endTimestamp)
			{
				memberTouches.insert(touch);
			}
		}
	}
	for (auto& event : results.systemWideEvents)
	{
		auto changed = true;
		while (changed)
		{
			changed = false;
			for (auto const* const touch : touches)
			{
				if (touch->timestamp <= event.endTimestamp || memberTouches.count(touch) > 0u)
				{
					continue;
				}
				if (touch->timestamp > event.endTimestamp + options.systemWideTailMs)
				{
					break;
				}
				if (std::find(event.entities.begin(), event.entities.end(), touch->entityID) == event.entities.end())
				{
					continue;
				}
				memberTouches.insert(touch);
				event.endTimestamp = std::max(event.endTimestamp, touch->endTimestamp);
				event.events.insert(event.events.end(), touch->events.begin(), touch->events.end());
				if (touch->isClockDomain)
				{
					appendUnique(event.clockDomainEntities, touch->entityID);
				}
				changed = true;
			}
		}
		std::sort(event.events.begin(), event.events.end());
	}
}

void JournalAnalysis::Private::computeIncidents(std::vector<EventJournal::Event> const&)
{
	// Entities plugged into each rack switch, per plane
	auto rackSizes = std::map<std::pair<EntityID, Plane>, std::size_t>{};
	for (auto const& [entityID, entity] : topology.entities)
	{
		for (auto const& [index, interface] : entity.interfaces)
		{
			if (auto const rack = rackOf(entityID, static_cast<Plane>(index)))
			{
				++rackSizes[std::make_pair(*rack, static_cast<Plane>(index))];
			}
		}
	}

	auto lastIncident = std::map<std::pair<EntityID, Plane>, std::size_t>{};
	for (auto const& hit : results.hits)
	{
		auto const rack = rackOf(hit.entityID, hit.plane);
		if (!rack)
		{
			continue;
		}
		auto const key = std::make_pair(*rack, hit.plane);
		if (auto const it = lastIncident.find(key); it != lastIncident.end() && hit.timestamp - results.incidents[it->second].endTimestamp <= options.hitWindowMs)
		{
			auto& incident = results.incidents[it->second];
			incident.endTimestamp = std::max(incident.endTimestamp, hit.endTimestamp);
			appendUnique(incident.entities, hit.entityID);
			incident.events.insert(incident.events.end(), hit.events.begin(), hit.events.end());
		}
		else
		{
			lastIncident[key] = results.incidents.size();
			auto incident = Incident{};
			incident.timestamp = hit.timestamp;
			incident.endTimestamp = hit.endTimestamp;
			incident.rackSwitch = *rack;
			incident.plane = hit.plane;
			incident.entities.push_back(hit.entityID);
			incident.rackSize = rackSizes[key];
			incident.events = hit.events;
			results.incidents.push_back(std::move(incident));
		}
	}
	for (auto& incident : results.incidents)
	{
		++results.incidentsPerPlane[incident.plane];
		if (incident.entities.size() >= incident.rackSize)
		{
			++results.fullRackIncidents;
		}
		incident.isSystemWide = isInsideSystemWideEvent(incident.timestamp, incident.endTimestamp);
		if (incident.isSystemWide)
		{
			++results.systemWideIncidents;
		}
	}
}

void JournalAnalysis::Private::computeStorms()
{
	auto lastStorm = std::map<std::pair<EntityID, Plane>, std::size_t>{};
	for (auto const& incident : results.incidents)
	{
		if (incident.isSystemWide)
		{
			continue;
		}
		auto const key = std::make_pair(incident.rackSwitch, incident.plane);
		if (auto const it = lastStorm.find(key); it != lastStorm.end() && incident.timestamp - results.storms[it->second].endTimestamp <= options.stormGapMs)
		{
			auto& storm = results.storms[it->second];
			storm.endTimestamp = std::max(storm.endTimestamp, incident.endTimestamp);
			++storm.incidents;
			storm.events.insert(storm.events.end(), incident.events.begin(), incident.events.end());
		}
		else
		{
			lastStorm[key] = results.storms.size();
			auto storm = Storm{};
			storm.timestamp = incident.timestamp;
			storm.endTimestamp = incident.endTimestamp;
			storm.rackSwitch = incident.rackSwitch;
			storm.branchSwitch = branchOf(incident.entities.front(), incident.plane);
			storm.plane = incident.plane;
			storm.incidents = 1u;
			storm.events = incident.events;
			results.storms.push_back(std::move(storm));
		}
	}
}

void JournalAnalysis::Private::computeRuns()
{
	auto lastRun = std::map<std::pair<EntityID, Plane>, std::size_t>{};
	auto runStorms = std::vector<std::vector<std::size_t>>{};
	for (auto stormIndex = std::size_t{ 0u }; stormIndex < results.storms.size(); ++stormIndex)
	{
		auto const& storm = results.storms[stormIndex];
		if (!storm.branchSwitch)
		{
			continue;
		}
		auto const key = std::make_pair(*storm.branchSwitch, storm.plane);
		auto runIndex = std::optional<std::size_t>{};
		if (auto const it = lastRun.find(key); it != lastRun.end() && storm.timestamp - results.runs[it->second].endTimestamp <= options.runGapMs)
		{
			runIndex = it->second;
		}
		else
		{
			lastRun[key] = results.runs.size();
			runIndex = results.runs.size();
			auto run = Run{};
			run.timestamp = storm.timestamp;
			run.endTimestamp = storm.endTimestamp;
			run.branchSwitch = *storm.branchSwitch;
			run.plane = storm.plane;
			results.runs.push_back(std::move(run));
			runStorms.emplace_back();
		}
		auto& run = results.runs[*runIndex];
		run.endTimestamp = std::max(run.endTimestamp, storm.endTimestamp);
		++run.storms;
		run.incidents += storm.incidents;
		run.events.insert(run.events.end(), storm.events.begin(), storm.events.end());
		runStorms[*runIndex].push_back(stormIndex);
	}
	// The racks of a run, ordered by their main storm (the one with the most incidents): an isolated incident of a rack before its turn does not move it in the port order
	for (auto runIndex = std::size_t{ 0u }; runIndex < results.runs.size(); ++runIndex)
	{
		auto& run = results.runs[runIndex];
		for (auto const stormIndex : runStorms[runIndex])
		{
			auto const& storm = results.storms[stormIndex];
			auto visitIt = std::find_if(run.racks.begin(), run.racks.end(),
				[&storm](Run::RackVisit const& visit)
				{
					return visit.rackSwitch == storm.rackSwitch;
				});
			if (visitIt == run.racks.end())
			{
				run.racks.push_back(Run::RackVisit{ storm.rackSwitch, storm.timestamp, storm.endTimestamp, 0u, 0u, {} });
				visitIt = std::prev(run.racks.end());
			}
			auto& visit = *visitIt;
			++visit.storms;
			visit.incidents += storm.incidents;
			// The main storm: the most incidents, the earliest on a tie (the first storm initialized the span)
			auto const mainIncidents = std::count_if(results.incidents.begin(), results.incidents.end(),
				[&visit, &run](Incident const& incident)
				{
					return !incident.isSystemWide && incident.plane == run.plane && incident.rackSwitch == visit.rackSwitch && incident.timestamp >= visit.timestamp && incident.timestamp <= visit.endTimestamp;
				});
			if (static_cast<std::size_t>(mainIncidents) < storm.incidents)
			{
				visit.timestamp = storm.timestamp;
				visit.endTimestamp = storm.endTimestamp;
			}
		}
		for (auto& visit : run.racks)
		{
			for (auto const& incident : results.incidents)
			{
				if (!incident.isSystemWide && incident.plane == run.plane && incident.rackSwitch == visit.rackSwitch && incident.timestamp >= run.timestamp && incident.timestamp <= run.endTimestamp)
				{
					for (auto const entityID : incident.entities)
					{
						appendUnique(visit.entities, entityID);
					}
				}
			}
		}
		std::sort(run.racks.begin(), run.racks.end(),
			[](Run::RackVisit const& lhs, Run::RackVisit const& rhs)
			{
				return lhs.timestamp < rhs.timestamp;
			});
	}
	// Series: the runs of each branch and plane, the periodicity of the sweeps (the runs visiting several racks) and whether their port order repeats
	auto series = std::map<std::pair<EntityID, Plane>, RunSeries>{};
	for (auto i = std::size_t{ 0u }; i < results.runs.size(); ++i)
	{
		auto const& run = results.runs[i];
		auto& entry = series[std::make_pair(run.branchSwitch, run.plane)];
		entry.branchSwitch = run.branchSwitch;
		entry.plane = run.plane;
		entry.runs.push_back(i);
		if (run.racks.size() >= 2u)
		{
			entry.sweeps.push_back(i);
		}
	}
	for (auto& [key, entry] : series)
	{
		auto orders = std::map<std::vector<EntityID>, std::size_t>{};
		for (auto i = std::size_t{ 0u }; i < entry.sweeps.size(); ++i)
		{
			auto const& run = results.runs[entry.sweeps[i]];
			auto order = std::vector<EntityID>{};
			for (auto const& visit : run.racks)
			{
				order.push_back(visit.rackSwitch);
			}
			entry.sameOrderSweeps = std::max(entry.sameOrderSweeps, ++orders[order]);
			if (i > 0u)
			{
				auto const period = run.timestamp - results.runs[entry.sweeps[i - 1u]].timestamp;
				entry.minPeriodMs = entry.minPeriodMs ? std::min(*entry.minPeriodMs, period) : period;
				entry.maxPeriodMs = entry.maxPeriodMs ? std::max(*entry.maxPeriodMs, period) : period;
			}
		}
		results.runSeries.push_back(entry);
	}
	std::sort(results.runSeries.begin(), results.runSeries.end(),
		[](RunSeries const& lhs, RunSeries const& rhs)
		{
			if (lhs.sweeps.size() != rhs.sweeps.size())
			{
				return lhs.sweeps.size() > rhs.sweeps.size();
			}
			return lhs.runs.size() > rhs.runs.size();
		});
}

void JournalAnalysis::Private::computeRankings()
{
	// Entities
	auto entityHits = std::map<EntityID, EntityHits>{};
	for (auto const& [key, count] : hitsPerEntityPlane)
	{
		auto& entry = entityHits[key.first];
		entry.entityID = key.first;
		entry.total += count;
		entry.perPlane[key.second] += count;
	}
	for (auto const& hit : clockHits)
	{
		auto& entry = entityHits[hit.entityID];
		entry.entityID = hit.entityID;
		entry.clockDomainUnlocks += hit.events.size();
	}
	for (auto const& [entityID, entry] : entityHits)
	{
		results.entityRanking.push_back(entry);
	}
	std::sort(results.entityRanking.begin(), results.entityRanking.end(),
		[this](EntityHits const& lhs, EntityHits const& rhs)
		{
			if (lhs.total != rhs.total)
			{
				return lhs.total > rhs.total;
			}
			return entityName(lhs.entityID) < entityName(rhs.entityID);
		});
	{
		auto totals = std::vector<qint64>{};
		for (auto const& entry : results.entityRanking)
		{
			totals.push_back(static_cast<qint64>(entry.total));
		}
		std::sort(totals.begin(), totals.end());
		results.medianEntityHits = static_cast<std::size_t>(medianOf(totals));
	}

	// Racks and branches
	auto racks = std::map<std::pair<EntityID, Plane>, RackHits>{};
	for (auto const& incident : results.incidents)
	{
		auto& entry = racks[std::make_pair(incident.rackSwitch, incident.plane)];
		entry.rackSwitch = incident.rackSwitch;
		entry.plane = incident.plane;
		entry.branchSwitch = branchOf(incident.entities.front(), incident.plane);
		++entry.incidents;
		if (!incident.isSystemWide)
		{
			++entry.backgroundIncidents;
		}
	}
	for (auto const& [entityID, entity] : topology.entities)
	{
		for (auto const& [index, interface] : entity.interfaces)
		{
			auto const plane = static_cast<Plane>(index);
			if (auto const rack = rackOf(entityID, plane))
			{
				if (auto const it = racks.find(std::make_pair(*rack, plane)); it != racks.end())
				{
					it->second.entities.push_back(entityID);
				}
			}
		}
	}
	auto branches = std::map<std::pair<std::optional<EntityID>, Plane>, BranchHits>{};
	for (auto const& [key, entry] : racks)
	{
		results.rackRanking.push_back(entry);
		auto& branch = branches[std::make_pair(entry.branchSwitch, entry.plane)];
		branch.branchSwitch = entry.branchSwitch;
		branch.plane = entry.plane;
		branch.incidents += entry.incidents;
		branch.backgroundIncidents += entry.backgroundIncidents;
		++branch.racks;
	}
	for (auto& entry : results.rackRanking)
	{
		std::sort(entry.entities.begin(), entry.entities.end(),
			[this](EntityID const lhs, EntityID const rhs)
			{
				return entityName(lhs) < entityName(rhs);
			});
	}
	std::sort(results.rackRanking.begin(), results.rackRanking.end(),
		[](RackHits const& lhs, RackHits const& rhs)
		{
			return lhs.incidents > rhs.incidents;
		});
	for (auto const& [key, entry] : branches)
	{
		results.branchRanking.push_back(entry);
	}
	std::sort(results.branchRanking.begin(), results.branchRanking.end(),
		[](BranchHits const& lhs, BranchHits const& rhs)
		{
			return lhs.incidents > rhs.incidents;
		});
}

void JournalAnalysis::Private::computeBothPlanes()
{
	auto perEntity = std::map<EntityID, std::pair<std::vector<Hit const*>, std::vector<Hit const*>>>{};
	for (auto const& hit : results.hits)
	{
		if (hit.plane == 0)
		{
			perEntity[hit.entityID].first.push_back(&hit);
		}
		else if (hit.plane == 1)
		{
			perEntity[hit.entityID].second.push_back(&hit);
		}
	}
	auto delays = std::vector<qint64>{};
	for (auto const& [entityID, planes] : perEntity)
	{
		auto const& primary = planes.first;
		auto const& secondary = planes.second;
		auto i = std::size_t{ 0u };
		auto j = std::size_t{ 0u };
		auto paired = std::size_t{ 0u };
		while (i < primary.size() && j < secondary.size())
		{
			auto const delay = std::abs(primary[i]->timestamp - secondary[j]->timestamp);
			if (delay <= options.bothPlanesWindowMs)
			{
				auto pair = BothPlanesPair{};
				pair.entityID = entityID;
				pair.timestamp = std::min(primary[i]->timestamp, secondary[j]->timestamp);
				pair.delayMs = delay;
				pair.events = primary[i]->events;
				pair.events.insert(pair.events.end(), secondary[j]->events.begin(), secondary[j]->events.end());
				std::sort(pair.events.begin(), pair.events.end());
				pair.isSystemWide = isInsideSystemWideEvent(pair.timestamp, pair.timestamp + delay);
				if (delay < options.simultaneousMs)
				{
					++results.simultaneousPairs;
				}
				delays.push_back(delay);
				results.bothPlanesPairs.push_back(std::move(pair));
				++paired;
				++i;
				++j;
			}
			else if (primary[i]->timestamp < secondary[j]->timestamp)
			{
				++i;
			}
			else
			{
				++j;
			}
		}
		results.primaryOnlyHits += primary.size() - paired;
		results.secondaryOnlyHits += secondary.size() - paired;
	}
	std::sort(results.bothPlanesPairs.begin(), results.bothPlanesPairs.end(),
		[](BothPlanesPair const& lhs, BothPlanesPair const& rhs)
		{
			return lhs.timestamp < rhs.timestamp;
		});
	if (!delays.empty())
	{
		std::sort(delays.begin(), delays.end());
		results.bothPlanesMinDelayMs = delays.front();
		results.bothPlanesMaxDelayMs = delays.back();
		results.bothPlanesMedianDelayMs = medianOf(delays);
	}
}

void JournalAnalysis::Private::computeClockDomain(std::vector<EventJournal::Event> const&)
{
	results.clockDomainStats = durationStats(results.clockDomainIntervals,
		[](Interval const&)
		{
			return true;
		});
	for (auto i = std::size_t{ 0u }; i < results.clockDomainIntervals.size(); ++i)
	{
		if (results.clockDomainIntervals[i].durationMs)
		{
			results.longestClockDomainIntervals.push_back(i);
		}
	}
	std::sort(results.longestClockDomainIntervals.begin(), results.longestClockDomainIntervals.end(),
		[this](std::size_t const lhs, std::size_t const rhs)
		{
			return *results.clockDomainIntervals[lhs].durationMs > *results.clockDomainIntervals[rhs].durationMs;
		});
	if (results.longestClockDomainIntervals.size() > options.rankingSize)
	{
		results.longestClockDomainIntervals.resize(options.rankingSize);
	}

	// What accompanied each clock domain unlock at stream level, and how many stream hits came with a clock domain unlock
	auto hitsPerEntity = std::map<EntityID, std::vector<Hit const*>>{};
	for (auto const& hit : results.hits)
	{
		hitsPerEntity[hit.entityID].push_back(&hit);
	}
	auto const overlaps = [this](qint64 const start, qint64 const end, qint64 const otherStart, qint64 const otherEnd)
	{
		return otherStart <= end + options.accompanyingWindowMs && otherEnd >= start - options.accompanyingWindowMs;
	};
	for (auto const& interval : results.clockDomainIntervals)
	{
		auto const end = interval.timestamp + interval.durationMs.value_or(0);
		auto hasPrimary = false;
		auto hasSecondary = false;
		if (auto const it = hitsPerEntity.find(interval.entityID); it != hitsPerEntity.end())
		{
			for (auto const* const hit : it->second)
			{
				if (overlaps(interval.timestamp, end, hit->timestamp, hit->endTimestamp))
				{
					hasPrimary = hasPrimary || hit->plane == 0;
					hasSecondary = hasSecondary || hit->plane == 1;
				}
			}
		}
		if (hasPrimary && hasSecondary)
		{
			++results.clockDomainCompanions.bothPlanes;
		}
		else if (hasPrimary)
		{
			++results.clockDomainCompanions.primaryOnly;
		}
		else if (hasSecondary)
		{
			++results.clockDomainCompanions.secondaryOnly;
		}
		else
		{
			++results.clockDomainCompanions.none;
		}
	}
	auto clockIntervalsPerEntity = std::map<EntityID, std::vector<Interval const*>>{};
	for (auto const& interval : results.clockDomainIntervals)
	{
		clockIntervalsPerEntity[interval.entityID].push_back(&interval);
	}
	for (auto const& hit : results.hits)
	{
		if (auto const it = clockIntervalsPerEntity.find(hit.entityID); it != clockIntervalsPerEntity.end())
		{
			for (auto const* const interval : it->second)
			{
				if (overlaps(hit.timestamp, hit.endTimestamp, interval->timestamp, interval->timestamp + interval->durationMs.value_or(0)))
				{
					++results.hitsWithClockDomainUnlock;
					break;
				}
			}
		}
	}

	// The entities unlocking their clock domain at the system wide events
	auto group = std::map<EntityID, std::size_t>{};
	for (auto const& event : results.systemWideEvents)
	{
		if (event.clockDomainEntities.empty())
		{
			continue;
		}
		++results.systemWideEventsWithClockDomainUnlocks;
		for (auto const entityID : event.clockDomainEntities)
		{
			++group[entityID];
		}
	}
	for (auto const& [entityID, count] : group)
	{
		results.clockDomainGroup.push_back(ClockDomainGroup{ entityID, count });
	}
	std::sort(results.clockDomainGroup.begin(), results.clockDomainGroup.end(),
		[this](ClockDomainGroup const& lhs, ClockDomainGroup const& rhs)
		{
			if (lhs.events != rhs.events)
			{
				return lhs.events > rhs.events;
			}
			return entityName(lhs.entityID) < entityName(rhs.entityID);
		});

	// Stream level pairs: per plane durations, and the periods where both planes of an entity were unlocked
	for (auto const& interval : results.streamIntervals)
	{
		results.streamStatsPerPlane[interval.plane] = DurationStats{};
	}
	for (auto& [plane, stats] : results.streamStatsPerPlane)
	{
		auto const filterPlane = plane;
		stats = durationStats(results.streamIntervals,
			[filterPlane](Interval const& interval)
			{
				return interval.plane == filterPlane;
			});
	}
	auto streamIntervalsPerEntity = std::map<EntityID, std::vector<Interval const*>>{};
	for (auto const& interval : results.streamIntervals)
	{
		if (interval.durationMs)
		{
			streamIntervalsPerEntity[interval.entityID].push_back(&interval);
		}
	}
	for (auto const& [entityID, intervals] : streamIntervalsPerEntity)
	{
		for (auto const* const primary : intervals)
		{
			if (primary->plane != 0)
			{
				continue;
			}
			auto const primaryEnd = primary->timestamp + *primary->durationMs;
			for (auto const* const secondary : intervals)
			{
				if (secondary->plane != 1)
				{
					continue;
				}
				auto const secondaryEnd = secondary->timestamp + *secondary->durationMs;
				auto const start = std::max(primary->timestamp, secondary->timestamp);
				auto const end = std::min(primaryEnd, secondaryEnd);
				if (end > start)
				{
					auto both = Interval{};
					both.entityID = entityID;
					both.subject = QString{ "%1 / %2" }.arg(primary->subject, secondary->subject);
					both.timestamp = start;
					both.durationMs = end - start;
					both.isClosed = true;
					both.startEvent = std::min(primary->startEvent, secondary->startEvent);
					both.endEvent = std::max(primary->endEvent.value_or(0u), secondary->endEvent.value_or(0u));
					results.bothPlanesUnlocked.push_back(std::move(both));
				}
			}
		}
	}
	std::sort(results.bothPlanesUnlocked.begin(), results.bothPlanesUnlocked.end(),
		[](Interval const& lhs, Interval const& rhs)
		{
			return lhs.timestamp < rhs.timestamp;
		});
}

void JournalAnalysis::Private::computeOtherFamilies(std::vector<EventJournal::Event> const& events)
{
	auto packetLoss = std::map<std::tuple<QString, EntityID, QString>, SubjectCount>{};
	auto controller = std::map<std::pair<QString, Plane>, SubjectCount>{};
	auto controllerEntities = std::map<std::pair<QString, Plane>, std::set<EntityID>>{};

	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		auto const& event = events[i];
		auto const& info = infos[i];
		switch (info.kind)
		{
			case Kind::GrandmasterChange:
			{
				++results.grandmasterChanges;
				auto& storms = results.grandmasterStorms;
				if (storms.empty() || event.timestamp - storms.back().endTimestamp > options.grandmasterStormGapMs)
				{
					auto storm = GrandmasterStorm{};
					storm.timestamp = event.timestamp;
					storms.push_back(std::move(storm));
				}
				auto& storm = storms.back();
				storm.endTimestamp = event.timestamp;
				++storm.changes;
				appendUnique(storm.entities, event.entityID);
				storm.events.push_back(i);
				if (std::find(storm.planes.begin(), storm.planes.end(), info.plane) == storm.planes.end())
				{
					storm.planes.push_back(info.plane);
				}
				auto const flipIt = std::find_if(storm.flips.begin(), storm.flips.end(),
					[&info](GrandmasterStorm::Flip const& flip)
					{
						return flip.from == info.from && flip.to == info.to;
					});
				if (flipIt != storm.flips.end())
				{
					++flipIt->count;
				}
				else
				{
					storm.flips.push_back(GrandmasterStorm::Flip{ info.from, info.to, 1u });
				}
				break;
			}
			case Kind::GrandmasterCounter:
				++results.grandmasterCounterIncreases;
				break;
			case Kind::AsPathChange:
				++results.asPathChanges;
				break;
			case Kind::PacketLoss:
			{
				auto& entry = packetLoss[std::make_tuple(info.name, event.entityID, event.subject)];
				entry.entityID = event.entityID;
				entry.subject = event.subject;
				entry.plane = info.plane;
				entry.name = info.name;
				++entry.count;
				entry.events.push_back(i);
				++results.packetLossPerCounter[info.name];
				++results.packetLossPerCounterAndPlane[std::make_pair(info.name, info.plane)];
				break;
			}
			case Kind::ControllerCounter:
			{
				auto const key = std::make_pair(info.name, info.plane);
				auto& entry = controller[key];
				entry.subject = event.subject;
				entry.plane = info.plane;
				entry.name = info.name;
				++entry.count;
				entry.events.push_back(i);
				controllerEntities[key].insert(event.entityID);
				if (info.increment > 0)
				{
					results.unsolicitedNotificationsLost += static_cast<std::size_t>(info.increment);
				}
				break;
			}
			case Kind::Latency:
				++results.latencyErrors;
				break;
			case Kind::RedundancyWarning:
				++results.redundancyWarnings;
				break;
			case Kind::OfflineOnOneInterface:
				++results.offlineOnOneInterface[info.plane];
				break;
			case Kind::UnsolicitedLoss:
				++results.unsolicitedLossesPerInterface[info.plane];
				break;
			case Kind::TransportError:
				++results.transportErrors;
				break;
			case Kind::Connection:
				++results.connectionChanges;
				break;
			case Kind::OperatorAction:
				results.operatorActions.push_back(i);
				break;
			default:
				break;
		}
	}
	std::sort(results.operatorActions.begin(), results.operatorActions.end());

	for (auto& storm : results.grandmasterStorms)
	{
		std::sort(storm.planes.begin(), storm.planes.end());
		std::sort(storm.flips.begin(), storm.flips.end(),
			[](GrandmasterStorm::Flip const& lhs, GrandmasterStorm::Flip const& rhs)
			{
				return lhs.count > rhs.count;
			});
		auto const windowStart = storm.timestamp - options.accompanyingWindowMs;
		auto const windowEnd = storm.endTimestamp + options.accompanyingWindowMs;
		for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
		{
			if (events[i].timestamp < windowStart)
			{
				continue;
			}
			if (events[i].timestamp > windowEnd)
			{
				break;
			}
			if (infos[i].kind == Kind::PacketLoss)
			{
				++storm.packetLossEvents;
			}
			else if (infos[i].kind == Kind::ClockUnlock)
			{
				++storm.clockDomainUnlocks;
			}
		}
	}
	for (auto const& [key, entry] : packetLoss)
	{
		results.packetLossCounts.push_back(entry);
	}
	std::sort(results.packetLossCounts.begin(), results.packetLossCounts.end(),
		[](SubjectCount const& lhs, SubjectCount const& rhs)
		{
			return lhs.count > rhs.count;
		});
	for (auto& [key, entry] : controller)
	{
		entry.entities = controllerEntities[key].size();
		results.controllerCounts.push_back(entry);
	}
}

void JournalAnalysis::Private::computeStreamData(std::vector<EventJournal::Event> const& events)
{
	auto monitored = std::map<std::pair<EntityID, QString>, std::size_t>{};
	auto lastLoss = std::map<std::pair<EntityID, QString>, EventIndex>{};
	auto lossesPerEntityPlane = std::map<EntityID, std::pair<std::vector<EventIndex>, std::vector<EventIndex>>>{};
	auto unlocksPerStream = std::map<std::pair<EntityID, QString>, std::vector<qint64>>{};
	auto hasMonitoringEvents = false;

	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		if (infos[i].kind == Kind::MediaUnlock)
		{
			unlocksPerStream[std::make_pair(events[i].entityID, events[i].subject)].push_back(events[i].timestamp);
		}
	}

	auto const streamEntry = [this, &monitored](EventJournal::Event const& event, Info const& info) -> MonitoredStream&
	{
		auto const key = std::make_pair(event.entityID, event.subject);
		auto it = monitored.find(key);
		if (it == monitored.end())
		{
			it = monitored.emplace(key, results.monitoredStreams.size()).first;
			auto stream = MonitoredStream{};
			stream.entityID = event.entityID;
			stream.subject = event.subject;
			stream.plane = info.plane;
			results.monitoredStreams.push_back(std::move(stream));
		}
		return results.monitoredStreams[it->second];
	};

	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		auto const& event = events[i];
		auto const& info = infos[i];
		if (info.kind == Kind::StreamDataMonitoring)
		{
			results.hasStreamDataEvents = true;
			hasMonitoringEvents = true;
			auto& stream = streamEntry(event, info);
			stream.isCovered = true;
			stream.expectedIncrement = info.expected;
			if (info.from && info.talkerStreamIndex >= 0)
			{
				stream.talker = la::avdecc::entity::model::StreamIdentification{ info.from, static_cast<la::avdecc::entity::model::StreamIndex>(info.talkerStreamIndex) };
			}
			stream.events.push_back(i);
		}
		else if (info.kind == Kind::StreamDataLoss)
		{
			results.hasStreamDataEvents = true;
			auto& stream = streamEntry(event, info);
			if (stream.expectedIncrement == 0)
			{
				stream.expectedIncrement = info.expected;
			}
			if (!stream.talker && info.from && info.talkerStreamIndex >= 0)
			{
				stream.talker = la::avdecc::entity::model::StreamIdentification{ info.from, static_cast<la::avdecc::entity::model::StreamIndex>(info.talkerStreamIndex) };
			}
			++stream.losses;
			stream.totalMissingMs += info.value;
			stream.events.push_back(i);
			++results.streamDataLosses;
			++results.streamDataLossesPerPlane[info.plane];
			auto const bucket = info.value >= 750 ? qint64{ 750 } : info.value >= 500 ? qint64{ 500 } : info.value >= 250 ? qint64{ 250 } : info.value >= 100 ? qint64{ 100 } : qint64{ 0 };
			++results.missingMsHistogram[bucket];
			if (info.plane == 0)
			{
				lossesPerEntityPlane[event.entityID].first.push_back(i);
			}
			else if (info.plane == 1)
			{
				lossesPerEntityPlane[event.entityID].second.push_back(i);
			}
			// Long interruptions: the resume edge, paired with the start edge when the previous loss of the stream is the start of the silence
			auto const key = std::make_pair(event.entityID, event.subject);
			if (info.secondaryValue > options.longInterruptionMs)
			{
				auto interruption = LongInterruption{};
				interruption.entityID = event.entityID;
				interruption.subject = event.subject;
				interruption.plane = info.plane;
				interruption.timestamp = event.timestamp;
				interruption.elapsedMs = info.secondaryValue;
				interruption.resumeEvent = i;
				if (auto const it = lastLoss.find(key); it != lastLoss.end() && event.timestamp - events[it->second].timestamp <= info.secondaryValue + options.accompanyingWindowMs)
				{
					interruption.startEvent = it->second;
				}
				results.longInterruptions.push_back(std::move(interruption));
			}
			lastLoss[key] = i;
			// A loss within the same seconds as a media unlock of the stream means the incident cuts the frames
			if (auto const it = unlocksPerStream.find(key); it != unlocksPerStream.end())
			{
				for (auto const timestamp : it->second)
				{
					if (std::abs(timestamp - event.timestamp) <= options.accompanyingWindowMs)
					{
						++results.lossesWithMediaUnlock;
						break;
					}
				}
			}
		}
	}
	if (!results.hasStreamDataEvents)
	{
		return;
	}
	std::sort(results.monitoredStreams.begin(), results.monitoredStreams.end(),
		[this](MonitoredStream const& lhs, MonitoredStream const& rhs)
		{
			if (lhs.losses != rhs.losses)
			{
				return lhs.losses > rhs.losses;
			}
			auto const lhsName = entityName(lhs.entityID);
			auto const rhsName = entityName(rhs.entityID);
			if (lhsName != rhsName)
			{
				return lhsName < rhsName;
			}
			return lhs.subject < rhs.subject;
		});

	// Coverage: the connected streams the rule never covered (only meaningful when the journal has the monitoring events, which are diagnostic level)
	if (hasMonitoringEvents)
	{
		auto coveredIndexes = std::set<std::pair<EntityID, int>>{};
		for (auto const& stream : results.monitoredStreams)
		{
			if (auto const index = numberAfter(stream.subject, QLatin1String{ "Input Stream " }))
			{
				coveredIndexes.insert(std::make_pair(stream.entityID, *index));
			}
		}
		for (auto const& [entityID, entity] : topology.entities)
		{
			for (auto const& [streamIndex, stream] : entity.streamInputs)
			{
				if (stream.talker && coveredIndexes.count(std::make_pair(entityID, static_cast<int>(streamIndex))) == 0u)
				{
					results.uncoveredStreams.push_back(UncoveredStream{ entityID, streamIndex, stream.plane, stream.talker });
				}
			}
		}
		std::sort(results.uncoveredStreams.begin(), results.uncoveredStreams.end(),
			[this](UncoveredStream const& lhs, UncoveredStream const& rhs)
			{
				auto const lhsName = entityName(lhs.entityID);
				auto const rhsName = entityName(rhs.entityID);
				if (lhsName != rhsName)
				{
					return lhsName < rhsName;
				}
				return lhs.streamIndex < rhs.streamIndex;
			});
	}

	// Both planes: losses of the two planes of the same entity within the same second
	for (auto const& [entityID, planes] : lossesPerEntityPlane)
	{
		auto const& primary = planes.first;
		auto const& secondary = planes.second;
		auto i = std::size_t{ 0u };
		auto j = std::size_t{ 0u };
		while (i < primary.size() && j < secondary.size())
		{
			auto const delay = std::abs(events[primary[i]].timestamp - events[secondary[j]].timestamp);
			if (delay <= options.hitWindowMs)
			{
				auto pair = BothPlanesPair{};
				pair.entityID = entityID;
				pair.timestamp = std::min(events[primary[i]].timestamp, events[secondary[j]].timestamp);
				pair.delayMs = delay;
				pair.events = { std::min(primary[i], secondary[j]), std::max(primary[i], secondary[j]) };
				results.streamDataBothPlanes.push_back(std::move(pair));
				++i;
				++j;
			}
			else if (events[primary[i]].timestamp < events[secondary[j]].timestamp)
			{
				++i;
			}
			else
			{
				++j;
			}
		}
	}
	std::sort(results.streamDataBothPlanes.begin(), results.streamDataBothPlanes.end(),
		[](BothPlanesPair const& lhs, BothPlanesPair const& rhs)
		{
			return lhs.timestamp < rhs.timestamp;
		});

	// Incidents: the listeners of one talker stream hit within the hit window, and the deepest switch common to their paths
	auto listenersPerTalker = std::map<std::pair<la::avdecc::entity::model::StreamIdentification, Plane>, EntityIDs>{};
	for (auto const& [entityID, entity] : topology.entities)
	{
		for (auto const& [streamIndex, stream] : entity.streamInputs)
		{
			if (stream.talker)
			{
				appendUnique(listenersPerTalker[std::make_pair(*stream.talker, stream.plane)], entityID);
			}
		}
	}
	auto lastIncident = std::map<std::pair<la::avdecc::entity::model::StreamIdentification, Plane>, std::size_t>{};
	for (auto i = std::size_t{ 0u }; i < events.size(); ++i)
	{
		auto const& event = events[i];
		auto const& info = infos[i];
		if (info.kind != Kind::StreamDataLoss || !info.from || info.talkerStreamIndex < 0)
		{
			continue;
		}
		auto const talker = la::avdecc::entity::model::StreamIdentification{ info.from, static_cast<la::avdecc::entity::model::StreamIndex>(info.talkerStreamIndex) };
		auto const key = std::make_pair(talker, info.plane);
		if (auto const it = lastIncident.find(key); it != lastIncident.end() && event.timestamp - results.streamDataIncidents[it->second].endTimestamp <= options.hitWindowMs)
		{
			auto& incident = results.streamDataIncidents[it->second];
			incident.endTimestamp = event.timestamp;
			appendUnique(incident.listeners, event.entityID);
			incident.events.push_back(i);
		}
		else
		{
			lastIncident[key] = results.streamDataIncidents.size();
			auto incident = StreamDataIncident{};
			incident.timestamp = event.timestamp;
			incident.endTimestamp = event.timestamp;
			incident.talker = talker;
			incident.plane = info.plane;
			incident.listeners.push_back(event.entityID);
			incident.events.push_back(i);
			results.streamDataIncidents.push_back(std::move(incident));
		}
	}
	for (auto& incident : results.streamDataIncidents)
	{
		auto const listenersIt = listenersPerTalker.find(std::make_pair(incident.talker, incident.plane));
		auto const& knownListeners = listenersIt != listenersPerTalker.end() ? listenersIt->second : EntityIDs{};
		incident.talkerListeners = knownListeners.size();
		// Longest common prefix of the paths of the listeners hit: its last element is the deepest common switch
		auto common = std::optional<std::vector<EntityID>>{};
		for (auto const listener : incident.listeners)
		{
			auto const path = topology.hops(listener, incident.plane);
			if (path.empty())
			{
				common.reset();
				break;
			}
			if (!common)
			{
				common = path;
			}
			else
			{
				auto const length = static_cast<std::size_t>(std::distance(common->begin(), std::mismatch(common->begin(), common->end(), path.begin(), path.end()).first));
				common->resize(length);
			}
		}
		if (common && !common->empty())
		{
			incident.commonHop = common->back();
			for (auto const listener : knownListeners)
			{
				if (std::find(incident.listeners.begin(), incident.listeners.end(), listener) != incident.listeners.end())
				{
					continue;
				}
				auto const path = topology.hops(listener, incident.plane);
				if (std::find(path.begin(), path.end(), *incident.commonHop) != path.end())
				{
					incident.sparedBehindHop.push_back(listener);
				}
			}
		}
	}
}

} // namespace modelsLibrary
} // namespace hive
