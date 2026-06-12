#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <print>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>


namespace Sudoku {

enum class parse_board_err
{
  invalid_char,
  invalid_row,
  invalid_box,
  invalid_sym_num, // make too_many_syms/too_few_syms later, e.g. 17 min symbols for 3x3
  // bear in mind having 17 or even more symbols doesn't guarantee a unique (well-formed) solution
  noncontinuous_syms,
  invalid_logic,
  invalid_total_len
};


template <typename SymT> // use concepts later
class Sudoku { // later create Board and BoardFactory classes (factory can be more complex after)
private:
  static constexpr size_t MIN_DIMS = 2;
  static constexpr size_t MIN_NUM_SYMBOLS = MIN_DIMS * MIN_DIMS;
  static constexpr size_t MAX_NUM_SYMBOLS = std::numeric_limits<SymT>::max();

  static constexpr char OPEN_BRACE = '[';
  static constexpr char CLOSE_BRACE = ']';
  static constexpr char SYM_SEPARATOR = ',';
  static constexpr char SYM_UNKNOWN = 'X';

  bool sym_found = true;

  size_t num_dims;
  size_t num_symbols;

  size_t num_file_chars;
  size_t num_row_chars;
  size_t num_box_chars;

  using BoxT = std::vector<SymT>;
  using RowT = std::vector<SymT>;
  using ColT = std::vector<SymT>;
  using BoxRowT = std::vector<BoxT>;
  using BoardT = std::vector<BoxRowT>;

  // use std::mdspan or smthn else later to get views across 1D array for rows/cols/boxes
  BoardT board;
  std::vector<RowT> rows;
  std::vector<ColT> cols;
  std::set<SymT> symbols{};

  using BoxSetT = std::set<SymT>;
  using BoxRowSetT = std::vector<BoxSetT>;
  using RowSetT = std::set<SymT>;
  using ColSetT = std::set<SymT>;

  std::vector<BoxRowSetT> box_row_sets;
  std::vector<RowSetT> row_sets;
  std::vector<ColSetT> col_sets;

  std::string result_file;

  void parse_board_dim(const char *board_dim_arg) {
    const std::string_view board_dim_input{board_dim_arg};

    const auto [ptr, ec] = std::from_chars(board_dim_input.data(), board_dim_input.data() + board_dim_input.size(), this->num_dims);
    if (ec == std::errc::invalid_argument || ptr != board_dim_input.data() + board_dim_input.size()) {
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: n must be an integer\n");
    }
    
    const size_t max_dims = floor(sqrt(MAX_NUM_SYMBOLS));
    if (ec == std::errc::result_out_of_range || this->num_dims < MIN_DIMS || this->num_dims > max_dims) {
      throw std::runtime_error(std::string("Usage: ./sudoku <n> <infile> [outfile]: 2 <= n <= ") + std::to_string(max_dims) + '\n');
    }

    this->num_symbols = this->num_dims * this->num_dims;
  }

  std::string parse_sudoku_file(const char *file_path_arg) {
    const std::filesystem::path path{file_path_arg};
    if (!std::filesystem::exists(path)) {
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile does not exist\n");
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile cannot be opened\n");
    }

    std::string board_str;
    if (!std::getline(file, board_str)) {
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile is empty\n");
    }

    // for m dimensions, n symbols, where n=m^2
    // n*(m^2)=m^4 symbols overall
    // ((n-1)*(m^2))+((m^2)-1)=m^4 -m^2 + m^2 - 1 = m^4 - 1 commas overall
    // m*(2m + 2) + 2 = 2m^2 + 2m + 2 brackets overall
    // overall: m^4 + m^4 - 1 + 2m^2 + 2m + 2 = 2m^4 + 2m^2 + 2m + 1
    this->num_file_chars = (2 * pow(this->num_dims, 4)) + (2 * pow(this->num_dims, 2)) + (2 * this->num_dims) + 1;
    if (this->num_file_chars != board_str.length()) {
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile is not of the correct length\n");
    }
    // box_row is m*(2m^2 + 1) boxes + (m-1) commas + 2 brackets = 2m^3 + 2m + 1
    this->num_row_chars = (2 * pow(this->num_dims, 3)) + (2 * this->num_dims) + 1;
    // box is n symbols, n-1 commas, 2 brackets, so 2n+1 -> 2m^2 + 1
    this->num_box_chars = (2 * pow(this->num_dims, 2)) + 1;

    return board_str;
  }

