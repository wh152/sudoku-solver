#pragma once
#include "Board.hpp"
#include <algorithm>
#include <tuple>


namespace Sudoku {

template<BoardSymbol SymT>
class Solver { // later create Board and BoardFactory classes (factory can be more complex after)
private:
  Sudoku::Board<SymT> board;
  bool sym_found = true;
  std::string result_file;
  std::set<BoardNode<SymT>> guessed_syms;

  BoardNode<SymT> get_first_backtracking_node();

  std::set<SymT> idx_valid_syms(const size_t row_idx, const size_t col_idx);

  std::set<SymT> idx_valid_options(const size_t row_idx, const size_t col_idx);

  void check_sole_valid_sym(const size_t row_idx, const size_t col_idx, std::set<SymT> &valid_options);

  void add_guessed_parent(const BoardNode<SymT> &parent);

  void add_guessed_child(const BoardNode<SymT> &parent, const BoxLocT &child_box_idx, const SymT sym);

  void del_guessed_parent(const BoardNode<SymT> &parent);

  void del_guessed_child(const BoardNode<SymT> &parent, const BoardNode<SymT> &child);

  void print_guessed_syms();

  std::expected<bool, BoardNode<SymT>>
  guess_sym(const BoardNode<SymT> &parent, bool using_row=true);

  std::expected<void, parse_board_err>
  test_box(const size_t box_row_idx, const size_t box_idx, const BoardNode<SymT> *parent);

  std::expected<void, parse_board_err> non_backtracking_search(const BoardNode<SymT> *parent, 
                                                                const BoardNode<SymT> *node);

public:
  Solver(const char *num_dims, const char *board_file, const char *outfile)
    : board(num_dims, board_file), result_file(outfile) {}

  bool solve();

