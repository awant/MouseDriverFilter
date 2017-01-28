#include "DriverMouseFilter.h"

static char* IrpName[] =
{ "IRP_MJ_CREATE",
"IRP_MJ_CREATE_NAMED_PIPE",
"IRP_MJ_CLOSE",
"IRP_MJ_READ",
"IRP_MJ_WRITE",
"IRP_MJ_QUERY_INFORMATION",
"IRP_MJ_SET_INFORMATION",
"IRP_MJ_QUERY_EA",
"IRP_MJ_SET_EA",
"IRP_MJ_FLUSH_BUFFERS",
"IRP_MJ_QUERY_VOLUME_INFORMATION",
"IRP_MJ_SET_VOLUME_INFORMATION",
"IRP_MJ_DIRECTORY_CONTROL",
"IRP_MJ_FILE_SYSTEM_CONTROL",
"IRP_MJ_DEVICE_CONTROL",
"IRP_MJ_INTERNAL_DEVICE_CONTROL",
"IRP_MJ_SHUTDOWN",
"IRP_MJ_LOCK_CONTROL",
"IRP_MJ_CLEANUP",
"IRP_MJ_CREATE_MAILSLOT",
"IRP_MJ_QUERY_SECURITY",
"IRP_MJ_SET_SECURITY",
"IRP_MJ_POWER",
"IRP_MJ_SYSTEM_CONTROL",
"IRP_MJ_DEVICE_CHANGE",
"IRP_MJ_QUERY_QUOTA",
"IRP_MJ_SET_QUOTA",
"IRP_MJ_PNP" };

static char* PnpName[] =
{ "IRP_MN_START_DEVICE",
"IRP_MN_QUERY_REMOVE_DEVICE",
"IRP_MN_REMOVE_DEVICE",
"IRP_MN_CANCEL_REMOVE_DEVICE",
"IRP_MN_STOP_DEVICE",
"IRP_MN_QUERY_STOP_DEVICE",
"IRP_MN_CANCEL_STOP_DEVICE",
"IRP_MN_QUERY_DEVICE_RELATIONS",
"IRP_MN_QUERY_INTERFACE",
"IRP_MN_QUERY_CAPABILITIES",
"IRP_MN_QUERY_RESOURCES",
"IRP_MN_QUERY_RESOURCE_REQUIREMENTS",
"IRP_MN_QUERY_DEVICE_TEXT",
"IRP_MN_FILTER_RESOURCE_REQUIREMENTS",
"",
"IRP_MN_READ_CONFIG",
"IRP_MN_WRITE_CONFIG",
"IRP_MN_EJECT",
"IRP_MN_SET_LOCK",
"IRP_MN_QUERY_ID",
"IRP_MN_QUERY_PNP_DEVICE_STATE",
"IRP_MN_QUERY_BUS_INFORMATION",
"IRP_MN_DEVICE_USAGE_NOTIFICATION",
"IRP_MN_SURPRISE_REMOVAL",
"IRP_MN_QUERY_LEGACY_BUS_INFORMATION" };

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	DbgPrint(("Mouse Filter: DriverEntry\n"));

	DriverObject->DriverUnload = DriverUnload;
	DriverObject->DriverExtension->AddDevice = AddDevice;

	ULONG i;
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) {
		DriverObject->MajorFunction[i] = DispatchDefault;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = DispatchInternalDeviceControl;

	return STATUS_SUCCESS;
}

