#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))
#define STACK_BASE 0x25010000u
#define STACK_SIZE 0x40u
#define ENTRY_ESP (STACK_BASE + 0x20u)
#define CALLER_OBJECT 0x26010000u
#define CALLER_SIZE 0xa0u
#define CALLEE_OBJECT 0x27010000u
#define CALLEE_SIZE 0x28u
#define OUTER_STACK_BASE 0x25020000u
#define OUTER_STACK_SIZE 0x100u
#define OUTER_ENTRY_ESP (OUTER_STACK_BASE + 0xc0u)
#define STATE_STACK_BASE 0x25030000u
#define STATE_STACK_SIZE 0x80u
#define STATE_ENTRY_ESP (STATE_STACK_BASE + 0x60u)
#define DEEP_STATE_STACK_BASE 0x25040000u
#define DEEP_STATE_STACK_SIZE 0x180u
#define DEEP_STATE_ENTRY_ESP (DEEP_STATE_STACK_BASE + 0x140u)
#define GROWTH_STACK_BASE 0x25050000u
#define GROWTH_STACK_SIZE 0x300u
#define GROWTH_ENTRY_ESP (GROWTH_STACK_BASE + 0x200u)
#define GROWTH_FRAME_ESP (GROWTH_ENTRY_ESP - 0x48u)
#define GROWTH_QUERY_OUTPUT (GROWTH_FRAME_ESP - 0x24u)
#define ED80_STACK_BASE 0x2506fc80u
#define ED80_STACK_SIZE 0x500u
#define ED80_ENTRY_ESP (ED80_STACK_BASE + 0x400u)
#define A0_STACK_BASE 0x25080000u
#define A0_STACK_SIZE 0x100u
#define A0_ENTRY_ESP (A0_STACK_BASE + 0x80u)
#define NOTIFY_STACK_BASE 0x25090000u
#define NOTIFY_STACK_SIZE 0x40u
#define NOTIFY_ENTRY_ESP (NOTIFY_STACK_BASE + 0x20u)
#define NOTIFY_CALLER_STACK_BASE 0x250a0000u
#define NOTIFY_CALLER_STACK_SIZE 0x100u
#define NOTIFY_CALLER_ENTRY_ESP (NOTIFY_CALLER_STACK_BASE + 0x60u)
#define NOTIFY_CALLER_RECORD 0x33010000u
#define OUTER_OBJECT 0x28010000u
#define OUTER_SIZE 0x60u
#define STATE_CALLER_SIZE 0xa0u
#define HELPER_OBJECT 0x29010000u
#define HELPER_SIZE 0xc0u
#define INTERFACE_OBJECT 0x2a010000u
#define VTABLE_OBJECT 0x2b010000u
#define ITEM_INTERFACE_OBJECT 0x2c010000u
#define ITEM_VTABLE_OBJECT 0x2d010000u
#define PARSER_PACKET_OBJECT 0x2e010000u
#define COPY_SOURCE_OBJECT 0x2f010000u
#define COPY_DESTINATION_OBJECT 0x2f020000u
#define STATE_4_SOURCE_OBJECT 0x2f030000u
#define STATE_4_DESTINATION_OBJECT 0x2f040000u
#define GROWTH_CALLER_OBJECT 0x30010000u
#define GROWTH_STREAM_OBJECT 0x30020000u
#define GROWTH_VTABLE_OBJECT 0x30030080u
#define GROWTH_PACKET_OBJECT 0x30040000u
#define GROWTH_CLEANUP_OBJECT 0x30050000u
#define GROWTH_NESTED_OBJECT 0x30060000u
#define GROWTH_PARSER_PACKET_OBJECT 0x30070000u
#define GROWTH_RESOURCE_OBJECT 0x00000020u
#define ED80_OBJECT 0x32010000u
#define A0_STAGE_FLAG 0x003b75ccu
#define A0_TEARDOWN_FLAG 0x003b7f00u
#define A0_SETUP_RECORDS 0x00edeac0u
#define A0_SETUP_SIZE 0x9c0u
#define A0_TEARDOWN_RECORDS 0x00ede020u
#define A0_TEARDOWN_SIZE 0xa80u
#define A0_ACTIVE_RECORDS 0x00ee3060u
#define A0_ACTIVE_SIZE 0xa80u
#define QUERY_A_TARGET 0x00300010u
#define QUERY_B_TARGET 0x00300020u
#define LOOP_CALLBACK_TARGET 0x00300030u
#define FINAL_CALLBACK_TARGET 0x00300040u
#define ACTIVATE_CALLBACK_TARGET 0x00300050u
#define FALLBACK_CALLBACK_TARGET 0x00300060u
#define DEEP_CALLBACK_TARGET 0x00300070u
#define STATE_TAIL_CALLBACK_TARGET 0x00300080u
#define STATE_2_PREPARE_TARGET 0x00300090u
#define STATE_2_COMMIT_TARGET 0x003000a0u
#define STATE_QUERY_TARGET 0x003000b0u
#define STATE_RELEASE_TARGET 0x003000c0u
#define GROWTH_18_TARGET 0x003000d0u
#define GROWTH_1C_TARGET 0x003000e0u
#define GROWTH_20_TARGET 0x003000f0u
#define STATE_4_PREPARE_TARGET 0x00300130u
#define STATE_4_COMMIT_TARGET 0x00300140u
#define ED80_BEGIN_TARGET 0x00300150u
#define ED80_END_TARGET 0x00300160u
#define ED80_F010_METHOD20_TARGET 0x00300170u
#define ED80_F010_METHOD1C_TARGET 0x00300180u
#define ED80_EFA6_QUERY_TARGET 0x00300190u
#define ED80_LOG_TARGET 0x003001a0u
#define NOTIFY_TARGET 0x003001b0u
#define NOTIFY_CALLER_TARGET 0x003001c0u
#define NOTIFY_GLOBALS 0x003b7f28u
#define NOTIFY_CALLER_GLOBALS_SIZE 0x58u
#define NOTIFY_TLS_INDEX 0x003b5258u
#define NOTIFY_TLS_LOW 0x00000000u
#define NOTIFY_TLS_SLOTS 0x00761000u
#define NOTIFY_TLS_DATA 0x00760000u
#define NOTIFY_CONTEXT 0x468ace02u
#define NOTIFY_ARG0 0x13579bdfu
#define NOTIFY_ARG1 0x2468ace0u
#define STATE_14_TAIL_SLOT 0x003b7ee0u
#define STATE_B_TAIL_SLOT 0x003b80f0u
#define STATE_A_TAIL_SLOT 0x00ed1200u
#define LOOP_CONTEXT 0x1234abcdu
#define FINAL_ARGUMENT 0x0badc0deu
#define STATE_DISPATCH_VALUE 0x1234u
#define STATE_A_DISPATCH_VALUE 0x000au
#define STATE_B_DISPATCH_VALUE 0x000bu
#define STATE_2_DISPATCH_VALUE 0x0002u
#define STATE_3_DISPATCH_VALUE 0x0003u
#define STATE_4_DISPATCH_VALUE 0x0004u
#define STATE_14_DISPATCH_VALUE 0x0014u
#define STATE_2_PREPARE_CONTEXT 0x22446688u
#define STATE_2_COMMIT_CONTEXT 0x11335577u
#define ED80_BEGIN_CONTEXT 0x13572468u
#define ED80_END_CONTEXT 0x24681357u
#define ED80_LOG_CONTEXT 0x35792468u
#define ED80_RECORD 0x32020000u
#define ED80_PROVIDER 0x32030000u
#define ED80_EFA6_ARRAY 0x32040000u
#define ED80_EFA6_VTABLE 0x32050000u
#define ED80_RESULT 0x00edeab0u
#define ED80_COUNT 0x00edeaacu
#define ED80_CALLBACKS 0x003b7f70u
#define ED80_F130_COUNT 0x003b7f08u
#define ED80_LOG_FORMAT 0x0023b444u
#define ED80_LOG_BUFFER 0x00ed1000u
#define ED80_LOG_GLOBALS 0x003b8124u
#define ED80_FORMAT_CLASS 0x0023f009u
#define ED80_FORMAT_STATE 0x0023efe8u
#define ED80_FORMAT_JUMP 0x001bd6e6u
#define ED80_MBCS_POINTER 0x00285d88u
#define ED80_MBCS_TABLE 0x32060000u

void sub_0018D9A0(void);
void sub_001889A0(void);
void sub_0018D3D0(void);
void sub_0018D3E8(void);
void sub_0018AEA0(void);
void sub_0018D380(void);
void sub_0018D6C0(void);
void sub_0018D720(void);
void sub_0018D760(void);
void sub_0018D780(void);
void sub_0018D7C5(void);
void sub_0018D894(void);
void sub_0018D8B0(void);
void sub_0018DB20(void);
void sub_0018DB70(void);
void sub_0018DBB0(void);
void sub_0018DBC0(void);
void sub_0018DBD0(void);
void sub_0018DC30(void);
void sub_0018DE00(void);
void sub_0018DE2A(void);
void sub_0018DE65(void);
void sub_0018DE76(void);
void sub_0018DE9E(void);
void sub_0018DF30(void);
void sub_0018DF90(void);
void sub_0018DFA4(void);
void sub_0018DFB1(void);
void sub_0018E690(void);
void sub_0018ED80(void);
void sub_00191180(void);
void sub_0018315B(void);
void sub_0019132E(void);
void sub_0018FFF0(void);
void sub_0019000F(void);
void sub_0019001C(void);
void sub_00190030(void);
void sub_0019003B(void);
void sub_00190053(void);
void sub_00190400(void);
void sub_0019040B(void);
void sub_00190423(void);
void sub_00190480(void);
void sub_001904B9(void);
void sub_00194AC0(void);
void sub_00194AB0(void);
void sub_00194B30(void);
void sub_00194B40(void);
void sub_00194C60(void);
void sub_00194C98(void);
void sub_00194CD0(void);
void sub_00194CF0(void);
void sub_00194D10(void);
void sub_00194D40(void);
void sub_00194D50(void);
void sub_00194D60(void);
void sub_00194E38(void);
void sub_00194E60(void);
void sub_00194EC0(void);
void sub_00194FA0(void);
void sub_00195040(void);
void sub_00195059(void);
void sub_00195068(void);
void sub_00195077(void);
void sub_00195086(void);
void sub_00195095(void);
void sub_001950A4(void);
void sub_001950B3(void);
void sub_001950C2(void);
void sub_00195200(void);
void sub_0019526C(void);
void sub_00195276(void);
void sub_001953A7(void);
void sub_001953E0(void);
void sub_001954F0(void);
void sub_00195540(void);
void sub_00195558(void);
void sub_00195660(void);
void sub_00195690(void);
void sub_00195C60(void);
void sub_00196050(void);
void sub_00196084(void);
void sub_00196090(void);
void sub_001960C4(void);
void sub_001960D0(void);
void sub_001960F0(void);
void sub_00196410(void);
void sub_00196760(void);
void sub_0019677A(void);
void sub_00196789(void);
void sub_00196797(void);
void sub_00196B70(void);
void sub_00196C90(void);
void sub_00196EA0(void);
void sub_00196EBA(void);
void sub_00196EC5(void);
void sub_001972F0(void);
void sub_00197307(void);
void sub_00196110(void);
void sub_001A9610(void);

static void query_a(void);
static void query_b(void);
static void loop_callback(void);
static void final_callback(void);
static void activate_callback(void);
static void fallback_callback(void);
static void deep_callback(void);
static void state_tail_callback(void);
static void state_2_prepare_callback(void);
static void state_2_commit_callback(void);
static void state_query_callback(void);
static void state_release_callback(void);
static void growth_18_callback(void);
static void growth_1c_callback(void);
static void growth_20_callback(void);
static void state_4_prepare_callback(void);
static void state_4_commit_callback(void);
static void ed80_begin_callback(void);
static void ed80_end_callback(void);
static void ed80_f010_method20_callback(void);
static void ed80_f010_method1c_callback(void);
static void ed80_efa6_query_callback(void);
static void ed80_log_callback(void);
static void notify_callback(void);
static void notify_caller_callback(void);

static void fail_unresolved_direct(uint32_t address)
{
    fprintf(
        stderr,
        "recomp runtime: unresolved direct call to 0x%08x\n",
        address);
    exit(2);
}

#define UNRESOLVED_DIRECT(symbol, address) \
    void symbol(void) \
    { \
        fail_unresolved_direct(address); \
    }

UNRESOLVED_DIRECT(sub_00196130, 0x00196130u)
UNRESOLVED_DIRECT(sub_00183148, 0x00183148u)
UNRESOLVED_DIRECT(sub_0018ABC0, 0x0018abc0u)
UNRESOLVED_DIRECT(sub_001900DF, 0x001900dfu)
UNRESOLVED_DIRECT(sub_001952B7, 0x001952b7u)
UNRESOLVED_DIRECT(sub_00195407, 0x00195407u)
UNRESOLVED_DIRECT(sub_00196510, 0x00196510u)
UNRESOLVED_DIRECT(sub_00196620, 0x00196620u)
UNRESOLVED_DIRECT(sub_00196D90, 0x00196d90u)
UNRESOLVED_DIRECT(sub_00188870, 0x00188870u)
UNRESOLVED_DIRECT(sub_0018AB80, 0x0018ab80u)
UNRESOLVED_DIRECT(sub_0018DC20, 0x0018dc20u)
UNRESOLVED_DIRECT(sub_0018E8B0, 0x0018e8b0u)
UNRESOLVED_DIRECT(sub_00188590, 0x00188590u)
UNRESOLVED_DIRECT(sub_0018ED55, 0x0018ed55u)
UNRESOLVED_DIRECT(sub_0018D700, 0x0018d700u)
UNRESOLVED_DIRECT(sub_0018E3D0, 0x0018e3d0u)
UNRESOLVED_DIRECT(sub_0018F970, 0x0018f970u)
UNRESOLVED_DIRECT(sub_001BCDC0, 0x001bcdc0u)
UNRESOLVED_DIRECT(sub_001BCEF5, 0x001bcef5u)
UNRESOLVED_DIRECT(sub_001BBEA4, 0x001bbea4u)
UNRESOLVED_DIRECT(sub_001BBEB6, 0x001bbeb6u)
UNRESOLVED_DIRECT(sub_001BCF0C, 0x001bcf0cu)
UNRESOLVED_DIRECT(sub_001BCF30, 0x001bcf30u)
UNRESOLVED_DIRECT(sub_001C0AC9, 0x001c0ac9u)
UNRESOLVED_DIRECT(sub_001C0B00, 0x001c0b00u)

#undef UNRESOLVED_DIRECT

typedef struct BoundaryFixture {
    const char *name;
    RecompRegisters initial;
    uint32_t caller_state;
    uint32_t callee_state;
    int calls_callee;
    int callee_writes;
    size_t expected_access_count;
} BoundaryFixture;

static const BoundaryFixture fixtures[] = {
    {
        .name = "caller-skips-call",
        .initial = {
            0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u,
            0x55555555u, 0x66666666u, 0x77777777u, ENTRY_ESP,
        },
        .caller_state = 4u,
        .callee_state = 3u,
        .calls_callee = 0,
        .callee_writes = 0,
        .expected_access_count = 4u,
    },
    {
        .name = "callee-skips-write",
        .initial = {
            0x12121212u, 0x23232323u, 0x34343434u, 0x45454545u,
            0x56565656u, 0x67676767u, 0x78787878u, ENTRY_ESP,
        },
        .caller_state = 3u,
        .callee_state = 4u,
        .calls_callee = 1,
        .callee_writes = 0,
        .expected_access_count = 11u,
    },
    {
        .name = "callee-performs-write",
        .initial = {
            0x13131313u, 0x24242424u, 0x35353535u, 0x46464646u,
            0x57575757u, 0x68686868u, 0x79797979u, ENTRY_ESP,
        },
        .caller_state = 3u,
        .callee_state = 3u,
        .calls_callee = 1,
        .callee_writes = 1,
        .expected_access_count = 12u,
    },
};