  // already assuming correct length
  // make functions for parsing rows and boxes
  std::expected<BoardT, parse_board_err> parse_board_str(const std::string_view board_view) {
    BoardT board;
    // find a way of removing this var later?
    std::size_t board_idx = 0;

    if (board_view.at(board_idx) != OPEN_BRACE) {
      return std::unexpected(parse_board_err::invalid_char);
    }
    ++board_idx;

    for (std::size_t row_idx = 0; row_idx < this->num_dims; ++row_idx) {
      auto box_row = parse_row_str(board_view.substr(board_idx, this->num_row_chars));
      if (!box_row.has_value()) {
        return std::unexpected(box_row.error());
      }
      board.emplace_back(std::move(box_row.value()));
      
      board_idx += this->num_row_chars;
      ++board_idx; // skip comma separating next box_row or board CLOSE_BRACE
    }

    if (board_idx != this->num_file_chars)
      return std::unexpected(parse_board_err::invalid_total_len);

    // some advanced or small (i.e. 2x2) sudokus may have some symbols not appear
    if (this->symbols.size() > this->num_symbols)
      return std::unexpected(parse_board_err::invalid_sym_num);
    if (this->symbols.size() < this->num_symbols) {
      const auto syms_continuous = guess_unknown_symbols();
      if (!syms_continuous.has_value())
        return std::unexpected(parse_board_err::noncontinuous_syms);
    }

    return board;
  }

  // could generalise logic here and call everything from parse_board_str
  // recursive function checking opening [, for loop recursive call, closing ],
  // also possibly check total len, but the function should be correct so shouldn't need to
  // takes string view substring, amount to create substring later (how to vary this?)
  // later get rows and cols here directly instead of boxes (would have to make a func for getting boxes)
  std::expected<BoxRowT, parse_board_err> parse_row_str(const std::string_view row_view) {
    BoxRowT box_row;
    size_t row_idx = 0;

    if (row_view.at(row_idx) != OPEN_BRACE) {
      return std::unexpected(parse_board_err::invalid_row);
    }
    ++row_idx;

    for (std::size_t box_idx = 0; box_idx < this->num_dims; ++box_idx) {
      auto box = parse_box_str(row_view.substr(row_idx, this->num_box_chars));
      if (!box.has_value()) {
        return std::unexpected(parse_board_err::invalid_box); // no invalid_row errors now
      }
      box_row.emplace_back(std::move(box.value()));

      row_idx += this->num_box_chars; // skip box
      ++row_idx; // skip comma separating next box or box_row CLOSE_BRACE
    }

    if (row_idx != this->num_row_chars) {
      return std::unexpected(parse_board_err::invalid_total_len);
    }


    return box_row;
  }

  // have to detect '?' chars
  // assume given subview for [X,...,num_symbols] num_box_chars long
  // assume symbols are all single chars for now
  // return std::expected and remove box_row/box errors and add comma/sym errors?
  std::optional<BoxT> parse_box_str(const std::string_view box_view) {
    BoxT box;
    std::unordered_set<SymT> box_symbols;
    // find a way of making this a const
    std::regex hex("ABCDEF", std::regex_constants::ECMAScript | std::regex_constants::icase);

    const auto box_len = box_view.length();
    for (size_t sym_idx = 0; sym_idx < box_len; ++sym_idx) {
      const SymT box_val = box_view.at(sym_idx);
      const char box_char = static_cast<char>(box_val); // overflow issue? if sizeof(SymT) > sizeof(char) could lap around?
      if (sym_idx == 0) {
        if (box_char != OPEN_BRACE)
          return std::nullopt;
        continue;
      } 
      if (sym_idx == box_len - 1) {
        if (box_char != CLOSE_BRACE)
          return std::nullopt;
        continue;
      } 
      if (sym_idx % 2 == 0) {
        if (box_char != SYM_SEPARATOR)
          return std::nullopt; // all odd indices will hold values
        continue;
      } 
      
      if (box_symbols.contains(box_val) && box_char != SYM_UNKNOWN) {
        return std::nullopt;
      } else if (box_val != SYM_UNKNOWN && !std::isdigit(box_val) && 
                  !std::regex_search(std::string{box_char}, hex)) {
        return std::nullopt; // later don't just accept numbers and hex
      }

      box.emplace_back(box_val);
      box_symbols.insert(box_val);
      if (box_char != SYM_UNKNOWN)
        this->symbols.insert(box_val);
    }

    return box;
  }

