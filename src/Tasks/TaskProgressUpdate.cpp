#include "TaskProgressUpdate.h"

String TaskProgressUpdate::toJson(){
    JsonDocument doc;

    doc["taskTypeUid"] = taskTypeUid;
    doc["taskUid"] = taskUid;
    doc["state"] = state;
    doc["progress"] = progress;
    doc["errorMessage"] = errorMessage;

    String json;
    serializeJsonPretty(doc, json);
    
    return json;
}