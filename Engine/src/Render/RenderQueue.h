#pragma once
#include <iostream>
#include <galaxymath/Maths.h>

#include "Core/UUID.h"

class RHIRenderer;
class Material;
class Mesh;

struct SortKey
{
    uint64_t materialKey = UUID_INVALID;
    uint64_t meshKey = UUID_INVALID;
    
    bool operator!=(const SortKey& sortKey) const
    {
        return materialKey != sortKey.materialKey || meshKey != sortKey.meshKey;
    }
};

class RenderCommand
{
public:
    virtual ~RenderCommand() = default;
    virtual bool BeforeExecute(RenderCommand* prevCommand) { return false; }
    virtual void AfterExecute() {}
    virtual void Execute(RenderCommand* prevCommand) = 0;

public:
    SortKey sortKey;
};

struct DrawCommandData
{
    uint64_t sortKey;
    
    Mesh* mesh;
    uint32_t subMeshIndex;
    Material* material;
    Mat4 transform;
    
    bool operator<(const DrawCommandData& other) const {
        return sortKey < other.sortKey;
    }
};

class DrawCommand : public RenderCommand
{
public:
    DrawCommand(const DrawCommandData& _data);

    bool BeforeExecute(RenderCommand* prevCommand) override;
    void Execute(RenderCommand* prevCommand) override;
    void AfterExecute() override;
};

class RenderQueue
{
public:
    void AddCommand(const DrawCommandData& data);
    
    void ExecuteCommands(RHIRenderer* renderer);
private:
    std::vector<DrawCommandData> m_drawCommands;
    
};