  std::expected<void, parse_board_err> guess_unknown_symbols() {
    // iterate over symbols and fill in gaps
    // if num_unknown_symbols 0 return noncontinuous_syms
    // decrement num_unknown_symbols and add to this->symbols
    // if num_unknown_symbols > 0 add symbols down to 0 exclusive
    // if num_unknown_symbols > 0 add symbols up until if num_unknown_symbols 0
    size_t num_unknown_symbols = this->num_symbols - this->symbols.size();

    const SymT min_sym = *std::min_element(this->symbols.begin(), this->symbols.end());
    const SymT max_sym = *std::max_element(this->symbols.begin(), this->symbols.end());

    SymT sym = min_sym + 1;
    for (SymT sym = min_sym + 1; sym < max_sym; ++sym) {
      if (this->symbols.contains(sym))
        continue;

      if (num_unknown_symbols == 0 && sym == max_sym - 1)
        return std::unexpected(parse_board_err::noncontinuous_syms);
      
      this->symbols.insert(sym);
      --num_unknown_symbols;
    }

    sym = min_sym - 1;
    while (num_unknown_symbols > 0 && sym > 0) {
      this->symbols.insert(sym);
      --num_unknown_symbols;
      --sym;
    }

    sym = max_sym + 1;
    while (num_unknown_symbols > 0) {
      this->symbols.insert(sym);
      --num_unknown_symbols;
      ++sym;
    }

    return {};
  }

  // assuming board filled in
  std::string board_to_str() {
    std::ostringstream oss;
    oss << '[';
    for (const auto &[row_idx, box_row] : std::views::enumerate(this->board)) {
      oss << '[';
      for (const auto &[box_idx, box] : std::views::enumerate(box_row)) {
        oss << '[';
        // std::copy(box.begin(), box.end() - 1, std::ostream_iterator<SymT>(oss, ","));
        // oss << box.back();
        for (const auto &[sym_idx, sym] : std::views::enumerate(box)) {
          oss << static_cast<char>(sym); // overflow bug, change with support multi-char symbol support
          if (sym_idx != this->num_symbols - 1)
            oss << ',';
        }
        oss << ']';
        if (box_idx != this->num_dims - 1)
          oss << ',';
      }
      oss << ']';
      if (row_idx != this->num_dims - 1)
          oss << ',';
    }
    oss << ']';

    return oss.str(); // this probably makes a copy, find a better way later
  }

  void board_to_rows() {
    this->rows = std::vector(this->num_symbols, std::vector<SymT>(0, 0)); // fix 0 elt vector

    for (const auto &[box_row_idx, box_row] : std::views::enumerate(this->board)) {
      for (size_t dim_idx = 0; dim_idx < this->num_dims; ++dim_idx) {
        for (const auto &[box_idx, box] : std::views::enumerate(box_row)) {
          const size_t row_idx = (box_row_idx * this->num_dims) + dim_idx;
          const size_t sym_idx = (dim_idx * this->num_dims);
          const auto start_idx = box.cbegin() + sym_idx;
          const auto end_idx = start_idx + this->num_dims; // does + take into account data size?

          this->rows.at(row_idx).insert(this->rows.at(row_idx).end(), start_idx, end_idx);
        }
      }
    }
  }

  void rows_to_cols() {
    this->cols = std::vector(this->num_symbols, std::vector<SymT>(0, 0)); // fix 0 elt vector
    for (size_t col_idx = 0; col_idx < this->num_symbols; ++col_idx) {
      for (const RowT &row : this->rows) {
        this->cols.at(col_idx).emplace_back(row.at(col_idx));
      }
    }
  }

  void initialize_box_row_sets() {
    this->box_row_sets = std::vector(this->num_dims, std::vector<std::set<SymT>>(this->num_dims));

    for (const auto &[box_row_idx, box_row] : std::views::enumerate(this->board)) {
      for (const auto &[box_idx, box] : std::views::enumerate(box_row)) {
        this->box_row_sets.at(box_row_idx).at(box_idx) = std::set<SymT>(box.begin(), box.end());
        this->box_row_sets.at(box_row_idx).at(box_idx).erase(SYM_UNKNOWN);
      }
    }
  }

