#ifndef MQTTCONTROLLER_H
#define MQTTCONTROLLER_H

#include <functional>
#include <string>
#include "../../Result.h"
#include "../../Tasks/TaskBase.h"

class MqttController {
public:
    bool initialized = false;

    virtual bool init() = 0;

    virtual Result publish(const std::string& topic, const std::string& message) = 0;

    // Same as publish(), but with an explicit QoS/retain instead of the
    // hardcoded QoS 2 / non-retained behavior publish() has always had -
    // added rather than changing publish()'s signature so every existing
    // call site (sensor data, task progress) keeps behaving exactly as
    // before. The uplink's status/birth (retained) and log/event batches
    // (QoS 1, plan §4) are the first callers that need control over this.
    virtual Result publishWithOptions(const std::string& topic, const std::string& message, int qos,
                                       bool retain) = 0;

    virtual void registerCallback(const std::string& topic, TaskBase* taskInstance) = 0;

    virtual void registerCallbackTaskStateInfoSync(const std::string& topic, TaskBase* taskInstance) = 0;

    // Plain-payload subscription for topics that aren't task-command
    // shaped (their last path segment isn't a numeric taskTypeUid) - the
    // uplink's `/server/<uuid>/ack` and `/server/<uuid>/log_config`
    // (plan §4/§5) are the first users. Kept separate from
    // registerCallback() rather than overloading it, since the two kinds
    // of topic are routed completely differently once a message arrives
    // (see WifiMqttController::processInWorker()).
    virtual void registerRawCallback(const std::string& topic,
                                      std::function<void(const std::string& payload)> callback) = 0;

    virtual ~MqttController() = default;
};

#endif
