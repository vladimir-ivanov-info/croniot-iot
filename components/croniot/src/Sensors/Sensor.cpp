#include "Sensor.h"

#include "esp_timer.h"

#include "Messages/MessageSensorData.h"
#include "SensorBatchEncoder.h"
#include "comm/MessageBus.h"
#include "log/Journal.h"

namespace {
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
}  // namespace

void Sensor::setReporting(uint32_t samplePeriodMs, const croniot::ReportPolicy& policy) {
    samplePeriodMs_ = samplePeriodMs;
    policy_ = policy;
}

void Sensor::sendSensorData(int sensorUid, const std::string& sensorValue) {
    MessageSensorData messageSensorData(sensorUid, sensorValue);
    croniot::MessageBus::instance().publishSensorData(sensorUid, messageSensorData.toString());
}

void Sensor::reportSample(int sensorUid, double value) {
    if (policy_.kind == croniot::ReportKind::Immediate) {
        sendSensorData(sensorUid, std::to_string(value));
        return;
    }

    auto it = buffers_.find(sensorUid);
    if (it == buffers_.end()) {
        it = buffers_.emplace(sensorUid, croniot::SensorReportBuffer(policy_)).first;
    }
    it->second.add({nowMs(), value});
}

void Sensor::maybeFlush(uint64_t nowMsValue) {
    for (auto& [sensorUid, buffer] : buffers_) {
        if (buffer.shouldFlush(nowMsValue)) {
            flushSensor(sensorUid, buffer);
        }
    }
}

void Sensor::flushSensor(int sensorUid, croniot::SensorReportBuffer& buffer) {
    auto samples = buffer.flush();
    if (samples.empty()) return;

    auto cbor = croniot::encodeSensorBatch(sensorUid, samples.front().timestampMs, samplePeriodMs_, samples);
    croniot::log::Journal::instance().appendRaw(croniot::log::Stream::Data, cbor.data(), cbor.size());
}
