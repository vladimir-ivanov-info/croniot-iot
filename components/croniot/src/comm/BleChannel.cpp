#include "BleChannel.h"

#include <cstring>

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "CJsonPtr.h"
#include "Sensors/SensorsController.h"
#include "Sensors/SensorType.h"
#include "Tasks/TaskController.h"
#include "Tasks/TaskType.h"

namespace croniot {

static const char* TAG = "BleChannel";

BleChannel* BleChannel::s_instance_ = nullptr;

namespace {

// Croniot custom 128-bit UUIDs (base 5e6f00xx-7c7b-4f6e-9e8a-2a3a1b2c3d4e)
#define CRONIOT_UUID128(byte0, byte1)                                          \
    BLE_UUID128_INIT(0x4e, 0x3d, 0x2c, 0x1b, 0x3a, 0x2a,                       \
                     0x8a, 0x9e, 0x6e, 0x4f, 0x7b, 0x7c,                       \
                     byte0, byte1, 0x6f, 0x5e)

static const ble_uuid128_t SERVICE_UUID         = CRONIOT_UUID128(0x01, 0x00);
static const ble_uuid128_t CHAR_DEVICE_INFO     = CRONIOT_UUID128(0x02, 0x00);
static const ble_uuid128_t CHAR_AUTH            = CRONIOT_UUID128(0x03, 0x00);
static const ble_uuid128_t CHAR_SENSOR_DATA     = CRONIOT_UUID128(0x04, 0x00);
static const ble_uuid128_t CHAR_TASK_PROGRESS   = CRONIOT_UUID128(0x05, 0x00);
static const ble_uuid128_t CHAR_TASK_COMMAND    = CRONIOT_UUID128(0x06, 0x00);
static const ble_uuid128_t CHAR_TASK_STATE_SYNC = CRONIOT_UUID128(0x07, 0x00);

uint16_t handleSensorData = 0;
uint16_t handleTaskProgress = 0;

int parseTaskTypeUid(const std::string& json) {
    CJsonPtr root(cJSON_Parse(json.c_str()));
    if (!root) return -1;
    cJSON* uid = cJSON_GetObjectItem(root.get(), "taskTypeUid");
    if (!cJSON_IsNumber(uid)) return -1;
    return uid->valueint;
}

int charAccessCb(uint16_t conn_handle,
                 uint16_t attr_handle,
                 struct ble_gatt_access_ctxt* ctxt,
                 void* arg) {
    auto* self = BleChannel::instance();
    if (!self) return BLE_ATT_ERR_UNLIKELY;

    const ble_uuid_t* uuid = ctxt->chr ? ctxt->chr->uuid : ctxt->dsc->uuid;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (ble_uuid_cmp(uuid, &CHAR_DEVICE_INFO.u) == 0) {
            const std::string& info = self->deviceInfoJson();
            int rc = os_mbuf_append(ctxt->om, info.data(), info.size());
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        std::string buf;
        buf.resize(len);
        uint16_t out_len = 0;
        if (ble_hs_mbuf_to_flat(ctxt->om, buf.data(), len, &out_len) != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf.resize(out_len);

        if (ble_uuid_cmp(uuid, &CHAR_TASK_COMMAND.u) == 0) {
            self->onTaskCommandWrite(buf);
            return 0;
        }
        if (ble_uuid_cmp(uuid, &CHAR_TASK_STATE_SYNC.u) == 0) {
            self->onTaskStateSyncWrite(buf);
            return 0;
        }
        if (ble_uuid_cmp(uuid, &CHAR_AUTH.u) == 0) {
            ESP_LOGI(TAG, "Auth write (stub): %.*s", (int)buf.size(), buf.data());
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

const struct ble_gatt_chr_def CHARS[] = {
    {
        .uuid       = &CHAR_DEVICE_INFO.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_READ,
        .val_handle = nullptr,
    },
    {
        .uuid       = &CHAR_AUTH.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = nullptr,
    },
    {
        .uuid       = &CHAR_SENSOR_DATA.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &handleSensorData,
    },
    {
        .uuid       = &CHAR_TASK_PROGRESS.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &handleTaskProgress,
    },
    {
        .uuid       = &CHAR_TASK_COMMAND.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_WRITE,
        .val_handle = nullptr,
    },
    {
        .uuid       = &CHAR_TASK_STATE_SYNC.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_WRITE,
        .val_handle = nullptr,
    },
    { 0 },
};

const struct ble_gatt_svc_def SERVICES[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &SERVICE_UUID.u,
        .includes        = nullptr,
        .characteristics = CHARS,
    },
    { 0 },
};

int gapEvent(struct ble_gap_event* event, void* arg) {
    auto* self = BleChannel::instance();
    if (!self) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connect status=%d", event->connect.status);
            if (event->connect.status == 0) {
                self->setConnHandle(event->connect.conn_handle);
            } else {
                self->setConnHandle(0xFFFF);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnect reason=%d", event->disconnect.reason);
            self->setConnHandle(0xFFFF);
            self->startAdvertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Subscribe attr_handle=%d cur_notify=%d",
                     event->subscribe.attr_handle, event->subscribe.cur_notify);
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

}

BleChannel::BleChannel(const std::string& deviceUuid,
                       const CroniotConfig::BleCfg& cfg)
    : deviceUuid_(deviceUuid),
      deviceName_(cfg.localName.empty() ? "croniot" : cfg.localName) {
    s_instance_ = this;
    rebuildDeviceInfoJson();
}

void BleChannel::rebuildDeviceInfoJson() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceUuid", deviceUuid_.c_str());
    cJSON_AddStringToObject(root, "name", deviceName_.c_str());

    cJSON* sensorTypesJson = cJSON_AddArrayToObject(root, "sensorTypes");
    for (auto* st : SensorsController::instance().getAllSensorTypes()) {
        if (!st) continue;
        cJSON* item = cJSON_CreateObject();
        st->toJson(item);
        cJSON_AddItemToArray(sensorTypesJson, item);
    }

    cJSON* taskTypesJson = cJSON_AddArrayToObject(root, "taskTypes");
    for (const auto& tt : TaskController::instance().getAllTaskTypes()) {
        cJSON* item = cJSON_CreateObject();
        tt.toJson(item);
        cJSON_AddItemToArray(taskTypesJson, item);
    }

    char* serialized = cJSON_PrintUnformatted(root);
    deviceInfoJson_ = serialized ? serialized : "{}";
    if (serialized) cJSON_free(serialized);
    cJSON_Delete(root);
}

void BleChannel::hostTaskTrampoline(void* arg) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BleChannel::onHostReset(int reason) {
    ESP_LOGW(TAG, "Host reset; reason=%d", reason);
}

void BleChannel::onHostSync() {
    auto* self = BleChannel::instance();
    if (!self) return;
    self->hostSynced_.store(true);

    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr rc=%d", rc);
        return;
    }
    self->startAdvertising();

    if (self->onReady_ && !self->readyFired_.exchange(true)) {
        self->onReady_();
    }
}

void BleChannel::startAdvertising() {
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t*)deviceName_.c_str();
    fields.name_len = deviceName_.size();
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER,
                           &adv_params, gapEvent, nullptr);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising as '%s'", deviceName_.c_str());
}

bool BleChannel::startConnection(ConnectionReadyCallback onReady) {
    onReady_ = std::move(onReady);

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
        return false;
    }

