#include "motion_demo.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "generated/logos.h"

#if LV_USE_TINY_TTF
#include "src/libs/tiny_ttf/lv_tiny_ttf.h"
#endif

typedef enum {
    STATUS_RUN,
    STATUS_CHECK,
    STATUS_DONE,
    STATUS_WAIT,
    STATUS_ERR,
} StatusType;

typedef enum {
    NOTIFY_NONE,
    NOTIFY_NEW,
    NOTIFY_PENDING,
} NotifyState;

typedef enum {
    MODE_OVERVIEW,
    MODE_NAVIGATING,
    MODE_DECIDING,
} InteractionMode;

typedef enum {
    DECISION_NONE,
    DECISION_COMMAND_PERMISSION,
    DECISION_OPTION_SELECT,
    DECISION_ACKNOWLEDGE,
} DecisionKind;

typedef enum {
    DECISION_PENDING,
    DECISION_ACCEPTED,
    DECISION_REJECTED,
    DECISION_DEFERRED,
} DecisionState;

enum {
    MAX_QUEUE_ITEMS = 10,
    TEMPLATE_COUNT = 8,
    COPY_COUNT = 3,
    VIEW_COUNT = MAX_QUEUE_ITEMS * COPY_COUNT,
    MAX_DECISION_OPTIONS = 3,
    OVERVIEW_VISIBLE_ROWS = 5,
    MIDDLE_COPY = 1,
    VISUAL_OFF = 0,
    VISUAL_OVERVIEW = 218,
    VISUAL_ON = 256,
    TRANSITION_OFF = 0,
    TRANSITION_ON = 256,
    PULSE_OFF = 0,
    PULSE_ON = 256,
    ANIM_SCROLL_MS = 420,
    ANIM_CARD_MS = 320,
    ANIM_DETAIL_MS = 360,
    ANIM_QUEUE_MS = 330,
    ANIM_PULSE_MS = 520,
    OVERVIEW_ROW_HEIGHT = 88,
    OVERVIEW_ROW_MIN_HEIGHT = 84,
    OVERVIEW_ROW_MAX_HEIGHT = 92,
    ROW_CONTENT_TRANSLATE_Y = -24,
    ROW_CONTENT_CENTER_TARGET_Y = -12,
    ROW_CONTENT_CENTER_TOLERANCE = 2,
    ROW_CONTENT_INSET_LEFT = 14,
    ROW_CONTENT_INSET_RIGHT = 12,
    ROW_TEXT_GLYPH_TRANSLATE_Y = -3,
    CAPSULE_LABEL_CENTER_TOLERANCE = 1,
    CARD_BORDER_WIDTH = 2,
    CARD_BORDER_OPA_BASE = 34,
    CARD_BORDER_OPA_OVERVIEW = 48,
    CARD_BORDER_OPA_NOTIFY = 78,
    CARD_BORDER_OPA_FOCUSED = 132,
    CARD_BORDER_OPA_EXPANDED = 158,
    STATUS_HALFTONE_MAX_CELLS = 180,
    RUN_SCAN_SUBPIXEL_SCALE = 256,
};

typedef struct {
    const char * agent;
    const lv_image_dsc_t * logo;
    const char * title;
    StatusType status;
    const char * summary;
    const char * detail;
    DecisionKind decision_kind;
    const char * decision_title;
    const char * evidence;
    const char * recommendation;
    const char * risk;
    const char * command;
    const char * options[MAX_DECISION_OPTIONS];
    int32_t option_count;
} TaskTemplate;

typedef struct {
    uint32_t id;
    char agent[16];
    const lv_image_dsc_t * logo;
    char title[112];
    StatusType status;
    char summary[180];
    char detail[440];
    DecisionKind decision_kind;
    DecisionState decision_state;
    char decision_title[112];
    char evidence[240];
    char recommendation[240];
    char risk[220];
    char command[180];
    char options[MAX_DECISION_OPTIONS][96];
    char decision_result[180];
    int32_t option_count;
    int32_t selected_action;
    NotifyState notify_state;
    bool entering;
    bool exiting;
} TaskModel;

typedef struct {
    StatusType status;
    uint32_t now_ms;
    int32_t strength;
    bool primary;
} StatusCapsuleEffect;

typedef struct {
    lv_obj_t * card;
    lv_obj_t * row;
    lv_obj_t * logo;
    lv_obj_t * text_column;
    lv_obj_t * title;
    lv_obj_t * summary;
    lv_obj_t * meta;
    lv_obj_t * status;
    lv_obj_t * status_label;
    StatusCapsuleEffect status_effect;
    lv_obj_t * slash_label;
    lv_obj_t * age;
    lv_obj_t * age_label;
    StatusCapsuleEffect age_effect;
    lv_obj_t * detail;
    lv_obj_t * detail_title;
    lv_obj_t * detail_label;
    lv_obj_t * evidence_label;
    lv_obj_t * recommendation_label;
    lv_obj_t * risk_label;
    lv_obj_t * decision_card;
    lv_obj_t * decision_action_label;
    lv_obj_t * decision_result_label;
    uint32_t task_id;
    int32_t logical_index;
    int32_t expanded_height;
    int32_t visual;
    int32_t transition;
    int32_t pulse;
    bool bound;
} ItemView;

typedef struct {
    lv_obj_t * root;
    lv_obj_t * list;
    TaskModel queue[MAX_QUEUE_ITEMS];
    ItemView views[VIEW_COUNT];
    int32_t queue_count;
    int32_t focus_seq;
    uint32_t expanded_id;
    uint32_t deleting_id;
    uint32_t delete_focus_id;
    int32_t deleting_index;
    bool deleting_was_expanded;
    uint32_t next_id;
    uint32_t add_serial;
    bool busy;
    bool recenter_after_scroll;
    bool queue_mutation_busy;
    bool focus_active;
    InteractionMode mode;
    bool auto_enabled;
    uint32_t auto_next_ms;
    uint32_t status_effect_now_ms;
    uint32_t entering_id;
    bool binding_views;
    bool anchor_scroll_active;
    bool detail_scroll_active;
    uint32_t detail_scroll_task_id;
    int32_t anchor_scroll_offset;
} DemoState;

static DemoState g_demo;
static uint32_t g_smoke_tick_ms;

static const lv_font_t * g_font_title = &lv_font_montserrat_24;
static const lv_font_t * g_font_summary = &lv_font_montserrat_16;
static const lv_font_t * g_font_capsule = &lv_font_montserrat_14;
static const lv_font_t * g_font_slash = &lv_font_montserrat_22;
static const lv_font_t * g_font_detail = &lv_font_montserrat_18;
static const lv_font_t * g_font_detail_title = &lv_font_montserrat_22;
static const lv_font_t * g_font_decision_result = &lv_font_montserrat_14;

static void detail_exec_cb(void * var, int32_t value);
static void card_visual_exec_cb(void * var, int32_t value);
static void transition_exec_cb(void * var, int32_t value);
static void status_pulse_exec_cb(void * var, int32_t value);
static void anchor_scroll_exec_cb(void * var, int32_t value);
static void keep_focus_scroll_with_current_layout(const ItemView * changed_view);
static void set_focus_visuals_immediate(void);
static void stop_view_animations(ItemView * view);
static void stop_anchor_scroll(void);
static void update_status_views_for_id(uint32_t task_id);
static void apply_overview_row_metrics(void);
static void apply_status_effect(ItemView * view, const TaskModel * task);
static void status_capsule_draw_event_cb(lv_event_t * e);
static void animate_scroll_to_focus(void);
static void follow_detail_scroll_to_focus(void);
static void finish_detail_scroll_follow(void);
static bool detail_scroll_follow_is_complete(void);
static void refresh_focus_visuals(void);
static void set_expanded_id(uint32_t task_id, bool animate);
static void expand_focused_task(bool animate);

static uint32_t smoke_tick_cb(void)
{
    return g_smoke_tick_ms;
}

static void smoke_use_controlled_tick(void)
{
    g_smoke_tick_ms = lv_tick_get();
    lv_tick_set_cb(smoke_tick_cb);
}

static void smoke_advance_frame(uint32_t delta_ms)
{
    g_smoke_tick_ms += delta_ms;
    lv_timer_handler();
    motion_demo_tick(g_smoke_tick_ms);
}

static const TaskTemplate k_templates[TEMPLATE_COUNT] = {
    {
        "Claude",
        &logo_claude,
        "Solid-State Battery Market Analysis for Auto...",
        STATUS_CHECK,
        "Research complete. Comprehensive report generated...",
        "Research complete. Comprehensive report generated with source notes and next action summary ready for review.",
        DECISION_NONE,
        "",
        "",
        "",
        "",
        "",
        { "", "", "" },
        0,
    },
    {
        "Gemini",
        &logo_gemini,
        "Pricing page with Stripe integration",
        STATUS_ERR,
        "Agent failed: TypeError: Cannot read properties of ...",
        "Agent failed: TypeError: Cannot read properties of undefined during checkout wiring.",
        DECISION_OPTION_SELECT,
        "Choose inspection handling",
        "Two visual crops are below confidence threshold; tray id and recipe version still match the current queue.",
        "Hold only the affected station and keep the rest of the queue moving.",
        "A full queue hold adds latency, while ignoring the check may pass an uncertain part downstream.",
        "",
        { "Hold affected station", "Full queue hold", "Continue with note" },
        3,
    },
    {
        "GPT",
        &logo_gpt,
        "Build /audit and /vulnerability-scan...",
        STATUS_DONE,
        "Claude Code wants to run: git commit -m \"feat:...",
        "Claude Code wants to run: git commit -m \"feat: add security audit workflow\"",
        DECISION_NONE,
        "",
        "",
        "",
        "",
        "",
        { "", "", "" },
        0,
    },
    {
        "DeepSeek",
        &logo_deepseek,
        "Extract the discount calculation...",
        STATUS_RUN,
        "Running Bash tool: pytest tests/billing/ (5 test...",
        "Running Bash tool: pytest tests/billing/ (5 tests still streaming)",
        DECISION_NONE,
        "",
        "",
        "",
        "",
        "",
        { "", "", "" },
        0,
    },
    {
        "Claude",
        &logo_claude,
        "LLM Reasoning Capabilities Meta-Analysis",
        STATUS_RUN,
        "Reading arxiv.org... (Consulted 28 sources so far)",
        "Reading arxiv.org... (Consulted 28 sources so far)",
        DECISION_ACKNOWLEDGE,
        "Confirm recovery path",
        "Torque stayed outside the expected band for three consecutive samples after retry.",
        "Acknowledge manual recovery, keep the last safe step visible, then release the fixture after inspection.",
        "Deferring keeps the task waiting and prevents the route from being marked recoverable.",
        "",
        { "Acknowledge recovery", "Defer review", "" },
        2,
    },
    {
        "Gemini",
        &logo_gemini,
        "Material readiness ranking is updating",
        STATUS_RUN,
        "Ranking tray readiness and downstream idle time...",
        "Gemini is ranking queued tasks by tray readiness, fixture occupancy, and downstream idle time. The next task candidate is promoted only when all three checks agree, keeping queue motion smooth during rapid task changes.",
        DECISION_NONE,
        "",
        "",
        "",
        "",
        "",
        { "", "", "" },
        0,
    },
    {
        "GPT",
        &logo_gpt,
        "Release checklist is under verification",
        STATUS_CHECK,
        "Verifying release checklist against current recipe...",
        "GPT is verifying the release checklist against the current product recipe, the last station state, and the operator acknowledgement history. Failed checks remain grouped by station so the intervention flow stays readable.",
        DECISION_COMMAND_PERMISSION,
        "Approve release checklist command",
        "Checklist failed on two acknowledgement records, but station state and product recipe are consistent.",
        "Run the simulated checklist refresh and continue only if the command reports clean.",
        "Rejecting leaves the queue in WAIT; approving is simulated here and does not execute a system command.",
        "agentctl refresh-checklist --station S2 --dry-run",
        { "Approve command", "Reject command", "" },
        2,
    },
    {
        "DeepSeek",
        &logo_deepseek,
        "Dashboard event history has been compressed",
        STATUS_DONE,
        "Compressed event history for task dashboard...",
        "DeepSeek compressed the event history into a short block for the task dashboard and a compact reminder for the square screen. The detail keeps the original timestamp range, severity count, and selected recovery action.",
        DECISION_NONE,
        "",
        "",
        "",
        "",
        "",
        { "", "", "" },
        0,
    },
};

