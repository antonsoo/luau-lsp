#include "doctest.h"
#include "Fixture.h"

TEST_SUITE_BEGIN("Rename");

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_is_empty")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr), "The new name must be a valid identifier", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_does_not_start_as_valid_identifier")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "1234";

    REQUIRE_THROWS_WITH_AS(
        workspace.rename(params, nullptr), "The new name must be a valid identifier starting with a character or underscore", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "fail_if_new_name_is_not_a_valid_identifier")
{
    auto uri = newDocument("foo.luau", "");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{0, 0};
    params.newName = "testing123!";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr),
        "The new name must be a valid identifier composed of characters, digits, and underscores only", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter")
{
    // https://github.com/JohnnyMorganz/luau-lsp/issues/488
    // Renaming: "S" inside of <S>

    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 26};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK(applyEdit(source, documentEdits) == R"(
        type LayerBuilder<State> = State & { ... }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_used_inside_of_type_alias")
{
    // https://github.com/JohnnyMorganz/luau-lsp/issues/488
    // Renaming: "S" inside of assignment `S & { ... }`

    auto source = R"(
        type LayerBuilder<S> = S & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 31};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type LayerBuilder<State> = State & { ... }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_used_as_another_generic")
{
    auto source = R"(
        type Baz<S...> = Bar<S...>
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 17};
    params.newName = "State";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Baz<State...> = Bar<State...>
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_parameter_in_function_definition")
{
    auto source = R"(
        function foo<T>(x: T, y: string)
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 21};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        function foo<Value>(x: Value, y: string)
        end
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_1")
{
    // Renaming "T" in Foo<T>
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 15};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<Value> = {
            x: Value,
            fn: <T>(T, T) -> T
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_2")
{
    // Renaming first "T" in <T>(T, T) -> T
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 17};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<T> = {
            x: T,
            fn: <Value>(Value, Value) -> Value
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_generic_type_correctly_handle_shadowing_3")
{
    // Renaming second "T" in <T>(T, T) -> T
    auto source = R"(
        type Foo<T> = {
            x: T,
            fn: <T>(T, T) -> T
        }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 20};
    params.newName = "Value";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Foo<T> = {
            x: T,
            fn: <Value>(Value, Value) -> Value
        }
    )");
}

TEST_CASE_FIXTURE(Fixture, "renaming_required_variable_should_also_rename_imported_type_prefixes")
{
    auto source = R"(
        local Types = require("path/to/types")

        type Foo = Types.Foo
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14};
    params.newName = "ActualTypes";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        local ActualTypes = require("path/to/types")

        type Foo = ActualTypes.Foo
    )");
}

TEST_CASE_FIXTURE(Fixture, "renaming_required_variable_should_not_rename_type_prefixes_of_shadowing_local")
{
    auto source = R"(
        local Types = require("path/to/types")

        do
            local Types = require("path/to/other_types")
            type Foo = Types.Foo
        end

        type Bar = Types.Foo
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 14};
    params.newName = "ActualTypes";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        local ActualTypes = require("path/to/types")

        do
            local Types = require("path/to/other_types")
            type Foo = Types.Foo
        end

        type Bar = ActualTypes.Foo
    )");
}

TEST_CASE_FIXTURE(Fixture, "renaming_local_from_within_generic_type_reference_prefix_should_rename_declaration")
{
    // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1203
    auto source = R"(
        local jecs = require("path/to/jecs")

        type Entity = jecs.Entity<Player>
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{3, 22}; // cursor on "jecs" prefix inside the type reference
    params.newName = "ecs";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        local ecs = require("path/to/jecs")

        type Entity = ecs.Entity<Player>
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_global_function_name")
{
    auto source = R"(
        function Main()
        end

        Main()
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{4, 11};
    params.newName = "Test";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        function Test()
        end

        Test()
    )");
}

