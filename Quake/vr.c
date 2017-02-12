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

static VrSystem		vr_hmd;
static VrCompositor	vr_compositor;

/*
===================
VR_Init
===================
*/
void VR_Init (void)
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