#include "kernel_abi.h"

#include <stdint.h>

uint32_t recomp_kernel_ntstatus_to_dos_error(uint32_t status)
{
    return status;
}

static void bridge_rtl_ntstatus_to_dos_error(void)
{
    uint32_t status = kernel_arg(1u);

    kernel_return(1u, recomp_kernel_ntstatus_to_dos_error(status));
}

/* NT time is 100-nanosecond ticks since 1601-01-01 UTC, and 1601 is the first
   year of a 400-year Gregorian cycle, so the calendar arithmetic below is
   exact with no epoch special cases. The civil/day conversion is the standard
   era method: shift the year to start in March, which puts the leap day at the
   end of the year and makes month lengths a repeating pattern.

   584694 is the number of days from 0000-03-01 (the era origin) to
   1601-01-01, so adding it turns a tick count into an era-relative day. */
enum {
    TICKS_PER_MILLISECOND = 10000u,
    MILLISECONDS_PER_DAY = 86400000u,
    ERA_DAYS = 146097u,       /* days in 400 Gregorian years */
    ERA_ORIGIN_TO_1601 = 584694u,
};

/* 1601-01-01 was a Monday, and TIME_FIELDS counts weekdays from Sunday. */
static uint16_t weekday_of_day(uint64_t day)
{
    return (uint16_t)((day + 1u) % 7u);
}

int recomp_kernel_time_to_time_fields(uint64_t time, uint16_t *fields)
{
    if (fields == NULL) {
        return 0;
    }

    uint64_t milliseconds = time / TICKS_PER_MILLISECOND;
    uint64_t day = milliseconds / MILLISECONDS_PER_DAY;
    uint32_t time_of_day = (uint32_t)(milliseconds % MILLISECONDS_PER_DAY);

    uint64_t shifted = day + ERA_ORIGIN_TO_1601;
    uint64_t era = shifted / ERA_DAYS;
    uint32_t day_of_era = (uint32_t)(shifted % ERA_DAYS);
    uint32_t year_of_era =
        (day_of_era - day_of_era / 1460u + day_of_era / 36524u -
         day_of_era / 146096u) / 365u;
    uint64_t year = year_of_era + era * 400u;
    uint32_t day_of_year =
        day_of_era -
        (365u * year_of_era + year_of_era / 4u - year_of_era / 100u);
    /* Month index counting from March, where the pattern is regular. */
    uint32_t march_month = (5u * day_of_year + 2u) / 153u;
    uint32_t day_of_month = day_of_year - (153u * march_month + 2u) / 5u + 1u;
    uint32_t month = march_month + (march_month < 10u ? 3u : (uint32_t)-9);
    if (month <= 2u) {
        ++year;
    }

    /* The guest stores the year in a CSHORT, so anything past 65535 cannot be
       reported. Leave the caller's buffer untouched rather than wrap. */
    if (year > 0xffffu) {
        return 0;
    }

    fields[0] = (uint16_t)year;
    fields[1] = (uint16_t)month;
    fields[2] = (uint16_t)day_of_month;
    fields[3] = (uint16_t)(time_of_day / 3600000u);
    fields[4] = (uint16_t)(time_of_day / 60000u % 60u);
    fields[5] = (uint16_t)(time_of_day / 1000u % 60u);
    fields[6] = (uint16_t)(time_of_day % 1000u);
    fields[7] = weekday_of_day(day);
    return 1;
}

static int is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u;
}

static uint32_t days_in_month(uint32_t year, uint32_t month)
{
    static const uint8_t lengths[12] = {
        31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
    };

    if (month == 2u && is_leap_year(year)) {
        return 29u;
    }
    return lengths[month - 1u];
}

int recomp_kernel_time_fields_to_time(const uint16_t *fields, uint64_t *time)
{
    if (fields == NULL || time == NULL) {
        return 0;
    }

    uint32_t year = fields[0];
    uint32_t month = fields[1];
    uint32_t day = fields[2];
    uint32_t hour = fields[3];
    uint32_t minute = fields[4];
    uint32_t second = fields[5];
    uint32_t millisecond = fields[6];

    /* The weekday field is output-only; the kernel derives it and ignores
       whatever the caller left there. */
    if (year < 1601u || month < 1u || month > 12u || day < 1u ||
        day > days_in_month(year, month) || hour > 23u || minute > 59u ||
        second > 59u || millisecond > 999u) {
        return 0;
    }

    /* Same era arithmetic run backwards: move January and February into the
       previous year so the leap day lands last. */
    uint32_t shifted_year = year - (month <= 2u ? 1u : 0u);
    uint32_t era = shifted_year / 400u;
    uint32_t year_of_era = shifted_year - era * 400u;
    uint32_t march_month = month + (month > 2u ? (uint32_t)-3 : 9u);
    uint32_t day_of_year = (153u * march_month + 2u) / 5u + day - 1u;
    uint32_t day_of_era = year_of_era * 365u + year_of_era / 4u -
                          year_of_era / 100u + day_of_year;
    uint64_t days = (uint64_t)era * ERA_DAYS + day_of_era - ERA_ORIGIN_TO_1601;

    uint64_t milliseconds = days * MILLISECONDS_PER_DAY +
                            hour * 3600000u + minute * 60000u +
                            second * 1000u + millisecond;
    *time = milliseconds * TICKS_PER_MILLISECOND;
    return 1;
}

