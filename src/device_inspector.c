/*
 * USB Tools - THCGaming: selected device inspector
 * Copyright © 2026 THCGaming
 *
 * This module intentionally reports "not available" when firmware or Windows
 * does not expose a property. It must never invent a device health value.
 */

#include <windows.h>
#include <winioctl.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "rufus.h"
#include "drive.h"
#include "dev.h"
#include "resource.h"
#include "msapi_utf8.h"
#include "darkmode.h"
#include "device_inspector.h"

#ifndef IOCTL_STORAGE_PREDICT_FAILURE
#define IOCTL_STORAGE_PREDICT_FAILURE CTL_CODE(IOCTL_STORAGE_BASE, 0x0440, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

typedef struct {
	ULONG PredictFailure;
	BYTE VendorSpecific[512];
} THCG_STORAGE_PREDICT_FAILURE;

extern RUFUS_DRIVE rufus_drive[MAX_DRIVES];
extern HINSTANCE hMainInstance;
extern HICON hBigIcon;

static int inspector_device_index = -1;
static THCG_DEVICE_INFO inspector_info;
static HICON inspector_device_icon = NULL;

static const char* THCG_BusName(int bus_type)
{
	/* STORAGE_BUS_TYPE numeric values, used numerically for MinGW compatibility. */
	switch (bus_type) {
	case 1: return "SCSI";
	case 2: return "ATAPI";
	case 3: return "ATA";
	case 4: return "IEEE 1394";
	case 6: return "Fibre Channel";
	case 7: return "USB";
	case 8: return "RAID";
	case 9: return "iSCSI";
	case 10: return "SAS";
	case 11: return "SATA";
	case 12: return "SD";
	case 13: return "MMC";
	case 14: return "Virtual";
	case 15: return "File-backed Virtual";
	case 16: return "Storage Spaces";
	case 17: return "NVMe";
	case 18: return "SCM";
	case 19: return "UFS";
	default: return "Không xác định";
	}
}

static const char* THCG_UsbSpeedName(uint32_t speed)
{
	switch (speed) {
	case USB_SPEED_LOW: return "USB Low-Speed";
	case USB_SPEED_FULL: return "USB Full-Speed";
	case USB_SPEED_HIGH: return "USB 2.0 High-Speed";
	case USB_SPEED_SUPER: return "USB 3.x SuperSpeed";
	case USB_SPEED_SUPER_PLUS: return "USB 3.x SuperSpeed+";
	default: return "Không xác định";
	}
}

static void THCG_CopyDescriptorString(char* dst, size_t dst_size, const BYTE* buffer,
	ULONG offset, DWORD returned)
{
	const char* src;
	size_t max_len;

	if ((dst == NULL) || (dst_size == 0))
		return;
	if ((offset == 0) || (offset >= returned))
		return;
	src = (const char*)&buffer[offset];
	max_len = returned - offset;
	if (memchr(src, 0, max_len) == NULL)
		return;
	safe_strcpy(dst, dst_size, src);
}

static void THCG_FormatDriveLetters(DWORD drive_index, char* dst, size_t dst_size)
{
	char letters[32] = { 0 };
	size_t i;

	if ((dst == NULL) || (dst_size == 0))
		return;
	dst[0] = 0;
	if (!GetDriveLetters(drive_index, letters) || letters[0] == 0) {
		safe_strcpy(dst, dst_size, "Không có ký tự ổ");
		return;
	}
	for (i = 0; letters[i] != 0; i++) {
		char item[8];
		safe_sprintf(item, sizeof(item), "%c:%s", letters[i], letters[i + 1] ? ", " : "");
		safe_strcat(dst, dst_size, item);
	}
}

static HICON THCG_LoadDeviceIcon(DWORD drive_index)
{
	char letters[32] = { 0 };
	char root[4] = "A:\\";
	SHFILEINFOA sfi;

	memset(&sfi, 0, sizeof(sfi));
	if (GetDriveLetters(drive_index, letters) && letters[0] != 0) {
		root[0] = letters[0];
		if (SHGetFileInfoA(root, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) != 0)
			return sfi.hIcon;
	}
	return hBigIcon;
}

