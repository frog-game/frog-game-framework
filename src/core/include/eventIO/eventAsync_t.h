#pragma once

typedef struct eventAsync_s
{
    // private
    void (*fnWork)(struct eventAsync_s*);
    void (*fnCancel)(struct eventAsync_s*);
    void* node[2];
} eventAsync_tt;