static void copy_text(char * dst, size_t dst_size, const char * src)
{
    if(dst_size == 0) return;
    if(src == NULL) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static lv_color_t color_hex(uint32_t hex)
{
    return lv_color_hex(hex);
}

static int32_t clamp_i32(int32_t value, int32_t low, int32_t high)
{
    if(value < low) return low;
    if(value > high) return high;
    return value;
}

static int32_t abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t positive_mod_i32(int32_t value, int32_t modulus)
{
    if(modulus <= 0) return 0;
    int32_t result = value % modulus;
    return result < 0 ? result + modulus : result;
}

static int32_t isqrt_i32(int32_t value)
{
    if(value <= 0) return 0;

    int32_t low = 0;
    int32_t high = value < 46340 ? value : 46340;
    while(low <= high) {
        int32_t mid = low + (high - low) / 2;
        int32_t square = mid * mid;
        if(square == value) return mid;
        if(square < value) low = mid + 1;
        else high = mid - 1;
    }
    return high;
}

static void set_classic_smooth_motion(lv_anim_t * anim, uint32_t duration_ms)
{
    lv_anim_set_duration(anim, duration_ms);
    lv_anim_set_path_cb(anim, lv_anim_path_custom_bezier3);
    LV_ANIM_SET_EASE_OUT_QUINT(anim);
}

static const char * status_text(StatusType status)
{
    switch(status) {
        case STATUS_RUN: return "RUN";
        case STATUS_CHECK: return "CHECK";
        case STATUS_DONE: return "APPROVE";
        case STATUS_WAIT: return "WAIT";
        case STATUS_ERR: return "ERROR";
        default: return "?";
    }
}

static lv_color_t status_color(StatusType status)
{
    switch(status) {
        case STATUS_RUN: return color_hex(0x4A8BC5);
        case STATUS_CHECK: return color_hex(0xF1F1F0);
        case STATUS_DONE: return color_hex(0x267A4B);
        case STATUS_WAIT: return color_hex(0x4A8BC5);
        case STATUS_ERR: return color_hex(0xC43B35);
        default: return color_hex(0x8B96A8);
    }
}

static lv_color_t status_highlight_color(StatusType status)
{
    switch(status) {
        case STATUS_RUN: return color_hex(0x8EC9FF);
        case STATUS_CHECK: return color_hex(0xFFFFFF);
        case STATUS_DONE: return color_hex(0x63D28E);
        case STATUS_WAIT: return color_hex(0x7FB7EC);
        case STATUS_ERR: return color_hex(0xFF8078);
        default: return color_hex(0xDDE7F3);
    }
}

static uint32_t status_effect_period_ms(StatusType status)
{
    switch(status) {
        case STATUS_ERR: return 680;
        case STATUS_CHECK: return 980;
        case STATUS_RUN: return 1480;
        case STATUS_WAIT: return 2200;
        case STATUS_DONE:
        default: return 2600;
    }
}

static int32_t status_effect_strength(StatusType status)
{
    switch(status) {
        case STATUS_ERR: return 118;
        case STATUS_CHECK: return 56;
        case STATUS_RUN: return 78;
        case STATUS_WAIT: return 46;
        case STATUS_DONE:
        default: return 18;
    }
}

static int32_t triangle_wave_u8(uint32_t now_ms, uint32_t period_ms)
{
    if(period_ms == 0) return 0;
    uint32_t phase = now_ms % period_ms;
    uint32_t half = period_ms / 2;
    if(half == 0) return 0;
    if(phase > half) phase = period_ms - phase;
    return (int32_t)((phase * 255U) / half);
}

static int32_t run_scan_head_scaled(uint32_t now_ms, int32_t visible_width, int32_t band_width, bool primary)
{
    if(visible_width <= 0 || band_width <= 0) return 0;

    uint32_t period = status_effect_period_ms(STATUS_RUN);
    uint32_t offset_ms = primary ? 0U : period / 5U;
    uint32_t phase = period > 0 ? (now_ms + offset_ms) % period : 0;
    int32_t scaled_path = (visible_width + (band_width * 2)) * RUN_SCAN_SUBPIXEL_SCALE;
    int32_t scan = period > 0 ? (int32_t)((phase * (uint32_t)scaled_path) / period) : 0;
    return scan - (band_width * RUN_SCAN_SUBPIXEL_SCALE);
}

static int32_t run_scan_head(uint32_t now_ms, int32_t visible_width, int32_t band_width, bool primary)
{
    return run_scan_head_scaled(now_ms, visible_width, band_width, primary) / RUN_SCAN_SUBPIXEL_SCALE;
}

static lv_color_t status_text_color(StatusType status)
{
    return status == STATUS_CHECK ? color_hex(0x202020) : color_hex(0xF1F2F2);
}

static const char * task_age_text(const TaskModel * task)
{
    if(task == NULL) return "";
    switch(task->status) {
        case STATUS_CHECK: return "6m";
        case STATUS_ERR: return "6m";
        case STATUS_DONE: return "24m";
        case STATUS_WAIT: return "8m";
        case STATUS_RUN:
        default: return task->logo == &logo_gpt ? "16m" : "8m";
    }
}

static bool status_requires_response(StatusType status)
{
    return status == STATUS_CHECK || status == STATUS_ERR;
}

static bool decision_is_actionable(const TaskModel * task)
{
    return task != NULL && task->decision_kind != DECISION_NONE && task->decision_state == DECISION_PENDING;
}

static int32_t decision_action_count(const TaskModel * task)
{
    if(task == NULL) return 0;
    if(task->decision_kind == DECISION_OPTION_SELECT) {
        return clamp_i32(task->option_count, 1, MAX_DECISION_OPTIONS);
    }
    if(task->decision_kind == DECISION_COMMAND_PERMISSION || task->decision_kind == DECISION_ACKNOWLEDGE) {
        return 2;
    }
    return 0;
}

static StatusType next_status(StatusType status)
{
    switch(status) {
        case STATUS_RUN: return STATUS_CHECK;
        case STATUS_CHECK: return STATUS_DONE;
        case STATUS_DONE: return STATUS_WAIT;
        case STATUS_WAIT: return STATUS_ERR;
        case STATUS_ERR: return STATUS_RUN;
        default: return STATUS_RUN;
    }
}

static int32_t logical_focus(void)
{
    if(g_demo.queue_count <= 0) return 0;
    return positive_mod_i32(g_demo.focus_seq, g_demo.queue_count);
}

static bool use_loop_copies(void)
{
    return g_demo.queue_count >= 4;
}

static int32_t copy_for_focus(void)
{
    if(g_demo.queue_count <= 0) return MIDDLE_COPY;
    if(!use_loop_copies()) return MIDDLE_COPY;
    return clamp_i32(g_demo.focus_seq / g_demo.queue_count, 0, COPY_COUNT - 1);
}

static int32_t view_index_for(int32_t copy, int32_t logical_index)
{
    return (copy * MAX_QUEUE_ITEMS) + logical_index;
}

static int32_t focused_view_index(void)
{
    return view_index_for(copy_for_focus(), logical_focus());
}

static TaskModel * task_by_id(uint32_t id)
{
    for(int32_t i = 0; i < g_demo.queue_count; ++i) {
        if(g_demo.queue[i].id == id) return &g_demo.queue[i];
    }
    return NULL;
}

static int32_t find_task_index_by_id(uint32_t id)
{
    for(int32_t i = 0; i < g_demo.queue_count; ++i) {
        if(g_demo.queue[i].id == id) return i;
    }
    return -1;
}

static void assign_template(TaskModel * task, const TaskTemplate * tmpl, uint32_t id)
{
    task->id = id;
    copy_text(task->agent, sizeof(task->agent), tmpl->agent);
    task->logo = tmpl->logo;
    copy_text(task->title, sizeof(task->title), tmpl->title);
    copy_text(task->summary, sizeof(task->summary), tmpl->summary);
    copy_text(task->detail, sizeof(task->detail), tmpl->detail);
    task->status = tmpl->status;
    task->decision_kind = tmpl->decision_kind;
    task->decision_state = DECISION_PENDING;
    copy_text(task->decision_title, sizeof(task->decision_title), tmpl->decision_title);
    copy_text(task->evidence, sizeof(task->evidence), tmpl->evidence);
    copy_text(task->recommendation, sizeof(task->recommendation), tmpl->recommendation);
    copy_text(task->risk, sizeof(task->risk), tmpl->risk);
    copy_text(task->command, sizeof(task->command), tmpl->command);
    task->option_count = clamp_i32(tmpl->option_count, 0, MAX_DECISION_OPTIONS);
    for(int32_t i = 0; i < MAX_DECISION_OPTIONS; ++i) {
        copy_text(task->options[i], sizeof(task->options[i]), tmpl->options[i]);
    }
    task->selected_action = 0;
    copy_text(task->decision_result, sizeof(task->decision_result), "");
    task->notify_state = status_requires_response(task->status) ? NOTIFY_PENDING : NOTIFY_NONE;
    task->entering = false;
    task->exiting = false;
}

static int32_t find_default_focus_index(void)
{
    StatusType priorities[] = { STATUS_ERR, STATUS_CHECK, STATUS_RUN };
    for(size_t p = 0; p < sizeof(priorities) / sizeof(priorities[0]); ++p) {
        for(int32_t i = 0; i < g_demo.queue_count; ++i) {
            if(g_demo.queue[i].status == priorities[p]) return i;
        }
    }
    return g_demo.queue_count > 0 ? 0 : -1;
}

static int32_t find_next_pending_from(int32_t start_index)
{
    if(g_demo.queue_count <= 0) return -1;
    for(int32_t offset = 0; offset < g_demo.queue_count; ++offset) {
        int32_t index = positive_mod_i32(start_index + offset, g_demo.queue_count);
        TaskModel * task = &g_demo.queue[index];
        if(task->notify_state != NOTIFY_NONE || status_requires_response(task->status)) {
            return index;
        }
    }
    return -1;
}

static int32_t find_post_delete_focus_old_index(int32_t delete_index)
{
    int32_t remaining = g_demo.queue_count - 1;
    if(remaining <= 0) return -1;

    for(int32_t offset = 0; offset < remaining; ++offset) {
        int32_t post_index = positive_mod_i32(delete_index + offset, remaining);
        int32_t old_index = post_index >= delete_index ? post_index + 1 : post_index;
        TaskModel * task = &g_demo.queue[old_index];
        if(task->notify_state != NOTIFY_NONE || status_requires_response(task->status)) {
            return old_index;
        }
    }

    if(delete_index < g_demo.queue_count - 1) {
        return delete_index + 1;
    }
    return delete_index - 1;
}

static void set_focus_to_logical(int32_t logical_index)
{
    if(g_demo.queue_count <= 0) {
        g_demo.focus_seq = 0;
        return;
    }
    g_demo.focus_seq = g_demo.queue_count + positive_mod_i32(logical_index, g_demo.queue_count);
}

static void sync_focus_to_current_id(uint32_t current_id)
{
    int32_t index = find_task_index_by_id(current_id);
    if(index >= 0) {
        set_focus_to_logical(index);
    }
    else if(g_demo.queue_count > 0) {
        set_focus_to_logical(clamp_i32(logical_focus(), 0, g_demo.queue_count - 1));
    }
}

static void update_task_notify(TaskModel * task)
{
    if(task->status == STATUS_DONE) {
        task->notify_state = NOTIFY_NONE;
    }
    else if(status_requires_response(task->status) && task->notify_state == NOTIFY_NONE) {
        task->notify_state = NOTIFY_PENDING;
    }
}

static void mark_current_new_seen(void)
{
    if(g_demo.queue_count <= 0) return;
    TaskModel * task = &g_demo.queue[logical_focus()];
    if(task->notify_state == NOTIFY_NEW && !status_requires_response(task->status)) {
        task->notify_state = NOTIFY_NONE;
    }
}

static void set_card_style(ItemView * view)
{
    TaskModel * task = task_by_id(view->task_id);
    bool focused = g_demo.focus_active && view->bound && focused_view_index() == (int32_t)(view - g_demo.views);
    bool overview = !g_demo.focus_active && g_demo.expanded_id == 0;
    bool expanded = view->bound && view->task_id == g_demo.expanded_id;
    bool notified = task != NULL && task->notify_state != NOTIFY_NONE;

    int32_t visual = overview ? VISUAL_OVERVIEW : clamp_i32(view->visual, VISUAL_OFF, VISUAL_ON);
    int32_t transition = clamp_i32(view->transition, TRANSITION_OFF, TRANSITION_ON);
    int32_t enter_scale = 236 + ((20 * transition) / TRANSITION_ON);
    int32_t scale = transition < TRANSITION_ON ? enter_scale : 256;
    int32_t opacity = (180 + ((75 * visual) / VISUAL_ON)) * transition / TRANSITION_ON;
    int32_t bg_opa = expanded ? 58 : (focused ? 32 : (notified ? 12 : 0));
    int32_t border_opa = expanded ? CARD_BORDER_OPA_EXPANDED :
                          (focused ? CARD_BORDER_OPA_FOCUSED :
                           (notified ? CARD_BORDER_OPA_NOTIFY :
                            (overview ? CARD_BORDER_OPA_OVERVIEW : CARD_BORDER_OPA_BASE)));
    int32_t pulse_opa = view->pulse / 3;
    lv_color_t accent = task != NULL ? status_color(task->status) : color_hex(0x2E3B4E);
    lv_color_t border_color = focused || expanded || notified ? accent : color_hex(0x56606A);

    lv_obj_set_style_bg_color(view->card, expanded ? color_hex(0x202427) : color_hex(0x1A1D1C), 0);
    lv_obj_set_style_bg_opa(view->card, (lv_opa_t)clamp_i32(bg_opa + pulse_opa, LV_OPA_TRANSP, LV_OPA_80), 0);
    lv_obj_set_style_transform_scale_x(view->card, scale, 0);
    lv_obj_set_style_transform_scale_y(view->card, scale, 0);
    lv_obj_set_style_shadow_width(view->card, focused || expanded ? 12 : 0, 0);
    lv_obj_set_style_border_width(view->card, CARD_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(view->card, border_color, 0);
    lv_obj_set_style_border_opa(view->card, (lv_opa_t)clamp_i32(border_opa, LV_OPA_TRANSP, LV_OPA_COVER), 0);
    lv_obj_set_style_opa(view->card, (lv_opa_t)clamp_i32(opacity, LV_OPA_TRANSP, LV_OPA_COVER), 0);
    if(task != NULL) {
        int32_t capsule_scale = 256 + (view->pulse / 18);
        lv_obj_set_style_transform_scale_x(view->status, capsule_scale, 0);
        lv_obj_set_style_transform_scale_y(view->status, capsule_scale, 0);
        lv_obj_set_style_transform_scale_x(view->age, capsule_scale, 0);
        lv_obj_set_style_transform_scale_y(view->age, capsule_scale, 0);
        apply_status_effect(view, task);
    }
}

static int32_t target_visual_for_view(int32_t view_index)
{
    if(!g_demo.focus_active && g_demo.expanded_id == 0) {
        return VISUAL_OVERVIEW;
    }
    return view_index == focused_view_index() ? VISUAL_ON : VISUAL_OFF;
}

static void card_visual_exec_cb(void * var, int32_t value)
{
    ItemView * view = (ItemView *)var;
    view->visual = clamp_i32(value, VISUAL_OFF, VISUAL_ON);
    set_card_style(view);
}

static void status_pulse_exec_cb(void * var, int32_t value)
{
    ItemView * view = (ItemView *)var;
    view->pulse = clamp_i32(value, PULSE_OFF, PULSE_ON);
    set_card_style(view);
}

static int32_t current_detail_target(ItemView * view)
{
    return view->bound && view->task_id == g_demo.expanded_id ? view->expanded_height : 0;
}

static int32_t current_item_scroll_target(int32_t view_index);

static int32_t predicted_card_height(ItemView * view, int32_t target_detail_height)
{
    lv_obj_update_layout(g_demo.root);

    int32_t predicted = lv_obj_get_height(view->row);
    predicted += lv_obj_get_style_pad_top(view->card, 0);
    predicted += lv_obj_get_style_pad_bottom(view->card, 0);

    if(target_detail_height > 0) {
        predicted += lv_obj_get_style_pad_row(view->card, 0);
        predicted += target_detail_height;
    }

    return LV_MAX(predicted, OVERVIEW_ROW_HEIGHT);
}

static void transition_exec_cb(void * var, int32_t value)
{
    ItemView * view = (ItemView *)var;
    int32_t transition = clamp_i32(value, TRANSITION_OFF, TRANSITION_ON);
    int32_t target_h = predicted_card_height(view, current_detail_target(view));
    view->transition = transition;

    if(transition >= TRANSITION_ON && view->bound) {
        lv_obj_set_height(view->card, LV_SIZE_CONTENT);
    }
    else {
        lv_obj_set_height(view->card, LV_MAX((target_h * transition) / TRANSITION_ON, 0));
    }
    set_card_style(view);
    keep_focus_scroll_with_current_layout(view);
}

static void detail_exec_cb(void * var, int32_t value)
{
    ItemView * view = (ItemView *)var;
    int32_t detail_height = LV_MAX(value, 0);
    int32_t target = view->expanded_height > 0 ? view->expanded_height : 1;
    int32_t opacity = (detail_height * LV_OPA_COVER) / target;
    if(detail_height > 0 || view->task_id == g_demo.expanded_id) {
        lv_obj_clear_flag(view->detail, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_height(view->detail, detail_height);
    lv_obj_set_style_opa(view->detail, (lv_opa_t)clamp_i32(opacity, LV_OPA_TRANSP, LV_OPA_COVER), 0);
    lv_obj_set_height(view->card, predicted_card_height(view, detail_height));
    if(detail_height <= 0 && view->task_id != g_demo.expanded_id) {
        lv_obj_add_flag(view->detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(view->card, OVERVIEW_ROW_HEIGHT);
    }
    keep_focus_scroll_with_current_layout(view);
}

static int32_t final_item_content_y(int32_t view_index)
{
    lv_obj_update_layout(g_demo.root);
    return lv_obj_get_y(g_demo.views[view_index].card);
}

static int32_t current_item_content_y(int32_t view_index)
{
    lv_obj_update_layout(g_demo.root);
    return lv_obj_get_y(g_demo.views[view_index].card);
}

static int32_t current_item_scroll_target(int32_t view_index)
{
    if(g_demo.queue_count <= 0) return 0;
    lv_obj_update_layout(g_demo.root);

    ItemView * view = &g_demo.views[view_index];
    int32_t list_h = lv_obj_get_height(g_demo.list);
    int32_t card_h = lv_obj_get_height(view->card);
    int32_t detail_h = lv_obj_get_height(view->detail);
    int32_t collapsed_space = list_h - card_h;
    int32_t collapsed_offset = collapsed_space > 0 ? collapsed_space / 2 : 0;
    int32_t peek_space = list_h - card_h;
    int32_t expanded_offset = peek_space > 0 ? peek_space / 2 : 0;
    int32_t detail_ref = view->expanded_height > 0 ? view->expanded_height : 1;
    int32_t mix = clamp_i32((detail_h * 256) / detail_ref, 0, 256);
    int32_t offset = collapsed_offset + (((expanded_offset - collapsed_offset) * mix) / 256);

    return current_item_content_y(view_index) - offset;
}

static int32_t item_scroll_target_for_detail_height(int32_t view_index, int32_t target_detail_height)
{
    if(g_demo.queue_count <= 0) return 0;
    lv_obj_update_layout(g_demo.root);

    ItemView * view = &g_demo.views[view_index];
    int32_t list_h = lv_obj_get_height(g_demo.list);
    int32_t card_h = predicted_card_height(view, target_detail_height);
    int32_t collapsed_space = list_h - card_h;
    int32_t collapsed_offset = collapsed_space > 0 ? collapsed_space / 2 : 0;
    int32_t peek_space = list_h - card_h;
    int32_t expanded_offset = peek_space > 0 ? peek_space / 2 : 0;
    int32_t detail_ref = view->expanded_height > 0 ? view->expanded_height : 1;
    int32_t mix = clamp_i32((target_detail_height * 256) / detail_ref, 0, 256);
    int32_t offset = collapsed_offset + (((expanded_offset - collapsed_offset) * mix) / 256);

    return final_item_content_y(view_index) - offset;
}

static int32_t item_scroll_target(int32_t view_index)
{
    return item_scroll_target_for_detail_height(view_index, current_detail_target(&g_demo.views[view_index]));
}

static void keep_focus_scroll_with_current_layout(const ItemView * changed_view)
{
    if((!g_demo.anchor_scroll_active && !g_demo.detail_scroll_active) || g_demo.binding_views || g_demo.queue_count <= 0) return;

    int32_t focused = focused_view_index();
    int32_t changed = (int32_t)(changed_view - g_demo.views);
    if(changed > focused) return;

    lv_obj_scroll_to_y(g_demo.list, current_item_scroll_target(focused) + (g_demo.anchor_scroll_active ? g_demo.anchor_scroll_offset : 0), LV_ANIM_OFF);
}

static void bind_status(ItemView * view, const TaskModel * task)
{
    lv_label_set_text(view->status_label, status_text(task->status));
    lv_obj_set_style_bg_color(view->status, status_color(task->status), 0);
    lv_obj_set_style_text_color(view->status_label, status_text_color(task->status), 0);
    lv_label_set_text(view->age_label, task_age_text(task));
    lv_obj_set_style_bg_color(view->age, status_color(task->status), 0);
    lv_obj_set_style_text_color(view->age_label, status_text_color(task->status), 0);
    apply_status_effect(view, task);
}

static bool point_is_inside_rounded_mask(const lv_area_t * mask, int32_t radius, int32_t x, int32_t y)
{
    if(mask == NULL) return false;
    if(x < mask->x1 || x > mask->x2 || y < mask->y1 || y > mask->y2) return false;

    int32_t width = lv_area_get_width(mask);
    int32_t height = lv_area_get_height(mask);
    radius = clamp_i32(radius, 0, LV_MIN(width, height) / 2);
    if(radius <= 0) return true;

    int32_t left = mask->x1 + radius;
    int32_t right = mask->x2 - radius;
    int32_t top = mask->y1 + radius;
    int32_t bottom = mask->y2 - radius;
    int32_t cx = x < left ? left : (x > right ? right : x);
    int32_t cy = y < top ? top : (y > bottom ? bottom : y);
    int32_t dx = x - cx;
    int32_t dy = y - cy;
    return (dx * dx) + (dy * dy) <= radius * radius;
}

static bool area_is_inside_rounded_mask(const lv_area_t * area, const lv_area_t * mask, int32_t radius)
{
    return area != NULL && mask != NULL &&
           point_is_inside_rounded_mask(mask, radius, area->x1, area->y1) &&
           point_is_inside_rounded_mask(mask, radius, area->x2, area->y1) &&
           point_is_inside_rounded_mask(mask, radius, area->x1, area->y2) &&
           point_is_inside_rounded_mask(mask, radius, area->x2, area->y2);
}

static bool fit_area_to_rounded_mask(lv_area_t * area, const lv_area_t * mask, int32_t radius)
{
    if(area == NULL || mask == NULL) return false;

    area->x1 = LV_MAX(area->x1, mask->x1);
    area->x2 = LV_MIN(area->x2, mask->x2);
    area->y1 = LV_MAX(area->y1, mask->y1);
    area->y2 = LV_MIN(area->y2, mask->y2);

    while(area->x1 <= area->x2 && area->y1 <= area->y2 && !area_is_inside_rounded_mask(area, mask, radius)) {
        bool shrink_left = !point_is_inside_rounded_mask(mask, radius, area->x1, area->y1) ||
                           !point_is_inside_rounded_mask(mask, radius, area->x1, area->y2);
        bool shrink_right = !point_is_inside_rounded_mask(mask, radius, area->x2, area->y1) ||
                            !point_is_inside_rounded_mask(mask, radius, area->x2, area->y2);
        bool shrink_top = !point_is_inside_rounded_mask(mask, radius, area->x1, area->y1) ||
                          !point_is_inside_rounded_mask(mask, radius, area->x2, area->y1);
        bool shrink_bottom = !point_is_inside_rounded_mask(mask, radius, area->x1, area->y2) ||
                             !point_is_inside_rounded_mask(mask, radius, area->x2, area->y2);

        if(shrink_left) area->x1++;
        if(shrink_right) area->x2--;
        if(shrink_top) area->y1++;
        if(shrink_bottom) area->y2--;
        if(!shrink_left && !shrink_right && !shrink_top && !shrink_bottom) break;
    }

    return area->x1 <= area->x2 && area->y1 <= area->y2;
}

static bool row_span_for_rounded_mask(const lv_area_t * mask, int32_t radius, int32_t y1, int32_t y2, int32_t * x1, int32_t * x2)
{
    if(mask == NULL || x1 == NULL || x2 == NULL) return false;

    int32_t width = lv_area_get_width(mask);
    int32_t height = lv_area_get_height(mask);
    radius = clamp_i32(radius, 0, LV_MIN(width, height) / 2);

    int32_t top = mask->y1 + radius;
    int32_t bottom = mask->y2 - radius;
    int32_t inset = 0;

    if(radius > 0 && y1 < top) {
        int32_t dy = top - y1;
        inset = LV_MAX(inset, radius - isqrt_i32(LV_MAX(0, (radius * radius) - (dy * dy))));
    }
    if(radius > 0 && y2 > bottom) {
        int32_t dy = y2 - bottom;
        inset = LV_MAX(inset, radius - isqrt_i32(LV_MAX(0, (radius * radius) - (dy * dy))));
    }

    *x1 = mask->x1 + inset;
    *x2 = mask->x2 - inset;
    return *x1 <= *x2;
}

static int32_t estimate_run_halftone_cells(const lv_area_t * inner,
                                           int32_t head,
                                           int32_t band_width,
                                           int32_t cell)
{
    if(inner == NULL || band_width <= 0 || cell <= 0) return 0;

    int32_t half = LV_MAX(1, band_width / 2);
    int32_t cells = 0;
    for(int32_t y = inner->y1; y <= inner->y2; y += cell) {
        int32_t row = (y - inner->y1) / cell;
        int32_t row_phase = (row & 1) ? cell / 2 : 0;

        for(int32_t x = inner->x1 - row_phase; x <= inner->x2; x += cell) {
            int32_t sample_x = (x - inner->x1) + row_phase + (cell / 2);
            if(abs_i32(sample_x - head) <= half) cells++;
        }
    }
    return cells;
}

static int32_t estimate_run_halftone_energy(const lv_area_t * inner,
                                            int32_t head_scaled,
                                            int32_t band_width,
                                            int32_t cell)
{
    if(inner == NULL || band_width <= 0 || cell <= 0) return 0;

    int32_t half_scaled = LV_MAX(RUN_SCAN_SUBPIXEL_SCALE, (band_width * RUN_SCAN_SUBPIXEL_SCALE) / 2);
    int32_t energy = 0;
    for(int32_t y = inner->y1; y <= inner->y2; y += cell) {
        int32_t row = (y - inner->y1) / cell;
        int32_t row_phase = (row & 1) ? cell / 2 : 0;

        for(int32_t x = inner->x1 - row_phase; x <= inner->x2; x += cell) {
            int32_t sample_x = (x - inner->x1) + row_phase + (cell / 2);
            int32_t dist = abs_i32((sample_x * RUN_SCAN_SUBPIXEL_SCALE) - head_scaled);
            if(dist > half_scaled) continue;

            int32_t fade = 255 - ((dist * 255) / half_scaled);
            int32_t eased = (fade * fade * (765 - (2 * fade))) / (255 * 255);
            energy += eased;
        }
    }
    return energy;
}

static void draw_run_scan_halftone(lv_layer_t * layer,
                                   const lv_area_t * inner,
                                   const lv_area_t * mask,
                                   int32_t mask_radius,
                                   int32_t head_scaled,
                                   int32_t band_width,
                                   lv_color_t color,
                                   lv_opa_t max_opa,
                                   int32_t cell)
{
    if(layer == NULL || inner == NULL || mask == NULL || band_width <= 0 || cell <= 0) return;

    int32_t half_scaled = LV_MAX(RUN_SCAN_SUBPIXEL_SCALE, (band_width * RUN_SCAN_SUBPIXEL_SCALE) / 2);
    int32_t inner_height = lv_area_get_height(inner);
    int32_t dot_min = LV_MAX(1, cell / 3);
    int32_t dot_max = LV_MAX(dot_min, cell - LV_MAX(1, cell / 5));

    for(int32_t y = inner->y1; y <= inner->y2; y += cell) {
        int32_t row = (y - inner->y1) / cell;
        int32_t row_phase = (row & 1) ? cell / 2 : 0;

        for(int32_t x = inner->x1 - row_phase; x <= inner->x2; x += cell) {
            int32_t sample_x = (x - inner->x1) + row_phase + (cell / 2);
            int32_t sample_y = (y - inner->y1) + (cell / 2);
            int32_t sample_scaled = sample_x * RUN_SCAN_SUBPIXEL_SCALE;
            int32_t dist = abs_i32(sample_scaled - head_scaled);
            if(dist > half_scaled) continue;

            int32_t fade = 255 - ((dist * 255) / half_scaled);
            int32_t eased = (fade * fade * (765 - (2 * fade))) / (255 * 255);
            int32_t vertical = inner_height > 0 ? LV_MIN(sample_y, inner_height - sample_y) : 0;
            int32_t edge_lift = inner_height > 0 ? 190 + ((65 * vertical) / LV_MAX(1, inner_height / 2)) : 255;
            int32_t level = (eased * edge_lift) / 255;
            int32_t dot = dot_min + (((dot_max - dot_min) * level) / 255);
            lv_opa_t opa = (lv_opa_t)clamp_i32(((int32_t)max_opa * (96 + level)) / 351, LV_OPA_TRANSP, LV_OPA_COVER);
            if(dot <= 0 || opa <= LV_OPA_TRANSP) continue;

            int32_t cx = x + cell / 2;
            int32_t cy = y + cell / 2;
            int32_t py1 = LV_MAX(cy - dot / 2, inner->y1);
            int32_t py2 = LV_MIN(cy + (dot - 1) / 2, inner->y2);
            int32_t mask_x1;
            int32_t mask_x2;
            if(py1 > py2 || !row_span_for_rounded_mask(mask, mask_radius, py1, py2, &mask_x1, &mask_x2)) continue;

            lv_area_t pixel = {
                .x1 = LV_MAX(LV_MAX(cx - dot / 2, inner->x1), mask_x1),
                .x2 = LV_MIN(LV_MIN(cx + (dot - 1) / 2, inner->x2), mask_x2),
                .y1 = py1,
                .y2 = py2,
            };
            if(pixel.x1 > pixel.x2 || pixel.y1 > pixel.y2) continue;

            lv_draw_fill_dsc_t fill_dsc;
            lv_draw_fill_dsc_init(&fill_dsc);
            fill_dsc.base.layer = layer;
            fill_dsc.color = color;
            fill_dsc.opa = opa;
            fill_dsc.radius = 0;
            lv_draw_fill(layer, &fill_dsc, &pixel);
        }
    }
}

static void status_capsule_draw_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_DRAW_MAIN_END) return;

    StatusCapsuleEffect * effect = (StatusCapsuleEffect *)lv_event_get_user_data(e);
    if(effect == NULL || effect->status != STATUS_RUN) return;

    lv_obj_t * capsule = lv_event_get_current_target_obj(e);
    lv_area_t coords;
    lv_obj_get_coords(capsule, &coords);
    int32_t width = lv_area_get_width(&coords);
    int32_t height = lv_area_get_height(&coords);
    if(width <= 0 || height <= 0) return;

    lv_area_t inner = coords;
    int32_t inner_width = lv_area_get_width(&inner);
    int32_t inner_height = lv_area_get_height(&inner);
    int32_t band_width = effect->primary ? LV_MAX(inner_height * 2, inner_width / 2) : LV_MAX((inner_height * 3) / 2, (inner_width * 2) / 5);
    int32_t core_width = LV_MAX(1, band_width / 3);
    int32_t head_scaled = run_scan_head_scaled(effect->now_ms, inner_width, band_width, effect->primary);
    lv_color_t glow = status_highlight_color(STATUS_RUN);
    lv_layer_t * layer = lv_event_get_layer(e);
    int32_t cell = LV_MAX(3, inner_height / (effect->primary ? 5 : 4));
    int32_t core_cell = effect->primary ? LV_MAX(3, (cell * 2) / 3) : cell;
    int32_t mask_radius = LV_MIN(lv_obj_get_style_radius(capsule, 0), LV_MIN(width, height) / 2);

    draw_run_scan_halftone(layer, &inner, &coords, mask_radius, head_scaled, band_width, glow, effect->primary ? LV_OPA_70 : LV_OPA_40, cell);
    draw_run_scan_halftone(layer, &inner, &coords, mask_radius, head_scaled, core_width, color_hex(0xE8F5FF), effect->primary ? LV_OPA_80 : LV_OPA_40, core_cell);
}

static void apply_capsule_effect(lv_obj_t * capsule, StatusCapsuleEffect * effect, StatusType status, uint32_t now_ms, int32_t strength, bool primary)
{
    lv_color_t color = status_color(status);
    uint32_t period = status_effect_period_ms(status);
    int32_t wave = triangle_wave_u8(now_ms, period);
    int32_t glow = (strength * wave) / 255;
    int32_t base_opa = status == STATUS_CHECK ? 230 : 220;
    int32_t bg_lift = status == STATUS_RUN ? (primary ? 12 : 7) :
                      (status == STATUS_CHECK ? glow / 8 : (primary ? glow / 3 : glow / 5));
    int32_t shadow = primary ? 4 + glow / 8 : 2 + glow / 16;
    int32_t shadow_opa = primary ? 24 + glow : 12 + glow / 2;
    int32_t border_opa = primary ? 88 + glow : 48 + glow / 2;
    lv_opa_t fill_opa = (lv_opa_t)clamp_i32(base_opa + bg_lift, LV_OPA_60, LV_OPA_COVER);
    lv_obj_set_style_bg_color(capsule, color, 0);
    lv_obj_set_style_bg_opa(capsule, fill_opa, 0);
    lv_obj_set_style_bg_grad_color(capsule, color, 0);
    lv_obj_set_style_bg_grad_dir(capsule, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_main_stop(capsule, 0, 0);
    lv_obj_set_style_bg_grad_stop(capsule, 255, 0);
    lv_obj_set_style_bg_grad_opa(capsule, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(capsule, 1, 0);
    lv_obj_set_style_border_color(capsule, color_hex(status == STATUS_CHECK ? 0xFFFFFF : 0xDDEEFF), 0);
    lv_obj_set_style_border_opa(capsule, (lv_opa_t)clamp_i32(border_opa, LV_OPA_20, LV_OPA_COVER), 0);
    lv_obj_set_style_shadow_color(capsule, color, 0);
    lv_obj_set_style_shadow_width(capsule, shadow, 0);
    lv_obj_set_style_shadow_opa(capsule, (lv_opa_t)clamp_i32(shadow_opa, LV_OPA_TRANSP, LV_OPA_80), 0);

    if(effect != NULL) {
        effect->status = status;
        effect->now_ms = now_ms;
        effect->strength = strength;
        effect->primary = primary;
    }
    lv_obj_invalidate(capsule);
}

static void apply_status_effect(ItemView * view, const TaskModel * task)
{
    if(view == NULL || task == NULL) return;

    int32_t strength = status_effect_strength(task->status);
    apply_capsule_effect(view->status, &view->status_effect, task->status, g_demo.status_effect_now_ms, strength, true);
    apply_capsule_effect(view->age, &view->age_effect, task->status, g_demo.status_effect_now_ms + 180U, strength / 2, false);
}

static void refresh_status_effects(uint32_t now_ms)
{
    g_demo.status_effect_now_ms = now_ms;
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || lv_obj_has_flag(view->card, LV_OBJ_FLAG_HIDDEN)) continue;
        apply_status_effect(view, task_by_id(view->task_id));
    }
}

static void set_hidden(lv_obj_t * obj, bool hidden)
{
    if(hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static const char * decision_state_text(DecisionState state)
{
    switch(state) {
        case DECISION_ACCEPTED: return "Accepted";
        case DECISION_REJECTED: return "Rejected";
        case DECISION_DEFERRED: return "Deferred";
        case DECISION_PENDING:
        default: return "Pending";
    }
}

static const char * binary_action_text(const TaskModel * task, int32_t action)
{
    if(task->decision_kind == DECISION_COMMAND_PERMISSION) {
        return action == 0 ? "Approve" : "Reject";
    }
    if(task->decision_kind == DECISION_ACKNOWLEDGE) {
        return action == 0 ? "Acknowledge" : "Defer";
    }
    return "";
}

static void update_decision_card(ItemView * view, const TaskModel * task)
{
    char text[360];
    int32_t action_count = decision_action_count(task);
    int32_t selected = clamp_i32(task->selected_action, 0, LV_MAX(action_count - 1, 0));

    if(task->decision_kind == DECISION_COMMAND_PERMISSION) {
        snprintf(text, sizeof(text), "Command permission\n%s\n%s%s%s",
                 task->command,
                 selected == 0 ? "[Approve]" : "Approve",
                 "   ",
                 selected == 1 ? "[Reject]" : "Reject");
    }
    else if(task->decision_kind == DECISION_OPTION_SELECT) {
        char option_line[220] = "";
        for(int32_t i = 0; i < action_count; ++i) {
            char chunk[112];
            snprintf(chunk, sizeof(chunk), "%s%s%s%s",
                     i == 0 ? "" : "  /  ",
                     selected == i ? "[" : "",
                     task->options[i],
                     selected == i ? "]" : "");
            strncat(option_line, chunk, sizeof(option_line) - strlen(option_line) - 1);
        }
        snprintf(text, sizeof(text), "Select response option\n%s", option_line);
    }
    else {
        snprintf(text, sizeof(text), "Operator acknowledgement\n%s%s%s",
                 selected == 0 ? "[Acknowledge]" : "Acknowledge",
                 "   ",
                 selected == 1 ? "[Defer]" : "Defer");
    }

    lv_label_set_text(view->decision_action_label, text);

    if(task->decision_state == DECISION_PENDING) {
        lv_label_set_text(view->decision_result_label, "Result: waiting for operator decision");
    }
    else {
        snprintf(text, sizeof(text), "Result: %s - %s", decision_state_text(task->decision_state), task->decision_result);
        lv_label_set_text(view->decision_result_label, text);
    }
}

static void update_detail_content(ItemView * view, const TaskModel * task)
{
    char text[360];
    bool has_decision = task->decision_kind != DECISION_NONE &&
                        (status_requires_response(task->status) || task->decision_state != DECISION_PENDING);

    set_hidden(view->detail_label, has_decision);
    set_hidden(view->detail_title, !has_decision);
    set_hidden(view->evidence_label, !has_decision);
    set_hidden(view->recommendation_label, !has_decision);
    set_hidden(view->risk_label, !has_decision);
    set_hidden(view->decision_card, !has_decision);

    if(!has_decision) {
        lv_label_set_text(view->detail_label, task->detail);
        return;
    }

    lv_label_set_text(view->detail_title, task->decision_title);
    snprintf(text, sizeof(text), "Evidence: %s", task->evidence);
    lv_label_set_text(view->evidence_label, text);
    snprintf(text, sizeof(text), "Recommended: %s", task->recommendation);
    lv_label_set_text(view->recommendation_label, text);
    snprintf(text, sizeof(text), "Impact: %s", task->risk);
    lv_label_set_text(view->risk_label, text);
    update_decision_card(view, task);
}

static void stop_view_animations(ItemView * view)
{
    lv_anim_delete(view, card_visual_exec_cb);
    lv_anim_delete(view, transition_exec_cb);
    lv_anim_delete(view, status_pulse_exec_cb);
    lv_anim_delete(view, detail_exec_cb);
}

static void hide_view(ItemView * view)
{
    view->bound = false;
    view->task_id = 0;
    view->logical_index = -1;
    view->expanded_height = 0;
    view->visual = VISUAL_OFF;
    view->transition = TRANSITION_ON;
    view->pulse = PULSE_OFF;
    stop_view_animations(view);
    lv_obj_set_height(view->detail, 0);
    lv_obj_add_flag(view->detail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(view->card, LV_SIZE_CONTENT);
    lv_obj_set_width(view->card, lv_pct(100));
    lv_obj_add_flag(view->card, LV_OBJ_FLAG_HIDDEN);
}

static void apply_overview_row_metrics(void)
{
    if(g_demo.root == NULL || g_demo.list == NULL) return;

    lv_obj_update_layout(g_demo.root);
    int32_t list_h = lv_obj_get_height(g_demo.list);
    if(list_h <= 0) return;

    int32_t list_gap = lv_obj_get_style_pad_row(g_demo.list, 0);
    int32_t list_pad_top = lv_obj_get_style_pad_top(g_demo.list, 0);
    int32_t list_pad_bottom = lv_obj_get_style_pad_bottom(g_demo.list, 0);

    int32_t visible_gaps = (OVERVIEW_VISIBLE_ROWS - 1) * list_gap;
    int32_t available = list_h - list_pad_top - list_pad_bottom - visible_gaps;
    int32_t row_min_h = available > 0 ? available / OVERVIEW_VISIBLE_ROWS : 0;
    row_min_h = clamp_i32(row_min_h, OVERVIEW_ROW_MIN_HEIGHT, OVERVIEW_ROW_MAX_HEIGHT);

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        lv_obj_set_style_min_height(g_demo.views[i].row, row_min_h, 0);
    }
}

static int32_t visual_center_y(const lv_obj_t * obj)
{
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    return coords.y1 + lv_obj_get_height(obj) / 2;
}

static bool row_content_is_vertically_balanced(const ItemView * view, const char * part, const lv_obj_t * obj)
{
    int32_t row_center = visual_center_y(view->row);
    int32_t content_center = visual_center_y(obj);
    int32_t delta = content_center - row_center;
    if(delta < ROW_CONTENT_CENTER_TARGET_Y - ROW_CONTENT_CENTER_TOLERANCE ||
       delta > ROW_CONTENT_CENTER_TARGET_Y + ROW_CONTENT_CENTER_TOLERANCE) {
        fprintf(stderr, "smoke: row %d %s center is %dpx from row center, expected %dpx\n",
                (int)view->logical_index,
                part,
                (int)delta,
                (int)ROW_CONTENT_CENTER_TARGET_Y);
        return false;
    }
    return true;
}

static bool row_content_stays_inside_card(const ItemView * view)
{
    lv_area_t card_coords;
    lv_area_t logo_coords;
    lv_area_t meta_coords;
    lv_obj_get_coords(view->card, &card_coords);
    lv_obj_get_coords(view->logo, &logo_coords);
    lv_obj_get_coords(view->meta, &meta_coords);

    if(logo_coords.x1 < card_coords.x1 + ROW_CONTENT_INSET_LEFT - 1) {
        fprintf(stderr, "smoke: row %d logo is too close to the left border\n", (int)view->logical_index);
        return false;
    }
    if(meta_coords.x2 > card_coords.x2 - ROW_CONTENT_INSET_RIGHT + 1) {
        fprintf(stderr, "smoke: row %d meta is too close to the right border\n", (int)view->logical_index);
        return false;
    }
    return true;
}

static bool capsule_label_is_vertically_centered(const ItemView * view, const char * part, const lv_obj_t * capsule, const lv_obj_t * label)
{
    int32_t capsule_center = visual_center_y(capsule);
    int32_t label_center = visual_center_y(label);
    int32_t delta = label_center - capsule_center;
    if(delta < -CAPSULE_LABEL_CENTER_TOLERANCE || delta > CAPSULE_LABEL_CENTER_TOLERANCE) {
        fprintf(stderr, "smoke: row %d %s label is %dpx from capsule center\n",
                (int)view->logical_index,
                part,
                (int)delta);
        return false;
    }
    return true;
}

static bool status_effect_is_animating(ItemView * breathing_view, ItemView * run_view)
{
    if(breathing_view == NULL || run_view == NULL) return false;

    motion_demo_tick(120U);
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);
    lv_opa_t first_shadow = lv_obj_get_style_shadow_opa(breathing_view->status, 0);
    uint32_t first_run_now = run_view->status_effect.now_ms;
    int32_t first_run_phase = run_scan_head_scaled(first_run_now, 160, 80, true);

    motion_demo_tick(121U);
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);
    uint32_t subpixel_run_now = run_view->status_effect.now_ms;
    int32_t subpixel_run_phase = run_scan_head_scaled(subpixel_run_now, 160, 80, true);

    motion_demo_tick(720U);
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);
    lv_opa_t second_shadow = lv_obj_get_style_shadow_opa(breathing_view->status, 0);
    uint32_t second_run_now = run_view->status_effect.now_ms;

    if(first_shadow == second_shadow) {
        fprintf(stderr, "smoke: status breathing effect did not animate\n");
        return false;
    }
    if(subpixel_run_now != 121U || subpixel_run_phase == first_run_phase) {
        fprintf(stderr, "smoke: run scan phase is not continuous at millisecond resolution\n");
        return false;
    }
    if(lv_obj_get_style_bg_grad_opa(breathing_view->status, 0) != LV_OPA_TRANSP) {
        fprintf(stderr, "smoke: non-run status should not use sweep gradient\n");
        return false;
    }
    if(run_view->status_effect.status != STATUS_RUN || first_run_now == second_run_now) {
        fprintf(stderr, "smoke: run scan effect did not advance\n");
        return false;
    }
    if(lv_obj_get_style_bg_grad_opa(run_view->status, 0) != LV_OPA_TRANSP) {
        fprintf(stderr, "smoke: run scan should use custom cyclic drawing, not style sweep gradient\n");
        return false;
    }
    return true;
}

static bool rounded_mask_clips_capsule_corners(const ItemView * view)
{
    if(view == NULL || view->status == NULL) return false;

    lv_area_t mask;
    lv_obj_get_coords(view->status, &mask);
    int32_t radius = LV_MIN(lv_obj_get_style_radius(view->status, 0), LV_MIN(lv_area_get_width(&mask), lv_area_get_height(&mask)) / 2);
    if(radius <= 0) return true;

    lv_area_t pixel = {
        .x1 = mask.x1,
        .x2 = mask.x1 + LV_MAX(1, radius / 2),
        .y1 = mask.y1,
        .y2 = mask.y1 + LV_MAX(1, radius / 2),
    };
    if(!fit_area_to_rounded_mask(&pixel, &mask, radius)) return true;

    if(!area_is_inside_rounded_mask(&pixel, &mask, radius)) {
        fprintf(stderr, "smoke: rounded status mask failed to contain fitted pixel\n");
        return false;
    }
    if(pixel.x1 == mask.x1 && pixel.y1 == mask.y1) {
        fprintf(stderr, "smoke: rounded status mask did not clip the corner\n");
        return false;
    }

    int32_t span_x1;
    int32_t span_x2;
    int32_t sample_y1 = mask.y1;
    int32_t sample_y2 = mask.y1 + LV_MAX(1, radius / 2);
    if(!row_span_for_rounded_mask(&mask, radius, sample_y1, sample_y2, &span_x1, &span_x2) ||
       span_x1 <= mask.x1 || span_x2 >= mask.x2) {
        fprintf(stderr, "smoke: rounded row span did not clip the capsule corner\n");
        return false;
    }

    lv_area_t span_pixel = {
        .x1 = span_x1,
        .x2 = span_x1 + LV_MAX(0, radius / 4),
        .y1 = sample_y1,
        .y2 = sample_y2,
    };
    if(!area_is_inside_rounded_mask(&span_pixel, &mask, radius)) {
        fprintf(stderr, "smoke: rounded row span produced an escaping pixel\n");
        return false;
    }
    return true;
}

static bool run_halftone_workload_is_bounded(const ItemView * view)
{
    if(view == NULL || view->status == NULL) return false;

    lv_area_t coords;
    lv_obj_get_coords(view->status, &coords);
    int32_t width = lv_area_get_width(&coords);
    int32_t height = lv_area_get_height(&coords);
    if(width <= 0 || height <= 0) return false;

    int32_t band_width = LV_MAX(height * 2, width / 2);
    int32_t core_width = LV_MAX(1, band_width / 3);
    int32_t head = run_scan_head(view->status_effect.now_ms, width, band_width, true);
    int32_t cell = LV_MAX(3, height / 5);
    int32_t core_cell = LV_MAX(3, (cell * 2) / 3);
    int32_t broad_cells = estimate_run_halftone_cells(&coords, head, band_width, cell);
    int32_t core_cells = estimate_run_halftone_cells(&coords, head, core_width, core_cell);
    int32_t total_cells = broad_cells + core_cells;

    if(total_cells > STATUS_HALFTONE_MAX_CELLS) {
        fprintf(stderr, "smoke: run halftone workload too dense: %d cells\n", (int)total_cells);
        return false;
    }
    return true;
}

static bool run_scan_curve_is_continuous(void)
{
    lv_area_t inner = {
        .x1 = 0,
        .y1 = 0,
        .x2 = 69,
        .y2 = 26,
    };
    int32_t width = lv_area_get_width(&inner);
    int32_t height = lv_area_get_height(&inner);
    int32_t band_width = LV_MAX(height * 2, width / 2);
    int32_t cell = LV_MAX(3, height / 5);
    uint32_t period = status_effect_period_ms(STATUS_RUN);
    int32_t last_head = run_scan_head_scaled(0, width, band_width, true);
    int32_t first_energy = estimate_run_halftone_energy(&inner, last_head, band_width, cell);
    int32_t last_energy = first_energy;
    int32_t max_energy_step = 0;

    for(uint32_t t = 1; t < period; ++t) {
        int32_t head = run_scan_head_scaled(t, width, band_width, true);
        int32_t step = head - last_head;
        if(step <= 0 || step > RUN_SCAN_SUBPIXEL_SCALE) {
            fprintf(stderr, "smoke: run scan head has discontinuous step %d at %u ms\n", (int)step, (unsigned)t);
            return false;
        }

        int32_t energy = estimate_run_halftone_energy(&inner, head, band_width, cell);
        max_energy_step = LV_MAX(max_energy_step, abs_i32(energy - last_energy));
        last_head = head;
        last_energy = energy;
    }

    int32_t loop_head = run_scan_head_scaled(period, width, band_width, true);
    int32_t loop_energy = estimate_run_halftone_energy(&inner, loop_head, band_width, cell);
    if(first_energy != 0 || last_energy != 0 || loop_energy != 0) {
        fprintf(stderr, "smoke: run scan loop reset is visible: first=%d last=%d loop=%d\n",
                (int)first_energy,
                (int)last_energy,
                (int)loop_energy);
        return false;
    }
    if(max_energy_step > 320) {
        fprintf(stderr, "smoke: run scan brightness curve has a large step: %d\n", (int)max_energy_step);
        return false;
    }
    return true;
}

static bool collapsing_detail_height_is_continuous(ItemView * view)
{
    if(view == NULL || !view->bound || view->expanded_height <= 0) return false;

    uint32_t saved_expanded_id = g_demo.expanded_id;
    int32_t samples[] = {
        view->expanded_height,
        view->expanded_height / 2,
        8,
        1,
        0,
    };
    int32_t previous_height = 0;
    bool previous_valid = false;

    for(size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        g_demo.expanded_id = samples[i] > 0 ? view->task_id : 0;
        detail_exec_cb(view, samples[i]);
        lv_obj_update_layout(g_demo.root);

        int32_t card_height = lv_obj_get_height(view->card);
        if(card_height < OVERVIEW_ROW_HEIGHT) {
            fprintf(stderr, "smoke: collapsing card undershot overview height: %d\n", (int)card_height);
            g_demo.expanded_id = saved_expanded_id;
            return false;
        }
        if(previous_valid && card_height > previous_height) {
            fprintf(stderr, "smoke: collapsing card rebounded from %d to %d\n",
                    (int)previous_height,
                    (int)card_height);
            g_demo.expanded_id = saved_expanded_id;
            return false;
        }

        previous_height = card_height;
        previous_valid = true;
    }

    if(lv_obj_get_height(view->card) != OVERVIEW_ROW_HEIGHT) {
        fprintf(stderr, "smoke: collapsed card ended at %dpx instead of %dpx\n",
                (int)lv_obj_get_height(view->card),
                (int)OVERVIEW_ROW_HEIGHT);
        g_demo.expanded_id = saved_expanded_id;
        return false;
    }

    g_demo.expanded_id = saved_expanded_id;
    detail_exec_cb(view, saved_expanded_id == view->task_id ? view->expanded_height : 0);
    lv_obj_update_layout(g_demo.root);
    return true;
}

static bool collapse_scroll_finishes_with_detail(void)
{
    if(g_demo.queue_count <= 0) return false;

    int32_t saved_focus_seq = g_demo.focus_seq;
    uint32_t saved_expanded_id = g_demo.expanded_id;
    int32_t saved_scroll_y = lv_obj_get_scroll_y(g_demo.list);
    bool saved_focus_active = g_demo.focus_active;
    InteractionMode saved_mode = g_demo.mode;

    set_focus_to_logical(2);
    g_demo.focus_active = true;
    g_demo.mode = MODE_NAVIGATING;
    expand_focused_task(false);
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);

    uint32_t expanded_id = g_demo.expanded_id;
    if(expanded_id == 0) {
        fprintf(stderr, "smoke: could not prepare expanded row for collapse timing check\n");
        return false;
    }

    set_expanded_id(0, true);
    refresh_focus_visuals();
    follow_detail_scroll_to_focus();

    int32_t final_scroll_y = lv_obj_get_scroll_y(g_demo.list);
    for(uint32_t elapsed = 0; elapsed < ANIM_DETAIL_MS + 80U; elapsed += 8U) {
        (void)elapsed;
        smoke_advance_frame(8);
        final_scroll_y = lv_obj_get_scroll_y(g_demo.list);
        ItemView * focused_view = &g_demo.views[focused_view_index()];
        if(detail_scroll_follow_is_complete() && lv_anim_get(focused_view, detail_exec_cb) == NULL && lv_obj_get_height(focused_view->detail) <= 0) {
            break;
        }
    }
    lv_timer_handler();
    motion_demo_tick(g_smoke_tick_ms);
    lv_obj_update_layout(g_demo.root);
    final_scroll_y = lv_obj_get_scroll_y(g_demo.list);

    if(g_demo.anchor_scroll_active || g_demo.busy || g_demo.detail_scroll_active) {
        ItemView * focused_view = &g_demo.views[focused_view_index()];
        lv_anim_t * detail_anim = lv_anim_get(focused_view, detail_exec_cb);
        fprintf(stderr, "smoke: collapse left scroll state active at detail end: anchor=%d busy=%d detail=%d offset=%d anim=%d\n",
                (int)g_demo.anchor_scroll_active,
                (int)g_demo.busy,
                (int)g_demo.detail_scroll_active,
                (int)g_demo.anchor_scroll_offset,
                (int)(lv_anim_get(&g_demo, anchor_scroll_exec_cb) != NULL));
        if(detail_anim != NULL) {
            fprintf(stderr, "smoke: detail anim act=%d duration=%d current=%d end=%d height=%d focused=%d\n",
                    (int)detail_anim->act_time,
                    (int)detail_anim->duration,
                    (int)detail_anim->current_value,
                    (int)detail_anim->end_value,
                    (int)lv_obj_get_height(focused_view->detail),
                    (int)focused_view_index());
        }
        return false;
    }

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != expanded_id) continue;
        if(lv_obj_get_height(view->detail) != 0 || lv_obj_get_height(view->card) != OVERVIEW_ROW_HEIGHT) {
            fprintf(stderr, "smoke: collapsed view ended at card=%d detail=%d\n",
                    (int)lv_obj_get_height(view->card),
                    (int)lv_obj_get_height(view->detail));
            return false;
        }
    }

    for(uint32_t elapsed = 0; elapsed < 80U; elapsed += 8U) {
        (void)elapsed;
        smoke_advance_frame(8);
        lv_obj_update_layout(g_demo.root);
        int32_t scroll_y = lv_obj_get_scroll_y(g_demo.list);
        if(abs_i32(scroll_y - final_scroll_y) > 1) {
            fprintf(stderr, "smoke: collapse scroll jumped after detail finish from %d to %d\n",
                    (int)final_scroll_y,
                    (int)scroll_y);
            return false;
        }
    }

    stop_anchor_scroll();
    set_expanded_id(saved_expanded_id, false);
    g_demo.focus_seq = saved_focus_seq;
    g_demo.focus_active = saved_focus_active;
    g_demo.mode = saved_mode;
    set_focus_visuals_immediate();
    lv_obj_scroll_to_y(g_demo.list, saved_scroll_y, LV_ANIM_OFF);
    lv_obj_update_layout(g_demo.root);

    return true;
}