static void THCG_QueryVolume(DWORD drive_index, THCG_DEVICE_INFO* info)
{
	char letters[32] = { 0 };
	char root[4] = "A:\\";
	char label[128] = { 0 };
	char fs[64] = { 0 };
	DWORD serial = 0, max_component = 0, flags = 0;

	safe_strcpy(info->filesystem, sizeof(info->filesystem), "Không xác định");
	safe_strcpy(info->volume_label, sizeof(info->volume_label), "Không có");
	if (!GetDriveLetters(drive_index, letters) || letters[0] == 0)
		return;

	root[0] = letters[0];
	if (GetVolumeInformationA(root, label, ARRAYSIZE(label), &serial, &max_component,
			&flags, fs, ARRAYSIZE(fs))) {
		if (label[0] != 0)
			safe_strcpy(info->volume_label, sizeof(info->volume_label), label);
		if (fs[0] != 0)
			safe_strcpy(info->filesystem, sizeof(info->filesystem), fs);
	}
}

static void THCG_QueryStorageDescriptor(HANDLE hPhysical, THCG_DEVICE_INFO* info)
{
	BYTE buffer[2048] = { 0 };
	STORAGE_PROPERTY_QUERY query;
	STORAGE_DEVICE_DESCRIPTOR* desc;
	DWORD returned = 0;

	memset(&query, 0, sizeof(query));
	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;
	if (!DeviceIoControl(hPhysical, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
			buffer, sizeof(buffer), &returned, NULL) || returned < sizeof(STORAGE_DEVICE_DESCRIPTOR))
		return;

	desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
	THCG_CopyDescriptorString(info->vendor, sizeof(info->vendor), buffer, desc->VendorIdOffset, returned);
	THCG_CopyDescriptorString(info->product, sizeof(info->product), buffer, desc->ProductIdOffset, returned);
	THCG_CopyDescriptorString(info->serial, sizeof(info->serial), buffer, desc->SerialNumberOffset, returned);
	THCG_CopyDescriptorString(info->firmware, sizeof(info->firmware), buffer, desc->ProductRevisionOffset, returned);
	safe_strcpy(info->bus, sizeof(info->bus), THCG_BusName(desc->BusType));
}

static void THCG_QueryHealth(HANDLE hPhysical, THCG_DEVICE_INFO* info)
{
	THCG_STORAGE_PREDICT_FAILURE prediction;
	DWORD returned = 0;

	memset(&prediction, 0, sizeof(prediction));
	info->health_available = FALSE;
	info->predict_failure = FALSE;
	safe_strcpy(info->health_source, sizeof(info->health_source), "Windows Storage Predict Failure");

	if (DeviceIoControl(hPhysical, IOCTL_STORAGE_PREDICT_FAILURE, NULL, 0,
			&prediction, sizeof(prediction), &returned, NULL) && returned >= sizeof(DWORD)) {
		info->health_available = TRUE;
		info->predict_failure = (prediction.PredictFailure != 0);
		if (info->predict_failure) {
			safe_strcpy(info->health, sizeof(info->health),
				"CẢNH BÁO - thiết bị dự báo lỗi phần cứng");
			safe_strcpy(info->assessment, sizeof(info->assessment),
				"Không nên ghi bộ cài vào thiết bị này trước khi sao lưu dữ liệu và kiểm tra chuyên sâu.");
		} else {
			safe_strcpy(info->health, sizeof(info->health),
				"Không phát hiện cảnh báo SMART");
			safe_strcpy(info->assessment, sizeof(info->assessment),
				"Chưa thấy cảnh báo phần cứng. Nên chạy kiểm tra bad blocks nếu thiết bị cũ hoặc không rõ nguồn gốc.");
		}
		return;
	}

	safe_strcpy(info->health, sizeof(info->health), "Không hỗ trợ / không đọc được SMART");
	safe_strcpy(info->assessment, sizeof(info->assessment),
		"Chưa đủ dữ liệu để kết luận độ ổn định. Có thể chạy kiểm tra bad blocks trước khi ghi.");
}

