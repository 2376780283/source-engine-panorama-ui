//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef INPUTENUMS_H
#define INPUTENUMS_H
#ifdef _WIN32
#pragma once
#endif

// Standard maximum +/- value of a joystick axis
#define MAX_BUTTONSAMPLE			32768

#if !defined( _X360 )
#define INVALID_USER_ID		-1
#else
#define INVALID_USER_ID		XBX_INVALID_USER_ID
#endif

//-----------------------------------------------------------------------------
// Forward declarations: 
//-----------------------------------------------------------------------------

enum
{
	MAX_JOYSTICKS = 1,
	MOUSE_BUTTON_COUNT = 5,
	MAX_NOVINT_DEVICES = 2,
};

#if defined( LINUX )
// Linux has a slightly different mapping order on the joystick axes
enum JoystickAxis_t
{
	JOY_AXIS_X = 0,
 	JOY_AXIS_Y,
	JOY_AXIS_Z,
	JOY_AXIS_U,
	JOY_AXIS_R,
	JOY_AXIS_V,
	MAX_JOYSTICK_AXES,
};
#else
enum JoystickAxis_t
{
	JOY_AXIS_X = 0,
 	JOY_AXIS_Y,
	JOY_AXIS_Z,
	JOY_AXIS_R,
	JOY_AXIS_U,
	JOY_AXIS_V,
	MAX_JOYSTICK_AXES,
};
#endif

//-----------------------------------------------------------------------------
// Extra mouse codes
//-----------------------------------------------------------------------------
enum
{
	MS_WM_XBUTTONDOWN	= 0x020B,
	MS_WM_XBUTTONUP		= 0x020C,
	MS_WM_XBUTTONDBLCLK	= 0x020D,
	MS_MK_BUTTON4		= 0x0020,
	MS_MK_BUTTON5		= 0x0040,
};

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------
#include "tier0/platwindow.h"

enum InputEventType_t
{
	IE_ButtonPressed = 0,	// m_nData contains a ButtonCode_t
	IE_ButtonReleased,		// m_nData contains a ButtonCode_t
	IE_ButtonDoubleClicked,	// m_nData contains a ButtonCode_t
	IE_AnalogValueChanged,	// m_nData contains an AnalogCode_t, m_nData2 contains the value
	IE_FingerDown,
	IE_FingerUp,
	IE_FingerMotion,
	// SE port: CS:GO's key-repeat event (panorama's input handling distinguishes it from
	// IE_ButtonPressed).  Appended after the existing Source Engine values so their numeric values
	// are unchanged.
	IE_ButtonPressedRepeating,
	
	IE_FirstSystemEvent = 100,
	IE_Quit = IE_FirstSystemEvent,
	IE_ControllerInserted,	// m_nData contains the controller ID
	IE_ControllerUnplugged,	// m_nData contains the controller ID
	// SE port: CS:GO's remaining system events (panorama/source2/uitoplevelwindowsource2.cpp
	// switches on IE_Close / IE_WindowSizeChanged / IE_ActivateWindow).  Appended after the existing
	// Source Engine values so their numeric values are unchanged.
	IE_Close,
	IE_WindowSizeChanged,	// m_nData contains width, m_nData2 contains height, m_nData3 = 0 if not minimized, 1 if minimized
	IE_ActivateApp,			// Tells if any window went foreground. m_hWnd is PLAT_WINDOW_INVALID.    m_nData = 1 -> activated, 0 -> deactivated
	IE_ActivateWindow,		// Tells if a specific window showed up or went away. m_hWindow is valid. m_nData = 1 -> activated, 0 -> deactivated
	IE_CopyData,			// Data to be copied between applications

	// SE port: CS:GO's UI event range.  panorama/source2/panoramauiengine.cpp
	// (CPanoramaUIEngine::HandleInputEvent) switches on these, and the IME events are driven by
	// imesource2.cpp.  CS:GO also defines IE_Close / IE_WindowSizeChanged / IE_ActivateApp /
	// IE_ActivateWindow / IE_CopyData in the system block; add them when the engine integration (M4)
	// starts posting them.
	IE_FirstUIEvent = 200,
	IE_LocateMouseClick = IE_FirstUIEvent,
	IE_SetCursor,
	IE_KeyTyped,
	IE_KeyCodeTyped,
	IE_KeyCodeReleased,
	IE_InputLanguageChanged,
	IE_IMESetWindow,
	IE_IMEStartComposition,
	IE_IMEComposition,
	IE_IMEEndComposition,
	IE_IMEShowCandidates,
	IE_IMEChangeCandidates,
	IE_IMECloseCandidates,
	IE_IMERecomputeModes,
	IE_OverlayEvent,

	IE_FirstVguiEvent = 1000,	// Assign ranges for other systems that post user events here
	IE_FirstAppEvent = 2000,

