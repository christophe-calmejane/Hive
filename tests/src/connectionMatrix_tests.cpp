/*
* Copyright (C) 2017-2022, Emilien Vallot, Christophe Calmejane and other contributors

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
* @file connectionMatrix_tests.cpp
* @author Christophe Calmejane
*/

#include <gtest/gtest.h>
#include <hive/modelsLibrary/controllerManager.hpp>
#include <connectionMatrix/model.hpp>

#include <QString>
#include <QModelIndex>
#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 4127) // Disable conditional expression is constant
#endif
#include <QTest>
#ifdef _WIN32
#	pragma warning(pop)
#endif
namespace
{
class ConnectionMatrix_F : public ::testing::Test
{
public:
	virtual void SetUp() override
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();

		// Create a controller
		try
		{
			controllerManager.createController(la::avdecc::protocol::ProtocolInterface::Type::Virtual, "Unit Tests", 0x0001, la::avdecc::UniqueIdentifier::getNullUniqueIdentifier(), "en", nullptr);
		}
		catch (la::avdecc::controller::Controller::Exception const&)
		{
			ASSERT_FALSE(true);
		}

		// Configure the model
		_model.setMode(connectionMatrix::Model::Mode::Stream);
		_model.setTransposed(false);
	}

	virtual void TearDown() override
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		controllerManager.destroyController();
	}

	void loadNetworkState(QString const& filePath)
	{
		auto& controllerManager = hive::modelsLibrary::ControllerManager::getInstance();
		auto const flags = la::avdecc::entity::model::jsonSerializer::Flags{ la::avdecc::entity::model::jsonSerializer::Flag::ProcessADP, la::avdecc::entity::model::jsonSerializer::Flag::ProcessCompatibility, la::avdecc::entity::model::jsonSerializer::Flag::ProcessDynamicModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessMilan, la::avdecc::entity::model::jsonSerializer::Flag::ProcessState, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStaticModel, la::avdecc::entity::model::jsonSerializer::Flag::ProcessStatistics };
		auto const [err, msg] = controllerManager.loadVirtualEntitiesFromJsonNetworkState(filePath, flags);
		ASSERT_EQ(la::avdecc::jsonSerializer::DeserializationError::NoError, err) << "Failed to load NetworkState file";
		QTest::qWait(0); // Flush Qt EventLoop
	}

	connectionMatrix::Model& getModel() noexcept
	{
		return _model;
	}

	void validateIntersectionData(int const talkerSection, int const listenerSection, connectionMatrix::Model::IntersectionData::Type const intersectionType, connectionMatrix::Model::IntersectionData::State const intersectionState, connectionMatrix::Model::IntersectionData::Flags const intersectionFlags) noexcept
	{
		auto& model = getModel();
		auto const& data = model.intersectionData(model.getIntersectionIndex(talkerSection, listenerSection));
		ASSERT_EQ(intersectionType, data.type);
		EXPECT_EQ(intersectionState, data.state);
		EXPECT_TRUE((intersectionFlags == data.flags));
	}

private:
	connectionMatrix::Model _model{ nullptr };
	int x{ 0 };
	QApplication _app{ x, nullptr };
};
} // namespace

