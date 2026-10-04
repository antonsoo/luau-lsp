#include "doctest.h"
#include "Fixture.h"
#include "LSP/LanguageServer.hpp"

TEST_SUITE_BEGIN("OnTypeFormatting");

static lsp::DocumentOnTypeFormattingResult processOnTypeFormatting(Fixture* fixture, const std::string& source, const lsp::Position& position)
{
    auto uri = fixture->newDocument("foo.luau", source);

    lsp::DocumentOnTypeFormattingParams params;
    params.textDocument.uri = uri;
    params.position = position;
    params.ch = source.at(position.character);
    params.options = {4, true};

    return fixture->workspace.onTypeFormatting(params);
}

// Types `source` into a document that was last parsed as `parsedSource`
static lsp::DocumentOnTypeFormattingResult processOnTypeFormattingAfterEdit(
    Fixture* fixture, const std::string& parsedSource, const std::string& source, const lsp::Position& position)
{
    // Enable pull-based diagnostics, otherwise updateTextDocument will trigger a diagnostic check
    fixture->client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    fixture->client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};

    auto uri = fixture->newDocument("foo.luau", parsedSource);
    fixture->updateDocument(uri, source);

    lsp::DocumentOnTypeFormattingParams params;
    params.textDocument.uri = uri;
    params.position = position;
    params.ch = "{";
    params.options = {4, true};

    return fixture->workspace.onTypeFormatting(params);
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_disabled_by_default")
{
    auto [source, marker] = sourceWithMarker(R"(
        print("aaa {|")
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aaa {|")
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(edits->at(0).newText, "`");
    CHECK_EQ(edits->at(1).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        print(`aaa {`)
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_in_unfinished_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aaa {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        print(`aaa {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_single_quoted_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print('aaa {|')
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(edits->at(0).newText, "`");
    CHECK_EQ(edits->at(1).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        print(`aaa {`)
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_unfinished_single_quoted_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print('aaa {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        print(`aaa {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_handles_escaped_quote_in_unfinished_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aaa \"bbb {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        print(`aaa \"bbb {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_when_bracket_typed_at_end_of_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local x = "aaa {|}")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(local x = "aaa ")", source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(applyEdit(source, edits.value()), "local x = `aaa {}`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_when_bracket_typed_in_middle_of_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local x = "aaa {|}bbb")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(local x = "aaa bbb")", source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(applyEdit(source, edits.value()), "local x = `aaa {}bbb`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_of_string_typed_since_last_parse")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local x = "aaa {|}")");

    auto edits = processOnTypeFormattingAfterEdit(this, "local x = ", source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(applyEdit(source, edits.value()), "local x = `aaa {}`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_when_string_moved_since_last_parse")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local y = 1; print("aaa {|}bbb"))");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(print("aaa bbb"))", source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(applyEdit(source, edits.value()), "local y = 1; print(`aaa {}bbb`)");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_when_string_moved_to_another_line")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker("local y = 1\nprint(\"hello {|}world\")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(print("hello world"))", source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(applyEdit(source, edits.value()), "local y = 1\nprint(`hello {}world`)");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_quotes_after_edit_with_utf16_positions")
{
    client->globalConfig.format.convertQuotes = true;
    std::string source = "local x = \"\U0001F600\"; print(\"hi \U0001F600{}\u4E16\u754C\")";

    // The cursor and edits use UTF-16 columns, not UTF-8 bytes or Unicode code points.
    auto edits = processOnTypeFormattingAfterEdit(this, R"(print("hello world"))", source, {0, 29});
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 2);

    CHECK_EQ(edits->at(0).range, lsp::Range{{0, 22}, {0, 23}});
    CHECK_EQ(edits->at(1).range, lsp::Range{{0, 32}, {0, 33}});
    CHECK_EQ(edits->at(0).newText, "`");
    CHECK_EQ(edits->at(1).newText, "`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_backtick_content")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aa`a {|")
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_backtick_content_in_unfinished_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aa`a {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_backtick_before_quote_in_unfinished_string")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print(`" {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_string_before_bracket")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        print("aaa" {|)
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_handles_multiple_strings_in_line")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        x = "foo"; y = "bar {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        x = "foo"; y = `bar {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_handles_mixed_quotes_in_line")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        x = "foo"; y = 'bar {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        x = "foo"; y = `bar {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_handles_backtick_string_and_string_in_line")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        x = `foo`; y = "bar {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        x = `foo`; y = `bar {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_string_after_unfinished_backtick")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        x = `foo "bar {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_handles_quote_after_another_unclosed_quote")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(
        x = 'foo "bar {|
    )");

    auto edits = processOnTypeFormatting(this, source, marker);
    REQUIRE(edits.has_value());
    REQUIRE(edits->size() == 1);
    CHECK_EQ(edits->at(0).newText, "`");

    CHECK_EQ(applyEdit(source, edits.value()), R"(
        x = `foo "bar {
    )");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_quotes_in_comments")
{
    client->globalConfig.format.convertQuotes = true;
    const std::vector<std::string> comments = {
        R"(-- "hello {|)",
        R"(local x = 1 -- 'hello {|})",
        R"(--[[ "hello {|} ]])",
        "--[=[\n\"hello {|}\n]=]",
        "--[==[\n'hello {|",
        R"(--- "documentation {|})",
    };

    for (const auto& marked : comments)
    {
        auto [source, marker] = sourceWithMarker(marked);
        auto edits = processOnTypeFormattingAfterEdit(this, "", source, marker);
        CHECK_MESSAGE(!edits.has_value(), marked);
    }
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_uses_current_comment_locations")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(-- "hello {|}")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(local x = "hello ")", source, marker);
    CHECK(!edits.has_value());
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_strings_containing_comment_markers")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local x = "--[[ hello {|}")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(local x = "--[[ hello ")", source, marker);
    REQUIRE(edits.has_value());
    CHECK_EQ(applyEdit(source, *edits), "local x = `--[[ hello {}`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_unfinished_string_after_block_comment")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(--[[comment]] local x = "hello {|)");

    auto edits = processOnTypeFormattingAfterEdit(this, "--[[comment]] local x = ", source, marker);
    REQUIRE(edits.has_value());
    CHECK_EQ(applyEdit(source, *edits), "--[[comment]] local x = `hello {");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_converts_string_after_comment_removed")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(local x = "hello {|}")");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(-- "hello ")", source, marker);
    REQUIRE(edits.has_value());
    CHECK_EQ(applyEdit(source, *edits), "local x = `hello {}`");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_ignores_quotes_in_preceding_block_comment")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(--[[ " ]] local x = "hello {|)");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(--[[ " ]] local x = )", source, marker);
    REQUIRE(edits.has_value());
    CHECK_EQ(applyEdit(source, *edits), "--[[ \" ]] local x = `hello {");
}

TEST_CASE_FIXTURE(Fixture, "on_type_formatting_does_not_convert_comment_quote_before_table")
{
    client->globalConfig.format.convertQuotes = true;
    auto [source, marker] = sourceWithMarker(R"(--[[ " ]] local x = {|})");

    auto edits = processOnTypeFormattingAfterEdit(this, R"(--[[ " ]] local x = )", source, marker);
    CHECK(!edits.has_value());
}

TEST_SUITE_END();
