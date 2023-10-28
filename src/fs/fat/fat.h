#ifndef ONIX_FAT_H
#define ONIX_FAT_H

#include <onix/types.h>
#include <onix/fs.h>
#include <onix/buffer.h>

enum
{
    fat_readonly = 0x01,
    fat_hidden = 0x02,
    fat_system = 0x04,
    fat_volume_id = 0x08,
    fat_directory = 0x10,
    fat_archive = 0x20,
    fat_long_filename = 0x0F
};

typedef struct fat_entry_t
{
    u8 name[8];
    u8 ext[3];
    u8 attrs;
    u8 RESERVED;
    u8 creation_stamp;
    u16 ctime;
    u16 cdate;
    u16 last_accessed_date;
    u16 RESERVED; // for fat32
    u16 mtime;
    u16 mdate;
    u16 cluster;
    u32 filesize;
} _packed fat_entry_t;

typedef struct fat_lentry_t
{
    u8 entry_index;
    u16 name1[5];
    u8 attributes;
    u8 entry_type;
    u8 checksum;
    u16 name2[6];
    u16 zero;
    u16 name3[2];
} _packed fat_lentry_t;

typedef struct fat_root_entry_t
{
    fat_entry_t entry;
    u32 fat_start_sector;
    u32 fat_cluster;
    u32 fat_cluster_size;

    u32 root_start_sector;
    u32 root_cluster;
    u32 root_cluster_size;

    u32 data_start_sector;
    u32 data_cluster;
    u32 data_cluster_size;

    u16 cluster_offset;
} _packed fat_root_entry_t;

typedef struct fat_super_t
{
    u8 jmp[3];
    u8 oem[8];
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sector_count;
    u8 fat_count;
    u16 root_directory_entry_count;
    u16 sector_count16;
    u8 media_descriptor_type;
    u16 sectors_per_fat;
    u16 sectors_per_track;
    u16 head_count;
    u32 hidden_sector_count;
    u32 sector_count;

    u8 drive_number;
    u8 nt_flags;
    u8 signature;
    u32 volume_id;
    u8 volume_label_string[11];
    u8 identifier[8];
} _packed fat_super_t;

time_t fat_format_time(u16 date, u16 time);
void fat_parse_time(time_t t, u16 *date, u16 *time);

u16 fat_make_mode(fat_entry_t *entry);
inode_t *fat_make_inode(super_t *super, fat_entry_t *entry, buffer_t *buf);
inode_t *fat_iget(super_t *super, fat_entry_t *entry, buffer_t *buf);

u8 fat_chksum(u8 *name);
int fat_format_short_name(char *data, fat_entry_t *entry);
bool fat_match_short_name(char *name, char *entry_name, char **next, int count);
int fat_format_long_name(char *data, fat_lentry_t **entries, int count, u8 chksum);
void fat_parse_short_name(char *name, char *ext, fat_entry_t *entry);
int fat_parse_long_name(char *data, fat_lentry_t **entries, int count, u8 chksum);
err_t fat_create_short_name(inode_t *dir, fat_entry_t *entry, char *name, char *ext, int namelen);

int validate_short_name(char *name, char **ext);
void fat_parse_short_name(char *name, char *ext, fat_entry_t *entry);

void fat_close(inode_t *inode);
int fat_stat(inode_t *inode, stat_t *stat);
int fat_permission(inode_t *inode, int mask);

#endif