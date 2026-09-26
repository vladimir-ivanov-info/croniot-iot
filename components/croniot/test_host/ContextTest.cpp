#include <gtest/gtest.h>

#include "log/Context.h"

using croniot::log::Context;

TEST(ContextTest, NoActiveContextMeansEmptyFields) {
    EXPECT_TRUE(Context::currentFields().empty());
}

TEST(ContextTest, FieldsAreVisibleWhileInScope) {
    Context ctx{{"task", "water"}, {"taskUid", "42"}};
    auto fields = Context::currentFields();
    ASSERT_EQ(fields.size(), 2u);
    EXPECT_EQ(fields[0].first, "task");
    EXPECT_EQ(fields[0].second, "water");
    EXPECT_EQ(fields[1].first, "taskUid");
    EXPECT_EQ(fields[1].second, "42");
}

TEST(ContextTest, FieldsDisappearWhenScopeEnds) {
    {
        Context ctx{{"transient", "yes"}};
        ASSERT_FALSE(Context::currentFields().empty());
    }
    EXPECT_TRUE(Context::currentFields().empty());
}

TEST(ContextTest, NestedContextsStackInnermostFirst) {
    Context outer{{"task", "water"}};
    {
        Context inner{{"step", "valve_open"}};
        auto fields = Context::currentFields();
        ASSERT_EQ(fields.size(), 2u);
        EXPECT_EQ(fields[0].first, "step");   // innermost first
        EXPECT_EQ(fields[1].first, "task");
    }
    auto fieldsAfterInnerEnds = Context::currentFields();
    ASSERT_EQ(fieldsAfterInnerEnds.size(), 1u);
    EXPECT_EQ(fieldsAfterInnerEnds[0].first, "task");
}
