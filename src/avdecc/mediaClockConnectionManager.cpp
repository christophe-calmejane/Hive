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

#include "mediaClockConnectionManager.hpp"
#include "controllerManager.hpp"
#include <la/avdecc/internals/streamFormat.hpp>
#include <atomic>
#include <QDebug>

namespace avdecc
{
class MediaClockConnectionManagerImpl final : public MediaClockConnectionManager
{
public:
	MediaClockConnectionManagerImpl() noexcept
	{
		// get mapping for every entity.
		// update the mapping for every entity on:
		// -stream connection change on a clock stream that contains the corresponding entity id.
		// -change in the clock domain (currentClockSource) of an entity.
		// -changes inside a chain trigger an update for every connected node...
		//_entityMediaClockMasterMappings.insert();

		auto& manager = avdecc::ControllerManager::getInstance();
		//manager.clockSourceChanged
		connect(&manager, &ControllerManager::streamConnectionChanged, this, &MediaClockConnectionManagerImpl::onStreamConnectionChanged);
		connect(&manager, &ControllerManager::clockSourceChanged, this, &MediaClockConnectionManagerImpl::onClockSourceChanged);
	}

	~MediaClockConnectionManagerImpl() noexcept {}

	Q_SLOT void onStreamConnectionChanged(la::avdecc::controller::model::StreamConnectionState const& streamConnectionState)
	{
		auto& manager = avdecc::ControllerManager::getInstance();
		la::avdecc::controller::ControlledEntity const& controlledEntity = *manager.getControlledEntity(streamConnectionState.listenerStream.entityID);
		la::avdecc::entity::model::DescriptorIndex activeConfigIndex = 0;
		la::avdecc::controller::model::EntityNode entityModel = controlledEntity.getEntityNode();

		const la::avdecc::controller::model::ConfigurationNode configNode = controlledEntity.getCurrentConfigurationNode();

		// find out if the stream connection is a clock stream connection:
		bool isClockStream = false;
		auto const& streamInput = configNode.streamInputs.at(streamConnectionState.listenerStream.streamIndex);
		auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.dynamicModel->streamInfo.streamFormat);
		for (auto const& streamFormat : streamInput.staticModel->formats)
		{
			auto const streamFormatInfo2 = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
			la::avdecc::entity::model::StreamFormatInfo::Type streamType = streamFormatInfo2->getType();
			if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
			{
				isClockStream = true;
			}
		}

		if (isClockStream)
		{
			// potenitally changes every other entity...
			emit mediaClockConnectionsUpdate();
		}
	}

	Q_SLOT void onClockSourceChanged(la::avdecc::UniqueIdentifier const entityId, la::avdecc::entity::model::ClockDomainIndex const clockDomainIndex, la::avdecc::entity::model::ClockSourceIndex const clockSourceIndex)
	{
		// potenitally changes every other entity...
		emit mediaClockConnectionsUpdate();
	}

	virtual const la::avdecc::UniqueIdentifier getMediaClockMaster(la::avdecc::UniqueIdentifier const entityID, MediaClockMasterDetectionError& error) noexcept
	{
		error = MediaClockMasterDetectionError::None;

		QList<la::avdecc::UniqueIdentifier> searchedEntityIds;
		la::avdecc::UniqueIdentifier currentEntityId = entityID;
		searchedEntityIds.append(currentEntityId);
		while (true)
		{
			auto& manager = avdecc::ControllerManager::getInstance();
			la::avdecc::controller::ControlledEntityGuard controlledEntity = manager.getControlledEntity(currentEntityId);
			if (controlledEntity.get() == nullptr)
			{
				error = MediaClockMasterDetectionError::UnknownEntity;
				return 0;
			}
			la::avdecc::entity::model::DescriptorIndex activeConfigIndex = 0;
			la::avdecc::controller::model::EntityNode entityModel = controlledEntity->getEntityNode();

			const la::avdecc::controller::model::ConfigurationNode configNode = controlledEntity->getCurrentConfigurationNode();

			// internal or external?
			bool clockSourceInternal = false;

			// assume there is only one clock domain.
			la::avdecc::entity::model::ClockSourceIndex clockSourceIndex = configNode.clockDomains.at(0).dynamicModel->clockSourceIndex;
			la::avdecc::controller::model::ClockSourceNode activeClockSourceNode = configNode.clockSources.at(clockSourceIndex);

			if (activeClockSourceNode.staticModel->clockSourceType == la::avdecc::entity::model::ClockSourceType::Internal)
			{
				return currentEntityId;
			}
			else
			{
				// find the clock stream:
				la::avdecc::entity::model::StreamIndex clockStreamIndex = 0;
				for (auto const& streamInput : configNode.streamInputs)
				{
					auto const streamFormatInfo = la::avdecc::entity::model::StreamFormatInfo::create(streamInput.second.dynamicModel->streamInfo.streamFormat);
					for (auto const& streamFormat : streamInput.second.staticModel->formats)
					{
						auto const streamFormatInfo2 = la::avdecc::entity::model::StreamFormatInfo::create(streamFormat);
						la::avdecc::entity::model::StreamFormatInfo::Type streamType = streamFormatInfo2->getType();
						if (la::avdecc::entity::model::StreamFormatInfo::Type::ClockReference == streamType)
						{
							clockStreamIndex = streamInput.first;
						}
					}
				}
				la::avdecc::UniqueIdentifier connectedTalker = configNode.streamInputs.at(clockStreamIndex).dynamicModel->connectionState.talkerStream.entityID;
				la::avdecc::entity::model::StreamIndex connectedTalkerStreamIndex = configNode.streamInputs.at(clockStreamIndex).dynamicModel->connectionState.talkerStream.streamIndex;
				if (searchedEntityIds.contains(connectedTalker))
				{
					error = MediaClockMasterDetectionError::Recursive;
					return 0;
				}

				currentEntityId = connectedTalker;
			}
		}
	}

private:
};

MediaClockConnectionManager& MediaClockConnectionManager::getInstance() noexcept
{
	static MediaClockConnectionManagerImpl s_manager{};

	return s_manager;
}

} // namespace avdecc
