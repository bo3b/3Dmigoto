#include "DirectInput.h"

using namespace std;

wchar_t InputDevice[MAX_PATH], InputAction[MAX_PATH], ToggleAction[MAX_PATH];
int InputDeviceId = 0;
DWORD ActionButton = 0xffffffff;
bool Action = false;
tState Toggle = onUp;
int XInputDeviceId = -1;

HRESULT InitDirectInput();
VOID FreeDirectInput();
void UpdateInputState();
static DWORD DeviceType;
static DIJOYSTATE2 JoystickState;
static DIMOUSESTATE2 MouseState;
static char KeyboardState[256];

BOOL CALLBACK    EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext);
BOOL CALLBACK    EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext);

// Stuff to filter out XInput devices
#include <wbemidl.h>
HRESULT SetupForIsXInputDevice();
bool IsXInputDevice(const GUID* pGuidProductFromDirectInput);
void CleanupForIsXInputDevice();

struct XINPUT_DEVICE_NODE
{
	DWORD dwVidPid;
	XINPUT_DEVICE_NODE* pNext;
};

struct DI_ENUM_CONTEXT
{
	DIJOYCONFIG* pPreferredJoyCfg;
	bool bPreferredJoyCfgValid;
};

static XINPUT_DEVICE_NODE*     g_pXInputDeviceList = NULL;
static LPDIRECTINPUT8          g_pDI = 0;
static LPDIRECTINPUTDEVICE8    g_pJoystick = 0;

