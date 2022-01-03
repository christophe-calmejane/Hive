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


#include "commandsExecutorImpl.hpp"
#include "hive/modelsLibrary/controllerManager.hpp"

#include <la/avdecc/utils.hpp>

namespace hive
{
namespace modelsLibrary
{
// CommandsExecutor overrides
void CommandsExecutorImpl::clear() noexcept
{
	_commands.clear();
}

bool CommandsExecutorImpl::isValid() const noexcept
{
	return _manager && _entityID && !_commands.empty();
}

CommandsExecutorImpl::operator bool() const noexcept
{
	return isValid();
}

ControllerManager* CommandsExecutorImpl::getControllerManager() noexcept
{
	return _manager;
}

la::avdecc::UniqueIdentifier CommandsExecutorImpl::getEntityID() const noexcept
{
	return _entityID;
}

void CommandsExecutorImpl::addCommand(Command&& command) noexcept
{
	_commands.emplace_back(std::move(command));
}

void CommandsExecutorImpl::processAECPResult(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept
{
	if (!status)
	{
		signalResult(ExecutorResult{ ExecutorResult::Result::AemError, status });
		return;
	}
	processNext();
}

void CommandsExecutorImpl::signalResult(ExecutorResult const result) noexcept
{
	// Clear token
	_exclusiveAccessToken = {};

	// Signal result in main thread (always Queue the message)
	if (!_commands.empty())
	{
		QMetaObject::invokeMethod(this,
			[this, result]()
			{
				emit executionComplete(result);
			},
			Qt::QueuedConnection);
	}

	// Call completion handler during next event loop
	QMetaObject::invokeMethod(this,
		[this]()
		{
			la::avdecc::utils::invokeProtectedHandler(_completionHandler, this);
		},
		Qt::QueuedConnection);
}

void CommandsExecutorImpl::processNext() noexcept
{
	if (_nextCommand >= _commands.size())
	{
		signalResult(ExecutorResult{});
		return;
	}

	// Signal progress in main thread (always Queue the message)
	QMetaObject::invokeMethod(this,
		[this, pos = _nextCommand + 1]()
		{
			emit executionProgress(static_cast<std::size_t>(pos), static_cast<std::size_t>(_commands.size()));
		},
		Qt::QueuedConnection);

	// Get next command to execute
	auto const& command = _commands[_nextCommand];
	++_nextCommand;

	// Execute command
	command();
}

void CommandsExecutorImpl::exec() noexcept
{
	if (!_commands.empty() && _requestExclusiveAccess)
	{
		_manager->requestExclusiveAccess(_entityID, la::avdecc::controller::Controller::ExclusiveAccessToken::AccessType::Lock,
			[this](auto const entityID, auto const status, auto&& token)
			{
				// Failed to get the exclusive access
				if (!status || !token)
				{
					switch (status)
					{
							// If the device does not support the exclusive access, still proceed
						case la::avdecc::entity::ControllerEntity::AemCommandStatus::NotImplemented:
						case la::avdecc::entity::ControllerEntity::AemCommandStatus::NotSupported:
							break;
						// Otherwise signal the error result and return
						case la::avdecc::entity::ControllerEntity::AemCommandStatus::UnknownEntity:
							signalResult(CommandsExecutor::ExecutorResult{ CommandsExecutor::ExecutorResult::Result::UnknownEntity, status });
						default:
							signalResult(CommandsExecutor::ExecutorResult{ CommandsExecutor::ExecutorResult::Result::AemError, status });
							return;
					}
				}
				// Save the token
				_exclusiveAccessToken = std::move(token);
				// Process first command
				processNext();
			});
	}
	else
	{
		// Process first command
		processNext();
	}
}

void CommandsExecutorImpl::invalidate() noexcept
{
	_manager = nullptr;
	_entityID = {};
	_completionHandler = nullptr;
}

void CommandsExecutorImpl::setCompletionHandler(CompletionHandler const& completionHandler) noexcept
{
	_completionHandler = completionHandler;
}

/** Constructor */
CommandsExecutorImpl::CommandsExecutorImpl(ControllerManager* const manager, la::avdecc::UniqueIdentifier const entityID, bool const requestExclusiveAccess) noexcept
	: _manager{ manager }
	, _entityID{ entityID }
	, _requestExclusiveAccess{ requestExclusiveAccess }
{
}

/** Destructor */
CommandsExecutorImpl::~CommandsExecutorImpl() noexcept
{
	//
}

CommandsExecutor::ExecutorResult::ExecutorResult(CommandsExecutor::ExecutorResult::Result const result) noexcept
	: _result{ result }
{
}
CommandsExecutor::ExecutorResult::ExecutorResult(CommandsExecutor::ExecutorResult::Result const result, la::avdecc::entity::ControllerEntity::AemCommandStatus const aemStatus) noexcept
	: _result{ result }
	, _aemStatus{ aemStatus }
{
}

CommandsExecutor::ExecutorResult::Result CommandsExecutor::ExecutorResult::getResult() const noexcept
{
	return _result;
}

la::avdecc::entity::ControllerEntity::AemCommandStatus CommandsExecutor::ExecutorResult::getAemStatus() const noexcept
{
	AVDECC_ASSERT(_result == Result::AemError, "getAemStatus() expected to be called only with Result::AemError");
	return _aemStatus;
}


} // namespace modelsLibrary
} // namespace hive
