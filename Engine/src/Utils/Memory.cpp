#include "Memory.h"

#include "Core/Engine.h"

size_t Memory::align(size_t value, size_t alignement)
{
    ASSERT(((alignement - 1) & alignement) == 0);
    return (value + alignement - 1) & ~(alignement - 1);
}
