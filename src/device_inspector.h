/*
 * USB Tools - THCGaming: selected device inspector
 * Copyright © 2026 THCGaming
 *
 * This file is part of a GPLv3-or-later work derived from Rufus.
 */
#pragma once

#include <windows.h>

typedef struct {
	char display_name[256];
	char volume_label[128];
	char vendor[128];
	char product[128];
	char serial[128];
	char firmware[64];
	char bus[64];
	char usb_id[32];
	char usb_speed[64];
	char capacity[64];
	char filesystem[64];
	char partition_style[32];
	char drive_letters[64];
	char removable[32];
	char manufacture_year[64];
	char health[128];
	char health_source[64];
	char assessment[256];
	BOOL health_available;
	BOOL predict_failure;
} THCG_DEVICE_INFO;

BOOL THCG_QueryDeviceInfo(int device_index, THCG_DEVICE_INFO* info);
INT_PTR CALLBACK THCG_DeviceInfoCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void THCG_ShowDeviceInspector(HWND hParent, int device_index);
