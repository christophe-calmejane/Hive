/*
* Copyright (C) 2017-2023, Emilien Vallot, Christophe Calmejane and other contributors

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

QString macAddressToString(la::networkInterface::MacAddress const& macAddress)
{
	return QString::fromStdString(la::networkInterface::NetworkInterfaceHelper::macAddressToString(macAddress));
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
	return localizedName.data();
}

QString localizedString(la::avdecc::controller::ControlledEntity const& controlledEntity, la::avdecc::entity::model::LocalizedStringReference const stringReference) noexcept
{
	auto const& localizedName = controlledEntity.getLocalizedString(stringReference);

	if (localizedName.empty())
	{
		return "(No Localization)";
	}
	return localizedName.data();
}

QString entityName(la::avdecc::controller::ControlledEntity const& controlledEntity) noexcept
{
	try
	{
		auto const& entity = controlledEntity.getEntity();

		if (entity.getEntityCapabilities().test(la::avdecc::entity::EntityCapability::AemSupported))
		{
			return controlledEntity.getEntityNode().dynamicModel.entityName.data();
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
			return controlledEntity.getEntityNode().dynamicModel.groupName.data();
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
		return objectName(&controlledEntity, streamNode);
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
		return objectName(&controlledEntity, streamNode);
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

QString getVendorName(la::avdecc::UniqueIdentifier const entityID) noexcept
{
	static auto s_fileLoaded = false;
	static auto s_oui24ToName = std::unordered_map<std::uint32_t, QString>{};
	static auto s_oui36ToName = std::unordered_map<std::uint64_t, QString>{};

	// If file not loaded, load it
	if (!s_fileLoaded)
	{
		s_fileLoaded = true;

		auto jsonFile = QFile{ ":/oui.json" };
		if (jsonFile.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			// Read file
			try
			{
				auto const jsonContent = json::parse(jsonFile.readAll().toStdString());

				// Read oui_24 key, if present
				if (auto it = jsonContent.find("oui_24"); it != jsonContent.end())
				{
					// Read each entry, converting "key" to hex and "value" to string
					for (auto const& [key, value] : it->items())
					{
						auto const oui24 = la::avdecc::utils::convertFromString<std::uint32_t>(key.c_str());
						auto const& vendorName = value.get<std::string>();
						s_oui24ToName.emplace(std::make_pair(oui24, QString::fromStdString(vendorName)));
					}
				}
			}
			catch (...)
			{
				// Ignore exception
			}
		}
	}

	// First search in OUI-24
	{
		auto const nameIt = s_oui24ToName.find(entityID.getVendorID<std::uint32_t>());
		if (nameIt != s_oui24ToName.end())
		{
			return nameIt->second;
		}
	}

	// Then search in OUI-36
	{
		auto const nameIt = s_oui36ToName.find(entityID.getVendorID<std::uint64_t>());
		if (nameIt != s_oui36ToName.end())
		{
			return nameIt->second;
		}
	}

	// If not found, convert to hex string
	return toHexQString(entityID.getVendorID<std::uint32_t>(), true, true);
}

} // namespace helper
} // namespace modelsLibrary
} // namespace hive