static void measure_and_apply_details(void)
{
    lv_obj_update_layout(g_demo.root);
    int32_t list_h = lv_obj_get_height(g_demo.list);
    int32_t target_card_h = list_h > 0 ? (list_h * 76) / 100 : 0;

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound) continue;

        lv_obj_clear_flag(view->detail, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(view->detail, LV_SIZE_CONTENT);
        lv_obj_set_style_opa(view->detail, LV_OPA_COVER, 0);
        lv_obj_update_layout(g_demo.root);

        int32_t natural_height = lv_obj_get_height(view->detail);
        int32_t label_height = lv_obj_get_height(view->detail_label);
        label_height += lv_obj_get_style_pad_top(view->detail, 0);
        label_height += lv_obj_get_style_pad_bottom(view->detail, 0);
        natural_height = LV_MAX(natural_height, label_height);

        lv_obj_set_height(view->detail, 0);
        lv_obj_update_layout(g_demo.root);
        int32_t collapsed_h = lv_obj_get_height(view->card);
        int32_t page_detail_h = target_card_h > 0 ? LV_MAX(target_card_h - collapsed_h, 0) : 0;
        view->expanded_height = LV_MAX(natural_height, page_detail_h);

        detail_exec_cb(view, view->task_id == g_demo.expanded_id ? view->expanded_height : 0);
        lv_obj_set_width(view->card, lv_pct(100));
        if(view->task_id != g_demo.expanded_id) {
            lv_obj_set_height(view->card, OVERVIEW_ROW_HEIGHT);
        }
    }
}

