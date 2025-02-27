// Copyright 2022, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Robin Textor-Falconi (textorr@informatik.uni-freiburg.de)

#include <gmock/gmock.h>

#include "../src/parser/data/VarOrTerm.h"

using namespace std::string_literals;
using ::testing::Optional;

ad_utility::AllocatorWithLimit<Id>& allocator() {
  static ad_utility::AllocatorWithLimit<Id> a{
      ad_utility::makeAllocationMemoryLeftThreadsafeObject(
          std::numeric_limits<size_t>::max())};
  return a;
}

struct ContextWrapper {
  Index _index{};
  ResultTable _resultTable{allocator()};
  ad_utility::HashMap<std::string, size_t> _hashMap{};

  Context createContextForRow(size_t row) const {
    return {row, _resultTable, _hashMap, _index};
  }
};

ContextWrapper prepareContext() { return {}; }

TEST(SparqlDataTypesTest, BlankNodeInvalidLabelsThrowException) {
  EXPECT_THROW(BlankNode(false, ""), ad_semsearch::Exception);
  EXPECT_THROW(BlankNode(false, "label with spaces"), ad_semsearch::Exception);
  EXPECT_THROW(BlankNode(false, "trailing-dash-"), ad_semsearch::Exception);
  EXPECT_THROW(BlankNode(false, "-leading-dash"), ad_semsearch::Exception);
  EXPECT_THROW(BlankNode(false, "trailing.dots."), ad_semsearch::Exception);
  EXPECT_THROW(BlankNode(false, ".leading.dots"), ad_semsearch::Exception);
}

TEST(SparqlDataTypesTest, BlankNodeEvaluatesCorrectlyBasedOnContext) {
  auto wrapper = prepareContext();

  BlankNode blankNodeA{false, "a"};
  BlankNode blankNodeB{true, "b"};
  Context context0 = wrapper.createContextForRow(0);

  EXPECT_THAT(blankNodeA.evaluate(context0, SUBJECT), Optional("_:u0_a"s));
  EXPECT_THAT(blankNodeA.evaluate(context0, PREDICATE), Optional("_:u0_a"s));
  EXPECT_THAT(blankNodeA.evaluate(context0, OBJECT), Optional("_:u0_a"s));
  EXPECT_THAT(blankNodeB.evaluate(context0, SUBJECT), Optional("_:g0_b"s));
  EXPECT_THAT(blankNodeB.evaluate(context0, PREDICATE), Optional("_:g0_b"s));
  EXPECT_THAT(blankNodeB.evaluate(context0, SUBJECT), Optional("_:g0_b"s));

  Context context10 = wrapper.createContextForRow(10);

  EXPECT_THAT(blankNodeA.evaluate(context10, SUBJECT), Optional("_:u10_a"s));
  EXPECT_THAT(blankNodeA.evaluate(context10, PREDICATE), Optional("_:u10_a"s));
  EXPECT_THAT(blankNodeA.evaluate(context10, OBJECT), Optional("_:u10_a"s));
  EXPECT_THAT(blankNodeB.evaluate(context10, SUBJECT), Optional("_:g10_b"s));
  EXPECT_THAT(blankNodeB.evaluate(context10, PREDICATE), Optional("_:g10_b"s));
  EXPECT_THAT(blankNodeB.evaluate(context10, SUBJECT), Optional("_:g10_b"s));
}

TEST(SparqlDataTypesTest, BlankNodeEvaluateIsPropagatedCorrectly) {
  auto wrapper = prepareContext();

  BlankNode blankNode{false, "label"};
  Context context = wrapper.createContextForRow(42);

  auto expectedLabel = Optional("_:u42_label"s);

  EXPECT_THAT(blankNode.evaluate(context, SUBJECT), expectedLabel);
  EXPECT_THAT(GraphTerm{blankNode}.evaluate(context, SUBJECT), expectedLabel);
  EXPECT_THAT(VarOrTerm{GraphTerm{blankNode}}.evaluate(context, SUBJECT),
              expectedLabel);
}