/* RtlTimeToTimeFields(PLARGE_INTEGER Time, PTIME_FIELDS TimeFields) returns
   void and writes eight CSHORTs. A guest pointer of zero, or a timestamp the
   fields cannot hold, leaves the destination alone. */
static void bridge_rtl_time_to_time_fields(void)
{
    uint32_t time = kernel_arg(1u);
    uint32_t destination = kernel_arg(2u);

    if (time != 0u && destination != 0u) {
        uint64_t value = (uint64_t)*recomp_memory_u32(time) |
                         ((uint64_t)*recomp_memory_u32(time + 4u) << 32u);
        uint16_t fields[RECOMP_TIME_FIELD_COUNT];

        if (recomp_kernel_time_to_time_fields(value, fields)) {
            for (unsigned i = 0u; i < RECOMP_TIME_FIELD_COUNT; ++i) {
                *recomp_memory_u16(destination + 2u * i) = fields[i];
            }
        }
    }
    kernel_return(2u, 0u);
}

/* RtlTimeFieldsToTime(PTIME_FIELDS TimeFields, PLARGE_INTEGER Time) is the
   inverse and returns BOOLEAN. The guest imports it alongside ordinal 305. */
static void bridge_rtl_time_fields_to_time(void)
{
    uint32_t source = kernel_arg(1u);
    uint32_t time = kernel_arg(2u);

    uint32_t converted = 0u;
    if (source != 0u && time != 0u) {
        uint16_t fields[RECOMP_TIME_FIELD_COUNT];
        uint64_t value = 0u;

        for (unsigned i = 0u; i < RECOMP_TIME_FIELD_COUNT; ++i) {
            fields[i] = *recomp_memory_u16(source + 2u * i);
        }
        if (recomp_kernel_time_fields_to_time(fields, &value)) {
            *recomp_memory_u32(time) = (uint32_t)value;
            *recomp_memory_u32(time + 4u) = (uint32_t)(value >> 32u);
            converted = 1u;
        }
    }
    kernel_return(2u, converted);
}

/* RtlInitAnsiString lays a guest ANSI_STRING {Length u16, MaximumLength u16,
   Buffer u32} over an existing NUL-terminated string without copying it. The
   buffer pointer is the guest source address itself. */
static void bridge_rtl_init_ansi_string(void)
{
    uint32_t destination = kernel_arg(1u);
    uint32_t source = kernel_arg(2u);

    uint32_t length = 0u;
    if (source != 0u) {
        while (length < 0xffffu &&
               *recomp_memory_i8(source + length) != 0) {
            ++length;
        }
    }
    if (destination != 0u) {
        uint8_t *dest = (uint8_t *)recomp_memory_u32(destination);
        dest[0] = (uint8_t)(length & 0xffu);
        dest[1] = (uint8_t)((length >> 8u) & 0xffu);
        uint32_t maximum = source == 0u ? 0u : length + 1u;
        dest[2] = (uint8_t)(maximum & 0xffu);
        dest[3] = (uint8_t)((maximum >> 8u) & 0xffu);
        *recomp_memory_u32(destination + 4u) = source;
    }
    kernel_return(2u, 0u);
}

/* RtlEqualString compares two guest ANSI_STRINGs by content. The native host
   stubbed this to pointer equality, but the guest now compares mounted path
   names by value, so compare the bytes, honouring the case-insensitive flag. */
static uint32_t ansi_char_at(uint32_t string, uint32_t index, int fold)
{
    uint32_t buffer = *recomp_memory_u32(string + 4u);
    uint32_t c = (uint8_t)*recomp_memory_i8(buffer + index);
    if (fold && c >= 'a' && c <= 'z') {
        c -= ('a' - 'A');
    }
    return c;
}

static void bridge_rtl_equal_string(void)
{
    uint32_t string1 = kernel_arg(1u);
    uint32_t string2 = kernel_arg(2u);
    uint32_t case_insensitive = kernel_arg(3u);

    int equal = 0;
    if (string1 != 0u && string2 != 0u) {
        uint8_t *s1 = (uint8_t *)recomp_memory_u32(string1);
        uint8_t *s2 = (uint8_t *)recomp_memory_u32(string2);
        uint32_t length1 = (uint32_t)s1[0] | ((uint32_t)s1[1] << 8u);
        uint32_t length2 = (uint32_t)s2[0] | ((uint32_t)s2[1] << 8u);

        if (length1 == length2) {
            equal = 1;
            for (uint32_t i = 0; i < length1; ++i) {
                if (ansi_char_at(string1, i, case_insensitive != 0u) !=
                    ansi_char_at(string2, i, case_insensitive != 0u)) {
                    equal = 0;
                    break;
                }
            }
        }
    }
    kernel_return(3u, (uint32_t)equal);
}

RecompFunction recomp_kernel_rtl(uint32_t ordinal)
{
    switch (ordinal) {
    case 279u: return bridge_rtl_equal_string;
    case 289u: return bridge_rtl_init_ansi_string;
    case 301u: return bridge_rtl_ntstatus_to_dos_error;
    case 304u: return bridge_rtl_time_fields_to_time;
    case 305u: return bridge_rtl_time_to_time_fields;
    default: return NULL;
    }
}
