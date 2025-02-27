//
// Created by johannes on 08.05.21.
//

#include <antlr4-runtime.h>
#include <gtest/gtest.h>

#include <iostream>
#include <type_traits>

#include "../../src/parser/sparqlParser/SparqlQleverVisitor.h"
#include "../src/parser/data/Types.h"
#include "../src/parser/sparqlParser/generated/SparqlAutomaticLexer.h"
#include "../src/util/antlr/ThrowingErrorStrategy.h"
#include "SparqlAntlrParserTestHelpers.h"

using namespace antlr4;

struct ParserAndVisitor {
 private:
  string input;
  ANTLRInputStream stream{input};
  SparqlAutomaticLexer lexer{&stream};
  CommonTokenStream tokens{&lexer};

 public:
  SparqlAutomaticParser parser{&tokens};
  SparqlQleverVisitor visitor;
  explicit ParserAndVisitor(string toParse) : input{std::move(toParse)} {
    parser.setErrorHandler(std::make_shared<ThrowingErrorStrategy>());
  }
};

template <typename T>
void testNumericLiteral(const std::string& input, T target) {
  ParserAndVisitor p(input);
  auto literalContext = p.parser.numericLiteral();
  auto result = p.visitor.visitNumericLiteral(literalContext).as<T>();

  if constexpr (std::is_floating_point_v<T>) {
    ASSERT_FLOAT_EQ(target, result);
  } else {
    ASSERT_EQ(target, result);
  }
}

auto nil = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#nil>";
auto first = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#first>";
auto rest = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#rest>";
auto type = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(SparqlParser, NumericLiterals) {
  testNumericLiteral("3.0", 3.0);
  testNumericLiteral("3.0e2", 300.0);
  testNumericLiteral("3.0e-2", 0.030);
  testNumericLiteral("3", 3ull);
  testNumericLiteral("-3.0", -3.0);
  testNumericLiteral("-3", -3ll);

  // TODO<joka921> : Unit tests with random numbers
}

TEST(SparqlParser, Prefix) {
  {
    string s = "PREFIX wd: <www.wikidata.org/>";
    ParserAndVisitor p{s};
    auto context = p.parser.prefixDecl();
    p.visitor.visitPrefixDecl(context);
    const auto& m = p.visitor.prefixMap();
    ASSERT_EQ(2ul, m.size());
    ASSERT_TRUE(m.at("wd") == "<www.wikidata.org/>");
    ASSERT_EQ(m.at(""), "<>");
  }
  {
    string s = "wd:bimbam";
    ParserAndVisitor p{s};
    auto& m = p.visitor.prefixMap();
    m["wd"] = "<www.wikidata.org/>";

    auto context = p.parser.pnameLn();
    auto result = p.visitor.visitPnameLn(context).as<string>();
    ASSERT_EQ(result, "<www.wikidata.org/bimbam>");
  }
  {
    string s = "wd:";
    ParserAndVisitor p{s};
    auto& m = p.visitor.prefixMap();
    m["wd"] = "<www.wikidata.org/>";

    auto context = p.parser.pnameNs();
    auto result = p.visitor.visitPnameNs(context).as<string>();
    ASSERT_EQ(result, "<www.wikidata.org/>");
  }
  {
    string s = "wd:bimbam";
    ParserAndVisitor p{s};
    auto& m = p.visitor.prefixMap();
    m["wd"] = "<www.wikidata.org/>";

    auto context = p.parser.prefixedName();
    auto result = p.visitor.visitPrefixedName(context).as<string>();
    ASSERT_EQ(result, "<www.wikidata.org/bimbam>");
  }
  {
    string s = "<somethingsomething> <rest>";
    ParserAndVisitor p{s};
    auto& m = p.visitor.prefixMap();
    m["wd"] = "<www.wikidata.org/>";

    auto context = p.parser.iriref();
    auto result = p.visitor.visitIriref(context).as<string>();
    auto sz = context->getText().size();

    ASSERT_EQ(result, "<somethingsomething>");
    ASSERT_EQ(sz, 20u);
  }
}

