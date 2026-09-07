#include "directory_model.h"

#include <stdio.h>
#include <string.h>

static uint32_t read_u32(const unsigned char *buffer, size_t offset)
{
    uint32_t value;

    memcpy(&value, buffer + offset, sizeof value);
    return value;
}

static uint64_t read_u64(const unsigned char *buffer, size_t offset)
{
    uint64_t value;

    memcpy(&value, buffer + offset, sizeof value);
    return value;
}

static bool add_entry(
    RecompDirectoryModel *model,
    const char *name,
    uint64_t size,
    uint32_t attributes)
{
    RecompDirectoryEntry entry = {
        .size = size,
        .attributes = attributes,
    };

    if (strlen(name) >= sizeof entry.name) {
        return false;
    }
    strcpy(entry.name, name);
    return recomp_directory_add(model, &entry);
}

int recomp_directory_model_test(void)
{
    RecompDirectoryModel model;
    RecompDirectoryEntry entry;
    unsigned char buffer[0x80];
    size_t bytes_written = 0u;
    int passed = 1;

    recomp_directory_reset(&model);
    passed &= !recomp_directory_next(&model, NULL, &entry);
    passed &= add_entry(&model, "alpha.bin", 12u, 0x80u);
    passed &= add_entry(&model, "Folder", 0u, 0x10u);
    passed &= add_entry(&model, "omega.dat", 34u, 0x80u);

    passed &= recomp_directory_next(&model, "*", &entry) &&
        strcmp(entry.name, "alpha.bin") == 0 && entry.size == 12u;
    passed &= recomp_directory_next(&model, "*.*", &entry) &&
        strcmp(entry.name, "Folder") == 0 && entry.attributes == 0x10u;
    passed &= recomp_directory_next(&model, "", &entry) &&
        strcmp(entry.name, "omega.dat") == 0;
    passed &= !recomp_directory_next(&model, NULL, &entry);

    recomp_directory_restart(&model);
    passed &= recomp_directory_next(&model, "fOlDeR", &entry) &&
        strcmp(entry.name, "Folder") == 0;
    passed &= !recomp_directory_next(&model, "missing", &entry);
    passed &= !recomp_directory_next(&model, "omega.dat", &entry);

    recomp_directory_restart(&model);
    passed &= recomp_directory_next(&model, "omega.dat", &entry) &&
        strcmp(entry.name, "omega.dat") == 0;
    entry.creation_time = 1u;
    entry.last_access_time = 2u;
    entry.last_write_time = 3u;
    entry.change_time = 4u;
    entry.allocation_size = 4096u;
    passed &= recomp_directory_serialize(
        &entry, buffer, sizeof buffer, &bytes_written);
    passed &= bytes_written == 0x40u + strlen(entry.name) + 1u;
    passed &= read_u64(buffer, 0x08u) == 1u;
    passed &= read_u64(buffer, 0x10u) == 2u;
    passed &= read_u64(buffer, 0x18u) == 3u;
    passed &= read_u64(buffer, 0x20u) == 4u;
    passed &= read_u64(buffer, 0x28u) == 34u;
    passed &= read_u64(buffer, 0x30u) == 4096u;
    passed &= read_u32(buffer, 0x38u) == 0x80u;
    passed &= read_u32(buffer, 0x3cu) == strlen(entry.name);
    passed &= strcmp((const char *)buffer + 0x40u, "omega.dat") == 0;
    passed &= !recomp_directory_serialize(
        &entry, buffer, 0x40u + strlen(entry.name), &bytes_written);
    passed &= bytes_written == 0u;

    /* A suffix filter must skip metadata and still return the payload. */
    recomp_directory_reset(&model);
    passed &= add_entry(&model, "metadata.xbx", 8u, 0x80u);
    passed &= add_entry(&model, "profile.dat", 20u, 0x80u);
    passed &= recomp_directory_next(&model, "*.dat", &entry) &&
        strcmp(entry.name, "profile.dat") == 0 && entry.size == 20u;
    passed &= !recomp_directory_next(&model, "*.dat", &entry);
    passed &= !recomp_directory_next(&model, "*", &entry);
    recomp_directory_restart(&model);
    passed &= recomp_directory_next(&model, "*.DAT", &entry) &&
        strcmp(entry.name, "profile.dat") == 0;
    recomp_directory_restart(&model);
    passed &= recomp_directory_next(&model, "meta*", &entry) &&
        strcmp(entry.name, "metadata.xbx") == 0;
    passed &= recomp_directory_next(&model, "*.dat", &entry) &&
        strcmp(entry.name, "profile.dat") == 0;

    {
        static const struct {
            const char *pattern;
            const char *name;
            bool matches;
        } cases[] = {
            {"*.dat", "PROFILE.DAT", true},
            {"*.dat", "metadata.xbx", false},
            {"*.dat", "profile.dat.bak", false},
            {"profile*.dat", "profile.dat", true},
            {"p**ro*file*.DAT", "profile.dat", true},
            {"*file.dat", "profilefile.dat", true},
            {"p*fi*z.dat", "profile.dat", false},
            {"file*", "profile.dat", false},
            {"profile.dat*", "profile.dat.bak", true},
            {"profile.dat", "profile.dat.bak", false},
            {"***", "Folder", true},
            {"*.*", "Folder", true},
            {NULL, "profile.dat", true},
            {"", "profile.dat", true},
        };
        for (size_t i = 0u; i < sizeof cases / sizeof cases[0]; ++i) {
            recomp_directory_reset(&model);
            passed &= add_entry(&model, cases[i].name, 1u, 0x80u);
            if (recomp_directory_next(&model, cases[i].pattern, &entry) !=
                cases[i].matches) {
                fprintf(stderr, "directory pattern '%s' for '%s' failed\n",
                    cases[i].pattern != NULL ? cases[i].pattern : "(null)",
                    cases[i].name);
                passed = 0;
            }
        }
    }

    if (!passed) {
        fprintf(stderr, "directory model test failed\n");
    }
    return passed;
}
