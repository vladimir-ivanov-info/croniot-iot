#include "provision/WifiCredentials.h"

#include <gtest/gtest.h>

using namespace croniot::provision;

TEST(WifiCredentialsTest, RoundTripsThroughJson) {
    WifiCredentials c{"My \"Home\" \\ Net", "pa\"ss\\word"};
    auto parsed = parseWifiJson(encodeWifiJson(c));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, c);
}

TEST(WifiCredentialsTest, OpenNetworkHasEmptyPassword) {
    auto parsed = parseWifiJson(R"({"ssid":"Cafe","password":""})");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->ssid, "Cafe");
    EXPECT_TRUE(parsed->password.empty());

    auto noPasswordKey = parseWifiJson(R"({"ssid":"Cafe"})");
    ASSERT_TRUE(noPasswordKey.has_value());
    EXPECT_TRUE(noPasswordKey->password.empty());
}

TEST(WifiCredentialsTest, IgnoresUnknownKeys) {
    auto parsed = parseWifiJson(R"({"v":1,"ssid":"Net","extra":{"a":[1,2]},"password":"12345678"})");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->password, "12345678");
}

TEST(WifiCredentialsTest, RejectsDamagedOrInvalidFiles) {
    EXPECT_FALSE(parseWifiJson("").has_value());
    EXPECT_FALSE(parseWifiJson(R"({"ssid":"Net","password":"12345678")").has_value());  // truncated
    EXPECT_FALSE(parseWifiJson(R"({"password":"12345678"})").has_value());              // no ssid
    EXPECT_FALSE(parseWifiJson(R"({"ssid":"","password":"12345678"})").has_value());
    EXPECT_FALSE(parseWifiJson(R"({"ssid":"Net","password":"short"})").has_value());
    EXPECT_FALSE(parseWifiJson(R"({"ssid":1,"password":"12345678"})").has_value());
}

TEST(WifiCredentialsTest, ValidatesSsid) {
    EXPECT_TRUE(ssidError("").has_value());
    EXPECT_FALSE(ssidError("a").has_value());
    EXPECT_FALSE(ssidError(std::string(32, 'x')).has_value());
    EXPECT_TRUE(ssidError(std::string(33, 'x')).has_value());
    EXPECT_TRUE(ssidError("bad\tname").has_value());
    EXPECT_FALSE(ssidError(" spaced ").has_value());
}

TEST(WifiCredentialsTest, ValidatesPassword) {
    EXPECT_FALSE(passwordError("").has_value());  // open network
    EXPECT_TRUE(passwordError("1234567").has_value());
    EXPECT_FALSE(passwordError("12345678").has_value());
    EXPECT_FALSE(passwordError(std::string(63, 'p')).has_value());
    EXPECT_FALSE(passwordError(std::string(64, 'a')).has_value());  // raw PSK, hex
    EXPECT_TRUE(passwordError(std::string(64, 'p')).has_value());   // 64 but not hex
    EXPECT_TRUE(passwordError(std::string(65, 'a')).has_value());
}

TEST(WifiCredentialsTest, DeviceFileWinsOverCompiledIn) {
    WifiCredentials out;
    WifiCredentials compiled{"Built", "12345678"};

    EXPECT_EQ(resolve(WifiCredentials{"Typed", "87654321"}, compiled, out), Source::Device);
    EXPECT_EQ(out.ssid, "Typed");

    EXPECT_EQ(resolve(std::nullopt, compiled, out), Source::Firmware);
    EXPECT_EQ(out.ssid, "Built");

    EXPECT_EQ(resolve(std::nullopt, WifiCredentials{}, out), Source::None);
    EXPECT_TRUE(out.ssid.empty());
}

TEST(WifiCredentialsTest, ClassifiesDisconnectReasons) {
    EXPECT_EQ(classifyDisconnect(15), ConnectFailure::WrongPassword);
    EXPECT_EQ(classifyDisconnect(204), ConnectFailure::WrongPassword);
    EXPECT_EQ(classifyDisconnect(201), ConnectFailure::NetworkNotFound);
    EXPECT_EQ(classifyDisconnect(210), ConnectFailure::NetworkNotFound);
    EXPECT_EQ(classifyDisconnect(1), ConnectFailure::Other);
}
