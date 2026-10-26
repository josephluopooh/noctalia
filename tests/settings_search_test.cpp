#include "i18n/i18n_service.h"
#include "shell/settings/settings_registry.h"
#include "tests/test_check.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
  TEST_CHECK(argc == 2);
  TEST_CHECK(setenv("NOCTALIA_ASSETS_DIR", argv[1], 1) == 0);
  i18n::Service::instance().init("ru");

  const Config config;
  const auto entries = settings::buildSettingsRegistry(config, nullptr);
  const std::vector<std::string> path{"brightness", "minimum_brightness"};
  const auto entry = std::ranges::find(entries, path, &settings::SettingEntry::path);
  TEST_CHECK(entry != entries.end());
  TEST_CHECK(settings::matchesSettingQuery(*entry, "МИНИМАЛЬНАЯ"));
  TEST_CHECK(settings::matchesSettingQuery(*entry, "минимальная"));
  TEST_CHECK(settings::matchesSettingQuery(*entry, "ЯРКОСТЬ"));
  TEST_CHECK(settings::matchesSettingQuery(*entry, "яркость"));
  TEST_CHECK(!settings::matchesSettingQuery(*entry, "несуществующий"));
  TEST_CHECK(settings::matchesSettingQuery(*entry, {}));

  TEST_CHECK(settings::normalizedSettingQuery("ÉCRAN") == "écran");
  TEST_CHECK(settings::normalizedSettingQuery("Straße") == "strasse");
  TEST_CHECK(settings::normalizedSettingQuery("ΣΊΣΥΦΟΣ") == settings::normalizedSettingQuery("Σίσυφος"));
  TEST_CHECK(settings::normalizedSettingQuery("日本語") == "日本語");
  TEST_CHECK(settings::normalizedSettingQuery("BRIGHTNESS") == "brightness");
  TEST_CHECK(settings::normalizedSettingQuery({}).empty());
  const std::string bounded = "ЯРКОСТЬ EXTRA";
  TEST_CHECK(settings::normalizedSettingQuery(std::string_view(bounded).substr(0, 14)) == "яркость");
}
