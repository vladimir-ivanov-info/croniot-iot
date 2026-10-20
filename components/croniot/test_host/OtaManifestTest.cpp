#include "ota/OtaManifest.h"

#include <gtest/gtest.h>

using croniot::ota::parseManifest;

namespace {

const char* kSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

std::string manifestJson(const std::string& extra = "") {
    return std::string(R"({"requestId":"r-1","version":"1.4.0","path":"/api/iot/firmware/12","size":1960112,"sha256":")") +
           kSha + "\"" + extra + "}";
}

}  // namespace

TEST(OtaManifestTest, ParsesFullManifest) {
    auto m = parseManifest(manifestJson(R"(,"port":8091)"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->requestId, "r-1");
    EXPECT_EQ(m->version, "1.4.0");
    EXPECT_EQ(m->path, "/api/iot/firmware/12");
    EXPECT_EQ(m->size, 1960112u);
    EXPECT_EQ(m->sha256, kSha);
    EXPECT_EQ(m->port, 8091);
}

TEST(OtaManifestTest, PortIsOptional) {
    auto m = parseManifest(manifestJson());
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->port, 0);
}

TEST(OtaManifestTest, UppercaseShaIsNormalized) {
    std::string upper = R"({"requestId":"r","version":"2","path":"/f","size":10,"sha256":"ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789"})";
    auto m = parseManifest(upper);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->sha256, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
}

TEST(OtaManifestTest, UnknownFieldsAreSkipped) {
    auto m = parseManifest(manifestJson(R"(,"notes":{"a":[1,2,"x"]},"future":true)"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->version, "1.4.0");
}

TEST(OtaManifestTest, EmptyPayloadClearsTheRequest) {
    EXPECT_FALSE(parseManifest("").has_value());
    EXPECT_FALSE(parseManifest("{}").has_value());
}

TEST(OtaManifestTest, MissingRequiredFieldsAreRejected) {
    EXPECT_FALSE(parseManifest(R"({"version":"1","path":"/f","size":1,"sha256":")" + std::string(kSha) + "\"}").has_value());
    EXPECT_FALSE(parseManifest(R"({"requestId":"r","path":"/f","size":1,"sha256":")" + std::string(kSha) + "\"}").has_value());
    EXPECT_FALSE(parseManifest(R"({"requestId":"r","version":"1","size":1,"sha256":")" + std::string(kSha) + "\"}").has_value());
    EXPECT_FALSE(parseManifest(R"({"requestId":"r","version":"1","path":"/f","sha256":")" + std::string(kSha) + "\"}").has_value());
    EXPECT_FALSE(parseManifest(R"({"requestId":"r","version":"1","path":"/f","size":1})").has_value());
}

TEST(OtaManifestTest, PathCannotNameAnotherHost) {
    auto withPath = [](const std::string& path) {
        return std::string(R"({"requestId":"r","version":"1","size":1,"sha256":")") + kSha + R"(","path":")" + path + "\"}";
    };
    EXPECT_FALSE(parseManifest(withPath("http://evil/fw.bin")).has_value());
    EXPECT_FALSE(parseManifest(withPath("//evil/fw.bin")).has_value());
    EXPECT_FALSE(parseManifest(withPath("fw.bin")).has_value());
    EXPECT_FALSE(parseManifest(withPath("/a b")).has_value());
    EXPECT_TRUE(parseManifest(withPath("/api/iot/firmware/1")).has_value());
}

TEST(OtaManifestTest, OutOfRangeNumbersAreRejected) {
    EXPECT_FALSE(parseManifest(manifestJson(R"(,"port":70000)")).has_value());
    EXPECT_FALSE(parseManifest(manifestJson(R"(,"port":-1)")).has_value());
    std::string zeroSize = R"({"requestId":"r","version":"1","path":"/f","size":0,"sha256":")" + std::string(kSha) + "\"}";
    EXPECT_FALSE(parseManifest(zeroSize).has_value());
    std::string huge = R"({"requestId":"r","version":"1","path":"/f","size":16777217,"sha256":")" + std::string(kSha) + "\"}";
    EXPECT_FALSE(parseManifest(huge).has_value());
}

TEST(OtaManifestTest, VersionMustFitTheAppDescriptor) {
    std::string longVersion(32, 'v');
    std::string json = R"({"requestId":"r","path":"/f","size":1,"sha256":")" + std::string(kSha) + R"(","version":")" + longVersion + "\"}";
    EXPECT_FALSE(parseManifest(json).has_value());
}

TEST(OtaManifestTest, BadShaIsRejected) {
    std::string json = R"({"requestId":"r","version":"1","path":"/f","size":1,"sha256":"xyz"})";
    EXPECT_FALSE(parseManifest(json).has_value());
}
