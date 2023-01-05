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

#pragma once

#include "hive/modelsLibrary/commandsExecutor.hpp"

namespace hive
{
namespace modelsLibrary
{
class CommandsExecutorImpl final : public CommandsExecutor
{
public:
	using CompletionHandler = std::function<void(CommandsExecutorImpl const* const executor)>;

	// CommandsExecutor overrides
	virtual void clear() noexcept override;
	virtual bool isValid() const noexcept override;
	virtual explicit operator bool() const noexcept override;
	virtual ControllerManager* getControllerManager() noexcept override;
	virtual la::avdecc::UniqueIdentifier getEntityID() const noexcept override;
	virtual void addCommand(Command&& command) noexcept override;
	virtual void processAECPResult(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept override;

	/** Starts the execution of the commands */
	void exec() noexcept;

	/** Invalidate the executor, preventing any further processing */
	void invalidate() noexcept;

	/** Sets a completion handler to be called when the executor completed it's tasks (with or without success) */
	void setCompletionHandler(CompletionHandler const& completionHandler) noexcept;

	/** Constructor */
	CommandsExecutorImpl(ControllerManager* const manager, la::avdecc::UniqueIdentifier const entityID, bool const requestExclusiveAccess) noexcept;

	/** Destructor */
	virtual ~CommandsExecutorImpl() noexcept;

private:
	friend ControllerManager;
	void signalResult(ExecutorResult const result) noexcept;
	void processNext() noexcept;

	ControllerManager* _manager{ nullptr };
	la::avdecc::UniqueIdentifier _entityID{};
	bool _requestExclusiveAccess{ false };
	CompletionHandler _completionHandler{ nullptr };
	la::avdecc::controller::Controller::ExclusiveAccessToken::UniquePointer _exclusiveAccessToken{ nullptr, nullptr };
	std::vector<Command> _commands{};
	decltype(_commands)::size_type _nextCommand{ 0u };
};

} // namespace modelsLibrary
} // namespace hive
