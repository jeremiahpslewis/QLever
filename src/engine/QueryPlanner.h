// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)
#pragma once

#include <set>
#include <vector>

#include "../parser/ParsedQuery.h"
#include "QueryExecutionTree.h"

using std::vector;

class QueryPlanner {
 public:
  explicit QueryPlanner(QueryExecutionContext* qec);

  QueryExecutionTree createExecutionTree(ParsedQuery& pq);

  class TripleGraph {
   public:
    TripleGraph();

    TripleGraph(const TripleGraph& other);

    TripleGraph& operator=(const TripleGraph& other);

    TripleGraph(const TripleGraph& other, vector<size_t> keepNodes);

    struct Node {
      Node(size_t id, const SparqlTriple& t)
          : _id(id), _triple(t), _variables(), _cvar(), _wordPart() {
        if (isVariable(t._s)) {
          _variables.insert(t._s);
        }
        if (isVariable(t._p)) {
          _variables.insert(t._p._iri);
        }
        if (isVariable(t._o)) {
          _variables.insert(t._o);
        }
      }

      Node(size_t id, const string& cvar, const string& wordPart,
           const vector<SparqlTriple>& trips)
          : _id(id),
            _triple(cvar,
                    PropertyPath(PropertyPath::Operation::IRI, 0,
                                 INTERNAL_TEXT_MATCH_PREDICATE, {}),
                    wordPart),
            _variables(),
            _cvar(cvar),
            _wordPart(wordPart) {
        _variables.insert(cvar);
        for (const auto& t : trips) {
          if (isVariable(t._s)) {
            _variables.insert(t._s);
          }
          if (isVariable(t._p)) {
            _variables.insert(t._p._iri);
          }
          if (isVariable(t._o)) {
            _variables.insert(t._o);
          }
        }
      }

      Node(const Node& other) = default;

      Node& operator=(const Node& other) = default;

      // Returns true if the two nodes equal apart from the id
      // and the order of variables
      bool isSimilar(const Node& other) const {
        return _triple == other._triple && _cvar == other._cvar &&
               _wordPart == other._wordPart && _variables == other._variables;
      }

      friend std::ostream& operator<<(std::ostream& out, const Node& n) {
        out << "id: " << n._id << " triple: " << n._triple.asString()
            << " _vars ";
        for (const std::string& s : n._variables) {
          out << s << ", ";
        }
        out << " cvar " << n._cvar << " wordPart " << n._wordPart;
        return out;
      }

      size_t _id;
      SparqlTriple _triple;
      std::set<std::string> _variables;
      string _cvar;
      string _wordPart;
    };

    // Allows for manually building triple graphs for testing
    TripleGraph(const std::vector<std::pair<Node, std::vector<size_t>>>& init);

    // Checks for id and order independent equality
    bool isSimilar(const TripleGraph& other) const;
    string asString() const;

    bool isTextNode(size_t i) const;

    vector<vector<size_t>> _adjLists;
    ad_utility::HashMap<size_t, Node*> _nodeMap;
    std::list<TripleGraph::Node> _nodeStorage;

    ad_utility::HashMap<string, vector<size_t>> identifyTextCliques() const;

    vector<size_t> bfsLeaveOut(size_t startNode,
                               ad_utility::HashSet<size_t> leaveOut) const;

    void collapseTextCliques();

    bool isPureTextQuery();

   private:
    vector<pair<TripleGraph, vector<SparqlFilter>>> splitAtContextVars(
        const vector<SparqlFilter>& origFilters,
        ad_utility::HashMap<string, vector<size_t>>& contextVarTotextNodes)
        const;

    vector<SparqlFilter> pickFilters(const vector<SparqlFilter>& origFilters,
                                     const vector<size_t>& nodes) const;
  };

  class SubtreePlan {
   public:
    enum Type { BASIC, OPTIONAL, MINUS };

    explicit SubtreePlan(QueryExecutionContext* qec)
        : _qet(std::make_shared<QueryExecutionTree>(qec)) {}

    std::shared_ptr<QueryExecutionTree> _qet;
    std::shared_ptr<ResultTable> _cachedResult;
    bool _isCached = false;
    uint64_t _idsOfIncludedNodes = 0;
    uint64_t _idsOfIncludedFilters = 0;
    Type type = Type::BASIC;

    size_t getCostEstimate() const;

    size_t getSizeEstimate() const;

    void addAllNodes(uint64_t otherNodes);
  };

  TripleGraph createTripleGraph(
      const GraphPatternOperation::BasicGraphPattern* pattern) const;

  static ad_utility::HashMap<string, size_t>
  createVariableColumnsMapForTextOperation(
      const string& contextVar, const string& entityVar,
      const ad_utility::HashSet<string>& freeVars,
      const vector<pair<QueryExecutionTree, size_t>>& subtrees);