  void initialize_row_sets() {
    this->row_sets = std::vector<std::set<SymT>>(this->num_symbols);

    for (const auto &[row_idx, row] : std::views::enumerate(this->rows)) {
      this->row_sets.at(row_idx) = std::set<SymT>(row.begin(), row.end());
      this->row_sets.at(row_idx).erase(SYM_UNKNOWN);
    }
  }

  void initialize_col_sets() {
    this->col_sets = std::vector<std::set<SymT>>(this->num_symbols);

    for (const auto &[col_idx, col] : std::views::enumerate(this->cols)) {
      this->col_sets.at(col_idx) = std::set<SymT>(col.begin(), col.end());
      this->col_sets.at(col_idx).erase(SYM_UNKNOWN);
    }
  }

  void print_set(const auto& set) {
    if (set.empty()) {
      std::println();
      return;
    }

    for (const auto& elem : set) {
      std::print("{0} ", static_cast<char>(elem));
    }
    std::println();
  }

  template <typename T>
  void print_board(std::vector<T> all_groups) {
    for (const auto &[idx, group] : std::views::enumerate(all_groups)) {
      if (idx % this->num_dims == 0) {
        for (size_t i = 1; i < 2 * this->num_symbols; ++i)
          std::print("-");
        std::println();
      }
      for (const auto &[sym_idx, sym] : std::views::enumerate(group))
        std::print("{0}{1}", static_cast<char>(sym), (((sym_idx + 1) % this->num_dims == 0) ? '|' : ' '));
      std::println();
    }
    for (size_t i = 1; i < 2 * this->num_symbols; ++i)
      std::print("-");
    std::println();
  }

  void print_box(BoxT box) {
    for (size_t i = 1; i < 2 * this->num_dims; ++i)
          std::print("-");
    std::println();
    for (const auto &[sym_idx, sym] : std::views::enumerate(box))
      std::print("{0}{1}", static_cast<char>(sym), (((sym_idx + 1) % this->num_dims == 0) ? '\n' : ' '));
    for (size_t i = 1; i < 2 * this->num_dims; ++i)
      std::print("-");
    std::println();
  }

  template<typename T>
  void print_group(T group) {
    for (const SymT elt : group)
      std::print("{0} ", static_cast<char>(elt));
    std::println();
  }

  std::pair<std::size_t, std::size_t> 
  box_idx_to_row_col_idx(const size_t box_row_idx, const size_t box_idx, const size_t box_sym_idx) {
    const size_t row_idx = (box_row_idx * this->num_dims) + (box_sym_idx / this->num_dims);
    const size_t col_idx = (box_idx * this->num_dims) + (box_sym_idx % this->num_dims);
    return std::make_pair(row_idx, col_idx);
  }

  std::pair<std::pair<std::size_t, std::size_t>, std::size_t> 
  row_col_idx_to_box_idx(const size_t row_idx, const size_t col_idx) {
    const size_t box_row_idx = row_idx / this->num_dims;
    const size_t box_row_box_idx = col_idx / this->num_dims;
    const size_t box_idx = box_row_idx + box_row_box_idx;
    const BoxT box = this->board.at(box_row_idx).at(box_row_box_idx);
    const size_t box_sym_idx = row_idx % this->num_dims + col_idx % this->num_dims;
    return std::make_pair(std::make_pair(box_row_idx, box_idx), box_sym_idx);
  }

  template<typename TGroup> // add concepts later
  std::vector<std::size_t> unknown_symbols_indices(TGroup group) {
    std::vector<std::size_t> unknown_sym_indices;
    for (const auto &[sym_idx, sym] : std::views::enumerate(group))
      if (sym == SYM_UNKNOWN)
        unknown_sym_indices.push_back(sym_idx);

    return unknown_sym_indices;
  }


  BoardT row_to_board() {
    return board;
  }

  bool full(const std::unordered_set<SymT> container) {
    return !container.contains(static_cast<SymT>(SYM_UNKNOWN)) && container.size() == this->num_symbols;
  }

