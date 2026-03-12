#pragma once

#include <definitions.h>
#include <ipc.h>
#include <mem.h>

#define IPC_NAME "desktop"
#define CLOSE_EVENT 0
#define KEY_EVENT 1
#define MOUSE_MOVE_EVENT 2
#define MOUSE_CLICK_EVENT 3

enum DesktopCalls
{
    CREATE_WINDOW,
    DESTROY_WINDOW
};
typedef union
{
    uint8_t type;
    struct
    {
        uint8_t type;
    } closeEvent;
    struct
    {
        uint8_t type;
        uint8_t scancode;
        bool pressed;
    } keyEvent;
    struct
    {
        uint8_t type;
        int32_t x;
        int32_t y;
    } mouseMoveEvent;
    struct
    {
        uint8_t type;
        bool left;
        bool pressed;
    } mouseClickEvent;
} WindowEvent;
typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    bool minimised;
    bool lockMouse;
    uint32_t bufferWidth;
    uint32_t bufferHeight;
    uint32_t* buffer;
    uint8_t eventsHead;
    uint8_t eventsTail;
    WindowEvent events[256];
    char title[64];
    uint32_t icon[24 * 24];
} Window;

static bool desktopRunning()
{
    return checkListener(IPC_NAME);
}

static inline Window* createWindow(uint32_t width, uint32_t height)
{
    return (Window*)sendMessage(IPC_NAME, CREATE_WINDOW, width, height, 0);
}

static inline void destroyWindow(Window* window)
{
    sendMessage(IPC_NAME, DESTROY_WINDOW, (uint64_t)window, 0, 0);
}