TEST(SparqlDataTypesTest, IriInvalidSyntaxThrowsException) {
  EXPECT_THROW(Iri{"http://linkwithoutangularbrackets"},
               ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<<nestedangularbrackets>>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<duplicatedangularbracker>>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<<duplicatedangularbracker>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<noend"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"nostart>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<\"withdoublequote>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<{withcurlybrace>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<}withcurlybrace>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<|withpipesymbol>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<^withcaret>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<\\withbackslash>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<`withbacktick>"}, ad_semsearch::Exception);
  // U+0000 (NULL) to U+0020 (Space) are all forbidden characters
  // but the following two are probably the most common cases
  EXPECT_THROW(Iri{"<with whitespace>"}, ad_semsearch::Exception);
  EXPECT_THROW(Iri{"<with\r\nnewline>"}, ad_semsearch::Exception);
}

TEST(SparqlDataTypesTest, IriValidIriIsPreserved) {
  ASSERT_EQ(Iri{"<http://valid-iri>"}.iri(), "<http://valid-iri>");
}

TEST(SparqlDataTypesTest, IriEvaluatesCorrectlyBasedOnContext) {
  auto wrapper = prepareContext();

  std::string iriString{"<http://some-iri>"};
  Iri iri{iriString};
  Context context0 = wrapper.createContextForRow(0);

  EXPECT_THAT(iri.evaluate(context0, SUBJECT), Optional(iriString));
  EXPECT_THAT(iri.evaluate(context0, PREDICATE), Optional(iriString));
  EXPECT_THAT(iri.evaluate(context0, OBJECT), Optional(iriString));

  Context context1337 = wrapper.createContextForRow(1337);

  EXPECT_THAT(iri.evaluate(context1337, SUBJECT), Optional(iriString));
  EXPECT_THAT(iri.evaluate(context1337, PREDICATE), Optional(iriString));
  EXPECT_THAT(iri.evaluate(context1337, OBJECT), Optional(iriString));
}

TEST(SparqlDataTypesTest, IriEvaluateIsPropagatedCorrectly) {
  auto wrapper = prepareContext();

  Iri iri{"<http://some-iri>"};
  Context context = wrapper.createContextForRow(42);

  auto expectedString = Optional("<http://some-iri>"s);

  EXPECT_THAT(iri.evaluate(context, SUBJECT), expectedString);
  EXPECT_THAT(GraphTerm{iri}.evaluate(context, SUBJECT), expectedString);
  EXPECT_THAT(VarOrTerm{GraphTerm{iri}}.evaluate(context, SUBJECT),
              expectedString);
}

TEST(SparqlDataTypesTest, LiteralBooleanIsCorrectlyFormatted) {
  EXPECT_EQ(Literal{true}.literal(), "true");
  EXPECT_EQ(Literal{false}.literal(), "false");
}

TEST(SparqlDataTypesTest, LiteralStringIsCorrectlyFormatted) {
  EXPECT_EQ(Literal{"abcdef"}.literal(), "abcdef");
  EXPECT_EQ(Literal{"\U0001f937\U0001f3fc\u200d\u2642\ufe0f"}.literal(),
            "🤷🏼‍♂️");
}

TEST(SparqlDataTypesTest, LiteralNumberIsCorrectlyFormatted) {
  EXPECT_EQ(Literal{1234567890}.literal(), "1234567890");
  EXPECT_EQ(Literal{-1337}.literal(), "-1337");
  EXPECT_EQ(Literal{1.3}.literal(), "1.3");
}

TEST(SparqlDataTypesTest, LiteralEvaluatesCorrectlyBasedOnContext) {
  auto wrapper = prepareContext();

  std::string literalString{"true"};
  Literal literal{literalString};
  Context context0 = wrapper.createContextForRow(0);

  EXPECT_EQ(literal.evaluate(context0, SUBJECT), std::nullopt);
  EXPECT_EQ(literal.evaluate(context0, PREDICATE), std::nullopt);
  EXPECT_THAT(literal.evaluate(context0, OBJECT), Optional(literalString));

  Context context1337 = wrapper.createContextForRow(1337);

  EXPECT_EQ(literal.evaluate(context1337, SUBJECT), std::nullopt);
  EXPECT_EQ(literal.evaluate(context1337, PREDICATE), std::nullopt);
  EXPECT_THAT(literal.evaluate(context1337, OBJECT), Optional(literalString));
}

TEST(SparqlDataTypesTest, LiteralEvaluateIsPropagatedCorrectly) {
  auto wrapper = prepareContext();

  Literal literal{"some literal"};
  Context context = wrapper.createContextForRow(42);

  EXPECT_EQ(literal.evaluate(context, SUBJECT), std::nullopt);
  EXPECT_EQ(GraphTerm{literal}.evaluate(context, SUBJECT), std::nullopt);
  EXPECT_EQ(VarOrTerm{GraphTerm{literal}}.evaluate(context, SUBJECT),
            std::nullopt);

  auto expectedString = Optional("some literal"s);

  EXPECT_THAT(literal.evaluate(context, OBJECT), expectedString);
  EXPECT_THAT(GraphTerm{literal}.evaluate(context, OBJECT), expectedString);
  EXPECT_THAT(VarOrTerm{GraphTerm{literal}}.evaluate(context, OBJECT),
              expectedString);
}