TEST(SparqlExpressionParser, First) {
  string s = "(5 * 5 ) bimbam";
  ParserAndVisitor p{s};
  auto context = p.parser.expression();
  // This is an example on how to access a certain parsed substring.
  /*
  LOG(INFO) << context->getText() << std::endl;
  LOG(INFO) << p.parser.getTokenStream()
                   ->getTokenSource()
                   ->getInputStream()
                   ->toString()
            << std::endl;
  LOG(INFO) << p.parser.getCurrentToken()->getStartIndex() << std::endl;
   */
  auto resultAsAny = p.visitor.visitExpression(context);
  auto resultAsExpression =
      std::move(resultAsAny.as<sparqlExpression::SparqlExpression::Ptr>());

  QueryExecutionContext* qec = nullptr;
  sparqlExpression::VariableToColumnAndResultTypeMap map;
  ad_utility::AllocatorWithLimit<Id> alloc{
      ad_utility::makeAllocationMemoryLeftThreadsafeObject(1000)};
  IdTable table{alloc};
  ResultTable::LocalVocab localVocab;
  sparqlExpression::EvaluationContext input{*qec, map, table, alloc,
                                            localVocab};
  auto result = resultAsExpression->evaluate(&input);
  AD_CHECK(std::holds_alternative<double>(result));
  ASSERT_FLOAT_EQ(25.0, std::get<double>(result));
}

TEST(SparqlParser, ComplexConstructQuery) {
  string input =
      "CONSTRUCT { [?a ( ?b (?c) )] ?d [?e [?f ?g]] . "
      "<http://wallscope.co.uk/resource/olympics/medal/#something> a "
      "<http://wallscope.co.uk/resource/olympics/medal/#somethingelse> } "
      "WHERE {}";
  ParserAndVisitor p{input};

  auto triples = p.parser.constructQuery()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(11));
  auto something =
      "<http://wallscope.co.uk/resource/olympics/medal/#something>";
  auto somethingElse =
      "<http://wallscope.co.uk/resource/olympics/medal/#somethingelse>";

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsVariable("?a"),        //
                                      IsBlankNode(true, "3")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "1"),  //
                                      IsIri(first),            //
                                      IsBlankNode(true, "2")));

  EXPECT_THAT(triples[2], ElementsAre(IsBlankNode(true, "1"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));

  EXPECT_THAT(triples[3], ElementsAre(IsBlankNode(true, "2"),  //
                                      IsIri(first),            //
                                      IsVariable("?c")));

  EXPECT_THAT(triples[4], ElementsAre(IsBlankNode(true, "2"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));

  EXPECT_THAT(triples[5], ElementsAre(IsBlankNode(true, "3"),  //
                                      IsIri(first),            //
                                      IsVariable("?b")));

  EXPECT_THAT(triples[6], ElementsAre(IsBlankNode(true, "3"),  //
                                      IsIri(rest),             //
                                      IsBlankNode(true, "1")));

  EXPECT_THAT(triples[7], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsVariable("?d"),        //
                                      IsBlankNode(true, "4")));

  EXPECT_THAT(triples[8], ElementsAre(IsBlankNode(true, "4"),  //
                                      IsVariable("?e"),        //
                                      IsBlankNode(true, "5")));

  EXPECT_THAT(triples[9], ElementsAre(IsBlankNode(true, "5"),  //
                                      IsVariable("?f"),        //
                                      IsVariable("?g")));

  EXPECT_THAT(triples[10], ElementsAre(IsIri(something),  //
                                       IsIri(type),       //
                                       IsIri(somethingElse)));
}

TEST(SparqlParser, GraphTermNumericLiteral) {
  string input = "1337";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsLiteral("1337"));
}

TEST(SparqlParser, GraphTermBooleanLiteral) {
  string input = "true";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsLiteral(input));
}

