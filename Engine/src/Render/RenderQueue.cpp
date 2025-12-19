#include "RenderQueue.h"

#include "RHI/RHIRenderer.h"

DrawCommand::DrawCommand(const DrawCommandData& _data)
{
}

bool DrawCommand::BeforeExecute(RenderCommand* prevCommand)
{
    return RenderCommand::BeforeExecute(prevCommand);
}

void DrawCommand::Execute(RenderCommand* prevCommand)
{
}

void DrawCommand::AfterExecute()
{
    RenderCommand::AfterExecute();
}

void RenderQueue::AddCommand(const DrawCommandData& data)
{
    
}

void RenderQueue::ExecuteCommands(RHIRenderer* renderer)
{
}
