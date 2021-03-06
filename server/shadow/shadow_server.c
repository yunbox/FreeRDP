/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/wnd.h>
#include <winpr/path.h>
#include <winpr/cmdline.h>
#include <winpr/winsock.h>

#include <freerdp/log.h>
#include <freerdp/version.h>

#include <winpr/tools/makecert.h>

#ifndef _WIN32
#include <sys/select.h>
#include <signal.h>
#endif

#include "shadow.h"

#define TAG SERVER_TAG("shadow")

static COMMAND_LINE_ARGUMENT_A shadow_args[] =
{
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Server port" },
	{ "ipc-socket", COMMAND_LINE_VALUE_REQUIRED, "<ipc-socket>", NULL, NULL, -1, NULL, "Server IPC socket" },
	{ "monitors", COMMAND_LINE_VALUE_OPTIONAL, "<0,1,2...>", NULL, NULL, -1, NULL, "Select or list monitors" },
	{ "rect", COMMAND_LINE_VALUE_REQUIRED, "<x,y,w,h>", NULL, NULL, -1, NULL, "Select rectangle within monitor to share" },
	{ "auth", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL, "Clients must authenticate" },
	{ "may-view", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Clients may view without prompt" },
	{ "may-interact", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Clients may interact without prompt" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "Print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "Print help" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

int shadow_server_print_command_line_help(int argc, char** argv)
{
	char* str;
	int length;
	COMMAND_LINE_ARGUMENT_A* arg;

	WLog_INFO(TAG, "Usage: %s [options]", argv[0]);
	WLog_INFO(TAG, "");

	WLog_INFO(TAG, "Syntax:");
	WLog_INFO(TAG, "    /flag (enables flag)");
	WLog_INFO(TAG, "    /option:<value> (specifies option with value)");
	WLog_INFO(TAG, "    +toggle -toggle (enables or disables toggle, where '/' is a synonym of '+')");
	WLog_INFO(TAG, "");

	arg = shadow_args;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			WLog_INFO(TAG, "    %s", "/");
			WLog_INFO(TAG, "%-20s", arg->Name);
			WLog_INFO(TAG, "\t%s", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) || (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			WLog_INFO(TAG, "    %s", "/");

			if (arg->Format)
			{
				length = (int) (strlen(arg->Name) + strlen(arg->Format) + 2);
				str = (char*) malloc(length + 1);
				sprintf_s(str, length + 1, "%s:%s", arg->Name, arg->Format);
				WLog_INFO(TAG, "%-20s", str);
				free(str);
			}
			else
			{
				WLog_INFO(TAG, "%-20s", arg->Name);
			}

			WLog_INFO(TAG, "\t%s", arg->Text);
		}
		else if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			length = (int) strlen(arg->Name) + 32;
			str = (char*) malloc(length + 1);
			sprintf_s(str, length + 1, "%s (default:%s)", arg->Name,
					arg->Default ? "on" : "off");

			WLog_INFO(TAG, "    %s", arg->Default ? "-" : "+");

			WLog_INFO(TAG, "%-20s", str);
			free(str);

			WLog_INFO(TAG, "\t%s", arg->Text);
		}
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

int shadow_server_command_line_status_print(rdpShadowServer* server, int argc, char** argv, int status)
{
	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		WLog_INFO(TAG, "FreeRDP version %s (git %s)", FREERDP_VERSION_FULL, GIT_REVISION);
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		return COMMAND_LINE_STATUS_PRINT;
	}
	else if (status < 0)
	{
		shadow_server_print_command_line_help(argc, argv);
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	return 1;
}

int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	if (argc < 2)
		return 1;

	CommandLineClearArgumentsA(shadow_args);

	flags = COMMAND_LINE_SEPARATOR_COLON;
	flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;

	status = CommandLineParseArgumentsA(argc, (const char**) argv, shadow_args, flags, server, NULL, NULL);

	if (status < 0)
		return status;

	arg = shadow_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "port")
		{
			server->port = (DWORD) atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "ipc-socket")
		{
			server->ipcSocket = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "may-view")
		{
			server->mayView = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "may-interact")
		{
			server->mayInteract = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "rect")
		{
			char* p;
			char* tok[4];
			int x, y, w, h;
			char* str = _strdup(arg->Value);

			if (!str)
				return -1;

			tok[0] = p = str;

			p = strchr(p + 1, ',');

			if (!p)
				return -1;

			*p++ = '\0';
			tok[1] = p;

			p = strchr(p + 1, ',');

			if (!p)
				return -1;

			*p++ = '\0';
			tok[2] = p;

			p = strchr(p + 1, ',');

			if (!p)
				return -1;

			*p++ = '\0';
			tok[3] = p;

			x = atoi(tok[0]);
			y = atoi(tok[1]);
			w = atoi(tok[2]);
			h = atoi(tok[3]);

			if ((x < 0) || (y < 0) || (w < 1) || (h < 1))
				return -1;

			server->subRect.left = x;
			server->subRect.top = y;
			server->subRect.right = x + w;
			server->subRect.bottom = y + h;
			server->shareSubRect = TRUE;
		}
		CommandLineSwitchCase(arg, "auth")
		{
			server->authentication = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	arg = CommandLineFindArgumentA(shadow_args, "monitors");

	if (arg && (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
	{
		int index;
		int numMonitors;
		MONITOR_DEF monitors[16];

		numMonitors = shadow_enum_monitors(monitors, 16);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			/* Select monitors */

			index = atoi(arg->Value);

			if (index < 0)
				index = 0;

			if (index >= numMonitors)
				index = 0;

			server->selectedMonitor = index;
		}
		else
		{
			int width, height;
			MONITOR_DEF* monitor;

			/* List monitors */

			for (index = 0; index < numMonitors; index++)
			{
				monitor = &monitors[index];

				width = monitor->right - monitor->left;
				height = monitor->bottom - monitor->top;

				WLog_INFO(TAG, "      %s [%d] %dx%d\t+%d+%d",
					(monitor->flags == 1) ? "*" : " ", index,
					width, height, monitor->left, monitor->top);
			}

			status = COMMAND_LINE_STATUS_PRINT;
		}
	}

	return status;
}

void* shadow_server_thread(rdpShadowServer* server)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;
	freerdp_listener* listener;
	rdpShadowSubsystem* subsystem;

	listener = server->listener;
	StopEvent = server->StopEvent;
	subsystem = server->subsystem;

	shadow_subsystem_start(server->subsystem);

	while (1)
	{
		nCount = listener->GetEventHandles(listener, events, 32);
		if (0 == nCount)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		events[nCount++] = server->StopEvent;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(server->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (!listener->CheckFileDescriptor(listener))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}

#ifdef _WIN32
		Sleep(100); /* FIXME: listener event handles */
#endif
	}

	listener->Close(listener);

	shadow_subsystem_stop(server->subsystem);

	ExitThread(0);

	return NULL;
}

int shadow_server_start(rdpShadowServer* server)
{
	BOOL status;
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	server->screen = shadow_screen_new(server);

	if (!server->screen)
		return -1;

	server->capture = shadow_capture_new(server);

	if (!server->capture)
		return -1;

	if (!server->ipcSocket)
		status = server->listener->Open(server->listener, NULL, (UINT16) server->port);
	else
		status = server->listener->OpenLocal(server->listener, server->ipcSocket);

	if (!status)
		return -1;

	if (!(server->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				shadow_server_thread, (void*) server, 0, NULL)))
	{
		return -1;
	}

	return 0;
}

int shadow_server_stop(rdpShadowServer* server)
{
	if (server->thread)
	{
		SetEvent(server->StopEvent);
		WaitForSingleObject(server->thread, INFINITE);
		CloseHandle(server->thread);
		server->thread = NULL;

		server->listener->Close(server->listener);
	}

	if (server->screen)
	{
		shadow_screen_free(server->screen);
		server->screen = NULL;
	}

	if (server->capture)
	{
		shadow_capture_free(server->capture);
		server->capture = NULL;
	}

	return 0;
}

int shadow_server_init_config_path(rdpShadowServer* server)
{
#ifdef _WIN32
	if (!server->ConfigPath)
	{
		server->ConfigPath = GetEnvironmentSubPath("LOCALAPPDATA", "freerdp");
	}
#endif

#ifdef __APPLE__
	if (!server->ConfigPath)
	{
		char* userLibraryPath;
		char* userApplicationSupportPath;

		userLibraryPath = GetKnownSubPath(KNOWN_PATH_HOME, "Library");

		if (userLibraryPath)
		{
			if (!PathFileExistsA(userLibraryPath) &&
				!CreateDirectoryA(userLibraryPath, 0))
			{
				WLog_ERR(TAG, "Failed to create directory '%s'", userLibraryPath);
				free(userLibraryPath);
				return -1;
			}

			userApplicationSupportPath = GetCombinedPath(userLibraryPath, "Application Support");

			if (userApplicationSupportPath)
			{
				if (!PathFileExistsA(userApplicationSupportPath) &&
					!CreateDirectoryA(userApplicationSupportPath, 0))
				{
					WLog_ERR(TAG, "Failed to create directory '%s'", userApplicationSupportPath);
					free(userLibraryPath);
					free(userApplicationSupportPath);
					return -1;
				}
				server->ConfigPath = GetCombinedPath(userApplicationSupportPath, "freerdp");
			}

			free(userLibraryPath);
			free(userApplicationSupportPath);
		}
	}
#endif

	if (!server->ConfigPath)
	{
		char* configHome;

		configHome = GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME);

		if (configHome)
		{
			if (!PathFileExistsA(configHome) &&
				!CreateDirectoryA(configHome, 0))
			{
				WLog_ERR(TAG, "Failed to create directory '%s'", configHome);
				free(configHome);
				return -1;
			}
			server->ConfigPath = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, "freerdp");
			free(configHome);
		}
	}

	if (!server->ConfigPath)
		return -1; /* no usable config path */

	return 1;
}