	// If m_nType is IE_ButtonPressed, m_nData2 will contain one or more of these modifier flags
	// (SE port: CS:GO defines them in this enum; Source Engine 2013 only had the m_nData2 comment).
	IE_ShiftPressed		= 1,
	IE_ControlPressed	= 2,
	IE_AltPressed		= 4,
	IE_GuiPressed		= 8,	// Windows key, Mac Command key, etc.
};

struct InputEvent_t
{
	int m_nType;				// Type of the event (see InputEventType_t)
	int m_nTick;				// Tick on which the event occurred
	int m_nData;				// Generic 32-bit data, what it contains depends on the event
	int m_nData2;				// Generic 32-bit data, what it contains depends on the event
	int m_nData3;				// Generic 32-bit data, what it contains depends on the event

	// SE port: CS:GO carries the destination window in the event (panorama routes input per window).
	PlatWindow_t	m_hWnd;
};

//-----------------------------------------------------------------------------
// Steam Controller Enums
//-----------------------------------------------------------------------------

#define MAX_STEAM_CONTROLLERS 8

typedef enum
{
	SK_NULL,
	SK_BUTTON_A,
	SK_BUTTON_B,
	SK_BUTTON_X,
	SK_BUTTON_Y,
	SK_BUTTON_UP,
	SK_BUTTON_RIGHT,
	SK_BUTTON_DOWN,
	SK_BUTTON_LEFT,
	SK_BUTTON_LEFT_BUMPER,
	SK_BUTTON_RIGHT_BUMPER,
	SK_BUTTON_LEFT_TRIGGER,
	SK_BUTTON_RIGHT_TRIGGER,
	SK_BUTTON_LEFT_GRIP,
	SK_BUTTON_RIGHT_GRIP,
	SK_BUTTON_LPAD_TOUCH,
	SK_BUTTON_RPAD_TOUCH,
	SK_BUTTON_LPAD_CLICK,
	SK_BUTTON_RPAD_CLICK,
	SK_BUTTON_LPAD_UP,
	SK_BUTTON_LPAD_RIGHT,
	SK_BUTTON_LPAD_DOWN,
	SK_BUTTON_LPAD_LEFT,
	SK_BUTTON_RPAD_UP,
	SK_BUTTON_RPAD_RIGHT,
	SK_BUTTON_RPAD_DOWN,
	SK_BUTTON_RPAD_LEFT,
	SK_BUTTON_SELECT,
	SK_BUTTON_START,
	SK_BUTTON_STEAM,
	SK_BUTTON_INACTIVE_START,
	SK_VBUTTON_F1,						// These are "virtual" buttons. Useful if you want to have flow that maps an action to button code to be interpreted by some UI that accepts keystrokes, but you
	SK_VBUTTON_F2,						// don't want to map to real button (perhaps because it would be interpreted by UI in a way you don't like). 																																										
	SK_VBUTTON_F3,
	SK_VBUTTON_F4,
	SK_VBUTTON_F5,
	SK_VBUTTON_F6,
	SK_VBUTTON_F7,
	SK_VBUTTON_F8,
	SK_VBUTTON_F9,
	SK_VBUTTON_F10,
	SK_VBUTTON_F11,
	SK_VBUTTON_F12,
	SK_MAX_KEYS
} sKey_t;

enum ESteamPadAxis
{
	LEFTPAD_AXIS_X,
	LEFTPAD_AXIS_Y,
	RIGHTPAD_AXIS_X,
	RIGHTPAD_AXIS_Y,
	LEFT_TRIGGER_AXIS,
	RIGHT_TRIGGER_AXIS,
	GYRO_AXIS_PITCH,
	GYRO_AXIS_ROLL,
	GYRO_AXIS_YAW,
	MAX_STEAMPADAXIS = GYRO_AXIS_YAW
};

enum
{
	LASTINPUT_KBMOUSE = 0,
	LASTINPUT_CONTROLLER = 1,
	LASTINPUT_STEAMCONTROLLER = 2
};

enum GameActionSet_t
{
	GAME_ACTION_SET_NONE = -1,
	GAME_ACTION_SET_MENUCONTROLS = 0,
	GAME_ACTION_SET_FPSCONTROLS,
	GAME_ACTION_SET_IN_GAME_HUD,
	GAME_ACTION_SET_SPECTATOR,
};

enum GameActionSetFlags_t
{
	GAME_ACTION_SET_FLAGS_NONE = 0,
	GAME_ACTION_SET_FLAGS_TAUNTING = (1<<0),
};

enum JoystickType_t
{
	INPUT_TYPE_GENERIC_JOYSTICK = 0,
	INPUT_TYPE_X360,
	INPUT_TYPE_STEAMCONTROLLER,
};

#endif // INPUTENUMS_H