//-----------------------------------------------------------------------------
// Name: InitDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
HRESULT InitDirectInput()
{
	HRESULT hr;

	// If we've already done init, we need to skip out. This is expected to be called often.
	if (g_pJoystick != NULL)
		return S_OK;

	// Log XInput devices.
	if (LogInput)
	{
		if (!LogFile) fopen_s(&LogFile, "nvapi_log.txt", "w");
		for (int i = 0; i < 4; ++i)
		{
			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));

			// Simply get the state of the controller from XInput.
			DWORD dwResult = XInputGetState(i, &state);
			if (dwResult == ERROR_SUCCESS && LogInput)
			{
				fprintf(LogFile, "Detected XInput device #%d\n", i);
			}
		}
	}

	// Register with the DirectInput subsystem and get a pointer
	// to a IDirectInput interface we can use.
	// Create a DInput object
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
		IID_IDirectInput8, (VOID **)&g_pDI, NULL)))
		return hr;

	SetupForIsXInputDevice();

	DIJOYCONFIG PreferredJoyCfg = { 0 };
	DI_ENUM_CONTEXT enumContext;
	enumContext.pPreferredJoyCfg = &PreferredJoyCfg;
	enumContext.bPreferredJoyCfgValid = false;

	IDirectInputJoyConfig8* pJoyConfig = NULL;
	if (FAILED(hr = g_pDI->QueryInterface(IID_IDirectInputJoyConfig8, (void**)&pJoyConfig)))
		return hr;

	PreferredJoyCfg.dwSize = sizeof(PreferredJoyCfg);
	if (SUCCEEDED(pJoyConfig->GetConfig(0, &PreferredJoyCfg, DIJC_GUIDINSTANCE))) // This function is expected to fail if no joystick is attached
		enumContext.bPreferredJoyCfgValid = true;
	pJoyConfig->Release(); pJoyConfig = 0;

	// Look for all input devices.
	if (FAILED(hr = g_pDI->EnumDevices(DI8DEVCLASS_ALL, // DI8DEVCLASS_GAMECTRL,
		EnumJoysticksCallback,
		&enumContext, DIEDFL_ATTACHEDONLY)))
		return hr;

	CleanupForIsXInputDevice();

	// Make sure we got a joystick
	if (NULL == g_pJoystick)
	{
		if (LogInput) fprintf(LogFile, "No DirectInput device used.\n");
		return S_OK;
	}

	// Set the cooperative level to let DInput know how this device should
	// interact with the system and with other DInput applications.
	if (FAILED(hr = g_pJoystick->SetCooperativeLevel(0, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE /* DISCL_EXCLUSIVE |
													   DISCL_FOREGROUND */)))
													   return hr;

	// A data format specifies which controls on a device we are interested in,
	// and how they should be reported. This tells DInput that we will be
	// passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
	if (DeviceType == DI8DEVTYPE_MOUSE)
	{
		if (LogInput) fprintf(LogFile, "Using mouse data format.\n");
		if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIMouse2)))
			return hr;
	}
	else if (DeviceType == DI8DEVTYPE_KEYBOARD)
	{
		if (LogInput) fprintf(LogFile, "Using keyboard data format.\n");
		if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIKeyboard)))
			return hr;
	}
	else
	{
		if (LogInput) fprintf(LogFile, "Using joystick data format for device type %x.\n", DeviceType);
		if (FAILED(hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2)))
			return hr;
	}

	// Seemed to be missing early Aquire, and relying upon retries in UpdateInput.
	// But might interfere with other uses/be blocked from input.
	if (FAILED(hr = g_pJoystick->Acquire()))
		return hr;

	// Enumerate the joystick objects. The callback function enabled user
	// interface elements for objects that are found, and sets the min/max
	// values property for discovered axes.
	if (FAILED(hr = g_pJoystick->EnumObjects(EnumObjectsCallback, 0, DIDFT_BUTTON)))
		return hr;

	if (ActionButton == 0xffffffff)
	{
		if (LogInput) fprintf(LogFile, "DirectInput action not found. Checking for ButtonXX syntax.\n");
		ActionButton = 0;
		int ret = swscanf_s(InputAction, L"Button%d", &ActionButton);
		if (ret > 0)
		{
			if (LogInput) fprintf(LogFile, "Using input button #%d for action at offset 0x%x.\n",
				ActionButton, ActionButton + offsetof(DIJOYSTATE2, rgbButtons));
			ActionButton += offsetof(DIJOYSTATE2, rgbButtons);
		}
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains 
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it’s an XInput device
// Unfortunately this information can not be found by just using DirectInput.
// Checking against a VID/PID of 0x028E/0x045E won't find 3rd party or future 
// XInput devices.
//
// This function stores the list of xinput devices in a linked list 
// at g_pXInputDeviceList, and IsXInputDevice() searchs that linked list
//-----------------------------------------------------------------------------
HRESULT SetupForIsXInputDevice()
{
	IWbemServices* pIWbemServices = NULL;
	IEnumWbemClassObject* pEnumDevices = NULL;
	IWbemLocator* pIWbemLocator = NULL;
	IWbemClassObject* pDevices[21] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	BSTR bstrDeviceID = NULL;
	BSTR bstrClassName = NULL;
	BSTR bstrNamespace = NULL;
	DWORD uReturned = 0;
	bool bCleanupCOM = false;
	UINT iDevice = 0;
	VARIANT var;
	HRESULT hr;

	// CoInit if needed
	hr = CoInitialize(NULL);
	bCleanupCOM = SUCCEEDED(hr);

	// Create WMI
	hr = CoCreateInstance(__uuidof(WbemLocator),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(IWbemLocator),
		(LPVOID*)&pIWbemLocator);
	if (FAILED(hr) || pIWbemLocator == NULL)
		goto LCleanup;

	// Create BSTRs for WMI
	bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2"); if (bstrNamespace == NULL) goto LCleanup;
	bstrDeviceID = SysAllocString(L"DeviceID");           if (bstrDeviceID == NULL)  goto LCleanup;
	bstrClassName = SysAllocString(L"Win32_PNPEntity");    if (bstrClassName == NULL) goto LCleanup;

	// Connect to WMI 
	hr = pIWbemLocator->ConnectServer(bstrNamespace, NULL, NULL, 0L,
		0L, NULL, NULL, &pIWbemServices);
	if (FAILED(hr) || pIWbemServices == NULL)
		goto LCleanup;

	// Switch security level to IMPERSONATE
	CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
		RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0);

	// Get list of Win32_PNPEntity devices
	hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, NULL, &pEnumDevices);
	if (FAILED(hr) || pEnumDevices == NULL)
		goto LCleanup;

	// Loop over all devices
	for (;;)
	{
		// Get 20 at a time
		hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
		if (FAILED(hr))
			goto LCleanup;
		if (uReturned == 0)
			break;

		for (iDevice = 0; iDevice < uReturned; iDevice++)
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, NULL, NULL);
			if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL)
			{
				// Check if the device ID contains "IG_".  If it does, then it's an XInput device
				// Unfortunately this information can not be found by just using DirectInput 
				if (wcsstr(var.bstrVal, L"IG_"))
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR* strVid = wcsstr(var.bstrVal, L"VID_");
					if (strVid && swscanf_s(strVid, L"VID_%4X", &dwVid) != 1)
						dwVid = 0;
					WCHAR* strPid = wcsstr(var.bstrVal, L"PID_");
					if (strPid && swscanf_s(strPid, L"PID_%4X", &dwPid) != 1)
						dwPid = 0;

					DWORD dwVidPid = MAKELONG(dwVid, dwPid);

					// Add the VID/PID to a linked list
					XINPUT_DEVICE_NODE* pNewNode = new XINPUT_DEVICE_NODE;
					if (pNewNode)
					{
						pNewNode->dwVidPid = dwVidPid;
						pNewNode->pNext = g_pXInputDeviceList;
						g_pXInputDeviceList = pNewNode;
					}
				}
			}
			pDevices[iDevice]->Release(); pDevices[iDevice] = 0;
		}
	}