static const RecompFunctionEntry functions[] = {
    {0x001889a0u, sub_001889A0},
    {0x0018315bu, sub_0018315B},
    {0x0018d3d0u, sub_0018D3D0},
    {0x0018d3e8u, sub_0018D3E8},
    {0x0018aea0u, sub_0018AEA0},
    {0x0018d380u, sub_0018D380},
    {0x0018d6c0u, sub_0018D6C0},
    {0x0018d720u, sub_0018D720},
    {0x0018d760u, sub_0018D760},
    {0x0018d780u, sub_0018D780},
    {0x0018d7c5u, sub_0018D7C5},
    {0x0018d894u, sub_0018D894},
    {0x0018d8b0u, sub_0018D8B0},
    {0x0018d9a0u, sub_0018D9A0},
    {0x0018db20u, sub_0018DB20},
    {0x0018db70u, sub_0018DB70},
    {0x0018dbb0u, sub_0018DBB0},
    {0x0018dbc0u, sub_0018DBC0},
    {0x0018dbd0u, sub_0018DBD0},
    {0x0018dc30u, sub_0018DC30},
    {0x0018de00u, sub_0018DE00},
    {0x0018de2au, sub_0018DE2A},
    {0x0018de65u, sub_0018DE65},
    {0x0018de76u, sub_0018DE76},
    {0x0018de9eu, sub_0018DE9E},
    {0x0018df30u, sub_0018DF30},
    {0x0018df90u, sub_0018DF90},
    {0x0018dfa4u, sub_0018DFA4},
    {0x0018dfb1u, sub_0018DFB1},
    {0x0018e690u, sub_0018E690},
    {0x0018ed80u, sub_0018ED80},
    {0x00191180u, sub_00191180},
    {0x0019132eu, sub_0019132E},
    {0x0018fff0u, sub_0018FFF0},
    {0x0019000fu, sub_0019000F},
    {0x0019001cu, sub_0019001C},
    {0x00190030u, sub_00190030},
    {0x0019003bu, sub_0019003B},
    {0x00190053u, sub_00190053},
    {0x00190400u, sub_00190400},
    {0x0019040bu, sub_0019040B},
    {0x00190423u, sub_00190423},
    {0x00190480u, sub_00190480},
    {0x001904b9u, sub_001904B9},
    {0x00194ac0u, sub_00194AC0},
    {0x00194ab0u, sub_00194AB0},
    {0x00194b30u, sub_00194B30},
    {0x00194b40u, sub_00194B40},
    {0x00194c60u, sub_00194C60},
    {0x00194c98u, sub_00194C98},
    {0x00194cd0u, sub_00194CD0},
    {0x00194cf0u, sub_00194CF0},
    {0x00194d10u, sub_00194D10},
    {0x00194d40u, sub_00194D40},
    {0x00194d50u, sub_00194D50},
    {0x00194d60u, sub_00194D60},
    {0x00194e38u, sub_00194E38},
    {0x00194e60u, sub_00194E60},
    {0x00194ec0u, sub_00194EC0},
    {0x00194fa0u, sub_00194FA0},
    {0x00195040u, sub_00195040},
    {0x00195059u, sub_00195059},
    {0x00195068u, sub_00195068},
    {0x00195077u, sub_00195077},
    {0x00195086u, sub_00195086},
    {0x00195095u, sub_00195095},
    {0x001950a4u, sub_001950A4},
    {0x001950b3u, sub_001950B3},
    {0x001950c2u, sub_001950C2},
    {0x00195200u, sub_00195200},
    {0x0019526cu, sub_0019526C},
    {0x00195276u, sub_00195276},
    {0x001953a7u, sub_001953A7},
    {0x001953e0u, sub_001953E0},
    {0x001954f0u, sub_001954F0},
    {0x00195540u, sub_00195540},
    {0x00195558u, sub_00195558},
    {0x00195660u, sub_00195660},
    {0x00195690u, sub_00195690},
    {0x00195c60u, sub_00195C60},
    {0x00196050u, sub_00196050},
    {0x00196084u, sub_00196084},
    {0x00196090u, sub_00196090},
    {0x001960c4u, sub_001960C4},
    {0x001960d0u, sub_001960D0},
    {0x001960f0u, sub_001960F0},
    {0x00196410u, sub_00196410},
    {0x00196760u, sub_00196760},
    {0x0019677au, sub_0019677A},
    {0x00196789u, sub_00196789},
    {0x00196797u, sub_00196797},
    {0x00196b70u, sub_00196B70},
    {0x00196c90u, sub_00196C90},
    {0x00196ea0u, sub_00196EA0},
    {0x00196ebau, sub_00196EBA},
    {0x00196ec5u, sub_00196EC5},
    {0x001972f0u, sub_001972F0},
    {0x00197307u, sub_00197307},
    {0x00196110u, sub_00196110},
    {0x001a9610u, sub_001A9610},
    {QUERY_A_TARGET, query_a},
    {QUERY_B_TARGET, query_b},
    {LOOP_CALLBACK_TARGET, loop_callback},
    {FINAL_CALLBACK_TARGET, final_callback},
    {ACTIVATE_CALLBACK_TARGET, activate_callback},
    {FALLBACK_CALLBACK_TARGET, fallback_callback},
    {DEEP_CALLBACK_TARGET, deep_callback},
    {STATE_TAIL_CALLBACK_TARGET, state_tail_callback},
    {STATE_2_PREPARE_TARGET, state_2_prepare_callback},
    {STATE_2_COMMIT_TARGET, state_2_commit_callback},
    {STATE_QUERY_TARGET, state_query_callback},
    {STATE_RELEASE_TARGET, state_release_callback},
    {GROWTH_18_TARGET, growth_18_callback},
    {GROWTH_1C_TARGET, growth_1c_callback},
    {GROWTH_20_TARGET, growth_20_callback},
    {STATE_4_PREPARE_TARGET, state_4_prepare_callback},
    {STATE_4_COMMIT_TARGET, state_4_commit_callback},
    {ED80_BEGIN_TARGET, ed80_begin_callback},
    {ED80_END_TARGET, ed80_end_callback},
    {ED80_F010_METHOD20_TARGET, ed80_f010_method20_callback},
    {ED80_F010_METHOD1C_TARGET, ed80_f010_method1c_callback},
    {ED80_EFA6_QUERY_TARGET, ed80_efa6_query_callback},
    {ED80_LOG_TARGET, ed80_log_callback},
    {NOTIFY_TARGET, notify_callback},
    {NOTIFY_CALLER_TARGET, notify_caller_callback},
};

static int interaction_failed;
static size_t interaction_count;
static size_t failed_expected_index;
static size_t failed_actual_index;
static const char *failed_interaction_kind;
static uint32_t failed_interaction_esp;
static uint32_t failed_interaction_args[4];
static uint32_t interaction_stack_adjust;
static uint32_t deep_measure_result;
static uint32_t state_query_output;
static uint32_t state_query_result;
static int state_query_packet;
static int growth_state2;
static uint32_t ed80_callback_esp;
static int ed80_f010_computed_callbacks;
static int ed80_f010_two_items;
static int ed80_f010_null_item;
static int ed80_f010_literal_log;
static uint32_t ed80_log_expected_byte;
static uint32_t notify_caller_expected_message;

static void fill_window(uint8_t *bytes, uint32_t address, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = (uint8_t)(((address + (uint32_t)i) * 33u + 17u) &
                             0xffu);
    }
}

static void record_interaction_failure(
    const char *kind,
    size_t expected_index,
    uint32_t call_esp,
    uint32_t arg0,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3)
{
    if (interaction_failed) {
        return;
    }

    interaction_failed = 1;
    failed_interaction_kind = kind;
    failed_interaction_esp = call_esp;
    failed_expected_index = expected_index;
    failed_actual_index = interaction_count;
    failed_interaction_args[0] = arg0;
    failed_interaction_args[1] = arg1;
    failed_interaction_args[2] = arg2;
    failed_interaction_args[3] = arg3;
}

