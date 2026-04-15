#include <definitions.h>
#include <desktop.h>
#include <filesystem.h>
#include <processes.h>
#include <scheduler.h>
#include <hpet.h>
#include <tty.h>
#include <str.h>
#include <bmp.h>
#include <psf.h>
#include <cpu.h>

#define TERMINAL_DEFAULT 0x989898
#define TERMINAL_WHITE 0xFFFFFF
#define TERMINAL_BLUE 0x0080FF
#define TERMINAL_GREEN 0x008000
#define TERMINAL_RED 0xFF5050
#define TERMINAL_NO_CHAR '\xff'
#define TERMINAL_COLOUR '\xfe'
#define KEY_LEFT_SHIFT 42
#define KEY_RIGHT_SHIFT 54
#define KEY_CAPS_LOCK 58
#define KEY_BACKSPACE 14
#define KEY_ENTER 28

Window* window = 0;
PsfFile* font = 0;
uint32_t fontWidth = 0;
uint32_t fontHeight = 0;
uint32_t terminalWidth = 0;
uint32_t terminalHeight = 0;
uint32_t terminalPitch = 0;
uint32_t cursorX = 0;
uint32_t cursorY = 0;
const uint32_t colours[256] = { 0, TERMINAL_DEFAULT, TERMINAL_WHITE, TERMINAL_BLUE, TERMINAL_GREEN, TERMINAL_RED };
uint8_t colour = 1;
const char scancodes[256] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
    '9', '0', '-', '=', 0, /* Backspace */
    0, /* Tab */
    'q', 'w', 'e', 'r', /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, /* Enter key */
    0, /* 29 - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
    '\'', '`', 0, /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', /* 49 */
    'm', ',', '.', '/', 0, /* Right shift */
    '*', 0, /* Alt */
    ' ', /* Space bar */
    0, /* Caps lock */
    0, /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0, 0, /* < ... F10 */
    0, /* 69 - Num lock */
    0, /* Scroll Lock */
    0, /* Home key */
    0, /* Up Arrow */
    0, /* Page Up */
    '-', 0, /* Left Arrow */
    0, 0, /* Right Arrow */
    '+', 0, /* 79 - End key */
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert Key */
    0, /* Delete Key */
    0, 0, 0, 0, /* F11 Key */
    0, /* F12 Key */
    0 /* All other keys are undefined */
};
const char capsScancodes[256] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', /* 9 */
    '(', ')', '_', '+', 0, /* Backspace */
    0, /* Tab */
    'Q', 'W', 'E', 'R', /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, /* Enter key */
    0, /* 29 - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
    '"', '~', 0, /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', /* 49 */
    'M', '<', '>', '?', 0, /* Right shift */
    '*', 0, /* Alt */
    ' ', /* Space bar */
    0, /* Caps lock */
    0, /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0, 0, /* < ... F10 */
    0, /* 69 - Num lock */
    0, /* Scroll Lock */
    0, /* Home key */
    0, /* Up Arrow */
    0, /* Page Up */
    '-', 0, /* Left Arrow */
    0, 0, /* Right Arrow */
    '+', 0, /* 79 - End key */
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert Key */
    0, /* Delete Key */
    0, 0, 0, 0, /* F11 Key */
    0, /* F12 Key */
    0 /* All other keys are undefined */
};
bool leftShift = false;
bool rightShift = false;
bool shift = false;
bool caps = false;
TtyBuffer ttyBuffer = { 0 };
bool waitingColour = false;
uint64_t shellId = 0;
bool exiting = false;

void drawCharacter(char character, uint32_t x, uint32_t y, uint32_t colour)
{
    uint32_t* address = window->buffer + y * window->bufferWidth + x;
    uint8_t* glyph = font->data + font->glyphSize * character;
    for (uint32_t y = 0; y < font->height; y++)
    {
        uint32_t width = font->width;
        while (width)
        {
            uint32_t amount = min(8, width);
            for (uint8_t x = 0; x < amount; x++)
            {
                if (*glyph & (0b10000000 >> x))
                {
                    *address = colour;
                }
                address++;
            }
            glyph++;
            width -= amount;
        }
        address += window->bufferWidth - font->width;
    }
}

void drop()
{
    copyMemory32(window->buffer + window->bufferWidth * font->height, window->buffer, window->bufferWidth * (window->bufferHeight - font->height));
    setMemory32(window->buffer + window->bufferWidth * (window->bufferHeight - font->height), 0, window->bufferWidth * font->height);
}

