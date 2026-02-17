#include <Arduino.h>
#include <unity.h>
#include "utils/configuration/config.h"
#include "utils/configuration/config.cpp"

void setUp() {
    Preferences::resetAllMockStorage();
}

void tearDown() {}

// ============ parseAppCfg tests ============

void test_parse_valid_full_config() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"MyWiFi","pass":"secret123","params":[1.0, 25.5, 30.0, 40.0, 80.0, 20.0, 70.0, 8.0, 42.0]})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL_STRING("MyWiFi", cfg.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret123", cfg.password.c_str());
    TEST_ASSERT_EQUAL(9, cfg.params.size());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, cfg.params[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.5f, cfg.params[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 42.0f, cfg.params[8]);
}

void test_parse_ssid_and_password_only() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"TestNet","pass":"pw"})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL_STRING("TestNet", cfg.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("pw", cfg.password.c_str());
    TEST_ASSERT_EQUAL(0, cfg.params.size());
}

void test_parse_empty_params_array() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"Net","pass":"pw","params":[]})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL(0, cfg.params.size());
}

void test_parse_unknown_keys_skipped() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"Net","pass":"pw","cmd":"config","version":"1.0"})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL_STRING("Net", cfg.ssid.c_str());
}

void test_parse_missing_ssid_fails() {
    AppConfig cfg;
    std::string msg = R"({"pass":"pw"})";
    TEST_ASSERT_FALSE(ConfigHandler::parseAppCfg(msg, cfg));
}

void test_parse_missing_password_fails() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"Net"})";
    TEST_ASSERT_FALSE(ConfigHandler::parseAppCfg(msg, cfg));
}

void test_parse_no_opening_brace_fails() {
    AppConfig cfg;
    std::string msg = R"("ssid":"Net","pass":"pw"})";
    TEST_ASSERT_FALSE(ConfigHandler::parseAppCfg(msg, cfg));
}

void test_parse_empty_string_fails() {
    AppConfig cfg;
    std::string msg = "";
    TEST_ASSERT_FALSE(ConfigHandler::parseAppCfg(msg, cfg));
}

void test_parse_unclosed_string_fails() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"Net)";
    TEST_ASSERT_FALSE(ConfigHandler::parseAppCfg(msg, cfg));
}

void test_parse_escape_in_password() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"Net","pass":"pass\"word"})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL_STRING("pass\"word", cfg.password.c_str());
}

void test_parse_negative_float_params() {
    AppConfig cfg;
    std::string msg = R"({"ssid":"N","pass":"P","params":[-5.0, 10.0, -3.5]})";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL(3, cfg.params.size());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -5.0f, cfg.params[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -3.5f, cfg.params[2]);
}

void test_parse_whitespace_handling() {
    AppConfig cfg;
    std::string msg = R"(  {  "ssid" : "Net" , "pass" : "pw" , "params" : [ 1.0 , 2.0 ] }  )";
    TEST_ASSERT_TRUE(ConfigHandler::parseAppCfg(msg, cfg));
    TEST_ASSERT_EQUAL_STRING("Net", cfg.ssid.c_str());
    TEST_ASSERT_EQUAL(2, cfg.params.size());
}

// ============ NVS save/load roundtrip tests ============

void test_save_and_load_roundtrip() {
    AppConfig saved;
    saved.ssid = "TestSSID";
    saved.password = "TestPass";
    saved.params = {1.0f, 2.0f, 3.0f};

    TEST_ASSERT_TRUE(ConfigHandler::save(saved));

    AppConfig loaded;
    TEST_ASSERT_TRUE(ConfigHandler::load(loaded));
    TEST_ASSERT_EQUAL_STRING("TestSSID", loaded.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("TestPass", loaded.password.c_str());
    TEST_ASSERT_EQUAL(3, loaded.params.size());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, loaded.params[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, loaded.params[2]);
}

void test_is_configured_after_save() {
    AppConfig cfg;
    cfg.ssid = "Net";
    cfg.password = "pw";
    TEST_ASSERT_TRUE(ConfigHandler::save(cfg));
    TEST_ASSERT_TRUE(ConfigHandler::isConfigured());
}

void test_clear_invalidates_config() {
    AppConfig cfg;
    cfg.ssid = "Net";
    cfg.password = "pw";
    ConfigHandler::save(cfg);
    TEST_ASSERT_TRUE(ConfigHandler::clear());
    TEST_ASSERT_FALSE(ConfigHandler::isConfigured());
}

void test_set_unconfigured() {
    AppConfig cfg;
    cfg.ssid = "Net";
    cfg.password = "pw";
    ConfigHandler::save(cfg);
    TEST_ASSERT_TRUE(ConfigHandler::setUnconfigured());
    TEST_ASSERT_FALSE(ConfigHandler::isConfigured());
}

void test_load_without_save_fails() {
    AppConfig cfg;
    TEST_ASSERT_FALSE(ConfigHandler::load(cfg));
}

void test_save_no_params() {
    AppConfig saved;
    saved.ssid = "X";
    saved.password = "Y";
    // params is empty

    TEST_ASSERT_TRUE(ConfigHandler::save(saved));

    AppConfig loaded;
    TEST_ASSERT_TRUE(ConfigHandler::load(loaded));
    TEST_ASSERT_EQUAL(0, loaded.params.size());
}

int main() {
    UNITY_BEGIN();

    // Parser tests
    RUN_TEST(test_parse_valid_full_config);
    RUN_TEST(test_parse_ssid_and_password_only);
    RUN_TEST(test_parse_empty_params_array);
    RUN_TEST(test_parse_unknown_keys_skipped);
    RUN_TEST(test_parse_missing_ssid_fails);
    RUN_TEST(test_parse_missing_password_fails);
    RUN_TEST(test_parse_no_opening_brace_fails);
    RUN_TEST(test_parse_empty_string_fails);
    RUN_TEST(test_parse_unclosed_string_fails);
    RUN_TEST(test_parse_escape_in_password);
    RUN_TEST(test_parse_negative_float_params);
    RUN_TEST(test_parse_whitespace_handling);

    // NVS roundtrip tests
    RUN_TEST(test_save_and_load_roundtrip);
    RUN_TEST(test_is_configured_after_save);
    RUN_TEST(test_clear_invalidates_config);
    RUN_TEST(test_set_unconfigured);
    RUN_TEST(test_load_without_save_fails);
    RUN_TEST(test_save_no_params);

    return UNITY_END();
}