LCleanup:
	if (bstrNamespace)
		SysFreeString(bstrNamespace);
	if (bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if (bstrClassName)
		SysFreeString(bstrClassName);
	for (iDevice = 0; iDevice < 20; iDevice++)
		if (pDevices[iDevice]) pDevices[iDevice]->Release(); pDevices[iDevice] = 0;
	if (pEnumDevices) pEnumDevices->Release(); pEnumDevices = 0;
	if (pIWbemLocator) pIWbemLocator->Release(); pIWbemLocator = 0;
	if (pIWbemServices) pIWbemServices->Release(); pIWbemServices = 0;

	return hr;
}


//-----------------------------------------------------------------------------
// Returns true if the DirectInput device is also an XInput device.
// Call SetupForIsXInputDevice() before, and CleanupForIsXInputDevice() after
//-----------------------------------------------------------------------------
bool IsXInputDevice(const GUID* pGuidProductFromDirectInput)
{
	// Check each xinput device to see if this device's vid/pid matches
	XINPUT_DEVICE_NODE* pNode = g_pXInputDeviceList;
	while (pNode)
	{
		if (pNode->dwVidPid == pGuidProductFromDirectInput->Data1)
			return true;
		pNode = pNode->pNext;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Cleanup needed for IsXInputDevice()
//-----------------------------------------------------------------------------
void CleanupForIsXInputDevice()
{
	// Cleanup linked list
	XINPUT_DEVICE_NODE* pNode = g_pXInputDeviceList;
	while (pNode)
	{
		XINPUT_DEVICE_NODE* pDelete = pNode;
		pNode = pNode->pNext;
		delete pDelete; pDelete = 0;
	}
}

//-----------------------------------------------------------------------------
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance,
	VOID* pContext)
{
	DI_ENUM_CONTEXT* pEnumContext = (DI_ENUM_CONTEXT*)pContext;
	HRESULT hr;

	if (IsXInputDevice(&pdidInstance->guidProduct))
		return DIENUM_CONTINUE;

	/*
	// Skip anything other than the perferred joystick device as defined by the control panel.
	// Instead you could store all the enumerated joysticks and let the user pick.
	if( pEnumContext->bPreferredJoyCfgValid &&
	!IsEqualGUID( pdidInstance->guidInstance, pEnumContext->pPreferredJoyCfg->guidInstance ) )
	return DIENUM_CONTINUE;
	*/

	// Obtain an interface to the enumerated joystick.
	LPDIRECTINPUTDEVICE8 pDevice = 0;
	hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &pDevice, NULL);

	// If it failed, then we can't use this joystick. (Maybe the user unplugged
	// it while we were in the middle of enumerating it.)
	if (FAILED(hr))
		return DIENUM_CONTINUE;

	DIDEVICEINSTANCE deviceInfo;
	deviceInfo.dwSize = sizeof(DIDEVICEINSTANCE);
	hr = pDevice->GetDeviceInfo(&deviceInfo);
	if (hr != DI_OK)
		return DIENUM_CONTINUE;
	wchar_t winstance[MAX_PATH];
	char instance[MAX_PATH];
	wcscpy(winstance, deviceInfo.tszInstanceName);
	wchar_t *end = winstance + wcslen(winstance) - 1; while (end > winstance && isspace(*end)) end--; *(end + 1) = 0;
	wcstombs(instance, winstance, MAX_PATH);
	DIPROPDWORD prop;
	prop.diph.dwSize = sizeof(DIPROPDWORD);
	prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	prop.diph.dwHow = DIPH_DEVICE;
	prop.diph.dwObj = 0;
	prop.dwData = 0;
	pDevice->GetProperty(DIPROP_JOYSTICKID, &prop.diph);
	if (LogInput) fprintf(LogFile, "Detected DirectInput device: instance = %s, id = %d\n", instance, prop.dwData);

	// Did we found our device?
	if (wcscmp(winstance, InputDevice) == 0)
	{
		if (InputDeviceId < 0 || InputDeviceId == prop.dwData)
		{
			DeviceType = deviceInfo.dwDevType & 0xff;
			g_pJoystick = pDevice;
			if (LogInput) fprintf(LogFile, "Using DirectInput device: instance = %s, id = %d\n", instance, prop.dwData);
			return DIENUM_STOP;
		}
	}

	return DIENUM_CONTINUE;
}

