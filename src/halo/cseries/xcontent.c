/* XapiFormatFATVolume (0x1D8368) - XAPILIB:xcontent.obj
 *
 * Formats a raw block device as a FATX volume. Called by XMountUtilityDrive
 * (0x1D3C95) when the utility partition is absent or corrupt.
 *
 * Layout written by this function:
 *   [0, min_cluster_size)                  - FATX superblock at sector 0,
 *                                            zeroed sectors after it
 *   [min_cluster_size, +fat_size)          - FAT table (FAT16 or FAT32)
 *   [min_cluster_size+fat_size, +cluster_size) - root-dir cluster (0xFF-filled)
 *
 * FAT type: FAT16 if cluster_count <= 0xFFEF, FAT32 otherwise.
 * cluster_size     = max(0x4000, page_aligned(BytesPerSector))
 * min_cluster_size = max(0x1000, page_aligned(BytesPerSector))
 *
 * Returns 1 on success, 0 on failure (Win32 or NT last error set).
 *
 * Confirmed: __stdcall, RET 0x4, one pointer argument (ANSI_STRING*).
 * IOCTL 0x70000 = IOCTL_DISK_GET_DRIVE_GEOMETRY -> 24-byte DISK_GEOMETRY.
 * IOCTL 0x74004 = IOCTL_DISK_GET_PARTITION_INFO -> 32-byte output.
 */

/* Do NOT include xbox.h here -- xboxkrnl.h defines XBAPI globals without
 * extern, causing duplicate symbol errors when linked with the kernel import
 * library.  Declare only what this TU needs, with natural alignment. */
#include "common.h"

/* Natural alignment for NT structs (common.h sets pack(1) globally). */
#pragma pack(push)
#pragma pack()

typedef long          NTSTATUS;
typedef unsigned long ULONG;
typedef void         *HANDLE;

typedef struct { unsigned long LowPart; long HighPart; } XAPI_LARGE_INTEGER;

typedef struct {
  unsigned short Length;
  unsigned short MaximumLength;
  char          *Buffer;
} XAPI_ANSI_STRING;

typedef struct {
  HANDLE            RootDirectory;
  XAPI_ANSI_STRING *ObjectName;
  ULONG             Attributes;
} XAPI_OBJECT_ATTRIBUTES;

typedef struct {
  NTSTATUS Status;
  ULONG   *Information;
} XAPI_IO_STATUS_BLOCK;

/* DISK_GEOMETRY as returned by IOCTL 0x70000 (24 bytes).
 * BytesPerSector is at offset 20 -- the field passed to FUN_001d8750. */
typedef struct {
  ULONG cylinders_lo;
  ULONG cylinders_hi;
  ULONG media_type;
  ULONG tracks_per_cylinder;
  ULONG sectors_per_track;
  ULONG bytes_per_sector; /* offset 20 */
} XAPI_DISK_GEOMETRY;

/* Subset of PARTITION_INFORMATION as returned by IOCTL 0x74004 (32 bytes).
 * PartitionLength (LARGE_INTEGER) is at offset 8. */
typedef struct {
  ULONG starting_offset_lo;
  ULONG starting_offset_hi;
  ULONG partition_length_lo; /* offset 8 */
  ULONG partition_length_hi; /* offset 12 */
  ULONG hidden_sectors;
  ULONG partition_number;
  ULONG partition_type_flags;
  ULONG reserved;
} XAPI_PARTITION_INFO;

#pragma pack(pop)

#define OBJ_CASE_INSENSITIVE 0x00000040UL

/* Xbox NT kernel functions -- resolved via xboxkrnl.exe import library. */
extern NTSTATUS __stdcall NtOpenFile(
    HANDLE *FileHandle, ULONG DesiredAccess,
    XAPI_OBJECT_ATTRIBUTES *ObjectAttributes,
    XAPI_IO_STATUS_BLOCK *IoStatusBlock,
    ULONG ShareAccess, ULONG OpenOptions);

extern NTSTATUS __stdcall NtDeviceIoControlFile(
    HANDLE FileHandle, HANDLE Event,
    void *ApcRoutine, void *ApcContext,
    XAPI_IO_STATUS_BLOCK *IoStatusBlock, ULONG IoControlCode,
    void *InputBuffer, ULONG InputBufferLength,
    void *OutputBuffer, ULONG OutputBufferLength);

