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

cvar_t vr_lefthanded = { "vr_lefthanded", "0", CVAR_NONE };
cvar_t vr_gunangle = { "vr_gunangle", "32", CVAR_NONE };

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

TrackedDevicePose_t* VR_GetControllerPosition(ETrackedControllerRole role, float *pos_x, float *pos_y, float *pos_z)
{
	TrackedDeviceIndex_t index = vr_hmd->GetTrackedDeviceIndexForControllerRole(role);
	TrackedDevicePose_t pose = vr.pose[index];
	if (pose.bPoseIsValid) {
		VR_GetPosition(pose, pos_x, pos_y, pos_z);
		return &vr.pose[index];
	}
	else {
		return NULL;
	}
}

/*
===================
VR_GetVelocity
===================
*/
void VR_GetVelocity(TrackedDevicePose_t pose, float *x, float *y, float *z)
{
	*x = pose.vVelocity.v[0];
	*y = pose.vVelocity.v[1];
	*z = pose.vVelocity.v[2];
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
VR_UpdateHiddenAreaMesh
===================
*/
static void VR_UpdateHiddenAreaMesh(void)
{
	uint32_t i;

	for (i = 0; i < NUM_VR_EYES; ++i)
	{
		vr.eye[i].hidden_area_mesh = vr_hmd->GetHiddenAreaMesh(vr.eye[i].vreye, EHiddenAreaMeshType_k_eHiddenAreaMesh_Standard);
	}
}

/*
=============
VR_DrawHiddenAreaMesh
=============
*/
void VR_DrawHiddenAreaMesh(void)
{
	int vertex_count = vr.eye[vr.current_eye].hidden_area_mesh.unTriangleCount * 3;
	
	if (!vertex_count)
		return;

	GL_SetCanvas(CANVAS_HIDDENAREAMESH);

	int i;
	VkBuffer buffer;
	VkDeviceSize buffer_offset;
	basicvertex_t * vertices = (basicvertex_t*)R_VertexAllocate(vertex_count * sizeof(basicvertex_t), &buffer, &buffer_offset);

	for (i = 0; i < vertex_count; ++i)
	{
		vertices[i].position[0] = vr.eye[vr.current_eye].hidden_area_mesh.pVertexData[i].v[0];
		vertices[i].position[1] = vr.eye[vr.current_eye].hidden_area_mesh.pVertexData[i].v[1];
		vertices[i].position[2] = 0.0f;
		vertices[i].texcoord[0] = 0.0f;
		vertices[i].texcoord[1] = 0.0f;
		vertices[i].color[0] = 0;
		vertices[i].color[1] = 0;
		vertices[i].color[2] = 0;
		vertices[i].color[3] = 255;
	}

	vkCmdBindVertexBuffers(vulkan_globals.command_buffer, 0, 1, &buffer, &buffer_offset);
	vkCmdBindPipeline(vulkan_globals.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_globals.hidden_area_mesh_pipeline);
	vkCmdDraw(vulkan_globals.command_buffer, vertex_count, 1, 0, 0);
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

	VR_ResetOrientation();

	VR_UpdateHiddenAreaMesh();
	VR_UpdateProjection();
	VR_UpdateEyeToHeadTransform();

	Cvar_RegisterVariable(&vr_lefthanded);
	Cvar_RegisterVariable(&vr_gunangle);
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

// Transforms a HMD Matrix34 to a Vector3
// Math borrowed from https://github.com/Omnifinity/OpenVR-Tracking-Example
HmdVector3_t Matrix34ToVector(HmdMatrix34_t in)
{
	HmdVector3_t vector;

	vector.v[0] = in.m[0][3];
	vector.v[1] = in.m[1][3];
	vector.v[2] = in.m[2][3];

	return vector;
}

// Transforms a HMD Matrix34 to a Quaternion
// Function logic nicked from https://github.com/Omnifinity/OpenVR-Tracking-Example
HmdQuaternion_t Matrix34ToQuaternion(HmdMatrix34_t in)
{
	HmdQuaternion_t q;

	q.w = sqrt(fmax(0, 1 + in.m[0][0] + in.m[1][1] + in.m[2][2])) / 2;
	q.x = sqrt(fmax(0, 1 + in.m[0][0] - in.m[1][1] - in.m[2][2])) / 2;
	q.y = sqrt(fmax(0, 1 - in.m[0][0] + in.m[1][1] - in.m[2][2])) / 2;
	q.z = sqrt(fmax(0, 1 - in.m[0][0] - in.m[1][1] + in.m[2][2])) / 2;
	q.x = copysign(q.x, in.m[2][1] - in.m[1][2]);
	q.y = copysign(q.y, in.m[0][2] - in.m[2][0]);
	q.z = copysign(q.z, in.m[1][0] - in.m[0][1]);
	return q;
}

// Following functions borrowed from http://icculus.org/~phaethon/q3a/misc/quats.html
void AnglesToQuat(const vec3_t angles, vec4_t quat)
{
	vec3_t a;
	float cr, cp, cy, sr, sp, sy, cpcy, spsy;

	a[PITCH] = (M_PI / 360.0) * angles[PITCH];
	a[YAW] = (M_PI / 360.0) * angles[YAW];
	a[ROLL] = (M_PI / 360.0) * angles[ROLL];

	cr = cos(a[ROLL]);
	cp = cos(a[PITCH]);
	cy = cos(a[YAW]);

	sr = sin(a[ROLL]);
	sp = sin(a[PITCH]);
	sy = sin(a[YAW]);

	cpcy = cp * cy;
	spsy = sp * sy;
	quat[0] = cr * cpcy + sr * spsy; // w
	quat[1] = sr * cpcy - cr * spsy; // x
	quat[2] = cr * sp * cy + sr * cp * sy; // y
	quat[3] = cr * cp * sy - sr * sp * cy; // z
}

HmdQuaternion_t AnglesToHmdQuat(const vec3_t angles)
{
	HmdQuaternion_t final;
	vec3_t a;
	float cr, cp, cy, sr, sp, sy, cpcy, spsy;

	a[PITCH] = (M_PI / 360.0) * angles[PITCH];
	a[YAW] = (M_PI / 360.0) * angles[YAW];
	a[ROLL] = (M_PI / 360.0) * angles[ROLL];

	cr = cos(a[ROLL]);
	cp = cos(a[PITCH]);
	cy = cos(a[YAW]);

	sr = sin(a[ROLL]);
	sp = sin(a[PITCH]);
	sy = sin(a[YAW]);

	cpcy = cp * cy;
	spsy = sp * sy;
	final.w = cr * cpcy + sr * spsy; // w
	final.x = sr * cpcy - cr * spsy; // x
	final.y = cr * sp * cy + sr * cp * sy; // y
	final.z = cr * cp * sy - sr * sp * cy; // z

	return final;
}

// Converts a quaternion to a euler angle
void QuatToAngles(const vec4_t q, vec3_t a)
{
	vec4_t q2;
	q2[0] = q[0] * q[0];
	q2[1] = q[1] * q[1];
	q2[2] = q[2] * q[2];
	q2[3] = q[3] * q[3];
	a[ROLL] = (180.0 / M_PI)*atan2(2 * (q[2] * q[3] + q[1] * q[0]), (-q2[1] - q2[2] + q2[3] + q2[0]));
	a[PITCH] = (180.0 / M_PI)*asin(-2 * (q[1] * q[3] - q[2] * q[0]));
	a[YAW] = (180.0 / M_PI)*atan2(2 * (q[1] * q[2] + q[3] * q[0]), (q2[1] - q2[2] - q2[3] + q2[0]));
}

// HmdQuaternion_t version
void HmdQuatToAngles(const HmdQuaternion_t q, vec3_t a)
{
	vec4_t q2;
	q2[0] = q.w * q.w;
	q2[1] = q.x * q.x;
	q2[2] = q.y * q.y;
	q2[3] = q.z * q.z;
	a[ROLL] = (180.0 / M_PI)*atan2(2 * (q.y * q.z + q.x * q.w), (-q2[1] - q2[2] + q2[3] + q2[0]));
	a[PITCH] = (180.0 / M_PI)*asin(-2 * (q.x * q.z - q.y * q.w));
	a[YAW] = (180.0 / M_PI)*atan2(2 * (q.x * q.y + q.z * q.w), (q2[1] - q2[2] - q2[3] + q2[0]));
}

// Rotates a vector by a quaternion and returns the results
// Based on math from https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
HmdVector3_t RotateVectorByQuaternion(HmdVector3_t v, HmdQuaternion_t q)
{
	HmdVector3_t u, result;
	u.v[0] = q.x;
	u.v[1] = q.y;
	u.v[2] = q.z;
	float s = q.w;

	// Dot products of u,v and u,u
	float uvDot = (u.v[0] * v.v[0] + u.v[1] * v.v[1] + u.v[2] * v.v[2]);
	float uuDot = (u.v[0] * u.v[0] + u.v[1] * u.v[1] + u.v[2] * u.v[2]);

	// Calculate cross product of u, v
	HmdVector3_t uvCross;
	uvCross.v[0] = u.v[1] * v.v[2] - u.v[2] * v.v[1];
	uvCross.v[1] = u.v[2] * v.v[0] - u.v[0] * v.v[2];
	uvCross.v[2] = u.v[0] * v.v[1] - u.v[1] * v.v[0];

	// Calculate each vectors' result individually because there aren't arthimetic functions for HmdVector3_t dsahfkldhsaklfhklsadh
	result.v[0] = u.v[0] * 2.0f * uvDot
		+ (s*s - uuDot) * v.v[0]
		+ 2.0f * s * uvCross.v[0];
	result.v[1] = u.v[1] * 2.0f * uvDot
		+ (s*s - uuDot) * v.v[1]
		+ 2.0f * s * uvCross.v[1];
	result.v[2] = u.v[2] * 2.0f * uvDot
		+ (s*s - uuDot) * v.v[2]
		+ 2.0f * s * uvCross.v[2];

	return result;
}

void QuatToYawPitchRoll(HmdQuaternion_t q, vec3_t out) {
	float sqw = q.w*q.w;
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	float unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
	float test = q.x*q.y + q.z*q.w;
	if (test > 0.499*unit) { // singularity at north pole
		out[YAW] = 2 * atan2(q.x, q.w) / M_PI_DIV_180;
		out[ROLL] = -M_PI / 2 / M_PI_DIV_180;
		out[PITCH] = 0;
	}
	else if (test < -0.499*unit) { // singularity at south pole
		out[YAW] = -2 * atan2(q.x, q.w) / M_PI_DIV_180;
		out[ROLL] = M_PI / 2 / M_PI_DIV_180;
		out[PITCH] = 0;
	}
	else {
		out[YAW] = atan2(2 * q.y*q.w - 2 * q.x*q.z, sqx - sqy - sqz + sqw) / M_PI_DIV_180;
		out[ROLL] = -asin(2 * test / unit) / M_PI_DIV_180;
		out[PITCH] = -atan2(2 * q.x*q.w - 2 * q.y*q.z, -sqx + sqy - sqz + sqw) / M_PI_DIV_180;
	}
}

// void VR_AddOrientationToViewAngles(vec3_t angles)
// {
// 	vec3_t orientation;
// 	QuatToYawPitchRoll(vr.eye[vr.current_eye]->orientation, orientation);
// 
// 	angles[PITCH] = angles[PITCH] + orientation[PITCH];
// 	angles[YAW] = angles[YAW] + orientation[YAW];
// 	angles[ROLL] = orientation[ROLL];
// }
