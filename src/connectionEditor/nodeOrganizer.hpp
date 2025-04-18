/*
* Copyright (C) 2017-2025, Emilien Vallot, Christophe Calmejane and other contributors

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

#include <QObject>
#include <QVariantAnimation>
#include <QPropertyAnimation>

#include <QtMate/flow/flowDefs.hpp>

class NodeOrganizer : public QObject
{
public:
	NodeOrganizer(qtMate::flow::FlowScene* scene, QObject* parent = nullptr);
	virtual ~NodeOrganizer() override;

private:
	void updateNodeData(qtMate::flow::FlowNodeUid const& uid);
	void doLayout();
	void animateTo(qtMate::flow::FlowNode* node, float x, float y);

private:
	qtMate::flow::FlowScene* _scene{};
	QPropertyAnimation* _sceneRectAnimation{};

	struct NodeData
	{
		qtMate::flow::FlowNode* node{};
		int activeInputCount{};
		int activeOutputCount{};
	};

	QHash<qtMate::flow::FlowNodeUid, NodeData> _nodeData{};
	QHash<qtMate::flow::FlowNodeUid, QVariantAnimation*> _animations{};
};