TEST_CASE_FIXTURE(Fixture, "disallow_renaming_of_global_from_type_definition")
{
    auto source = R"(
        local x = game:GetService("Foo")
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 19}; // 'game'
    params.newName = "game2";

    REQUIRE_THROWS_WITH_AS(workspace.rename(params, nullptr), "Cannot rename a global variable", JsonRpcException);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_local_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        local function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 27}; // 'useFunction' definition
    params.newName = "useFunction2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_global_function")
{
    auto uri = newDocument("useFunction.luau", R"(
        function useFunction()
        end

        return useFunction
    )");

    auto user = newDocument("user.luau", R"(
        local useFunction = require("useFunction.luau")

        local value = useFunction()
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 20}; // 'useFunction' definition
    params.newName = "useFunction2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "dont_rename_cross_module_usages_of_a_returned_table")
{
    auto uri = newDocument("tbl.luau", R"(
        local tbl = {}

        return tbl
    )");

    auto user = newDocument("user.luau", R"(
        local tbl = require("tbl.luau")

        local value = tbl
    )");

    workspace.frontend.parse(workspace.fileResolver.getModuleName(user));

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 15}; // 'tbl' definition
    params.newName = "tbl2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);
    CHECK_EQ(result->changes.begin()->first, uri);
    CHECK_EQ(result->changes.begin()->second.size(), 2);
}

TEST_CASE_FIXTURE(Fixture, "response_json_is_valid_structure")
{
    auto uri = newDocument("tbl.luau", R"(
        local value = 1
    )");

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{1, 15}; // 'value' definition
    params.newName = "value2";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->changes.size(), 1);

    json response = result;
    CHECK_EQ(response.dump(), R"({"changes":{")" + uri.toString() +
                                  R"(":[{"newText":"value2","range":{"end":{"character":19,"line":1},"start":{"character":14,"line":1}}}]}})");
}

TEST_CASE_FIXTURE(Fixture, "rename_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.rename(lsp::RenameParams{{{document}, lsp::Position{}}, "y"}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "rename_property_from_bracket_notation")
{
    auto source = R"(
        type Tbl = {
            name: string
        }

        local x: Tbl
        local v = x["name"]
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{6, 22}; // cursor on 'name' inside brackets
    params.newName = "title";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Tbl = {
            title: string
        }

        local x: Tbl
        local v = x["title"]
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_property_affects_both_dot_and_bracket_notation")
{
    auto source = R"(
        type Tbl = {
            name: string
        }

        local x: Tbl
        local v1 = x.name
        local v2 = x["name"]
    )";

    auto uri = newDocument("foo.luau", source);

    // Rename from dot notation should also update bracket notation
    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{6, 22}; // cursor on 'name' in x.name
    params.newName = "title";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        type Tbl = {
            title: string
        }

        local x: Tbl
        local v1 = x.title
        local v2 = x["title"]
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_property_from_bracket_notation_definition_in_table_literal")
{
    // When table is defined using bracket notation keys: {["key"] = value}
    auto source = R"(
        local T = {
            ["name"] = "string"
        }

        local v1 = T.name
        local v2 = T["name"]
    )";

    auto uri = newDocument("foo.luau", source);

    // Rename from the bracket notation definition in the table literal
    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{2, 15}; // cursor on 'name' inside ["name"] definition
    params.newName = "title";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
        local T = {
            ["title"] = "string"
        }

        local v1 = T.title
        local v2 = T["title"]
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_method_through_metatable_inheritance")
{
    auto source = R"(
local Foo = {}
Foo.__index = Foo
export type Foo = typeof(setmetatable({}, Foo))

function Foo.Test(self: Foo, a)
    print(a)
end

local Bar = setmetatable({}, Foo)
Bar.__index = Bar
export type Bar = typeof(setmetatable({}, Foo))

function Bar.new()
    local self = setmetatable({}, Bar)
    return self
end

function Bar.DoSomething(self: Bar)
    self:Test("Hello, World!")
end

return Bar
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{5, 13}; // cursor on 'Test' in function Foo.Test
    params.newName = "Run";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
local Foo = {}
Foo.__index = Foo
export type Foo = typeof(setmetatable({}, Foo))

function Foo.Run(self: Foo, a)
    print(a)
end

local Bar = setmetatable({}, Foo)
Bar.__index = Bar
export type Bar = typeof(setmetatable({}, Foo))

function Bar.new()
    local self = setmetatable({}, Bar)
    return self
end

function Bar.DoSomething(self: Bar)
    self:Run("Hello, World!")
end

return Bar
    )");
}

TEST_CASE_FIXTURE(Fixture, "rename_method_from_metatable_call_site")
{
    auto source = R"(
local Foo = {}
Foo.__index = Foo
export type Foo = typeof(setmetatable({}, Foo))

function Foo.Test(self: Foo, a)
    print(a)
end

local Bar = setmetatable({}, Foo)
Bar.__index = Bar
export type Bar = typeof(setmetatable({}, Foo))

function Bar.new()
    local self = setmetatable({}, Bar)
    return self
end

function Bar.DoSomething(self: Bar)
    self:Test("Hello, World!")
end

return Bar
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = lsp::Position{19, 9}; // cursor on 'Test' in self:Test()
    params.newName = "Run";

    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);

    auto documentEdits = result->changes.begin()->second;
    CHECK_EQ(applyEdit(source, documentEdits), R"(
local Foo = {}
Foo.__index = Foo
export type Foo = typeof(setmetatable({}, Foo))

function Foo.Run(self: Foo, a)
    print(a)
end

local Bar = setmetatable({}, Foo)
Bar.__index = Bar
export type Bar = typeof(setmetatable({}, Foo))

function Bar.new()
    local self = setmetatable({}, Bar)
    return self
end

function Bar.DoSomething(self: Bar)
    self:Run("Hello, World!")
end

return Bar
    )");
}



