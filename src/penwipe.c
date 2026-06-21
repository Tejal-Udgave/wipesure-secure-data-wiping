/*
 * penwipe.c
 *
 * Linux-only program to securely wipe and format USB drives.
 *
 * Usage:
 *   gcc -O2 -o penwipe penwipe.c
 *
 *   sudo ./penwipe list
 *   sudo ./penwipe /dev/sdb wipe
 *   sudo ./penwipe /dev/sdb format fat32
 *   sudo ./penwipe /dev/sdb format exfat
 *   sudo ./penwipe /dev/sdb verify
 *
 * NOTE:
 *  - Use the whole device (e.g. /dev/sdb), not a partition (/dev/sdb1).
 *  - Run as root for destructive operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define CMD_MAX 512

/* Confirm with user before destructive actions */
int confirm_action(const char *msg)
{
    char buf[8];
    printf("%s [y/N]: ", msg);
    if (!fgets(buf, sizeof(buf), stdin))
        return 0;
    return tolower((unsigned char)buf[0]) == 'y';
}

/* Show usage */
void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s list\n", prog);
    printf("  %s <device> wipe\n", prog);
    printf("  %s <device> format <fat32|exfat>\n", prog);
    printf("  %s <device> verify\n", prog);
}

/* List removable devices */
void list_drives()
{
    printf("Listing removable devices:\n");
    system("lsblk -o NAME,SIZE,MODEL,RM,MOUNTPOINT,LABEL");
}

/* Wipe device using shred (preferred) or dd */
int wipe_device(const char *dev)
{
    if (!confirm_action("This will irreversibly erase all data on the device. Continue?"))
    {
        printf("Aborted.\n");
        return 1;
    }

    char cmd[CMD_MAX];
    printf("Unmounting partitions (if any)...\n");
    snprintf(cmd, sizeof(cmd), "umount %s* 2>/dev/null || true", dev);
    system(cmd);

    if (system("command -v shred >/dev/null 2>&1") == 0)
    {
        printf("Using shred for secure erase...\n");
        snprintf(cmd, sizeof(cmd), "shred -v -n 3 -z %s", dev);
    }
    else
    {
        printf("shred not available, using dd (zero fill)...\n");
        snprintf(cmd, sizeof(cmd), "dd if=/dev/zero of=%s bs=4M status=progress conv=fsync", dev);
    }

    printf("Running: %s\n", cmd);
    return system(cmd);
}

/* Format device */
int format_device(const char *dev, const char *fs)
{
    if (!confirm_action("This will create a new partition table and format the device. Continue?"))
    {
        printf("Aborted.\n");
        return 1;
    }

    char cmd[CMD_MAX], part[256];
    printf("Creating new partition table...\n");
    snprintf(cmd, sizeof(cmd), "parted -s %s mklabel msdos", dev);
    system(cmd);

    printf("Creating single partition...\n");
    snprintf(cmd, sizeof(cmd), "parted -s -a optimal %s mkpart primary 0%% 100%%", dev);
    system(cmd);

    snprintf(part, sizeof(part), "%s1", dev);

    if (strcasecmp(fs, "fat32") == 0)
    {
        printf("Formatting as FAT32...\n");
        snprintf(cmd, sizeof(cmd), "mkfs.vfat -F 32 %s", part);
    }
    else if (strcasecmp(fs, "exfat") == 0)
    {
        printf("Formatting as exFAT...\n");
        snprintf(cmd, sizeof(cmd), "mkfs.exfat %s", part);
    }
    else
    {
        printf("Unsupported filesystem: %s\n", fs);
        return 1;
    }

    printf("Running: %s\n", cmd);
    return system(cmd);
}

/* Verify device */
int verify_device(const char *dev)
{
    char cmd[CMD_MAX];
    printf("Checking device info:\n");
    snprintf(cmd, sizeof(cmd), "lsblk %s -o NAME,SIZE,TYPE,MOUNTPOINT", dev);
    system(cmd);

    printf("Reading first 1KB from device:\n");
    snprintf(cmd, sizeof(cmd), "dd if=%s bs=512 count=2 2>/dev/null | hexdump -C | head", dev);
    return system(cmd);
}

/* Main */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "list") == 0)
    {
        list_drives();
        return 0;
    }

    if (argc < 3)
    {
        usage(argv[0]);
        return 1;
    }

    const char *dev = argv[1];
    const char *op = argv[2];

    if (strcmp(op, "wipe") == 0)
    {
        return wipe_device(dev);
    }
    else if (strcmp(op, "format") == 0)
    {
        if (argc < 4)
        {
            printf("Specify filesystem: fat32 or exfat\n");
            return 1;
        }
        return format_device(dev, argv[3]);
    }
    else if (strcmp(op, "verify") == 0)
    {
        return verify_device(dev);
    }
    else
    {
        usage(argv[0]);
        return 1;
    }
}
