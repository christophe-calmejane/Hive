#pragma once

#include <QtMate/flow/flowlink.hpp>
#include <QtMate/flow/flowdefs.hpp>

namespace qtMate::flow {

class FlowConnection : public FlowLink {
public:
	FlowConnection(QGraphicsItem* parent = nullptr);
	virtual ~FlowConnection() override;

	FlowConnectionDescriptor descriptor() const;

	void setOutput(FlowOutput* output);
	FlowOutput* output() const;

	void setInput(FlowInput* input);
	FlowInput* input() const;

private:
	void updatePath();

private:
	friend class FlowNode;

	FlowOutput* _output{};
	FlowInput* _input{};
};

} // namespace qtMate::flow