BOOL THCG_QueryDeviceInfo(int device_index, THCG_DEVICE_INFO* info)
{
	RUFUS_DRIVE* drive;
	HANDLE hPhysical = INVALID_HANDLE_VALUE;
	char path[64];

	if ((info == NULL) || (device_index < 0) || (device_index >= MAX_DRIVES))
		return FALSE;
	drive = &rufus_drive[device_index];
	if ((drive->size == 0) || (drive->index < DRIVE_INDEX_MIN))
		return FALSE;

	memset(info, 0, sizeof(*info));
	safe_strcpy(info->display_name, sizeof(info->display_name),
		(drive->name != NULL) ? drive->name : "Thiết bị lưu trữ");
	safe_strcpy(info->vendor, sizeof(info->vendor), "Không được cung cấp");
	safe_strcpy(info->product, sizeof(info->product), "Không được cung cấp");
	safe_strcpy(info->serial, sizeof(info->serial), "Không được cung cấp");
	safe_strcpy(info->firmware, sizeof(info->firmware), "Không được cung cấp");
	safe_strcpy(info->bus, sizeof(info->bus), drive->is_usb ? "USB" : "Không xác định");
	safe_strcpy(info->manufacture_year, sizeof(info->manufacture_year),
		"Không được firmware cung cấp");
	safe_strcpy(info->removable, sizeof(info->removable),
		drive->is_removable ? "Có" : "Không / Fixed media");
	safe_sprintf(info->capacity, sizeof(info->capacity), "%s",
		SizeToHumanReadable(drive->size, FALSE, FALSE));
	THCG_FormatDriveLetters(drive->index, info->drive_letters, sizeof(info->drive_letters));
	THCG_QueryVolume(drive->index, info);

	if ((drive->vid >= 0) && (drive->pid >= 0))
		safe_sprintf(info->usb_id, sizeof(info->usb_id), "%04X:%04X", drive->vid, drive->pid);
	else
		safe_strcpy(info->usb_id, sizeof(info->usb_id), "Không xác định");

	if (drive->is_uasp)
		safe_sprintf(info->usb_speed, sizeof(info->usb_speed), "%s (UASP)", THCG_UsbSpeedName(drive->usb_speed));
	else
		safe_strcpy(info->usb_speed, sizeof(info->usb_speed), THCG_UsbSpeedName(drive->usb_speed));

	switch (SelectedDrive.PartitionStyle) {
	case PARTITION_STYLE_MBR:
		safe_strcpy(info->partition_style, sizeof(info->partition_style), "MBR");
		break;
	case PARTITION_STYLE_GPT:
		safe_strcpy(info->partition_style, sizeof(info->partition_style), "GPT");
		break;
	default:
		safe_strcpy(info->partition_style, sizeof(info->partition_style), "RAW / Không xác định");
		break;
	}

	safe_sprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", drive->index - DRIVE_INDEX_MIN);
	hPhysical = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hPhysical != INVALID_HANDLE_VALUE) {
		THCG_QueryStorageDescriptor(hPhysical, info);
		THCG_QueryHealth(hPhysical, info);
		CloseHandle(hPhysical);
	} else {
		safe_strcpy(info->health, sizeof(info->health), "Không đọc được trạng thái thiết bị");
		safe_strcpy(info->health_source, sizeof(info->health_source), "Không có");
		safe_strcpy(info->assessment, sizeof(info->assessment),
			"Không đủ dữ liệu để đánh giá. Hãy thử chạy ứng dụng với quyền Administrator.");
	}

	return TRUE;
}

static void THCG_SetInfoText(HWND hDlg, int id, const char* text)
{
	SetDlgItemTextU(hDlg, id, (text != NULL && text[0] != 0) ? text : "—");
}