int shadow_server_init_certificate(rdpShadowServer* server)
{
	char* filepath;
	MAKECERT_CONTEXT* makecert;

	const char* makecert_argv[6] =
	{
		"makecert",
		"-rdp",
		"-live",
		"-silent",
		"-y", "5"
	};

	int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

	if (!PathFileExistsA(server->ConfigPath) &&
		!CreateDirectoryA(server->ConfigPath, 0))
	{
		WLog_ERR(TAG, "Failed to create directory '%s'", server->ConfigPath);
		return -1;
	}

	if (!(filepath = GetCombinedPath(server->ConfigPath, "shadow")))
		return -1;

	if (!PathFileExistsA(filepath) &&
		!CreateDirectoryA(filepath, 0))
	{
		WLog_ERR(TAG, "Failed to create directory '%s'", filepath);
		free(filepath);
		return -1;
	}

	server->CertificateFile = GetCombinedPath(filepath, "shadow.crt");
	server->PrivateKeyFile = GetCombinedPath(filepath, "shadow.key");

	if ((!PathFileExistsA(server->CertificateFile)) ||
			(!PathFileExistsA(server->PrivateKeyFile)))
	{
		makecert = makecert_context_new();

		makecert_context_process(makecert, makecert_argc, (char**) makecert_argv);

		makecert_context_set_output_file_name(makecert, "shadow");

		if (!PathFileExistsA(server->CertificateFile))
			makecert_context_output_certificate_file(makecert, filepath);

		if (!PathFileExistsA(server->PrivateKeyFile))
			makecert_context_output_private_key_file(makecert, filepath);

		makecert_context_free(makecert);
	}

	free(filepath);

	return 1;
}