  static ad_utility::HashMap<string, size_t>
  createVariableColumnsMapForTextOperation(
      const string& contextVar, const string& entityVar,
      const vector<pair<QueryExecutionTree, size_t>>& subtrees) {
    return createVariableColumnsMapForTextOperation(
        contextVar, entityVar, ad_utility::HashSet<string>(), subtrees);
  };

  static ad_utility::HashMap<string, size_t>
  createVariableColumnsMapForTextOperation(const string& contextVar,
                                           const string& entityVar) {
    return createVariableColumnsMapForTextOperation(
        contextVar, entityVar, vector<pair<QueryExecutionTree, size_t>>());
  }

  static ad_utility::HashMap<string, size_t>
  createVariableColumnsMapForTextOperation(
      const string& contextVar, const string& entityVar,
      const ad_utility::HashSet<string>& freeVars) {
    return createVariableColumnsMapForTextOperation(
        contextVar, entityVar, freeVars,
        vector<pair<QueryExecutionTree, size_t>>());
  };

  void setEnablePatternTrick(bool enablePatternTrick);

 private:
  QueryExecutionContext* _qec;

  // Used to count the number of unique variables created using
  // generateUniqueVarName
  size_t _internalVarCount;

  bool _enablePatternTrick;

  std::vector<QueryPlanner::SubtreePlan> optimize(
      ParsedQuery::GraphPattern* rootPattern);

  /**
   * @brief Fills varToTrip with a mapping from all variables in the root graph
   * pattern (no matter whether they are in the subject, predicate or object) to
   * the triple they occur in. Fills contextVars with all subject variables for
   * which the predicate is either 'contains-word' or 'contains-entity'.
   */
  void getVarTripleMap(
      const ParsedQuery& pq,
      ad_utility::HashMap<string, vector<SparqlTriple>>* varToTrip,
      ad_utility::HashSet<string>* contextVars) const;

  /**
   * @brief Fills children with all operations that are associated with a single
   * node in the triple graph (e.g. IndexScans).
   */
  vector<SubtreePlan> seedWithScansAndText(
      const TripleGraph& tg,
      const vector<vector<QueryPlanner::SubtreePlan>>& children);

  /**
   * @brief Returns a subtree plan that will compute the values for the
   * variables in this single triple. Depending on the triple's PropertyPath
   * this subtree can be arbitrarily large.
   */
  vector<SubtreePlan> seedFromPropertyPathTriple(const SparqlTriple& triple);

  /**
   * @brief Returns a parsed query for the property path.
   */
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromPropertyPath(
      const std::string& left, const PropertyPath& path,
      const std::string& right);

