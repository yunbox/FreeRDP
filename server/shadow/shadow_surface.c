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

#include "shadow_surface.h"

rdpShadowSurface* shadow_surface_new(rdpShadowServer* server, int x, int y, int width, int height)
{
	rdpShadowSurface* surface;

	surface = (rdpShadowSurface*) calloc(1, sizeof(rdpShadowSurface));

	if (!surface)
		return NULL;

	surface->server = server;

	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	surface->scanline = (surface->width + (surface->width % 4)) * 4;

	surface->data = (BYTE*) calloc(1, surface->scanline * surface->height);
	if (!surface->data)
	{
		free (surface);
		return NULL;
	}

	if (!InitializeCriticalSectionAndSpinCount(&(surface->lock), 4000))
	{
		free (surface->data);
		free (surface);
		return NULL;
	}

	region16_init(&(surface->invalidRegion));

	return surface;
}

void shadow_surface_free(rdpShadowSurface* surface)
{
	if (!surface)
		return;

	free(surface->data);

	DeleteCriticalSection(&(surface->lock));

	region16_uninit(&(surface->invalidRegion));

	free(surface);
}

void shadow_surface_reset(rdpShadowSurface *surface, int x, int y, int width, int height)
{
	BYTE* buffer = NULL;
	int scanline = (width + (width % 4)) * 4;
	RECTANGLE_16 newRect;

	if (!surface)
		return;

	buffer = (BYTE*) calloc(1, scanline * height * 2);
	if (buffer)
	{
		surface->x = x;
		surface->y = y;
		surface->width = width;
		surface->height = height;
		surface->scanline = scanline;

		free(surface->data);
		surface->data = buffer;

		newRect.left = newRect.top = 0;
		newRect.right = width;
		newRect.bottom = height;

		// constraint invalid rect with new bound
		region16_intersect_rect(&(surface->invalidRegion), &(surface->invalidRegion), &newRect);
	}
}