int shadow_server_init(rdpShadowServer* server)
{
	int status;

	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());

	if (!(server->clients = ArrayList_New(TRUE)))
		goto fail_client_array;

	if (!(server->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_stop_event;

	if (!InitializeCriticalSectionAndSpinCount(&(server->lock), 4000))
		goto fail_server_lock;

	status = shadow_server_init_config_path(server);

	if (status < 0)
		goto fail_config_path;

	status = shadow_server_init_certificate(server);

	if (status < 0)
		goto fail_certificate;

	server->listener = freerdp_listener_new();

	if (!server->listener)
		goto fail_listener;

	server->listener->info = (void*) server;
	server->listener->PeerAccepted = shadow_client_accepted;

	server->subsystem = shadow_subsystem_new();

	if (!server->subsystem)
		goto fail_subsystem_new;

	status = shadow_subsystem_init(server->subsystem, server);

	if (status >= 0)
		return status;

fail_subsystem_new:
	freerdp_listener_free(server->listener);
	server->listener = NULL;
fail_listener:
	free(server->CertificateFile);
	server->CertificateFile = NULL;
	free(server->PrivateKeyFile);
	server->PrivateKeyFile = NULL;
fail_certificate:
	free(server->ConfigPath);
	server->ConfigPath = NULL;
fail_config_path:
	DeleteCriticalSection(&(server->lock));
fail_server_lock:
	CloseHandle(server->StopEvent);
	server->StopEvent = NULL;
fail_stop_event:
	ArrayList_Free(server->clients);
	server->clients = NULL;
fail_client_array:
	WLog_ERR(TAG, "Failed to initialize shadow server");
	return -1;
}

int shadow_server_uninit(rdpShadowServer* server)
{
	shadow_server_stop(server);

	if (server->listener)
	{
		freerdp_listener_free(server->listener);
		server->listener = NULL;
	}

	if (server->CertificateFile)
	{
		free(server->CertificateFile);
		server->CertificateFile = NULL;
	}
	
	if (server->PrivateKeyFile)
	{
		free(server->PrivateKeyFile);
		server->PrivateKeyFile = NULL;
	}

	if (server->ipcSocket)
	{
		free(server->ipcSocket);
		server->ipcSocket = NULL;
	}

	shadow_subsystem_uninit(server->subsystem);

	return 1;
}

rdpShadowServer* shadow_server_new()
{
	rdpShadowServer* server;

	server = (rdpShadowServer*) calloc(1, sizeof(rdpShadowServer));

	if (!server)
		return NULL;

	server->port = 3389;
	server->mayView = TRUE;
	server->mayInteract = TRUE;

	server->authentication = FALSE;

	return server;
}

void shadow_server_free(rdpShadowServer* server)
{
	if (!server)
		return;

	DeleteCriticalSection(&(server->lock));

	if (server->clients)
	{
		ArrayList_Free(server->clients);
		server->clients = NULL;
	}

	shadow_subsystem_free(server->subsystem);

	free(server);
}

