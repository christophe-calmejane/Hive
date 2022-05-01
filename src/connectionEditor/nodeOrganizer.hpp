#pragma once

#include <QObject>
#include <QVariantAnimation>

#include<QtMate/flow/flowdefs.hpp>

class NodeOrganizer : public QObject {
public:
	NodeOrganizer(qtMate::flow::FlowScene* scene, QObject* parent = nullptr);
	virtual ~NodeOrganizer() override;

private:
	void updateNodeData(qtMate::flow::FlowNodeUid const& uid);
	void doLayout();
	void animateTo(qtMate::flow::FlowNode* node, float x, float y);

private:
	qtMate::flow::FlowScene* _scene{};

	struct NodeData {
		qtMate::flow::FlowNode* node{};
		int activeInputCount{};
		int activeOutputCount{};;
	};

	QHash<qtMate::flow::FlowNodeUid, NodeData> _nodeData{};
	QHash<qtMate::flow::FlowNodeUid, QVariantAnimation*> _animations{};
};
