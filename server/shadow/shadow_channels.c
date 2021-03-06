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

#include "shadow.h"

#include "shadow_channels.h"

int shadow_client_channels_post_connect(rdpShadowClient* client)
{
	if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, ENCOMSP_SVC_CHANNEL_NAME))
	{
		shadow_client_encomsp_init(client);
	}

	if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, REMDESK_SVC_CHANNEL_NAME))
	{
		shadow_client_remdesk_init(client);
	}

	if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, "rdpsnd"))
	{
		shadow_client_rdpsnd_init(client);
	}

	shadow_client_audin_init(client);

	return 1;
}

void shadow_client_channels_free(rdpShadowClient* client)
{
	if (client->audin)
	{
		audin_server_context_free(client->audin);
	}

	if (client->rdpsnd)
	{
		(void)client->rdpsnd->Stop(client->rdpsnd);
		rdpsnd_server_context_free(client->rdpsnd);
	}

	if (client->remdesk)
		remdesk_server_context_free(client->remdesk);

	if (client->encomsp)
		encomsp_server_context_free(client->encomsp);
}
