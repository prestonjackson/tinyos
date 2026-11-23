#ifndef TINYOS_UEFI_H
#define TINYOS_UEFI_H

#include <stdint.h>

/*
 * Minimal UEFI Specification Definitions
 * --------------------------------------
 * Why define these ~130 lines ourselves instead of using EDK2 or gnu-efi?
 *   EDK2 is millions of lines of build machinery, and gnu-efi adds ELF-to-PE
 *   wrapper hacks. Because UEFI is just a binary ABI of C function-pointer
 *   tables, defining only the structs we actually call keeps the OS tiny,
 *   self-contained, and buildable with nothing but `clang` + `lld-link`.
 *
 * Why is UINTN `uint64_t` instead of `unsigned long`?
 *   `aarch64-unknown-windows` uses the LLP64 data model where `long` is 32-bit
 *   even on 64-bit CPUs. Using `uint64_t` prevents silent 32-bit truncation of
 *   UEFI status codes, pointers, and physical addresses.
 */
typedef uint64_t UINTN;
typedef uint64_t EFI_STATUS;
typedef void    *EFI_HANDLE;
typedef uint16_t CHAR16; /* UEFI strings are UTF-16LE (`u"..."` in C11/C23) */

#define EFI_SUCCESS 0

typedef struct {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t  Data4[8];
} EFI_GUID;

typedef struct {
  uint64_t Signature;
  uint32_t Revision;
  uint32_t HeaderSize;
  uint32_t CRC32;
  uint32_t Reserved;
} EFI_TABLE_HEADER;

/* Console text output (used only before ExitBootServices) */
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  void *Reset;
  EFI_STATUS (*OutputString)(
      struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
  void *TestString;
  void *QueryMode;
  void *SetMode;
  void *SetAttribute;
  EFI_STATUS (*ClearScreen)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
  uint32_t Type;
  uint64_t PhysicalStart;
  uint64_t VirtualStart;
  uint64_t NumberOfPages;
  uint64_t Attribute;
} EFI_MEMORY_DESCRIPTOR;

/*
 * Graphics Output Protocol (GOP)
 * Why GOP instead of legacy VGA (0xB8000)?
 *   ARM64 machines and modern cloud VMs have no legacy 0xB8000 VGA text
 *   hardware. Instead, UEFI's GOP provides a linear pixel framebuffer in RAM
 *   that remains writable after ExitBootServices() (with `-device ramfb`).
 */
typedef enum {
  PixelRedGreenBlueReserved8BitPerColor = 0,
  PixelBlueGreenRedReserved8BitPerColor = 1,
  PixelBitMask = 2,
  PixelBltOnly = 3,
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
  uint32_t RedMask;
  uint32_t GreenMask;
  uint32_t BlueMask;
  uint32_t ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
  uint32_t                  Version;
  uint32_t                  HorizontalResolution;
  uint32_t                  VerticalResolution;
  EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
  EFI_PIXEL_BITMASK         PixelInformation;
  uint32_t                  PixelsPerScanLine; /* Stride (may exceed width) */
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
  uint32_t                              MaxMode;
  uint32_t                              Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  UINTN                                 SizeOfInfo;
  uint64_t                              FrameBufferBase;
  UINTN                                 FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
  void *QueryMode;
  void *SetMode;
  void *Blt;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
  { 0x9042a9de, 0x23dc, 0x4a38,           \
    { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

/*
 * EFI_BOOT_SERVICES table: field order must match the UEFI Spec exactly
 * because C accesses fields by byte offset. Unused entries are `void *`.
 */
typedef struct {
  EFI_TABLE_HEADER Hdr;
  void *RaiseTPL;
  void *RestoreTPL;
  void *AllocatePages;
  void *FreePages;
  EFI_STATUS (*GetMemoryMap)(
      UINTN *MemoryMapSize,
      EFI_MEMORY_DESCRIPTOR *MemoryMap,
      UINTN *MapKey,
      UINTN *DescriptorSize,
      uint32_t *DescriptorVersion);
  void *AllocatePool;
  void *FreePool;
  void *CreateEvent;
  void *SetTimer;
  void *WaitForEvent;
  void *SignalEvent;
  void *CloseEvent;
  void *CheckEvent;
  void *InstallProtocolInterface;
  void *ReinstallProtocolInterface;
  void *UninstallProtocolInterface;
  void *HandleProtocol;
  void *Reserved;
  void *RegisterProtocolNotify;
  void *LocateHandle;
  void *LocateDevicePath;
  void *InstallConfigurationTable;
  void *LoadImage;
  void *StartImage;
  void *Exit;
  void *UnloadImage;
  EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
  void *GetNextMonotonicCount;
  void *Stall;
  EFI_STATUS (*SetWatchdogTimer)(
      UINTN Timeout, uint64_t WatchdogCode,
      UINTN DataSize, CHAR16 *WatchdogData);
  void *ConnectController;
  void *DisconnectController;
  void *OpenProtocol;
  void *CloseProtocol;
  void *OpenProtocolInformation;
  void *ProtocolsPerHandle;
  void *LocateHandleBuffer;
  EFI_STATUS (*LocateProtocol)(
      EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
  EFI_TABLE_HEADER Hdr;
  CHAR16 *FirmwareVendor;
  uint32_t FirmwareRevision;
  EFI_HANDLE ConsoleInHandle;
  void *ConIn;
  EFI_HANDLE ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
  EFI_HANDLE StandardErrorHandle;
  void *StdErr;
  void *RuntimeServices;
  EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

#endif /* TINYOS_UEFI_H */