NTSTATUS AddDevice(PDRIVER_OBJECT Driver, PDEVICE_OBJECT PDO) {
	PDEVICE_EXTENSION        DeviceExtension;
	IO_ERROR_LOG_PACKET      errorLogEntry;
	PDEVICE_OBJECT           device;
	NTSTATUS                 status = STATUS_SUCCESS;

	PAGED_CODE();

	DbgPrint(("Mouse Filter: AddDevice\n"));

	status = IoCreateDevice(Driver,
		sizeof(DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_MOUSE,
		0,
		FALSE,
		&device
		);

	if (!NT_SUCCESS(status)) {
		return (status);
	}

	RtlZeroMemory(device->DeviceExtension, sizeof(DEVICE_EXTENSION));

	DeviceExtension = (PDEVICE_EXTENSION)device->DeviceExtension;
	DeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(device, PDO);
	if (DeviceExtension->LowerDeviceObject == NULL) {
		IoDeleteDevice(device);
		return STATUS_DEVICE_NOT_CONNECTED;
	}

	ASSERT(DeviceExtension->LowerDeviceObject);

	DeviceExtension->ClassDeviceObject = device;

	device->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	device->Flags &= ~DO_DEVICE_INITIALIZING;

	DeviceExtension->IsInverted = FALSE;

	return status;
}

NTSTATUS DeviceCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
	PKEVENT LockEvent = (PKEVENT)Context;

	DbgPrint(("Mouse Filter: DeviceCompletionRoutine\n"));

	KeSetEvent(LockEvent, 0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION  Stack;
	NTSTATUS            status;
	PDEVICE_EXTENSION   DeviceExtension;

	PAGED_CODE();

	DbgPrint(("Mouse Filter: DispatchCreateClose\n"));

	Stack = IoGetCurrentIrpStackLocation(Irp);
	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	status = Irp->IoStatus.Status;

	switch (Stack->MajorFunction) {
	case IRP_MJ_CREATE:
		if (NULL == DeviceExtension->ConnectData.ClassService) {
			status = STATUS_INVALID_DEVICE_STATE;
		}
		else if (InterlockedIncrement(&DeviceExtension->CreateCounter) >= 1) {
			// created
		}
		break;

	case IRP_MJ_CLOSE:
		if (0 >= InterlockedDecrement(&DeviceExtension->CreateCounter)) {
			// closed
		}
		break;
	}

	Irp->IoStatus.Status = status;

	return DispatchDefault(DeviceObject, Irp);
}

NTSTATUS DispatchDefault(PDEVICE_OBJECT DeviceObject, PIRP Irp) {

	UCHAR MajorFunction = IoGetCurrentIrpStackLocation(Irp)->MajorFunction;
	if (MajorFunction >= ArraySize(IrpName))
		DebugPrint(("Mouse Filter: DispatchDefault - Unknown IRP, major type %X\n", MajorFunction));
	else
		DebugPrint(("Mouse Filter: DispatchDefault - %s\n", IrpName[MajorFunction]));

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
}

NTSTATUS DispatchInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION          Stack;
	PDEVICE_EXTENSION           DeviceExtension;
	KEVENT                      LockEvent;
	PCONNECT_DATA               connectData;
	NTSTATUS                    status = STATUS_SUCCESS;

	DbgPrint(("Mouse Filter: DispatchInternalDeviceControl\n"));

	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	Irp->IoStatus.Information = 0;
	Stack = IoGetCurrentIrpStackLocation(Irp);

	switch (Stack->Parameters.DeviceIoControl.IoControlCode) {
	case IOCTL_INTERNAL_MOUSE_CONNECT:
		if (DeviceExtension->ConnectData.ClassService != NULL) {
			status = STATUS_SHARING_VIOLATION;
			break;
		}
		else if (Stack->Parameters.DeviceIoControl.InputBufferLength <
			sizeof(CONNECT_DATA)) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		connectData = ((PCONNECT_DATA)(Stack->Parameters.DeviceIoControl.Type3InputBuffer));
		DeviceExtension->ConnectData = *connectData;
		connectData->ClassDeviceObject = DeviceExtension->ClassDeviceObject;
		connectData->ClassService = MouFilter_ServiceCallback;

		break;

	case IOCTL_INTERNAL_MOUSE_DISCONNECT:
		status = STATUS_NOT_IMPLEMENTED;
		break;

	default:
		break;
	}

	if (!NT_SUCCESS(status)) {
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	return DispatchDefault(DeviceObject, Irp);
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PDEVICE_EXTENSION           DeviceExtension;
	PIO_STACK_LOCATION          Stack;
	NTSTATUS                    status = STATUS_SUCCESS;
	KIRQL                       oldIrql;
	KEVENT                      LockEvent;

	PAGED_CODE();

	DbgPrint(("Mouse Filter: DispatchPnp\n"));

	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	Stack = IoGetCurrentIrpStackLocation(Irp);

	switch (Stack->MinorFunction) {
	case IRP_MN_START_DEVICE: {
		IoCopyCurrentIrpStackLocationToNext(Irp);
		KeInitializeEvent(&LockEvent, NotificationEvent, FALSE);

		IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE)DeviceCompletionRoutine,
			&LockEvent, TRUE, TRUE, TRUE);

		status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);

		if (STATUS_PENDING == status) {
			KeWaitForSingleObject(
				&LockEvent,
				Executive,
				KernelMode,
				FALSE,
				NULL);
		}

		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		break;
	}

	case IRP_MN_SURPRISE_REMOVAL:
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
		break;

	case IRP_MN_REMOVE_DEVICE:
		Irp->IoStatus.Status = STATUS_SUCCESS;

		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);

		IoDetachDevice(DeviceExtension->LowerDeviceObject);
		IoDeleteDevice(DeviceObject);

		break;

	default:
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
		break;
	}

	return status;
}

NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	PIO_STACK_LOCATION  Stack;
	PDEVICE_EXTENSION   DeviceExtension;
	POWER_STATE         powerState;
	POWER_STATE_TYPE    powerType;

	PAGED_CODE();

	DbgPrint(("Mouse Filter: DispatchPower\n"));

	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	Stack = IoGetCurrentIrpStackLocation(Irp);

	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
}

VOID MouFilter_ServiceCallback(PDEVICE_OBJECT DeviceObject,
                               PMOUSE_INPUT_DATA InputDataStart,
                               PMOUSE_INPUT_DATA InputDataEnd,
                               PULONG InputDataConsumed) {
	PDEVICE_EXTENSION DeviceExtension;
	PMOUSE_INPUT_DATA pCursor;
	LONG temp;

	DeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	for (pCursor = InputDataStart; pCursor < InputDataEnd; ++pCursor) {
		DbgPrint("Mouse Filter: MouFilter_ServiceCallback()  %hu\n", pCursor->Buttons);
		if (WheelWasPressed(pCursor->Buttons)) {
			DeviceExtension->IsInverted = !DeviceExtension->IsInverted;
		} else if (DeviceExtension->IsInverted) {
			pCursor->LastY = -pCursor->LastY;
		}
	}

	(*(PSERVICE_CALLBACK_ROUTINE)DeviceExtension->ConnectData.ClassService)(
		DeviceExtension->ConnectData.ClassDeviceObject,
		InputDataStart,
		InputDataEnd,
		InputDataConsumed
		);
}

VOID DriverUnload(PDRIVER_OBJECT Driver) {
	DbgPrint(("Mouse Filter: DriverUnload\n"));
	PAGED_CODE();
}

BOOLEAN WheelWasPressed(ULONG Buttons) {
	DbgPrint(("Mouse Filter: WheelWasPressed\n"));
	return (Buttons == MOUSE_MIDDLE_BUTTON_DOWN);
}
