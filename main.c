#include <stdint.h>
#include <stddef.h>

/* --- Minimal UEFI Definitions --- */
typedef uint64_t UINTN;
typedef uint64_t EFI_STATUS;
typedef void    *EFI_HANDLE;
typedef uint16_t CHAR16;

#define EFI_SUCCESS 0

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (*OutputString)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, CHAR16 *String);
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
        uint32_t *DescriptorVersion
    );
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

/* --- PL011 UART Driver (QEMU virt @ 0x09000000) --- */
#define UART0_BASE 0x09000000UL
#define UART_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_FR_TXFF (1U << 5) /* Transmit FIFO full */
#define UART_FR_RXFE (1U << 4) /* Receive FIFO empty */

static void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF) {}
    UART_DR = (uint32_t)c;
}

static void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static char uart_getc(void) {
    while (UART_FR & UART_FR_RXFE) {}
    return (char)(UART_DR & 0xFF);
}

/* --- UEFI Entry Point --- */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, u"Hello from TinyOS UEFI!\r\n");

    /* 1. Fetch the memory map and exit UEFI Boot Services */
    static uint8_t mmap_buf[16384];
    UINTN mmap_size = sizeof(mmap_buf);
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_ver = 0;

    EFI_STATUS status = SystemTable->BootServices->GetMemoryMap(
        &mmap_size,
        (EFI_MEMORY_DESCRIPTOR *)mmap_buf,
        &map_key,
        &desc_size,
        &desc_ver
    );

    if (status == EFI_SUCCESS) {
        status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
    }

    if (status != EFI_SUCCESS) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, u"ExitBootServices failed!\r\n");
        while (1) {}
    }

    /* 2. UEFI Boot Services are now gone; our OS owns the hardware! */
    uart_puts("\n[TinyOS Kernel] Took control from UEFI.\n");
    uart_puts("PL011 UART active. Type something to echo:\n> ");

    while (1) {
        char c = uart_getc();
        if (c == '\r') {
            uart_puts("\n> ");
        } else {
            uart_putc(c);
        }
    }
}