TEST(SparqlParser, GraphTermBlankNode) {
  string input = "[]";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsBlankNode(true, "0"));
}

TEST(SparqlParser, GraphTermIri) {
  string input = "<http://dummy-iri.com#fragment>";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsIri(input));
}

TEST(SparqlParser, GraphTermRdfLiteral) {
  string input = "\"abc\"";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsLiteral(input));
}

TEST(SparqlParser, GraphTermRdfNil) {
  string input = "()";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.graphTerm()->accept(&p.visitor).as<GraphTerm>();
  EXPECT_THAT(graphTerm, IsIri(nil));
}

TEST(SparqlParser, RdfCollectionSingleVar) {
  string input = "( ?a )";
  ParserAndVisitor p{input};

  const auto [node, triples] = p.parser.collection()
                                   ->accept(&p.visitor)
                                   .as<ad_utility::sparql_types::Node>();

  EXPECT_THAT(node, IsBlankNode(true, "0"));

  ASSERT_THAT(triples, SizeIs(2));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(first),            //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));
}

TEST(SparqlParser, RdfCollectionTripleVar) {
  string input = "( ?a ?b ?c )";
  ParserAndVisitor p{input};

  const auto [node, triples] = p.parser.collection()
                                   ->accept(&p.visitor)
                                   .as<ad_utility::sparql_types::Node>();

  EXPECT_THAT(node, IsBlankNode(true, "2"));

  ASSERT_THAT(triples, SizeIs(6));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(first),            //
                                      IsVariable("?c")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));

  EXPECT_THAT(triples[2], ElementsAre(IsBlankNode(true, "1"),  //
                                      IsIri(first),            //
                                      IsVariable("?b")));

  EXPECT_THAT(triples[3], ElementsAre(IsBlankNode(true, "1"),  //
                                      IsIri(rest),             //
                                      IsBlankNode(true, "0")));

  EXPECT_THAT(triples[4], ElementsAre(IsBlankNode(true, "2"),  //
                                      IsIri(first),            //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[5], ElementsAre(IsBlankNode(true, "2"),  //
                                      IsIri(rest),             //
                                      IsBlankNode(true, "1")));
}

TEST(SparqlParser, BlankNodeAnonymous) {
  string input = "[ \t\r\n]";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.blankNode()->accept(&p.visitor).as<BlankNode>();
  EXPECT_THAT(graphTerm, IsBlankNode(true, "0"));
}

TEST(SparqlParser, BlankNodeLabelled) {
  string input = "_:label123";
  ParserAndVisitor p{input};

  auto graphTerm = p.parser.blankNode()->accept(&p.visitor).as<BlankNode>();
  EXPECT_THAT(graphTerm, IsBlankNode(false, "label123"));
}

TEST(SparqlParser, ConstructTemplateEmpty) {
  string input = "{}";
  ParserAndVisitor p{input};

  auto triples = p.parser.constructTemplate()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, IsEmpty());
}

TEST(SparqlParser, ConstructTriplesSingletonWithTerminator) {
  string input = "?a ?b ?c .";
  ParserAndVisitor p{input};

  auto triples = p.parser.constructTriples()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(1));

  EXPECT_THAT(triples[0], ElementsAre(IsVariable("?a"),  //
                                      IsVariable("?b"),  //
                                      IsVariable("?c")));
}

TEST(SparqlParser, ConstructTriplesWithTerminator) {
  string input = "?a ?b ?c . ?d ?e ?f . ?g ?h ?i .";
  ParserAndVisitor p{input};

  auto triples = p.parser.constructTriples()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(3));

  EXPECT_THAT(triples[0], ElementsAre(IsVariable("?a"),  //
                                      IsVariable("?b"),  //
                                      IsVariable("?c")));

  EXPECT_THAT(triples[1], ElementsAre(IsVariable("?d"),  //
                                      IsVariable("?e"),  //
                                      IsVariable("?f")));

  EXPECT_THAT(triples[2], ElementsAre(IsVariable("?g"),  //
                                      IsVariable("?h"),  //
                                      IsVariable("?i")));
}