  std::expected<void, parse_board_err> 
  test_box(const size_t box_row_idx, const size_t box_idx) {
    std::println("\ntest_box");
    BoxT box = this->board[box_row_idx][box_idx]; // have to update board, rows, cols
    print_group<BoxT>(box);
    BoxSetT box_set = this->box_row_sets[box_row_idx][box_idx];

    const size_t sym_count = box_set.size();
    if (sym_count == this->num_symbols) {
      std::println("Returning");
      return {};
    }

    auto unknown_sym_indices = unknown_symbols_indices<BoxT>(box);
    std::print("unknown_sym_indices = ");
    for (const auto idx : unknown_sym_indices) {
      std::print("{0} ", idx);
    }
    std::println();
    
    if (sym_count == this->num_symbols - 1) {
      const SymT last_unknown_sym = unknown_sym_indices.at(0);
      const size_t box_sym_idx = *std::find(box.begin(), box.end(), last_unknown_sym);
      add_box_sym(last_unknown_sym, box_row_idx, box_idx, box_sym_idx, true);
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

    const auto unordered_box_syms = this->box_row_sets.at(box_row_idx).at(box_idx);
    const auto box_syms = std::set<SymT>(unordered_box_syms.begin(), unordered_box_syms.end());
    std::print("box_syms = ");
    print_group<std::set<SymT>>(box_syms);

    std::set<SymT> unknown_syms{};
    std::set_difference(this->symbols.begin(), this->symbols.end(), box_syms.begin(), 
                        box_syms.end(), std::inserter(unknown_syms, unknown_syms.end()));
    std::print("unknown_syms (size={0}): ", std::to_string(unknown_syms.size()));
    print_group<std::set<SymT>>(unknown_syms);

    std::vector<std::set<SymT>> unknown_boxes_syms{};

    // These can be created once outside the while loop and then removed from if a sym is found
    std::println("Creating unknown sym sets");
    for (const size_t box_sym_idx : unknown_sym_indices) {
      std::println("box_sym_idx: {0}", std::to_string(box_sym_idx));

      const auto [row_idx, col_idx] = box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);
      std::println("row_idx = {0}, col_idx = ", row_idx, col_idx);
      
      const auto row_syms = this->row_sets.at(row_idx);
      std::print("row_syms = ");
      print_set(row_syms);
      const auto col_syms = this->col_sets.at(col_idx);
      std::print("col_syms = ");
      print_set(col_syms);

      std::vector<SymT> row_invalid_syms;
      std::set_intersection(unknown_syms.begin(), unknown_syms.end(), row_syms.begin(), 
                            row_syms.end(), std::back_inserter(row_invalid_syms));
      std::print("row_invalid_syms = ");
      print_set(row_invalid_syms);

      std::vector<SymT> col_invalid_syms;
      std::set_intersection(unknown_syms.begin(), unknown_syms.end(), col_syms.begin(), 
                            col_syms.end(), std::back_inserter(col_invalid_syms));
      std::print("col_invalid_syms = ");
      print_set(col_invalid_syms);

      std::vector<SymT> idx_invalid_syms;
      std::set_union(row_invalid_syms.begin(), row_invalid_syms.end(), 
                      col_invalid_syms.begin(), col_invalid_syms.end(), 
                      std::back_inserter(idx_invalid_syms));
      std::print("idx_invalid_syms = ");
      print_set(idx_invalid_syms);

      std::vector<SymT> idx_valid_syms;
      std::set_difference(unknown_syms.begin(), unknown_syms.end(), 
                          idx_invalid_syms.begin(), idx_invalid_syms.end(), 
                          std::back_inserter(idx_valid_syms));
      std::print("idx_valid_syms = ");
      print_set(idx_valid_syms);
      const auto idx_valid_syms_set = std::set<SymT>(idx_valid_syms.begin(), 
                                                        idx_valid_syms.end());

      unknown_boxes_syms.push_back(idx_valid_syms_set);
    }

    // TODO: investigate bug for top-right box, thinks row 1 has a 4 in it
    // logs show it didn't add a 4 in the row but it thinks the row has a 4 in it
    // row_syms = 1 2 4 8 9, col_syms = 5 6 

    std::println("Unknown sym sets:");
    for (const auto unknown_box_syms : unknown_boxes_syms)
      print_set(unknown_box_syms);
    print_set<RowSetT>(this->row_sets.at(1));
    print_box(box);

    // need to update box_syms in case a symbols got added
    bool box_updated = true;
    while (box_updated) {
      box_updated = false;

      std::println("Trying to find symbols");
      for (const auto [unknown_sym_idx, box_sym_idx] : std::views::enumerate(unknown_sym_indices)) {
        std::println("unknown_sym_idx = {0}, box_sym_idx = {1}", unknown_sym_idx, box_sym_idx);
        const auto unknown_box_syms = unknown_boxes_syms.at(unknown_sym_idx);
        std::print("unknown_box_syms = ");
        print_set(unknown_box_syms);

        if (unknown_box_syms.size() == 1) {
          const SymT found_sym = static_cast<SymT>(*unknown_box_syms.begin());
          std::println("Index {0} must be {1}", unknown_sym_idx, static_cast<char>(found_sym));
          // this logic is repeated below, put in a function later?
          unknown_syms.erase(unknown_syms.find(found_sym));
          unknown_boxes_syms.erase(unknown_boxes_syms.begin() + unknown_sym_idx);
          for (auto box_idx = 0; box_idx < unknown_boxes_syms.size(); ++box_idx)
            unknown_boxes_syms.at(box_idx).erase(found_sym);
          for (auto unknown_box_sym : unknown_boxes_syms)
            print_set(unknown_box_sym);
          std::println("Calling add_box_sym, box_sym_idx = {0}", box_sym_idx);
          add_box_sym(found_sym, box_row_idx, box_idx, box_sym_idx, true);
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
          print_set(other_unknown_box_syms);

          for (const SymT invalid_sym : other_unknown_box_syms)
            other_boxes_valid_syms.insert(invalid_sym);
        }
        std::print("other_boxes_valid_syms = ");
        print_set(other_boxes_valid_syms);

        std::vector<SymT> invalid_syms;
        std::set_intersection(unknown_box_syms.begin(), unknown_box_syms.end(), 
                              other_boxes_valid_syms.begin(), other_boxes_valid_syms.end(), 
                              std::back_inserter(invalid_syms));
        std::print("invalid_syms = ");
        print_set(invalid_syms);

        std::vector<SymT> valid_syms;
        std::print("unknown_box_syms = ");
        print_set(unknown_box_syms);
        std::set_difference(unknown_box_syms.begin(), unknown_box_syms.end(), 
                            invalid_syms.begin(), invalid_syms.end(), 
                            std::back_inserter(valid_syms));
        std::print("valid_syms (size={0}) = ", valid_syms.size());
        print_set(valid_syms);

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
          print_set(unknown_box_sym);
        add_box_sym(found_sym, box_row_idx, box_idx, box_sym_idx, true);
        unknown_sym_indices.erase(unknown_sym_indices.begin() + unknown_sym_idx);
        box_updated = true;
        std::println("Added symbol {0}, trying to update box again", (char)found_sym);
        break;
      }
    }

    // exit(EXIT_FAILURE);

    return {};
  }

