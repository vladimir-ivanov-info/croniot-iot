#include "BleChannel.h"

#include <cctype>
#include <cstring>
#include <memory>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "CJsonPtr.h"
#include "Sensors/SensorsController.h"
#include "Sensors/SensorType.h"
#include "Tasks/TaskController.h"
#include "Tasks/TaskProgressUpdate.h"
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
static const ble_uuid128_t CHAR_SYNC_COMMAND    = CRONIOT_UUID128(0x08, 0x00);
static const ble_uuid128_t CHAR_SYNC_DATA       = CRONIOT_UUID128(0x09, 0x00);

uint16_t handleSensorData   = 0;
uint16_t handleTaskProgress = 0;
uint16_t handleSyncData     = 0;

int parseTaskTypeUid(const std::string& json) {
    CJsonPtr root(cJSON_Parse(json.c_str()));
    if (!root) return -1;
    cJSON* uid = cJSON_GetObjectItem(root.get(), "taskTypeUid");
    if (!cJSON_IsNumber(uid)) return -1;
    return uid->valueint;
}

// Parses {"type":"requestSync","data":{"taskTypeUid":N}} or legacy {"taskTypeUid":N}.
int parseRequestSyncTaskTypeUid(const std::string& json) {
    CJsonPtr root(cJSON_Parse(json.c_str()));
    if (!root) return -1;
    cJSON* type = cJSON_GetObjectItem(root.get(), "type");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "requestSync") == 0) {
        cJSON* data = cJSON_GetObjectItem(root.get(), "data");
        if (cJSON_IsObject(data)) {
            cJSON* uid = cJSON_GetObjectItem(data, "taskTypeUid");
            if (cJSON_IsNumber(uid)) return uid->valueint;
        }
    }
    // Fallback: legacy flat format
    cJSON* uid = cJSON_GetObjectItem(root.get(), "taskTypeUid");
    if (cJSON_IsNumber(uid)) return uid->valueint;
    return -1;
}

int charAccessCb(uint16_t conn_handle,
                 uint16_t attr_handle,
                 struct ble_gatt_access_ctxt* ctxt,
                 void* arg) {
    auto* self = BleChannel::instance();
    if (!self) return BLE_ATT_ERR_UNLIKELY;
    (void)attr_handle;

    const ble_uuid_t* uuid = ctxt->chr ? ctxt->chr->uuid : ctxt->dsc->uuid;

    if (self->isSecurityEnabled() &&
        conn_handle != BLE_HS_CONN_HANDLE_NONE &&
        !self->isConnectionSecure(conn_handle)) {
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }

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
            const char* resp = R"({"success":true})";
            struct os_mbuf* om = ble_hs_mbuf_from_flat(resp, strlen(resp));
            if (om) ble_gatts_notify_custom(conn_handle, attr_handle, om);
            return 0;
        }
        if (ble_uuid_cmp(uuid, &CHAR_SYNC_COMMAND.u) == 0) {
            self->onSyncCommandWrite(buf);
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
    {
        .uuid       = &CHAR_SYNC_COMMAND.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_WRITE,
        .val_handle = nullptr,
    },
    {
        .uuid       = &CHAR_SYNC_DATA.u,
        .access_cb  = charAccessCb,
        .flags      = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &handleSyncData,
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
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connect status=%d", event->connect.status);
            if (event->connect.status == 0) {
                self->setConnHandle(event->connect.conn_handle);

                if (self->isSecurityEnabled()) {
                    int rc = ble_gap_security_initiate(event->connect.conn_handle);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "ble_gap_security_initiate rc=%d", rc);
                        ble_gap_terminate(event->connect.conn_handle, 0x13);
                    } else {
                        ESP_LOGI(TAG, "Security procedure initiated");
                    }
                }
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

        case BLE_GAP_EVENT_ENC_CHANGE: {
            struct ble_gap_conn_desc desc = {};
            int rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            if (rc == 0) {
                ESP_LOGI(TAG,
                         "ENC_CHANGE status=%d encrypted=%d authenticated=%d bonded=%d",
                         event->enc_change.status,
                         desc.sec_state.encrypted,
                         desc.sec_state.authenticated,
                         desc.sec_state.bonded);
            } else {
                ESP_LOGW(TAG, "ENC_CHANGE status=%d, conn_find rc=%d",
                         event->enc_change.status, rc);
            }

            if (self->isSecurityEnabled() &&
                (event->enc_change.status != 0 ||
                 !self->isConnectionSecure(event->enc_change.conn_handle))) {
                ESP_LOGE(TAG, "Link is not secure after pairing, disconnecting");
                ble_gap_terminate(event->enc_change.conn_handle, 0x13);
            }
            break;
        }

        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            struct ble_sm_io pkey = {};
            pkey.action = event->passkey.params.action;
            int rc = 0;

            switch (event->passkey.params.action) {
                case BLE_SM_IOACT_DISP:
                case BLE_SM_IOACT_INPUT:
                case BLE_SM_IOACT_STATIC:
                    pkey.passkey = self->staticPasskey();
                    ESP_LOGI(TAG, "Using BLE passkey %06lu",
                             static_cast<unsigned long>(pkey.passkey));
                    rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                    break;

                case BLE_SM_IOACT_NUMCMP:
                    pkey.numcmp_accept = 1;
                    rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
                    break;

                default:
                    ESP_LOGW(TAG, "Unsupported passkey action=%d",
                             event->passkey.params.action);
                    ble_gap_terminate(event->passkey.conn_handle, 0x13);
                    return 0;
            }

            if (rc != 0) {
                ESP_LOGE(TAG, "ble_sm_inject_io rc=%d", rc);
                return rc;
            }
            break;
        }

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            ESP_LOGI(TAG, "Repeat pairing requested; retrying");
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU update: %d", event->mtu.value);
            self->setNegotiatedMtu(event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

} // namespace