TEST(SparqlParser, TriplesSameSubjectVarOrTerm) {
  string input = "?a ?b ?c";
  ParserAndVisitor p{input};

  auto triples = p.parser.constructTriples()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(1));

  EXPECT_THAT(triples[0], ElementsAre(IsVariable("?a"),  //
                                      IsVariable("?b"),  //
                                      IsVariable("?c")));
}

TEST(SparqlParser, TriplesSameSubjectTriplesNodeWithPropertyList) {
  string input = "(?a) ?b ?c";
  ParserAndVisitor p{input};

  auto triples = p.parser.triplesSameSubject()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(3));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(first),            //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));

  EXPECT_THAT(triples[2], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsVariable("?b"),        //
                                      IsVariable("?c")));
}

TEST(SparqlParser, TriplesSameSubjectTriplesNodeEmptyPropertyList) {
  string input = "(?a)";
  ParserAndVisitor p{input};

  auto triples = p.parser.triplesSameSubject()
                     ->accept(&p.visitor)
                     .as<ad_utility::sparql_types::Triples>();
  ASSERT_THAT(triples, SizeIs(2));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(first),            //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));
}

TEST(SparqlParser, PropertyList) {
  string input = "a ?a";
  ParserAndVisitor p{input};

  const auto [tuples, triples] =
      p.parser.propertyList()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::PropertyList>();

  EXPECT_THAT(triples, IsEmpty());

  ASSERT_THAT(tuples, SizeIs(1));

  EXPECT_THAT(tuples[0], ElementsAre(IsIri(type), IsVariable("?a")));
}

TEST(SparqlParser, EmptyPropertyList) {
  ParserAndVisitor p{""};

  const auto [tuples, triples] =
      p.parser.propertyList()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::PropertyList>();
  ASSERT_THAT(tuples, IsEmpty());
  ASSERT_THAT(triples, IsEmpty());
}

TEST(SparqlParser, PropertyListNotEmptySingletonWithTerminator) {
  string input = "a ?a ;";
  ParserAndVisitor p{input};

  const auto [tuples, triples] =
      p.parser.propertyListNotEmpty()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::PropertyList>();
  EXPECT_THAT(triples, IsEmpty());

  ASSERT_THAT(tuples, SizeIs(1));

  EXPECT_THAT(tuples[0], ElementsAre(IsIri(type), IsVariable("?a")));
}

TEST(SparqlParser, PropertyListNotEmptyWithTerminator) {
  string input = "a ?a ; a ?b ; a ?c ;";
  ParserAndVisitor p{input};

  const auto [tuples, triples] =
      p.parser.propertyListNotEmpty()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::PropertyList>();
  EXPECT_THAT(triples, IsEmpty());

  ASSERT_THAT(tuples, SizeIs(3));

  EXPECT_THAT(tuples[0], ElementsAre(IsIri(type), IsVariable("?a")));
  EXPECT_THAT(tuples[1], ElementsAre(IsIri(type), IsVariable("?b")));
  EXPECT_THAT(tuples[2], ElementsAre(IsIri(type), IsVariable("?c")));
}

TEST(SparqlParser, VerbA) {
  string input = "a";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.verb()->accept(&p.visitor).as<VarOrTerm>();
  ASSERT_THAT(varOrTerm, IsIri(type));
}

TEST(SparqlParser, VerbVariable) {
  string input = "?a";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.verb()->accept(&p.visitor).as<VarOrTerm>();
  ASSERT_THAT(varOrTerm, IsVariable("?a"));
}

TEST(SparqlParser, ObjectListSingleton) {
  string input = "?a";
  ParserAndVisitor p{input};

  const auto [objects, triples] =
      p.parser.objectList()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::ObjectList>();
  EXPECT_THAT(triples, IsEmpty());

  ASSERT_THAT(objects, SizeIs(1));
  ASSERT_THAT(objects[0], IsVariable("?a"));
}

