#include <gtest/gtest.h>

#include "log/LevelResolver.h"

using croniot::log::Level;
using croniot::log::LevelResolver;

TEST(LevelResolver, FallsBackToKconfigDefaultWhenNothingElseIsSet) {
    LevelResolver resolver;
    resolver.setKconfigDefault(Level::Info);
    EXPECT_EQ(resolver.effectiveLevel("AnyTag", 1000), Level::Info);
}

TEST(LevelResolver, CodeDefaultOverridesKconfigDefault) {
    LevelResolver resolver;
    resolver.setKconfigDefault(Level::Info);
    resolver.setCodeDefault(Level::Debug);
    EXPECT_EQ(resolver.effectiveLevel("AnyTag", 1000), Level::Debug);
}

TEST(LevelResolver, CodeTagOverrideWinsOverCodeDefault) {
    LevelResolver resolver;
    resolver.setCodeDefault(Level::Info);
    resolver.setCodeTagLevel("TaskWaterPlants", Level::Debug);
    EXPECT_EQ(resolver.effectiveLevel("TaskWaterPlants", 1000), Level::Debug);
    EXPECT_EQ(resolver.effectiveLevel("OtherTag", 1000), Level::Info);
}

TEST(LevelResolver, RemoteConfigOverridesCode) {
    LevelResolver resolver;
    resolver.setCodeDefault(Level::Info);
    resolver.setRemoteDefault(Level::Warn);
    EXPECT_EQ(resolver.effectiveLevel("AnyTag", 1000), Level::Warn);
}

TEST(LevelResolver, RemoteTagOverrideWinsOverEverythingPersisted) {
    LevelResolver resolver;
    resolver.setKconfigDefault(Level::Info);
    resolver.setCodeDefault(Level::Debug);
    resolver.setCodeTagLevel("Tag", Level::Debug);
    resolver.setRemoteTagLevel("Tag", Level::Trace);
    EXPECT_EQ(resolver.effectiveLevel("Tag", 1000), Level::Trace);
}

TEST(LevelResolver, TtlOverrideWinsWhileValid) {
    LevelResolver resolver;
    resolver.setRemoteDefault(Level::Warn);
    resolver.setTtlOverride(Level::Trace, /*expiresAtMs=*/2000);
    EXPECT_EQ(resolver.effectiveLevel("Tag", 1000), Level::Trace);
}

TEST(LevelResolver, TtlOverrideRevertsToRemoteOnceExpired) {
    LevelResolver resolver;
    resolver.setRemoteDefault(Level::Warn);
    resolver.setTtlOverride(Level::Trace, /*expiresAtMs=*/2000);
    EXPECT_EQ(resolver.effectiveLevel("Tag", 2500), Level::Warn);
}

TEST(LevelResolver, TtlTagOverrideDoesNotAffectOtherTags) {
    LevelResolver resolver;
    resolver.setRemoteDefault(Level::Warn);
    resolver.setTtlTagOverride("Special", Level::Trace, 5000);
    EXPECT_EQ(resolver.effectiveLevel("Special", 1000), Level::Trace);
    EXPECT_EQ(resolver.effectiveLevel("Other", 1000), Level::Warn);
}

TEST(LevelResolver, ExpiredTagTtlFallsBackToStillValidBlanketTtl) {
    LevelResolver resolver;
    resolver.setRemoteDefault(Level::Warn);
    resolver.setTtlTagOverride("Special", Level::Error, /*expiresAtMs=*/1000);
    resolver.setTtlOverride(Level::Trace, /*expiresAtMs=*/9000);
    // The tag-specific override lapsed at 1000; the blanket one is still
    // running, so it should take over instead of falling straight to Warn.
    EXPECT_EQ(resolver.effectiveLevel("Special", 1500), Level::Trace);
}

TEST(LevelResolver, ClearRemoteFallsBackToCode) {
    LevelResolver resolver;
    resolver.setCodeDefault(Level::Info);
    resolver.setRemoteDefault(Level::Warn);
    resolver.clearRemote();
    EXPECT_EQ(resolver.effectiveLevel("Tag", 1000), Level::Info);
}

TEST(LevelResolver, ClearTtlOverridesFallsBackToPersisted) {
    LevelResolver resolver;
    resolver.setRemoteDefault(Level::Warn);
    resolver.setTtlOverride(Level::Trace, 9000);
    resolver.clearTtlOverrides();
    EXPECT_EQ(resolver.effectiveLevel("Tag", 1000), Level::Warn);
}