//-----------------------------------------------------------------------------
// Name: EnumObjectsCallback()
// Desc: Callback function for enumerating objects (axes, buttons, POVs) on a 
//       joystick. This function enables user interface elements for objects
//       that are found to exist, and scales axes min/max values.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi,
	VOID* pContext)
{
	static int nSliderCount = 0;  // Number of returned slider controls
	static int nPOVCount = 0;     // Number of returned POV controls

	/*
	// For axes that are returned, set the DIPROP_RANGE property for the
	// enumerated axis in order to scale min/max values.
	if( pdidoi->dwType & DIDFT_AXIS )
	{
	DIPROPRANGE diprg;
	diprg.diph.dwSize = sizeof( DIPROPRANGE );
	diprg.diph.dwHeaderSize = sizeof( DIPROPHEADER );
	diprg.diph.dwHow = DIPH_BYID;
	diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
	diprg.lMin = -1000;
	diprg.lMax = +1000;

	// Set the range for the axis
	if (FAILED(g_pJoystick->SetProperty(DIPROP_RANGE, &diprg.diph)))
	return DIENUM_STOP;
	}
	*/

	char obj[MAX_PATH];
	wcstombs(obj, pdidoi->tszName, MAX_PATH);
	if (LogInput) fprintf(LogFile, "Found input item %s\n", obj);
	if (wcscmp(pdidoi->tszName, InputAction) == 0)
	{
		if (LogInput) fprintf(LogFile, "Using input item %s at data offset 0x0%x for action.\n", obj, pdidoi->dwOfs);
		ActionButton = pdidoi->dwOfs;
	}
	if (wcscmp(pdidoi->tszName, ToggleAction) == 0)
	{
		if (LogInput) fprintf(LogFile, "Using input item %s at data offset 0x0%x for toggle.\n", obj, pdidoi->dwOfs);
		ActionButton = pdidoi->dwOfs;
	}

	return DIENUM_CONTINUE;
}