  std::string write_answer();
};

template<BoardSymbol SymT>
BoardNode<SymT> Solver<SymT>::get_first_backtracking_node() {
  BoardNode<SymT> start_node;
  size_t start_row_idx = 0;

  bool start_row_found = false;
  while (!start_row_found) {
    const auto start_row_set = this->board.row_sets.at(start_row_idx);
    if (start_row_set.size() == this->board.symbols.size()) {
      std::println("start_row_set full, start_row_idx = {0}", start_row_idx);
      ++start_row_idx;
      continue;
    }
    start_row_found = true;

    std::println("start_row_idx = {0}", start_row_idx);
    const auto start_row = this->board.rows.at(start_row_idx);
    std::println("start_row = ");
    this->board.print_group(start_row);
    const auto start_col_iter = std::find(start_row.begin(), start_row.end(), this->board.SYM_UNKNOWN);
    const size_t start_col = std::distance(start_row.begin(), start_col_iter);
    std::println("start_col = {0}", start_col);
    const auto start_valid_syms = idx_valid_syms(start_row_idx, start_col);
    std::println("start_valid_syms = ");
    this->board.print_set(start_valid_syms);
    const SymT start_sym = *start_valid_syms.begin();
    std::println("start_sym = {0}", static_cast<char>(start_sym));
    start_node = BoardNode<SymT>{start_row_idx, start_col, start_sym};
  } while (!start_row_found);

  return start_node;
}

// update this to check if any other unknown syms in box/row/col have no valid syms left
template<BoardSymbol SymT>
std::set<SymT> Solver<SymT>::idx_valid_syms(const size_t row_idx, const size_t col_idx) {
  const auto &[box_indices, box_sym_idx] = board.row_col_idx_to_box_idx(row_idx, col_idx);
  const auto &[box_row_idx, box_idx] = box_indices;

  const std::set<SymT> box_syms = this->board.box_row_sets.at(box_row_idx).at(box_idx);
  const std::set<SymT> row_syms = this->board.row_sets.at(row_idx);
  const std::set<SymT> col_syms = this->board.col_sets.at(col_idx);

  std::set<SymT> valid_syms{};
  for (const SymT sym : this->board.symbols)
    if (!box_syms.contains(sym) && !row_syms.contains(sym) && !col_syms.contains(sym))
      valid_syms.insert(sym);

  return valid_syms;
}

// TODO: track which added syms made a state invalid, and prevent that combination coming up again
// TODO: do non-backtracking scan after inserting each element
//  - keep hashmap of parent node to set of (row,col,sym) tuples
//  - remove everything in the set when going back
template<BoardSymbol SymT>
std::set<SymT> Solver<SymT>::idx_valid_options(const size_t row_idx, const size_t col_idx) {
  auto valid_options = idx_valid_syms(row_idx, col_idx);
  std::print("({0},{1}) initial valid_options = ", row_idx, col_idx);
  this->board.print_set(valid_options);

  const auto &[box_indices, box_sym_idx] = board.row_col_idx_to_box_idx(row_idx, col_idx);
  const auto &[box_row_idx, box_idx] = box_indices;

  // if other unknown sym in box/row/col has 1 valid sym that would be 
  // taken by this guess, then it's not a valid option

  for (const auto &[other_col_idx, other_row_sym] : std::views::enumerate(this->board.rows.at(row_idx)))
    if (col_idx != other_col_idx && other_row_sym == this->board.SYM_UNKNOWN)
      check_sole_valid_sym(row_idx, other_col_idx, valid_options);
  for (const auto &[other_row_idx, other_col_sym] : std::views::enumerate(this->board.cols.at(col_idx)))
    if (row_idx != other_row_idx && other_col_sym == this->board.SYM_UNKNOWN)
      check_sole_valid_sym(other_row_idx, col_idx, valid_options);

  return valid_options;
}

template<BoardSymbol SymT>
void Solver<SymT>::check_sole_valid_sym(const size_t row_idx, const size_t col_idx, std::set<SymT> &valid_options) {
  const auto other_valid_syms = idx_valid_syms(row_idx, col_idx);
  if (other_valid_syms.size() != 1)
    return;

  const SymT other_only_valid_sym = *other_valid_syms.begin();
  if (valid_options.contains(other_only_valid_sym)) {
    std::println("({0},{1}) clashes, erasing {2} from valid_options", row_idx, col_idx, (char) other_only_valid_sym);
    valid_options.erase(other_only_valid_sym);
  }
}

/* Next step for hard sudokus is to guess the next numbers
* Store 2D valid_syms sets to plug values in
* Get probabilities and use dynamic programming or DFS to solve
* Backtracking solution:
* - after guaranteed picks, choose the first empty index by box
* - create tree where nodes have a sym val and a row/col idx pair
*   - there's always at least another unknown sym in any box/row/col
* - check either row/col arbitrarily minus first sym from row/col_set for validity
*   - the first time this is guaranteed true but helps simplify recursion
* - iterate over each symbols in the row/col, assign a remaining sym and recurse
*   - alternate between rows and cols (otherwise end up filling the same row/col)
*   - don't think it matters if you re-use a row/col from earlier
*   - update vectors/sets for box/row/col every time
* - leaf node when row/col is filled and valid
*   - go up and continue filling out
* - prune when the current symbol is invalid or no symbols remaining
*   - check before inserting into vectors/sets for box/row/col
*   - delete from vectors/sets for box/row/col if earlier tested sym invalid
*     - add to set of invalid nodes
*     - return std::unexpected with sym/row/col
*   - if call returns with unexpected, check if sym/row/col match
*     - if not return unexpected up
*     - if yes don't return nullptr (std::optional) so only valid path gives val
*/

/* Note: vectorize later by checking multiple possibilities simultaneously?
*   - would have to do set subtractions based on pre-defined list of sim
* Note: could use backtracking (dp) for general solution, not just set substitution?
* Note: all sym idx options could still restrict options for another sym
*   e.g. hard_solution.txt: mid-bottom box has both options for 9 on the bottom row
*   so 9 must be in idx 1 or 4 in the bottom-right box, can't be at idx 7
*   also bottom-left box has 3 in one of indices 0, 2, 3, 5, but not idx 8
*   also 3/4 chance top-left box's top row has a 2
*     -> 3/4 chance top-mid box's bottom-right is a 2
*     -> 
* Note: to get exact probabilities have to follow the entire chain
* - ignore probabilities for now (not sure if calculating is even quicker)
*/

template<BoardSymbol SymT>
void Solver<SymT>::print_guessed_syms() {
  for (const auto &[parent_guess, children_guesses] : this->board.guessed_syms) {
    std::print("{0} - [", this->board.show_node(parent_guess));
    for (const auto & [child_idx, child_guess] : std::views::enumerate(children_guesses)) {
      std::print("{0}", this->board.show_node(child_guess));
      if (child_idx != children_guesses.size() - 1)
        std::print(", ");
    }
    std::println("]({0})", children_guesses.size());
  }
}

template<BoardSymbol SymT>
void Solver<SymT>::add_guessed_parent(const BoardNode<SymT> &parent) {
  std::println("In check_guessed_parent, parent = {0}", this->board.show_node(parent));
  if (!this->board.guessed_syms.contains(parent))
    this->board.guessed_syms[parent] = std::set<BoardNode<SymT>>{};
}

template<BoardSymbol SymT>
void Solver<SymT>::add_guessed_child(const BoardNode<SymT> &parent, const BoxLocT &child_box_idx, const SymT sym) {
  const auto &[box_row_idx, box_idx, box_sym_idx] = child_box_idx;
  const auto &[row_idx, col_idx] = this->board.box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
  std::println("In add_guessed_child: [{0},{1}: {2}] guessing child [{3},{4}: {5}]",
                parent.row_idx, parent.col_idx, (char)parent.sym, row_idx, col_idx, (char)sym);
  add_guessed_parent(parent);
  this->board.guessed_syms.at(parent).emplace(BoardNode<SymT>{row_idx, col_idx, sym});
}

template<BoardSymbol SymT>
void Solver<SymT>::del_guessed_parent(const BoardNode<SymT> &parent) {
  std::println("In del_guessed_parent with parent = {0}", this->board.show_node(parent));
  if (!this->board.guessed_syms.contains(parent)) {
    std::println("guessed_syms doesn't contain {0}, returning...", this->board.show_node(parent));
    return;
  }
  print_guessed_syms();
  for (const BoardNode<SymT> &child : this->board.guessed_syms.at(parent))
    del_guessed_parent(child);
  for (const BoardNode<SymT> &child : this->board.guessed_syms.at(parent)) {
    std::println("[{0},{1}: {2}] deleting child [{3},{4}: {5}]",
                parent.row_idx, parent.col_idx, (char)parent.sym, child.row_idx, child.col_idx, (char)child.sym);
    this->board.del_sym(child.sym, child.row_idx, child.col_idx);
  }
  this->board.guessed_syms.erase(parent);
  std::println("Deleting parent {0}", this->board.show_node(parent));
  this->board.del_sym(parent.sym, parent.row_idx, parent.col_idx);
}

template<BoardSymbol SymT>
void Solver<SymT>::del_guessed_child(const BoardNode<SymT> &parent, const BoardNode<SymT> &child) {
  std::println("In del_guessed_child with parent = {0}, child = {1}", 
                this->board.show_node(parent), this->board.show_node(child));
  if (!this->board.guessed_syms.contains(parent)) {
    std::println("guessed_syms doesn't contain {0}, returning...", this->board.show_node(parent));
    return;
  }
  this->board.guessed_syms.at(parent).erase(child);
}

// TODO: if 1 sym left in box/row/col check, prune branch if invalid (dp)
// e.g. hard.txt has bottom-right sym in top-left box ? when it should be pruned
// when detect invalid sym, must check if any substituted syms caused this
// must add to the guessed_syms state and find a way of recursing back to there
// use std::unexpected with the value of the failure-causing sym
// but how to know 
// TODO: keep state of valid_syms for each (row, col)
// every time you add a symbol check if valid syms in box/row/col are empty
// for boxes in the cross-shape on the box the symbol got added to
// if pos A has (x,y,z) and any pos B in cross-shape has (x,y) remove z
// from every valid syms set for all pos in cross-shape
// this scanning can be done recursively until no updates are left
template<BoardSymbol SymT>
std::expected<bool, BoardNode<SymT>> 
Solver<SymT>::guess_sym(const BoardNode<SymT> &parent, bool using_row) {
  std::println("In guess_sym, parent={0}, ({1},{2}), using_row={3}",
                static_cast<char>(parent.sym), parent.row_idx, parent.col_idx, using_row);
  this->board.print_board(this->board.rows);
  std::vector<SymT> lane, perp_lane;
  std::set<SymT> lane_syms;

  if (using_row) {
    lane = this->board.rows.at(parent.row_idx);
    perp_lane = this->board.cols.at(parent.col_idx);
    lane_syms = this->board.row_sets.at(parent.row_idx);
  } else {
    lane = this->board.cols.at(parent.col_idx);
    perp_lane = this->board.rows.at(parent.row_idx);
    lane_syms = this->board.col_sets.at(parent.col_idx);
  }
  std::print("lane = ");
  board.print_group(lane);
  std::print("perp_lane = ");
  board.print_group(perp_lane);

  const auto first_unknown_sym_iter = std::find(lane.begin(), lane.end(), board.SYM_UNKNOWN);
  if (first_unknown_sym_iter == lane.end())
    return true;

  std::println("Iterating over the lane");
  for (size_t lane_idx = 0; lane_idx < lane.size(); ++lane_idx) {
    std::println("Lane ({0},{1})[{2}], lane_idx = {3}, sym = {4}", 
                  parent.row_idx, parent.col_idx, using_row, lane_idx, (char)lane.at(lane_idx));
    if (lane.at(lane_idx) != board.SYM_UNKNOWN)
      continue;
    std::println("[{0},({1},{2}),{3}]: lane_idx = {4}, lane_val = {5}", 
                  (char)parent.sym, parent.row_idx, parent.col_idx, using_row,
                  lane_idx, static_cast<char>(lane.at(lane_idx)));

    const size_t sym_row_idx = (using_row ? parent.row_idx : lane_idx);
    const size_t sym_col_idx = (using_row ? lane_idx : parent.col_idx);

    std::set<SymT> valid_options;
    valid_options = idx_valid_options(sym_row_idx, sym_col_idx);
    std::print("({0},{1}) valid_options = ", sym_row_idx, sym_col_idx);
    this->board.print_set(valid_options);
    if (valid_options.size() == 0)
      return std::unexpected(parent);

    std::println("Iterating over the valid_options");
    for (const SymT test_sym : valid_options) {
      std::print("parent: ({0},{1}), test_sym = {2} valid_options=", 
                  parent.row_idx, parent.col_idx, static_cast<char>(test_sym));
      this->board.print_set(valid_options);
      const auto node_option = BoardNode<SymT>{sym_row_idx, sym_col_idx, test_sym};
      std::println("parent_option {0} of parent {1}", 
                    this->board.show_node(node_option), this->board.show_node(parent));
      const auto &[parent_option_box_data, parent_option_sym_idx] = this->board.row_col_idx_to_box_idx(
                                                            node_option.row_idx, node_option.col_idx);
      const auto &[parent_option_box_row_idx, parent_option_box_idx] = parent_option_box_data;
      const BoxLocT parent_option_loc = std::make_tuple(parent_option_box_row_idx, parent_option_box_idx, 
                                                        parent_option_sym_idx);
      add_guessed_parent(node_option);
      if (parent != this->board.first_backtracking_node)
        add_guessed_child(parent, parent_option_loc, test_sym);
      board.add_row_sym(node_option.sym, node_option.row_idx, node_option.col_idx, true);
      const auto non_backtracking_res = non_backtracking_search(&parent, &node_option);
      if (!non_backtracking_res.has_value()) { // maybe check for logic_error, does that mean something went wrong earlier?
        std::println("parent_option {0} is invalid", this->board.show_node(node_option));
        del_guessed_parent(node_option);
        if (parent != this->board.first_backtracking_node) {
          del_guessed_child(parent, node_option);
          if (const auto search_err = non_backtracking_res.error(); search_err == parse_board_err::invalid_parent) {
            std::println("Invalid parent {0}", this->board.show_node(parent));
            return std::unexpected(parent);
          }
        }
        print_guessed_syms();
        std::println("Retracted board since parent option {0} invalid", this->board.show_node(node_option));
        this->board.print_board(this->board.rows);
        continue;
      }
      const auto node = BoardNode<SymT>{sym_row_idx, sym_col_idx, test_sym};
      std::println("node = {0}, ({1},{2})", (char)node.sym, node.row_idx, node.col_idx);
      const auto sym_valid_val = guess_sym(node, !using_row);
      if (!sym_valid_val.has_value()) {
        std::println("node ({0},{1}) is invalid", node.row_idx, node.col_idx);
        del_guessed_parent(node);
        if (parent != this->board.first_backtracking_node)
          del_guessed_child(parent, node);
        std::println("Retracted board since node {0} invalid", this->board.show_node(node));
        this->board.print_board(this->board.rows);
        continue;
      }
      const bool sym_valid = sym_valid_val.value();
      if (sym_valid)
        return true;
    }
  }

  del_guessed_parent(parent);
  return false;
}

template<BoardSymbol SymT>
std::expected<void, parse_board_err> 
Solver<SymT>::test_box(const size_t box_row_idx, const size_t box_idx, const BoardNode<SymT> *parent) {
  std::print("test_box ({0},{1})", box_row_idx, box_idx);
  if (parent != nullptr) {
    std::println(", parent = {0}", this->board.show_node(*parent));
  } else
    std::println();
  BoxT<SymT> box = this->board.boxes[box_row_idx][box_idx];
  this->board.print_group(box);
  BoxSetT<SymT> box_set = this->board.box_row_sets[box_row_idx][box_idx];

  // TODO: why does this fail?
  auto unknown_sym_indices = this->board.unknown_symbols_indices(box);
  std::print("unknown_sym_indices = ");
  for (const auto unknown_sym_idx : unknown_sym_indices)
    std::print("{0} ", unknown_sym_idx);
  std::println();

  std::set<SymT> unknown_syms{};
  std::set_difference(this->board.symbols.begin(), this->board.symbols.end(), box_set.begin(), 
                      box_set.end(), std::inserter(unknown_syms, unknown_syms.end()));
  std::print("unknown_syms = ");
  this->board.print_group(unknown_syms);
  
  const auto unordered_box_syms = this->board.box_row_sets.at(box_row_idx).at(box_idx);
  const auto box_syms = std::set<SymT>(unordered_box_syms.begin(), unordered_box_syms.end());
  std::print("({0},{1}): box_syms = ", box_row_idx, box_idx);
  this->board.print_group(box_syms);

  std::vector<std::set<SymT>> all_valid_options;
  for (const size_t box_sym_idx : unknown_sym_indices) {
    const auto &[sym_row_idx, sym_col_idx] = this->board.box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
    all_valid_options.emplace_back(idx_valid_options(sym_row_idx, sym_col_idx));
  }
  std::set<SymT> all_valid_syms;
  for (const auto &[idx_valid_options, sym_valid_options] : std::views::enumerate(all_valid_options)) {
    std::print("valid_options[{0}] = ", idx_valid_options);
    this->board.print_set(sym_valid_options);
    all_valid_syms.insert_range(sym_valid_options);
  }
  for (const SymT unknown_sym : unknown_syms) {
    if (!all_valid_syms.contains(unknown_sym)) {
      std::println("({0},{1}) has no index where {2} can go, parent invalid", box_row_idx, box_idx, (char)unknown_sym);
      return std::unexpected(parse_board_err::invalid_parent);
    }
  }

  bool box_updated = true;
  while (box_updated && !unknown_sym_indices.empty()) {
    box_updated = false;

    std::println("Trying to find symbols");
    for (const auto [unknown_sym_idx, box_sym_idx] : std::views::enumerate(unknown_sym_indices)) {
      const auto valid_options = all_valid_options.at(unknown_sym_idx);
      const auto &[sym_row_idx, sym_col_idx] = this->board.box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
      std::print("({0},{1}) valid_options = ", sym_row_idx, sym_col_idx);
      this->board.print_set(valid_options);

      std::set<SymT> invalid_syms;
      for (const auto [other_unknown_sym_idx, other_box_sym_idx] : std::views::enumerate(unknown_sym_indices))
        if (unknown_sym_idx != other_unknown_sym_idx)
          invalid_syms.insert_range(all_valid_options.at(other_unknown_sym_idx));
      std::print("invalid_syms = ");
      this->board.print_set(invalid_syms);

      std::vector<SymT> sym_guesses;
      std::set_difference(valid_options.begin(), valid_options.end(), 
                          invalid_syms.begin(), invalid_syms.end(),
                          std::back_inserter(sym_guesses));
      std::print("sym_guesses = ");
      this->board.print_set(sym_guesses);

      if (sym_guesses.size() > 1)
        return std::unexpected(parse_board_err::invalid_logic);
      if (sym_guesses.size() == 0)
        continue;

      const SymT sym_guess = sym_guesses.at(0);
      this->board.add_box_sym(sym_guess, box_row_idx, box_idx, box_sym_idx, true);
      if (parent != nullptr) {
        add_guessed_child(*parent, std::make_tuple(box_row_idx, box_idx, box_sym_idx), sym_guess);
      }

      box_updated = true;
      std::println("Box ({0},{1}), added child guess (({2},{3}): {4})", 
                    box_row_idx, box_idx, sym_row_idx, sym_col_idx, static_cast<char>(sym_guess));
      this->board.print_board(this->board.rows);

      break;
    }

    unknown_sym_indices = this->board.unknown_symbols_indices(this->board.boxes[box_row_idx][box_idx]);
    std::print("updated unknown_sym_indices = ");
    for (const auto unknown_sym_idx : unknown_sym_indices)
      std::print("{0} ", unknown_sym_idx);
    std::println();

    box_set = this->board.box_row_sets[box_row_idx][box_idx];
    std::print("updated box_set = ");
    this->board.print_set(box_set);
    unknown_syms.clear();
    std::set_difference(this->board.symbols.begin(), this->board.symbols.end(), box_set.begin(), 
                        box_set.end(), std::inserter(unknown_syms, unknown_syms.end()));
    std::print("updated unknown_syms = ");
    this->board.print_group(unknown_syms);

    all_valid_options.clear();
    for (const size_t box_sym_idx : unknown_sym_indices) {
      const auto &[sym_row_idx, sym_col_idx] = this->board.box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
      all_valid_options.emplace_back(idx_valid_options(sym_row_idx, sym_col_idx));
    }
    all_valid_syms.clear();
    for (const auto &[idx_valid_options, sym_valid_options] : std::views::enumerate(all_valid_options)) {
      std::print("updated valid_options[{0}] = ", idx_valid_options);
      this->board.print_set(sym_valid_options);
      all_valid_syms.insert_range(sym_valid_options);
    }
    for (const SymT unknown_sym : unknown_syms) {
      if (!all_valid_syms.contains(unknown_sym)) {
        std::println("({0},{1}) has no index where {2} can go, parent invalid", box_row_idx, box_idx, (char)unknown_sym);
        return std::unexpected(parse_board_err::invalid_parent);
      }
    }
  }

  return {};
}

// TODO: check idx_valid_options when backtracking too
template<BoardSymbol SymT>
std::expected<void, parse_board_err> Solver<SymT>::non_backtracking_search(const BoardNode<SymT> *parent,
                                                                            const BoardNode<SymT> *node) {
  if (node != nullptr)
    std::println("Non-backtracking search, parent = ({0},{1}): {2}", 
                  (*node).row_idx, (*node).col_idx, (char)(*node).sym);
  else
    std::println("Non-backtracking search, parent = NULL");
  this->board.print_board(this->board.rows);
  while (this->board.sym_found) {
    this->board.sym_found = false;

    for (size_t box_row_idx = 0; box_row_idx < this->board.num_dims; ++box_row_idx)
      for (size_t box_idx = 0; box_idx < this->board.num_dims; ++box_idx) {
        const auto test_box_res = test_box(box_row_idx, box_idx, node);
        if (test_box_res.has_value())
          continue;
        if (parent != nullptr && node != nullptr && this->board.parent_caused_fill(*parent, *node))
          return std::unexpected(parse_board_err::invalid_parent);
        return std::unexpected(test_box_res.error());
      }
  }
  std::println("Non-backtracking search done");
  this->board.print_board(this->board.rows);
  std::println("Guessed syms");
  print_guessed_syms();

  return {};
}

template<BoardSymbol SymT>
bool Solver<SymT>::solve() {
  // have vectors for rows and columns
  // create matrix from board for now
  // ideally just make a matrix from the string after
  this->board.setup_board();
  this->board.print_board(this->board.rows);
  const auto non_backtracking_res = non_backtracking_search(nullptr, nullptr);
  if (!non_backtracking_res.has_value())
    return false;
  if (this->board.is_solved())
    return true;

  std::println("\n\n\n\n\n***** BACKTRACKING *****\n\n\n\n\n");
  this->board.set_first_backtracking_node(get_first_backtracking_node());
  const auto backtracking_res = guess_sym(this->board.first_backtracking_node, true);
  if (!backtracking_res.has_value()) {
    const auto error_node = backtracking_res.error();
    std::println("Backtracking failed with BoardNode ({0},{1})", error_node.row_idx, error_node.col_idx);
  } else
    std::println("Backtracking returned with {0}", (backtracking_res.value() == true));
  
  this->board.print_board(this->board.rows);

  return this->board.is_solved();
}

// assume operating on this->board.boxes in solve()
template<BoardSymbol SymT>
std::string Solver<SymT>::write_answer() {
  const auto board_str = this->board.boxes_to_str();
  std::ofstream outfile(this->result_file);
  outfile << board_str;
  outfile.close();
  return "Written result to " + this->result_file;
}
} // namespace Sudoku