#include "dbus/system_bus.h"
#include "ipc/ipc_service.h"
#include "system/keyboard_backlight_service.h"
#include "tests/test_check.h"

#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

namespace {

  constexpr auto kInterface = "org.freedesktop.UPower.KbdBacklight";

  class FakeBacklight {
  public:
    FakeBacklight(sdbus::IConnection& connection, int index, int maxBrightness)
        : path("/org/freedesktop/UPower/KbdBacklight/" + std::to_string(index)), maximum(maxBrightness),
          object(sdbus::createObject(connection, path)) {
      object
          ->addVTable(
              sdbus::registerMethod("GetMaxBrightness").implementedAs([this]() { return this->maximum; }),
              sdbus::registerMethod("GetBrightness").implementedAs([this]() { return brightness.load(); }),
              sdbus::registerMethod("SetBrightness").implementedAs([this](int value) {
                TEST_CHECK(value >= 0 && value <= this->maximum);
                brightness = value;
              }),
              sdbus::registerSignal("BrightnessChanged").withParameters<int>("brightness")
          )
          .forInterface(kInterface);
    }

    sdbus::ObjectPath path;
    int maximum;
    std::atomic<int> brightness{0};
    std::unique_ptr<sdbus::IObject> object;
  };

} // namespace

int main() {
  const auto connection = sdbus::createSessionBusConnection(sdbus::ServiceName{"org.freedesktop.UPower"});
  const auto manager = sdbus::createObject(*connection, sdbus::ObjectPath{"/org/freedesktop/UPower"});
  const std::array maximums{14660, 255, 3, 0, std::numeric_limits<int>::max()};
  const std::array steps{733, 13, 1, 0, 107374182};
  std::vector<std::unique_ptr<FakeBacklight>> devices;
  devices.reserve(maximums.size());
  for (std::size_t i = 0; i < maximums.size(); ++i) {
    devices.push_back(std::make_unique<FakeBacklight>(*connection, static_cast<int>(i), maximums[i]));
  }
  manager
      ->addVTable(
          sdbus::registerMethod("EnumerateKbdBacklights").implementedAs([&devices]() {
            std::vector<sdbus::ObjectPath> paths;
            paths.reserve(devices.size());
            for (const auto& device : devices) {
              paths.push_back(device->path);
            }
            return paths;
          }),
          sdbus::registerSignal("DeviceAdded").withParameters<sdbus::ObjectPath>("path"),
          sdbus::registerSignal("DeviceRemoved").withParameters<sdbus::ObjectPath>("path")
      )
      .forInterface("org.freedesktop.UPower");
  connection->enterEventLoopAsync();

  SystemBus bus;
  KeyboardBacklightService service(bus);
  IpcService ipc;
  service.registerIpc(ipc);
  TEST_CHECK(service.available());
  TEST_CHECK(ipc.execute("keyboard-backlight-up") == "ok\n");
  for (std::size_t i = 0; i < devices.size(); ++i) {
    TEST_CHECK(devices[i]->brightness == steps[i]);
  }
  TEST_CHECK(ipc.execute("keyboard-backlight-down") == "ok\n");
  TEST_CHECK(ipc.execute("keyboard-backlight-down") == "ok\n");
  for (const auto& device : devices) {
    TEST_CHECK(device->brightness == 0);
  }
  TEST_CHECK(ipc.execute("keyboard-backlight-set 100") == "ok\n");
  TEST_CHECK(ipc.execute("keyboard-backlight-up") == "ok\n");
  for (const auto& device : devices) {
    TEST_CHECK(device->brightness == device->maximum);
  }
  TEST_CHECK(ipc.execute("keyboard-backlight-down") == "ok\n");
  for (std::size_t i = 0; i < devices.size(); ++i) {
    TEST_CHECK(devices[i]->brightness == maximums[i] - steps[i]);
  }
  TEST_CHECK(ipc.execute("keyboard-backlight-toggle") == "ok\n");
  for (const auto& device : devices) {
    TEST_CHECK(device->brightness == 0);
  }
  TEST_CHECK(ipc.execute("keyboard-backlight-up extra").starts_with("error:"));
  for (const auto& device : devices) {
    TEST_CHECK(device->brightness == 0);
  }
}