INT_PTR CALLBACK THCG_DeviceInfoCallback(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG:
		SetDarkModeForDlg(hDlg);
		SetWindowTextU(hDlg, "Thông tin thiết bị - USB Tools - THCGaming");
		CenterDialog(hDlg, NULL);
		inspector_device_icon = (inspector_device_index >= 0 && inspector_device_index < MAX_DRIVES) ?
			THCG_LoadDeviceIcon(rufus_drive[inspector_device_index].index) : hBigIcon;
		SendDlgItemMessage(hDlg, IDC_DI_ICON, STM_SETICON, (WPARAM)inspector_device_icon, 0);

		SetDlgItemTextU(hDlg, IDC_DI_TITLE_LABEL, "Thiết bị đang chọn");
		SetDlgItemTextU(hDlg, IDC_DI_VENDOR_LABEL, "Nhà sản xuất:");
		SetDlgItemTextU(hDlg, IDC_DI_PRODUCT_LABEL, "Model / tên thiết bị:");
		SetDlgItemTextU(hDlg, IDC_DI_SERIAL_LABEL, "Serial:");
		SetDlgItemTextU(hDlg, IDC_DI_FIRMWARE_LABEL, "Firmware:");
		SetDlgItemTextU(hDlg, IDC_DI_USBID_LABEL, "USB VID:PID:");
		SetDlgItemTextU(hDlg, IDC_DI_BUS_LABEL, "Kết nối:");
		SetDlgItemTextU(hDlg, IDC_DI_SPEED_LABEL, "Tốc độ:");
		SetDlgItemTextU(hDlg, IDC_DI_CAPACITY_LABEL, "Dung lượng:");
		SetDlgItemTextU(hDlg, IDC_DI_DRIVE_LABEL, "Ký tự ổ:");
		SetDlgItemTextU(hDlg, IDC_DI_VOLUME_LABEL, "Tên volume:");
		SetDlgItemTextU(hDlg, IDC_DI_FS_LABEL, "Định dạng:");
		SetDlgItemTextU(hDlg, IDC_DI_PARTITION_LABEL, "Partition:");
		SetDlgItemTextU(hDlg, IDC_DI_REMOVABLE_LABEL, "Removable:");
		SetDlgItemTextU(hDlg, IDC_DI_YEAR_LABEL, "Năm sản xuất:");
		SetDlgItemTextU(hDlg, IDC_DI_HEALTH_LABEL, "Sức khỏe:");
		SetDlgItemTextU(hDlg, IDC_DI_ASSESSMENT_LABEL, "Đánh giá trước khi ghi:");
		SetDlgItemTextU(hDlg, IDOK, "Đóng");

		if (!THCG_QueryDeviceInfo(inspector_device_index, &inspector_info)) {
			SetDlgItemTextU(hDlg, IDC_DI_NAME, "Không có thiết bị hợp lệ được chọn.");
			break;
		}
		THCG_SetInfoText(hDlg, IDC_DI_NAME, inspector_info.display_name);
		THCG_SetInfoText(hDlg, IDC_DI_VENDOR, inspector_info.vendor);
		THCG_SetInfoText(hDlg, IDC_DI_PRODUCT, inspector_info.product);
		THCG_SetInfoText(hDlg, IDC_DI_SERIAL, inspector_info.serial);
		THCG_SetInfoText(hDlg, IDC_DI_FIRMWARE, inspector_info.firmware);
		THCG_SetInfoText(hDlg, IDC_DI_USBID, inspector_info.usb_id);
		THCG_SetInfoText(hDlg, IDC_DI_BUS, inspector_info.bus);
		THCG_SetInfoText(hDlg, IDC_DI_SPEED, inspector_info.usb_speed);
		THCG_SetInfoText(hDlg, IDC_DI_CAPACITY, inspector_info.capacity);
		THCG_SetInfoText(hDlg, IDC_DI_DRIVE, inspector_info.drive_letters);
		THCG_SetInfoText(hDlg, IDC_DI_VOLUME, inspector_info.volume_label);
		THCG_SetInfoText(hDlg, IDC_DI_FS, inspector_info.filesystem);
		THCG_SetInfoText(hDlg, IDC_DI_PARTITION, inspector_info.partition_style);
		THCG_SetInfoText(hDlg, IDC_DI_REMOVABLE, inspector_info.removable);
		THCG_SetInfoText(hDlg, IDC_DI_YEAR, inspector_info.manufacture_year);
		THCG_SetInfoText(hDlg, IDC_DI_HEALTH, inspector_info.health);
		THCG_SetInfoText(hDlg, IDC_DI_ASSESSMENT, inspector_info.assessment);
		SetDarkModeForChild(hDlg);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL)) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	case WM_DESTROY:
		if ((inspector_device_icon != NULL) && (inspector_device_icon != hBigIcon))
			DestroyIcon(inspector_device_icon);
		inspector_device_icon = NULL;
		break;
	}
	return (INT_PTR)FALSE;
}

void THCG_ShowDeviceInspector(HWND hParent, int device_index)
{
	inspector_device_index = device_index;
	MyDialogBox(hMainInstance, IDD_DEVICE_INFO, hParent, THCG_DeviceInfoCallback);
}