TEST(SparqlParser, ObjectList) {
  string input = "?a , ?b , ?c";
  ParserAndVisitor p{input};

  const auto [objects, triples] =
      p.parser.objectList()
          ->accept(&p.visitor)
          .as<ad_utility::sparql_types::ObjectList>();
  EXPECT_THAT(triples, IsEmpty());

  ASSERT_THAT(objects, SizeIs(3));
  ASSERT_THAT(objects[0], IsVariable("?a"));
  ASSERT_THAT(objects[1], IsVariable("?b"));
  ASSERT_THAT(objects[2], IsVariable("?c"));
}

TEST(SparqlParser, BlankNodePropertyList) {
  string input = "[ a ?a ; a ?b ; a ?c ]";
  ParserAndVisitor p{input};

  const auto [node, triples] = p.parser.blankNodePropertyList()
                                   ->accept(&p.visitor)
                                   .as<ad_utility::sparql_types::Node>();
  EXPECT_THAT(node, IsBlankNode(true, "0"));

  ASSERT_THAT(triples, SizeIs(3));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(type),             //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(type),             //
                                      IsVariable("?b")));

  EXPECT_THAT(triples[2], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(type),             //
                                      IsVariable("?c")));
}

TEST(SparqlParser, GraphNodeVarOrTerm) {
  string input = "?a";
  ParserAndVisitor p{input};

  const auto [node, triples] = p.parser.graphNode()
                                   ->accept(&p.visitor)
                                   .as<ad_utility::sparql_types::Node>();
  EXPECT_THAT(node, IsVariable("?a"));
  EXPECT_THAT(triples, IsEmpty());
}

TEST(SparqlParser, GraphNodeTriplesNode) {
  string input = "(?a)";
  ParserAndVisitor p{input};

  const auto [node, triples] = p.parser.graphNode()
                                   ->accept(&p.visitor)
                                   .as<ad_utility::sparql_types::Node>();
  EXPECT_THAT(node, IsBlankNode(true, "0"));

  ASSERT_THAT(triples, SizeIs(2));

  EXPECT_THAT(triples[0], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(first),            //
                                      IsVariable("?a")));

  EXPECT_THAT(triples[1], ElementsAre(IsBlankNode(true, "0"),  //
                                      IsIri(rest),             //
                                      IsIri(nil)));
}

TEST(SparqlParser, VarOrTermVariable) {
  string input = "?a";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.varOrTerm()->accept(&p.visitor).as<VarOrTerm>();
  EXPECT_THAT(varOrTerm, IsVariable("?a"));
}

TEST(SparqlParser, VarOrTermGraphTerm) {
  string input = "()";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.varOrTerm()->accept(&p.visitor).as<VarOrTerm>();
  EXPECT_THAT(varOrTerm, IsIri(nil));
}

TEST(SparqlParser, VarOrIriVariable) {
  string input = "?a";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.varOrIri()->accept(&p.visitor).as<VarOrTerm>();
  EXPECT_THAT(varOrTerm, IsVariable("?a"));
}

TEST(SparqlParser, VarOrIriIri) {
  string input = "<http://testiri>";
  ParserAndVisitor p{input};

  auto varOrTerm = p.parser.varOrIri()->accept(&p.visitor).as<VarOrTerm>();
  EXPECT_THAT(varOrTerm, IsIri(input));
}

TEST(SparqlParser, VariableWithQuestionMark) {
  string input = "?variableName";
  ParserAndVisitor p{input};

  auto variable = p.parser.var()->accept(&p.visitor).as<Variable>();
  EXPECT_THAT(variable, IsVariable(input));
}

TEST(SparqlParser, VariableWithDollarSign) {
  string input = "$variableName";
  ParserAndVisitor p{input};

  auto variable = p.parser.var()->accept(&p.visitor).as<Variable>();
  EXPECT_THAT(variable, IsVariable("?variableName"));
}
