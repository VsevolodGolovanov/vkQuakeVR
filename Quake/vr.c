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
// vr.c -- virtual reality headset support

#include "quakedef.h"

typedef struct VR_IVRSystem_FnTable		*VrSystem;
typedef struct VR_IVRCompositor_FnTable	*VrCompositor;

vrdef_t	vr;				// global vr state

//====================================

static VrSystem					vr_hmd;
static VrCompositor				vr_compositor;
static VRVulkanTextureData_t	vr_texture_data[2];
static Texture_t				vr_texture[2];

/*
===================
VR_Submit
===================
*/
void VR_Submit(VkImage left_color_buffer, VkImage right_color_buffer)
{
	vr_texture_data[0].m_nImage = (uint64_t)left_color_buffer;
	vr_compositor->Submit(EVREye_Eye_Left, &vr_texture[0], NULL, EVRSubmitFlags_Submit_Default);

	vr_texture_data[1].m_nImage = (uint64_t)right_color_buffer;
	vr_compositor->Submit(EVREye_Eye_Right, &vr_texture[1], NULL, EVRSubmitFlags_Submit_Default);

	vr_compositor->WaitGetPoses(NULL, 0, NULL, 0);
}

/*
===================
VR_SetTextureData
===================
*/
void VR_SetTextureData(VRVulkanTextureData_t texture_data)
{
	vr_texture_data[0] = texture_data;
	vr_texture_data[1] = texture_data;
}

/*
===================
VR_GetVulkanInstanceExtensionsRequired
===================
*/
uint32_t VR_GetVulkanInstanceExtensionsRequired(char *extension_names, uint32_t buffer_size)
{
	return vr_compositor->GetVulkanInstanceExtensionsRequired(extension_names, buffer_size);
}

/*
===================
VR_GetVulkanDeviceExtensionsRequired
===================
*/
uint32_t VR_GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *physical_device, char *extension_names, uint32_t buffer_size)
{
	return vr_compositor->GetVulkanDeviceExtensionsRequired(physical_device, extension_names, buffer_size);
}

/*
===================
VR_Init
===================
*/
void VR_Init(void)
{
	EVRInitError i_err;
	char fntable_name[128];

	Con_Printf("\nVR Initialization\n");

	VR_InitInternal(&i_err, EVRApplicationType_VRApplication_Scene);
	if (i_err != EVRInitError_VRInitError_None)
		Sys_Error("Couldn't init VR runtime: %s", VR_GetVRInitErrorAsEnglishDescription(i_err));

	sprintf_s(fntable_name, sizeof(fntable_name), "FnTable:%s", IVRSystem_Version);
	vr_hmd = (VrSystem)VR_GetGenericInterface(fntable_name, &i_err);
	if (i_err != EVRInitError_VRInitError_None)
		Sys_Error("Couldn't get VR system interface: %s", VR_GetVRInitErrorAsEnglishDescription(i_err));

	sprintf_s(fntable_name, sizeof(fntable_name), "FnTable:%s", IVRCompositor_Version);
	vr_compositor = (VrCompositor)VR_GetGenericInterface(fntable_name, &i_err);
	if (i_err != EVRInitError_VRInitError_None)
		Sys_Error("Couldn't get VR compositor interface: %s", VR_GetVRInitErrorAsEnglishDescription(i_err));

	ETrackedPropertyError p_err;
	char *buf;
	uint32_t len;

	len = vr_hmd->GetStringTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_ManufacturerName_String, NULL, 0, &p_err);
	if (len && p_err == ETrackedPropertyError_TrackedProp_BufferTooSmall)
	{
		buf = malloc(len);
		vr_hmd->GetStringTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_ManufacturerName_String, buf, len, &p_err);
		if (p_err == ETrackedPropertyError_TrackedProp_Success)
			Con_Printf("Vendor: %s\n", buf);
		free(buf);
	}

	len = vr_hmd->GetStringTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_ModelNumber_String, NULL, 0, &p_err);
	if (len && p_err == ETrackedPropertyError_TrackedProp_BufferTooSmall)
	{
		buf = malloc(len);
		vr_hmd->GetStringTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_ModelNumber_String, buf, len, &p_err);
		if (p_err == ETrackedPropertyError_TrackedProp_Success)
			Con_Printf("Device: %s\n", buf);
		free(buf);
	}

	vr_hmd->GetRecommendedRenderTargetSize(&vr.width, &vr.height);
	Con_Printf("Render target size: %u x %u\n", vr.width, vr.height);

	vr_texture[0].eColorSpace = EColorSpace_ColorSpace_Auto;
	vr_texture[0].eType = ETextureType_TextureType_Vulkan;
	vr_texture[0].handle = &vr_texture_data[0];
	vr_texture[1].eColorSpace = EColorSpace_ColorSpace_Auto;
	vr_texture[1].eType = ETextureType_TextureType_Vulkan;
	vr_texture[1].handle = &vr_texture_data[1];
}

/*
=================
VR_Shutdown
=================
*/
void VR_Shutdown (void)
{
	if (vr_hmd)
	{
		VR_ShutdownInternal();
		vr_hmd = NULL;
		vr_compositor = NULL;
	}
}