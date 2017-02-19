/*
Copyright (C) 2017 Felix Rueegg

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __VR_DEFS_H
#define __VR_DEFS_H

// vr.h -- virtual reality headset support

#define VR_EYE_LEFT		0
#define VR_EYE_RIGHT	1
#define NUM_VR_EYES		2

typedef struct
{
	EVREye					vreye;
	Texture_t				texture;
	VRVulkanTextureData_t	texture_data;
} vreye_t;

typedef struct
{
	vreye_t			eye[NUM_VR_EYES];
	uint32_t		current_eye;
	uint32_t		width;
	uint32_t		height;
} vrdef_t;

extern	vrdef_t vr;				// global vr state

void		VR_Init (void);
void		VR_Shutdown (void);

void		VR_Submit(uint32_t eye, VkImage color_buffer);
uint32_t	VR_GetVulkanInstanceExtensionsRequired(char *extension_names, uint32_t buffer_size);
uint32_t	VR_GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *physical_device, char *extension_names, uint32_t buffer_size);

#endif	/* __VR_DEFS_H */

