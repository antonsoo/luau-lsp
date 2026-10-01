#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("CallHierarchy");

static lsp::CallHierarchyItem prepareCallHierarchy(Fixture* fixture, const lsp::DocumentUri& uri, const lsp::Position& position)
{
    lsp::CallHierarchyPrepareParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = position;

    auto items = fixture->workspace.prepareCallHierarchy(params, nullptr);
    REQUIRE_EQ(1, items.size());
    return items[0];
}

TEST_CASE_FIXTURE(Fixture, "incoming_calls_of_a_local_function")
{
    auto uri = newDocument("foo.luau", R"(
        local function useFunction()
        end

        local function caller()
            useFunction()
        end

        useFunction()
    )");

    auto item = prepareCallHierarchy(this, uri, lsp::Position{1, 28});
    CHECK_EQ("useFunction", item.name);
    CHECK_EQ(uri, item.uri);

    auto result = workspace.callHierarchyIncomingCalls(lsp::CallHierarchyIncomingCallsParams{item}, nullptr);
    REQUIRE_EQ(2, result.size());

    CHECK_EQ("caller", result[0].from.name);
    CHECK_EQ(uri, result[0].from.uri);
    REQUIRE_EQ(1, result[0].fromRanges.size());
    CHECK_EQ(lsp::Range{{5, 12}, {5, 23}}, result[0].fromRanges[0]);

    CHECK_EQ("<no function>", result[1].from.name);
    REQUIRE_EQ(1, result[1].fromRanges.size());
    CHECK_EQ(lsp::Range{{8, 8}, {8, 19}}, result[1].fromRanges[0]);
}

TEST_CASE_FIXTURE(Fixture, "outgoing_calls_of_a_local_function")
{
    auto uri = newDocument("foo.luau", R"(
        local function useFunction()
        end

        local function caller()
            useFunction()
        end
    )");

    auto item = prepareCallHierarchy(this, uri, lsp::Position{4, 25});
    CHECK_EQ("caller", item.name);

    auto result = workspace.callHierarchyOutgoingCalls(lsp::CallHierarchyOutgoingCallsParams{item}, nullptr);
    REQUIRE_EQ(1, result.size());

    CHECK_EQ("useFunction", result[0].to.name);
    CHECK_EQ(uri, result[0].to.uri);
    REQUIRE_EQ(1, result[0].fromRanges.size());
    CHECK_EQ(lsp::Range{{5, 12}, {5, 23}}, result[0].fromRanges[0]);
}

TEST_CASE_FIXTURE(Fixture, "cross_module_incoming_calls_include_dependents_that_have_not_been_checked")
{
    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local function caller()
            useFunction()
        end
    )");

    auto item = prepareCallHierarchy(this, uri, lsp::Position{1, 28});

    auto result = workspace.callHierarchyIncomingCalls(lsp::CallHierarchyIncomingCallsParams{item}, nullptr);
    REQUIRE_EQ(1, result.size());

    CHECK_EQ("caller", result[0].from.name);
    CHECK_EQ(user, result[0].from.uri);
    REQUIRE_EQ(1, result[0].fromRanges.size());
    CHECK_EQ(lsp::Range{{4, 12}, {4, 23}}, result[0].fromRanges[0]);
}

// Use-after-free regression test: a dependent that was checked earlier retains a type graph that references the types of
// the module it requires. Rechecking the required module destroys those types, so the dependent must be rechecked before
// its type graph is read (without this, the incoming calls request below crashes under ASAN)
TEST_CASE_FIXTURE(Fixture, "cross_module_incoming_calls_recheck_dependents_after_the_required_module_changed")
{
    // Enable pull-based diagnostics so that updating the document does not synchronously recheck its dependents
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};

    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local function caller()
            useFunction()
        end
    )");

    workspace.checkStrict(workspace.fileResolver.getModuleName(user), /* cancellationToken= */ nullptr);

    updateDocument(uri, R"(
        local function useFunction(): number
            return 1
        end

        return useFunction
    )");

    // Rechecks useFunction.luau
    auto item = prepareCallHierarchy(this, uri, lsp::Position{1, 28});

    auto result = workspace.callHierarchyIncomingCalls(lsp::CallHierarchyIncomingCallsParams{item}, nullptr);
    REQUIRE_EQ(1, result.size());

    CHECK_EQ("caller", result[0].from.name);
    CHECK_EQ(user, result[0].from.uri);
    REQUIRE_EQ(1, result[0].fromRanges.size());
    CHECK_EQ(lsp::Range{{4, 12}, {4, 23}}, result[0].fromRanges[0]);
}

// Use-after-free regression test: the item may have been prepared before a module it requires changed (the editor keeps
// the call hierarchy open), leaving its module with a type graph that references destroyed types
TEST_CASE_FIXTURE(Fixture, "cross_module_outgoing_calls_recheck_the_module_after_a_required_module_changed")
{
    // Enable pull-based diagnostics so that updating the document does not synchronously recheck its dependents
    client->capabilities.textDocument = lsp::TextDocumentClientCapabilities{};
    client->capabilities.textDocument->diagnostic = lsp::DiagnosticClientCapabilities{};

    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local function caller()
            useFunction()
        end
    )");

    auto item = prepareCallHierarchy(this, user, lsp::Position{3, 25});
    CHECK_EQ("caller", item.name);

    updateDocument(uri, R"(
        local function useFunction(): number
            return 1
        end

        return useFunction
    )");

    // Rechecks useFunction.luau
    prepareCallHierarchy(this, uri, lsp::Position{1, 28});

    auto result = workspace.callHierarchyOutgoingCalls(lsp::CallHierarchyOutgoingCallsParams{item}, nullptr);
    REQUIRE_EQ(1, result.size());

    CHECK_EQ("useFunction", result[0].to.name);
    CHECK_EQ(uri, result[0].to.uri);
    REQUIRE_EQ(1, result[0].fromRanges.size());
    CHECK_EQ(lsp::Range{{4, 12}, {4, 23}}, result[0].fromRanges[0]);
}

TEST_SUITE_END();
