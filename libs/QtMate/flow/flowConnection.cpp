#include "QtMate/flow/flowConnection.hpp"
#include "QtMate/flow/flowNode.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"

namespace qtMate::flow
{
FlowConnection::FlowConnection(QGraphicsItem* parent)
	: FlowLink{ parent }
{
}

FlowConnection::~FlowConnection()
{
	setInput(nullptr);
	setOutput(nullptr);
}

FlowConnectionDescriptor FlowConnection::descriptor() const
{
	if (!_output || !_input)
	{
		qWarning() << "querying descriptor on invalid connection";
		return InvalidFlowConnectionDescriptor;
	}

	return FlowConnectionDescriptor{ _output->slot(), _input->slot() };
}

void FlowConnection::setOutput(FlowOutput* output)
{
	if (output != _output)
	{
		if (_output)
		{
			_output->removeConnection(this);
		}
		_output = output;
		if (_output)
		{
			_output->addConnection(this);
		}
	}

	updatePath();
}

FlowOutput* FlowConnection::output() const
{
	return _output;
}

void FlowConnection::setInput(FlowInput* input)
{
	if (input != _input)
	{
		if (_input)
		{
			_input->setConnection(nullptr);
		}
		_input = input;
		if (_input)
		{
			_input->setConnection(this);
		}
	}

	updatePath();
}

FlowInput* FlowConnection::input() const
{
	return _input;
}

void FlowConnection::updatePath()
{
	if (!_output || !_input)
	{
		setPath({});
		return;
	}

	setStart(_output->hotSpotSceneCenter());
	setStop(_input->hotSpotSceneCenter());
}

} // namespace qtMate::flow