void terminalPut(char character)
{
    drawCharacter('_', cursorX * fontWidth, cursorY * fontHeight, 0);
    if (waitingColour)
    {
        waitingColour = false;
        colour = character;
    }
    else if (character == '\b')
    {
        if (cursorX > 0)
        {
            cursorX--;
        }
        else
        {
            cursorY--;
            cursorX = terminalWidth - 1;
        }
        uint32_t x = cursorX * fontWidth;
        uint32_t y = cursorY * fontHeight;
        uint32_t* address = window->buffer + y * window->bufferWidth + x;
        for (uint32_t y = 0; y < fontHeight; y++)
        {
            for (uint32_t x = 0; x < fontWidth; x++)
            {
                *address++ = 0;
            }
            address += window->bufferWidth - fontWidth;
        }
    }
    else if (character == '\n')
    {
        cursorX = 0;
        if (cursorY + 1 != terminalHeight)
        {
            cursorY++;
        }
        else
        {
            drop();
        }
    }
    else if (character == TTY_CLEAR)
    {
        setMemory32(window->buffer, 0, window->bufferWidth * window->bufferHeight);
        cursorX = 0;
        cursorY = 0;
    }
    else if (character == TERMINAL_COLOUR)
    {
        waitingColour = true;
    }
    else
    {
        drawCharacter(character, cursorX * fontWidth, cursorY * fontHeight, colours[colour]);
        cursorX++;
        if (cursorX == terminalWidth)
        {
            cursorX = 0;
            if (cursorY + 1 != terminalHeight)
            {
                cursorY++;
            }
            else
            {
                drop();
            }
        }
    }
    drawCharacter('_', cursorX * fontWidth, cursorY * fontHeight, colours[colour]);
}

void exitTerminal()
{
    copyString("exit", ttyBuffer.readBuffer);
    ttyBuffer.readCursor = 4;
    ttyBuffer.readBuffer = 0;
    waitForThread(shellId);
    unregisterTty(&ttyBuffer);
    destroyWindow(window);
    quit();
}

void entry()
{
    if (!desktopRunning())
    {
        write("Desktop is not running\n");
        quit();
    }
    window = createWindow(800, 480);
    copyString("Terminal", window->title);
    readBmp((BmpHeader*)getFile("/programs/terminal/desktop.bmp", 0), window->icon);
    setMemory32(window->buffer, 0, window->bufferWidth * window->bufferHeight);
    registerTty(&ttyBuffer);
    font = (PsfFile*)getFile("/naul/font16.psf", 0);
    fontWidth = font->width + 1;
    fontHeight = font->height;
    terminalWidth = window->bufferWidth / fontWidth;
    terminalHeight = window->bufferHeight / fontHeight;
    terminalPitch = terminalWidth * 2;
    bool blink = false;
    uint64_t last = getFemtoseconds();
    shellId = execute("/programs/shell/shell.nxe");
    while (true)
    {
        while (ttyBuffer.writeTail != ttyBuffer.writeHead)
        {
            terminalPut(ttyBuffer.writeBuffer[ttyBuffer.writeTail]);
            ttyBuffer.writeTail++;
        }
        while (window->eventsTail != window->eventsHead)
        {
            switch (window->events[window->eventsTail].type)
            {
                case CLOSE_EVENT:
                    exiting = true;
                    break;
                case KEY_EVENT:
                    switch (window->events[window->eventsTail].keyEvent.scancode)
                    {
                        case KEY_LEFT_SHIFT:
                            leftShift = window->events[window->eventsTail].keyEvent.pressed;
                            shift = leftShift || rightShift;
                            break;
                        case KEY_RIGHT_SHIFT:
                            rightShift = window->events[window->eventsTail].keyEvent.pressed;
                            shift = leftShift || rightShift;
                            break;
                        case KEY_CAPS_LOCK:
                            if (window->events[window->eventsTail].keyEvent.pressed)
                            {
                                caps = !caps;
                            }
                            break;
                    }
                    if (ttyBuffer.readBuffer)
                    {
                        switch (window->events[window->eventsTail].keyEvent.scancode)
                        {
                            case KEY_BACKSPACE:
                                if (window->events[window->eventsTail].keyEvent.pressed && ttyBuffer.readCursor > 0)
                                {
                                    ttyBuffer.readCursor--;
                                    terminalPut('\b');
                                }
                                break;
                            case KEY_ENTER:
                                if (window->events[window->eventsTail].keyEvent.pressed)
                                {
                                    ttyBuffer.readBuffer[ttyBuffer.readCursor] = '\0';
                                    if (compareStrings(ttyBuffer.readBuffer, "exit") == 0)
                                    {
                                        exitTerminal();
                                    }
                                    terminalPut('\n');
                                    ttyBuffer.readBuffer = 0;
                                }
                                break;
                            default:
                                if (window->events[window->eventsTail].keyEvent.pressed && ttyBuffer.readCursor < ttyBuffer.readLength)
                                {
                                    char character = (caps ? !shift : shift) ? capsScancodes[window->events[window->eventsTail].keyEvent.scancode] : scancodes[window->events[window->eventsTail].keyEvent.scancode];
                                    if (character)
                                    {
                                        ttyBuffer.readBuffer[ttyBuffer.readCursor++] = character;
                                        terminalPut(character);
                                    }
                                }
                                break;
                        }
                    }
                    break;
            }
            window->eventsTail++;
        }
        uint64_t femtoseconds = getFemtoseconds();
        if (femtoseconds - last >= FEMTOSECONDS_PER_SECOND / 2)
        {
            last = femtoseconds;
            drawCharacter('_', cursorX * fontWidth, cursorY * fontHeight, blink ? colours[colour] : 0);
            blink = !blink;
        }
        if (exiting && ttyBuffer.readBuffer)
        {
            exitTerminal();
        }
        yieldThread();
    }
}