extern NTSTATUS __stdcall NtFsControlFile(
    HANDLE FileHandle, HANDLE Event,
    void *ApcRoutine, void *ApcContext,
    XAPI_IO_STATUS_BLOCK *IoStatusBlock, ULONG FsControlCode,
    void *InputBuffer, ULONG InputBufferLength,
    void *OutputBuffer, ULONG OutputBufferLength);

extern NTSTATUS __stdcall NtWriteFile(
    HANDLE FileHandle, HANDLE Event,
    void *ApcRoutine, void *ApcContext,
    XAPI_IO_STATUS_BLOCK *IoStatusBlock,
    void *Buffer, ULONG Length,
    XAPI_LARGE_INTEGER *ByteOffset);

extern void __stdcall NtClose(HANDLE Handle);

extern void __stdcall KeQuerySystemTime(XAPI_LARGE_INTEGER *CurrentTime);

/* FUN_001d0bb9, FUN_001d0c16, FUN_001d8750, XapiSetLastNTError, SetLastError
 * are all declared in the generated decl.h (via common.h / kb.json). */

#define FATX_MAGIC 0x58544146UL

int __stdcall XapiFormatFATVolume(void *device_path)
{
  XAPI_ANSI_STRING       *ansi_path = (XAPI_ANSI_STRING *)device_path;
  XAPI_OBJECT_ATTRIBUTES  oa;
  XAPI_IO_STATUS_BLOCK    iosb;
  HANDLE                  hdev;
  NTSTATUS                status;

  XAPI_DISK_GEOMETRY  geom;
  XAPI_PARTITION_INFO part;

  ULONG bytes_per_sector;
  ULONG sector_shift;
  ULONG sector_size_aligned;
  ULONG cluster_size;
  ULONG min_cluster_size;
  ULONG partition_lo;
  ULONG partition_hi;
  ULONG cluster_count;
  ULONG fat_raw;
  ULONG fat_size;
  ULONG *buf;
  ULONG *p;
  ULONG i;
  ULONG off_lo;
  ULONG off_hi;
  ULONG remaining;
  XAPI_LARGE_INTEGER timestamp;
  NTSTATUS write_status;

  oa.RootDirectory = 0;
  oa.ObjectName    = ansi_path;
  oa.Attributes    = OBJ_CASE_INSENSITIVE;

  status = NtOpenFile(&hdev, 0x100003, &oa, &iosb, 0, 0x18);
  if (status < 0) {
    XapiSetLastNTError(status);
    return 0;
  }

  /* Query disk geometry: BytesPerSector at offset 20. */
  status = NtDeviceIoControlFile(hdev, 0, 0, 0, &iosb,
                                 0x70000, 0, 0, &geom, sizeof(geom));
  if (status < 0) {
    NtClose(hdev);
    goto fail;
  }

  bytes_per_sector = geom.bytes_per_sector;
  sector_shift     = FUN_001d8750(bytes_per_sector) & 0xff;

  /* Query partition info: PartitionLength at offset 8. */
  status = NtDeviceIoControlFile(hdev, 0, 0, 0, &iosb,
                                 0x74004, 0, 0, &part, sizeof(part));
  if (status < 0) {
    NtClose(hdev);
    goto fail;
  }

  partition_lo = part.partition_length_lo;
  partition_hi = part.partition_length_hi;

  sector_size_aligned = (bytes_per_sector + 0xfffU) & 0xfffff000U;
  cluster_size     = (sector_size_aligned > 0x4000U) ? sector_size_aligned : 0x4000U;
  min_cluster_size = (sector_size_aligned > 0x1000U) ? sector_size_aligned : 0x1000U;

  if ((partition_hi == 0) && (partition_lo <= min_cluster_size)) {
    NtClose(hdev);
    SetLastError(0x70); /* ERROR_DISK_FULL */
    return 0;
  }

  /* Original uses __alldiv (signed 64-bit divide) and handles partition_hi > 0.
   * We use 32-bit division: no Xbox utility partition exceeds 4GB, so
   * partition_hi is always 0 in practice.  Reject explicitly if it isn't. */
  if (partition_hi != 0) {
    NtClose(hdev);
    status = (NTSTATUS)0xc000009aL; /* STATUS_INSUFFICIENT_RESOURCES */
    goto fail;
  }
  cluster_count = (partition_lo / cluster_size) + 1U;

  fat_raw  = (cluster_count > 0xffefU) ? (cluster_count * 4U) : (cluster_count * 2U);
  fat_size = (fat_raw - 1U + sector_size_aligned) & ~(sector_size_aligned - 1U);

  /* Check full layout fits.  min_cluster_size + fat_size + cluster_size won't
   * overflow 32 bits for any real Xbox partition; partition_hi == 0 here. */
  {
    ULONG needed = min_cluster_size + fat_size + cluster_size;
    if (partition_lo < needed) {
      NtClose(hdev);
      SetLastError(0x70);
      return 0;
    }
  }

  buf = (ULONG *)FUN_001d0bb9(0, sector_size_aligned);
  if (!buf) {
    NtClose(hdev);
    status = (NTSTATUS)0xc000009aL; /* STATUS_INSUFFICIENT_RESOURCES */
    goto fail;
  }

  p = buf;
  for (i = sector_size_aligned >> 2; i != 0; i--)
    *p++ = 0xffffffffU;

  buf[0] = FATX_MAGIC;
  buf[2] = (cluster_size >> (sector_shift & 0x1f)) & 0xff; /* sectors per cluster */
  buf[3] = 1; /* root directory first cluster */
  *(unsigned short *)(buf + 4) = 0;
  KeQuerySystemTime(&timestamp);
  buf[1] = timestamp.LowPart; /* volume serial */

  off_lo    = 0;
  off_hi    = 0;
  remaining = min_cluster_size;

  /* Region 1: superblock at sector 0, zeroed sectors to min_cluster_size. */
  do {
    XAPI_LARGE_INTEGER byte_offset;
    byte_offset.LowPart  = off_lo;
    byte_offset.HighPart = (long)off_hi;
    write_status = NtWriteFile(hdev, 0, 0, 0, &iosb,
                               buf, sector_size_aligned, &byte_offset);
    off_lo += sector_size_aligned;
    if (off_lo < sector_size_aligned)
      off_hi++;
    if (write_status < 0) {
      FUN_001d0c16(buf);
      goto flush_close;
    }
    p = buf;
    for (i = sector_size_aligned >> 2; i != 0; i--)
      *p++ = 0;
    remaining -= sector_size_aligned;
  } while (remaining != 0);

  /* Region 2: FAT starting at min_cluster_size. */
  if (cluster_count > 0xffefU) {
    buf[0] = 0xfffffff8U;
    buf[1] = 0xffffffffU;
  } else {
    ((unsigned short *)buf)[0] = 0xfff8;
    ((unsigned short *)buf)[1] = 0xffff;
  }

  off_lo    = min_cluster_size;
  off_hi    = 0;
  remaining = fat_size;

  do {
    XAPI_LARGE_INTEGER byte_offset;
    byte_offset.LowPart  = off_lo;
    byte_offset.HighPart = (long)off_hi;
    write_status = NtWriteFile(hdev, 0, 0, 0, &iosb,
                               buf, sector_size_aligned, &byte_offset);
    off_lo += sector_size_aligned;
    if (off_lo < sector_size_aligned)
      off_hi++;
    if (write_status < 0)
      goto free_flush;
    buf[0] = 0;
    buf[1] = 0;
    remaining -= sector_size_aligned;
  } while (remaining != 0);

  /* Region 3: root-dir cluster (0xFF = no entries). */
  p = buf;
  for (i = sector_size_aligned >> 2; i != 0; i--)
    *p++ = 0xffffffffU;

  remaining = cluster_size;

  do {
    XAPI_LARGE_INTEGER byte_offset;
    byte_offset.LowPart  = off_lo;
    byte_offset.HighPart = (long)off_hi;
    write_status = NtWriteFile(hdev, 0, 0, 0, &iosb,
                               buf, sector_size_aligned, &byte_offset);
    off_lo += sector_size_aligned;
    if (off_lo < sector_size_aligned)
      off_hi++;
    if (write_status < 0)
      break;
    remaining -= sector_size_aligned;
  } while (remaining != 0);

free_flush:
  FUN_001d0c16(buf);
flush_close:
  NtFsControlFile(hdev, 0, 0, 0, &iosb, 0x90020, 0, 0, 0, 0);
  NtClose(hdev);
  if (write_status >= 0)
    return 1;

fail:
  XapiSetLastNTError(status);
  return 0;
}