TEST(SparqlDataTypesTest, VariableNormalizesDollarSign) {
  Variable varWithQuestionMark{"?abc"};
  EXPECT_EQ(varWithQuestionMark.name(), "?abc");

  Variable varWithDollarSign{"$abc"};
  EXPECT_EQ(varWithQuestionMark.name(), "?abc");
}

TEST(SparqlDataTypesTest, VariableInvalidNamesThrowException) {
  EXPECT_THROW(Variable{"no_leading_var_or_dollar"}, ad_semsearch::Exception);
  EXPECT_THROW(Variable{""}, ad_semsearch::Exception);
  EXPECT_THROW(Variable{"? var with space"}, ad_semsearch::Exception);
  EXPECT_THROW(Variable{"?"}, ad_semsearch::Exception);
  EXPECT_THROW(Variable{"$"}, ad_semsearch::Exception);
}

TEST(SparqlDataTypesTest, VariableEvaluatesCorrectlyBasedOnContext) {
  auto wrapper = prepareContext();

  wrapper._hashMap["?var"] = 0;
  wrapper._resultTable._resultTypes.push_back(qlever::ResultType::VERBATIM);
  wrapper._resultTable._idTable.setCols(1);
  Id value1 = 69;
  Id value2 = 420;
  wrapper._resultTable._idTable.push_back({value1});
  wrapper._resultTable._idTable.push_back({value2});

  Variable variable{"?var"};
  Context context0 = wrapper.createContextForRow(0);

  EXPECT_THAT(variable.evaluate(context0, SUBJECT), Optional("69"s));
  EXPECT_THAT(variable.evaluate(context0, PREDICATE), Optional("69"s));
  EXPECT_THAT(variable.evaluate(context0, OBJECT), Optional("69"s));

  Context context1 = wrapper.createContextForRow(1);

  EXPECT_THAT(variable.evaluate(context1, SUBJECT), Optional("420"s));
  EXPECT_THAT(variable.evaluate(context1, PREDICATE), Optional("420"s));
  EXPECT_THAT(variable.evaluate(context1, OBJECT), Optional("420"s));
}

TEST(SparqlDataTypesTest, VariableEvaluatesNothingForUnusedName) {
  auto wrapper = prepareContext();

  Variable variable{"?var"};
  Context context0 = wrapper.createContextForRow(0);

  EXPECT_EQ(variable.evaluate(context0, SUBJECT), std::nullopt);
  EXPECT_EQ(variable.evaluate(context0, PREDICATE), std::nullopt);
  EXPECT_EQ(variable.evaluate(context0, OBJECT), std::nullopt);

  Context context1337 = wrapper.createContextForRow(1337);

  EXPECT_EQ(variable.evaluate(context1337, SUBJECT), std::nullopt);
  EXPECT_EQ(variable.evaluate(context1337, PREDICATE), std::nullopt);
  EXPECT_EQ(variable.evaluate(context1337, OBJECT), std::nullopt);
}

TEST(SparqlDataTypesTest, VariableEvaluateIsPropagatedCorrectly) {
  auto wrapper = prepareContext();

  wrapper._hashMap["?var"] = 0;
  wrapper._resultTable._resultTypes.push_back(qlever::ResultType::VERBATIM);
  wrapper._resultTable._idTable.setCols(1);
  Id value = 69;
  wrapper._resultTable._idTable.push_back({value});

  Variable variableKnown{"?var"};
  Context context = wrapper.createContextForRow(0);

  EXPECT_THAT(variableKnown.evaluate(context, SUBJECT), Optional("69"s));
  EXPECT_THAT(VarOrTerm{variableKnown}.evaluate(context, SUBJECT),
              Optional("69"s));

  Variable variableUnknown{"?unknownVar"};

  EXPECT_EQ(variableUnknown.evaluate(context, SUBJECT), std::nullopt);
  EXPECT_EQ(VarOrTerm{variableUnknown}.evaluate(context, SUBJECT),
            std::nullopt);
}
