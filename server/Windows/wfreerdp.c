/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <freerdp/constants.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/locale/keyboard.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/listener.h>

HANDLE g_done_event;
int g_thread_count = 0;

struct wf_peer_context
{
	rdpContext _p;
	boolean activated;
};
typedef struct wf_peer_context wfPeerContext;

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{

}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	if (context)
	{

	}
}

static void wf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	freerdp_peer_context_new(client);
}

boolean wf_peer_post_connect(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */

	printf("Client %s is activated (osMajorType %d osMinorType %d)", client->local ? "(local)" : client->hostname,
		client->settings->os_major_type, client->settings->os_minor_type);

	if (client->settings->autologon)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->domain ? client->settings->domain : "",
			client->settings->username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->width, client->settings->height, client->settings->color_depth);

	/* A real server should tag the peer as activated here and start sending updates in main loop. */

	/* Return false here would stop the execution of the peer mainloop. */
	return true;
}

boolean wf_peer_activate(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	context->activated = true;

	return true;
}

void wf_peer_synchronize_event(rdpInput* input, uint32 flags)
{
	//printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void wf_peer_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	INPUT keyboard_event;

	//printf("Client sent a keyboard event (flags:0x%X code:0x%X)\n", flags, code);

	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_SCANCODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		keyboard_event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	SendInput(1, &keyboard_event, sizeof(INPUT));
}

void wf_peer_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	INPUT keyboard_event;

	//printf("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);

	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_UNICODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &keyboard_event, sizeof(INPUT));
}

void wf_peer_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	INPUT mouse_event;

	//printf("Client sent a mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);

	ZeroMemory(&mouse_event, sizeof(INPUT));
	mouse_event.type = INPUT_MOUSE;

	if (flags & PTR_FLAGS_WHEEL)
	{
		mouse_event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		mouse_event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			mouse_event.mi.mouseData *= -1;

		SendInput(1, &mouse_event, sizeof(INPUT));
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
		{
			mouse_event.mi.dx = x * (0xFFFF / GetSystemMetrics(SM_CXSCREEN));
			mouse_event.mi.dy = y * (0xFFFF / GetSystemMetrics(SM_CYSCREEN));
			mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}

		mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
	}
}

void wf_peer_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	//printf("Client sent an extended mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		INPUT mouse_event;
		ZeroMemory(&mouse_event, sizeof(INPUT));

		mouse_event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			mouse_event.mi.dx = x * (0xFFFF / GetSystemMetrics(SM_CXSCREEN));
			mouse_event.mi.dy = y * (0xFFFF / GetSystemMetrics(SM_CYSCREEN));
			mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}

		mouse_event.mi.dx = mouse_event.mi.dy = mouse_event.mi.dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			mouse_event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			mouse_event.mi.mouseData = XBUTTON2;

		SendInput(1, &mouse_event, sizeof(INPUT));
	}
	else
	{
		wf_peer_mouse_event(input, flags, x, y);
	}
}

static DWORD WINAPI wf_peer_main_loop(LPVOID lpParam)
{
	int rcount;
	void* rfds[32];
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	memset(rfds, 0, sizeof(rfds));

	wf_peer_init(client);

	/* Initialize the real server settings here */
	client->settings->cert_file = xstrdup("server.crt");
	client->settings->privatekey_file = xstrdup("server.key");

	client->PostConnect = wf_peer_post_connect;
	client->Activate = wf_peer_activate;

	client->input->SynchronizeEvent = wf_peer_synchronize_event;
	client->input->KeyboardEvent = wf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = wf_peer_unicode_keyboard_event;
	client->input->MouseEvent = wf_peer_mouse_event;
	client->input->ExtendedMouseEvent = wf_peer_extended_mouse_event;

	client->Initialize(client);
	context = (wfPeerContext*) client->context;

	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		if (client->CheckFileDescriptor(client) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	printf("Client %s disconnected.\n", client->local ? "(local)" : client->hostname);

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return 0;
}

static void wf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	/* start peer main loop thread */

	if (CreateThread(NULL, 0, wf_peer_main_loop, client, 0, NULL) != 0)
		g_thread_count++;
}

static void wf_server_main_loop(freerdp_listener* instance)
{
	int rcount;
	void* rfds[32];

	memset(rfds, 0, sizeof(rfds));

	while (1)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		if (instance->CheckFileDescriptor(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	instance->Close(instance);
}

int main(int argc, char* argv[])
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	int port = 3389;
	WSADATA wsa_data;
	freerdp_listener* instance;

	instance = freerdp_listener_new();

	instance->PeerAccepted = wf_peer_accepted;

	if (WSAStartup(0x101, &wsa_data) != 0)
		return 1;

	g_done_event = CreateEvent(0, 1, 0, 0);

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("DefaultPort"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			port = dwValue;
	}

	RegCloseKey(hKey);

	if (argc == 2)
		port = atoi(argv[1]);

	/* Open the server socket and start listening. */

	if (instance->Open(instance, NULL, port))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		wf_server_main_loop(instance);
	}

	if (g_thread_count > 0)
		WaitForSingleObject(g_done_event, INFINITE);
	else
		MessageBox(GetConsoleWindow(),
			L"Failed to start wfreerdp-server.\n\nPlease check the debug output.",
			L"FreeRDP Error", MB_ICONSTOP);

	WSACleanup();

	freerdp_listener_free(instance);

	return 0;
}

