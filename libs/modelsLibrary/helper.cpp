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


#include "hive/modelsLibrary/helper.hpp"

#include <la/avdecc/utils.hpp>
#include <nlohmann/json.hpp>

#include <QFile>

#include <cctype>
#include <thread>
#include <type_traits>

using json = nlohmann::json;

namespace hive
{
namespace modelsLibrary
{
namespace helper
{
QString uniqueIdentifierToString(la::avdecc::UniqueIdentifier const& identifier)
{
	return toHexQString(identifier.getValue(), true, true);
}

QString interfaceTypeName(la::avdecc::controller::InterfaceType const interfaceType) noexcept
{
	switch (interfaceType)
	{
		case la::avdecc::controller::InterfaceType::Primary:
			return "Primary";
		case la::avdecc::controller::InterfaceType::Secondary:
			return "Secondary";
		default:
			return "Unknown";
	}
}

std::optional<la::avdecc::controller::InterfaceType> redundantInterfaceType(la::avdecc::entity::model::AvbInterfaceIndex const avbInterfaceIndex, la::avdecc::entity::model::MilanVersion const& milanVersion) noexcept
{
	// Milan redundancy (Milan 1.0+) defines AVB interface index 0 as the primary network and index 1 as the secondary network
	if (milanVersion >= la::avdecc::entity::model::MilanVersion{ 1u, 0u })
	{
		switch (avbInterfaceIndex)
		{
			case 0u:
				return la::avdecc::controller::InterfaceType::Primary;
			case 1u:
				return la::avdecc::controller::InterfaceType::Secondary;
			default:
				break;
		}
	}
	return std::nullopt;
}

QString macAddressToString(la::networkInterface::MacAddress const& macAddress)
{
	return QString::fromStdString(la::networkInterface::NetworkInterfaceHelper::macAddressToString(macAddress));
}

QString propagationDelayToString(std::uint32_t const delayNsec, bool const asDistance) noexcept
{
	if (asDistance)
	{
		// Signal propagation speed in a copper cable: 530 ns per 100 m (the estimate is too coarse below 5 meters to show a value)
		auto const meters = delayNsec * (100.0 / 530.0);
		if (meters < 5.0)
		{
			return QStringLiteral("< 5 m");
		}
		return QString::number(meters, 'f', 0) + " m";
	}
	if (delayNsec >= 1000u)
	{
		return QString::number(delayNsec / 1000.0, 'f', 2) + QString::fromUtf8(" \xC2\xB5s");
	}
	return QString::number(delayNsec) + " ns";
}

QString propagationDelayWithDistanceToString(std::uint32_t const delayNsec) noexcept
{
	return QString{ "%1 (%2)" }.arg(propagationDelayToString(delayNsec, false), propagationDelayToString(delayNsec, true));
}

QString configurationName(la::avdecc::controller::ControlledEntity const* const controlledEntity, la::avdecc::controller::model::ConfigurationNode const& node) noexcept
{
	return objectName(controlledEntity, node.descriptorIndex, node);
}

QString localizedString(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::ConfigurationIndex const configurationIndex, la::avdecc::entity::model::LocalizedStringReference const stringReference) noexcept
{
	auto const& localizedName = controlledEntity.getLocalizedString(configurationIndex, stringReference);

	if (localizedName.empty())
	{
		return "(No Localization)";
	}
	return QString::fromStdString(localizedName);
}

QString entityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	try
	{
		auto const& entity = controlledEntity.getEntity();

		if (entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return QString::fromStdString(controlledEntity.getEntityNode().dynamicModel.entityName);
		}
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString smartEntityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	QString name;

	name = entityName(controlledEntity);

	if (name.isEmpty())
	{
		name = uniqueIdentifierToString(controlledEntity.getEntity().getEntityID());
	}

	return name;
}

QString smartEntityName(hive::modelsLibrary::DiscoveredEntitiesModel::Entity const& entity) noexcept
{
	QString name;

	name = entity.name;

	if (name.isEmpty())
	{
		name = uniqueIdentifierToString(entity.entityID);
	}

	return name;
}

QString groupName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	try
	{
		auto const& entity = controlledEntity.getEntity();

		if (entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return QString::fromStdString(controlledEntity.getEntityNode().dynamicModel.groupName);
		}
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString outputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
{
	try
	{
		auto const& entityNode = controlledEntity.getEntityNode();
		auto const& streamNode = controlledEntity.getStreamOutputNode(entityNode.dynamicModel.currentConfiguration, streamIndex);
		return objectName(&controlledEntity, entityNode.dynamicModel.currentConfiguration, streamNode);
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString inputStreamName(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::StreamIndex const streamIndex) noexcept
{
	try
	{
		auto const& entityNode = controlledEntity.getEntityNode();
		auto const& streamNode = controlledEntity.getStreamInputNode(entityNode.dynamicModel.currentConfiguration, streamIndex);
		return objectName(&controlledEntity, entityNode.dynamicModel.currentConfiguration, streamNode);
	}
	catch (la::avdecc::controller::ControlledEntity::Exception const&)
	{
		// Ignore exception
	}
	catch (...)
	{
		// Uncaught exception
		AVDECC_ASSERT(false, "Uncaught exception");
	}
	return {};
}

QString redundantOutputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept
{
	return QString{ "Redundant Stream Output %1" }.arg(QString::number(redundantIndex));
}

QString redundantInputName(la::avdecc::controller::model::VirtualIndex const redundantIndex) noexcept
{
	return QString{ "Redundant Stream Input %1" }.arg(QString::number(redundantIndex));
}

QString toUpperCamelCase(std::string const& text) noexcept
{
#pragma message("TODO: Use a regex, if possible")
	auto output = std::string{};

	auto shouldUpperCase = true;
	for (auto const c : text)
	{
		if (c == '_')
		{
			output.push_back(' ');
			shouldUpperCase = true;
		}
		else if (shouldUpperCase)
		{
			output.push_back(std::toupper(c));
			shouldUpperCase = false;
		}
		else
		{
			output.push_back(std::tolower(c));
		}
	}

	return QString::fromStdString(output);
}

namespace
{
struct VendorNameMaps
{
	std::unordered_map<std::uint32_t, QString> oui24ToName{};
	std::unordered_map<std::uint32_t, QString> oui28ToName{};
	std::unordered_map<std::uint64_t, QString> oui36ToName{};
};

// Loads the OUI database on first call (thread-safe magic static, concurrent callers block until loaded).
// The maps are immutable once built, so lookups require no synchronization.
VendorNameMaps const& getVendorNameMaps() noexcept
{
	static auto const s_maps = []()
	{
		auto maps = VendorNameMaps{};
		auto jsonFile = QFile{ ":/oui.json" };
		if (jsonFile.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			// Read file
			try
			{
				auto const jsonContent = json::parse(jsonFile.readAll().toStdString());

				// Reads the given table, if present, converting each "key" to hex and each "value" to string
				auto const loadTable = [&jsonContent](auto& ouiToName, char const* const tableName)
				{
					using KeyType = typename std::decay_t<decltype(ouiToName)>::key_type;
					if (auto const it = jsonContent.find(tableName); it != jsonContent.end())
					{
						for (auto const& [key, value] : it->items())
						{
							auto const oui = la::avdecc::utils::convertFromString<KeyType>(key.c_str());
							auto const& vendorName = value.get<std::string>();
							ouiToName.emplace(std::make_pair(oui, QString::fromStdString(vendorName)));
						}
					}
				};

				loadTable(maps.oui24ToName, "oui_24");
				loadTable(maps.oui28ToName, "oui_28");
				loadTable(maps.oui36ToName, "oui_36");
			}
			catch (...)
			{
				// Ignore exception
			}
		}
		return maps;
	}();
	return s_maps;
}

// Returns the vendor name registered in the given table for the OUI of the given identifier, or nullptr if not found.
// The returned pointer is valid for the whole lifetime of the program, the tables being immutable statics.
template<la::avdecc::OuiType Type, typename MapType>
QString const* findVendorName(MapType const& ouiToName, la::avdecc::UniqueIdentifier const entityID) noexcept
{
	auto const nameIt = ouiToName.find(entityID.getVendorID<Type>());
	return nameIt == ouiToName.end() ? nullptr : &nameIt->second;
}
} // namespace

void warmUpVendorNamesCache() noexcept
{
	// Parsing the OUI database is slow enough to cause a noticeable main thread freeze (especially in debug builds), warm the cache up front from a background thread so the first getVendorName() caller, whichever thread it is on, doesn't pay for it
	std::thread{ []()
		{
			getVendorNameMaps();
		} }
		.detach();
}

QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	auto const& maps = getVendorNameMaps();

	// Search each OUI size in turn. The IEEE never assigns overlapping blocks (a 24 bits block is either assigned as a whole, or subdivided into smaller ones), so at most one table can match.
	if (auto const* const name = findVendorName<la::avdecc::OuiType::Oui24>(maps.oui24ToName, entityID); name != nullptr)
	{
		return *name;
	}
	if (auto const* const name = findVendorName<la::avdecc::OuiType::Oui28>(maps.oui28ToName, entityID); name != nullptr)
	{
		return *name;
	}
	if (auto const* const name = findVendorName<la::avdecc::OuiType::Oui36>(maps.oui36ToName, entityID); name != nullptr)
	{
		return *name;
	}

	// If not found, convert to hex string
	return toHexQString(entityID.getVendorID<la::avdecc::OuiType::Oui24>(), true, true);
}

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