/* *********************************
   Redundant Redundant Summary
*/
TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedWrongDomain_LinkDown)
{
	loadNetworkState("data/connectionMatrix/10-Redundant_Redundant-ConnectedWrongDomain_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_WrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/11-Redundant_Redundant-WrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedWrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/12-Redundant_Redundant-ConnectedWrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedWrongFormat_LinkDown)
{
	loadNetworkState("data/connectionMatrix/13-Redundant_Redundant-ConnectedWrongFormat_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_WrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/14-Redundant_Redundant-WrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedWrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/15-Redundant_Redundant-ConnectedWrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedNoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/16-Redundant_Redundant-ConnectedNoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{});
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_NoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/17-Redundant_Redundant-NoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedNoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/18-Redundant_Redundant-ConnectedNoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedNoError_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/19-Redundant_Redundant-ConnectedNoError_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_ConnectedWrongFormat_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/20-Redundant_Redundant-ConnectedWrongFormat_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, RedundantRedundantSummary_RedundantRedundant_NoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/21-Redundant_Redundant-NoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(9, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	validateIntersectionData(12, 1, connectionMatrix::Model::IntersectionData::Type::Redundant_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatImpossible, connectionMatrix::Model::IntersectionData::Flag::WrongFormatType });
}


/* *********************************
   Entity Stream Summary
*/
TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedNoError_WrongFormat)
{
	loadNetworkState("data/connectionMatrix/1-Normal_Normal-ConnectedNoError_WrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_NoError_ConnectedWrongFormat)
{
	loadNetworkState("data/connectionMatrix/2-Normal_Normal-NoError_ConnectedWrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
}
#pragma message("TODO: Ajouter un fichier comme 2-Normal_Normal-NoError_ConnectedWrongFormat.json, mais avec un WrongFormatImpossible de connecté (mediaclock sur audio), et faire le TU")

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedNoError_ConnectedWrongFormat)
{
	loadNetworkState("data/connectionMatrix/3-Normal_Normal-ConnectedNoError_ConnectedWrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedWrongDomain_WrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/4-Normal_Normal-ConnectedWrongDomain_WrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_WrongDomain_ConnectedWrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/5-Normal_Normal-WrongDomain_ConnectedWrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedWrongDomain_ConnectedWrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/6-Normal_Normal-ConnectedWrongDomain_ConnectedWrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedNoError_NoError)
{
	loadNetworkState("data/connectionMatrix/7-Normal_Normal-ConnectedNoError_NoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_NoError_ConnectedNoError)
{
	loadNetworkState("data/connectionMatrix/8-Normal_Normal-NoError_ConnectedNoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_NormalNormal_ConnectedNoError_ConnectedNoError)
{
	loadNetworkState("data/connectionMatrix/9-Normal_Normal-ConnectedNoError_ConnectedNoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Stream0
	{
		validateIntersectionData(5, 1, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Talker - Stream1
	{
		validateIntersectionData(5, 2, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream0 - Listener
	{
		validateIntersectionData(6, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
	// Stream1 - Listener
	{
		validateIntersectionData(7, 0, connectionMatrix::Model::IntersectionData::Type::Entity_SingleStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedWrongDomain_LinkDown)
{
	loadNetworkState("data/connectionMatrix/10-Redundant_Redundant-ConnectedWrongDomain_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_WrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/11-Redundant_Redundant-WrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedWrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/12-Redundant_Redundant-ConnectedWrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedWrongFormat_LinkDown)
{
	loadNetworkState("data/connectionMatrix/13-Redundant_Redundant-ConnectedWrongFormat_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_WrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/14-Redundant_Redundant-WrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedWrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/15-Redundant_Redundant-ConnectedWrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedNoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/16-Redundant_Redundant-ConnectedNoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_NoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/17-Redundant_Redundant-NoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedNoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/18-Redundant_Redundant-ConnectedNoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown, connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedNoError_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/19-Redundant_Redundant-ConnectedNoError_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_ConnectedWrongFormat_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/20-Redundant_Redundant-ConnectedWrongFormat_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

TEST_F(ConnectionMatrix_F, EntityStreamSummary_RedundantRedundant_NoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/21-Redundant_Redundant-NoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	// Talker - Redundant0
	{
		validateIntersectionData(8, 1, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Primary
	{
		validateIntersectionData(8, 2, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream0 Secondary
	{
		validateIntersectionData(8, 3, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - Redundant1
	{
		validateIntersectionData(8, 4, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Primary
	{
		validateIntersectionData(8, 5, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Talker - RedundantStream1 Secondary
	{
		validateIntersectionData(8, 6, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant0 - Listener
	{
		validateIntersectionData(9, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Primary - Listener
	{
		validateIntersectionData(10, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream0 Secondary - Listener
	{
		validateIntersectionData(11, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// Redundant1 - Listener
	{
		validateIntersectionData(12, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Redundant, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Primary - Listener
	{
		validateIntersectionData(13, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
	// RedundantStream1 Secondary - Listener
	{
		validateIntersectionData(14, 0, connectionMatrix::Model::IntersectionData::Type::Entity_RedundantStream, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
	}
}

/* *********************************
   Entity Entity Summary
*/
TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedNoError_WrongFormat)
{
	loadNetworkState("data/connectionMatrix/1-Normal_Normal-ConnectedNoError_WrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_NoError_ConnectedWrongFormat)
{
	loadNetworkState("data/connectionMatrix/2-Normal_Normal-NoError_ConnectedWrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedNoError_ConnectedWrongFormat)
{
	loadNetworkState("data/connectionMatrix/3-Normal_Normal-ConnectedNoError_ConnectedWrongFormat.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedWrongDomain_WrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/4-Normal_Normal-ConnectedWrongDomain_WrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_WrongDomain_ConnectedWrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/5-Normal_Normal-WrongDomain_ConnectedWrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedWrongDomain_ConnectedWrongFormatWrongDomain)
{
	loadNetworkState("data/connectionMatrix/6-Normal_Normal-ConnectedWrongDomain_ConnectedWrongFormatWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedNoError_NoError)
{
	loadNetworkState("data/connectionMatrix/7-Normal_Normal-ConnectedNoError_NoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_NoError_ConnectedNoError)
{
	loadNetworkState("data/connectionMatrix/8-Normal_Normal-NoError_ConnectedNoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_NormalNormal_ConnectedNoError_ConnectedNoError)
{
	loadNetworkState("data/connectionMatrix/9-Normal_Normal-ConnectedNoError_ConnectedNoError.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(5, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::MediaLocked });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedWrongDomain_LinkDown)
{
	loadNetworkState("data/connectionMatrix/10-Redundant_Redundant-ConnectedWrongDomain_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_WrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/11-Redundant_Redundant-WrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedWrongDomain_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/12-Redundant_Redundant-ConnectedWrongDomain_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedWrongFormat_LinkDown)
{
	loadNetworkState("data/connectionMatrix/13-Redundant_Redundant-ConnectedWrongFormat_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_WrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/14-Redundant_Redundant-WrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedWrongFormat_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/15-Redundant_Redundant-ConnectedWrongFormat_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedNoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/16-Redundant_Redundant-ConnectedNoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{});
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_NoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/17-Redundant_Redundant-NoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::PartiallyConnected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedNoError_ConnectedLinkDown)
{
	loadNetworkState("data/connectionMatrix/18-Redundant_Redundant-ConnectedNoError_ConnectedLinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::InterfaceDown });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedNoError_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/19-Redundant_Redundant-ConnectedNoError_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_ConnectedWrongFormat_ConnectedWrongDomain)
{
	loadNetworkState("data/connectionMatrix/20-Redundant_Redundant-ConnectedWrongFormat_ConnectedWrongDomain.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::Connected, connectionMatrix::Model::IntersectionData::Flags{ connectionMatrix::Model::IntersectionData::Flag::WrongDomain, connectionMatrix::Model::IntersectionData::Flag::WrongFormatPossible });
}

TEST_F(ConnectionMatrix_F, EntityEntitySummary_RedundantRedundant_NoError_LinkDown)
{
	loadNetworkState("data/connectionMatrix/21-Redundant_Redundant-NoError_LinkDown.json");
	if (HasFatalFailure())
	{
		return;
	}
	validateIntersectionData(8, 0, connectionMatrix::Model::IntersectionData::Type::Entity_Entity, connectionMatrix::Model::IntersectionData::State::NotConnected, connectionMatrix::Model::IntersectionData::Flags{});
}
