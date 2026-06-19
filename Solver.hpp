#pragma once
#include "Board.hpp"


namespace Sudoku {

template<BoardSymbol SymT>
struct BoardNode {
  SymT sym;
  size_t row_idx;
  size_t col_idx;
};


template<BoardSymbol SymT>
class Solver { // later create Board and BoardFactory classes (factory can be more complex after)
private:
  Sudoku::Board<SymT> board;
  bool sym_found = true;
  std::string result_file;
  std::set<BoardNode<SymT>> guessed_syms;

  // update this to check if any other unknown syms in box/row/col have no valid syms left
  std::set<SymT> idx_valid_syms(const size_t row_idx, const size_t col_idx) {
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

  std::set<SymT> idx_valid_options(const size_t row_idx, const size_t col_idx) {
    auto valid_options = idx_valid_syms(row_idx, col_idx);
    std::print("({0},{1}) initial valid_options = ", row_idx, col_idx);
    this->board.print_set(valid_options);

    const auto &[box_indices, box_sym_idx] = board.row_col_idx_to_box_idx(row_idx, col_idx);
    const auto &[box_row_idx, box_idx] = box_indices;

    // replace this with a check over every box on the row or col
    // can result in 1 or 0 valid syms left for e.g. box below
    // just check all the boxes since rows and cols are included

    // if other unknown sym in box/row/col has 1 valid sym that would be 
    // taken by this guess, then it's not a valid option
    std::println("Checking over boxes in the same box row");
    for (size_t other_box_idx = 0; other_box_idx < this->board.num_dims; ++other_box_idx) {
      const auto box = this->board.boxes.at(box_row_idx).at(other_box_idx);
      for (const auto &[other_idx, other_sym] : std::views::enumerate(box)) {
        const auto &[other_row_idx, other_col_idx] = board.box_idx_to_row_col_idx(box_row_idx, other_box_idx, other_idx);
        if ((other_row_idx == row_idx && other_col_idx == col_idx) || other_sym != board.SYM_UNKNOWN)
          continue;

        std::print("({0},{1}) ", other_row_idx, other_col_idx);
        check_sole_valid_sym(other_row_idx, other_col_idx, valid_options);
        if (valid_options.size() == 0) {
          std::println("valid_options empty");
          return valid_options;
        }
      }
    }
    std::println();

    std::println("Checking over boxes in the same box col");
    for (size_t other_box_row_idx = 0; other_box_row_idx < this->board.num_dims; ++other_box_row_idx) {
      if (box_row_idx == other_box_row_idx)
        continue;

      const auto box = this->board.boxes.at(other_box_row_idx).at(box_idx);
      for (const auto &[other_idx, other_sym] : std::views::enumerate(box)) {
        const auto &[other_row_idx, other_col_idx] = board.box_idx_to_row_col_idx(other_box_row_idx, box_idx, other_idx);
        if ((other_row_idx == row_idx && other_col_idx == col_idx) || other_sym != board.SYM_UNKNOWN)
          continue;

        std::print("({0},{1}) ", other_row_idx, other_col_idx);
        check_sole_valid_sym(other_row_idx, other_col_idx, valid_options);
        if (valid_options.size() == 0) {
          std::println("valid_options empty");
          return valid_options;
        }
      }
    }
    std::println();

    return valid_options;
  }

  void check_sole_valid_sym(const size_t row_idx, const size_t col_idx, std::set<SymT> &valid_options) {
    const auto other_valid_syms = idx_valid_syms(row_idx, col_idx);
    if (other_valid_syms.size() != 1)
      return;

    const SymT other_only_valid_sym = *other_valid_syms.begin();
    if (valid_options.contains(other_only_valid_sym)) {
      std::println("\nerasing {0} from valid_options", (char) other_only_valid_sym);
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

  // TODO: if 1 sym left in box/row/col check, prune branch if invalid (dp)
  // e.g. hard.txt has bottom-right sym in top-left box ? when it should be pruned
  // when detect invalid sym, must check if any substituted syms caused this
  // must add to the guessed_syms state and find a way of recursing back to there
  // use std::unexpected with the value of the failure-causing sym
  // but how to know 
  // TODO: keep state of valid_syms for each (row, col)
  // every time you add a symbol check if valid syms in box/row/col are empty
  std::expected<bool, BoardNode<SymT>> 
  guess_sym(const BoardNode<SymT> parent, bool using_row=true) {
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
        std::println("parent: ({0},{1}), test_sym = {2}", parent.row_idx, parent.col_idx, static_cast<char>(test_sym));
        board.add_row_sym(test_sym, sym_row_idx, sym_col_idx, true);
        auto node = BoardNode<SymT>{test_sym, sym_row_idx, sym_col_idx};
        std::println("node = {0}, ({1},{2})", (char)node.sym, node.row_idx, node.col_idx);
        const auto sym_valid_val = guess_sym(node, !using_row);
        if (!sym_valid_val.has_value()) {
          std::println("node ({0},{1}) is invalid", node.row_idx, node.col_idx);
          this->board.del_sym(node.sym, node.row_idx, node.col_idx);
          continue;
        }
        const bool sym_valid = sym_valid_val.value();
        if (sym_valid)
          return true;
      }
    }

    this->board.del_sym(parent.sym, parent.row_idx, parent.col_idx);
    return false;
  }

  std::expected<void, parse_board_err> 
  test_box(const size_t box_row_idx, const size_t box_idx) {
    std::println("\ntest_box");
    BoxT<SymT> box = this->board.boxes[box_row_idx][box_idx]; // have to update board, rows, cols
    this->board.print_group(box);
    BoxSetT<SymT> box_set = this->board.box_row_sets[box_row_idx][box_idx];

    const size_t sym_count = box_set.size();
    if (sym_count == this->board.num_symbols) {
      std::println("Returning");
      return {};
    }

    auto unknown_sym_indices = this->board.unknown_symbols_indices(box);
    std::print("unknown_sym_indices = ");
    for (const auto idx : unknown_sym_indices) {
      std::print("{0} ", idx);
    }
    std::println();
    
    if (sym_count == this->board.num_symbols - 1) {
      const SymT last_unknown_sym = unknown_sym_indices.at(0);
      const size_t box_sym_idx = *std::find(box.begin(), box.end(), last_unknown_sym);
      this->board.add_box_sym(last_unknown_sym, box_row_idx, box_idx, box_sym_idx, true);
      this->board.sym_found = true;
    }
    
    std::println("Checking each unknown index");

    // ***** FIX FOR LOGIC ERROR *****
    /*
    * Create 2 num_dims-long vector of std::set for rows and cols
    * BEFORE ITERATING OVER EACH SYM fill this out
    * Assign each set to the intersection of the row/col sym set and sym_options
    * This shows the symbols that cannot be in that row/col
    * THEN, for each symbol take the intersection of each row AND COLUMN off-index
    * If more than 1 symbol parse_board_err::invalid_logic
    * If 0 symbols continue
    * If 1 symbol add_box_sym
    * 
    * For each box:
    *   - for each unknown sym index, get all possible syms
    *     - start with the set of unknown syms
    *     - for the corresponding row and column get intersections with box's unknown syms
    *       - i.e. the symbols not in the box that are in the row or column
    *     - get the union of row and columns sets
    *       - i.e. the set of symbols that cannot be in that square
    *     - get the difference between the set of unknown symbols and this union
    *       - i.e. the possible symbols in that square
    *   - for each unknown sym index, get the possible syms at that index
    *     - take union of other unknown sym indices
    *     - get set difference of the intersection of this sym index with the union
    *     - if set size 0 logic_error, if 1 add in, if > 1 continue
    * 
    * 1 2 3
    * X Y 6 .... 4
    * 7 8 Z 
    *     :
    *     :
    *     9
    * {5,9},{5,9},{4,5}
    * X: {5,9}-({5,9}&{4,5,9})={5,9}-{5,9}={}
    * Y: {5,9}-({5,9}&{4,5,9})={5,9}-{5,9}={}
    * Z: {4,5}-({4,5}&{5,9})={4,5}-{5}={4}
    */

    // For each missing symbol start with set of possible symbols for the box
    // Subtract the intersection of symbols for rows/cols not connected
    // If you get one result at the end add_box_sym (we start with > 1 syms)
    // If you get zero results return invalid_logic
    // Otherwise ignore for now, later create sets for each symbols per box per box row
    // try to do each row + col, then over all syms later
    // create sets for each unknown symbol (rly have sets for all symbols already)
    // for each box, group by rows and cols
    // for rows/cols, group by boxes
    // this is O(3n)=O(n) instead of O(n^2)
    // also box_syms could be fixed here and removed from if a sym is found

    const auto unordered_box_syms = this->board.box_row_sets.at(box_row_idx).at(box_idx);
    const auto box_syms = std::set<SymT>(unordered_box_syms.begin(), unordered_box_syms.end());
    std::print("box_syms = ");
    this->board.print_group(box_syms);

    std::set<SymT> unknown_syms{};
    std::set_difference(this->board.symbols.begin(), this->board.symbols.end(), box_syms.begin(), 
                        box_syms.end(), std::inserter(unknown_syms, unknown_syms.end()));
    std::print("unknown_syms (size={0}): ", std::to_string(unknown_syms.size()));
    this->board.print_group(unknown_syms);

    std::vector<std::set<SymT>> unknown_boxes_syms{};

    // These can be created once outside the while loop and then removed from if a sym is found
    std::println("Creating unknown sym sets");
    for (const size_t box_sym_idx : unknown_sym_indices) {
      std::println("box_sym_idx: {0}", std::to_string(box_sym_idx));

      const auto [row_idx, col_idx] = this->board.box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
      std::println("row_idx = {0}, col_idx = ", row_idx, col_idx);
      
      const auto row_syms = this->board.row_sets.at(row_idx);
      std::print("row_syms = ");
      this->board.print_set(row_syms);
      const auto col_syms = this->board.col_sets.at(col_idx);
      std::print("col_syms = ");
      this->board.print_set(col_syms);

      std::vector<SymT> row_invalid_syms;
      std::set_intersection(unknown_syms.begin(), unknown_syms.end(), row_syms.begin(), 
                            row_syms.end(), std::back_inserter(row_invalid_syms));
      std::print("row_invalid_syms = ");
      this->board.print_set(row_invalid_syms);

      std::vector<SymT> col_invalid_syms;
      std::set_intersection(unknown_syms.begin(), unknown_syms.end(), col_syms.begin(), 
                            col_syms.end(), std::back_inserter(col_invalid_syms));
      std::print("col_invalid_syms = ");
      this->board.print_set(col_invalid_syms);

      std::vector<SymT> idx_invalid_syms;
      std::set_union(row_invalid_syms.begin(), row_invalid_syms.end(), 
                      col_invalid_syms.begin(), col_invalid_syms.end(), 
                      std::back_inserter(idx_invalid_syms));
      std::print("idx_invalid_syms = ");
      this->board.print_set(idx_invalid_syms);

      std::vector<SymT> idx_valid_syms;
      std::set_difference(unknown_syms.begin(), unknown_syms.end(), 
                          idx_invalid_syms.begin(), idx_invalid_syms.end(), 
                          std::back_inserter(idx_valid_syms));
      std::print("idx_valid_syms = ");
      this->board.print_set(idx_valid_syms);
      const auto idx_valid_syms_set = std::set<SymT>(idx_valid_syms.begin(), 
                                                        idx_valid_syms.end());

      unknown_boxes_syms.push_back(idx_valid_syms_set);
    }

    // TODO: investigate bug for top-right box, thinks row 1 has a 4 in it
    // logs show it didn't add a 4 in the row but it thinks the row has a 4 in it
    // row_syms = 1 2 4 8 9, col_syms = 5 6 

    std::println("Unknown sym sets:");
    for (const auto unknown_box_syms : unknown_boxes_syms)
      this->board.print_set(unknown_box_syms);
    this->board.print_set(this->board.row_sets.at(1));
    this->board.print_box(box);

    // need to update box_syms in case a symbols got added
    bool box_updated = true;
    while (box_updated) {
      box_updated = false;

      std::println("Trying to find symbols");
      for (const auto [unknown_sym_idx, box_sym_idx] : std::views::enumerate(unknown_sym_indices)) {
        std::println("unknown_sym_idx = {0}, box_sym_idx = {1}", unknown_sym_idx, box_sym_idx);
        const auto unknown_box_syms = unknown_boxes_syms.at(unknown_sym_idx);
        std::print("unknown_box_syms = ");
        this->board.print_set(unknown_box_syms);

        if (unknown_box_syms.size() == 1) {
          const SymT found_sym = static_cast<SymT>(*unknown_box_syms.begin());
          std::println("Index {0} must be {1}", unknown_sym_idx, static_cast<char>(found_sym));
          // this logic is repeated below, put in a function later?
          unknown_syms.erase(unknown_syms.find(found_sym));
          unknown_boxes_syms.erase(unknown_boxes_syms.begin() + unknown_sym_idx);
          for (auto box_idx = 0; box_idx < unknown_boxes_syms.size(); ++box_idx)
            unknown_boxes_syms.at(box_idx).erase(found_sym);
          for (auto unknown_box_sym : unknown_boxes_syms)
            this->board.print_set(unknown_box_sym);
          std::println("Calling add_box_sym, box_sym_idx = {0}", box_sym_idx);
          this->board.add_box_sym(found_sym, box_row_idx, box_idx, box_sym_idx, true);
          this->board.sym_found = true;
          unknown_sym_indices.erase(unknown_sym_indices.begin() + unknown_sym_idx);
          box_updated = true;
          std::println("Added symbol {0}, trying to update box again", (char)found_sym);
          break;
        }

        std::set<SymT> other_boxes_valid_syms;
        std::println("Getting other_boxes_valid_syms");
        for (const auto [other_unknown_sym_idx, other_box_sym_idx] : std::views::enumerate(unknown_sym_indices)) {
          if (box_sym_idx == other_box_sym_idx) continue;

          std::println("other_unknown_sym_idx = {0}, other_box_sym_idx = {1}", other_unknown_sym_idx, other_box_sym_idx);
          const auto other_unknown_box_syms = unknown_boxes_syms.at(other_unknown_sym_idx);
          std::print("other_unknown_box_syms = ");
          this->board.print_set(other_unknown_box_syms);

          for (const SymT invalid_sym : other_unknown_box_syms)
            other_boxes_valid_syms.insert(invalid_sym);
        }
        std::print("other_boxes_valid_syms = ");
        this->board.print_set(other_boxes_valid_syms);

        std::vector<SymT> invalid_syms;
        std::set_intersection(unknown_box_syms.begin(), unknown_box_syms.end(), 
                              other_boxes_valid_syms.begin(), other_boxes_valid_syms.end(), 
                              std::back_inserter(invalid_syms));
        std::print("invalid_syms = ");
        this->board.print_set(invalid_syms);

        std::vector<SymT> valid_syms;
        std::print("unknown_box_syms = ");
        this->board.print_set(unknown_box_syms);
        std::set_difference(unknown_box_syms.begin(), unknown_box_syms.end(), 
                            invalid_syms.begin(), invalid_syms.end(), 
                            std::back_inserter(valid_syms));
        std::print("valid_syms (size={0}) = ", valid_syms.size());
        this->board.print_set(valid_syms);

        if (valid_syms.size() > 1)
          return std::unexpected(parse_board_err::invalid_logic);
        if (valid_syms.size() == 0)
          continue;

        const SymT found_sym = valid_syms.at(0);
        if (!unknown_syms.contains(found_sym))
          return std::unexpected(parse_board_err::invalid_logic);

        unknown_syms.erase(found_sym);
        unknown_boxes_syms.erase(unknown_boxes_syms.begin() + unknown_sym_idx);
        for (auto box_idx = 0; box_idx < unknown_boxes_syms.size(); ++box_idx)
          unknown_boxes_syms.at(box_idx).erase(found_sym);
        for (auto unknown_box_sym : unknown_boxes_syms)
          this->board.print_set(unknown_box_sym);
        this->board.add_box_sym(found_sym, box_row_idx, box_idx, box_sym_idx, true);
        unknown_sym_indices.erase(unknown_sym_indices.begin() + unknown_sym_idx);
        box_updated = true;
        std::println("Added symbol {0}, trying to update box again", (char)found_sym);
        break;
      }
    }

    // exit(EXIT_FAILURE);

    return {};
  }

public:
  Solver(const char *num_dims, const char *board_file, const char *outfile)
    : board(num_dims, board_file), result_file(outfile) {}
  
  bool solve() { // later find a way of testing numbers for intermediate/advanced sudokus
    // have vectors for rows and columns
    // create matrix from board for now
    // ideally just make a matrix from the string after
    this->board.setup_board();
    this->board.print_board(this->board.rows);
    this->board.print_board(this->board.cols);
    std::println("Printing box_row_sets");
    for (const auto &box_row_set : this->board.box_row_sets)
      for (const auto &box_set : box_row_set)
        this->board.print_set(box_set);
    std::println("Printing row_sets");
    for (const auto &row_set : this->board.row_sets)
      this->board.print_set(row_set);
    std::println("Printing col_sets");
    for (const auto &col_set : this->board.col_sets)
      this->board.print_set(col_set);
    /*
    * Check all boxes, then all rows, then all columns
    * If no change return check_solved
    * For any change, update state in box/row/column and check other 2 structures
    * If just one missing symbol missing insert that
    * Otherwise create vector of indices of missing symbols
    * For each missing symbol check perpendicular rows/cols
    */
    while (this->board.sym_found) {
      this->board.sym_found = false;

      for (size_t box_row_idx = 0; box_row_idx < this->board.num_dims; ++box_row_idx)
        for (size_t box_idx = 0; box_idx < this->board.num_dims; ++box_idx) {
          if (!test_box(box_row_idx, box_idx).has_value())
            return false; // return std::unexpected but don't use it here...
          this->board.print_box(this->board.boxes.at(box_row_idx).at(box_idx));
        }
    }

    if (this->board.is_solved())
      return true;

    std::println("***** BACKTRACKING ***");
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
      start_node = BoardNode<SymT>{start_sym, start_row_idx, start_col};
    } while (!start_row_found);
    
    const auto backtracking_res = guess_sym(start_node, true);
    if (!backtracking_res.has_value()) {
      const auto error_node = backtracking_res.error();
      std::println("Backtracking failed with BoardNode ({0},{1})", error_node.row_idx, error_node.col_idx);
    } else
     std::println("Backtracking returned with {0}", (backtracking_res.value() == true));
    
    this->board.print_board(this->board.rows);

    return this->board.is_solved();
  }

  // assume operating on this->board.boxes in solve()
  std::string write_answer() {
    const auto board_str = this->board.boxes_to_str();
    std::ofstream outfile(this->result_file);
    outfile << board_str;
    outfile.close();
    return "Written result to " + this->result_file;
  }
};
} // namespace Sudoku