static void checkScopedTypeRename(Fixture& fixture, std::string markedSource, const std::string& expected)
{
    auto [source, position] = sourceWithMarker(std::move(markedSource));
    auto uri = fixture.newDocument("scoped.luau", source);
    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = position;
    params.newName = "Renamed";
    auto result = fixture.workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 1);
    CHECK_EQ(applyEdit(source, result->changes.at(uri)), expected);
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_captured_generic")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T): T
    local function inner(y: T): T| return y end
    return inner(x)
end)",
        R"(local function outer<Renamed>(x: Renamed): Renamed
    local function inner(y: Renamed): Renamed return y end
    return inner(x)
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_captured_generic_alias")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T)
    type Value = T|
    local value: Value = x
    return value
end)",
        R"(local function outer<Renamed>(x: Renamed)
    type Value = Renamed
    local value: Value = x
    return value
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_function_shadow")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T): T|
    local function inner<T>(y: T): T return y end
    return x
end)",
        R"(local function outer<Renamed>(x: Renamed): Renamed
    local function inner<T>(y: T): T return y end
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_alias_generic_shadow")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T): T|
    type Inner<T> = {value: T}
    return x
end)",
        R"(local function outer<Renamed>(x: Renamed): Renamed
    type Inner<T> = {value: T}
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_function_type_shadow")
{
    checkScopedTypeRename(*this, R"(type Outer<T> = {
    value: T,
    callback: <T>(T) -> T,
    capture: <U>(U, T) -> T|
})",
        R"(type Outer<Renamed> = {
    value: Renamed,
    callback: <T>(T) -> T,
    capture: <U>(U, Renamed) -> Renamed
})");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_inner_generic")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T): T
    local function inner<T>(y: T): T| return y end
    return x