BleChannel::BleChannel(const std::string& deviceUuid,
                       const CroniotConfig::BleCfg& cfg)
    : deviceUuid_(deviceUuid),
      deviceName_(cfg.localName.empty() ? "croniot" : cfg.localName),
      blePassword_(cfg.password) {
    securityEnabled_ = cfg.securityEnabled;
    passkeyEnabled_  = parseStaticPasskey(blePassword_, staticPasskey_);
    if (securityEnabled_ && !passkeyEnabled_) {
        ESP_LOGE(TAG,
                 "Invalid BLE password. Expected exactly 6 digits in config.ble.password");
    }
    s_instance_ = this;
    rebuildSchemaJson();
    schemaVersion_ = djb2Hash(schemaJson_);
    rebuildDeviceInfoJson();
}

bool BleChannel::parseStaticPasskey(const std::string& password,
                                    uint32_t& outPasskey) const {
    if (password.size() != 6) return false;

    uint32_t passkey = 0;
    for (char c : password) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        passkey = (passkey * 10) + static_cast<uint32_t>(c - '0');
    }

    if (passkey > 999999) return false;
    outPasskey = passkey;
    return true;
}

bool BleChannel::isConnectionSecure(uint16_t connHandle) const {
    struct ble_gap_conn_desc desc = {};
    int rc = ble_gap_conn_find(connHandle, &desc);
    if (rc != 0) return false;
    return desc.sec_state.encrypted && desc.sec_state.authenticated;
}

// Small metadata read by the app on connect to check schemaVersion cache.
void BleChannel::rebuildDeviceInfoJson() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "deviceId", deviceUuid_.c_str());
    cJSON_AddNumberToObject(root, "protocolVersion", 1);
    cJSON_AddNumberToObject(root, "schemaVersion",
                            static_cast<double>(schemaVersion_));
    char* serialized = cJSON_PrintUnformatted(root);
    deviceInfoJson_ = serialized ? serialized : "{}";
    if (serialized) cJSON_free(serialized);
    cJSON_Delete(root);
}

// Full sensor+task schema sent via chunked NOTIFY on demand.
void BleChannel::rebuildSchemaJson() {
    cJSON* root = cJSON_CreateObject();

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
    schemaJson_ = serialized ? serialized : "{}";
    if (serialized) cJSON_free(serialized);
    cJSON_Delete(root);
}

