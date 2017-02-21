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
VR_ConvertFromHmdMatrix34
===================
*/
static void VR_ConvertFromHmdMatrix34(float *dst, HmdMatrix34_t *src)
{
	int c, r;

	for (c = 0; c < 4; ++c)
	{
		for (r = 0; r < 4; ++r)
		{
			if (r < 3)
				dst[c * 4 + r] = src->m[r][c];
			else
				dst[c * 4 + r] = 0.0f;
		}
	}
}

/*
===================
VR_ConvertFromHmdMatrix44
===================
*/
static void VR_ConvertFromHmdMatrix44(float *dst, HmdMatrix44_t *src)
{
	int c, r;

	for (c = 0; c < 4; ++c)
	{
		for (r = 0; r < 4; ++r)
		{
			dst[c * 4 + r] = src->m[r][c];
		}
	}
}

/*
===================
VR_GetPosition
===================
*/
void VR_GetPosition(TrackedDevicePose_t pose, float *pos_x, float *pos_y, float *pos_z)
{
	HmdMatrix34_t mat;

	mat = pose.mDeviceToAbsoluteTracking;

	*pos_x = mat.m[0][3];
	*pos_y = mat.m[1][3];
	*pos_z = mat.m[2][3];
}

/*
===================
VR_GetOrientation
===================
*/
void VR_GetOrientation(TrackedDevicePose_t pose, float *pitch, float *yaw, float *roll)
{
	HmdMatrix34_t mat;
	float pM, a, b, c;

	mat = pose.mDeviceToAbsoluteTracking;

	pM = -mat.m[1][2];
	if (pM < -1.0f + 0.0000001f)
	{ // South pole singularity
		a = 0.0f;
		b = (float)(M_PI * -0.5);
		c = (float)atan2(-mat.m[0][1], mat.m[0][0]);
	}
	else if (pM > 1.0f - 0.0000001f)
	{ // North pole singularity
		a = 0.0f;
		b = (float)M_PI * 0.5;
		c = (float)atan2(-mat.m[0][1], mat.m[0][0]);
	}
	else
	{
		a = (float)atan2(mat.m[0][2], mat.m[2][2]);
		b = (float)asin(pM);
		c = (float)atan2(mat.m[1][0], mat.m[1][1]);
	}

	*pitch = (float)(-b * 180.0f / M_PI);
	*yaw = (float)(a * 180.0f / M_PI);
	*roll = (float)(-c * 180.0f / M_PI);
}

/*
===================
VR_UpdatePose
===================
*/
void VR_UpdatePose(void)
{
	float seconds_since_vsync, display_frequency, frame_duration, vsync_to_photons, predicted_seconds_from_now;

	vr_hmd->GetTimeSinceLastVsync(&seconds_since_vsync, NULL);

	display_frequency = vr_hmd->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL);
	frame_duration = 1.f / display_frequency;
	vsync_to_photons = vr_hmd->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, NULL);
	predicted_seconds_from_now = frame_duration - seconds_since_vsync + vsync_to_photons;

	vr_hmd->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, predicted_seconds_from_now, vr.pose, MAX_VR_TRACKED_DEVICE_POSES);
}

/*
===================
VR_UpdateEyeToHeadTransform
===================
*/
static void VR_UpdateEyeToHeadTransform(void)
{
	uint32_t i, c, r;
	HmdMatrix34_t mat;

	for (i = 0; i < NUM_VR_EYES; ++i)
	{
		mat = vr_hmd->GetEyeToHeadTransform(vr.eye[i].vreye);

		for (c = 0; c < 4; ++c)
		{
			for (r = 0; r < 3; ++r)
			{
				vr.eye[i].eye_to_head_transform[c * 4 + r] = mat.m[r][c];
			}
		}
		vr.eye[i].eye_to_head_transform[0 * 4] = 0.0f;
		vr.eye[i].eye_to_head_transform[1 * 4] = 0.0f;
		vr.eye[i].eye_to_head_transform[2 * 4] = 0.0f;
		vr.eye[i].eye_to_head_transform[3 * 4] = 1.0f;
	}
}

/*
===================
VR_UpdateProjection
===================
*/
static void VR_UpdateProjection(void)
{
	uint32_t i;
	HmdMatrix44_t mat;

	for (i = 0; i < NUM_VR_EYES; ++i)
	{
		mat = vr_hmd->GetProjectionMatrix(vr.eye[i].vreye, 4.0f, 16384.0f);
		mat.m[1][1] = -mat.m[1][1];
		VR_ConvertFromHmdMatrix44(vr.eye[i].projection, &mat);
	}
	vr.fov_x = (float)(atan(1 / vr.eye[VR_EYE_LEFT].projection[0]) * 360.0f / M_PI);
	vr.fov_y = (float)(atan(1 / -vr.eye[VR_EYE_LEFT].projection[1 * 4 + 1]) * 360.0f / M_PI);
}

/*
===================
VR_Submit
===================
*/
void VR_Submit(uint32_t eye, VkImage color_buffer)
{
	vr.eye[eye].texture_data.m_nImage = (uint64_t)color_buffer;
	
	vr_compositor->Submit(vr.eye[eye].vreye, &vr.eye[eye].texture, NULL, EVRSubmitFlags_Submit_Default);

	if (eye == VR_EYE_RIGHT)
		vr_compositor->WaitGetPoses(NULL, 0, NULL, 0);
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
	EVRInitError	i_err;
	char			fntable_name[128];
	uint32_t		i;

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

	for (i = 0; i < NUM_VR_EYES; ++i)
	{
		vr.eye[i].texture.handle = &vr.eye[i].texture_data;
		vr.eye[i].texture.eColorSpace = EColorSpace_ColorSpace_Auto;
		vr.eye[i].texture.eType = ETextureType_TextureType_Vulkan;

		if (i == VR_EYE_LEFT)
			vr.eye[i].vreye = EVREye_Eye_Left;
		else
			vr.eye[i].vreye = EVREye_Eye_Right;
	}

	VR_UpdateProjection();
	VR_UpdateEyeToHeadTransform();
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