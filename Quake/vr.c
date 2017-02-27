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

#define VR_SIDEMOVE_SPEED			200.0f
#define VR_FORWARDMOVE_SPEED		200.0f
#define VR_KEYINPUT_AXIS_DEADZONE	0.3f

typedef struct VREvent_t VREvent_t;
typedef struct VR_IVRSystem_FnTable		*VrSystem;
typedef struct VR_IVRCompositor_FnTable	*VrCompositor;

vrdef_t	vr;				// global vr state

static uint32_t		vr_token;
static VrSystem		vr_hmd;
static VrSystem		vr_context_system;
static VrCompositor	vr_context_compositor;

static void			VR_Context_Clear();
static void			VR_Context_CheckClear();
static VrSystem		VR_System();
static VrCompositor	VR_Compositor();

static void			VR_ProcessEvent(VREvent_t event);
static void			VR_Event_Button(VREvent_t event);
static qboolean		VR_GetControllerAxisState(ETrackedControllerRole role, EVRControllerAxisType axis_type, float *x, float *y);
static int			VR_GetKeyForAxisState(float x, float y);

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
VR_ProcessEvents
===================
*/
void VR_ProcessEvents()
{
	VREvent_t event;

	while (vr_hmd->PollNextEvent(&event, sizeof(event)))
		VR_ProcessEvent(event);
}

/*
===================
VR_ProcessEvent
===================
*/
static void VR_ProcessEvent(VREvent_t event)
{
	switch (event.eventType)
	{
	case EVREventType_VREvent_ButtonPress:
	case EVREventType_VREvent_ButtonUnpress:
		VR_Event_Button(event);
		break;
	default:
		break;
	}
}

/*
===================
VR_Event_Button
===================
*/
static void VR_Event_Button(VREvent_t event)
{
	ETrackedControllerRole role;
	qboolean down;
	float x, y;
	int key;
	
	role = vr_hmd->GetControllerRoleForTrackedDeviceIndex(event.trackedDeviceIndex);
	down = (event.eventType == EVREventType_VREvent_ButtonPress) ? true : false;

	if (role == ETrackedControllerRole_TrackedControllerRole_RightHand)
	{
		switch (event.data.controller.button)
		{
		case EVRButtonId_k_EButton_SteamVR_Trigger: Key_Event(K_CTRL, down); break;
		case EVRButtonId_k_EButton_ApplicationMenu: Key_Event('/', down); break;
		default: break;
		}
	}
	else if (role == ETrackedControllerRole_TrackedControllerRole_LeftHand)
	{
		switch (event.data.controller.button)
		{
		case EVRButtonId_k_EButton_SteamVR_Trigger: Key_Event(K_ENTER, down); break;
		case EVRButtonId_k_EButton_Grip: Key_Event(K_ESCAPE, down); break;
		case EVRButtonId_k_EButton_SteamVR_Touchpad:
			if (key_dest != key_game)
			{
				if (VR_GetControllerAxisState(role, EVRControllerAxisType_k_eControllerAxis_TrackPad, &x, &y))
				{
					key = VR_GetKeyForAxisState(x, y);
					if (key)
						Key_Event(key, down);
				}
			}
			break;
		default: break;
		}
	}
}

/*
===================
VR_GetControllerAxisState
===================
*/
static qboolean VR_GetControllerAxisState(ETrackedControllerRole role, EVRControllerAxisType axis_type, float *x, float *y)
{
	ETrackedPropertyError	err;
	TrackedDeviceIndex_t	device_index;
	VRControllerState_t		state;
	uint32_t				i;
	int32_t					found_axis_type;
	
	*x = 0.0f;
	*y = 0.0f;

	device_index = vr_hmd->GetTrackedDeviceIndexForControllerRole(role);

	if (vr_hmd->GetControllerState(device_index, &state, sizeof(state)))
	{
		for (i = 0; i < k_unControllerStateAxisCount; ++i)
		{
			found_axis_type = vr_hmd->GetInt32TrackedDeviceProperty(device_index, ETrackedDeviceProperty_Prop_Axis0Type_Int32 + i, &err);

			if (err == ETrackedPropertyError_TrackedProp_Success && found_axis_type == axis_type)
			{
				*x = state.rAxis[i].x;
				*y = state.rAxis[i].y;
				return true;
			}
		}
	}
	return false;
}

