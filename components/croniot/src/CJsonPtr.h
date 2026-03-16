#pragma once

#include <memory>
#include "cJSON.h"

struct CJsonDeleter {
    void operator()(cJSON* p) const { cJSON_Delete(p); }
};

using CJsonPtr = std::unique_ptr<cJSON, CJsonDeleter>;

// Uso:
//   CJsonPtr root(cJSON_Parse(json));
//   if (!root) return error;
//   cJSON* item = cJSON_GetObjectItem(root.get(), "key");
//   // No hace falta cJSON_Delete, se libera solo al salir del scope
