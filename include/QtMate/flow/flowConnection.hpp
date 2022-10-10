#pragma once

#include <QtMate/flow/flowLink.hpp>
#include <QtMate/flow/flowDefs.hpp>

namespace qtMate::flow
{
class FlowConnection : public FlowLink
{
public:
	FlowConnection(QGraphicsItem* parent = nullptr);
	virtual ~FlowConnection() override;

	FlowConnectionDescriptor descriptor() const;

	void setOutput(FlowOutput* output);
	FlowOutput* output() const;

	void setInput(FlowInput* input);
	FlowInput* input() const;

	void updatePath();

private:
	friend class FlowNode;

	FlowOutput* _output{};
	FlowInput* _input{};
};

} // namespace qtMate::flow