    ble_hs_cfg.sync_cb  = &BleChannel::onHostSync;
    ble_hs_cfg.reset_cb = &BleChannel::onHostReset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(SERVICES);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_count_cfg rc=%d", rc);
        return false;
    }
    rc = ble_gatts_add_svcs(SERVICES);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_add_svcs rc=%d", rc);
        return false;
    }

    rc = ble_svc_gap_device_name_set(deviceName_.c_str());
    if (rc != 0) {
        ESP_LOGW(TAG, "device_name_set rc=%d", rc);
    }

    setSensorDataAttrHandle(handleSensorData);
    setTaskProgressAttrHandle(handleTaskProgress);

    nimble_port_freertos_init(&BleChannel::hostTaskTrampoline);
    return true;
}

bool BleChannel::startMessaging() {
    return hostSynced_.load();
}

bool BleChannel::isConnected() const {
    return connHandle_.load() != 0xFFFF;
}

Result BleChannel::registerDevice(const std::string&)     { return Result(false, "BLE: not server-auth channel"); }
Result BleChannel::login(const std::string&)              { return Result(false, "BLE: not server-auth channel"); }
Result BleChannel::registerSensorType(const std::string&) { return Result(false, "BLE: not server-auth channel"); }
Result BleChannel::registerTaskType(const std::string&)   { return Result(false, "BLE: not server-auth channel"); }

Result BleChannel::publishSensorData(const std::string& /*deviceUuid*/,
                                     int /*sensorUid*/,
                                     const std::string& jsonValue) {
    uint16_t conn = connHandle_.load();
    if (conn == 0xFFFF || hSensorData_ == 0) {
        return Result(false, "BLE: no subscriber");
    }
    struct os_mbuf* om = ble_hs_mbuf_from_flat(jsonValue.data(), jsonValue.size());
    if (!om) return Result(false, "BLE: mbuf alloc failed");
    int rc = ble_gatts_notify_custom(conn, hSensorData_, om);
    return rc == 0 ? Result(true, "BLE notify ok")
                   : Result(false, "BLE notify failed");
}

Result BleChannel::publishTaskProgressUpdate(const std::string& /*deviceUuid*/,
                                             const std::string& jsonPayload) {
    uint16_t conn = connHandle_.load();
    if (conn == 0xFFFF || hTaskProgress_ == 0) {
        return Result(false, "BLE: no subscriber");
    }
    struct os_mbuf* om = ble_hs_mbuf_from_flat(jsonPayload.data(), jsonPayload.size());
    if (!om) return Result(false, "BLE: mbuf alloc failed");
    int rc = ble_gatts_notify_custom(conn, hTaskProgress_, om);
    return rc == 0 ? Result(true, "BLE notify ok")
                   : Result(false, "BLE notify failed");
}

void BleChannel::subscribeTaskCommand(const std::string&,
                                      int taskTypeUid,
                                      TaskBase* taskInstance) {
    taskCommandHandlers_[taskTypeUid] = taskInstance;
}

void BleChannel::subscribeTaskStateInfoSync(const std::string&,
                                            int taskTypeUid,
                                            TaskBase* taskInstance) {
    taskStateSyncHandlers_[taskTypeUid] = taskInstance;
}

void BleChannel::onTaskCommandWrite(const std::string& payload) {
    int taskTypeUid = parseTaskTypeUid(payload);
    if (taskTypeUid < 0) {
        ESP_LOGW(TAG, "TaskCommand write missing taskTypeUid: %s", payload.c_str());
        return;
    }
    TaskController::instance().processMessageTaskData(taskTypeUid, payload, payload.length());
}

void BleChannel::onTaskStateSyncWrite(const std::string& payload) {
    int taskTypeUid = parseTaskTypeUid(payload);
    if (taskTypeUid < 0) {
        ESP_LOGW(TAG, "TaskStateSync write missing taskTypeUid: %s", payload.c_str());
        return;
    }
    TaskController::instance().processMessageTaskStateInfoSync(taskTypeUid, payload, payload.length());
}

}