  std::shared_ptr<ParsedQuery::GraphPattern> seedFromSequence(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromAlternative(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromTransitive(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromTransitiveMin(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromTransitiveMax(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromInverse(
      const std::string& left, const PropertyPath& path,
      const std::string& right);
  std::shared_ptr<ParsedQuery::GraphPattern> seedFromIri(
      const std::string& left, const PropertyPath& path,
      const std::string& right);

  std::string generateUniqueVarName();

  // Creates a tree of unions with the given patterns as the trees leaves
  ParsedQuery::GraphPattern uniteGraphPatterns(
      std::vector<ParsedQuery::GraphPattern>&& patterns) const;

  /**
   * @brief Merges two rows of the dp optimization table using various types of
   * joins.
   * @return A new row for the dp table that contains plans created by joining
   * the result of a plan in a and a plan in b.
   */
  vector<SubtreePlan> merge(const vector<SubtreePlan>& a,
                            const vector<SubtreePlan>& b,
                            const TripleGraph& tg) const;

  std::vector<QueryPlanner::SubtreePlan> createJoinCandidates(
      const SubtreePlan& a, const SubtreePlan& b,
      std::optional<TripleGraph> tg) const;

  vector<SubtreePlan> getOrderByRow(
      const ParsedQuery& pq, const vector<vector<SubtreePlan>>& dpTab) const;

  vector<SubtreePlan> getGroupByRow(
      const ParsedQuery& pq, const vector<vector<SubtreePlan>>& dpTab) const;

  vector<SubtreePlan> getDistinctRow(
      const ParsedQuery::SelectClause& selectClause,
      const vector<vector<SubtreePlan>>& dpTab) const;

  vector<SubtreePlan> getPatternTrickRow(
      const ParsedQuery::SelectClause& selectClause,
      const vector<vector<SubtreePlan>>& dpTab,
      const SparqlTriple& patternTrickTriple);

  vector<SubtreePlan> getHavingRow(
      const ParsedQuery& pq, const vector<vector<SubtreePlan>>& dpTab) const;

  bool connected(const SubtreePlan& a, const SubtreePlan& b,
                 const TripleGraph& graph) const;

  vector<array<Id, 2>> getJoinColumns(const SubtreePlan& a,
                                      const SubtreePlan& b) const;

  string getPruningKey(const SubtreePlan& plan,
                       const vector<size_t>& orderedOnColumns) const;

  void applyFiltersIfPossible(vector<SubtreePlan>& row,
                              const vector<SparqlFilter>& filters,
                              bool replaceInsteadOfAddPlans) const;

  std::shared_ptr<Operation> createFilterOperation(
      const SparqlFilter& filter, const SubtreePlan& parent) const;

  /**
   * @brief Optimize a set of triples, filters and precomputed candidates
   * for child graph patterns
   *
   *
   * Optimize every GraphPattern starting with the leaves of the
   * GraphPattern tree.

   * Strategy:
   * Create a graph.
   * Each triple corresponds to a node, there is an edge between two nodes
   * iff they share a variable.

   * TripleGraph tg = createTripleGraph(&arg);

   * Each node/triple corresponds to a scan (more than one way possible),
   * each edge corresponds to a possible join.

   * Enumerate and judge possible query plans using a DP table.
   * Each ExecutionTree for a sub-problem gives an estimate:
   * There are estimates for cost and size ( and multiplicity per column).
   * Start bottom up, i.e. with the scans for triples.
   * Always merge two solutions from the table by picking one possible
   * join. A join is possible, if there is an edge between the results.
   * Therefore we keep track of all edges that touch a sub-result.
   * When joining two sub-results, the results edges are those that belong
   * to exactly one of the two input sub-trees.
   * If two of them have the same target, only one out edge is created.
   * All edges that are shared by both subtrees, are checked if they are
   * covered by the join or if an extra filter/select is needed.

   * The algorithm then creates all possible plans for 1 to n triples.
   * To generate a plan for k triples, all subsets between i and k-i are
   * joined.

   * Filters are now added to the mix when building execution plans.
   * Without them, a plan has an execution tree and a set of
   * covered triple nodes.
   * With them, it also has a set of covered filters.
   * A filter can be applied as soon as all variables that occur in the
   * filter Are covered by the query. This is also always the place where
   * this is done.

   * Text operations form cliques (all triples connected via the context
   * cvar). Detect them and turn them into nodes with stored word part and
   * edges to connected variables.

   * Each text operation has two ways how it can be used.
   * 1) As leave in the bottom row of the tab.
   * According to the number of connected variables, the operation creates
   * a cross product with n entities that can be used in subsequent joins.
   * 2) as intermediate unary (downwards) nodes in the execution tree.
   * This is a bit similar to sorts: they can be applied after each step
   * and will filter on one variable.
   * Cycles have to be avoided (by previously removing a triple and using
   * it as a filter later on).
   */
  vector<vector<SubtreePlan>> fillDpTab(
      const TripleGraph& graph, const vector<SparqlFilter>& fs,
      const vector<vector<SubtreePlan>>& children);

  size_t getTextLimit(const string& textLimitString) const;

  SubtreePlan getTextLeafPlan(const TripleGraph::Node& node) const;

  SubtreePlan optionalJoin(const SubtreePlan& a, const SubtreePlan& b) const;
  SubtreePlan minus(const SubtreePlan& a, const SubtreePlan& b) const;
  SubtreePlan multiColumnJoin(const SubtreePlan& a, const SubtreePlan& b) const;

  /**
   * @brief Determines if the pattern trick (and in turn the
   * CountAvailablePredicates operation) are applicable to the given
   * parsed query. If a ql:has-predicate triple is found and
   * CountAvailblePredicates can be used for it, the triple will be removed from
   * the parsed query.
   * @param pq The parsed query.
   * @param patternTrickTriple An output parameter in which the triple that
   * satisfies the requirements for the pattern trick is stored.
   * @return True if the pattern trick should be used.
   */
  bool checkUsePatternTrick(ParsedQuery* pq,
                            SparqlTriple* patternTrickTriple) const;

  /**
   * @brief return the index of the cheapest execution tree in the argument.
   *
   * If we are in the unit test mode, this is deterministic by additionally
   * sorting by the cache key when comparing equally cheap indices, else the
   * first element that has the minimum index is returned.
   */
  size_t findCheapestExecutionTree(
      const std::vector<SubtreePlan>& lastRow) const;

  /// if this Planner is not associated with a queryExecutionContext we are only
  /// in the unit test mode
  [[nodiscard]] bool isInTestMode() const { return _qec == nullptr; }
};
