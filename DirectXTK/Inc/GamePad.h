//--------------------------------------------------------------------------------------
// File: GamePad.h
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#ifndef _XBOX_ONE
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP)
#if (_WIN32_WINNT >= 0x0602)
#pragma comment(lib,"xinput.lib")
#else
#pragma comment(lib,"xinput9_1_0.lib")
#endif
#endif
#endif

// VS 2010/2012 do not support =default =delete
#ifndef DIRECTX_CTOR_DEFAULT
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define DIRECTX_CTOR_DEFAULT {}
#define DIRECTX_CTOR_DELETE ;
#else
#define DIRECTX_CTOR_DEFAULT =default;
#define DIRECTX_CTOR_DELETE =delete;
#endif
#endif

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4005)
#include <stdint.h>
#include <intsafe.h>
#pragma warning(pop)


namespace DirectX
{
    class GamePad
    {
    public:
        GamePad();
        GamePad(GamePad&& moveFrom);
        GamePad& operator= (GamePad&& moveFrom);
        virtual ~GamePad();

#ifdef _XBOX_ONE
        static const int MAX_PLAYER_COUNT = 8;
#else
        static const int MAX_PLAYER_COUNT = 4;
#endif

        enum DeadZone
        {
            DEAD_ZONE_INDEPENDENT_AXES = 0,
            DEAD_ZONE_CIRCULAR,
            DEAD_ZONE_NONE,
        };

        struct Buttons
        {
            bool a;
            bool b;
            bool x;
            bool y;
            bool leftStick;
            bool rightStick;
            bool leftShoulder;
            bool rightShoulder;
            bool back;
            bool start;
        };

        struct DPad
        {
            bool up;
            bool down;
            bool right;
            bool left;
        };

        struct ThumbSticks
        {
            float leftX;
            float leftY;
            float rightX;
            float rightY;
        };

        struct Triggers
        {
            float left;
            float right;
        };
        
        struct State
        {
            bool        connected;
            uint64_t    packet;
            Buttons     buttons;
            DPad        dpad;
            ThumbSticks thumbSticks;
            Triggers    triggers;

            bool __cdecl IsConnected() const { return connected; }

            // Is the button pressed currently?
            bool __cdecl IsAPressed() const { return buttons.a; }
            bool __cdecl IsBPressed() const { return buttons.b; }
            bool __cdecl IsXPressed() const { return buttons.x; }
            bool __cdecl IsYPressed() const { return buttons.y; }

            bool __cdecl IsLeftStickPressed() const { return buttons.leftStick; }
            bool __cdecl IsRightStickPressed() const { return buttons.rightStick; }

            bool __cdecl IsLeftShoulderPressed() const { return buttons.leftShoulder; }
            bool __cdecl IsRightShoulderPressed() const { return buttons.rightShoulder; }

            bool __cdecl IsBackPressed() const { return buttons.back; }
            bool __cdecl IsViewPressed() const { return buttons.back; }
            bool __cdecl IsStartPressed() const { return buttons.start; }
            bool __cdecl IsMenuPressed() const { return buttons.start; }

            bool __cdecl IsDPadDownPressed() const { return dpad.down; };
            bool __cdecl IsDPadUpPressed() const { return dpad.up; };
            bool __cdecl IsDPadLeftPressed() const { return dpad.left; };
            bool __cdecl IsDPadRightPressed() const { return dpad.right; };

            bool __cdecl IsLeftThumbStickUp() const { return (thumbSticks.leftY > 0.5f) != 0; }
            bool __cdecl IsLeftThumbStickDown() const { return (thumbSticks.leftY < -0.5f) != 0; }
            bool __cdecl IsLeftThumbStickLeft() const { return (thumbSticks.leftX < -0.5f) != 0; }
            bool __cdecl IsLeftThumbStickRight() const { return (thumbSticks.leftX > 0.5f) != 0; }

            bool __cdecl IsRightThumbStickUp() const { return (thumbSticks.rightY > 0.5f ) != 0; }
            bool __cdecl IsRightThumbStickDown() const { return (thumbSticks.rightY < -0.5f) != 0; }
            bool __cdecl IsRightThumbStickLeft() const { return (thumbSticks.rightX < -0.5f) != 0; }
            bool __cdecl IsRightThumbStickRight() const { return (thumbSticks.rightX > 0.5f) != 0; }

            bool __cdecl IsLeftTriggerPressed() const { return (triggers.left > 0.5f) != 0; }
            bool __cdecl IsRightTriggerPressed() const { return (triggers.right > 0.5f) != 0; }
        };

        struct Capabilities
        {
            enum Type
            {
                UNKNOWN = 0,
                GAMEPAD,
                WHEEL,
                ARCADE_STICK,
                FLIGHT_STICK,
                DANCE_PAD,
                GUITAR,
                GUITAR_ALTERNATE,
                DRUM_KIT,
                GUITAR_BASS = 11,
                ARCADE_PAD = 19,
            };

            bool        connected;
            Type        gamepadType;
            uint64_t    id;

            bool __cdecl IsConnected() const { return connected; }
        };

        class ButtonStateTracker
        {
        public:
            enum ButtonState
            {
                UP = 0,         // Button is up
                HELD = 1,       // Button is held down
                RELEASED = 2,   // Button was just released
                PRESSED = 3,    // Buton was just pressed
            };

            ButtonState a;
            ButtonState b;
            ButtonState x;
            ButtonState y;

            ButtonState leftStick;
            ButtonState rightStick;

            ButtonState leftShoulder;
            ButtonState rightShoulder;

            ButtonState back;
            ButtonState start;

            ButtonState dpadUp;
            ButtonState dpadDown;
            ButtonState dpadLeft;
            ButtonState dpadRight;

            ButtonStateTracker() { Reset(); }

            void __cdecl Update( const State& state );

            void __cdecl Reset();

        private:
            State lastState;
        };

        // Retrieve the current state of the gamepad of the associated player index
        State __cdecl GetState(int player, DeadZone deadZoneMode = DEAD_ZONE_INDEPENDENT_AXES);

        // Retrieve the current capabilities of the gamepad of the associated player index
        Capabilities __cdecl GetCapabilities(int player);

        // Set the vibration motor speeds of the gamepad
        bool __cdecl SetVibration( int player, float leftMotor, float rightMotor, float leftTrigger = 0.f, float rightTrigger = 0.f );

        // Handle suspending/resuming
        void __cdecl Suspend();
        void __cdecl Resume();

    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;

        // Prevent copying.
        GamePad(GamePad const&) DIRECTX_CTOR_DELETE
        GamePad& operator=(GamePad const&) DIRECTX_CTOR_DELETE
    };
}
