#include "QtMate/flow/flowSceneDelegate.hpp"
#include "QtMate/flow/flowSocket.hpp"
#include "QtMate/flow/flowInput.hpp"
#include "QtMate/flow/flowOutput.hpp"

namespace qtMate::flow
{
QColor FlowSceneDelegate::socketTypeColor(FlowSocketType type) const
{
	return Qt::darkGray;
}

bool FlowSceneDelegate::canConnect(FlowOutput* output, FlowInput* input) const
{
	return output->descriptor().type == input->descriptor().type;
}

} // namespace qtMate::flow
