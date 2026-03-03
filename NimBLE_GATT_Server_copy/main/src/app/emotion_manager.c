/*
 * Emotion Manager Implementation - 情绪管理器实现
 */

#include "app/emotion_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "EmotionMgr";

/* NVS存储键名 */
#define NVS_NAMESPACE "emotion"
#define NVS_KEY_ZONE "zone"
#define NVS_KEY_INTERACTIONS "interactions"
#define NVS_KEY_STATISTICS "statistics"

/* 全局情绪状态 */
static emotion_state_t g_emotion_state = {
    .current_zone = EMOTION_ZONE_C,  // 默认日常区间
    .zone_enter_time = 0,
    .total_interactions = 0,
    .zone_interaction_count = 0,
    .is_initialized = false
};

/* 全局统计信息 */
static emotion_statistics_t g_statistics = {0};

/* 情绪区间名称表 */
static const char *zone_names[EMOTION_ZONE_MAX] = {
    "S-亢奋",
    "A-兴奋",
    "B-积极",
    "C-日常",
    "D-消极"
};

/* 情绪区间描述表 */
static const char *zone_descriptions[EMOTION_ZONE_MAX] = {
    "S区间：极度兴奋，活跃度最高",
    "A区间：兴奋状态，互动积极",
    "B区间：积极正面，情绪良好",
    "C区间：日常平稳，正常状态",
    "D区间：消极低落，需要关注"
};

/* ==================== 私有函数 ==================== */

/**
 * @brief 更新统计信息
 */
static void update_statistics(emotion_zone_t old_zone, emotion_zone_t new_zone) {
    uint32_t current_time = esp_timer_get_time() / 1000000;  // 转换为秒
    
    if (old_zone < EMOTION_ZONE_MAX) {
        // 计算在旧区间的停留时长
        uint32_t duration = current_time - (g_emotion_state.zone_enter_time / 1000);
        g_statistics.zone_duration[old_zone] += duration;
    }
    
    if (new_zone < EMOTION_ZONE_MAX) {
        // 增加新区间的进入次数
        g_statistics.zone_count[new_zone]++;
    }
    
    g_statistics.last_change_time = current_time;
}

/* ==================== 公共函数实现 ==================== */

esp_err_t emotion_manager_init(void) {
    if (g_emotion_state.is_initialized) {
        ESP_LOGW(TAG, "Emotion manager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing emotion manager...");
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    // 设置初始时间戳
    g_emotion_state.zone_enter_time = esp_timer_get_time() / 1000;  // 毫秒
    g_emotion_state.is_initialized = true;
    
    // 尝试从NVS加载
    emotion_manager_load_from_nvs();
    
    ESP_LOGI(TAG, "Emotion manager initialized, current zone: %s", 
             emotion_manager_get_zone_name(g_emotion_state.current_zone));
    
    return ESP_OK;
}

esp_err_t emotion_manager_set_zone(emotion_zone_t zone) {
    if (zone >= EMOTION_ZONE_MAX) {
        ESP_LOGE(TAG, "Invalid zone: %d", zone);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_emotion_state.is_initialized) {
        ESP_LOGE(TAG, "Emotion manager not initialized");
        return ESP_FAIL;
    }
    
    emotion_zone_t old_zone = g_emotion_state.current_zone;
    
    if (old_zone != zone) {
        // 更新统计信息
        update_statistics(old_zone, zone);
        
        // 更新状态
        g_emotion_state.current_zone = zone;
        g_emotion_state.zone_enter_time = esp_timer_get_time() / 1000;  // 毫秒
        g_emotion_state.zone_interaction_count = 0;  // 重置区间互动计数
        
        ESP_LOGI(TAG, "Emotion zone changed: %s -> %s", 
                 emotion_manager_get_zone_name(old_zone),
                 emotion_manager_get_zone_name(zone));
    } else {
        ESP_LOGD(TAG, "Zone unchanged: %s", emotion_manager_get_zone_name(zone));
    }
    
    return ESP_OK;
}

emotion_zone_t emotion_manager_get_zone(void) {
    return g_emotion_state.current_zone;
}

esp_err_t emotion_manager_get_state(emotion_state_t *state) {
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(state, &g_emotion_state, sizeof(emotion_state_t));
    return ESP_OK;
}

esp_err_t emotion_manager_increment_interaction(void) {
    if (!g_emotion_state.is_initialized) {
        ESP_LOGE(TAG, "Emotion manager not initialized");
        return ESP_FAIL;
    }
    
    g_emotion_state.total_interactions++;
    g_emotion_state.zone_interaction_count++;
    
    ESP_LOGD(TAG, "Interaction count: total=%lu, zone=%d", 
             g_emotion_state.total_interactions,
             g_emotion_state.zone_interaction_count);
    
    return ESP_OK;
}

esp_err_t emotion_manager_get_statistics(emotion_statistics_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 更新当前区间的停留时长
    uint32_t current_time = esp_timer_get_time() / 1000000;  // 秒
    uint32_t current_duration = current_time - (g_emotion_state.zone_enter_time / 1000);
    
    memcpy(stats, &g_statistics, sizeof(emotion_statistics_t));
    stats->zone_duration[g_emotion_state.current_zone] += current_duration;
    
    return ESP_OK;
}

esp_err_t emotion_manager_reset_statistics(void) {
    memset(&g_statistics, 0, sizeof(emotion_statistics_t));
    ESP_LOGI(TAG, "Statistics reset");
    return ESP_OK;
}

esp_err_t emotion_manager_save_to_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // 打开NVS命名空间
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 保存情绪区间
    ret = nvs_set_u8(nvs_handle, NVS_KEY_ZONE, (uint8_t)g_emotion_state.current_zone);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save zone: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 保存互动次数
    ret = nvs_set_u32(nvs_handle, NVS_KEY_INTERACTIONS, g_emotion_state.total_interactions);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save interactions: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 保存统计信息
    ret = nvs_set_blob(nvs_handle, NVS_KEY_STATISTICS, &g_statistics, sizeof(emotion_statistics_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save statistics: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // 提交更改
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Emotion state saved to NVS");
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t emotion_manager_load_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // 打开NVS命名空间
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved state in NVS, using defaults");
        return ESP_OK;  // 不是错误，只是没有保存的数据
    }
    
    // 加载情绪区间
    uint8_t zone;
    ret = nvs_get_u8(nvs_handle, NVS_KEY_ZONE, &zone);
    if (ret == ESP_OK && zone < EMOTION_ZONE_MAX) {
        g_emotion_state.current_zone = (emotion_zone_t)zone;
        ESP_LOGI(TAG, "Loaded zone from NVS: %s", emotion_manager_get_zone_name(zone));
    }
    
    // 加载互动次数
    ret = nvs_get_u32(nvs_handle, NVS_KEY_INTERACTIONS, &g_emotion_state.total_interactions);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded interactions from NVS: %lu", g_emotion_state.total_interactions);
    }
    
    // 加载统计信息
    size_t required_size = sizeof(emotion_statistics_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_STATISTICS, &g_statistics, &required_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded statistics from NVS");
    }
    
    nvs_close(nvs_handle);
    return ESP_OK;
}

const char* emotion_manager_get_zone_name(emotion_zone_t zone) {
    if (zone >= EMOTION_ZONE_MAX) {
        return "Unknown";
    }
    return zone_names[zone];
}

const char* emotion_manager_get_zone_description(emotion_zone_t zone) {
    if (zone >= EMOTION_ZONE_MAX) {
        return "Unknown zone";
    }
    return zone_descriptions[zone];
}