static void bind_views_from_queue(uint32_t entering_id)
{
    g_demo.binding_views = true;
    apply_overview_row_metrics();

    for(int32_t copy = 0; copy < COPY_COUNT; ++copy) {
        for(int32_t slot = 0; slot < MAX_QUEUE_ITEMS; ++slot) {
            int32_t view_index = view_index_for(copy, slot);
            ItemView * view = &g_demo.views[view_index];

            if((!use_loop_copies() && copy != MIDDLE_COPY) || slot >= g_demo.queue_count) {
                hide_view(view);
                continue;
            }

            TaskModel * task = &g_demo.queue[slot];
            stop_view_animations(view);
            view->bound = true;
            view->task_id = task->id;
            view->logical_index = slot;
            view->visual = target_visual_for_view(view_index);
            view->transition = task->entering && task->id == entering_id ? TRANSITION_OFF : TRANSITION_ON;
            view->pulse = 0;

            lv_obj_clear_flag(view->card, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(view->logo, task->logo);
            lv_label_set_text(view->title, task->title);
            lv_label_set_text(view->summary, task->summary);
            bind_status(view, task);
            update_detail_content(view, task);
            lv_obj_set_width(view->card, lv_pct(100));
            lv_obj_set_height(view->card, task->id == g_demo.expanded_id ? LV_SIZE_CONTENT : OVERVIEW_ROW_HEIGHT);
            set_card_style(view);
        }
    }

    measure_and_apply_details();

    if(entering_id != 0) {
        for(int32_t i = 0; i < VIEW_COUNT; ++i) {
            ItemView * view = &g_demo.views[i];
            if(view->bound && view->task_id == entering_id) {
                transition_exec_cb(view, TRANSITION_OFF);
            }
        }
    }

    g_demo.binding_views = false;
}

static void scroll_completed_cb(lv_anim_t * anim)
{
    (void)anim;
    if(g_demo.recenter_after_scroll && g_demo.queue_count > 0) {
        if(g_demo.focus_seq < g_demo.queue_count) {
            g_demo.focus_seq += g_demo.queue_count;
        }
        else if(g_demo.focus_seq >= g_demo.queue_count * 2) {
            g_demo.focus_seq -= g_demo.queue_count;
        }
        lv_obj_scroll_to_y(g_demo.list, item_scroll_target(focused_view_index()), LV_ANIM_OFF);
        set_focus_visuals_immediate();
    }
    else {
        set_focus_visuals_immediate();
    }
    g_demo.recenter_after_scroll = false;
    g_demo.anchor_scroll_active = false;
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
    g_demo.anchor_scroll_offset = 0;
    g_demo.busy = false;
}

static void stop_anchor_scroll(void)
{
    lv_anim_delete(&g_demo, anchor_scroll_exec_cb);
    g_demo.anchor_scroll_active = false;
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
    g_demo.anchor_scroll_offset = 0;
    g_demo.recenter_after_scroll = false;
    g_demo.busy = false;
}

static void animate_scroll_to_focus(void)
{
    if(g_demo.queue_count <= 0) return;
    lv_anim_delete(&g_demo, anchor_scroll_exec_cb);

    g_demo.busy = true;
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
    g_demo.recenter_after_scroll = g_demo.focus_seq < g_demo.queue_count || g_demo.focus_seq >= (g_demo.queue_count * 2);
    g_demo.anchor_scroll_active = true;
    g_demo.anchor_scroll_offset = lv_obj_get_scroll_y(g_demo.list) - current_item_scroll_target(focused_view_index());

    if(abs_i32(g_demo.anchor_scroll_offset) <= 1) {
        anchor_scroll_exec_cb(&g_demo, 0);
        scroll_completed_cb(NULL);
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, &g_demo);
    lv_anim_set_exec_cb(&anim, anchor_scroll_exec_cb);
    lv_anim_set_values(&anim, g_demo.anchor_scroll_offset, 0);
    set_classic_smooth_motion(&anim, ANIM_SCROLL_MS);
    lv_anim_set_completed_cb(&anim, scroll_completed_cb);
    lv_anim_start(&anim);
}

static void follow_detail_scroll_to_focus(void)
{
    if(g_demo.queue_count <= 0) return;
    lv_anim_delete(&g_demo, anchor_scroll_exec_cb);

    g_demo.anchor_scroll_active = false;
    g_demo.anchor_scroll_offset = 0;
    g_demo.recenter_after_scroll = false;
    g_demo.busy = false;
    g_demo.detail_scroll_active = true;
    g_demo.detail_scroll_task_id = g_demo.queue[logical_focus()].id;
    lv_obj_scroll_to_y(g_demo.list, current_item_scroll_target(focused_view_index()), LV_ANIM_OFF);

    if(detail_scroll_follow_is_complete()) {
        finish_detail_scroll_follow();
    }
}

static void finish_detail_scroll_follow(void)
{
    if(g_demo.detail_scroll_active && g_demo.queue_count > 0) {
        lv_obj_scroll_to_y(g_demo.list, current_item_scroll_target(focused_view_index()), LV_ANIM_OFF);
    }
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
}

static bool detail_scroll_follow_is_complete(void)
{
    if(!g_demo.detail_scroll_active || g_demo.queue_count <= 0) return true;

    int32_t focused = focused_view_index();
    if(focused < 0 || focused >= VIEW_COUNT) return true;

    uint32_t task_id = g_demo.detail_scroll_task_id;
    if(task_id == 0) {
        task_id = g_demo.views[focused].task_id;
    }

    for(int32_t i = 0; i <= focused && i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;
        if(lv_anim_get(view, detail_exec_cb) != NULL || lv_obj_get_height(view->detail) > 0) {
            return false;
        }
    }

    return true;
}

static void anchor_scroll_exec_cb(void * var, int32_t value)
{
    (void)var;
    if(g_demo.queue_count <= 0) return;

    g_demo.anchor_scroll_offset = value;
    lv_obj_scroll_to_y(g_demo.list, current_item_scroll_target(focused_view_index()) + value, LV_ANIM_OFF);
}

static void animate_card_visual(ItemView * view, int32_t target)
{
    if(!view->bound || view->visual == target) return;

    lv_anim_delete(view, card_visual_exec_cb);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, view);
    lv_anim_set_exec_cb(&anim, card_visual_exec_cb);
    lv_anim_set_values(&anim, view->visual, target);
    set_classic_smooth_motion(&anim, ANIM_CARD_MS);
    lv_anim_start(&anim);
}

