#include <iostream>
#include <Windows.h>
#include <string>
#include <thread>
#include "../include/MinHook.h"


// The message the 2nd thread will be in charge of continously sending
#define MY_CUSTOM_MESSAGE (WM_USER + 1)

// Used to tell the 2nd thread to stop
std::atomic<bool> PostMessages(true);


// MH_STATUS Wrapper class for easier error checking
class MHStatusWrapper {
public:
	MHStatusWrapper(MH_STATUS status) : m_status(status) {}

	operator bool() const {
		return m_status == MH_OK;
	}

	MH_STATUS GetStatus() const {
		return m_status;
	}

private:
	MH_STATUS m_status;
};

// Handle events such as closing via the X button, or the task manager...
BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		// Initiate shutdown

		PostMessages = false;
		PostQuitMessage(0);

		return TRUE;  // Indicate that we handled the event
	default:
		return FALSE;
	}
}

// Original function pointer
typedef BOOL(WINAPI* GetMessage_t)(LPMSG, HWND, UINT, UINT);
GetMessage_t fpGetMessage = NULL;

// Hooked GetMessage function
BOOL WINAPI HookedGetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
	std::cout << "Entered GetMessage Hook\n";
	BOOL result = fpGetMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);

	// Check if the original GetMessage function retrieved a message
	if (result) {
		std::cout << "Received result\n";
		// Inject a 'B' key press message when any key is pressed
		if (lpMsg->message == WM_KEYDOWN) {
			std::cout << "WM_KEYDOWN Detected\n";
			lpMsg->wParam = 'B';  // Change the key to 'B'
		}
	}

	return result;
}

// Initialize, create and enable MinHook
MHStatusWrapper InstallHook() {

	// Initialize MinHook
	MHStatusWrapper status = MH_Initialize();
	if (!status) return status;

	// Create a hook for GetMessage
	status = MH_CreateHook(&GetMessage, &HookedGetMessage, reinterpret_cast<LPVOID*>(&fpGetMessage));
	if (!status) return status;

	// Enable the hook
	status = MH_EnableHook(&GetMessage);
	return status;
}

// Output the keys we press and exit if the End key is pressed
LRESULT CALLBACK HookProcedure(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* s = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

	if (wParam == WM_KEYDOWN && s->vkCode == VK_END)
	{
		// Initiate shutdown

		PostMessages = false;
		PostQuitMessage(0);

		return 1;
	}
	else if (wParam == WM_KEYDOWN)
	{
		// Output non VK_END keys

		char key = MapVirtualKey(s->vkCode, MAPVK_VK_TO_CHAR);
		std::cout << "KEY: " << key << '\n';
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Simulates messages (we lack a window to post messages normally)
void LoopPostCustomMessage(DWORD &threadId)
{
	while (PostMessages)
	{
		Sleep(1000);
		PostThreadMessage(threadId, MY_CUSTOM_MESSAGE, 0, 0);
	}
}


int main() {
	// get the thread id that the message loop is located in, so we know into which thread to post the messages

	DWORD threadId = GetCurrentThreadId();


	// Set the console control handler and initialize the hooks

	if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
	{
		std::cout << "Failed to set console control handler\n";
		return 1;
	}

	HHOOK kbd = SetWindowsHookEx(WH_KEYBOARD_LL, &HookProcedure, NULL, NULL);
	if (!kbd)
	{
		std::cerr << "Failed to hook keyboard\n";
		return 1;
	}

	MHStatusWrapper hookStatus = InstallHook();
	if (!hookStatus)
	{
		std::cerr << "Failed to install the hook: " << MH_StatusToString(hookStatus.GetStatus()) << '\n';
		return 1;
	}


	// Set-up a thread that will continuously post messages (console does not normally receive messages only those belonging to a window, so we have to emulate it)

	std::thread threadLoopMessages(LoopPostCustomMessage, std::ref(threadId));


	// Inform the user everything is ready\n

	std::cout << "READY\n";


	// Standard message loop

	MSG msg;
	while (GetMessage(&msg, NULL, NULL, NULL) > 0) {
		std::cout << "Message received\n";
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	// Cleanup & Exit

	threadLoopMessages.join();
	UnhookWindowsHookEx(kbd);
	MH_Uninitialize();


	std::cout << "Exited Application\n";
	return 0;
}