uint32_t BleChannel::djb2Hash(const std::string& s) {
    uint32_t hash = 5381;
    for (unsigned char c : s) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void BleChannel::onSyncCommandWrite(const std::string& payload) {
    // Strip trailing null bytes (Android may include null terminator)
    std::string command = payload;
    while (!command.empty() && command.back() == '\0') command.pop_back();

    ESP_LOGI(TAG, "SyncCommand write: '%.*s' (%zu bytes)",
             (int)command.size(), command.data(), command.size());

    if (command != "START_SCHEMA_SYNC") {
        ESP_LOGW(TAG, "Unknown sync command, ignoring");
        return;
    }
    if (syncInProgress_.exchange(true)) {
        ESP_LOGW(TAG, "Schema sync already in progress");
        return;
    }

    // Rebuild fresh — sensor/task types are registered at boot before BleChannel is created.
    rebuildSchemaJson();
    schemaVersion_ = djb2Hash(schemaJson_);
    rebuildDeviceInfoJson();
    ESP_LOGI(TAG, "Schema: %zu bytes, version=%lu",
             schemaJson_.size(), static_cast<unsigned long>(schemaVersion_));

    xTaskCreate(
        [](void* arg) {
            static_cast<BleChannel*>(arg)->runSchemaSync();
            vTaskDelete(nullptr);
        },
        "ble_schema_sync",
        4096,
        this,
        1,
        nullptr
    );
}

void BleChannel::runSchemaSync() {
    uint16_t conn = connHandle_.load();
    ESP_LOGI(TAG, "runSchemaSync: conn=0x%04x hSyncData=%d", conn, hSyncData_);
    if (conn == 0xFFFF) {
        ESP_LOGE(TAG, "runSchemaSync: no active connection");
        syncInProgress_.store(false);
        return;
    }
    if (hSyncData_ == 0) {
        ESP_LOGE(TAG, "runSchemaSync: hSyncData_ not set (GATT handle missing)");
        syncInProgress_.store(false);
        return;
    }

    // Each NOTIFY payload = negotiatedMtu - 3 (ATT header); 2 bytes used for [seq][total].
    const uint16_t mtu          = negotiatedMtu_;
    const uint16_t dataPerChunk = static_cast<uint16_t>(mtu > 5 ? mtu - 5 : 18);
    const size_t   schemaSize   = schemaJson_.size();
    const size_t   numChunks    = (schemaSize + dataPerChunk - 1) / dataPerChunk;

    if (numChunks > 255) {
        ESP_LOGE(TAG, "Schema too large: %zu chunks (max 255)", numChunks);
        syncInProgress_.store(false);
        return;
    }

    ESP_LOGI(TAG, "Schema sync: %zu bytes → %zu chunks (%u bytes/chunk)",
             schemaSize, numChunks, dataPerChunk);

    const uint8_t total = static_cast<uint8_t>(numChunks);
    for (uint8_t i = 0; i < total; ++i) {
        const size_t offset   = static_cast<size_t>(i) * dataPerChunk;
        const size_t chunkLen = std::min(static_cast<size_t>(dataPerChunk),
                                         schemaSize - offset);
        const size_t bufSize  = 2 + chunkLen;

        std::unique_ptr<uint8_t[]> buf(new uint8_t[bufSize]);
        buf[0] = i;
        buf[1] = total;
        std::memcpy(buf.get() + 2, schemaJson_.data() + offset, chunkLen);

        struct os_mbuf* om = ble_hs_mbuf_from_flat(buf.get(), bufSize);
        if (!om) {
            ESP_LOGE(TAG, "mbuf alloc failed at chunk %d", i);
            break;
        }

        int rc = ble_gatts_notify_custom(conn, hSyncData_, om);
        if (rc != 0) {
            ESP_LOGE(TAG, "Notify chunk %d failed rc=%d", i, rc);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    syncInProgress_.store(false);
    ESP_LOGI(TAG, "Schema sync complete");
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

    // ble_gatts_start() has run by now — GATT handles are valid.
    self->setSensorDataAttrHandle(handleSensorData);
    self->setTaskProgressAttrHandle(handleTaskProgress);
    self->setSyncDataAttrHandle(handleSyncData);
    ESP_LOGI(TAG, "GATT handles: sensorData=%d taskProgress=%d syncData=%d",
             handleSensorData, handleTaskProgress, handleSyncData);

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
    // Primary advertisement: flags + device name
    struct ble_hs_adv_fields fields = {};
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name                  = (uint8_t*)deviceName_.c_str();
    fields.name_len              = deviceName_.size();
    fields.name_is_complete      = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    // Scan response: Croniot service UUID (17 bytes, fits in 31-byte response).
    // Android filters with ScanFilter.setServiceUuid() to show only Croniot devices.
    struct ble_hs_adv_fields rsp = {};
    rsp.uuids128             = &SERVICE_UUID;
    rsp.num_uuids128         = 1;
    rsp.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields rc=%d", rc);
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

    if (securityEnabled_ && !passkeyEnabled_) {
        ESP_LOGE(TAG, "Cannot start BLE with security enabled but no valid 6-digit passkey");
        return false;
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
        return false;
    }

    ble_hs_cfg.sync_cb  = &BleChannel::onHostSync;
    ble_hs_cfg.reset_cb = &BleChannel::onHostReset;
    if (securityEnabled_) {
        ble_hs_cfg.sm_io_cap         = BLE_HS_IO_DISPLAY_ONLY;
        ble_hs_cfg.sm_bonding        = 0;
        ble_hs_cfg.sm_mitm           = 1;
        ble_hs_cfg.sm_sc             = 1;
        ble_hs_cfg.sm_our_key_dist   = 0;
        ble_hs_cfg.sm_their_key_dist = 0;
    }

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
    if (conn == 0xFFFF || hSensorData_ == 0 || (securityEnabled_ && !isConnectionSecure(conn))) {
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
    if (conn == 0xFFFF || hTaskProgress_ == 0 || (securityEnabled_ && !isConnectionSecure(conn))) {
        return Result(false, "BLE: no subscriber");
    }

    // Android expects {"type":"stateInfoEvent","data":{...}}
    cJSON* inner = cJSON_Parse(jsonPayload.c_str());
    cJSON* envelope = cJSON_CreateObject();
    if (!envelope) {
        if (inner) cJSON_Delete(inner);
        return Result(false, "BLE: cJSON alloc failed");
    }
    cJSON_AddStringToObject(envelope, "type", "stateInfoEvent");
    cJSON_AddItemToObject(envelope, "data", inner ? inner : cJSON_CreateObject());

    char* serialized = cJSON_PrintUnformatted(envelope);
    cJSON_Delete(envelope);
    if (!serialized) return Result(false, "BLE: serialize failed");

    std::string wrapped(serialized);
    cJSON_free(serialized);

    struct os_mbuf* om = ble_hs_mbuf_from_flat(wrapped.data(), wrapped.size());
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
    ESP_LOGI(TAG, "TaskCommand write: %.*s", (int)payload.size(), payload.data());

    CJsonPtr root(cJSON_Parse(payload.c_str()));
    if (!root) {
        ESP_LOGW(TAG, "TaskCommand: JSON parse failed");
        return;
    }

    // Support {"type":"addTask","data":{...}} and legacy flat format
    cJSON* data = cJSON_GetObjectItem(root.get(), "data");
    if (!data) data = root.get();

    // taskTypeUid may arrive as string "3" or number 3
    int taskTypeUid = -1;
    cJSON* uidNode = cJSON_GetObjectItem(data, "taskTypeUid");
    if (cJSON_IsNumber(uidNode)) {
        taskTypeUid = uidNode->valueint;
    } else if (cJSON_IsString(uidNode)) {
        taskTypeUid = atoi(uidNode->valuestring);
    }

    if (taskTypeUid < 0) {
        ESP_LOGW(TAG, "TaskCommand: missing or invalid taskTypeUid");
        return;
    }

    // Build normalized JSON that processMessageTaskData expects (flat, numeric uid)
    cJSON* normalized = cJSON_CreateObject();
    cJSON_AddNumberToObject(normalized, "taskTypeUid", taskTypeUid);
    cJSON_AddNumberToObject(normalized, "taskUid", 0);
    cJSON* paramsNode = cJSON_GetObjectItem(data, "parametersValues");
    if (paramsNode) {
        cJSON_AddItemToObject(normalized, "parametersValues",
                              cJSON_Duplicate(paramsNode, /*recurse=*/1));
    }
    char* normStr = cJSON_PrintUnformatted(normalized);
    cJSON_Delete(normalized);
    if (!normStr) return;
    std::string normJson(normStr);
    cJSON_free(normStr);

    TaskController::instance().processMessageTaskData(taskTypeUid, normJson,
                                                      normJson.length());

    // Echo the desired state back immediately via TASK_PROGRESS NOTIFY.
    // State = first parametersValues value (e.g. "on" / "off").
    if (paramsNode && paramsNode->child && cJSON_IsString(paramsNode->child)) {
        std::string state = paramsNode->child->valuestring;
        TaskProgressUpdate update(taskTypeUid, 0, state, 0.0f, "");
        TaskController::instance().enqueueTaskProgressUpdate(update);
        ESP_LOGI(TAG, "TaskCommand: uid=%d state=%s", taskTypeUid, state.c_str());
    }
}

void BleChannel::onTaskStateSyncWrite(const std::string& payload) {
    int taskTypeUid = parseRequestSyncTaskTypeUid(payload);
    if (taskTypeUid < 0) {
        ESP_LOGW(TAG, "TaskStateSync write missing taskTypeUid: %s", payload.c_str());
        return;
    }
    TaskController::instance().processMessageTaskStateInfoSync(taskTypeUid, payload,
                                                               payload.length());
}

} // namespace croniot
