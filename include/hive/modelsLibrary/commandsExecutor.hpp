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

#include <la/avdecc/controller/avdeccController.hpp>
#include <la/avdecc/internals/uniqueIdentifier.hpp>

#include <QObject>

#include <functional>
#include <utility>

namespace hive
{
namespace modelsLibrary
{
class ControllerManager;
/**
 * @Brief Sequential commands executor
 * @Details Simple executor that will sequentially execute pre-registered commands.
 *          Get a CommandsExecutor from ControllerManager.
 */
class CommandsExecutor : public QObject
{
	Q_OBJECT
public:
	class ExecutorResult final
	{
	public:
		enum class Result
		{
			Success = 0, /**< Successfully executed */
			Aborted = 1, /**< Aborted, either by the user or the controller manager */
			UnknownEntity = 2, /**< Unknown Entity */
			AemError = 3, /**< Encountered an AEM Error, call getAemStatus to get more details */
			InternalError = 99, /**< An internal error occured */
		};

		ExecutorResult() noexcept = default;
		ExecutorResult(Result const result) noexcept;
		ExecutorResult(Result const result, la::avdecc::entity::ControllerEntity::AemCommandStatus const aemStatus) noexcept;

		Result getResult() const noexcept;
		la::avdecc::entity::ControllerEntity::AemCommandStatus getAemStatus() const noexcept;

	private:
		Result _result{ Result::Success };
		la::avdecc::entity::ControllerEntity::AemCommandStatus _aemStatus{ la::avdecc::entity::ControllerEntity::AemCommandStatus::Success };
	};

	/** Registers a ControllerManager AECP command to be executed */
	template<typename ManagerMethod, typename... Parameters>
	void addAemCommand(ManagerMethod&& method, Parameters&&... params) noexcept
	{
		addCommand(std::bind(std::forward<ManagerMethod>(method), getControllerManager(), getEntityID(), std::forward<Parameters>(params)...,
			[](la::avdecc::UniqueIdentifier const /*entityID*/)
			{
				// TODO: Might be used for progress notification
			},
			[this](la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status)
			{
				processAECPResult(entityID, status);
			}));
	}

	/** Removes all commands from the executor */
	virtual void clear() noexcept = 0;

	/** Returns true if the executor is valid (ie. the entity is valid and at least one command is registered) */
	virtual bool isValid() const noexcept = 0;

	/** Executor validity bool operator (equivalent to isValid()). */
	virtual explicit operator bool() const noexcept = 0;

	// Public signals
	/** Signal raised before each command is executed, with current starting at 1 up to maxumum. */
	Q_SIGNAL void executionProgress(std::size_t const current, std::size_t maximum);
	/** Signal raised when the execution completes, either successfully or not. Will not be raised if the executor is empty. */
	Q_SIGNAL void executionComplete(hive::modelsLibrary::CommandsExecutor::ExecutorResult const result);

	// Deleted compiler auto-generated methods
	CommandsExecutor(CommandsExecutor const&) = delete;
	CommandsExecutor(CommandsExecutor&&) = delete;
	CommandsExecutor& operator=(CommandsExecutor const&) = delete;
	CommandsExecutor& operator=(CommandsExecutor&&) = delete;

protected:
	using Command = std::function<void()>;

	/** Constructor */
	CommandsExecutor() noexcept;

	/** Destructor */
	virtual ~CommandsExecutor() noexcept;

private:
	virtual ControllerManager* getControllerManager() noexcept = 0;
	virtual la::avdecc::UniqueIdentifier getEntityID() const noexcept = 0;
	virtual void addCommand(Command&& command) noexcept = 0;
	virtual void processAECPResult(la::avdecc::UniqueIdentifier const entityID, la::avdecc::entity::ControllerEntity::AemCommandStatus const status) noexcept = 0;
};

} // namespace modelsLibrary
} // namespace hive
