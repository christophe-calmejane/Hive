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

#include "commandChain.hpp"
#include "controllerManager.hpp"
#include "helper.hpp"
#include <la/avdecc/internals/streamFormatInfo.hpp>
#include <atomic>
#include <optional>
#include <unordered_set>
#include <math.h>

namespace avdecc
{
namespace commandChain
{
/////////////////////////////////////////////////////////////////////////////////////////

CommandExecutionError AsyncParallelCommandSet::controlStatusToCommandError(la::avdecc::entity::ControllerEntity::ControlStatus status)
{
	switch (status)
	{
		case la::avdecc::entity::LocalEntity::ControlStatus::Success:
			return CommandExecutionError::NoError;
		case la::avdecc::entity::LocalEntity::ControlStatus::TimedOut:
			return CommandExecutionError::Timeout;
		case la::avdecc::entity::LocalEntity::ControlStatus::NetworkError:
		case la::avdecc::entity::LocalEntity::ControlStatus::ProtocolError:
			return CommandExecutionError::NetworkIssue;
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerMisbehaving:
		case la::avdecc::entity::LocalEntity::ControlStatus::ListenerMisbehaving:
			return CommandExecutionError::EntityError;
		case la::avdecc::entity::LocalEntity::ControlStatus::ListenerUnknownID:
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerUnknownID:
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerDestMacFail:
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoStreamIndex:
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerNoBandwidth:
		case la::avdecc::entity::LocalEntity::ControlStatus::TalkerExclusive:
		case la::avdecc::entity::LocalEntity::ControlStatus::ListenerTalkerTimeout:
		case la::avdecc::entity::LocalEntity::ControlStatus::ListenerExclusive:
		case la::avdecc::entity::LocalEntity::ControlStatus::StateUnavailable:
		case la::avdecc::entity::LocalEntity::ControlStatus::NotConnected:
		case la::avdecc::entity::LocalEntity::ControlStatus::NoSuchConnection:
		case la::avdecc::entity::LocalEntity::ControlStatus::CouldNotSendMessage:
		case la::avdecc::entity::LocalEntity::ControlStatus::ControllerNotAuthorized:
		case la::avdecc::entity::LocalEntity::ControlStatus::IncompatibleRequest:
		case la::avdecc::entity::LocalEntity::ControlStatus::UnknownEntity:
		case la::avdecc::entity::LocalEntity::ControlStatus::InternalError:
			return CommandExecutionError::CommandFailure;
		case la::avdecc::entity::LocalEntity::ControlStatus::NotSupported:
			return CommandExecutionError::NotSupported;
		default:
			return CommandExecutionError::CommandFailure;
	}
}

CommandExecutionError AsyncParallelCommandSet::aemCommandStatusToCommandError(la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
{
	switch (status)
	{
		case la::avdecc::entity::LocalEntity::AemCommandStatus::Success:
			return CommandExecutionError::NoError;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::TimedOut:
			return CommandExecutionError::Timeout;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::AcquiredByOther:
			return CommandExecutionError::AcquiredByOther;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::LockedByOther:
			return CommandExecutionError::LockedByOther;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NetworkError:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::ProtocolError:
			return CommandExecutionError::NetworkIssue;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::EntityMisbehaving:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NotImplemented:
			return CommandExecutionError::EntityError;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NotSupported:
			return CommandExecutionError::NotSupported;
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NoSuchDescriptor:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NotAuthenticated:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::AuthenticationDisabled:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::BadArguments:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::NoResources:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::InProgress:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::StreamIsRunning:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::UnknownEntity:
		case la::avdecc::entity::LocalEntity::AemCommandStatus::InternalError:
			return CommandExecutionError::CommandFailure;
		default:
			return CommandExecutionError::CommandFailure;
	}
}

/**
		* Default constructor.
		*/
AsyncParallelCommandSet::AsyncParallelCommandSet() {}

/**
		* Constructor taking a single command function.
		*/
AsyncParallelCommandSet::AsyncParallelCommandSet(AsyncCommand command)
{
	_commands.push_back(command);
}

/**
		* Constructor taking multiple command functions.
		*/
AsyncParallelCommandSet::AsyncParallelCommandSet(std::vector<AsyncCommand> commands)
{
	_commands.insert(std::end(_commands), std::begin(commands), std::end(commands));
}

/**
		* Appends a command function to the internal list.
		*/
void AsyncParallelCommandSet::append(AsyncCommand command)
{
	_commands.push_back(command);
}

/**
		* Appends a command function to the internal list.
		*/
void AsyncParallelCommandSet::append(std::vector<AsyncCommand> commands)
{
	_commands.insert(std::end(_commands), std::begin(commands), std::end(commands));
}

/**
		* Adds error info for acmp commands.
		*/
void AsyncParallelCommandSet::addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error, avdecc::ControllerManager::AcmpCommandType commandType)
{
	CommandErrorInfo info{ error };
	info.commandTypeAcmp = commandType;
	_errors.emplace(entityId, info);
}

/**
		* Adds error info for aecp commands.
		*/
void AsyncParallelCommandSet::addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error, avdecc::ControllerManager::AecpCommandType commandType)
{
	CommandErrorInfo info{ error };
	info.commandTypeAecp = commandType;
	_errors.emplace(entityId, info);
}

/**
		* Adds general error info (without command relation).
		*/
void AsyncParallelCommandSet::addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error)
{
	CommandErrorInfo info{ error };
	_errors.emplace(entityId, info);
}