static void set_focus_visuals_immediate(void)
{
    int32_t focused = focused_view_index();
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        if(!g_demo.views[i].bound) continue;
        lv_anim_delete(&g_demo.views[i], card_visual_exec_cb);
        g_demo.views[i].visual = g_demo.focus_active ? (i == focused ? VISUAL_ON : VISUAL_OFF) : VISUAL_OVERVIEW;
        set_card_style(&g_demo.views[i]);
    }
}

static void refresh_focus_visuals(void)
{
    int32_t focused = focused_view_index();
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        if(!g_demo.views[i].bound) continue;
        animate_card_visual(&g_demo.views[i], g_demo.focus_active ? (i == focused ? VISUAL_ON : VISUAL_OFF) : VISUAL_OVERVIEW);
    }
}

static void animate_detail_for_id(uint32_t task_id, bool opening)
{
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;

        lv_anim_delete(view, detail_exec_cb);
        lv_obj_set_width(view->card, lv_pct(100));
        lv_obj_set_height(view->card, LV_SIZE_CONTENT);

        int32_t start = lv_obj_get_height(view->detail);
        int32_t end = opening ? view->expanded_height : 0;
        if(start == end) {
            detail_exec_cb(view, end);
            continue;
        }

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, view);
        lv_anim_set_exec_cb(&anim, detail_exec_cb);
        lv_anim_set_values(&anim, start, end);
        set_classic_smooth_motion(&anim, ANIM_DETAIL_MS);
        lv_anim_start(&anim);
    }
}

