#include "input.h"

#include <Limelight.h>
#include <SDL.h>
#include "streaming/streamutils.h"

void SdlInputHandler::handleMouseButtonEvent(SDL_MouseButtonEvent* event)
{
    int button;

    if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }
    else if (!isCaptureActive()) {
        if (event->button == SDL_BUTTON_LEFT && event->state == SDL_RELEASED) {
            // Capture the mouse again if clicked when unbound.
            // We start capture on left button released instead of
            // pressed to avoid sending an errant mouse button released
            // event to the host when clicking into our window (since
            // the pressed event was consumed by this code).
            setCaptureActive(true);
        }

        // Not capturing
        return;
    }

    switch (event->button)
    {
        case SDL_BUTTON_LEFT:
            button = BUTTON_LEFT;
            break;
        case SDL_BUTTON_MIDDLE:
            button = BUTTON_MIDDLE;
            break;
        case SDL_BUTTON_RIGHT:
            button = BUTTON_RIGHT;
            break;
        case SDL_BUTTON_X1:
            button = BUTTON_X1;
            break;
        case SDL_BUTTON_X2:
            button = BUTTON_X2;
            break;
        default:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Unhandled button event: %d",
                        event->button);
            return;
    }

    LiSendMouseButtonEvent(event->state == SDL_PRESSED ?
                               BUTTON_ACTION_PRESS :
                               BUTTON_ACTION_RELEASE,
                           button);
}

void SdlInputHandler::handleMouseMotionEvent(SDL_MouseMotionEvent* event)
{
    if (!isCaptureActive()) {
        // Not capturing
        return;
    }
    else if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }

    if (m_AbsoluteMouseMode) {
        SDL_Rect src, dst;

        src.x = src.y = 0;
        src.w = m_StreamWidth;
        src.h = m_StreamHeight;

        dst.x = dst.y = 0;
        SDL_GetWindowSize(m_Window, &dst.w, &dst.h);

        // Use the stream and window sizes to determine the video region
        StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

        // Clamp motion to the video region
        short x = qMin(qMax(event->x - dst.x, 0), dst.w);
        short y = qMin(qMax(event->y - dst.y, 0), dst.h);

        // Send the mouse position update
        LiSendMousePositionEvent(x, y, dst.w, dst.h);
    }
    else {
        // Batch until the next mouse polling window or we'll get awful
        // input lag everything except GFE 3.14 and 3.15.
        SDL_AtomicAdd(&m_MouseDeltaX, event->xrel);
        SDL_AtomicAdd(&m_MouseDeltaY, event->yrel);
    }
}

void SdlInputHandler::handleMouseWheelEvent(SDL_MouseWheelEvent* event)
{
    if (!isCaptureActive()) {
        // Not capturing
        return;
    }
    else if (event->which == SDL_TOUCH_MOUSEID) {
        // Ignore synthetic mouse events
        return;
    }

    if (event->y != 0) {
        LiSendScrollEvent((signed char)event->y);
    }
}

void SdlInputHandler::sendSyntheticMouseState(Uint32 type, Uint32 button) {
    int mouseX, mouseY;
    int windowX, windowY;
    SDL_Event event;
    Uint32 buttonState = SDL_GetGlobalMouseState(&mouseX, &mouseY);
    SDL_GetWindowPosition(m_Window, &windowX, &windowY);

    switch (type) {
    case SDL_MOUSEMOTION:
        event.motion.type = type;
        event.motion.timestamp = SDL_GetTicks();
        event.motion.windowID = SDL_GetWindowID(m_Window);
        event.motion.which = 0;
        event.motion.state = buttonState;
        event.motion.x = mouseX - windowX;
        event.motion.y = mouseY - windowY;
        event.motion.xrel = 0;
        event.motion.yrel = 0;
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        event.button.type = type;
        event.button.timestamp = SDL_GetTicks();
        event.button.windowID = SDL_GetWindowID(m_Window);
        event.button.which = 0;
        event.button.button = button;
        event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        event.button.clicks = 1;
        event.button.x = mouseX - windowX;
        event.button.y = mouseY - windowY;
        break;

    default:
        SDL_assert(false);
    }

    SDL_PushEvent(&event);
}

Uint32 SdlInputHandler::mouseMoveTimerCallback(Uint32 interval, void *param)
{
    auto me = reinterpret_cast<SdlInputHandler*>(param);

    short deltaX = (short)SDL_AtomicSet(&me->m_MouseDeltaX, 0);
    short deltaY = (short)SDL_AtomicSet(&me->m_MouseDeltaY, 0);

    if (deltaX != 0 || deltaY != 0) {
        LiSendMouseMoveEvent(deltaX, deltaY);
    }

    if (me->m_PendingFocusGain && me->m_AbsoluteMouseMode) {
        Uint32 buttonState = SDL_GetGlobalMouseState(NULL, NULL);

        // Update the position first
        me->sendSyntheticMouseState(SDL_MOUSEMOTION, 0);

        // If the button has come up since last time, send that too
        if ((buttonState & SDL_BUTTON(me->m_PendingFocusButtonUp)) == 0) {
            me->sendSyntheticMouseState(SDL_MOUSEBUTTONUP, me->m_PendingFocusButtonUp);

            // Focus gain has completed
            me->m_PendingFocusGain = false;
        }
    }

    return interval;
}