//-----------------------------------------------------------------------------
// Name: UpdateInputState()
// Desc: Get the input device's state and display it.
//-----------------------------------------------------------------------------
void UpdateInputState()
{
	HRESULT hr;

	// This is late binding init, right when we want to start using it.  The reason to do this late
	// init is because there is some glitch with DirectInput that will prevent this from ever seeing
	// any changes in the keyboard or mouse status if we init 'too early.' It seems somehow locked out
	// by games, sometimes, not always.  This late-binding technique seems to always work.
	InitDirectInput();

	// Reading XInput device.
	if (XInputDeviceId >= 0)
	{
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		// Simply get the state of the controller from XInput.
		DWORD dwResult = XInputGetState(XInputDeviceId, &state);
		if (dwResult == ERROR_SUCCESS)
		{
			if (wcscmp(L"LeftTrigger", InputAction) == 0) Action = state.Gamepad.bLeftTrigger != 0;
			else if (wcscmp(L"RightTrigger", InputAction) == 0) Action = state.Gamepad.bRightTrigger != 0;
			else if (wcscmp(L"DPAD_UP", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
			else if (wcscmp(L"DPAD_DOWN", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
			else if (wcscmp(L"DPAD_LEFT", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
			else if (wcscmp(L"DPAD_RIGHT", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
			else if (wcscmp(L"START", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
			else if (wcscmp(L"BACK", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
			else if (wcscmp(L"LEFT_THUMB", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
			else if (wcscmp(L"RIGHT_THUMB", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
			else if (wcscmp(L"LEFT_SHOULDER", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
			else if (wcscmp(L"RIGHT_SHOULDER", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
			else if (wcscmp(L"A", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
			else if (wcscmp(L"B", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
			else if (wcscmp(L"X", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
			else if (wcscmp(L"Y", InputAction) == 0) Action = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
		}
		return;
	}

	if (NULL == g_pJoystick)
		return;

	// Poll the device to read the current state
	hr = g_pJoystick->Poll();
	if (FAILED(hr))
	{
		if (LogInput) fprintf(LogFile, "DirectInput poll failed hr=%x\n", hr);

		// DInput is telling us that the input stream has been
		// interrupted. We aren't tracking any state between polls, so
		// we don't have any special reset that needs to be done. We
		// just re-acquire and try again.
		hr = g_pJoystick->Acquire();
		while (hr == DIERR_INPUTLOST)
			hr = g_pJoystick->Acquire();

		// hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
		// may occur when the app is minimized or in the process of 
		// switching, so just try again later 
		return;
	}

	// Get the input's device state
	if (!FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &JoystickState)))
	{
		char *data = (char *)&JoystickState;
		Action = data[ActionButton] != 0;
	}
	else if (!FAILED(hr = g_pJoystick->GetDeviceState(sizeof(DIMOUSESTATE2), &MouseState)))
	{
		char *data = (char *)&MouseState;
		Action = data[ActionButton] != 0;
	}
	else if (!FAILED(hr = g_pJoystick->GetDeviceState(sizeof(KeyboardState), KeyboardState)))
	{
		Action = KeyboardState[ActionButton] != 0;

		// Must cycle through different states based solely on user input.
		switch (Toggle)	
		{
			case offUp:		if (Action) Toggle = onDown;
				break;
			case onDown:	if (!Action) Toggle = onUp;
				break;
			case onUp:		if (Action) Toggle = offDown;
				break;
			case offDown:	if (!Action) Toggle = offUp;
				break;
		}
	}
	else if (LogInput) fprintf(LogFile, "GetDeviceState failed hr=%x\n", hr);
}

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
VOID FreeDirectInput()
{
	// Unacquire the device one last time just in case 
	// the app tried to exit while the device is still acquired.
	if (g_pJoystick)
	{
		g_pJoystick->Unacquire();
		g_pJoystick->Release(); g_pJoystick = 0;
	}

	// Release any DirectInput objects.
	if (g_pDI) g_pDI->Release(); g_pDI = 0;
}