static void set_expanded_id(uint32_t task_id, bool animate)
{
    if(g_demo.expanded_id == task_id) return;

    uint32_t old_id = g_demo.expanded_id;
    g_demo.expanded_id = task_id;

    if(old_id != 0) {
        if(animate) animate_detail_for_id(old_id, false);
        else {
            for(int32_t i = 0; i < VIEW_COUNT; ++i) {
                if(g_demo.views[i].bound && g_demo.views[i].task_id == old_id) {
                    detail_exec_cb(&g_demo.views[i], 0);
                    lv_obj_set_width(g_demo.views[i].card, lv_pct(100));
                }
            }
        }
    }

    if(task_id != 0) {
        if(animate) animate_detail_for_id(task_id, true);
        else {
            for(int32_t i = 0; i < VIEW_COUNT; ++i) {
                if(g_demo.views[i].bound && g_demo.views[i].task_id == task_id) {
                    detail_exec_cb(&g_demo.views[i], g_demo.views[i].expanded_height);
                    lv_obj_set_width(g_demo.views[i].card, lv_pct(100));
                }
            }
        }
    }
}

static void pulse_views_for_id(uint32_t task_id)
{
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;

        lv_anim_delete(view, status_pulse_exec_cb);
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, view);
        lv_anim_set_exec_cb(&anim, status_pulse_exec_cb);
        lv_anim_set_values(&anim, PULSE_ON, PULSE_OFF);
        set_classic_smooth_motion(&anim, ANIM_PULSE_MS);
        lv_anim_start(&anim);
    }
}

static void cancel_active_scroll_for_input(void)
{
    bool was_busy = g_demo.busy;

    lv_anim_delete(&g_demo, anchor_scroll_exec_cb);
    g_demo.anchor_scroll_active = false;
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
    g_demo.anchor_scroll_offset = 0;

    if(!was_busy) return;

    g_demo.busy = false;
    g_demo.recenter_after_scroll = false;

    if(g_demo.queue_count > 0 && (g_demo.focus_seq < g_demo.queue_count || g_demo.focus_seq >= g_demo.queue_count * 2)) {
        set_focus_to_logical(logical_focus());
        lv_obj_scroll_to_y(g_demo.list, item_scroll_target(focused_view_index()), LV_ANIM_OFF);
    }
    set_focus_visuals_immediate();
}

static void activate_focus(void)
{
    if(!g_demo.focus_active) {
        g_demo.focus_seq = logical_focus();
        g_demo.focus_active = true;
        g_demo.mode = MODE_NAVIGATING;
    }
}

static void return_to_overview(void)
{
    g_demo.mode = MODE_OVERVIEW;
    g_demo.focus_active = false;
    set_expanded_id(0, true);
    refresh_focus_visuals();
    lv_obj_scroll_to_y(g_demo.list, 0, LV_ANIM_ON);
}

static void set_mode_for_task(TaskModel * task)
{
    g_demo.mode = decision_is_actionable(task) ? MODE_DECIDING : MODE_NAVIGATING;
}

static void expand_focused_task(bool animate)
{
    if(g_demo.queue_count <= 0) return;
    activate_focus();
    TaskModel * task = &g_demo.queue[logical_focus()];
    mark_current_new_seen();
    set_expanded_id(task->id, animate);
    set_mode_for_task(task);
    refresh_focus_visuals();
    animate_scroll_to_focus();
}

static bool select_next_pending_for_decision(void)
{
    int32_t start = g_demo.focus_active ? logical_focus() : 0;
    int32_t next = find_next_pending_from(start);
    if(next < 0) return false;

    g_demo.focus_active = true;
    set_focus_to_logical(next);
    expand_focused_task(true);
    return true;
}

static TaskModel * expanded_task(void)
{
    if(g_demo.expanded_id == 0) return NULL;
    return task_by_id(g_demo.expanded_id);
}

static void refresh_decision_views_for_task(uint32_t task_id)
{
    TaskModel * task = task_by_id(task_id);
    if(task == NULL) return;

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(view->bound && view->task_id == task_id) {
            update_detail_content(view, task);
            set_card_style(view);
        }
    }
}

static void move_decision_selection(int32_t delta)
{
    TaskModel * task = expanded_task();
    int32_t count = decision_action_count(task);
    if(!decision_is_actionable(task) || count <= 0) return;

    task->selected_action = positive_mod_i32(task->selected_action + delta, count);
    refresh_decision_views_for_task(task->id);
}

static void complete_decision(TaskModel * task, int32_t action)
{
    if(!decision_is_actionable(task)) return;

    int32_t count = decision_action_count(task);
    task->selected_action = clamp_i32(action, 0, LV_MAX(count - 1, 0));

    if(task->decision_kind == DECISION_COMMAND_PERMISSION) {
        if(task->selected_action == 0) {
            task->decision_state = DECISION_ACCEPTED;
            task->status = STATUS_RUN;
            snprintf(task->decision_result, sizeof(task->decision_result), "simulated approve for `%s`", task->command);
        }
        else {
            task->decision_state = DECISION_REJECTED;
            task->status = STATUS_WAIT;
            copy_text(task->decision_result, sizeof(task->decision_result), "operator rejected simulated command");
        }
    }
    else if(task->decision_kind == DECISION_OPTION_SELECT) {
        task->decision_state = DECISION_ACCEPTED;
        task->status = STATUS_RUN;
        snprintf(task->decision_result, sizeof(task->decision_result), "selected `%s`; queue response updated", task->options[task->selected_action]);
    }
    else if(task->decision_kind == DECISION_ACKNOWLEDGE) {
        if(task->selected_action == 0) {
            task->decision_state = DECISION_ACCEPTED;
            task->status = STATUS_RUN;
            copy_text(task->decision_result, sizeof(task->decision_result), "manual recovery acknowledged");
        }
        else {
            task->decision_state = DECISION_DEFERRED;
            task->status = STATUS_WAIT;
            copy_text(task->decision_result, sizeof(task->decision_result), "operator deferred recovery review");
        }
    }

    task->notify_state = NOTIFY_NONE;
    g_demo.mode = MODE_NAVIGATING;
    update_status_views_for_id(task->id);
    refresh_decision_views_for_task(task->id);
    pulse_views_for_id(task->id);
    refresh_focus_visuals();
}

static void submit_current_decision(void)
{
    TaskModel * task = expanded_task();
    if(decision_is_actionable(task)) {
        complete_decision(task, task->selected_action);
    }
}

static void approve_current_command(void)
{
    TaskModel * task = expanded_task();
    if(decision_is_actionable(task) && task->decision_kind == DECISION_COMMAND_PERMISSION) {
        complete_decision(task, 0);
    }
}

static void reject_current_command(void)
{
    TaskModel * task = expanded_task();
    if(decision_is_actionable(task) && task->decision_kind == DECISION_COMMAND_PERMISSION) {
        complete_decision(task, 1);
    }
}

static void move_focus(int32_t delta)
{
    if(g_demo.queue_count <= 0 || g_demo.deleting_id != 0) return;

    activate_focus();
    g_demo.focus_seq += delta;
    if(!use_loop_copies() || g_demo.focus_seq < 0 || g_demo.focus_seq >= g_demo.queue_count * COPY_COUNT) {
        set_focus_to_logical(logical_focus());
    }

    mark_current_new_seen();
    if(g_demo.expanded_id != 0) {
        set_expanded_id(0, true);
    }
    g_demo.mode = MODE_NAVIGATING;
    refresh_focus_visuals();
    animate_scroll_to_focus();
}

static void toggle_detail(void)
{
    if(g_demo.queue_count <= 0 || g_demo.deleting_id != 0) return;

    if(!g_demo.focus_active) {
        if(select_next_pending_for_decision()) return;
        activate_focus();
    }

    uint32_t id = g_demo.queue[logical_focus()].id;
    TaskModel * task = &g_demo.queue[logical_focus()];
    if(g_demo.expanded_id == id) {
        if(g_demo.mode == MODE_DECIDING && decision_is_actionable(task)) {
            submit_current_decision();
            return;
        }
        else {
            g_demo.mode = MODE_NAVIGATING;
            set_expanded_id(0, true);
        }
    }
    else {
        expand_focused_task(true);
        return;
    }

    refresh_focus_visuals();
    follow_detail_scroll_to_focus();
}

