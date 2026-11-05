#include "provision/WifiSetupDialog.h"

#include <gtest/gtest.h>

using namespace croniot::provision;
using Action = WifiSetupDialog::Action;

namespace {

// Types `text` into the editor and returns the finished line, if any,
// plus everything echoed back.
std::optional<std::string> type(LineEditor& editor, const std::string& text, std::string& echoed) {
    std::optional<std::string> line;
    std::string echo;
    for (char c : text) {
        auto done = editor.feed(c, echo);
        echoed += echo;
        if (done) line = done;
    }
    return line;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST(LineEditorTest, EchoesAndFinishesOnEnter) {
    LineEditor editor;
    std::string echoed;
    auto line = type(editor, "wifi\r", echoed);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(*line, "wifi");
    EXPECT_EQ(echoed, "wifi\r\n");
}

TEST(LineEditorTest, CrLfIsOneEnter) {
    LineEditor editor;
    std::string echoed;
    EXPECT_EQ(type(editor, "a\r\n", echoed), std::optional<std::string>("a"));
    // The LF must not finish a second, empty line.
    std::string echo;
    EXPECT_FALSE(editor.feed('b', echo).has_value());
    EXPECT_EQ(type(editor, "\n", echoed), std::optional<std::string>("b"));
}

TEST(LineEditorTest, MasksPasswords) {
    LineEditor editor;
    editor.setMasked(true);
    std::string echoed;
    auto line = type(editor, "secret12\r", echoed);
    EXPECT_EQ(*line, "secret12");
    EXPECT_EQ(echoed, "********\r\n");
    EXPECT_FALSE(contains(echoed, "secret"));
}

TEST(LineEditorTest, BackspaceErasesWholeCharacters) {
    LineEditor editor;
    std::string echoed;
    // "Añb", then two backspaces: 'b' and the two-byte 'ñ' go.
    auto line = type(editor, "A\xC3\xB1" "b\x7f\x08\r", echoed);
    EXPECT_EQ(*line, "A");
    // Backspace on an empty line does nothing.
    std::string echo;
    editor.feed(0x7f, echo);
    EXPECT_TRUE(echo.empty());
}

TEST(WifiSetupDialogTest, HappyPathConnectsAndSaves) {
    WifiSetupDialog d;
    auto r = d.onLine("wifi");
    EXPECT_TRUE(contains(r.text, "SSID"));
    EXPECT_FALSE(d.expectsPassword());

    r = d.onLine("Home");
    EXPECT_TRUE(contains(r.text, "Password"));
    EXPECT_TRUE(d.expectsPassword());

    r = d.onLine("12345678");
    EXPECT_EQ(r.action, Action::TryConnect);
    EXPECT_EQ(d.pending().ssid, "Home");
    EXPECT_EQ(d.pending().password, "12345678");
    EXPECT_TRUE(d.busy());

    r = d.onConnectResult(true, ConnectFailure::Other);
    EXPECT_EQ(r.action, Action::Save);
    EXPECT_TRUE(contains(r.text, "OK"));
    // The console says "Saved" only once the write worked.
    EXPECT_FALSE(contains(r.text, "Saved"));
    EXPECT_FALSE(d.busy());
}

TEST(WifiSetupDialogTest, NeverPrintsThePassword) {
    WifiSetupDialog d;
    std::string all;
    all += d.onLine("wifi").text;
    all += d.onLine("Home").text;
    all += d.onLine("topsecret").text;
    all += d.onConnectResult(false, ConnectFailure::WrongPassword).text;
    all += d.onLine("y").text;
    EXPECT_FALSE(contains(all, "topsecret"));
}

TEST(WifiSetupDialogTest, FailureSaysWhyAndAsksBeforeSaving) {
    WifiSetupDialog d;
    d.onLine("wifi");
    d.onLine("Home");
    d.onLine("12345678");

    auto r = d.onConnectResult(false, ConnectFailure::WrongPassword);
    EXPECT_EQ(r.action, Action::None);
    EXPECT_TRUE(contains(r.text, "FAILED"));
    EXPECT_TRUE(contains(r.text, "rejected the password"));
    EXPECT_TRUE(contains(r.text, "[y/N]"));

    // Anything but yes reverts.
    r = d.onLine("");
    EXPECT_EQ(r.action, Action::Revert);
    EXPECT_FALSE(d.busy());
}

TEST(WifiSetupDialogTest, FailureCanBeSavedAnyway) {
    WifiSetupDialog d;
    d.onLine("wifi");
    d.onLine("Garden");
    d.onLine("12345678");
    d.onConnectResult(false, ConnectFailure::NetworkNotFound);
    auto r = d.onLine("y");
    EXPECT_EQ(r.action, Action::Save);
    EXPECT_FALSE(contains(r.text, "Saved"));
}

TEST(WifiSetupDialogTest, RejectsInvalidInputAndAsksAgain) {
    WifiSetupDialog d;
    d.onLine("wifi");
    auto r = d.onLine(std::string(33, 'x'));
    EXPECT_TRUE(contains(r.text, "32"));
    EXPECT_TRUE(contains(r.text, "SSID"));

    d.onLine("Home");
    r = d.onLine("short");
    EXPECT_EQ(r.action, Action::None);
    EXPECT_TRUE(contains(r.text, "at least 8"));
    EXPECT_TRUE(d.expectsPassword());
}

TEST(WifiSetupDialogTest, EmptyNameCancels) {
    WifiSetupDialog d;
    d.onLine("wifi");
    auto r = d.onLine("");
    EXPECT_TRUE(contains(r.text, "Cancelled"));
    EXPECT_FALSE(d.busy());
}

TEST(WifiSetupDialogTest, KeepsSpacesInNetworkName) {
    WifiSetupDialog d;
    d.onLine("wifi");
    d.onLine(" My Net ");
    d.onLine("");  // open network
    EXPECT_EQ(d.pending().ssid, " My Net ");
    EXPECT_TRUE(d.pending().password.empty());
}

TEST(WifiSetupDialogTest, OtherCommands) {
    WifiSetupDialog d;
    EXPECT_EQ(d.onLine("WiFi Status").action, Action::ShowStatus);
    EXPECT_EQ(d.onLine("  wifi clear ").action, Action::Clear);
    EXPECT_EQ(d.onLine("").action, Action::None);
    auto r = d.onLine("hello");
    EXPECT_TRUE(contains(r.text, "Unknown command"));
    EXPECT_TRUE(contains(r.text, "wifi status"));
}

TEST(WifiSetupDialogTest, ConnectResultOutsideAnAttemptIsIgnored) {
    WifiSetupDialog d;
    EXPECT_EQ(d.onConnectResult(true, ConnectFailure::Other).action, Action::None);
}

TEST(LineEditorTest, ResetDropsAHalfTypedLine) {
    LineEditor editor;
    std::string echoed;
    type(editor, "abc", echoed);
    editor.reset();
    EXPECT_EQ(type(editor, "y\r", echoed), std::optional<std::string>("y"));
}