static void run_query(
    size_t expected_index,
    uint32_t expected_object,
    uint32_t expected_mode,
    uint32_t expected_output,
    uint32_t output_value)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);

    if (!interaction_failed &&
        (interaction_count != expected_index ||
         object != expected_object ||
         mode != expected_mode ||
         output != expected_output)) {
        record_interaction_failure(
            "query",
            expected_index,
            call_esp,
            object,
            mode,
            output,
            0u);
    }
    if (object == expected_object &&
        mode == expected_mode &&
        output == expected_output) {
        *recomp_memory_u32(output) = output_value;
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void query_a(void)
{
    if (interaction_count == 0u) {
        run_query(
            0u,
            INTERFACE_OBJECT,
            0u,
            OUTER_ENTRY_ESP - interaction_stack_adjust - 0x18u,
            0xabcdef01u);
    } else {
        run_query(
            3u,
            ITEM_INTERFACE_OBJECT,
            1u,
            OUTER_ENTRY_ESP - interaction_stack_adjust - 0x18u,
            0x11111111u);
    }
}

static void query_b(void)
{
    if (interaction_count == 1u) {
        run_query(
            1u,
            INTERFACE_OBJECT,
            1u,
            OUTER_ENTRY_ESP - interaction_stack_adjust - 0x10u,
            CALLER_OBJECT);
    } else {
        run_query(
            4u,
            ITEM_INTERFACE_OBJECT,
            0u,
            OUTER_ENTRY_ESP - interaction_stack_adjust - 0x10u,
            0x22222222u);
    }
}

static void loop_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t index = *recomp_memory_u32(call_esp + 8u);
    uint32_t first = *recomp_memory_u32(call_esp + 12u);
    uint32_t second = *recomp_memory_u32(call_esp + 16u);

    if (interaction_count != 2u ||
        context != LOOP_CONTEXT ||
        index != 0u ||
        first != 30u ||
        second != 14u) {
        record_interaction_failure(
            "loop callback",
            2u,
            call_esp,
            context,
            index,
            first,
            second);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void final_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t argument = *recomp_memory_u32(call_esp + 4u);

    if (interaction_count != 5u || argument != FINAL_ARGUMENT) {
        record_interaction_failure(
            "final callback",
            5u,
            call_esp,
            argument,
            0u,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void activate_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t enabled = *recomp_memory_u32(call_esp + 8u);

    if (interaction_count != 0u ||
        object != INTERFACE_OBJECT ||
        enabled != 1u) {
        record_interaction_failure(
            "activate callback",
            0u,
            call_esp,
            object,
            enabled,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void fallback_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t enabled = *recomp_memory_u32(call_esp + 8u);
    uint32_t third = *recomp_memory_u32(call_esp + 12u);
    uint32_t fourth = interaction_count == 0u
        ? *recomp_memory_u32(call_esp + 16u)
        : 0u;

    if ((interaction_count == 0u &&
         (object != INTERFACE_OBJECT ||
          enabled != 1u ||
          third != 0x7fffffffu ||
          fourth != OUTER_OBJECT + 0x14u)) ||
        (interaction_count == 1u &&
         (object != INTERFACE_OBJECT ||
          enabled != 1u ||
          third != OUTER_OBJECT + 0x14u)) ||
        interaction_count > 1u) {
        record_interaction_failure(
            "fallback callback",
            interaction_count < 2u ? interaction_count : 1u,
            call_esp,
            object,
            enabled,
            third,
            fourth);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void deep_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = interaction_count == 2u
        ? *recomp_memory_u32(call_esp + 12u)
        : 0u;

    if ((interaction_count == 1u &&
         (object != ITEM_INTERFACE_OBJECT || mode != 0u)) ||
        (interaction_count == 2u &&
         (object != INTERFACE_OBJECT ||
          mode != 1u ||
          output != OUTER_OBJECT + 0x14u)) ||
        interaction_count < 1u ||
        interaction_count > 2u) {
        record_interaction_failure(
            "deep callback",
            interaction_count < 2u ? 1u : 2u,
            call_esp,
            object,
            mode,
            output,
            0u);
    }

    recomp_runtime.registers.eax =
        interaction_count == 1u ? deep_measure_result : 0u;
    ++interaction_count;
    recomp_runtime.registers.esp += 4u;
}

static void state_tail_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);

    if (interaction_count != 3u || object != HELPER_OBJECT) {
        record_interaction_failure(
            "state tail callback",
            3u,
            call_esp,
            object,
            0u,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0x5a5a5a5au;
    recomp_runtime.registers.esp += 4u;
}

static void state_2_prepare_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t first = *recomp_memory_u32(call_esp + 8u);
    uint32_t second = *recomp_memory_u32(call_esp + 12u);
    uint32_t third = *recomp_memory_u32(call_esp + 16u);

    if (interaction_count != 0u ||
        context != STATE_2_PREPARE_CONTEXT ||
        first != HELPER_OBJECT + 0x68u ||
        second != HELPER_OBJECT + 0x6cu ||
        third != HELPER_OBJECT + 0x70u) {
        record_interaction_failure(
            "state 2 prepare callback",
            0u,
            call_esp,
            context,
            first,
            second,
            third);
    }

    *recomp_memory_u32(first) = 0u;
    *recomp_memory_u32(second) = 0u;
    *recomp_memory_u32(third) = 0u;
    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void state_2_commit_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t size = *recomp_memory_u32(call_esp + 8u);
    uint32_t count = *recomp_memory_u32(call_esp + 12u);

    if (interaction_count != 1u ||
        context != STATE_2_COMMIT_CONTEXT ||
        size != 0u ||
        count != 0u) {
        record_interaction_failure(
            "state 2 commit callback",
            1u,
            call_esp,
            context,
            size,
            count,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0x62626262u;
    recomp_runtime.registers.esp += 4u;
}

static void state_query_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t capacity = *recomp_memory_u32(call_esp + 12u);
    uint32_t output = *recomp_memory_u32(call_esp + 16u);

    if (interaction_count != 0u ||
        object != INTERFACE_OBJECT ||
        mode != 1u ||
        capacity != 0x1000u ||
        output != state_query_output) {
        record_interaction_failure(
            "state query callback",
            0u,
            call_esp,
            object,
            mode,
            capacity,
            output);
    }

    if (state_query_packet) {
        *recomp_memory_u32(output) = PARSER_PACKET_OBJECT;
    }
    *recomp_memory_u32(output + 4u) = state_query_result;
    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void state_release_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);

    if (interaction_count != 1u ||
        object != INTERFACE_OBJECT ||
        mode != 1u ||
        output != state_query_output) {
        record_interaction_failure(
            "state release callback",
            1u,
            call_esp,
            object,
            mode,
            output,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void growth_18_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t capacity = *recomp_memory_u32(call_esp + 12u);
    uint32_t output = *recomp_memory_u32(call_esp + 16u);
    uint32_t expected_output =
        interaction_count == 0u ? GROWTH_FRAME_ESP + 0x20u :
        interaction_count == 1u ? GROWTH_FRAME_ESP + 0x28u :
        GROWTH_QUERY_OUTPUT;
    uint32_t expected_capacity =
        interaction_count < 2u ? 0x7fffffffu : 0x1000u;

    if ((interaction_count != 0u &&
         interaction_count != 1u &&
         interaction_count != 5u) ||
        object != GROWTH_STREAM_OBJECT ||
        mode != 1u ||
        capacity != expected_capacity ||
        output != expected_output) {
        record_interaction_failure(
            "growth +18 callback",
            interaction_count,
            call_esp,
            object,
            mode,
            capacity,
            output);
    } else if (interaction_count == 0u) {
        *recomp_memory_u32(output) = GROWTH_PACKET_OBJECT;
        *recomp_memory_u32(output + 4u) = GROWTH_RESOURCE_OBJECT;
        *recomp_memory_u32(output + 8u) = GROWTH_STREAM_OBJECT;
        *recomp_memory_u32(output + 12u) = 4u;
    } else if (interaction_count == 5u) {
        if (growth_state2) {
            *recomp_memory_u32(output) =
                GROWTH_PARSER_PACKET_OBJECT;
        }
        *recomp_memory_u32(output + 4u) =
            growth_state2 ? 0x10u : 2u;
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void growth_20_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);
    uint32_t expected_output =
        interaction_count == 2u ?
        GROWTH_FRAME_ESP + 0x20u :
        GROWTH_FRAME_ESP + 0x28u;

    if ((interaction_count != 2u && interaction_count != 3u) ||
        object != GROWTH_STREAM_OBJECT ||
        mode != 0u ||
        output != expected_output) {
        record_interaction_failure(
            "growth +20 callback",
            interaction_count,
            call_esp,
            object,
            mode,
            output,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void growth_1c_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t object = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);
    uint32_t expected_output =
        interaction_count == 4u ?
        GROWTH_FRAME_ESP + 0x38u :
        GROWTH_QUERY_OUTPUT;

    if ((interaction_count != 4u && interaction_count != 6u) ||
        object != GROWTH_STREAM_OBJECT ||
        mode != 1u ||
        output != expected_output) {
        record_interaction_failure(
            "growth +1c callback",
            interaction_count,
            call_esp,
            object,
            mode,
            output,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void state_4_prepare_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t first = *recomp_memory_u32(call_esp + 8u);
    uint32_t second = *recomp_memory_u32(call_esp + 12u);
    uint32_t third = *recomp_memory_u32(call_esp + 16u);

    if (interaction_count != 0u ||
        context != STATE_2_PREPARE_CONTEXT ||
        first != HELPER_OBJECT + 0x68u ||
        second != HELPER_OBJECT + 0x6cu ||
        third != HELPER_OBJECT + 0x70u) {
        record_interaction_failure(
            "state 4 prepare callback",
            0u,
            call_esp,
            context,
            first,
            second,
            third);
    }

    *recomp_memory_u32(first) = 0u;
    *recomp_memory_u32(second) = 2u;
    *recomp_memory_u32(third) = 0u;
    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void state_4_commit_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t scaled_count = *recomp_memory_u32(call_esp + 8u);
    uint32_t count = *recomp_memory_u32(call_esp + 12u);

    if (interaction_count != 1u ||
        context != STATE_2_COMMIT_CONTEXT ||
        scaled_count != 2u ||
        count != 2u) {
        record_interaction_failure(
            "state 4 commit callback",
            1u,
            call_esp,
            context,
            scaled_count,
            count,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0x74747474u;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_begin_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    size_t expected_index = ed80_f010_computed_callbacks &&
            interaction_count != 0u ?
        4u :
        0u;
    uint32_t expected_esp = ed80_f010_computed_callbacks ?
        (expected_index == 0u ?
            ED80_ENTRY_ESP - 0x78u :
            ED80_ENTRY_ESP - 0x38u) :
        ed80_callback_esp;

    if (interaction_count != expected_index ||
        call_esp != expected_esp ||
        context != ED80_BEGIN_CONTEXT) {
        record_interaction_failure(
            "ED80 begin callback",
            expected_index,
            call_esp,
            context,
            0u,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0xababababu;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_end_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    size_t expected_index = ed80_f010_computed_callbacks &&
            interaction_count != 3u ?
        5u :
        (ed80_f010_computed_callbacks ? 3u : 1u);
    uint32_t expected_esp = ed80_f010_computed_callbacks ?
        (expected_index == 3u ?
            ED80_ENTRY_ESP - 0x78u :
            ED80_ENTRY_ESP - 0x38u) :
        ed80_callback_esp;

    if (interaction_count != expected_index ||
        call_esp != expected_esp ||
        context != ED80_END_CONTEXT) {
        record_interaction_failure(
            "ED80 end callback",
            expected_index,
            call_esp,
            context,
            0u,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0xcdcdcdcdu;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_f010_method20_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t item = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);
    size_t expected_index = ed80_f010_computed_callbacks ?
        1u :
        ((ed80_f010_null_item || ed80_f010_literal_log) ? 1u :
         (ed80_f010_two_items && interaction_count != 0u ? 2u : 0u));

    if (interaction_count != expected_index ||
        call_esp != ED80_ENTRY_ESP - 0x88u ||
        item != ITEM_INTERFACE_OBJECT ||
        mode != 0u ||
        output != ED80_ENTRY_ESP - 0x48u) {
        record_interaction_failure(
            "ED80 F010 method20 callback",
            expected_index,
            call_esp,
            item,
            mode,
            output,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_f010_method1c_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t item = *recomp_memory_u32(call_esp + 4u);
    uint32_t mode = *recomp_memory_u32(call_esp + 8u);
    uint32_t output = *recomp_memory_u32(call_esp + 12u);
    size_t expected_index = ed80_f010_computed_callbacks ?
        2u :
        ((ed80_f010_null_item || ed80_f010_literal_log) ? 2u :
         (ed80_f010_two_items && interaction_count != 1u ? 3u : 1u));

    if (interaction_count != expected_index ||
        call_esp != ED80_ENTRY_ESP - 0x94u ||
        item != ITEM_INTERFACE_OBJECT ||
        mode != 1u ||
        output != ED80_ENTRY_ESP - 0x40u) {
        record_interaction_failure(
            "ED80 F010 method1c callback",
            expected_index,
            call_esp,
            item,
            mode,
            output,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_efa6_query_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t array = *recomp_memory_u32(call_esp + 4u);
    uint32_t index = *recomp_memory_u32(call_esp + 8u);

    if (interaction_count > 1u ||
        call_esp != ED80_ENTRY_ESP - 0x60u ||
        array != ED80_EFA6_ARRAY ||
        index != interaction_count) {
        record_interaction_failure(
            "ED80 EFA6 query callback",
            interaction_count < 2u ? interaction_count : 1u,
            call_esp,
            array,
            index,
            0u,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 1u;
    recomp_runtime.registers.esp += 4u;
}

static void ed80_log_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t buffer = *recomp_memory_u32(call_esp + 8u);

    if (interaction_count != 0u ||
        call_esp != ED80_ENTRY_ESP - 0x7cu ||
        context != ED80_LOG_CONTEXT ||
        buffer != ED80_LOG_BUFFER ||
        (uint32_t)(uint8_t)*recomp_memory_i8(buffer) !=
            ed80_log_expected_byte) {
        record_interaction_failure(
            "ED80 log callback",
            0u,
            call_esp,
            context,
            buffer,
            (uint32_t)(uint8_t)*recomp_memory_i8(buffer),
            0u);
    }

    *recomp_memory_u32(ED80_RECORD + 0x10u) = ITEM_INTERFACE_OBJECT;
    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void notify_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t arg1 = *recomp_memory_u32(call_esp + 8u);
    uint32_t arg0 = *recomp_memory_u32(call_esp + 12u);

    if (interaction_count != 0u ||
        call_esp != NOTIFY_ENTRY_ESP - 0x10u ||
        context != NOTIFY_CONTEXT ||
        arg1 != NOTIFY_ARG1 ||
        arg0 != NOTIFY_ARG0) {
        record_interaction_failure(
            "191180 notify callback",
            0u,
            call_esp,
            context,
            arg1,
            arg0,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static void notify_caller_callback(void)
{
    uint32_t call_esp = recomp_runtime.registers.esp;
    uint32_t context = *recomp_memory_u32(call_esp + 4u);
    uint32_t message = *recomp_memory_u32(call_esp + 8u);
    uint32_t value = *recomp_memory_u32(call_esp + 12u);

    if (interaction_count != 0u ||
        call_esp != NOTIFY_CALLER_ENTRY_ESP - 0x1cu ||
        context != NOTIFY_CONTEXT ||
        message != notify_caller_expected_message ||
        value != 0u) {
        record_interaction_failure(
            "19132e notify callback",
            0u,
            call_esp,
            context,
            message,
            value,
            0u);
    }

    ++interaction_count;
    recomp_runtime.registers.eax = 0u;
    recomp_runtime.registers.esp += 4u;
}

static int expect_registers(
    const char *case_name,
    const RecompRegisters *actual,
    const RecompRegisters *expected)
{
    static const char *const names[] = {
        "eax", "ecx", "edx", "ebx", "esi", "edi", "ebp", "esp",
    };
    const uint32_t actual_values[] = {
        actual->eax, actual->ecx, actual->edx, actual->ebx,
        actual->esi, actual->edi, actual->ebp, actual->esp,
    };
    const uint32_t expected_values[] = {
        expected->eax, expected->ecx, expected->edx, expected->ebx,
        expected->esi, expected->edi, expected->ebp, expected->esp,
    };

    for (size_t i = 0; i < ARRAY_SIZE(names); ++i) {
        if (actual_values[i] != expected_values[i]) {
            fprintf(
                stderr,
                "%s: first divergence register %s was 0x%08x, "
                "expected 0x%08x\n",
                case_name,
                names[i],
                actual_values[i],
                expected_values[i]);
            return 0;
        }
    }

    return 1;
}

static int expect_u32(
    const char *case_name,
    const char *field,
    uint32_t actual,
    uint32_t expected)
{
    if (actual == expected) {
        return 1;
    }

    fprintf(
        stderr,
        "%s: first divergence %s was 0x%08x, expected 0x%08x\n",
        case_name,
        field,
        actual,
        expected);
    return 0;
}

static int expect_bytes(
    const char *case_name,
    const char *window_name,
    const void *actual,
    const void *expected,
    size_t size)
{
    const uint8_t *actual_bytes = actual;
    const uint8_t *expected_bytes = expected;

    for (size_t i = 0; i < size; ++i) {
        if (actual_bytes[i] != expected_bytes[i]) {
            fprintf(
                stderr,
                "%s: first divergence %s+0x%zx was 0x%02x, "
                "expected 0x%02x\n",
                case_name,
                window_name,
                i,
                actual_bytes[i],
                expected_bytes[i]);
            return 0;
        }
    }

    return 1;
}

static int run_fixture(const BoundaryFixture *fixture)
{
    uint32_t stack[STACK_SIZE / sizeof(uint32_t)];
    uint32_t caller[CALLER_SIZE / sizeof(uint32_t)];
    uint32_t callee[CALLEE_SIZE / sizeof(uint32_t)];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t expected_caller[ARRAY_SIZE(caller)];
    uint32_t expected_callee[ARRAY_SIZE(callee)];
    RecompMemoryAccess accesses[16];
    RecompMemoryRegion regions[] = {
        {STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {CALLER_OBJECT, sizeof(caller), (uint8_t *)caller},
        {CALLEE_OBJECT, sizeof(callee), (uint8_t *)callee},
    };
    RecompRegisters expected_registers = fixture->initial;

    fill_window((uint8_t *)stack, STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)caller, CALLER_OBJECT, sizeof(caller));
    fill_window((uint8_t *)callee, CALLEE_OBJECT, sizeof(callee));
    stack[(ENTRY_ESP + 4u - STACK_BASE) / 4u] = CALLER_OBJECT;
    caller[4u / 4u] = fixture->caller_state;
    caller[8u / 4u] = CALLEE_OBJECT;
    caller[0x8cu / 4u] = 0xfeedbeefu;
    callee[0x0cu / 4u] = fixture->callee_state;
    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_caller, caller, sizeof(caller));
    memcpy(expected_callee, callee, sizeof(callee));

    expected_stack[(ENTRY_ESP - 4u - STACK_BASE) / 4u] =
        fixture->initial.esi;
    if (fixture->calls_callee) {
        expected_stack[(ENTRY_ESP - 8u - STACK_BASE) / 4u] = CALLEE_OBJECT;
        expected_stack[(ENTRY_ESP - 12u - STACK_BASE) / 4u] = 0u;
        expected_caller[0x8cu / 4u] = 0u;
        expected_caller[4u / 4u] = 0u;
        expected_registers.eax = 0u;
    }
    if (fixture->callee_writes) {
        expected_callee[0x0cu / 4u] = 0u;
    }
    expected_registers.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = fixture->initial;

    if (!recomp_dispatch(0x00194d10u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", fixture->name);
        return 0;
    }

    if (!expect_registers(
            fixture->name,
            &recomp_runtime.registers,
            &expected_registers) ||
        !expect_bytes(
            fixture->name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            fixture->name,
            "caller",
            caller,
            expected_caller,
            sizeof(caller)) ||
        !expect_bytes(
            fixture->name,
            "callee",
            callee,
            expected_callee,
            sizeof(callee))) {
        return 0;
    }

    if (recomp_runtime.access_count != fixture->expected_access_count) {
        fprintf(
            stderr,
            "%s: first divergence access_count was %zu, expected %zu\n",
            fixture->name,
            recomp_runtime.access_count,
            fixture->expected_access_count);
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            fixture->name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_outer_fixture(
    uint32_t entry,
    int optional_calls,
    int zero_path)
{
    const int from_dispatcher =
        entry == 0x0018df30u || entry == 0x0018df90u;
    const int from_state_caller = entry == 0x0018df90u;
    const int zero_state = zero_path != 0;
    const int fallback_state = zero_path == 2;
    const int deep_cleanup = zero_path == 3;
    const int deep_full = zero_path == 4;
    const int state_b_null = zero_path == 5;
    const int state_b_tail = zero_path == 6;
    const int state_b = state_b_null || state_b_tail;
    const int state_14_null = zero_path == 7;
    const int state_14_tail = zero_path == 8;
    const int state_14 = state_14_null || state_14_tail;
    const int state_a_tail = zero_path == 9;
    const int state_1_skip = zero_path == 10;
    const int state_2_busy = zero_path == 11;
    const int state_3_secondary_1 = zero_path == 12;
    const int state_3_secondary_0 = zero_path == 13;
    const int state_4_secondary_1 = zero_path == 14;
    const int state_4_secondary_0 = zero_path == 15;
    const int state_0_idle = zero_path == 16;
    const int state_0_busy = zero_path == 17;
    const int state_0 = state_0_idle || state_0_busy;
    const int secondary_1 =
        state_3_secondary_1 || state_4_secondary_1;
    const int state_3 =
        state_3_secondary_1 || state_3_secondary_0;
    const int state_4 =
        state_4_secondary_1 || state_4_secondary_0;
    const int secondary_state = state_3 || state_4;
    const int recognized_state =
        state_0_busy || state_1_skip || state_2_busy ||
        state_a_tail || state_b || state_14;
    const int deep_state =
        deep_cleanup || deep_full || recognized_state ||
        secondary_state || state_0_idle;
    const char *case_name = state_0_busy
        ? "next-caller-state-0-busy"
        : state_0_idle
        ? "next-caller-state-0-idle"
        : state_4_secondary_0
        ? "next-caller-state-4-secondary-0"
        : state_4_secondary_1
        ? "next-caller-state-4-secondary-1"
        : state_3_secondary_0
        ? "next-caller-state-3-secondary-0"
        : state_3_secondary_1
        ? "next-caller-state-3-secondary-1"
        : state_2_busy
        ? "next-caller-state-2-busy"
        : state_1_skip
        ? "next-caller-state-1-secondary-skip"
        : state_a_tail
        ? "next-caller-state-a-computed-tail"
        : state_14_tail
        ? "next-caller-state-14-computed-tail"
        : state_14_null
        ? "next-caller-state-14-null-tail"
        : state_b_tail
        ? "next-caller-state-b-computed-tail"
        : state_b_null
        ? "next-caller-state-b-null-tail"
        : deep_full
        ? "next-caller-zero-state-deep-complete"
        : deep_cleanup
        ? "next-caller-zero-state-deep-cleanup"
        : fallback_state
        ? "next-caller-zero-state-fallback"
        : zero_state
        ? "next-caller-zero-state-computed-call"
        : from_dispatcher
        ? "next-caller-all-computed-calls"
        : optional_calls
        ? "outer-caller-all-computed-calls"
        : "outer-caller-runtime-interaction";
    uint32_t stack[OUTER_STACK_SIZE / sizeof(uint32_t)];
    uint32_t outer[OUTER_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t interface_object[1];
    uint32_t vtable[0x28u / sizeof(uint32_t)];
    uint32_t item_interface[1];
    uint32_t item_vtable[0x28u / sizeof(uint32_t)];
    uint32_t state_14_tail_slot[1];
    uint32_t state_b_tail_slot[1];
    uint32_t state_a_tail_slot[1];
    uint32_t caller[CALLER_SIZE / sizeof(uint32_t)];
    uint32_t callee[CALLEE_SIZE / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_outer[ARRAY_SIZE(outer)];
    uint32_t expected_helper[ARRAY_SIZE(helper)];
    uint32_t interface_before[ARRAY_SIZE(interface_object)];
    uint32_t vtable_before[ARRAY_SIZE(vtable)];
    uint32_t item_interface_before[ARRAY_SIZE(item_interface)];
    uint32_t item_vtable_before[ARRAY_SIZE(item_vtable)];
    uint32_t state_14_tail_slot_before[ARRAY_SIZE(state_14_tail_slot)];
    uint32_t state_b_tail_slot_before[ARRAY_SIZE(state_b_tail_slot)];
    uint32_t state_a_tail_slot_before[ARRAY_SIZE(state_a_tail_slot)];
    uint32_t expected_caller[ARRAY_SIZE(caller)];
    uint32_t expected_callee[ARRAY_SIZE(callee)];
    RecompMemoryAccess accesses[256];
    RecompMemoryRegion regions[] = {
        {OUTER_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {OUTER_OBJECT, sizeof(outer), (uint8_t *)outer},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {
            INTERFACE_OBJECT,
            sizeof(interface_object),
            (uint8_t *)interface_object,
        },
        {VTABLE_OBJECT, sizeof(vtable), (uint8_t *)vtable},
        {
            ITEM_INTERFACE_OBJECT,
            sizeof(item_interface),
            (uint8_t *)item_interface,
        },
        {
            ITEM_VTABLE_OBJECT,
            sizeof(item_vtable),
            (uint8_t *)item_vtable,
        },
        {
            STATE_14_TAIL_SLOT,
            sizeof(state_14_tail_slot),
            (uint8_t *)state_14_tail_slot,
        },
        {
            STATE_B_TAIL_SLOT,
            sizeof(state_b_tail_slot),
            (uint8_t *)state_b_tail_slot,
        },
        {
            STATE_A_TAIL_SLOT,
            sizeof(state_a_tail_slot),
            (uint8_t *)state_a_tail_slot,
        },
        {CALLER_OBJECT, sizeof(caller), (uint8_t *)caller},
        {CALLEE_OBJECT, sizeof(callee), (uint8_t *)callee},
    };
    const RecompRegisters initial = {
        0x81818181u,
        0x82828282u,
        0x83838383u,
        0x84848484u,
        0x85858585u,
        0x86868686u,
        0x87878787u,
        OUTER_ENTRY_ESP,
    };
    RecompRegisters expected_registers = initial;
    const size_t expected_interactions =
        state_0_busy ? 2u :
        state_0_idle ? 3u :
        secondary_state ? 3u :
        state_2_busy ? 2u :
        state_1_skip ? 2u :
        state_a_tail ? 4u :
        state_14_tail ? 4u :
        state_14_null ? 3u :
        state_b_tail ? 4u :
        state_b_null ? 3u :
        deep_full ? 2u :
        deep_cleanup ? 3u :
        fallback_state ? 2u :
        zero_state ? 1u :
        optional_calls ? 6u : 2u;
    const size_t frame_start =
        (OUTER_ENTRY_ESP - (from_dispatcher ? 0x90u : 0x80u)) -
        OUTER_STACK_BASE;
    const size_t incoming_start = OUTER_ENTRY_ESP - OUTER_STACK_BASE;

    fill_window((uint8_t *)stack, OUTER_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)outer, OUTER_OBJECT, sizeof(outer));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window(
        (uint8_t *)interface_object,
        INTERFACE_OBJECT,
        sizeof(interface_object));
    fill_window((uint8_t *)vtable, VTABLE_OBJECT, sizeof(vtable));
    fill_window(
        (uint8_t *)item_interface,
        ITEM_INTERFACE_OBJECT,
        sizeof(item_interface));
    fill_window(
        (uint8_t *)item_vtable,
        ITEM_VTABLE_OBJECT,
        sizeof(item_vtable));
    fill_window(
        (uint8_t *)state_14_tail_slot,
        STATE_14_TAIL_SLOT,
        sizeof(state_14_tail_slot));
    fill_window(
        (uint8_t *)state_b_tail_slot,
        STATE_B_TAIL_SLOT,
        sizeof(state_b_tail_slot));
    fill_window(
        (uint8_t *)state_a_tail_slot,
        STATE_A_TAIL_SLOT,
        sizeof(state_a_tail_slot));
    fill_window((uint8_t *)caller, CALLER_OBJECT, sizeof(caller));
    fill_window((uint8_t *)callee, CALLEE_OBJECT, sizeof(callee));

    stack[(OUTER_ENTRY_ESP - OUTER_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(OUTER_ENTRY_ESP + 4u - OUTER_STACK_BASE) / 4u] = OUTER_OBJECT;
    stack[(OUTER_ENTRY_ESP + 8u - OUTER_STACK_BASE) / 4u] = CALLER_OBJECT;
    outer[4u / 4u] = HELPER_OBJECT;
    outer[8u / 4u] = INTERFACE_OBJECT;
    outer[0x0cu / 4u] = ITEM_INTERFACE_OBJECT;
    outer[0x14u / 4u] = 10u;
    outer[0x18u / 4u] = state_0 ? 1u : 20u;
    outer[0x1cu / 4u] = 30u;
    outer[0x20u / 4u] = 20u;
    outer[0x2cu / 4u] = 1u;
    outer[0x30u / 4u] = 2u;
    outer[0x34u / 4u] = fallback_state ? 100u : 90u;
    outer[0x3cu / 4u] = optional_calls ? 10u : 0xffffffffu;
    outer[0x40u / 4u] = 3u;
    outer[0x44u / 4u] = 4u;
    outer[0x48u / 4u] = optional_calls ? FINAL_CALLBACK_TARGET : 0u;
    outer[0x4cu / 4u] = FINAL_ARGUMENT;
    outer[0x50u / 4u] = optional_calls ? LOOP_CALLBACK_TARGET : 0u;
    outer[0x54u / 4u] = LOOP_CONTEXT;
    ((uint8_t *)outer)[3u] =
        (fallback_state || deep_state) ? 0u : 1u;
    if (from_state_caller) {
        ((uint8_t *)outer)[1u] = 2u;
    }
    ((uint8_t *)helper)[0x0eu] = optional_calls ? 1u : 0u;
    if (deep_full || recognized_state) {
        ((uint8_t *)helper)[0x0du] = 8u;
        ((uint8_t *)helper)[0x0eu] = 2u;
    }
    if (state_0_busy) {
        ((uint8_t *)helper)[0x0fu] = 2u;
    }
    helper[0x18u / 4u] = 100u;
    helper[0x10u / 4u] = 10u;
    helper[0x90u / 4u] = 7u;
    helper[0x94u / 4u] = 5u;
    helper[0x98u / 4u] =
        (secondary_state ? (secondary_1 ? 1u << 16 : 0u) :
         state_1_skip ? 3u << 16 :
         helper[0x98u / 4u] & 0xffff0000u) |
        (state_a_tail ? STATE_A_DISPATCH_VALUE :
         state_14 ? STATE_14_DISPATCH_VALUE :
         state_b ? STATE_B_DISPATCH_VALUE :
         state_4 ? STATE_4_DISPATCH_VALUE :
         state_3 ? STATE_3_DISPATCH_VALUE :
         state_2_busy ? STATE_2_DISPATCH_VALUE :
         state_1_skip ? 1u :
         state_0 ? 0u : STATE_DISPATCH_VALUE);
    helper[0xb0u / 4u] = state_b_tail ? 1u : 0u;
    helper[0xbcu / 4u] = state_14_tail ? 1u : 0u;
    helper[4u / 4u] = zero_state ? 0u : 3u;
    helper[8u / 4u] = CALLEE_OBJECT;
    helper[0x8cu / 4u] = 0xfeedbeefu;
    interface_object[0] = VTABLE_OBJECT;
    vtable[0x18u / 4u] = FALLBACK_CALLBACK_TARGET;
    vtable[0x1cu / 4u] =
        deep_state ? DEEP_CALLBACK_TARGET :
        fallback_state ? FALLBACK_CALLBACK_TARGET :
        QUERY_B_TARGET;
    vtable[0x20u / 4u] = QUERY_A_TARGET;
    vtable[0x24u / 4u] = ACTIVATE_CALLBACK_TARGET;
    item_interface[0] = ITEM_VTABLE_OBJECT;
    item_vtable[0x1cu / 4u] = QUERY_B_TARGET;
    item_vtable[0x20u / 4u] = QUERY_A_TARGET;
    item_vtable[0x24u / 4u] = DEEP_CALLBACK_TARGET;
    state_14_tail_slot[0] = STATE_TAIL_CALLBACK_TARGET;
    state_b_tail_slot[0] = STATE_TAIL_CALLBACK_TARGET;
    state_a_tail_slot[0] = STATE_TAIL_CALLBACK_TARGET;
    caller[4u / 4u] = 3u;
    caller[8u / 4u] = CALLEE_OBJECT;
    caller[0x8cu / 4u] = 0xfeedbeefu;
    callee[0x0cu / 4u] = 3u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_outer, outer, sizeof(outer));
    memcpy(expected_helper, helper, sizeof(helper));
    memcpy(interface_before, interface_object, sizeof(interface_object));
    memcpy(vtable_before, vtable, sizeof(vtable));
    memcpy(item_interface_before, item_interface, sizeof(item_interface));
    memcpy(item_vtable_before, item_vtable, sizeof(item_vtable));
    memcpy(
        state_14_tail_slot_before,
        state_14_tail_slot,
        sizeof(state_14_tail_slot));
    memcpy(
        state_b_tail_slot_before,
        state_b_tail_slot,
        sizeof(state_b_tail_slot));
    memcpy(
        state_a_tail_slot_before,
        state_a_tail_slot,
        sizeof(state_a_tail_slot));
    memcpy(expected_caller, caller, sizeof(caller));
    memcpy(expected_callee, callee, sizeof(callee));
    if (zero_state && !deep_state) {
        ((uint8_t *)expected_outer)[1u] = 3u;
        expected_registers.eax = 0u;
        expected_registers.ecx = HELPER_OBJECT;
    } else if (deep_full || recognized_state) {
        expected_helper[4u / 4u] = 1u;
        expected_helper[0x48u / 4u] = 10u;
        expected_helper[0x4cu / 4u] = state_0_busy ? 0u : 10u;
        expected_helper[0x74u / 4u] = 0u;
        expected_helper[0x90u / 4u] = 0u;
        expected_helper[0x94u / 4u] = 0u;
        expected_registers.eax = (state_a_tail || state_14) ? 0u : 1u;
        expected_registers.ecx = state_0_busy ? HELPER_OBJECT :
            (state_a_tail || state_14) ? 2u :
            state_b_tail ? 1u :
            state_b_null ? 0u : HELPER_OBJECT;
        expected_registers.edx = state_0_busy ? 1u :
            (state_a_tail || state_14) ? 90u : 0u;
    } else if (deep_cleanup || secondary_state || state_0_idle) {
        expected_registers.eax = 0u;
        expected_registers.ecx = HELPER_OBJECT;
        expected_registers.edx = VTABLE_OBJECT;
    } else {
        expected_outer[0x2cu / 4u] = 8u;
        expected_outer[0x30u / 4u] = 7u;
        expected_outer[0x34u / 4u] = 97u;
        expected_outer[0x40u / 4u] = 10u;
        expected_outer[0x44u / 4u] = 9u;
        expected_helper[4u / 4u] = 0u;
        expected_helper[0x8cu / 4u] = 0u;
        expected_callee[0x0cu / 4u] = 0u;
        expected_registers.eax = 0u;
        expected_registers.ecx =
            optional_calls ? FINAL_ARGUMENT : 10u;
        expected_registers.edx = HELPER_OBJECT;
    }
    expected_registers.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    interaction_stack_adjust = from_dispatcher ? 0x10u : 0u;
    deep_measure_result =
        (deep_full || recognized_state) ? 30u : 10u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(entry)) {
        fprintf(stderr, "%s: outer caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: args "
            "0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count != expected_interactions) {
        fprintf(
            stderr,
            "%s: first divergence interaction_count was %zu, expected %zu\n",
            case_name,
            interaction_count,
            expected_interactions);
        return 0;
    }
    if (from_dispatcher) {
        if (!expect_u32(
                case_name,
                "saved esi",
                stack[(OUTER_ENTRY_ESP - 4u - OUTER_STACK_BASE) / 4u],
                initial.esi) ||
            !expect_u32(
                case_name,
                "saved edi",
                stack[(OUTER_ENTRY_ESP - 8u - OUTER_STACK_BASE) / 4u],
                initial.edi)) {
            return 0;
        }
        if (zero_state) {
            if (!expect_u32(
                    case_name,
                    "nested saved ebp",
                    stack[
                        (OUTER_ENTRY_ESP - 0x18u - OUTER_STACK_BASE) /
                        4u],
                    state_0_busy
                        ? CALLEE_OBJECT
                        : (state_2_busy || secondary_state)
                        ? HELPER_OBJECT
                        : initial.ebp) ||
                !expect_u32(
                    case_name,
                    "nested saved esi",
                    stack[
                        (OUTER_ENTRY_ESP - 0x1cu - OUTER_STACK_BASE) /
                        4u],
                    state_0_busy
                        ? 0u
                        : state_2_busy
                        ? CALLEE_OBJECT
                        : state_a_tail
                        ? (HELPER_OBJECT & 0xffff0000u) |
                              STATE_A_DISPATCH_VALUE
                        : state_14
                        ? (HELPER_OBJECT & 0xffff0000u) |
                              STATE_14_DISPATCH_VALUE
                        : HELPER_OBJECT)) {
                return 0;
            }
        } else if (!expect_u32(
                case_name,
                "nested saved ebp",
                stack[(OUTER_ENTRY_ESP - 0x14u - OUTER_STACK_BASE) / 4u],
                initial.ebp)) {
            return 0;
        }
    } else if (!expect_u32(
                   case_name,
                   "saved ebp",
                   stack[(OUTER_ENTRY_ESP - 4u - OUTER_STACK_BASE) / 4u],
                   initial.ebp)) {
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected_registers) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "outer",
            outer,
            expected_outer,
            sizeof(outer)) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            expected_helper,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "interface",
            interface_object,
            interface_before,
            sizeof(interface_object)) ||
        !expect_bytes(
            case_name,
            "vtable",
            vtable,
            vtable_before,
            sizeof(vtable)) ||
        !expect_bytes(
            case_name,
            "item interface",
            item_interface,
            item_interface_before,
            sizeof(item_interface)) ||
        !expect_bytes(
            case_name,
            "item vtable",
            item_vtable,
            item_vtable_before,
            sizeof(item_vtable)) ||
        !expect_bytes(
            case_name,
            "state 14 tail slot",
            state_14_tail_slot,
            state_14_tail_slot_before,
            sizeof(state_14_tail_slot)) ||
        !expect_bytes(
            case_name,
            "state B tail slot",
            state_b_tail_slot,
            state_b_tail_slot_before,
            sizeof(state_b_tail_slot)) ||
        !expect_bytes(
            case_name,
            "state A tail slot",
            state_a_tail_slot,
            state_a_tail_slot_before,
            sizeof(state_a_tail_slot)) ||
        !expect_bytes(
            case_name,
            "caller",
            caller,
            expected_caller,
            sizeof(caller)) ||
        !expect_bytes(
            case_name,
            "callee",
            callee,
            expected_callee,
            sizeof(callee))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_2_computed_fixture(
    uint32_t entry,
    const char *case_name,
    uint16_t primary_state,
    uint16_t secondary_state)
{
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t callee[CALLEE_SIZE / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_helper[ARRAY_SIZE(helper)];
    uint32_t callee_before[ARRAY_SIZE(callee)];
    RecompMemoryAccess accesses[128];
    RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {CALLEE_OBJECT, sizeof(callee), (uint8_t *)callee},
    };
    const RecompRegisters initial = {
        0x91919191u,
        0x92929292u,
        0x93939393u,
        0x94949494u,
        0x95959595u,
        0x96969696u,
        0x97979797u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        STATE_ENTRY_ESP - 36u - STATE_STACK_BASE;
    const size_t incoming_start =
        STATE_ENTRY_ESP - STATE_STACK_BASE;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window((uint8_t *)callee, CALLEE_OBJECT, sizeof(callee));

    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        HELPER_OBJECT;
    helper[4u / 4u] = 1u;
    helper[8u / 4u] = CALLEE_OBJECT;
    helper[0x60u / 4u] = 0u;
    helper[0x78u / 4u] = STATE_2_PREPARE_TARGET;
    helper[0x7cu / 4u] = STATE_2_PREPARE_CONTEXT;
    helper[0x80u / 4u] = STATE_2_COMMIT_TARGET;
    helper[0x84u / 4u] = STATE_2_COMMIT_CONTEXT;
    helper[0x90u / 4u] = 7u;
    helper[0x94u / 4u] = 5u;
    ((uint8_t *)helper)[0x0eu] = 2u;
    ((uint16_t *)(void *)helper)[0x98u / 2u] = primary_state;
    ((uint16_t *)(void *)helper)[0x9au / 2u] = secondary_state;
    callee[0x0cu / 4u] = 0u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_helper, helper, sizeof(helper));
    memcpy(callee_before, callee, sizeof(callee));
    expected_helper[4u / 4u] = 3u;
    expected_helper[0x68u / 4u] = 0u;
    expected_helper[0x6cu / 4u] = 0u;
    expected_helper[0x70u / 4u] = 0u;
    expected_helper[0x90u / 4u] = 0u;
    expected_helper[0x94u / 4u] = 0u;
    expected.eax = 0x62626262u;
    expected.ecx = STATE_2_COMMIT_CONTEXT;
    expected.edx = 0u;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    interaction_stack_adjust = 0u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(entry)) {
        fprintf(stderr, "%s: handler dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: "
            "args 0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count != 2u) {
        fprintf(
            stderr,
            "%s: first divergence interaction_count was %zu, expected 2\n",
            case_name,
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            expected_helper,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "callee",
            callee,
            callee_before,
            sizeof(callee))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_0_setup_fixture(void)
{
    const char *case_name = "state-0-object-setup";
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t callee[CALLEE_SIZE / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t helper_before[ARRAY_SIZE(helper)];
    uint32_t expected_callee[ARRAY_SIZE(callee)];
    RecompMemoryAccess accesses[128];
    RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {CALLEE_OBJECT, sizeof(callee), (uint8_t *)callee},
    };
    const RecompRegisters initial = {
        0xa1a1a1a1u,
        0xa2a2a2a2u,
        0xa3a3a3a3u,
        0xa4a4a4a4u,
        0xa5a5a5a5u,
        0xa6a6a6a6u,
        0xa7a7a7a7u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        STATE_ENTRY_ESP - 52u - STATE_STACK_BASE;
    const size_t incoming_start =
        STATE_ENTRY_ESP - STATE_STACK_BASE;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window((uint8_t *)callee, CALLEE_OBJECT, sizeof(callee));

    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        HELPER_OBJECT;
    helper[8u / 4u] = CALLEE_OBJECT;
    helper[0x48u / 4u] = 0x01020304u;
    helper[0x4cu / 4u] = 4u;
    helper[0x50u / 4u] = 2u;
    helper[0x58u / 4u] = 1u;
    helper[0x5cu / 4u] = 0u;
    helper[0x60u / 4u] = 0u;
    helper[0x68u / 4u] = 0u;
    helper[0x6cu / 4u] = 0u;
    helper[0x70u / 4u] = 0u;
    callee[0x0cu / 4u] = 0u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(helper_before, helper, sizeof(helper));
    memcpy(expected_callee, callee, sizeof(callee));
    expected_callee[0x0cu / 4u] = 1u;
    expected_callee[0x10u / 4u] = 0u;
    expected_callee[0x14u / 4u] = 1u;
    expected_callee[0x18u / 4u] = 0x01020304u;
    expected_callee[0x1cu / 4u] = 0u;
    expected_callee[0x20u / 4u] = 0u;
    expected_callee[0x24u / 4u] = 0u;
    expected_stack[
        (STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] = 0u;
    expected.eax = CALLEE_OBJECT;
    expected.ecx = 0u;
    expected.edx = 0u;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x00194d60u)) {
        fprintf(stderr, "%s: setup dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)expected_stack + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            helper_before,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "callee",
            callee,
            expected_callee,
            sizeof(callee))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_0_finalize_fixture(void)
{
    const char *case_name = "state-0-object-finalize";
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t child[CALLEE_SIZE / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t expected_helper[ARRAY_SIZE(helper)];
    uint32_t child_before[ARRAY_SIZE(child)];
    RecompMemoryAccess accesses[128];
    RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {CALLEE_OBJECT, sizeof(child), (uint8_t *)child},
    };
    const RecompRegisters initial = {
        0xb1b1b1b1u,
        0xb2b2b2b2u,
        0xb3b3b3b3u,
        0xb4b4b4b4u,
        0xb5b5b5b5u,
        0xb6b6b6b6u,
        0xb7b7b7b7u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        STATE_ENTRY_ESP - 44u - STATE_STACK_BASE;
    const size_t incoming_start =
        STATE_ENTRY_ESP - STATE_STACK_BASE;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window((uint8_t *)child, CALLEE_OBJECT, sizeof(child));

    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        HELPER_OBJECT;
    helper[8u / 4u] = CALLEE_OBJECT;
    helper[0x40u / 4u] = 10u;
    helper[0x44u / 4u] = 0x11223344u;
    helper[0x50u / 4u] = 10u;
    helper[0x54u / 4u] = 3u;
    helper[0x58u / 4u] = 2u;
    helper[0x5cu / 4u] = 0x55667788u;
    helper[0x68u / 4u] = 2u;
    helper[0x70u / 4u] = 0u;
    child[0x10u / 4u] = 5u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_helper, helper, sizeof(helper));
    memcpy(child_before, child, sizeof(child));
    expected_stack[
        (STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] = 1u;
    expected_helper[0x90u / 4u] = 1u;
    expected_helper[0x94u / 4u] = 15u;
    expected.eax = 10u;
    expected.ecx = 15u;
    expected.edx = 0xffffffffu;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x00194ec0u)) {
        fprintf(stderr, "%s: finalize dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)expected_stack + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            expected_helper,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "child",
            child,
            child_before,
            sizeof(child))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_caller_skip_fixture(void)
{
    const char *case_name = "state-caller-skips-dispatch";
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t outer[OUTER_SIZE / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t outer_before[ARRAY_SIZE(outer)];
    RecompMemoryAccess accesses[16];
    RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {OUTER_OBJECT, sizeof(outer), (uint8_t *)outer},
    };
    const RecompRegisters initial = {
        0xc1c1c1c1u,
        0xc2c2c2c2u,
        0xc3c3c3c3u,
        0xc4c4c4c4u,
        0xc5c5c5c5u,
        0xc6c6c6c6u,
        0xc7c7c7c7u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)outer, OUTER_OBJECT, sizeof(outer));
    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        OUTER_OBJECT;
    ((uint8_t *)outer)[1u] = 0u;
    memcpy(stack_before, stack, sizeof(stack));
    memcpy(outer_before, outer, sizeof(outer));
    expected.eax &= 0xffffff00u;
    expected.ecx = OUTER_OBJECT;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018df90u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            stack_before,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "outer",
            outer,
            outer_before,
            sizeof(outer))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_caller_callbacks_fixture(void)
{
    const char *case_name = "state-caller-computed-callbacks";
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t outer[OUTER_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t interface_object[1];
    uint32_t vtable[0x20u / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t outer_before[ARRAY_SIZE(outer)];
    uint32_t helper_before[ARRAY_SIZE(helper)];
    uint32_t interface_before[ARRAY_SIZE(interface_object)];
    uint32_t vtable_before[ARRAY_SIZE(vtable)];
    RecompMemoryAccess accesses[128];
    RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {OUTER_OBJECT, sizeof(outer), (uint8_t *)outer},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {
            INTERFACE_OBJECT,
            sizeof(interface_object),
            (uint8_t *)interface_object,
        },
        {VTABLE_OBJECT, sizeof(vtable), (uint8_t *)vtable},
    };
    const RecompRegisters initial = {
        0xd1d1d1d1u,
        0xd2d2d2d2u,
        0xd3d3d3d3u,
        0xd4d4d4d4u,
        0xd5d5d5d5u,
        0xd6d6d6d6u,
        0xd7d7d7d7u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        STATE_ENTRY_ESP - 52u - STATE_STACK_BASE;
    const size_t incoming_start =
        STATE_ENTRY_ESP - STATE_STACK_BASE;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)outer, OUTER_OBJECT, sizeof(outer));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window(
        (uint8_t *)interface_object,
        INTERFACE_OBJECT,
        sizeof(interface_object));
    fill_window((uint8_t *)vtable, VTABLE_OBJECT, sizeof(vtable));

    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] = 0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        OUTER_OBJECT;
    ((uint8_t *)outer)[1u] = 1u;
    outer[4u / 4u] = HELPER_OBJECT;
    outer[8u / 4u] = INTERFACE_OBJECT;
    interface_object[0] = VTABLE_OBJECT;
    vtable[0x18u / 4u] = STATE_QUERY_TARGET;
    vtable[0x1cu / 4u] = STATE_RELEASE_TARGET;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(outer_before, outer, sizeof(outer));
    memcpy(helper_before, helper, sizeof(helper));
    memcpy(interface_before, interface_object, sizeof(interface_object));
    memcpy(vtable_before, vtable, sizeof(vtable));
    expected.eax = 0u;
    expected.ecx = STATE_ENTRY_ESP - 16u;
    expected.edx = VTABLE_OBJECT;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    state_query_output = STATE_ENTRY_ESP - 16u;
    state_query_result = 2u;
    state_query_packet = 0;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018df90u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: "
            "args 0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count != 2u) {
        fprintf(
            stderr,
            "%s: first divergence interaction_count was %zu, expected 2\n",
            case_name,
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "outer",
            outer,
            outer_before,
            sizeof(outer)) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            helper_before,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "interface",
            interface_object,
            interface_before,
            sizeof(interface_object)) ||
        !expect_bytes(
            case_name,
            "vtable",
            vtable,
            vtable_before,
            sizeof(vtable))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_caller_parser_fixture(void)
{
    const char *case_name = "state-caller-authenticated-parser";
    uint32_t stack[DEEP_STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t outer[STATE_CALLER_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t interface_object[1];
    uint32_t vtable[0x20u / sizeof(uint32_t)];
    uint32_t packet[0x10u / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_outer[ARRAY_SIZE(outer)];
    uint32_t expected_helper[ARRAY_SIZE(helper)];
    uint32_t interface_before[ARRAY_SIZE(interface_object)];
    uint32_t vtable_before[ARRAY_SIZE(vtable)];
    uint32_t packet_before[ARRAY_SIZE(packet)];
    RecompMemoryAccess accesses[256];
    RecompMemoryRegion regions[] = {
        {DEEP_STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {OUTER_OBJECT, sizeof(outer), (uint8_t *)outer},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {
            INTERFACE_OBJECT,
            sizeof(interface_object),
            (uint8_t *)interface_object,
        },
        {VTABLE_OBJECT, sizeof(vtable), (uint8_t *)vtable},
        {PARSER_PACKET_OBJECT, sizeof(packet), (uint8_t *)packet},
    };
    const RecompRegisters initial = {
        0xe1e1e1e1u,
        0xe2e2e2e2u,
        0xe3e3e3e3u,
        0xe4e4e4e4u,
        0xe5e5e5e5u,
        0xe6e6e6e6u,
        0xe7e7e7e7u,
        DEEP_STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        DEEP_STATE_ENTRY_ESP - 0xa8u - DEEP_STATE_STACK_BASE;
    const size_t incoming_start =
        DEEP_STATE_ENTRY_ESP - DEEP_STATE_STACK_BASE;
    const uint32_t parser_context = 0u;

    fill_window((uint8_t *)stack, DEEP_STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)outer, OUTER_OBJECT, sizeof(outer));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window(
        (uint8_t *)interface_object,
        INTERFACE_OBJECT,
        sizeof(interface_object));
    fill_window((uint8_t *)vtable, VTABLE_OBJECT, sizeof(vtable));
    packet[0] = 0x00000080u;
    packet[1] = 0x00000010u;
    packet[2] = 0u;
    packet[3] = 0u;

    stack[(DEEP_STATE_ENTRY_ESP - DEEP_STATE_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(DEEP_STATE_ENTRY_ESP + 4u - DEEP_STATE_STACK_BASE) / 4u] =
        OUTER_OBJECT;
    ((uint8_t *)outer)[1u] = 1u;
    outer[4u / 4u] = HELPER_OBJECT;
    outer[8u / 4u] = INTERFACE_OBJECT;
    helper[0xa4u / 4u] = 1u;
    interface_object[0] = VTABLE_OBJECT;
    vtable[0x18u / 4u] = STATE_QUERY_TARGET;
    vtable[0x1cu / 4u] = STATE_RELEASE_TARGET;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_outer, outer, sizeof(outer));
    memcpy(expected_helper, helper, sizeof(helper));
    memcpy(interface_before, interface_object, sizeof(interface_object));
    memcpy(vtable_before, vtable, sizeof(vtable));
    memcpy(packet_before, packet, sizeof(packet));

    ((uint8_t *)expected_outer)[1u] = 2u;
    expected_outer[0x98u / 4u] = 4u;
    ((uint16_t *)(void *)expected_helper)[2u / 2u] = 1u;
    ((uint8_t *)expected_helper)[0x0cu] = 0x10u;
    ((uint8_t *)expected_helper)[0x0du] = 8u;
    ((uint8_t *)expected_helper)[0x0eu] = 0u;
    ((uint8_t *)expected_helper)[0x0fu] = 0u;
    expected_helper[0x10u / 4u] = 0x60u;
    expected_helper[0x14u / 4u] = 0u;
    expected_helper[0x18u / 4u] = 0u;
    ((uint16_t *)(void *)expected_helper)[0x1cu / 2u] =
        (uint16_t)parser_context;
    expected_helper[0x20u / 4u] = parser_context;
    ((uint16_t *)(void *)expected_helper)[0x24u / 2u] =
        (uint16_t)parser_context;
    ((uint16_t *)(void *)expected_helper)[0x26u / 2u] =
        (uint16_t)parser_context;
    expected_helper[0x28u / 4u] = parser_context;
    expected_helper[0x2cu / 4u] = parser_context;
    expected_helper[0x30u / 4u] = parser_context;
    expected_helper[0x34u / 4u] = parser_context;
    expected_helper[0x50u / 4u] = 0u;
    expected_helper[0x54u / 4u] = 0u;
    expected_helper[0x58u / 4u] = 0x60u;
    expected_helper[0x5cu / 4u] = helper[0x3cu / 4u];
    expected_helper[0x60u / 4u] = helper[0x40u / 4u];
    expected_helper[0x64u / 4u] = helper[0x44u / 4u];
    expected_helper[0x88u / 4u] = parser_context;
    expected_helper[0x8cu / 4u] = parser_context;
    ((uint16_t *)(void *)expected_helper)[0x98u / 2u] = 0xau;

    expected.eax = 0u;
    expected.ecx = VTABLE_OBJECT;
    expected.edx = DEEP_STATE_ENTRY_ESP - 16u;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    state_query_output = DEEP_STATE_ENTRY_ESP - 16u;
    state_query_result = 0x10u;
    state_query_packet = 1;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018df90u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed || interaction_count != 2u) {
        fprintf(
            stderr,
            "%s: first divergence interaction %s, count %zu\n",
            case_name,
            interaction_failed ? failed_interaction_kind : "count",
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "outer",
            outer,
            expected_outer,
            sizeof(outer)) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            expected_helper,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "interface",
            interface_object,
            interface_before,
            sizeof(interface_object)) ||
        !expect_bytes(
            case_name,
            "vtable",
            vtable,
            vtable_before,
            sizeof(vtable)) ||
        !expect_bytes(
            case_name,
            "packet",
            packet,
            packet_before,
            sizeof(packet))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_guest_memcpy_fixture(void)
{
    const char *case_name = "guest-memcpy";
    uint8_t source[8];
    uint8_t destination[8];
    uint8_t source_before[sizeof(source)];
    uint8_t expected_destination[sizeof(destination)];
    RecompMemoryAccess accesses[2];
    RecompMemoryRegion regions[] = {
        {COPY_SOURCE_OBJECT, sizeof(source), source},
        {COPY_DESTINATION_OBJECT, sizeof(destination), destination},
    };

    fill_window(source, COPY_SOURCE_OBJECT, sizeof(source));
    fill_window(
        destination,
        COPY_DESTINATION_OBJECT,
        sizeof(destination));
    memcpy(source_before, source, sizeof(source));
    memcpy(
        expected_destination,
        destination,
        sizeof(destination));
    memcpy(expected_destination + 1u, source + 2u, 4u);

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        NULL,
        0u);
    recomp_guest_memcpy(
        COPY_DESTINATION_OBJECT + 1u,
        COPY_SOURCE_OBJECT + 2u,
        4u);

    if (recomp_runtime.access_count != 2u ||
        accesses[0].address != COPY_SOURCE_OBJECT + 2u ||
        accesses[0].width != 4u ||
        accesses[1].address != COPY_DESTINATION_OBJECT + 1u ||
        accesses[1].width != 4u) {
        fprintf(
            stderr,
            "%s: first divergence memory access log\n",
            case_name);
        return 0;
    }
    if (!expect_bytes(
            case_name,
            "source",
            source,
            source_before,
            sizeof(source)) ||
        !expect_bytes(
            case_name,
            "destination",
            destination,
            expected_destination,
            sizeof(destination))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_outward_growth_fixture(int state2)
{
    const char *case_name = state2 ?
        "outward-authenticated-state-2" :
        "outward-authenticated-caller";
    uint32_t stack[GROWTH_STACK_SIZE / sizeof(uint32_t)];
    uint32_t caller[0xa8u / sizeof(uint32_t)];
    uint32_t stream[1];
    uint32_t vtable[0x24u / sizeof(uint32_t)];
    uint32_t packet[0x20u / sizeof(uint32_t)];
    uint32_t parser_packet[0x10u / sizeof(uint32_t)];
    uint32_t cleanup[0xa0u / sizeof(uint32_t)];
    uint32_t resource[0xd0u / sizeof(uint32_t)];
    uint32_t nested[0x30u / sizeof(uint32_t)];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_caller[ARRAY_SIZE(caller)];
    uint32_t stream_before[ARRAY_SIZE(stream)];
    uint32_t vtable_before[ARRAY_SIZE(vtable)];
    uint32_t packet_before[ARRAY_SIZE(packet)];
    uint32_t parser_packet_before[ARRAY_SIZE(parser_packet)];
    uint32_t expected_cleanup[ARRAY_SIZE(cleanup)];
    uint32_t expected_resource[ARRAY_SIZE(resource)];
    uint32_t expected_nested[ARRAY_SIZE(nested)];
    RecompMemoryAccess accesses[512];
    RecompMemoryRegion regions[] = {
        {GROWTH_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {
            GROWTH_CALLER_OBJECT,
            sizeof(caller),
            (uint8_t *)caller,
        },
        {
            GROWTH_STREAM_OBJECT,
            sizeof(stream),
            (uint8_t *)stream,
        },
        {
            GROWTH_VTABLE_OBJECT,
            sizeof(vtable),
            (uint8_t *)vtable,
        },
        {
            GROWTH_PACKET_OBJECT,
            sizeof(packet),
            (uint8_t *)packet,
        },
        {
            GROWTH_PARSER_PACKET_OBJECT,
            sizeof(parser_packet),
            (uint8_t *)parser_packet,
        },
        {
            GROWTH_CLEANUP_OBJECT,
            sizeof(cleanup),
            (uint8_t *)cleanup,
        },
        {
            GROWTH_RESOURCE_OBJECT,
            sizeof(resource),
            (uint8_t *)resource,
        },
        {
            GROWTH_NESTED_OBJECT,
            sizeof(nested),
            (uint8_t *)nested,
        },
    };
    const RecompRegisters initial = {
        0xf1f1f1f1u,
        0xf2f2f2f2u,
        0xf3f3f3f3u,
        0xf4f4f4f4u,
        0xf5f5f5f5u,
        0xf6f6f6f6u,
        0xf7f7f7f7u,
        GROWTH_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        GROWTH_ENTRY_ESP - (state2 ? 0x104u : 0xa0u) -
        GROWTH_STACK_BASE;
    const size_t incoming_start =
        GROWTH_ENTRY_ESP - GROWTH_STACK_BASE;

    fill_window((uint8_t *)stack, GROWTH_STACK_BASE, sizeof(stack));
    fill_window(
        (uint8_t *)caller,
        GROWTH_CALLER_OBJECT,
        sizeof(caller));
    fill_window(
        (uint8_t *)stream,
        GROWTH_STREAM_OBJECT,
        sizeof(stream));
    fill_window(
        (uint8_t *)vtable,
        GROWTH_VTABLE_OBJECT,
        sizeof(vtable));
    fill_window(
        (uint8_t *)packet,
        GROWTH_PACKET_OBJECT,
        sizeof(packet));
    fill_window(
        (uint8_t *)parser_packet,
        GROWTH_PARSER_PACKET_OBJECT,
        sizeof(parser_packet));
    fill_window(
        (uint8_t *)cleanup,
        GROWTH_CLEANUP_OBJECT,
        sizeof(cleanup));
    fill_window(
        (uint8_t *)resource,
        GROWTH_RESOURCE_OBJECT,
        sizeof(resource));
    fill_window(
        (uint8_t *)nested,
        GROWTH_NESTED_OBJECT,
        sizeof(nested));

    stack[(GROWTH_ENTRY_ESP - GROWTH_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(GROWTH_ENTRY_ESP + 4u - GROWTH_STACK_BASE) / 4u] =
        GROWTH_CALLER_OBJECT;
    caller[4u / 4u] = GROWTH_CLEANUP_OBJECT;
    caller[0x14u / 4u] = GROWTH_STREAM_OBJECT;
    caller[0x48u / 4u] = 0u;
    ((uint8_t *)caller)[0x98u] = 1u;
    caller[0xa4u / 4u] = 7u;
    stream[0] = GROWTH_VTABLE_OBJECT;
    vtable[0x18u / 4u] = GROWTH_18_TARGET;
    vtable[0x1cu / 4u] = GROWTH_1C_TARGET;
    vtable[0x20u / 4u] = GROWTH_20_TARGET;
    packet[0] = 0x1b000180u;
    parser_packet[0] = 0x00000080u;
    parser_packet[1] = 0x00000010u;
    parser_packet[2] = 0u;
    parser_packet[3] = 0u;
    cleanup[4u / 4u] = GROWTH_RESOURCE_OBJECT;
    cleanup[8u / 4u] = GROWTH_STREAM_OBJECT;
    cleanup[0x2cu / 4u] = 3u;
    resource[8u / 4u] = GROWTH_NESTED_OBJECT;
    resource[0xa4u / 4u] = 1u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_caller, caller, sizeof(caller));
    memcpy(stream_before, stream, sizeof(stream));
    memcpy(vtable_before, vtable, sizeof(vtable));
    memcpy(packet_before, packet, sizeof(packet));
    memcpy(
        parser_packet_before,
        parser_packet,
        sizeof(parser_packet));
    memcpy(expected_cleanup, cleanup, sizeof(cleanup));
    memcpy(expected_resource, resource, sizeof(resource));
    memcpy(expected_nested, nested, sizeof(nested));

    ((uint8_t *)expected_caller)[0x98u] = 0u;
    expected_caller[0xa4u / 4u] = 10u;
    ((uint8_t *)expected_cleanup)[1u] = 1u;
    ((uint8_t *)expected_cleanup)[3u] = 0u;
    expected_cleanup[0x2cu / 4u] = 0u;
    expected_cleanup[0x30u / 4u] = 0u;
    expected_cleanup[0x34u / 4u] = 0u;
    expected_cleanup[0x38u / 4u] = 0x7fffffffu;
    expected_cleanup[0x3cu / 4u] = 0xffffffffu;
    expected_cleanup[0x40u / 4u] = 0u;
    expected_cleanup[0x44u / 4u] = 0u;
    expected_cleanup[0x98u / 4u] = 0u;
    expected_resource[4u / 4u] = 0u;
    expected_nested[0x0cu / 4u] = 0u;
    expected_nested[0x28u / 4u] = 0u;
    expected_nested[0x2cu / 4u] = 0u;

    if (state2) {
        ((uint8_t *)expected_caller)[0x98u] = 1u;
        ((uint8_t *)expected_cleanup)[1u] = 2u;
        expected_cleanup[0x38u / 4u] = 0u;
        expected_cleanup[0x3cu / 4u] = 0u;
        expected_cleanup[0x98u / 4u] = 4u;
        ((uint16_t *)(void *)expected_resource)[2u / 2u] = 1u;
        ((uint8_t *)expected_resource)[0x0cu] = 0x10u;
        ((uint8_t *)expected_resource)[0x0du] = 8u;
        ((uint8_t *)expected_resource)[0x0eu] = 0u;
        ((uint8_t *)expected_resource)[0x0fu] = 0u;
        expected_resource[0x10u / 4u] = 0x60u;
        expected_resource[0x14u / 4u] = 0u;
        expected_resource[0x18u / 4u] = 0u;
        ((uint16_t *)(void *)expected_resource)[0x1cu / 2u] = 0u;
        expected_resource[0x20u / 4u] = 0u;
        ((uint16_t *)(void *)expected_resource)[0x24u / 2u] = 0u;
        ((uint16_t *)(void *)expected_resource)[0x26u / 2u] = 0u;
        expected_resource[0x28u / 4u] = 0u;
        expected_resource[0x2cu / 4u] = 0u;
        expected_resource[0x30u / 4u] = 0u;
        expected_resource[0x34u / 4u] = 0u;
        expected_resource[0x50u / 4u] = 0u;
        expected_resource[0x54u / 4u] = 0u;
        expected_resource[0x58u / 4u] = 0x60u;
        expected_resource[0x5cu / 4u] = resource[0x3cu / 4u];
        expected_resource[0x60u / 4u] = resource[0x40u / 4u];
        expected_resource[0x64u / 4u] = resource[0x44u / 4u];
        expected_resource[0x88u / 4u] = 0u;
        expected_resource[0x8cu / 4u] = 0u;
        ((uint16_t *)(void *)expected_resource)[0x98u / 2u] = 0xau;
        expected_resource[0xa8u / 4u] = 0u;
        expected_resource[0xacu / 4u] = 0u;
        expected_resource[0xb4u / 4u] = 0u;
        expected_resource[0xb8u / 4u] = 0u;
        expected_resource[0xc0u / 4u] = 0u;
        expected_resource[0xc4u / 4u] = 0u;
    }

    expected.eax = state2 ? 0u : 1u;
    expected.ecx =
        state2 ? GROWTH_CLEANUP_OBJECT : GROWTH_QUERY_OUTPUT;
    expected.edx = state2 ? 0u : GROWTH_VTABLE_OBJECT;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    growth_state2 = state2;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018e690u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed || interaction_count != 7u) {
        fprintf(
            stderr,
            "%s: first divergence interaction %s, count %zu\n",
            case_name,
            interaction_failed ? failed_interaction_kind : "count",
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "caller",
            caller,
            expected_caller,
            sizeof(caller)) ||
        !expect_bytes(
            case_name,
            "stream",
            stream,
            stream_before,
            sizeof(stream)) ||
        !expect_bytes(
            case_name,
            "vtable",
            vtable,
            vtable_before,
            sizeof(vtable)) ||
        !expect_bytes(
            case_name,
            "packet",
            packet,
            packet_before,
            sizeof(packet)) ||
        !expect_bytes(
            case_name,
            "parser packet",
            parser_packet,
            parser_packet_before,
            sizeof(parser_packet)) ||
        !expect_bytes(
            case_name,
            "cleanup",
            cleanup,
            expected_cleanup,
            sizeof(cleanup)) ||
        !expect_bytes(
            case_name,
            "resource",
            resource,
            expected_resource,
            sizeof(resource)) ||
        !expect_bytes(
            case_name,
            "nested",
            nested,
            expected_nested,
            sizeof(nested))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_state_3_4_computed_fixture(
    const char *case_name,
    uint16_t primary_state)
{
    uint32_t stack[STATE_STACK_SIZE / sizeof(uint32_t)];
    uint32_t helper[HELPER_SIZE / sizeof(uint32_t)];
    uint32_t callee[CALLEE_SIZE / sizeof(uint32_t)];
    uint8_t source[4];
    uint8_t destination[8];
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint32_t expected_helper[ARRAY_SIZE(helper)];
    uint32_t callee_before[ARRAY_SIZE(callee)];
    uint8_t source_before[sizeof(source)];
    uint8_t expected_destination[sizeof(destination)];
    RecompMemoryAccess accesses[192];
    const RecompMemoryRegion regions[] = {
        {STATE_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {HELPER_OBJECT, sizeof(helper), (uint8_t *)helper},
        {CALLEE_OBJECT, sizeof(callee), (uint8_t *)callee},
        {STATE_4_SOURCE_OBJECT, sizeof(source), source},
        {
            STATE_4_DESTINATION_OBJECT,
            sizeof(destination),
            destination,
        },
    };
    const RecompRegisters initial = {
        0x81818181u,
        0x82828282u,
        0x83838383u,
        0x84848484u,
        0x85858585u,
        0x86868686u,
        0x87878787u,
        STATE_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        STATE_ENTRY_ESP - 36u - STATE_STACK_BASE;
    const size_t incoming_start =
        STATE_ENTRY_ESP - STATE_STACK_BASE;

    fill_window((uint8_t *)stack, STATE_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)helper, HELPER_OBJECT, sizeof(helper));
    fill_window((uint8_t *)callee, CALLEE_OBJECT, sizeof(callee));
    fill_window(source, STATE_4_SOURCE_OBJECT, sizeof(source));
    fill_window(
        destination,
        STATE_4_DESTINATION_OBJECT,
        sizeof(destination));

    stack[(STATE_ENTRY_ESP - STATE_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(STATE_ENTRY_ESP + 4u - STATE_STACK_BASE) / 4u] =
        HELPER_OBJECT;
    helper[4u / 4u] = 1u;
    helper[8u / 4u] = CALLEE_OBJECT;
    ((uint8_t *)helper)[0x0eu] = 1u;
    helper[0x48u / 4u] = STATE_4_SOURCE_OBJECT;
    helper[0x4cu / 4u] = 2u;
    helper[0x5cu / 4u] = STATE_4_DESTINATION_OBJECT;
    helper[0x60u / 4u] = 2u;
    helper[0x78u / 4u] = STATE_4_PREPARE_TARGET;
    helper[0x7cu / 4u] = STATE_2_PREPARE_CONTEXT;
    helper[0x80u / 4u] = STATE_4_COMMIT_TARGET;
    helper[0x84u / 4u] = STATE_2_COMMIT_CONTEXT;
    helper[0x90u / 4u] = 7u;
    helper[0x94u / 4u] = 5u;
    ((uint16_t *)(void *)helper)[0x98u / 2u] = primary_state;
    ((uint16_t *)(void *)helper)[0x9au / 2u] = 1u;
    callee[0x0cu / 4u] = 0u;
    source[0] = 0x11u;
    source[1] = 0x22u;

    memcpy(stack_before, stack, sizeof(stack));
    memcpy(expected_helper, helper, sizeof(helper));
    memcpy(callee_before, callee, sizeof(callee));
    memcpy(source_before, source, sizeof(source));
    memcpy(expected_destination, destination, sizeof(destination));
    expected_helper[4u / 4u] = 3u;
    expected_helper[0x68u / 4u] = 0u;
    expected_helper[0x6cu / 4u] = 2u;
    expected_helper[0x70u / 4u] = 0u;
    expected_helper[0x90u / 4u] = 2u;
    expected_helper[0x94u / 4u] = 2u;
    expected_destination[0] = 0u;
    expected_destination[1] = 0x11u;
    expected_destination[2] = 0u;
    expected_destination[3] = 0x22u;
    expected.eax = 0x74747474u;
    expected.ecx = 2u;
    expected.edx = 2u;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    ed80_callback_esp = ED80_ENTRY_ESP - 52u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x00195040u)) {
        fprintf(stderr, "%s: dispatcher dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed || interaction_count != 2u) {
        fprintf(
            stderr,
            "%s: first divergence interaction %s, count %zu\n",
            case_name,
            interaction_failed ? failed_interaction_kind : "count",
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_bytes(
            case_name,
            "helper",
            helper,
            expected_helper,
            sizeof(helper)) ||
        !expect_bytes(
            case_name,
            "callee",
            callee,
            callee_before,
            sizeof(callee)) ||
        !expect_bytes(
            case_name,
            "source",
            source,
            source_before,
            sizeof(source)) ||
        !expect_bytes(
            case_name,
            "destination",
            destination,
            expected_destination,
            sizeof(destination))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_ed80_outward_fixture(void)
{
    const char *case_name = "ed80-state-0-outward";
    uint32_t stack[ED80_STACK_SIZE / sizeof(uint32_t)];
    uint32_t object[0x98u / sizeof(uint32_t)];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t object_before[ARRAY_SIZE(object)];
    RecompMemoryAccess accesses[64];
    const RecompMemoryRegion regions[] = {
        {ED80_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {ED80_OBJECT, sizeof(object), (uint8_t *)object},
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x33334444u,
        0x55556666u,
        0x77778888u,
        0x9999aaaau,
        0xbbbbccccu,
        0xddddeeeeu,
        ED80_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, ED80_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)object, ED80_OBJECT, sizeof(object));
    stack[(ED80_ENTRY_ESP - ED80_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(ED80_ENTRY_ESP + 4u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    ((uint8_t *)object)[1u] = 0u;
    object[8u / 4u] = 0u;
    object[0x94u / 4u] = 0u;

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(object_before, object, sizeof(object));
    expected_stack[
        (ED80_ENTRY_ESP - 4u - ED80_STACK_BASE) / 4u] =
        initial.esi;
    expected_stack[
        (ED80_ENTRY_ESP - 8u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 12u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 16u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 20u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected.eax = 0u;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018ed80u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "object",
            object,
            object_before,
            sizeof(object))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_ed80_state_4_fixture(void)
{
    const char *case_name = "ed80-state-4-positive-record";
    uint32_t stack[ED80_STACK_SIZE / sizeof(uint32_t)];
    uint32_t object[0x98u / sizeof(uint32_t)];
    uint32_t record[0x38u / sizeof(uint32_t)];
    uint32_t result[1] = {0xdeadbeefu};
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t object_before[ARRAY_SIZE(object)];
    uint32_t record_before[ARRAY_SIZE(record)];
    RecompMemoryAccess accesses[96];
    const RecompMemoryRegion regions[] = {
        {ED80_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {ED80_OBJECT, sizeof(object), (uint8_t *)object},
        {ED80_RECORD, sizeof(record), (uint8_t *)record},
        {ED80_RESULT, sizeof(result), (uint8_t *)result},
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x33334444u,
        0x55556666u,
        0x77778888u,
        0x9999aaaau,
        0xbbbbccccu,
        0xddddeeeeu,
        ED80_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, ED80_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)object, ED80_OBJECT, sizeof(object));
    fill_window((uint8_t *)record, ED80_RECORD, sizeof(record));
    stack[(ED80_ENTRY_ESP - ED80_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(ED80_ENTRY_ESP + 4u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    ((uint8_t *)object)[1u] = 4u;
    object[8u / 4u] = 0u;
    object[0x0cu / 4u] = ED80_RECORD;
    object[0x94u / 4u] = 0u;
    record[0x34u / 4u] = 1u;

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(object_before, object, sizeof(object));
    memcpy(record_before, record, sizeof(record));
    expected_stack[
        (ED80_ENTRY_ESP - 4u - ED80_STACK_BASE) / 4u] =
        initial.esi;
    expected_stack[
        (ED80_ENTRY_ESP - 8u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 12u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 16u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 20u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 24u - ED80_STACK_BASE) / 4u] =
        ED80_RECORD;
    expected_stack[
        (ED80_ENTRY_ESP - 28u - ED80_STACK_BASE) / 4u] = 0u;
    expected.eax = 0u;
    expected.ecx = ED80_RECORD;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018ed80u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "object",
            object,
            object_before,
            sizeof(object)) ||
        !expect_bytes(
            case_name,
            "record",
            record,
            record_before,
            sizeof(record)) ||
        !expect_u32(case_name, "result", result[0], 1u)) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_ed80_state_4_write_fixture(int computed_calls)
{
    const char *case_name = computed_calls ?
        "ed80-state-4-computed-write" :
        "ed80-state-4-zero-record-write";
    uint32_t stack[ED80_STACK_SIZE / sizeof(uint32_t)];
    uint32_t object[0x98u / sizeof(uint32_t)];
    uint32_t record[0x94u / sizeof(uint32_t)];
    uint32_t callbacks[4] = {0};
    uint32_t result[1] = {0xdeadbeefu};
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t expected_object[ARRAY_SIZE(object)];
    uint32_t expected_record[ARRAY_SIZE(record)];
    uint32_t callbacks_before[ARRAY_SIZE(callbacks)];
    RecompMemoryAccess accesses[160];
    const RecompMemoryRegion regions[] = {
        {ED80_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {ED80_OBJECT, sizeof(object), (uint8_t *)object},
        {ED80_RECORD, sizeof(record), (uint8_t *)record},
        {ED80_CALLBACKS, sizeof(callbacks), (uint8_t *)callbacks},
        {ED80_RESULT, sizeof(result), (uint8_t *)result},
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x33334444u,
        0x55556666u,
        0x77778888u,
        0x9999aaaau,
        0xbbbbccccu,
        0xddddeeeeu,
        ED80_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, ED80_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)object, ED80_OBJECT, sizeof(object));
    fill_window((uint8_t *)record, ED80_RECORD, sizeof(record));
    stack[(ED80_ENTRY_ESP - ED80_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(ED80_ENTRY_ESP + 4u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    ((uint8_t *)object)[1u] = 4u;
    object[8u / 4u] = 0u;
    object[0x0cu / 4u] = ED80_RECORD;
    object[0x94u / 4u] = 0u;
    ((uint8_t *)record)[1u] = 0u;
    record[0x34u / 4u] = 0u;
    record[0x90u / 4u] = 0xfacefeedu;
    if (computed_calls) {
        callbacks[0] = ED80_BEGIN_TARGET;
        callbacks[1] = ED80_BEGIN_CONTEXT;
        callbacks[2] = ED80_END_TARGET;
        callbacks[3] = ED80_END_CONTEXT;
    }

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_object, object, sizeof(object));
    memcpy(expected_record, record, sizeof(record));
    memcpy(callbacks_before, callbacks, sizeof(callbacks));
    ((uint8_t *)expected_object)[1u] = 5u;
    expected_record[0x90u / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 4u - ED80_STACK_BASE) / 4u] =
        initial.esi;
    expected_stack[
        (ED80_ENTRY_ESP - 8u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 12u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 16u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 20u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 24u - ED80_STACK_BASE) / 4u] =
        ED80_RECORD;
    expected_stack[
        (ED80_ENTRY_ESP - 28u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 32u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 36u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 40u - ED80_STACK_BASE) / 4u] =
        ED80_ENTRY_ESP - 24u;
    expected_stack[
        (ED80_ENTRY_ESP - 44u - ED80_STACK_BASE) / 4u] = 0u;
    if (computed_calls) {
        expected_stack[
            (ED80_ENTRY_ESP - 48u - ED80_STACK_BASE) / 4u] =
            ED80_END_CONTEXT;
        expected_stack[
            (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] = 0u;
    }
    expected.eax = 0u;
    expected.ecx = 0u;
    expected.edx = ED80_RECORD;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018ed80u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: args "
            "0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count != (computed_calls ? 2u : 0u)) {
        fprintf(
            stderr,
            "%s: first divergence callback count was %zu, expected %zu\n",
            case_name,
            interaction_count,
            (size_t)(computed_calls ? 2u : 0u));
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "object",
            object,
            expected_object,
            sizeof(object)) ||
        !expect_bytes(
            case_name,
            "record",
            record,
            expected_record,
            sizeof(record)) ||
        !expect_bytes(
            case_name,
            "callbacks",
            callbacks,
            callbacks_before,
            sizeof(callbacks)) ||
        !expect_u32(case_name, "result", result[0], 0u)) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_ed80_state_2_fixture(void)
{
    const char *case_name = "ed80-state-2-no-callback";
    uint32_t stack[ED80_STACK_SIZE / sizeof(uint32_t)];
    uint32_t object[0x98u / sizeof(uint32_t)];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t expected_object[ARRAY_SIZE(object)];
    RecompMemoryAccess accesses[128];
    const RecompMemoryRegion regions[] = {
        {ED80_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {ED80_OBJECT, sizeof(object), (uint8_t *)object},
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x33334444u,
        0x55556666u,
        0x77778888u,
        0x9999aaaau,
        0xbbbbccccu,
        0xddddeeeeu,
        ED80_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, ED80_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)object, ED80_OBJECT, sizeof(object));
    stack[(ED80_ENTRY_ESP - ED80_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(ED80_ENTRY_ESP + 4u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    ((uint8_t *)object)[1u] = 2u;
    object[4u / 4u] = ED80_OBJECT;
    object[8u / 4u] = 0u;
    object[0x0cu / 4u] = ED80_OBJECT;
    object[0x34u / 4u] = 0u;
    object[0x48u / 4u] = 0u;
    ((uint8_t *)object)[0x70u] = 1u;
    object[0x94u / 4u] = 0u;

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_object, object, sizeof(object));
    ((uint8_t *)expected_object)[0x71u] = 1u;
    expected_stack[
        (ED80_ENTRY_ESP - 4u - ED80_STACK_BASE) / 4u] =
        initial.esi;
    expected_stack[
        (ED80_ENTRY_ESP - 8u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 12u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 16u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 20u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 28u - ED80_STACK_BASE) / 4u] =
        0x7fffffffu;
    expected_stack[
        (ED80_ENTRY_ESP - 32u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 40u - ED80_STACK_BASE) / 4u] =
        initial.ebx;
    expected_stack[
        (ED80_ENTRY_ESP - 44u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 48u - ED80_STACK_BASE) / 4u] =
        initial.edi;
    expected_stack[
        (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 56u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 60u - ED80_STACK_BASE) / 4u] = 0u;
    expected.eax = 0u;
    expected.ecx = 0u;
    expected.edx = ED80_OBJECT;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018ed80u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "object",
            object,
            expected_object,
            sizeof(object))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_ed80_state_3_fixture(int path)
{
    const int deeper = path != 0;
    const int zero_count = path >= 2;
    const int computed_calls = path == 3;
    const int f130_path = path == 4;
    const int f010_path = path == 5;
    const int f010_item_path = path == 6;
    const int efa6_path = path == 7;
    const int f010_computed_path = path == 8;
    const int f010_two_item_path = path == 9;
    const int f010_null_item_path = path == 10;
    const int f010_literal_path = path == 11;
    const int f010_log_path = f010_null_item_path || f010_literal_path;
    const int f010_any_path =
        f010_path || f010_item_path || f010_computed_path ||
        f010_two_item_path || f010_log_path;
    const int f010_has_item =
        f010_item_path || f010_computed_path || f010_two_item_path;
    const int f010_processes_item =
        f010_has_item || f010_log_path;
    const int runs_f130 =
        f130_path || f010_any_path || efa6_path;
    const char *case_name =
        path == 0 ? "ed80-state-3-provider-skips-loop" :
        path == 1 ? "ed80-state-3-negative-count" :
        path == 2 ? "ed80-state-3-zero-count" :
        path == 3 ? "ed80-state-3-zero-count-computed" :
        path == 4 ? "ed80-state-3-f130-loop" :
        path == 5 ? "ed80-state-3-f010-zero-items" :
        path == 6 ? "ed80-state-3-f010-one-item" :
        path == 7 ? "ed80-state-3-efa6-f130" :
        path == 8 ? "ed80-state-3-f010-computed-callbacks" :
        path == 9 ? "ed80-state-3-f010-two-items" :
        path == 10 ? "ed80-state-3-f010-null-item-log" :
        "ed80-state-3-f010-literal-log";
    uint32_t stack[ED80_STACK_SIZE / sizeof(uint32_t)];
    uint32_t object[0x98u / sizeof(uint32_t)];
    uint8_t provider[8];
    uint8_t record[0x90];
    uint32_t callbacks[4] = {0};
    uint32_t count[1] = {0xdeadbeefu};
    uint32_t f130_count[1] = {0xdeadbeefu};
    uint32_t item_interface[1];
    uint32_t item_vtable[0x24u / sizeof(uint32_t)];
    uint32_t efa6_array[1];
    uint32_t efa6_vtable[0x58u / sizeof(uint32_t)];
    uint8_t log_format[2] = {0, 0};
    uint8_t log_buffer[2] = {0xa5u, 0xa5u};
    uint32_t log_globals[2] = {0};
    uint8_t format_class[1] = {0};
    uint8_t format_state[1] = {0x60u};
    uint32_t format_jump[1] = {0x001bd159u};
    uint32_t mbcs_pointer[1] = {ED80_MBCS_TABLE};
    uint8_t mbcs_table[1] = {0};
    const uint32_t provider_record = ED80_RECORD;
    const uint32_t pending_state = 0xfeedfaceu;
    const uint32_t f130_remainder = 0xf4d3b291u;
    const uint32_t f130_accumulator = 0x14d2914eu;
    const uint32_t f010_remainder = 0x510fcd8au;
    const uint32_t f010_accumulator_34 = 0x5817d592u;
    const uint32_t f010_accumulator_4c = 0x702fedaa;
    const uint32_t item_vtable_object = ITEM_VTABLE_OBJECT;
    const uint32_t item_interface_address = ITEM_INTERFACE_OBJECT;
    const uint32_t efa6_vtable_address = ED80_EFA6_VTABLE;
    const uint32_t efa6_array_address = ED80_EFA6_ARRAY;
    const uint32_t f010_item_base = 0x8c6b4a29u;
    const uint32_t f010_item_length = 0x10efceadu;
    const uint32_t f010_item_scaled = 0x0f90f880u;
    const uint32_t f010_item_remaining = 0x015ed62du;
    const uint32_t f010_item_next = 0x9bfc42a9u;
    const uint32_t zero = 0u;
    const uint32_t one = 1u;
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t expected_object[ARRAY_SIZE(object)];
    uint8_t provider_before[sizeof(provider)];
    uint8_t expected_record[sizeof(record)];
    uint32_t callbacks_before[ARRAY_SIZE(callbacks)];
    uint32_t item_interface_before[ARRAY_SIZE(item_interface)];
    uint32_t item_vtable_before[ARRAY_SIZE(item_vtable)];
    uint32_t efa6_array_before[ARRAY_SIZE(efa6_array)];
    uint32_t efa6_vtable_before[ARRAY_SIZE(efa6_vtable)];
    uint8_t log_format_before[sizeof(log_format)];
    uint8_t expected_log_buffer[sizeof(log_buffer)];
    uint32_t log_globals_before[ARRAY_SIZE(log_globals)];
    uint8_t format_class_before[sizeof(format_class)];
    uint8_t format_state_before[sizeof(format_state)];
    uint32_t format_jump_before[ARRAY_SIZE(format_jump)];
    uint32_t mbcs_pointer_before[ARRAY_SIZE(mbcs_pointer)];
    uint8_t mbcs_table_before[sizeof(mbcs_table)];
    static RecompMemoryAccess accesses[16384];
    const RecompMemoryRegion regions[] = {
        {ED80_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {ED80_OBJECT, sizeof(object), (uint8_t *)object},
        {ED80_PROVIDER, sizeof(provider), provider},
        {ED80_RECORD, sizeof(record), record},
        {ED80_CALLBACKS, sizeof(callbacks), (uint8_t *)callbacks},
        {ED80_COUNT, sizeof(count), (uint8_t *)count},
        {ED80_F130_COUNT, sizeof(f130_count), (uint8_t *)f130_count},
        {
            ITEM_INTERFACE_OBJECT,
            sizeof(item_interface),
            (uint8_t *)item_interface,
        },
        {
            ITEM_VTABLE_OBJECT,
            sizeof(item_vtable),
            (uint8_t *)item_vtable,
        },
        {ED80_EFA6_ARRAY, sizeof(efa6_array), (uint8_t *)efa6_array},
        {
            ED80_EFA6_VTABLE,
            sizeof(efa6_vtable),
            (uint8_t *)efa6_vtable,
        },
        {ED80_LOG_FORMAT, sizeof(log_format), log_format},
        {ED80_LOG_BUFFER, sizeof(log_buffer), log_buffer},
        {
            ED80_LOG_GLOBALS,
            sizeof(log_globals),
            (uint8_t *)log_globals,
        },
        {ED80_FORMAT_CLASS, sizeof(format_class), format_class},
        {ED80_FORMAT_STATE, sizeof(format_state), format_state},
        {
            ED80_FORMAT_JUMP,
            sizeof(format_jump),
            (uint8_t *)format_jump,
        },
        {
            ED80_MBCS_POINTER,
            sizeof(mbcs_pointer),
            (uint8_t *)mbcs_pointer,
        },
        {
            ED80_MBCS_TABLE + 0x83u,
            sizeof(mbcs_table),
            mbcs_table,
        },
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x33334444u,
        0x55556666u,
        0x77778888u,
        0x9999aaaau,
        zero_count ? 0x77778888u : 0xbbbbccccu,
        0xddddeeeeu,
        ED80_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, ED80_STACK_BASE, sizeof(stack));
    fill_window((uint8_t *)object, ED80_OBJECT, sizeof(object));
    fill_window(provider, ED80_PROVIDER, sizeof(provider));
    fill_window(record, ED80_RECORD, sizeof(record));
    fill_window(
        (uint8_t *)item_interface,
        ITEM_INTERFACE_OBJECT,
        sizeof(item_interface));
    fill_window(
        (uint8_t *)item_vtable,
        ITEM_VTABLE_OBJECT,
        sizeof(item_vtable));
    fill_window(
        (uint8_t *)efa6_array,
        ED80_EFA6_ARRAY,
        sizeof(efa6_array));
    fill_window(
        (uint8_t *)efa6_vtable,
        ED80_EFA6_VTABLE,
        sizeof(efa6_vtable));
    memcpy(
        item_interface,
        &item_vtable_object,
        sizeof(item_vtable_object));
    item_vtable[0x20u / 4u] = ED80_F010_METHOD20_TARGET;
    item_vtable[0x1cu / 4u] = ED80_F010_METHOD1C_TARGET;
    memcpy(
        efa6_array,
        &efa6_vtable_address,
        sizeof(efa6_vtable_address));
    efa6_vtable[0x54u / 4u] = ED80_EFA6_QUERY_TARGET;
    stack[(ED80_ENTRY_ESP - ED80_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(ED80_ENTRY_ESP + 4u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    ((uint8_t *)object)[1u] = 3u;
    object[4u / 4u] = ED80_PROVIDER;
    object[8u / 4u] = 0u;
    object[0x0cu / 4u] = ED80_RECORD;
    object[0x94u / 4u] = 0u;
    provider[1u] = deeper ? 3u : 2u;
    memcpy(provider + 4u, &provider_record, sizeof(provider_record));
    record[1u] = f010_any_path ? 1u : 0u;
    record[3u] = f010_two_item_path ?
        2u :
        (f010_processes_item ? 1u : 0u);
    record[5u] = runs_f130 ? 1u : 0u;
    record[0x0eu] = zero_count ? 0u : 0xffu;
    if (efa6_path) {
        memcpy(record + 0x38u, &efa6_array_address, sizeof(uint32_t));
        memcpy(record + 0x5cu, &one, sizeof(one));
        ((uint8_t *)record)[3u] = 2u;
    } else {
        memset(record + 0x5cu, 0, sizeof(uint32_t));
    }
    if (f010_has_item) {
        memcpy(
            record + 0x10u,
            &item_interface_address,
            sizeof(item_interface_address));
    }
    if (f010_log_path) {
        memset(record + 0x10u, 0, sizeof(uint32_t));
    }
    if (f010_two_item_path) {
        memcpy(
            record + 0x14u,
            &item_interface_address,
            sizeof(item_interface_address));
        memcpy(
            record + 0x20u,
            &f010_item_scaled,
            sizeof(f010_item_scaled));
        memcpy(
            record + 0x24u,
            &f010_item_base,
            sizeof(f010_item_base));
    }
    memcpy(record + 0x8cu, &pending_state, sizeof(pending_state));
    if (computed_calls || f010_computed_path) {
        callbacks[0] = ED80_BEGIN_TARGET;
        callbacks[1] = ED80_BEGIN_CONTEXT;
        callbacks[2] = ED80_END_TARGET;
        callbacks[3] = ED80_END_CONTEXT;
    }
    if (f010_log_path) {
        log_globals[0] = ED80_LOG_TARGET;
        log_globals[1] = ED80_LOG_CONTEXT;
    }
    if (f010_literal_path) {
        log_format[0] = 0x41u;
    }

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_object, object, sizeof(object));
    memcpy(provider_before, provider, sizeof(provider));
    memcpy(expected_record, record, sizeof(record));
    memcpy(callbacks_before, callbacks, sizeof(callbacks));
    memcpy(
        item_interface_before,
        item_interface,
        sizeof(item_interface));
    memcpy(item_vtable_before, item_vtable, sizeof(item_vtable));
    memcpy(efa6_array_before, efa6_array, sizeof(efa6_array));
    memcpy(efa6_vtable_before, efa6_vtable, sizeof(efa6_vtable));
    memcpy(log_format_before, log_format, sizeof(log_format));
    memcpy(expected_log_buffer, log_buffer, sizeof(log_buffer));
    memcpy(log_globals_before, log_globals, sizeof(log_globals));
    memcpy(format_class_before, format_class, sizeof(format_class));
    memcpy(format_state_before, format_state, sizeof(format_state));
    memcpy(format_jump_before, format_jump, sizeof(format_jump));
    memcpy(mbcs_pointer_before, mbcs_pointer, sizeof(mbcs_pointer));
    memcpy(mbcs_table_before, mbcs_table, sizeof(mbcs_table));
    if (zero_count) {
        ((uint8_t *)expected_object)[1u] = 4u;
        memset(expected_record + 0x8cu, 0, sizeof(uint32_t));
    }
    if (f130_path || efa6_path) {
        expected_record[5u] = 0u;
        memcpy(
            expected_record + 0x50u,
            &f130_remainder,
            sizeof(f130_remainder));
        memcpy(
            expected_record + 0x54u,
            &f130_accumulator,
            sizeof(f130_accumulator));
        memcpy(expected_record + 0x58u, &zero, sizeof(zero));
    }
    if (efa6_path) {
        memset(expected_record + 0x5cu, 0, sizeof(uint32_t));
    }
    if (f010_any_path) {
        expected_record[1u] = 0u;
        expected_record[5u] = 0u;
        memcpy(
            expected_record + 0x2cu,
            &f010_remainder,
            sizeof(f010_remainder));
        memcpy(
            expected_record + 0x34u,
            &f010_accumulator_34,
            sizeof(f010_accumulator_34));
        memcpy(expected_record + 0x3cu, &zero, sizeof(zero));
        memcpy(
            expected_record + 0x4cu,
            &f010_accumulator_4c,
            sizeof(f010_accumulator_4c));
        memcpy(
            expected_record + 0x50u,
            &f010_remainder,
            sizeof(f010_remainder));
        memcpy(expected_record + 0x54u, &zero, sizeof(zero));
        memcpy(expected_record + 0x58u, &zero, sizeof(zero));
    }
    if (f010_log_path) {
        memcpy(
            expected_record + 0x10u,
            &item_interface_address,
            sizeof(item_interface_address));
        if (f010_literal_path) {
            expected_log_buffer[0] = 0x41u;
            expected_log_buffer[1] = 0u;
        } else {
            expected_log_buffer[0] = 0u;
        }
    }
    expected_stack[
        (ED80_ENTRY_ESP - 4u - ED80_STACK_BASE) / 4u] =
        initial.esi;
    expected_stack[
        (ED80_ENTRY_ESP - 8u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 12u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 16u - ED80_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (ED80_ENTRY_ESP - 20u - ED80_STACK_BASE) / 4u] =
        ED80_OBJECT;
    expected_stack[
        (ED80_ENTRY_ESP - 24u - ED80_STACK_BASE) / 4u] = 0u;
    if (deeper) {
        expected_stack[
            (ED80_ENTRY_ESP - 24u - ED80_STACK_BASE) / 4u] =
            zero_count ? ED80_RECORD : initial.edi;
        expected_stack[
            (ED80_ENTRY_ESP - 28u - ED80_STACK_BASE) / 4u] =
            zero_count ? 0u : ED80_RECORD;
        expected_stack[
            (ED80_ENTRY_ESP - 32u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (zero_count) {
        expected_stack[
            (ED80_ENTRY_ESP - 36u - ED80_STACK_BASE) / 4u] =
            initial.ebx;
        expected_stack[
            (ED80_ENTRY_ESP - 40u - ED80_STACK_BASE) / 4u] =
            ED80_OBJECT;
        expected_stack[
            (ED80_ENTRY_ESP - 44u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 24u;
        expected_stack[
            (ED80_ENTRY_ESP - 48u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (computed_calls) {
        expected_stack[
            (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] =
            ED80_END_CONTEXT;
        expected_stack[
            (ED80_ENTRY_ESP - 56u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (runs_f130) {
        expected_stack[
            (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD;
        expected_stack[
            (ED80_ENTRY_ESP - 56u - ED80_STACK_BASE) / 4u] =
            initial.edi;
        expected_stack[
            (ED80_ENTRY_ESP - 60u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD;
        expected_stack[
            (ED80_ENTRY_ESP - 64u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 76u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD;
    }
    if (f010_any_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] =
            ED80_OBJECT;
        expected_stack[
            (ED80_ENTRY_ESP - 32u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 88u - ED80_STACK_BASE) / 4u] =
            f010_two_item_path ? 2u : (f010_processes_item ? 1u : 0u);
        expected_stack[
            (ED80_ENTRY_ESP - 96u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 100u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD;
        expected_stack[
            (ED80_ENTRY_ESP - 104u - ED80_STACK_BASE) / 4u] =
            initial.edi;
        expected_stack[
            (ED80_ENTRY_ESP - 108u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 84u;
        expected_stack[
            (ED80_ENTRY_ESP - 112u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (f010_processes_item) {
        expected_stack[
            (ED80_ENTRY_ESP - 60u - ED80_STACK_BASE) / 4u] =
            f010_item_remaining;
        expected_stack[
            (ED80_ENTRY_ESP - 64u - ED80_STACK_BASE) / 4u] =
            f010_item_next;
        expected_stack[
            (ED80_ENTRY_ESP - 68u - ED80_STACK_BASE) / 4u] =
            f010_item_scaled;
        expected_stack[
            (ED80_ENTRY_ESP - 72u - ED80_STACK_BASE) / 4u] =
            f010_item_base;
        expected_stack[
            (ED80_ENTRY_ESP - 76u - ED80_STACK_BASE) / 4u] =
            f010_item_length;
        expected_stack[
            (ED80_ENTRY_ESP - 80u - ED80_STACK_BASE) / 4u] =
            f010_item_base;
        expected_stack[
            (ED80_ENTRY_ESP - 116u - ED80_STACK_BASE) / 4u] =
            f010_item_scaled;
        expected_stack[
            (ED80_ENTRY_ESP - 120u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 80u;
        expected_stack[
            (ED80_ENTRY_ESP - 124u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 72u;
        expected_stack[
            (ED80_ENTRY_ESP - 128u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 132u - ED80_STACK_BASE) / 4u] =
            ITEM_INTERFACE_OBJECT;
        expected_stack[
            (ED80_ENTRY_ESP - 136u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 64u;
        expected_stack[
            (ED80_ENTRY_ESP - 140u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 144u - ED80_STACK_BASE) / 4u] =
            ITEM_INTERFACE_OBJECT;
        expected_stack[
            (ED80_ENTRY_ESP - 148u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (f010_computed_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 52u - ED80_STACK_BASE) / 4u] =
            ED80_END_CONTEXT;
        expected_stack[
            (ED80_ENTRY_ESP - 56u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 116u - ED80_STACK_BASE) / 4u] =
            ED80_END_CONTEXT;
        expected_stack[
            (ED80_ENTRY_ESP - 120u - ED80_STACK_BASE) / 4u] = 0u;
    }
    if (f010_two_item_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 60u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 64u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 68u - ED80_STACK_BASE) / 4u] =
            f010_item_base;
        expected_stack[
            (ED80_ENTRY_ESP - 72u - ED80_STACK_BASE) / 4u] =
            f010_item_scaled;
        expected_stack[
            (ED80_ENTRY_ESP - 76u - ED80_STACK_BASE) / 4u] =
            f010_item_base;
        expected_stack[
            (ED80_ENTRY_ESP - 80u - ED80_STACK_BASE) / 4u] =
            f010_item_scaled;
    }
    if (f010_log_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 0x314u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD + 0x10u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xe8u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xd4u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xd0u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xc0u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 0x84u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xbcu - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xb8u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 0xa4u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xb4u - ED80_STACK_BASE) / 4u] =
            ED80_LOG_FORMAT;
        expected_stack[
            (ED80_ENTRY_ESP - 0xb0u - ED80_STACK_BASE) / 4u] =
            ED80_ENTRY_ESP - 0x68u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xacu - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xa8u - ED80_STACK_BASE) / 4u] =
            ED80_RECORD;
        expected_stack[
            (ED80_ENTRY_ESP - 0xa4u - ED80_STACK_BASE) / 4u] =
            ED80_LOG_BUFFER;
        expected_stack[
            (ED80_ENTRY_ESP - 0xa0u - ED80_STACK_BASE) / 4u] =
            0x7ffffffeu;
        expected_stack[
            (ED80_ENTRY_ESP - 0x9cu - ED80_STACK_BASE) / 4u] =
            ED80_LOG_BUFFER;
        expected_stack[
            (ED80_ENTRY_ESP - 0x98u - ED80_STACK_BASE) / 4u] = 0x42u;
    }
    if (f010_literal_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 0x320u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0x31cu - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0x318u - ED80_STACK_BASE) / 4u] =
            ED80_LOG_BUFFER;
        expected_stack[
            (ED80_ENTRY_ESP - 0xf4u - ED80_STACK_BASE) / 4u] = 6u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xe4u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xd4u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xb4u - ED80_STACK_BASE) / 4u] =
            ED80_LOG_FORMAT + 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xa4u - ED80_STACK_BASE) / 4u] =
            ED80_LOG_BUFFER + 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 0xa0u - ED80_STACK_BASE) / 4u] =
            0x7ffffffdu;
    }
    if (efa6_path) {
        expected_stack[
            (ED80_ENTRY_ESP - 68u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 72u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 80u - ED80_STACK_BASE) / 4u] =
            ED80_OBJECT;
        expected_stack[
            (ED80_ENTRY_ESP - 84u - ED80_STACK_BASE) / 4u] = 0u;
        expected_stack[
            (ED80_ENTRY_ESP - 88u - ED80_STACK_BASE) / 4u] = 1u;
        expected_stack[
            (ED80_ENTRY_ESP - 92u - ED80_STACK_BASE) / 4u] =
            ED80_EFA6_ARRAY;
        expected_stack[
            (ED80_ENTRY_ESP - 96u - ED80_STACK_BASE) / 4u] = 0u;
    }
    expected.eax = 0u;
    if (deeper) {
        expected.ecx = zero_count ? 0u : ED80_RECORD;
    }
    if (zero_count) {
        expected.edx = 0u;
    }
    if (f010_any_path) {
        expected.ecx = 1u;
        expected.edx = 1u;
    }
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    ed80_callback_esp = ED80_ENTRY_ESP - 56u;
    ed80_f010_computed_callbacks = f010_computed_path;
    ed80_f010_two_items = f010_two_item_path;
    ed80_f010_null_item = f010_null_item_path;
    ed80_f010_literal_log = f010_literal_path;
    ed80_log_expected_byte = f010_literal_path ? 0x41u : 0u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0018ed80u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: args "
            "0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count !=
        (f010_computed_path ? 6u :
         f010_two_item_path ? 4u :
         f010_log_path ? 3u :
         (computed_calls || f010_item_path || efa6_path) ? 2u : 0u)) {
        fprintf(
            stderr,
            "%s: first divergence callback count was %zu, expected %zu\n",
            case_name,
            interaction_count,
            (size_t)(
                f010_computed_path ? 6u :
                f010_two_item_path ? 4u :
                f010_log_path ? 3u :
                (computed_calls || f010_item_path || efa6_path) ? 2u : 0u));
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "object",
            object,
            expected_object,
            sizeof(object)) ||
        !expect_bytes(
            case_name,
            "provider",
            provider,
            provider_before,
            sizeof(provider)) ||
        !expect_bytes(
            case_name,
            "record",
            record,
            expected_record,
            sizeof(record)) ||
        !expect_bytes(
            case_name,
            "callbacks",
            callbacks,
            callbacks_before,
            sizeof(callbacks)) ||
        !expect_bytes(
            case_name,
            "item interface",
            item_interface,
            item_interface_before,
            sizeof(item_interface)) ||
        !expect_bytes(
            case_name,
            "item vtable",
            item_vtable,
            item_vtable_before,
            sizeof(item_vtable)) ||
        !expect_bytes(
            case_name,
            "efa6 array",
            efa6_array,
            efa6_array_before,
            sizeof(efa6_array)) ||
        !expect_bytes(
            case_name,
            "efa6 vtable",
            efa6_vtable,
            efa6_vtable_before,
            sizeof(efa6_vtable)) ||
        !expect_bytes(
            case_name,
            "log format",
            log_format,
            log_format_before,
            sizeof(log_format)) ||
        !expect_bytes(
            case_name,
            "log buffer",
            log_buffer,
            expected_log_buffer,
            sizeof(log_buffer)) ||
        !expect_bytes(
            case_name,
            "log globals",
            log_globals,
            log_globals_before,
            sizeof(log_globals)) ||
        !expect_bytes(
            case_name,
            "format class",
            format_class,
            format_class_before,
            sizeof(format_class)) ||
        !expect_bytes(
            case_name,
            "format state",
            format_state,
            format_state_before,
            sizeof(format_state)) ||
        !expect_bytes(
            case_name,
            "format jump",
            format_jump,
            format_jump_before,
            sizeof(format_jump)) ||
        !expect_bytes(
            case_name,
            "mbcs pointer",
            mbcs_pointer,
            mbcs_pointer_before,
            sizeof(mbcs_pointer)) ||
        !expect_bytes(
            case_name,
            "mbcs table",
            mbcs_table,
            mbcs_table_before,
            sizeof(mbcs_table)) ||
        !expect_u32(
            case_name,
            "count",
            count[0],
            zero_count ? 0u :
            deeper ? 0xffffffffu :
            0xdeadbeefu) ||
        !expect_u32(
            case_name,
            "F130 count",
            f130_count[0],
            efa6_path ? 0u :
            runs_f130 ? 200u :
            0xdeadbeefu)) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_1889a0_outward_fixture(void)
{
    const char *case_name = "1889a0-outward-record-scan";
    uint32_t stack[A0_STACK_SIZE / sizeof(uint32_t)];
    uint32_t stage_flag[1] = {0};
    uint32_t teardown_flag[1] = {0};
    uint8_t setup_records[A0_SETUP_SIZE] = {0};
    uint8_t teardown_records[A0_TEARDOWN_SIZE] = {0};
    uint8_t active_records[A0_ACTIVE_SIZE] = {0};
    uint32_t stack_before[ARRAY_SIZE(stack)];
    uint8_t setup_before[sizeof(setup_records)];
    uint8_t teardown_before[sizeof(teardown_records)];
    uint8_t active_before[sizeof(active_records)];
    RecompMemoryAccess accesses[128];
    const RecompMemoryRegion regions[] = {
        {A0_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {A0_STAGE_FLAG, sizeof(stage_flag), (uint8_t *)stage_flag},
        {
            A0_TEARDOWN_FLAG,
            sizeof(teardown_flag),
            (uint8_t *)teardown_flag,
        },
        {A0_SETUP_RECORDS, sizeof(setup_records), setup_records},
        {
            A0_TEARDOWN_RECORDS,
            sizeof(teardown_records),
            teardown_records,
        },
        {A0_ACTIVE_RECORDS, sizeof(active_records), active_records},
    };
    const RecompRegisters initial = {
        0x21212121u,
        0x32323232u,
        0x43434343u,
        0x54545454u,
        0x65656565u,
        0x76767676u,
        0x87878787u,
        A0_ENTRY_ESP,
    };
    RecompRegisters expected = initial;
    const size_t frame_start =
        A0_ENTRY_ESP - 0x40u - A0_STACK_BASE;
    const size_t incoming_start =
        A0_ENTRY_ESP - A0_STACK_BASE;

    fill_window((uint8_t *)stack, A0_STACK_BASE, sizeof(stack));
    stack[(A0_ENTRY_ESP - A0_STACK_BASE) / 4u] =
        0xcafef00du;
    active_records[0] = 1u;
    memcpy(stack_before, stack, sizeof(stack));
    memcpy(setup_before, setup_records, sizeof(setup_records));
    memcpy(teardown_before, teardown_records, sizeof(teardown_records));
    memcpy(active_before, active_records, sizeof(active_records));
    expected.eax = 0u;
    expected.esp += 4u;

    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x001889a0u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack prefix",
            stack,
            stack_before,
            frame_start) ||
        !expect_bytes(
            case_name,
            "incoming stack",
            (uint8_t *)stack + incoming_start,
            (uint8_t *)stack_before + incoming_start,
            sizeof(stack) - incoming_start) ||
        !expect_u32(
            case_name,
            "stage flag",
            stage_flag[0],
            0u) ||
        !expect_u32(
            case_name,
            "teardown flag",
            teardown_flag[0],
            0u) ||
        !expect_bytes(
            case_name,
            "setup records",
            setup_records,
            setup_before,
            sizeof(setup_records)) ||
        !expect_bytes(
            case_name,
            "teardown records",
            teardown_records,
            teardown_before,
            sizeof(teardown_records)) ||
        !expect_bytes(
            case_name,
            "active records",
            active_records,
            active_before,
            sizeof(active_records))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_notify_fixture(int callback_enabled)
{
    const char *case_name = callback_enabled ?
        "191180-callback-dispatches" :
        "191180-callback-null-skips";
    uint32_t stack[NOTIFY_STACK_SIZE / sizeof(uint32_t)];
    uint32_t globals[2] = {
        callback_enabled ? NOTIFY_TARGET : 0u,
        NOTIFY_CONTEXT,
    };
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint32_t globals_before[ARRAY_SIZE(globals)];
    RecompMemoryAccess accesses[32];
    const RecompMemoryRegion regions[] = {
        {NOTIFY_STACK_BASE, sizeof(stack), (uint8_t *)stack},
        {NOTIFY_GLOBALS, sizeof(globals), (uint8_t *)globals},
    };
    const RecompRegisters initial = {
        0x11223344u,
        0x22334455u,
        0x33445566u,
        0x44556677u,
        0x55667788u,
        0x66778899u,
        0x778899aau,
        NOTIFY_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window((uint8_t *)stack, NOTIFY_STACK_BASE, sizeof(stack));
    stack[(NOTIFY_ENTRY_ESP - NOTIFY_STACK_BASE) / 4u] =
        0xcafef00du;
    stack[(NOTIFY_ENTRY_ESP + 4u - NOTIFY_STACK_BASE) / 4u] =
        NOTIFY_ARG0;
    stack[(NOTIFY_ENTRY_ESP + 8u - NOTIFY_STACK_BASE) / 4u] =
        NOTIFY_ARG1;
    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(globals_before, globals, sizeof(globals));
    if (callback_enabled) {
        expected_stack[
            (NOTIFY_ENTRY_ESP - 4u - NOTIFY_STACK_BASE) / 4u] =
            NOTIFY_ARG0;
        expected_stack[
            (NOTIFY_ENTRY_ESP - 8u - NOTIFY_STACK_BASE) / 4u] =
            NOTIFY_ARG1;
        expected_stack[
            (NOTIFY_ENTRY_ESP - 12u - NOTIFY_STACK_BASE) / 4u] =
            NOTIFY_CONTEXT;
        expected_stack[
            (NOTIFY_ENTRY_ESP - 16u - NOTIFY_STACK_BASE) / 4u] = 0u;
        expected.ecx = NOTIFY_CONTEXT;
        expected.edx = NOTIFY_ARG1;
    }
    expected.eax = 0u;
    expected.esp += 4u;

    interaction_failed = 0;
    interaction_count = 0u;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x00191180u)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed ||
        interaction_count != (callback_enabled ? 1u : 0u)) {
        fprintf(
            stderr,
            "%s: first divergence interaction %s, count %zu\n",
            case_name,
            interaction_failed ? failed_interaction_kind : "count",
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "notify globals",
            globals,
            globals_before,
            sizeof(globals))) {
        return 0;
    }
    if (recomp_runtime.access_count !=
        (callback_enabled ? 11u : 1u)) {
        fprintf(
            stderr,
            "%s: first divergence access_count was %zu, expected %u\n",
            case_name,
            recomp_runtime.access_count,
            callback_enabled ? 11u : 1u);
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

static int run_notify_caller_fixture(uint32_t error_code)
{
    const char *case_name = error_code == 6u ?
        "19132e-last-error-6" :
        "19132e-last-error-30";
    const uint32_t message = error_code == 6u ?
        0x0023b84cu : 0x0023b874u;
    const uint32_t saved_edi = 0x8899aabbu;
    const uint32_t saved_esi = 0x778899aau;
    const uint32_t saved_ebp = 0x66778899u;
    const uint32_t saved_ebx = 0x55667788u;
    const uint32_t saved_ecx = 0x44556677u;
    const uint32_t tls_data_address = NOTIFY_TLS_DATA;
    const uint32_t tls_slot_address = NOTIFY_TLS_SLOTS + 0x24u;
    const uint32_t tls_rwdata = 0x00700000u;
    const uint32_t one = 1u;
    uint32_t stack[NOTIFY_CALLER_STACK_SIZE / sizeof(uint32_t)];
    uint8_t record[0x50];
    uint32_t globals[NOTIFY_CALLER_GLOBALS_SIZE / sizeof(uint32_t)];
    uint32_t tls_index[1] = {0u};
    uint8_t tls_low[0x2c];
    uint8_t tls_slots[0x28];
    uint8_t tls_data[0x2c];
    uint32_t expected_stack[ARRAY_SIZE(stack)];
    uint8_t expected_record[sizeof(record)];
    uint32_t expected_globals[ARRAY_SIZE(globals)];
    uint32_t tls_index_before[ARRAY_SIZE(tls_index)];
    uint8_t tls_low_before[sizeof(tls_low)];
    uint8_t tls_slots_before[sizeof(tls_slots)];
    uint8_t tls_data_before[sizeof(tls_data)];
    RecompMemoryAccess accesses[128];
    const RecompMemoryRegion regions[] = {
        {
            NOTIFY_CALLER_STACK_BASE,
            sizeof(stack),
            (uint8_t *)stack,
        },
        {NOTIFY_CALLER_RECORD, sizeof(record), record},
        {
            NOTIFY_GLOBALS,
            sizeof(globals),
            (uint8_t *)globals,
        },
        {
            NOTIFY_TLS_INDEX,
            sizeof(tls_index),
            (uint8_t *)tls_index,
        },
        {NOTIFY_TLS_LOW, sizeof(tls_low), tls_low},
        {NOTIFY_TLS_SLOTS, sizeof(tls_slots), tls_slots},
        {NOTIFY_TLS_DATA, sizeof(tls_data), tls_data},
    };
    const RecompRegisters initial = {
        0x11112222u,
        0x22223333u,
        0x33334444u,
        0x11223303u,
        NOTIFY_CALLER_RECORD,
        0u,
        0u,
        NOTIFY_CALLER_ENTRY_ESP,
    };
    RecompRegisters expected = initial;

    fill_window(
        (uint8_t *)stack,
        NOTIFY_CALLER_STACK_BASE,
        sizeof(stack));
    fill_window(record, NOTIFY_CALLER_RECORD, sizeof(record));
    fill_window(
        (uint8_t *)globals,
        NOTIFY_GLOBALS,
        sizeof(globals));
    fill_window(tls_low, NOTIFY_TLS_LOW, sizeof(tls_low));
    fill_window(tls_slots, NOTIFY_TLS_SLOTS, sizeof(tls_slots));
    fill_window(tls_data, NOTIFY_TLS_DATA, sizeof(tls_data));
    stack[(NOTIFY_CALLER_ENTRY_ESP - NOTIFY_CALLER_STACK_BASE) / 4u] =
        saved_edi;
    stack[(NOTIFY_CALLER_ENTRY_ESP + 4u -
           NOTIFY_CALLER_STACK_BASE) / 4u] = saved_esi;
    stack[(NOTIFY_CALLER_ENTRY_ESP + 8u -
           NOTIFY_CALLER_STACK_BASE) / 4u] = saved_ebp;
    stack[(NOTIFY_CALLER_ENTRY_ESP + 12u -
           NOTIFY_CALLER_STACK_BASE) / 4u] = saved_ebx;
    stack[(NOTIFY_CALLER_ENTRY_ESP + 16u -
           NOTIFY_CALLER_STACK_BASE) / 4u] = saved_ecx;
    stack[(NOTIFY_CALLER_ENTRY_ESP + 20u -
           NOTIFY_CALLER_STACK_BASE) / 4u] = 0xcafef00du;
    globals[0] = NOTIFY_CALLER_TARGET;
    globals[1] = NOTIFY_CONTEXT;
    globals[(0x003b7f70u - NOTIFY_GLOBALS) / 4u] = 0u;
    globals[(0x003b7f78u - NOTIFY_GLOBALS) / 4u] = 0u;
    memcpy(tls_low + 4u, &tls_slot_address, sizeof(tls_slot_address));
    tls_low[0x24u] = 2u;
    memcpy(tls_low + 0x28u, &tls_data_address, sizeof(tls_data_address));
    memcpy(tls_slots, &tls_data_address, sizeof(tls_data_address));
    memcpy(
        tls_slots + 0x24u,
        &tls_data_address,
        sizeof(tls_data_address));
    memcpy(tls_data + 0x18u, &error_code, sizeof(error_code));
    memcpy(tls_data + 0x28u, &tls_rwdata, sizeof(tls_rwdata));

    memcpy(expected_stack, stack, sizeof(stack));
    memcpy(expected_record, record, sizeof(record));
    memcpy(expected_globals, globals, sizeof(globals));
    memcpy(tls_index_before, tls_index, sizeof(tls_index));
    memcpy(tls_low_before, tls_low, sizeof(tls_low));
    memcpy(tls_slots_before, tls_slots, sizeof(tls_slots));
    memcpy(tls_data_before, tls_data, sizeof(tls_data));
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 4u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 8u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 12u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 16u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = 0u;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 20u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = message;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 24u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = NOTIFY_CONTEXT;
    expected_stack[
        (NOTIFY_CALLER_ENTRY_ESP - 28u -
         NOTIFY_CALLER_STACK_BASE) / 4u] = 0u;
    expected_record[1u] = 3u;
    memcpy(expected_record + 0x44u, &one, sizeof(one));
    memset(expected_record + 0x48u, 0, sizeof(uint32_t));
    expected_globals[(0x003b7f4cu - NOTIFY_GLOBALS) / 4u] = 0u;
    expected.eax = 0u;
    expected.ecx = saved_ecx;
    expected.edx = message;
    expected.ebx = saved_ebx;
    expected.esi = saved_esi;
    expected.edi = saved_edi;
    expected.ebp = saved_ebp;
    expected.esp += 24u;

    interaction_failed = 0;
    interaction_count = 0u;
    notify_caller_expected_message = message;
    recomp_runtime_init(
        regions,
        ARRAY_SIZE(regions),
        accesses,
        ARRAY_SIZE(accesses),
        functions,
        ARRAY_SIZE(functions));
    recomp_runtime.registers = initial;

    if (!recomp_dispatch(0x0019132eu)) {
        fprintf(stderr, "%s: caller dispatch failed\n", case_name);
        return 0;
    }
    if (interaction_failed) {
        fprintf(
            stderr,
            "%s: first divergence %s[%zu/%zu] at esp 0x%08x: args "
            "0x%08x,0x%08x,0x%08x,0x%08x\n",
            case_name,
            failed_interaction_kind,
            failed_actual_index,
            failed_expected_index,
            failed_interaction_esp,
            failed_interaction_args[0],
            failed_interaction_args[1],
            failed_interaction_args[2],
            failed_interaction_args[3]);
        return 0;
    }
    if (interaction_count != 1u) {
        fprintf(
            stderr,
            "%s: first divergence callback count was %zu, expected 1\n",
            case_name,
            interaction_count);
        return 0;
    }
    if (!expect_registers(
            case_name,
            &recomp_runtime.registers,
            &expected) ||
        !expect_bytes(
            case_name,
            "stack",
            stack,
            expected_stack,
            sizeof(stack)) ||
        !expect_bytes(
            case_name,
            "record",
            record,
            expected_record,
            sizeof(record)) ||
        !expect_bytes(
            case_name,
            "notify globals",
            globals,
            expected_globals,
            sizeof(globals)) ||
        !expect_bytes(
            case_name,
            "TLS index",
            tls_index,
            tls_index_before,
            sizeof(tls_index)) ||
        !expect_bytes(
            case_name,
            "TLS low memory",
            tls_low,
            tls_low_before,
            sizeof(tls_low)) ||
        !expect_bytes(
            case_name,
            "TLS slots",
            tls_slots,
            tls_slots_before,
            sizeof(tls_slots)) ||
        !expect_bytes(
            case_name,
            "TLS data",
            tls_data,
            tls_data_before,
            sizeof(tls_data))) {
        return 0;
    }
    if (recomp_runtime.undeclared_access_count != 0u) {
        fprintf(
            stderr,
            "%s: first divergence undeclared_access_count was %zu, "
            "expected 0\n",
            case_name,
            recomp_runtime.undeclared_access_count);
        return 0;
    }

    return 1;
}

int main(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(fixtures); ++i) {
        if (!run_fixture(&fixtures[i])) {
            return EXIT_FAILURE;
        }
    }
    if (!run_outer_fixture(0x0018d9a0u, 0, 0) ||
        !run_outer_fixture(0x0018d9a0u, 1, 0) ||
        !run_outer_fixture(0x0018df30u, 1, 0) ||
        !run_outer_fixture(0x0018df30u, 0, 1) ||
        !run_outer_fixture(0x0018df30u, 0, 2) ||
        !run_outer_fixture(0x0018df30u, 0, 3) ||
        !run_outer_fixture(0x0018df30u, 0, 4) ||
        !run_outer_fixture(0x0018df30u, 0, 5) ||
        !run_outer_fixture(0x0018df30u, 0, 6) ||
        !run_outer_fixture(0x0018df30u, 0, 7) ||
        !run_outer_fixture(0x0018df30u, 0, 8) ||
        !run_outer_fixture(0x0018df30u, 0, 9) ||
        !run_outer_fixture(0x0018df30u, 0, 10) ||
        !run_outer_fixture(0x0018df30u, 0, 11) ||
        !run_outer_fixture(0x0018df30u, 0, 12) ||
        !run_outer_fixture(0x0018df30u, 0, 13) ||
        !run_outer_fixture(0x0018df30u, 0, 14) ||
        !run_outer_fixture(0x0018df30u, 0, 15) ||
        !run_outer_fixture(0x0018df30u, 0, 16) ||
        !run_outer_fixture(0x0018df30u, 0, 17) ||
        !run_outer_fixture(0x0018df90u, 0, 0) ||
        !run_state_caller_skip_fixture() ||
        !run_state_caller_callbacks_fixture() ||
        !run_state_caller_parser_fixture() ||
        !run_guest_memcpy_fixture() ||
        !run_outward_growth_fixture(0) ||
        !run_outward_growth_fixture(1) ||
        !run_state_3_4_computed_fixture(
            "state-4-computed-buffer-write",
            STATE_4_DISPATCH_VALUE) ||
        !run_state_3_4_computed_fixture(
            "state-3-computed-buffer-write",
            STATE_3_DISPATCH_VALUE) ||
        !run_ed80_outward_fixture() ||
        !run_ed80_state_4_fixture() ||
        !run_ed80_state_4_write_fixture(0) ||
        !run_ed80_state_4_write_fixture(1) ||
        !run_ed80_state_2_fixture() ||
        !run_ed80_state_3_fixture(0) ||
        !run_ed80_state_3_fixture(1) ||
        !run_ed80_state_3_fixture(2) ||
        !run_ed80_state_3_fixture(3) ||
        !run_ed80_state_3_fixture(4) ||
        !run_ed80_state_3_fixture(5) ||
        !run_ed80_state_3_fixture(6) ||
        !run_ed80_state_3_fixture(7) ||
        !run_ed80_state_3_fixture(8) ||
        !run_ed80_state_3_fixture(9) ||
        !run_ed80_state_3_fixture(10) ||
        !run_ed80_state_3_fixture(11) ||
        !run_notify_fixture(0) ||
        !run_notify_fixture(1) ||
        !run_notify_caller_fixture(6u) ||
        !run_notify_caller_fixture(0x1eu) ||
        !run_1889a0_outward_fixture() ||
        !run_state_2_computed_fixture(
            0x00196410u,
            "state-2-computed-callbacks",
            STATE_2_DISPATCH_VALUE,
            0u) ||
        !run_state_2_computed_fixture(
            0x00195040u,
            "state-1-dispatched-computed-callbacks",
            1u,
            0u) ||
        !run_state_0_setup_fixture() ||
        !run_state_0_finalize_fixture()) {
        return EXIT_FAILURE;
    }

    puts(
        "recomp runtime: next outward caller branch paths passed "
        "(original-x86 parity provisional)");
    return EXIT_SUCCESS;
}