static void enter_completed_cb(lv_anim_t * anim)
{
    (void)anim;
    if(g_demo.entering_id != 0) {
        g_demo.entering_id = 0;
    }
    g_demo.queue_mutation_busy = false;
}

static bool animate_entering_task(uint32_t task_id)
{
    bool completion_attached = false;

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;

        lv_anim_delete(view, transition_exec_cb);
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, view);
        lv_anim_set_exec_cb(&anim, transition_exec_cb);
        lv_anim_set_values(&anim, TRANSITION_OFF, TRANSITION_ON);
        set_classic_smooth_motion(&anim, ANIM_QUEUE_MS);
        if(!completion_attached && i == view_index_for(MIDDLE_COPY, view->logical_index)) {
            lv_anim_set_completed_cb(&anim, enter_completed_cb);
            completion_attached = true;
        }
        lv_anim_start(&anim);
    }

    return completion_attached;
}

static void add_demo_task(void)
{
    if(g_demo.queue_count >= MAX_QUEUE_ITEMS || g_demo.deleting_id != 0 || g_demo.queue_mutation_busy) return;

    uint32_t focus_id = g_demo.queue_count > 0 ? g_demo.queue[logical_focus()].id : 0;
    int32_t insert_at = g_demo.queue_count > 0 ? logical_focus() + 1 : 0;
    const TaskTemplate * tmpl = &k_templates[g_demo.add_serial % TEMPLATE_COUNT];

    for(int32_t i = g_demo.queue_count; i > insert_at; --i) {
        g_demo.queue[i] = g_demo.queue[i - 1];
    }

    TaskModel * task = &g_demo.queue[insert_at];
    assign_template(task, tmpl, g_demo.next_id++);
    snprintf(task->title, sizeof(task->title), "New response queued from %s #%lu", tmpl->agent, (unsigned long)(g_demo.add_serial + 1));
    snprintf(task->detail, sizeof(task->detail),
             "%s created a new queue response. It is inserted after the current decision, highlighted as a new item, and kept collapsed so the operator can finish the active task before switching.",
             tmpl->agent);
    task->notify_state = status_requires_response(task->status) ? NOTIFY_PENDING : NOTIFY_NEW;
    task->entering = true;
    ++g_demo.queue_count;
    ++g_demo.add_serial;
    g_demo.queue_mutation_busy = true;
    g_demo.entering_id = task->id;

    sync_focus_to_current_id(focus_id);
    bind_views_from_queue(task->id);
    bool completion_attached = animate_entering_task(task->id);
    pulse_views_for_id(task->id);
    if(g_demo.focus_active || g_demo.expanded_id != 0) {
        animate_scroll_to_focus();
    }
    task->entering = false;

    if(!completion_attached) {
        enter_completed_cb(NULL);
    }
}

static void update_status_views_for_id(uint32_t task_id)
{
    TaskModel * task = task_by_id(task_id);
    if(task == NULL) return;

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;
        bind_status(view, task);
        update_detail_content(view, task);
        set_card_style(view);
    }
}

static void cycle_current_status(void)
{
    if(g_demo.queue_count <= 0 || g_demo.deleting_id != 0) return;

    TaskModel * task = &g_demo.queue[logical_focus()];
    task->status = next_status(task->status);
    if(status_requires_response(task->status) && task->decision_kind != DECISION_NONE) {
        task->decision_state = DECISION_PENDING;
        task->selected_action = 0;
        copy_text(task->decision_result, sizeof(task->decision_result), "");
    }
    update_task_notify(task);
    update_status_views_for_id(task->id);
    pulse_views_for_id(task->id);
}

static void remove_deleting_task(void)
{
    int32_t delete_index = find_task_index_by_id(g_demo.deleting_id);
    int32_t anchor_screen_y = 0;
    bool has_anchor_screen_y = false;

    if(g_demo.delete_focus_id != 0 && g_demo.queue_count > 0) {
        int32_t anchor_view = focused_view_index();
        if(anchor_view >= 0 && anchor_view < VIEW_COUNT && g_demo.views[anchor_view].task_id == g_demo.delete_focus_id) {
            anchor_screen_y = current_item_content_y(anchor_view) - lv_obj_get_scroll_y(g_demo.list);
            has_anchor_screen_y = true;
        }
    }

    stop_anchor_scroll();

    if(delete_index < 0) {
        g_demo.deleting_id = 0;
        g_demo.delete_focus_id = 0;
        g_demo.deleting_index = 0;
        g_demo.deleting_was_expanded = false;
        g_demo.queue_mutation_busy = false;
        return;
    }

    for(int32_t i = delete_index; i < g_demo.queue_count - 1; ++i) {
        g_demo.queue[i] = g_demo.queue[i + 1];
    }
    --g_demo.queue_count;

    if(g_demo.queue_count <= 0) {
        g_demo.deleting_id = 0;
        g_demo.delete_focus_id = 0;
        g_demo.deleting_index = 0;
        g_demo.deleting_was_expanded = false;
        g_demo.queue_mutation_busy = false;
        bind_views_from_queue(0);
        return;
    }

    int32_t next_focus = find_task_index_by_id(g_demo.delete_focus_id);
    if(next_focus < 0) {
        int32_t next_pending = find_next_pending_from(g_demo.deleting_index);
        next_focus = next_pending >= 0 ? next_pending : clamp_i32(g_demo.deleting_index, 0, g_demo.queue_count - 1);
    }

    uint32_t expanded_after_delete = g_demo.expanded_id == g_demo.delete_focus_id ? g_demo.delete_focus_id : 0;
    set_focus_to_logical(next_focus);
    g_demo.expanded_id = expanded_after_delete;
    g_demo.deleting_id = 0;
    g_demo.delete_focus_id = 0;
    g_demo.deleting_index = 0;
    g_demo.deleting_was_expanded = false;
    g_demo.detail_scroll_active = false;
    g_demo.detail_scroll_task_id = 0;
    g_demo.queue_mutation_busy = false;

    bind_views_from_queue(0);
    set_focus_visuals_immediate();

    if(has_anchor_screen_y) {
        lv_obj_scroll_to_y(g_demo.list, current_item_content_y(focused_view_index()) - anchor_screen_y, LV_ANIM_OFF);
    }
    else {
        lv_obj_scroll_to_y(g_demo.list, item_scroll_target(focused_view_index()), LV_ANIM_OFF);
    }

    animate_scroll_to_focus();
}

static void delete_completed_cb(lv_anim_t * anim)
{
    (void)anim;
    if(g_demo.deleting_id != 0) {
        remove_deleting_task();
    }
}

static void delete_current_task(void)
{
    if(g_demo.queue_count <= 1 || g_demo.deleting_id != 0 || g_demo.queue_mutation_busy) return;

    int32_t focus = logical_focus();
    uint32_t task_id = g_demo.queue[focus].id;
    int32_t next_focus_old = find_post_delete_focus_old_index(focus);
    uint32_t next_focus_id = next_focus_old >= 0 ? g_demo.queue[next_focus_old].id : 0;

    g_demo.queue[focus].exiting = true;
    g_demo.deleting_id = task_id;
    g_demo.delete_focus_id = next_focus_id;
    g_demo.deleting_index = focus;
    g_demo.deleting_was_expanded = g_demo.expanded_id == task_id;
    g_demo.queue_mutation_busy = true;

    if(next_focus_old >= 0) {
        set_focus_to_logical(next_focus_old);
    }

    if(g_demo.deleting_was_expanded) {
        set_expanded_id(next_focus_id, true);
    }
    refresh_focus_visuals();
    animate_scroll_to_focus();

    bool completion_attached = false;
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || view->task_id != task_id) continue;

        lv_anim_delete(view, transition_exec_cb);
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, view);
        lv_anim_set_exec_cb(&anim, transition_exec_cb);
        lv_anim_set_values(&anim, view->transition, TRANSITION_OFF);
        set_classic_smooth_motion(&anim, ANIM_QUEUE_MS);
        if(i == view_index_for(MIDDLE_COPY, focus)) {
            lv_anim_set_completed_cb(&anim, delete_completed_cb);
            completion_attached = true;
        }
        lv_anim_start(&anim);
    }

    if(!completion_attached) {
        remove_deleting_task();
    }
}

static void jump_to_next_pending(void)
{
    if(g_demo.queue_count <= 0 || g_demo.deleting_id != 0) return;

    int32_t next = find_next_pending_from((g_demo.focus_active ? logical_focus() : -1) + 1);
    if(next < 0) return;

    g_demo.focus_active = true;
    set_focus_to_logical(next);
    expand_focused_task(true);
}

static void disable_auto_loop(void)
{
    g_demo.auto_enabled = false;
}

void motion_demo_handle_key(uint32_t key)
{
    disable_auto_loop();

    if(g_demo.deleting_id != 0) return;
    if(g_demo.queue_mutation_busy && (key == MOTION_DEMO_KEY_ADD || key == MOTION_DEMO_KEY_DELETE)) return;

    cancel_active_scroll_for_input();

    if(key == LV_KEY_UP) {
        move_focus(-1);
    }
    else if(key == LV_KEY_DOWN) {
        move_focus(1);
    }
    else if(key == LV_KEY_LEFT) {
        move_decision_selection(-1);
    }
    else if(key == LV_KEY_RIGHT) {
        move_decision_selection(1);
    }
    else if(key == LV_KEY_ENTER) {
        toggle_detail();
    }
    else if(key == MOTION_DEMO_KEY_ADD) {
        add_demo_task();
    }
    else if(key == MOTION_DEMO_KEY_DELETE) {
        delete_current_task();
    }
    else if(key == MOTION_DEMO_KEY_STATUS) {
        cycle_current_status();
    }
    else if(key == MOTION_DEMO_KEY_NEXT_PENDING) {
        jump_to_next_pending();
    }
    else if(key == MOTION_DEMO_KEY_APPROVE) {
        approve_current_command();
    }
    else if(key == MOTION_DEMO_KEY_REJECT) {
        reject_current_command();
    }
    else if(key == MOTION_DEMO_KEY_ESCAPE) {
        return_to_overview();
    }
}

static bool time_reached(uint32_t now_ms, uint32_t target_ms)
{
    return (int32_t)(now_ms - target_ms) >= 0;
}

void motion_demo_tick(uint32_t now_ms)
{
    refresh_status_effects(now_ms);
    if(g_demo.detail_scroll_active && detail_scroll_follow_is_complete()) {
        finish_detail_scroll_follow();
    }

    if(!g_demo.auto_enabled || g_demo.busy || g_demo.deleting_id != 0 || g_demo.queue_mutation_busy || !time_reached(now_ms, g_demo.auto_next_ms)) {
        return;
    }

    move_focus(1);
    g_demo.auto_next_ms = now_ms + 1650;
}

