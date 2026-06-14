#pragma once
#include "Board.hpp"


namespace Sudoku {

template<BoardSymbol SymT>
class Solver { // later create Board and BoardFactory classes (factory can be more complex after)
private:
  Sudoku::Board<SymT> board;
  bool sym_found = true;
  std::string result_file;

  std::expected<void, parse_board_err> 
  test_box(const size_t box_row_idx, const size_t box_idx) {
    std::println("\ntest_box");
    BoxT<SymT> box = this->board.boxes[box_row_idx][box_idx]; // have to update board, rows, cols
    this->board.template print_group<BoxT<SymT>>(box);
    BoxSetT<SymT> box_set = this->board.box_row_sets[box_row_idx][box_idx];

    const size_t sym_count = box_set.size();
    if (sym_count == this->board.num_symbols) {
      std::println("Returning");
      return {};
    }

    auto unknown_sym_indices = this->board.template unknown_symbols_indices<BoxT<SymT>>(box);
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

    /* Next step for hard sudokus is to guess the next numbers
    * Store 2D valid_syms sets to plug values in
    * Get probabilities and use dynamic programming or DFS to solve
    */

    const auto unordered_box_syms = this->board.box_row_sets.at(box_row_idx).at(box_idx);
    const auto box_syms = std::set<SymT>(unordered_box_syms.begin(), unordered_box_syms.end());
    std::print("box_syms = ");
    this->board.template print_group<std::set<SymT>>(box_syms);

    std::set<SymT> unknown_syms{};
    std::set_difference(this->board.symbols.begin(), this->board.symbols.end(), box_syms.begin(), 
                        box_syms.end(), std::inserter(unknown_syms, unknown_syms.end()));
    std::print("unknown_syms (size={0}): ", std::to_string(unknown_syms.size()));
    this->board.template print_group<std::set<SymT>>(unknown_syms);

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
    this->board.template print_set<Sudoku::RowSetT<SymT>>(this->board.row_sets.at(1));
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
    this->board.boxes_to_rows();
    this->board.template print_board<Sudoku::RowT<SymT>>(this->board.rows);
    this->board.rows_to_cols();
    this->board.template print_board<Sudoku::ColT<SymT>>(this->board.cols);
    this->board.initialize_box_row_sets();
    this->board.initialize_row_sets();
    this->board.initialize_col_sets();
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

    this->board.template print_board<Sudoku::RowT<SymT>>(this->board.rows);

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