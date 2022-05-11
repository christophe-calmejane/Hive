#include "QtMate/flow/flowscenedelegate.hpp"
#include "QtMate/flow/flowsocket.hpp"
#include "QtMate/flow/flowinput.hpp"
#include "QtMate/flow/flowoutput.hpp"

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