end)",
        R"(local function outer<T>(x: T): T
    local function inner<Renamed>(y: Renamed): Renamed return y end
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_block_alias_inner")
{
    checkScopedTypeRename(*this, R"(type Value = number
do
    type Value = string
    local x: Value| = 'x'
end
local y: Value = 1)",
        R"(type Value = number
do
    type Renamed = string
    local x: Renamed = 'x'
end
local y: Value = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_block_alias_outer")
{
    checkScopedTypeRename(*this, R"(type Value = number
do
    type Value = string
    local x: Value = 'x'
end
local y: Value| = 1)",
        R"(type Renamed = number
do
    type Value = string
    local x: Value = 'x'
end
local y: Renamed = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_same_underlying_type")
{
    checkScopedTypeRename(*this, R"(type Value = number
do
    type Value = number
    local inner: Value = 1
end
local outer: Value| = 1)",
        R"(type Renamed = number
do
    type Value = number
    local inner: Value = 1
end
local outer: Renamed = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_alias_hoisting")
{
    checkScopedTypeRename(*this, R"(type Value = number
do
    local before: Value = 'x'
    type Value = string
    local after: Value| = 'y'
end
local outer: Value = 1)",
        R"(type Value = number
do
    local before: Renamed = 'x'
    type Renamed = string
    local after: Renamed = 'y'
end
local outer: Value = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_generic_shadows_alias")
{
    checkScopedTypeRename(*this, R"(type T = number
local function identity<T>(x: T): T| return x end
local y: T = 1)",
        R"(type T = number
local function identity<Renamed>(x: Renamed): Renamed return x end
local y: T = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_alias_excludes_generic")
{
    checkScopedTypeRename(*this, R"(type T = number
local function identity<T>(x: T): T return x end
local y: T| = 1)",
        R"(type Renamed = number
local function identity<T>(x: T): T return x end
local y: Renamed = 1)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_alias_shadows_generic")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T): T|
    do
        type T = string
        local inner: T = 'x'
    end
    return x
end)",
        R"(local function outer<Renamed>(x: Renamed): Renamed
    do
        type T = string
        local inner: T = 'x'
    end
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_nested_pack")
{
    checkScopedTypeRename(*this, R"(local function outer<T...>(...: T...): T...
    local function inner(...: T...): T|... return ... end
    return inner(...)
end)",
        R"(local function outer<Renamed...>(...: Renamed...): Renamed...
    local function inner(...: Renamed...): Renamed... return ... end
    return inner(...)
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_pack_shadow")
{
    checkScopedTypeRename(*this, R"(local function outer<T...>(...: T...): T|...
    local function inner<T...>(...: T...): T... return ... end
    type F<T...> = (T...) -> T...
    return ...
end)",
        R"(local function outer<Renamed...>(...: Renamed...): Renamed...
    local function inner<T...>(...: T...): T... return ... end
    type F<T...> = (T...) -> T...
    return ...
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_alias_pack")
{
    checkScopedTypeRename(*this, R"(type Function<T...> = (T...) -> T|...)", R"(type Function<Renamed...> = (Renamed...) -> Renamed...)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_generic_default")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T)
    type Inner<U = T> = {outer: T|, inner: U}
    return x
end)",
        R"(local function outer<Renamed>(x: Renamed)
    type Inner<U = Renamed> = {outer: Renamed, inner: U}
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_shadowing_default")
{
    checkScopedTypeRename(*this, R"(local function outer<T>(x: T)
    type Inner<T = T|> = {inner: T}
    return x
end)",
        R"(local function outer<Renamed>(x: Renamed)
    type Inner<T = Renamed> = {inner: T}
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_repeat_scope")
{
    checkScopedTypeRename(*this, R"(type Value = string
repeat
    type Value = number
    local x: Value = 1
until (1 :: Value|) == 1
local y: Value = 'x')",
        R"(type Value = string
repeat
    type Renamed = number
    local x: Renamed = 1
until (1 :: Renamed) == 1
local y: Value = 'x')");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_qualified_type")
{
    newDocument("Types.luau", "export type T = number\nreturn {}\n");
    checkScopedTypeRename(*this, R"(local Types = require('./Types')
local function outer<T>(x: T, y: Types.T): T|
    return x
end)",
        R"(local Types = require('./Types')
local function outer<Renamed>(x: Renamed, y: Types.T): Renamed
    return x
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_exported_alias_preserves_local_shadows")
{
    auto [source, position] = sourceWithMarker(R"(export type Value| = number
do
    type Value = string
    local inner: Value = 'x'
end
local outer: Value = 1
return {})");
    auto types = newDocument("types.luau", source);
    registerDocumentForVirtualPath(types, "game/Testing/Types");
    const std::string consumerSource = "local Types = require(game.Testing.Types)\nlocal x: Types.Value = 1";
    auto consumer = newDocument("consumer.luau", consumerSource);
    workspace.checkStrict(workspace.fileResolver.getModuleName(consumer), nullptr);

    lsp::RenameParams params;
    params.textDocument = lsp::TextDocumentIdentifier{types};
    params.position = position;
    params.newName = "Renamed";
    auto result = workspace.rename(params, nullptr);
    REQUIRE(result);
    REQUIRE(result->changes.size() == 2);
    CHECK_EQ(applyEdit(source, result->changes.at(types)), R"(export type Renamed = number
do
    type Value = string
    local inner: Value = 'x'
end
local outer: Renamed = 1
return {})");
    CHECK_EQ(applyEdit(consumerSource, result->changes.at(consumer)), "local Types = require(game.Testing.Types)\nlocal x: Types.Renamed = 1");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_pack_default")
{
    checkScopedTypeRename(*this, R"(local function outer<T...>(...: T...)
    type Inner<U... = (|T...)> = (U...) -> T...
    return ...
end)",
        R"(local function outer<Renamed...>(...: Renamed...)
    type Inner<U... = (Renamed...)> = (U...) -> Renamed...
    return ...
end)");
}

TEST_CASE_FIXTURE(Fixture, "rename_scoped_pack_shadowing_default")
{
    checkScopedTypeRename(*this, R"(local function outer<T...>(...: T...)
    type Inner<T... = (T|...)> = (T...) -> T...
    return ...
end)",
        R"(local function outer<Renamed...>(...: Renamed...)
    type Inner<T... = (Renamed...)> = (T...) -> T...
    return ...
end)");
}

TEST_SUITE_END();