  void add_box_sym(const SymT sym, const size_t box_row_idx, const size_t box_idx, 
                    const size_t box_sym_idx, bool root=false) {
    std::println("*** add_box_sym, sym = {0}, box_row_idx = {1}, box_idx = {2}, box_sym_idx = {3}, root = {4}",
                  (char)sym, box_row_idx, box_idx, box_sym_idx, root);
    this->board.at(box_row_idx).at(box_idx)[box_sym_idx] = sym;
    this->box_row_sets.at(box_row_idx).at(box_idx).insert(sym);
    this->sym_found = true;

    if (!root) return;
    
    const auto [row_idx, col_idx] = box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);

    add_row_sym(sym, row_idx, col_idx);
    add_col_sym(sym, col_idx, row_idx);
  }

  void add_row_sym(const SymT sym, const size_t row_idx, const size_t row_sym_idx, bool root=false) {
    std::println("*** add_row_sym, sym = {0}, row_idx = {1}, row_sym_idx = {2}, root = {3}", 
                  (char)sym, row_idx, row_sym_idx, root);
    this->rows.at(row_idx)[row_sym_idx] = sym;
    this->row_sets.at(row_idx).insert(sym);
    this->sym_found = true;

    if (!root) return;

    const auto [box_indices, box_sym_idx] = row_col_idx_to_box_idx(row_idx, row_sym_idx);
    const auto [box_row_idx, box_idx] = box_indices;
    
    add_col_sym(sym, row_sym_idx, row_idx);
    add_box_sym(sym, box_row_idx, box_idx, box_sym_idx);
  }

