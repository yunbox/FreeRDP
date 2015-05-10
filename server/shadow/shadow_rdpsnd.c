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

#include "shadow_rdpsnd.h"

#define TAG SERVER_TAG("shadow")

static const AUDIO_FORMAT supported_audio_formats[] =
{
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL }
};

static void rdpsnd_activated(RdpsndServerContext* context)
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
	context->SetVolume(context, 0x7FFF, 0x7FFF);
}

int shadow_client_rdpsnd_init(rdpShadowClient* client)
{
	RdpsndServerContext* rdpsnd;

	rdpsnd = client->rdpsnd = rdpsnd_server_context_new(client->vcm);
	if (!rdpsnd)
	{
		return 0;
	}

	rdpsnd->data = client;

	rdpsnd->server_formats = supported_audio_formats;
	rdpsnd->num_server_formats =
		sizeof(supported_audio_formats) / sizeof(supported_audio_formats[0]);

	rdpsnd->src_format = supported_audio_formats[0];

	rdpsnd->Activated = rdpsnd_activated;

	rdpsnd->Initialize(rdpsnd, TRUE);

	return 1;

}

