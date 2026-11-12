/******************************************************************************/
// Bullfrog Engine Emulation Library - AmigaOS backend
/******************************************************************************/
/** @file ssemphr.cpp
 *     Semaphores. The game and the input handling run in one task on AmigaOS
 *     (IDCMP is polled from the game loop), so locking is not needed.
 * @par Comment:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfsemphr.hpp"

LbSemaphore::LbSemaphore(void)
{
}

LbSemaphore::~LbSemaphore(void)
{
}

LbSemaLock::LbSemaLock(class LbSemaphore *sem, bool isLocked)
{
    (void)sem;
    this->locked = isLocked;
    this->invalid = 0;
}

LbSemaLock::~LbSemaLock(void)
{
    Release();
}

void LbSemaLock::Release(void)
{
    this->locked = 0;
}

int LbSemaLock::Lock(bool wait_forever)
{
    (void)wait_forever;
    this->locked = 1;
    return 1;
}
/******************************************************************************/
