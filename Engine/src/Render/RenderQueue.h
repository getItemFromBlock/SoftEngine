#pragma once
#include <iostream>
#include <galaxymath/Maths.h>

#include "Core/UUID.h"

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
    SortKey sortKey;
    int vertexArrayID;
    class Material* material = nullptr;
    Mat4 modelMatrix;
    Mat4 MVP;
    bool hasLSM = false;
    Mat4 LSM;
    Vec3f ViewPos;
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
    
private:
    
};