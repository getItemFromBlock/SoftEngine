#pragma once

namespace Memory
{
    // Align an integer to the next nearest memory aligned value. Alignement MUST be a power of two!
    size_t align(size_t value, size_t alignement);
};