bool motion_demo_smoke_check(void)
{
    smoke_use_controlled_tick();

    if(g_demo.root == NULL || g_demo.list == NULL || g_demo.queue_count != TEMPLATE_COUNT) {
        fprintf(stderr, "smoke: missing root/list or unexpected queue count\n");
        return false;
    }

    uint32_t focused_before = g_demo.queue[logical_focus()].id;
    int32_t count_before = g_demo.queue_count;
    lv_obj_update_layout(g_demo.root);

    for(int32_t i = 0; i < OVERVIEW_VISIBLE_ROWS; ++i) {
        ItemView * view = &g_demo.views[view_index_for(0, i)];
        if(!view->bound || lv_obj_has_flag(view->card, LV_OBJ_FLAG_HIDDEN)) {
            fprintf(stderr, "smoke: initial row %d hidden\n", (int)i);
            return false;
        }
        if(lv_obj_get_y(view->card) < 0) {
            fprintf(stderr, "smoke: initial row %d y below 0\n", (int)i);
            return false;
        }
        if(lv_obj_get_y(view->card) + lv_obj_get_height(view->card) > 480) {
            fprintf(stderr, "smoke: initial row %d extends past viewport\n", (int)i);
            return false;
        }
        if(lv_obj_get_style_border_width(view->card, 0) != CARD_BORDER_WIDTH ||
           lv_obj_get_style_border_opa(view->card, 0) < CARD_BORDER_OPA_OVERVIEW) {
            fprintf(stderr, "smoke: initial row %d border is too faint\n", (int)i);
            return false;
        }
        if(lv_obj_get_x(view->meta) <= lv_obj_get_x(view->text_column)) {
            fprintf(stderr, "smoke: initial row %d meta overlaps text column\n", (int)i);
            return false;
        }
        if(!row_content_is_vertically_balanced(view, "logo", view->logo) ||
           !row_content_is_vertically_balanced(view, "text", view->text_column) ||
           !row_content_is_vertically_balanced(view, "meta", view->meta)) {
            return false;
        }
        if(!row_content_stays_inside_card(view) ||
           !capsule_label_is_vertically_centered(view, "status", view->status, view->status_label) ||
           !capsule_label_is_vertically_centered(view, "age", view->age, view->age_label)) {
            return false;
        }
    }

    if(!status_effect_is_animating(&g_demo.views[view_index_for(0, 1)], &g_demo.views[view_index_for(0, 3)]) ||
       !rounded_mask_clips_capsule_corners(&g_demo.views[view_index_for(0, 3)]) ||
       !run_halftone_workload_is_bounded(&g_demo.views[view_index_for(0, 3)]) ||
       !run_scan_curve_is_continuous() ||
       !collapse_scroll_finishes_with_detail()) {
        return false;
    }

    motion_demo_handle_key(LV_KEY_DOWN);
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);

    if(g_demo.queue[logical_focus()].id == focused_before) {
        fprintf(stderr, "smoke: down did not move focus\n");
        return false;
    }
    if(copy_for_focus() != 0) {
        fprintf(stderr, "smoke: first focused row jumped to copy %d\n", (int)copy_for_focus());
        return false;
    }

    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || lv_obj_has_flag(view->card, LV_OBJ_FLAG_HIDDEN) || view->task_id == g_demo.expanded_id) {
            continue;
        }
        if(lv_obj_get_style_transform_scale_x(view->card, 0) != 256 || lv_obj_get_style_transform_scale_y(view->card, 0) != 256) {
            fprintf(stderr, "smoke: focused state left collapsed row %d scaled\n", (int)i);
            return false;
        }
    }

    motion_demo_handle_key(LV_KEY_ENTER);
    motion_demo_handle_key(MOTION_DEMO_KEY_STATUS);
    motion_demo_handle_key(MOTION_DEMO_KEY_ADD);

    for(int32_t tick = 0; tick < 80 && g_demo.queue_mutation_busy; ++tick) {
        smoke_advance_frame(8);
    }
    lv_timer_handler();
    lv_obj_update_layout(g_demo.root);

    if(g_demo.queue_count != count_before + 1) {
        fprintf(stderr, "smoke: add did not increase queue, count=%d busy=%d\n", (int)g_demo.queue_count, (int)g_demo.queue_mutation_busy);
        return false;
    }
    if(g_demo.expanded_id == 0) {
        fprintf(stderr, "smoke: enter did not expand focus\n");
        return false;
    }
    ItemView * expanded_view = &g_demo.views[focused_view_index()];
    if(!expanded_view->bound || expanded_view->task_id != g_demo.expanded_id) {
        fprintf(stderr, "smoke: focused view is not the expanded task\n");
        return false;
    }
    int32_t list_content_w = lv_obj_get_width(g_demo.list) -
                             lv_obj_get_style_pad_left(g_demo.list, 0) -
                             lv_obj_get_style_pad_right(g_demo.list, 0);
    if(lv_obj_get_width(expanded_view->card) != list_content_w) {
        fprintf(stderr, "smoke: expanded card width %d does not match list content width %d\n",
                (int)lv_obj_get_width(expanded_view->card),
                (int)list_content_w);
        return false;
    }
    if(lv_obj_get_x(expanded_view->detail) < 0 ||
       lv_obj_get_x(expanded_view->detail) + lv_obj_get_width(expanded_view->detail) > lv_obj_get_width(expanded_view->card)) {
        fprintf(stderr, "smoke: expanded detail escapes card width\n");
        return false;
    }
    if(lv_obj_get_x(expanded_view->meta) + lv_obj_get_width(expanded_view->meta) > lv_obj_get_width(expanded_view->row)) {
        fprintf(stderr, "smoke: expanded meta escapes row width\n");
        return false;
    }
    if(!row_content_is_vertically_balanced(expanded_view, "logo", expanded_view->logo) ||
       !row_content_is_vertically_balanced(expanded_view, "text", expanded_view->text_column) ||
       !row_content_is_vertically_balanced(expanded_view, "meta", expanded_view->meta)) {
        return false;
    }
    if(!row_content_stays_inside_card(expanded_view) ||
       !capsule_label_is_vertically_centered(expanded_view, "status", expanded_view->status, expanded_view->status_label) ||
       !capsule_label_is_vertically_centered(expanded_view, "age", expanded_view->age, expanded_view->age_label)) {
        return false;
    }
    if(!collapsing_detail_height_is_continuous(expanded_view)) {
        return false;
    }

    int32_t visible_rows = 0;
    for(int32_t i = 0; i < VIEW_COUNT; ++i) {
        ItemView * view = &g_demo.views[i];
        if(!view->bound || lv_obj_has_flag(view->card, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        int32_t y = lv_obj_get_y(view->card) - lv_obj_get_scroll_y(g_demo.list);
        if(y >= 0 && y < 480) {
            ++visible_rows;
            if(lv_obj_get_x(view->meta) + lv_obj_get_width(view->meta) > 800) {
                fprintf(stderr, "smoke: final visible row %d meta past viewport\n", (int)i);
                return false;
            }
        }
    }

    if(visible_rows <= 0) {
        fprintf(stderr, "smoke: no visible rows after interactions\n");
        return false;
    }

    return true;
}

static void style_plain_container(lv_obj_t * obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static lv_obj_t * make_label(lv_obj_t * parent, const char * text, const lv_font_t * font, lv_color_t color)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    return label;
}

static void init_reference_fonts(void)
{
#if LV_USE_TINY_TTF
    static bool initialized;
    static lv_font_t * title;
    static lv_font_t * summary;
    static lv_font_t * capsule;
    static lv_font_t * slash;
    static lv_font_t * detail;
    static lv_font_t * detail_title;
    static lv_font_t * decision_result;

    if(initialized) return;
    initialized = true;

    const char * sf_path = "A:/System/Library/Fonts/SFNS.ttf";
    title = lv_tiny_ttf_create_file_ex(sf_path, 24, 0, 256);
    summary = lv_tiny_ttf_create_file_ex(sf_path, 17, 0, 256);
    capsule = lv_tiny_ttf_create_file_ex(sf_path, 15, 0, 192);
    slash = lv_tiny_ttf_create_file_ex(sf_path, 24, 0, 128);
    detail = lv_tiny_ttf_create_file_ex(sf_path, 18, 0, 256);
    detail_title = lv_tiny_ttf_create_file_ex(sf_path, 22, 0, 256);
    decision_result = lv_tiny_ttf_create_file_ex(sf_path, 14, 0, 128);

    if(title != NULL) g_font_title = title;
    if(summary != NULL) g_font_summary = summary;
    if(capsule != NULL) g_font_capsule = capsule;
    if(slash != NULL) g_font_slash = slash;
    if(detail != NULL) g_font_detail = detail;
    if(detail_title != NULL) g_font_detail_title = detail_title;
    if(decision_result != NULL) g_font_decision_result = decision_result;
#endif
}

static lv_obj_t * create_status_capsule(lv_obj_t * parent)
{
    lv_obj_t * capsule = lv_obj_create(parent);
    style_plain_container(capsule);
    lv_obj_set_height(capsule, LV_SIZE_CONTENT);
    lv_obj_set_width(capsule, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(capsule, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(capsule, 8, 0);
    lv_obj_set_style_pad_hor(capsule, 11, 0);
    lv_obj_set_style_pad_ver(capsule, 4, 0);
    lv_obj_set_flex_flow(capsule, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(capsule, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_clip_corner(capsule, true, 0);

    make_label(capsule, "RUN", g_font_capsule, color_hex(0xF1F2F2));
    return capsule;
}

static void create_view(size_t physical_index)
{
    ItemView * view = &g_demo.views[physical_index];

    lv_obj_t * card = lv_obj_create(g_demo.list);
    view->card = card;
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(card, color_hex(0x1A1D1C), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, CARD_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(card, color_hex(0x56606A), 0);
    lv_obj_set_style_border_opa(card, CARD_BORDER_OPA_BASE, 0);
    lv_obj_set_style_shadow_color(card, color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_shadow_spread(card, 0, 0);
    lv_obj_set_style_pad_left(card, 0, 0);
    lv_obj_set_style_pad_right(card, 0, 0);
    lv_obj_set_style_pad_top(card, 0, 0);
    lv_obj_set_style_pad_bottom(card, 0, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * row = lv_obj_create(card);
    view->row = row;
    style_plain_container(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row, 52, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(row, ROW_CONTENT_INSET_LEFT, 0);
    lv_obj_set_style_pad_right(row, ROW_CONTENT_INSET_RIGHT, 0);
    lv_obj_set_style_pad_column(row, 16, 0);

    view->logo = lv_image_create(row);
    lv_obj_set_style_image_opa(view->logo, LV_OPA_COVER, 0);
    lv_image_set_scale(view->logo, 244);
    lv_obj_set_style_pad_left(view->logo, 0, 0);
    lv_obj_set_style_translate_y(view->logo, ROW_CONTENT_TRANSLATE_Y, 0);

    view->text_column = lv_obj_create(row);
    style_plain_container(view->text_column);
    lv_obj_set_width(view->text_column, 0);
    lv_obj_set_flex_grow(view->text_column, 1);
    lv_obj_set_flex_flow(view->text_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view->text_column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(view->text_column, 3, 0);
    lv_obj_set_style_translate_y(view->text_column, ROW_CONTENT_TRANSLATE_Y, 0);

    view->title = make_label(view->text_column, "", g_font_title, color_hex(0xF0F1EF));
    lv_label_set_long_mode(view->title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(view->title, lv_pct(100));
    lv_obj_set_style_text_line_space(view->title, 0, 0);
    lv_obj_set_style_translate_y(view->title, ROW_TEXT_GLYPH_TRANSLATE_Y, 0);

    view->summary = make_label(view->text_column, "", g_font_summary, color_hex(0x969696));
    lv_label_set_long_mode(view->summary, LV_LABEL_LONG_DOT);
    lv_obj_set_width(view->summary, lv_pct(100));
    lv_obj_set_style_text_line_space(view->summary, 0, 0);
    lv_obj_set_style_translate_y(view->summary, ROW_TEXT_GLYPH_TRANSLATE_Y, 0);

    view->meta = lv_obj_create(row);
    style_plain_container(view->meta);
    lv_obj_set_width(view->meta, LV_SIZE_CONTENT);
    lv_obj_set_height(view->meta, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(view->meta, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(view->meta, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(view->meta, 6, 0);
    lv_obj_set_style_translate_y(view->meta, ROW_CONTENT_TRANSLATE_Y, 0);

    view->status = create_status_capsule(view->meta);
    view->status_label = lv_obj_get_child(view->status, 0);
    lv_obj_add_event_cb(view->status, status_capsule_draw_event_cb, LV_EVENT_DRAW_MAIN_END, &view->status_effect);
    view->slash_label = make_label(view->meta, "/", g_font_slash, color_hex(0xF1F2F2));
    lv_obj_set_style_translate_y(view->slash_label, ROW_TEXT_GLYPH_TRANSLATE_Y, 0);
    view->age = create_status_capsule(view->meta);
    view->age_label = lv_obj_get_child(view->age, 0);
    lv_obj_add_event_cb(view->age, status_capsule_draw_event_cb, LV_EVENT_DRAW_MAIN_END, &view->age_effect);

    lv_obj_t * detail = lv_obj_create(card);
    view->detail = detail;
    style_plain_container(detail);
    lv_obj_set_width(detail, lv_pct(100));
    lv_obj_set_height(detail, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(detail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(detail, color_hex(0x202A37), 0);
    lv_obj_set_style_bg_opa(detail, LV_OPA_80, 0);
    lv_obj_set_style_radius(detail, 12, 0);
    lv_obj_set_style_pad_left(detail, 52, 0);
    lv_obj_set_style_pad_right(detail, 40, 0);
    lv_obj_set_style_pad_top(detail, 8, 0);
    lv_obj_set_style_pad_bottom(detail, 14, 0);
    lv_obj_set_style_pad_row(detail, 7, 0);

    view->detail_label = make_label(detail, "", g_font_detail, color_hex(0xB8C1CF));
    lv_label_set_long_mode(view->detail_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->detail_label, lv_pct(100));
    lv_obj_set_style_text_line_space(view->detail_label, 4, 0);

    view->detail_title = make_label(detail, "", g_font_detail_title, color_hex(0xF4F7FA));
    lv_label_set_long_mode(view->detail_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(view->detail_title, lv_pct(100));

    view->evidence_label = make_label(detail, "", g_font_summary, color_hex(0xB8C1CF));
    lv_label_set_long_mode(view->evidence_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->evidence_label, lv_pct(100));
    lv_obj_set_style_text_line_space(view->evidence_label, 3, 0);

    view->recommendation_label = make_label(detail, "", g_font_summary, color_hex(0xA7F3D0));
    lv_label_set_long_mode(view->recommendation_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->recommendation_label, lv_pct(100));
    lv_obj_set_style_text_line_space(view->recommendation_label, 3, 0);

    view->risk_label = make_label(detail, "", g_font_summary, color_hex(0xF8C77E));
    lv_label_set_long_mode(view->risk_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->risk_label, lv_pct(100));
    lv_obj_set_style_text_line_space(view->risk_label, 3, 0);

    view->decision_card = lv_obj_create(detail);
    style_plain_container(view->decision_card);
    lv_obj_set_width(view->decision_card, lv_pct(100));
    lv_obj_set_height(view->decision_card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(view->decision_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(view->decision_card, color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(view->decision_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view->decision_card, 10, 0);
    lv_obj_set_style_border_width(view->decision_card, 1, 0);
    lv_obj_set_style_border_color(view->decision_card, color_hex(0x315163), 0);
    lv_obj_set_style_border_opa(view->decision_card, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(view->decision_card, 10, 0);
    lv_obj_set_style_pad_row(view->decision_card, 6, 0);

    view->decision_action_label = make_label(view->decision_card, "", g_font_summary, color_hex(0xF4F7FA));
    lv_label_set_long_mode(view->decision_action_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->decision_action_label, lv_pct(100));
    lv_obj_set_style_text_line_space(view->decision_action_label, 3, 0);

    view->decision_result_label = make_label(view->decision_card, "", g_font_decision_result, color_hex(0x8B96A8));
    lv_label_set_long_mode(view->decision_result_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->decision_result_label, lv_pct(100));

    view->logical_index = -1;
    view->transition = TRANSITION_ON;
    hide_view(view);
}

static void init_queue(void)
{
    g_demo.queue_count = TEMPLATE_COUNT;
    g_demo.next_id = 1;
    g_demo.add_serial = 0;
    for(int32_t i = 0; i < g_demo.queue_count; ++i) {
        assign_template(&g_demo.queue[i], &k_templates[i], g_demo.next_id++);
    }

    set_focus_to_logical(0);
    g_demo.expanded_id = 0;
}

void motion_demo_create(lv_obj_t * parent)
{
    init_reference_fonts();

    lv_obj_remove_style_all(parent);
    lv_obj_set_style_bg_color(parent, color_hex(0x0B0F14), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_size(parent, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 0, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);

    g_demo.root = parent;
    g_demo.busy = false;
    g_demo.recenter_after_scroll = false;
    g_demo.queue_mutation_busy = false;
    g_demo.focus_active = false;
    g_demo.mode = MODE_OVERVIEW;
    g_demo.auto_enabled = false;
    g_demo.auto_next_ms = 1200;
    g_demo.entering_id = 0;
    g_demo.deleting_id = 0;
    g_demo.delete_focus_id = 0;
    g_demo.deleting_index = 0;
    g_demo.deleting_was_expanded = false;

    lv_obj_t * list = lv_obj_create(parent);
    g_demo.list = list;
    style_plain_container(list);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_style_pad_ver(list, 0, 0);
    lv_obj_set_style_pad_left(list, 8, 0);
    lv_obj_set_style_pad_right(list, 4, 0);

    for(size_t i = 0; i < VIEW_COUNT; ++i) {
        create_view(i);
    }

    init_queue();
    bind_views_from_queue(0);
    set_focus_visuals_immediate();
    lv_obj_scroll_to_y(g_demo.list, 0, LV_ANIM_OFF);
}