/*
===================
VR_GetKeyForAxisState
===================
*/
static int VR_GetKeyForAxisState(float x, float y)
{
	vec3_t vector;
	float angle;

	vector[0] = x;
	vector[1] = y;
	vector[2] = 0.0f;

	if (VectorLength(vector) > VR_KEYINPUT_AXIS_DEADZONE)
	{
		angle = atan2(y, x) / M_PI_DIV_180;

		if (angle <= 135.0f && angle > 45.0f)
			return K_UPARROW;
		else if (angle <= 45.0f && angle > -45.0f)
			return K_RIGHTARROW;
		else if (angle <= -45.0f && angle > -135.0f)
			return K_DOWNARROW;
		else
			return K_LEFTARROW;
	}
	else
	{
		return 0;
	}
}

/*
===================
VR_InputMove
===================
*/
void VR_InputMove(usercmd_t *cmd)
{
	float x, y;
	
	if (VR_GetControllerAxisState(ETrackedControllerRole_TrackedControllerRole_LeftHand, EVRControllerAxisType_k_eControllerAxis_TrackPad, &x, &y) ||
		VR_GetControllerAxisState(ETrackedControllerRole_TrackedControllerRole_LeftHand, EVRControllerAxisType_k_eControllerAxis_Joystick, &x, &y))
	{
		cmd->sidemove += VR_SIDEMOVE_SPEED * x;
		cmd->forwardmove += VR_FORWARDMOVE_SPEED * y;
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
	
	VR_Compositor()->Submit(vr.eye[eye].vreye, &vr.eye[eye].texture, NULL, EVRSubmitFlags_Submit_Default);

	if (eye == VR_EYE_RIGHT)
		VR_Compositor()->WaitGetPoses(NULL, 0, NULL, 0);
}

/*
===================
VR_GetVulkanInstanceExtensionsRequired
===================
*/
uint32_t VR_GetVulkanInstanceExtensionsRequired(char *extension_names, uint32_t buffer_size)
{
	return VR_Compositor()->GetVulkanInstanceExtensionsRequired(extension_names, buffer_size);
}

/*
===================
VR_GetVulkanDeviceExtensionsRequired
===================
*/
uint32_t VR_GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *physical_device, char *extension_names, uint32_t buffer_size)
{
	return VR_Compositor()->GetVulkanDeviceExtensionsRequired(physical_device, extension_names, buffer_size);
}

/*
===================
VR_Init
===================
*/
void VR_Init(void)
{
	EVRInitError	i_err;
	uint32_t		i;

	Con_Printf("\nVR Initialization\n");

	vr_token = VR_InitInternal(&i_err, EVRApplicationType_VRApplication_Scene);
	VR_Context_Clear();

	if (i_err == EVRInitError_VRInitError_None)
	{
		if (VR_IsInterfaceVersionValid(IVRSystem_Version))
			vr_hmd = VR_System();
		else
			i_err = EVRInitError_VRInitError_Init_InterfaceNotFound;
	}
	if (i_err != EVRInitError_VRInitError_None)
		Sys_Error("Couldn't init VR runtime: %s", VR_GetVRInitErrorAsEnglishDescription(i_err));
	
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
	}
}

/*
=================
VR_Context_Clear
=================
*/
static inline void VR_Context_Clear()
{
	vr_context_system = NULL;
	vr_context_compositor = NULL;
}

/*
=================
VR_Context_CheckClear
=================
*/
static inline void VR_Context_CheckClear()
{
	if (vr_token != VR_GetInitToken())
	{
		VR_Context_Clear();
		vr_token = VR_GetInitToken();
	}
}

/*
=================
VR_System
=================
*/
static VrSystem VR_System()
{
	EVRInitError	err;
	char			fntable_name[128];

	VR_Context_CheckClear();

	if (!vr_context_system)
	{
		q_snprintf(fntable_name, sizeof(fntable_name), "FnTable:%s", IVRSystem_Version);
		vr_context_system = (VrSystem)VR_GetGenericInterface(fntable_name, &err);
		if (err != EVRInitError_VRInitError_None)
			Sys_Error("Couldn't get VR system interface: %s", VR_GetVRInitErrorAsEnglishDescription(err));
	}
	return vr_context_system;
}

/*
=================
VR_Compositor
=================
*/
static VrCompositor VR_Compositor()
{
	EVRInitError	err;
	char			fntable_name[128];

	VR_Context_CheckClear();

	if (!vr_context_compositor)
	{
		q_snprintf(fntable_name, sizeof(fntable_name), "FnTable:%s", IVRCompositor_Version);
		vr_context_compositor = (VrCompositor)VR_GetGenericInterface(fntable_name, &err);
		if (err != EVRInitError_VRInitError_None)
			Sys_Error("Couldn't get VR compositor interface: %s", VR_GetVRInitErrorAsEnglishDescription(err));
	}
	return vr_context_compositor;
}