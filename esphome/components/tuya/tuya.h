#pragma once

#include <cinttypes>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome {
namespace tuya {

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  size_t len;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaDatapoint)> on_datapoint;
};

enum class TuyaCommandType : uint8_t {
  FACTORY_RESET = 0x00,
  PRODUCT_QUERY = 0x01,
  NETWORK_STATUS = 0x02,
  CONFIGURATION_COMMAND = 0x03,
  COMMAND = 0x04,
  RESPONSE = 0x05,
  DATAPOINT_PROACTIVE = 0x06,
  QUERY_DATAPOINTS = 0x28,
};

enum class TuyaExtendedServicesCommandType : uint8_t {
  RESET_NOTIFICATION = 0x04,
  MODULE_RESET = 0x05,
  UPDATE_IN_PROGRESS = 0x0A,
};

enum class TuyaInitState : uint8_t {
  INIT_PRODUCT = 0x00,
  INIT_DONE,
};

struct TuyaCommand {
  uint16_t seq_no;
  TuyaCommandType cmd;
  std::vector<uint8_t> payload;
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func);
  void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_tx_wake_up_pin(GPIOPin *tx_wake_up_pin) { this->tx_wake_up_pin_ = tx_wake_up_pin; }
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  TuyaInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  void add_on_initialized_callback(std::function<void()> callback) {
    this->initialized_callback_.add(std::move(callback));
  }

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  optional<TuyaDatapoint> get_datapoint_(uint8_t datapoint_id);
  bool validate_message_();

  void handle_command_(uint16_t seq_no, uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(const TuyaCommand &command);
  void send_empty_command_(TuyaCommandType command);
  uint16_t get_next_seq_no();
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  void send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data);
  void set_status_pin_();

#ifdef USE_TIME
  void send_local_time_();
  time::RealTimeClock *time_id_{nullptr};
  bool time_sync_callback_registered_{false};
#endif
  TuyaInitState init_state_ = TuyaInitState::INIT_PRODUCT;
  bool init_failed_{false};
  int init_retries_{0};
  uint8_t protocol_version_ = -1;
  InternalGPIOPin *status_pin_{nullptr};
  GPIOPin *tx_wake_up_pin_;
  int status_pin_reported_ = -1;
  int reset_pin_reported_ = -1;
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;
  std::string product_ = "";
  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  std::vector<TuyaCommand> command_queue_;
  optional<TuyaCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  CallbackManager<void()> initialized_callback_{};
};

}  // namespace tuya
}  // namespace esphome
