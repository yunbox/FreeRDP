/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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

#include <freerdp/log.h>
#include "shadow.h"

#include "shadow_audin.h"

#define TAG SERVER_TAG("shadow")

static const AUDIO_FORMAT supported_audio_formats[] =
{
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL }
};

static void AudinServerOpening(audin_server_context* context)
{
	AUDIO_FORMAT* agreed_format = NULL;
	int i = 0, j = 0;
	for (i = 0; i < context->num_client_formats; i++)
	{
		for (j = 0; j < context->num_server_formats; j++)
		{
			if ((context->client_formats[i].wFormatTag == context->server_formats[j].wFormatTag) &&
					(context->client_formats[i].nChannels == context->server_formats[j].nChannels) &&
					(context->client_formats[i].nSamplesPerSec == context->server_formats[j].nSamplesPerSec))
			{
				agreed_format = (AUDIO_FORMAT*) &context->server_formats[j];
				break;
			}
		}
		if (agreed_format != NULL)
			break;

	}

	if (agreed_format == NULL)
	{
		WLog_ERR(TAG, "Could not agree on a audio format with the server\n");
		return;
	}

	context->SelectFormat(context, i);
}
static void AudinServerOpenResult(audin_server_context* context, UINT32 result)
{
	WLog_INFO(TAG, "AUDIN open result %u.\n", result);
}
static void AudinServerReceiveSamples(audin_server_context* context, const void* buf, int nframes)
{
	rdpShadowClient* client = (rdpShadowClient* )context->data;
	rdpShadowSubsystem* subsystem = (rdpShadowSubsystem *)client->subsystem;
	if (subsystem->AudinServerReceiveSamples)
		subsystem->AudinServerReceiveSamples(subsystem, buf, nframes);
}

int shadow_client_audin_init(rdpShadowClient* client)
{
	audin_server_context* audin;
	audin = client->audin = audin_server_context_new(client->vcm);
	if (!audin)
	{
		return 0;
	}

	audin->data = client;

	audin->server_formats = supported_audio_formats;
	audin->num_server_formats = sizeof(supported_audio_formats) / sizeof(supported_audio_formats[0]);

	audin->dst_format = supported_audio_formats[0];

	audin->Opening = AudinServerOpening;
	audin->OpenResult = AudinServerOpenResult;
	audin->ReceiveSamples = AudinServerReceiveSamples;

	return 1;
}