/**
		* Gets the count of commands.
		*/
size_t AsyncParallelCommandSet::parallelCommandCount() const noexcept
{
	return _commands.size();
}

/**
		* Executes all commands. Eventually emits commandSetCompleted if none of the commands has anything to do.
		*/
void AsyncParallelCommandSet::exec() noexcept
{
	if (_commands.empty())
	{
		invokeCommandCompleted(0, false);
		return;
	}
	int index = 0;
	for (auto const& command : _commands)
	{
		if (!command(this, index))
		{
			invokeCommandCompleted(index, false);
		}
		index++;
	}
}

/**
		* After a command was executed, this is called.
		*/
void AsyncParallelCommandSet::invokeCommandCompleted(int commandIndex, bool error) noexcept
{
	if (error)
	{
		_errorOccured = true;
		_commandCompletionCounter++;
	}
	else
	{
		_commandCompletionCounter++;
	}

	if (_commandCompletionCounter >= static_cast<int>(_commands.size()))
	{
		emit commandSetCompleted(_errors);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
		* Consturctor.
		* Sets up signla slot connections.
		*/
SequentialAsyncCommandExecuter::SequentialAsyncCommandExecuter()
{
	auto& manager = ControllerManager::getInstance();
}

/**
		* Sets the commands to be executed.
		*/
void SequentialAsyncCommandExecuter::setCommandChain(std::vector<AsyncParallelCommandSet*> commands)
{
	int totalCommandCount = 0;
	_commands = commands;
	for (auto* command : _commands)
	{
		command->setParent(this);
		totalCommandCount += command->parallelCommandCount();
	}
	_totalCommandCount = totalCommandCount;
	_completedCommandCount = 0;
	_currentCommandSet = 0;
}

/**
		* Starts or continues the sequence of commands that was set via setCommandChain.
		*/
void SequentialAsyncCommandExecuter::start()
{
	if (_currentCommandSet < static_cast<int>(_commands.size()))
	{
		connect(_commands.at(_currentCommandSet), &AsyncParallelCommandSet::commandSetCompleted, this,
			[this](CommandExecutionErrors errors)
			{
				_errors.insert(errors.begin(), errors.end());
				_completedCommandCount += _commands.at(_currentCommandSet)->parallelCommandCount();
				progressUpdate(_completedCommandCount, _totalCommandCount);

				// start next set
				_currentCommandSet++;
				start();
			});

		_commands.at(_currentCommandSet)->exec();
	}
	else
	{
		// clear the command list once completed.
		qDeleteAll(_commands);
		_commands.clear();

		emit completed(_errors);

		_errors.clear();
	}
}

} // namespace commandChain
} // namespace avdecc
