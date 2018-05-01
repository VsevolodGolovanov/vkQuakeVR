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

#define MAX_VR_TRACKED_DEVICE_POSES	16
#define VR_EYE_LEFT		0
#define VR_EYE_RIGHT	1
#define NUM_VR_EYES		2

typedef struct
{
	EVREye					vreye;
	Texture_t				texture;
	VRVulkanTextureData_t	texture_data;
	HiddenAreaMesh_t		hidden_area_mesh;
	float					eye_to_head_transform[16];
	float					projection[16];
} vreye_t;

typedef struct {
	vec3_t position;
	vec3_t orientation;
	HmdVector3_t rawvector;
	HmdQuaternion_t raworientation;
} vr_controller;

typedef struct
{
	TrackedDevicePose_t pose[MAX_VR_TRACKED_DEVICE_POSES];
	vreye_t			eye[NUM_VR_EYES];
	vr_controller	controllers[2];
	uint32_t		current_eye;
	uint32_t		width;
	uint32_t		height;
	float			fov_x;
	float			fov_y;
} vrdef_t;

extern	vrdef_t vr;				// global vr state

void		VR_Init (void);
void		VR_Shutdown (void);

void		VR_ProcessEvents();
void		VR_InputMove(usercmd_t *cmd);
void		VR_GetPosition(TrackedDevicePose_t pose, float *pos_x, float *pos_y, float *pos_z);
TrackedDevicePose_t* VR_GetControllerPosition(ETrackedControllerRole role, float *pos_x, float *pos_y, float *pos_z);
void		VR_GetVelocity(TrackedDevicePose_t pose, float *x, float *y, float *z);
void		VR_GetOrientation(TrackedDevicePose_t pose, float *pitch, float *yaw, float *roll);
void		VR_UpdatePose(void);
void		VR_Submit(uint32_t eye, VkImage color_buffer);
void		VR_DrawHiddenAreaMesh(void);
uint32_t	VR_GetVulkanInstanceExtensionsRequired(char *extension_names, uint32_t buffer_size);
uint32_t	VR_GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *physical_device, char *extension_names, uint32_t buffer_size);

S_API intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
S_API void VR_ShutdownInternal();
S_API bool VR_IsHmdPresent();
S_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
S_API bool VR_IsRuntimeInstalled();
S_API const char * VR_GetVRInitErrorAsSymbol(EVRInitError error);
S_API const char * VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);
S_API uint32_t VR_GetInitToken();
S_API bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);

HmdVector3_t Matrix34ToVector(HmdMatrix34_t in);
HmdQuaternion_t Matrix34ToQuaternion(HmdMatrix34_t in);
void AnglesToQuat(const vec3_t angles, vec4_t quat);
HmdQuaternion_t AnglesToHmdQuat(const vec3_t angles);
HmdVector3_t RotateVectorByQuaternion(HmdVector3_t v, HmdQuaternion_t q);
void QuatToYawPitchRoll(HmdQuaternion_t q, vec3_t out);

extern cvar_t vr_gunangle;

#endif	/* __VR_DEFS_H */