  void add_col_sym(const SymT sym, const size_t col_idx, const size_t col_sym_idx, bool root=false) {
    std::println("*** add_col_sym, sym = {0}, col_idx = {1}, col_sym_idx = {2}, root = {3}", 
                  (char)sym, col_idx, col_sym_idx, root);
    this->cols.at(col_idx)[col_sym_idx] = sym;
    this->col_sets.at(col_idx).insert(sym);
    this->sym_found = true;

    if (!root) return;

    const auto [box_indices, box_sym_idx] = row_col_idx_to_box_idx(col_sym_idx, col_idx);
    const auto [box_row_idx, box_idx] = box_indices;
    
    add_row_sym(sym, col_sym_idx, col_idx);
    add_box_sym(sym, box_row_idx, box_idx, box_sym_idx);
  }

  bool is_solved() {
    // check all rows sets are equal to the set of all symbols used
    // assume boxrows and cols sets/vectors and rows vector updated
    for (const RowT row : this->rows) {
      std::set<SymT> row_symbols{row.begin(), row.end()};
      if (!(this->symbols == row_symbols)) return false;
    }

    return true;
  }


public:
  Sudoku(const char *num_dims, const char *board_file, const char *outfile) {
    parse_board_dim(num_dims);
    this->result_file = outfile;
    const std::string &sudoku_str = parse_sudoku_file(board_file);
    
    if (const auto parsed_board = parse_board_str(sudoku_str); parsed_board.has_value())
      this->board = parsed_board.value();
    else if (parsed_board.error() == parse_board_err::invalid_char)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile contains an invalid character\n");
    else if (parsed_board.error() == parse_board_err::invalid_row)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile contains invalid box row format\n");
    else if (parsed_board.error() == parse_board_err::invalid_box)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile contains invalid box format\n");
    else if (parsed_board.error() == parse_board_err::invalid_sym_num)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile contains too many symbols\n");
    else if (parsed_board.error() == parse_board_err::noncontinuous_syms)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile contains non-continuous symbols\n");
    else if (parsed_board.error() == parse_board_err::invalid_total_len)
      throw std::runtime_error("Usage: ./sudoku <n> <infile> [outfile]: infile length is invalid\n");
  }
  
  // operate on this->board inplace
  bool solve() { // later find a way of testing numbers for intermediate/advanced sudokus
    // have vectors for rows and columns
    // create matrix from board for now
    // ideally just make a matrix from the string after
    board_to_rows();
    print_board<RowT>(this->rows);
    rows_to_cols();
    print_board<ColT>(this->cols);
    initialize_box_row_sets();
    initialize_row_sets();
    initialize_col_sets();
    std::println("Printing box_row_sets");
    for (const auto &box_row_set : this->box_row_sets)
      for (const auto &box_set : box_row_set)
        print_set(box_set);
    std::println("Printing row_sets");
    for (const auto &row_set : this->row_sets)
      print_set(row_set);
    std::println("Printing col_sets");
    for (const auto &col_set : this->col_sets)
      print_set(col_set);
    /*
    * Check all boxes, then all rows, then all columns
    * If no change return check_solved
    * For any change, update state in box/row/column and check other 2 structures
    * If just one missing symbol missing insert that
    * Otherwise create vector of indices of missing symbols
    * For each missing symbol check perpendicular rows/cols
    */
    while (this->sym_found) {
      this->sym_found = false;

      for (size_t box_row_idx = 0; box_row_idx < this->num_dims; ++box_row_idx)
        for (size_t box_idx = 0; box_idx < this->num_dims; ++box_idx) {
          if (!test_box(box_row_idx, box_idx).has_value())
            return false; // return std::unexpected but don't use it here...
          print_box(this->board.at(box_row_idx).at(box_idx));
        }
    }

    print_board<RowT>(this->rows);

    return is_solved();
  }

  // assume operating on this->board in solve()
  std::string write_answer() {
    const auto board_str = board_to_str();
    std::ofstream outfile(this->result_file);
    outfile << board_str;
    outfile.close();
    return "Written result to " + this->result_file;
  }
};
} // namespace Sudoku


int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: ./soduko <n> <infile> [outfile]" << '\n';
    return EXIT_FAILURE;
  }

  const char *outfile = (argc == 4) ? argv[3] : "result.txt";
  auto board = Sudoku::Sudoku<std::uint16_t>(argv[1], argv[2], outfile);
  bool solved = board.solve();
  std::println("{0}", (solved ? "Solved" : "Failed"));
  std::println("{0}", board.write_answer());

  return EXIT_SUCCESS;
}