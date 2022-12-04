#pragma once

struct Arena;

struct Context
{
    // The allocator to use for memory that needs to stay allocated for longer periods of time.
    // By default, if not manually freed, memory taken from bump stays allocated until the program
    // shuts down.
    Arena* bump = nullptr;

    // The allocator to use for memory with short lifetimes. We make no guarantees that allocations
    // taken from it stay valid for longer than a frame, and may verify that everything has been freed
    // at the end of the frame. Generally, allocations from tmp_bump should be freed as soon as they are not needed anymore.
    Arena* tmp_bump = nullptr;
};