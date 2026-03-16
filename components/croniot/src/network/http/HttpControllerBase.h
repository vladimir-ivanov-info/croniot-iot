#pragma once

#include <string>
#include "esp_log.h"
#include "cJSON.h"
#include "Result.h"

class HttpControllerBase {
public:
    virtual bool init() = 0;

    Result parseResult(const std::string& jsonString) {
        const char* TAG = "HttpController";

        cJSON* root = cJSON_Parse(jsonString.c_str());
        if (!root) {
            ESP_LOGE(TAG, "cJSON_Parse() failed: %s", cJSON_GetErrorPtr());
            return Result(false, "Local cJSON_Parse() failed, JSON:\n" + jsonString);
        }

        cJSON* successItem = cJSON_GetObjectItem(root, "success");
        cJSON* messageItem = cJSON_GetObjectItem(root, "message");

        bool success = (successItem && cJSON_IsBool(successItem)) ? cJSON_IsTrue(successItem) : false;
        std::string message = (messageItem && cJSON_IsString(messageItem)) ? messageItem->valuestring : "";

        cJSON_Delete(root);
        return Result(success, message);
    }

    virtual ~HttpControllerBase() = default;
};
