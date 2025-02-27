//  Copyright 2021, University of Freiburg, Chair of Algorithms and Data
//  Structures. Author: Johannes Kalmbach <kalmbacj@cs.uni-freiburg.de>
// Created by johannes on 09.05.21.
//

#ifndef QLEVER_SPARQLEXPRESSION_H
#define QLEVER_SPARQLEXPRESSION_H

#include <memory>
#include <span>
#include <variant>
#include <vector>

#include "../../global/Id.h"
#include "../../util/ConstexprSmallString.h"
#include "../CallFixedSize.h"
#include "../QueryExecutionContext.h"
#include "../ResultTable.h"
#include "./SparqlExpressionTypes.h"
#include "./SparqlExpressionValueGetters.h"
#include "SetOfIntervals.h"

namespace sparqlExpression {

/// Virtual base class for an arbitrary Sparql Expression which holds the
/// structure of the expression as well as the logic to evaluate this expression
/// on a given intermediate result
class SparqlExpression {
 private:
  std::string _descriptor;

 public:
  /// ________________________________________________________________________
  using Ptr = std::unique_ptr<SparqlExpression>;

  /// Evaluate a Sparql expression.
  virtual ExpressionResult evaluate(EvaluationContext*) const = 0;

  /// Return all variables and IRIs, needed for certain parser methods.
  /// TODO<joka921> should be called getStringLiteralsAndVariables
  virtual vector<string*> strings() final {
    vector<string*> result;
    // Recursively aggregate the strings from all children.
    for (auto& child : children()) {
      auto childStrings = child->strings();
      result.insert(result.end(), childStrings.begin(), childStrings.end());
    }

    // Add the strings from this expression.
    auto locallyAdded = getStringLiteralsAndVariablesNonRecursive();
    for (auto& el : locallyAdded) {
      result.push_back(&el);
    }
    return result;
  }

  /// Return all the variables that occur in the expression, but are not
  /// aggregated.
  virtual vector<std::string> getUnaggregatedVariables() {
    // Default implementation: This expression adds no variables, but all
    // unaggregated variables from the children remain unaggregated.
    std::vector<string> result;
    for (const auto& child : children()) {
      auto childResult = child->getUnaggregatedVariables();
      result.insert(result.end(), std::make_move_iterator(childResult.begin()),
                    std::make_move_iterator(childResult.end()));
    }
    return result;
  }

  /// Get a unique identifier for this expression, used as cache key.
  virtual string getCacheKey(const VariableToColumnMap& varColMap) const = 0;

  /// Get a short, human readable identifier for this expression.
  virtual const string& descriptor() const final { return _descriptor; }
  virtual string& descriptor() final { return _descriptor; }

  /// For the pattern trick we need to know, whether this expression
  /// is a non-distinct count of a single variable. In this case we return
  /// the variable. Otherwise we return std::nullopt.
  virtual std::optional<string> getVariableForNonDistinctCountOrNullopt()
      const {
    return std::nullopt;
  }

  /// Helper function for getVariableForNonDistinctCountOrNullopt() : If this
  /// expression is a single variable, return the name of this variable.
  /// Otherwise, return std::nullopt.
  virtual std::optional<string> getVariableOrNullopt() const {
    return std::nullopt;
  }

  // __________________________________________________________________________
  virtual ~SparqlExpression() = default;

 private:
  // Get the direct child expressions.
  virtual std::span<SparqlExpression::Ptr> children() = 0;

  // Helper function for strings(). Get all variables, iris, and string literals
  // that are included in this expression directly, ignoring possible child
  // expressions.
  virtual std::span<string> getStringLiteralsAndVariablesNonRecursive() {
    // Default implementation: This expression adds no strings or variables.
    return {};
  }
};
}  // namespace sparqlExpression

#endif  // QLEVER_SPARQLEXPRESSION_H
