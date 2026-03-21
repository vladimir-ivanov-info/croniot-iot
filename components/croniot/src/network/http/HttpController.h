#pragma once

#include <string>
#include "../../Result.h"
#include "esp_log.h"
#include "CJsonPtr.h"

class HttpController {
public:
    virtual Result sendHttpPost(const std::string& content, const std::string& route) = 0;
    virtual ~HttpController() = default;

protected:
    Result parseResult(const std::string& jsonString) {
        const char* TAG = "HttpController";

        if (jsonString.empty() || jsonString[0] != '{') {
            ESP_LOGE(TAG, "Invalid or empty JSON string: '%s'", jsonString.c_str());
            return Result(false, "Empty or invalid JSON: " + jsonString);
        }

        CJsonPtr root(cJSON_Parse(jsonString.c_str()));
        if (!root) {
            ESP_LOGE(TAG, "cJSON_Parse() failed: %s", cJSON_GetErrorPtr());
            return Result(false, "Local cJSON_Parse() failed, JSON:\n" + jsonString);
        }

        cJSON* successItem = cJSON_GetObjectItem(root.get(), "success");
        cJSON* messageItem = cJSON_GetObjectItem(root.get(), "message");

        bool success = (successItem && cJSON_IsBool(successItem)) ? cJSON_IsTrue(successItem) : false;
        std::string message = (messageItem && cJSON_IsString(messageItem)) ? messageItem->valuestring : "";

        return Result(success, message);
    }
};
