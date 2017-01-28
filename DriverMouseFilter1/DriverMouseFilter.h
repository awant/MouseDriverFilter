#ifndef MOUFILTER_H
#define MOUFILTER_H

#include "ntddk.h"
#include <ntddmou.h>
#include <stdio.h>

#if DBG
#define DebugPrint(Text) DbgPrint Text
#else
#define DebugPrint(Text)
#endif

#define ArraySize(Array) (sizeof(Array)/sizeof((Array)[0]))

typedef struct _CONNECT_DATA {
	IN PDEVICE_OBJECT ClassDeviceObject;
	IN PVOID ClassService;
} CONNECT_DATA, *PCONNECT_DATA;

#define IOCTL_INTERNAL_MOUSE_CONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0080, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_INTERNAL_MOUSE_DISCONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0100, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef
VOID
(*PSERVICE_CALLBACK_ROUTINE) (
IN PVOID NormalContext,
IN PVOID SystemArgument1,
IN PVOID SystemArgument2,
IN OUT PVOID SystemArgument3
);

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT  ClassDeviceObject;
	PDEVICE_OBJECT  LowerDeviceObject;
	LONG CreateCounter;
	CONNECT_DATA ConnectData;
	BOOLEAN IsInverted;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING);
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT BusDeviceObject);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp);

VOID MouFilter_ServiceCallback(PDEVICE_OBJECT DeviceObject,
	                           PMOUSE_INPUT_DATA InputDataStart,
							   PMOUSE_INPUT_DATA InputDataEnd,
							   PULONG InputDataConsumed);

NTSTATUS DeviceCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
BOOLEAN WheelWasPressed(ULONG ButtonFlags);

#endif