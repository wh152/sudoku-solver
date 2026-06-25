#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
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
  invalid_parent,
  noncontinuous_syms,
  invalid_logic,
  invalid_total_len
};

template<typename T>
concept BoardSymbol = std::unsigned_integral<T>;

template<BoardSymbol SymT>
using BoxT = std::vector<SymT>;
template<BoardSymbol SymT>
using RowT = std::vector<SymT>;
template<BoardSymbol SymT>
using ColT = std::vector<SymT>;
template<BoardSymbol SymT>
using BoxRowT = std::vector<BoxT<SymT>>;
template<BoardSymbol SymT>
using BoxesT = std::vector<BoxRowT<SymT>>;
template<BoardSymbol SymT>
using BoxSetT = std::set<SymT>;
template<BoardSymbol SymT>
using BoxRowSetT = std::vector<BoxSetT<SymT>>;
template<BoardSymbol SymT>
using RowSetT = std::set<SymT>;
template<BoardSymbol SymT>
using ColSetT = std::set<SymT>;

using BoxLocT = std::tuple<size_t, size_t, size_t>;

template<BoardSymbol SymT>
struct BoardNode {
  size_t row_idx;
  size_t col_idx;
  SymT sym;

  auto operator<=>(const BoardNode &other) const = default;
};

template<BoardSymbol SymT>
using NodeMapT = std::map<BoardNode<SymT>, std::set<BoardNode<SymT>>>;

template<Sudoku::BoardSymbol SymT>
class Solver;

template<BoardSymbol SymT>
class Board {
  friend class Solver<SymT>;

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

  // use std::mdspan or smthn else later to get views across 1D array for rows/cols/boxes
  BoxesT<SymT> boxes;
  std::vector<RowT<SymT>> rows;
  std::vector<ColT<SymT>> cols;
  std::set<SymT> symbols{};

  std::vector<BoxRowSetT<SymT>> box_row_sets;
  std::vector<RowSetT<SymT>> row_sets;
  std::vector<ColSetT<SymT>> col_sets;

  NodeMapT<SymT> guessed_syms;

  BoardNode<SymT> first_backtracking_node;

  void parse_board_dim(const char *board_dim_arg);

  std::string parse_sudoku_file(const char *file_path_arg);

  std::expected<BoxesT<SymT>, parse_board_err>
  parse_board_str(const std::string_view board_view);

  std::expected<BoxRowT<SymT>, parse_board_err>
  parse_row_str(const std::string_view row_view);

  std::optional<BoxT<SymT>>
  parse_box_str(const std::string_view box_view);

  std::expected<void, parse_board_err>
  guess_unknown_symbols();

  std::string boxes_to_str();

  void boxes_to_rows();

  void rows_to_cols();

  void initialize_box_row_sets();

  void initialize_row_sets();

  void initialize_col_sets();

  void initialize_guessed_syms();

  void setup_board();

  void set_first_backtracking_node(const BoardNode<SymT> &first_node);

  bool parent_caused_fill(const BoardNode<SymT> &parent, const BoardNode<SymT> &child);

  void print_set(const auto& set);

  template <typename T>
  void print_board(std::vector<T> all_groups);

  void print_box(BoxT<SymT> box);

  template<typename T>
  void print_group(T group);

  const std::string show_node(const BoardNode<SymT> &node);

  std::pair<std::size_t, std::size_t> 
  box_idx_to_row_col_idx(const size_t box_row_idx, const size_t box_idx, const size_t box_sym_idx);

  std::pair<std::pair<std::size_t, std::size_t>, std::size_t> 
  row_col_idx_to_box_idx(const size_t row_idx, const size_t col_idx);

  template<typename TGroup> // add concepts later
  std::vector<std::size_t> unknown_symbols_indices(TGroup group);

  bool full(const std::set<SymT> &container);

  void add_box_sym(const SymT sym, const size_t box_row_idx, const size_t box_idx, 
                    const size_t box_sym_idx, bool root=false);

  void add_row_sym(const SymT sym, const size_t row_idx, const size_t row_sym_idx, bool root=false);

  void add_col_sym(const SymT sym, const size_t col_idx, const size_t col_sym_idx, bool root=false);

  void del_sym(const SymT sym, const size_t row_idx, const size_t col_idx);

  bool is_solved();

public:
  Board(const char *num_dims, const char *board_file);
};

template<BoardSymbol SymT>
void Board<SymT>::parse_board_dim(const char *board_dim_arg) {
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

template<BoardSymbol SymT>
std::string Board<SymT>::parse_sudoku_file(const char *file_path_arg) {
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
template<BoardSymbol SymT>
std::expected<BoxesT<SymT>, parse_board_err> Board<SymT>::parse_board_str(const std::string_view board_view) {
  BoxesT<SymT> boxes;
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
    boxes.emplace_back(std::move(box_row.value()));
    
    board_idx += this->num_row_chars;
    ++board_idx; // skip comma separating next box_row or boxes CLOSE_BRACE
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

  return boxes;
}

// could generalise logic here and call everything from parse_board_str
// recursive function checking opening [, for loop recursive call, closing ],
// also possibly check total len, but the function should be correct so shouldn't need to
// takes string view substring, amount to create substring later (how to vary this?)
// later get rows and cols here directly instead of boxes (would have to make a func for getting boxes)
template<BoardSymbol SymT>
std::expected<BoxRowT<SymT>, parse_board_err> Board<SymT>::parse_row_str(const std::string_view row_view) {
  BoxRowT<SymT> box_row;
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
template<BoardSymbol SymT>
std::optional<BoxT<SymT>> Board<SymT>::parse_box_str(const std::string_view box_view) {
  BoxT<SymT> box;
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

template<BoardSymbol SymT>
std::expected<void, parse_board_err> Board<SymT>::guess_unknown_symbols() {
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

// assuming boxes filled in
template<BoardSymbol SymT>
std::string Board<SymT>::boxes_to_str() {
  std::ostringstream oss;
  oss << '[';
  for (const auto &[row_idx, box_row] : std::views::enumerate(this->boxes)) {
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

template<BoardSymbol SymT>
void Board<SymT>::boxes_to_rows() {
  this->rows = std::vector(this->num_symbols, std::vector<SymT>(0, 0)); // fix 0 elt vector

  for (const auto &[box_row_idx, box_row] : std::views::enumerate(this->boxes)) {
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

template<BoardSymbol SymT>
void Board<SymT>::rows_to_cols() {
  this->cols = std::vector(this->num_symbols, std::vector<SymT>(0, 0)); // fix 0 elt vector
  for (size_t col_idx = 0; col_idx < this->num_symbols; ++col_idx) {
    for (const RowT<SymT> &row : this->rows) {
      this->cols.at(col_idx).emplace_back(row.at(col_idx));
    }
  }
}

template<BoardSymbol SymT>
void Board<SymT>::initialize_box_row_sets() {
  this->box_row_sets = std::vector(this->num_dims, std::vector<std::set<SymT>>(this->num_dims));

  for (const auto &[box_row_idx, box_row] : std::views::enumerate(this->boxes)) {
    for (const auto &[box_idx, box] : std::views::enumerate(box_row)) {
      this->box_row_sets.at(box_row_idx).at(box_idx) = std::set<SymT>(box.begin(), box.end());
      this->box_row_sets.at(box_row_idx).at(box_idx).erase(SYM_UNKNOWN);
    }
  }
}

template<BoardSymbol SymT>
void Board<SymT>::initialize_row_sets() {
  this->row_sets = std::vector<std::set<SymT>>(this->num_symbols);

  for (const auto &[row_idx, row] : std::views::enumerate(this->rows)) {
    this->row_sets.at(row_idx) = std::set<SymT>(row.begin(), row.end());
    this->row_sets.at(row_idx).erase(SYM_UNKNOWN);
  }
}

template<BoardSymbol SymT>
void Board<SymT>::initialize_col_sets() {
  this->col_sets = std::vector<std::set<SymT>>(this->num_symbols);

  for (const auto &[col_idx, col] : std::views::enumerate(this->cols)) {
    this->col_sets.at(col_idx) = std::set<SymT>(col.begin(), col.end());
    this->col_sets.at(col_idx).erase(SYM_UNKNOWN);
  }
}

template<BoardSymbol SymT>
void Board<SymT>::initialize_guessed_syms() {
  this->guessed_syms = NodeMapT<SymT>{};
}

template<BoardSymbol SymT>
void Board<SymT>::setup_board() {
  boxes_to_rows();
  rows_to_cols();
  initialize_box_row_sets();
  initialize_row_sets();
  initialize_col_sets();
  initialize_guessed_syms();
}

template<BoardSymbol SymT>
void Board<SymT>::set_first_backtracking_node(const BoardNode<SymT> &first_node) {
  this->first_backtracking_node = first_node;
}

template<BoardSymbol SymT>
bool Board<SymT>::parent_caused_fill(const BoardNode<SymT> &parent, const BoardNode<SymT> &child) {
  const auto &[parent_box_row_idx, parent_box_idx] = row_col_idx_to_box_idx(parent.row_idx, parent.col_idx).first;
  const auto parent_box_set = this->box_row_sets.at(parent_box_row_idx).at(parent_box_idx);
  const auto parent_row_set = this->row_sets.at(parent.row_idx);
  const auto parent_col_set = this->row_sets.at(parent.col_idx);

  const bool parent_box_full = full(parent_box_set);
  const bool parent_row_full = full(parent_row_set);
  const bool parent_col_full = full(parent_col_set);
  if (parent_box_full)
    std::println("Parent {0} made its box full", show_node(parent));
  if (parent_row_full)
    std::println("Parent {0} made its row full", show_node(parent));
  if (parent_col_full)
    std::println("Parent {0} made its col full", show_node(parent));

  return full(parent_box_set) || full(parent_row_set) || full(parent_col_set);
}

template<BoardSymbol SymT>
void Board<SymT>::print_set(const auto& set) {
  if (set.empty()) {
    std::println();
    return;
  }

  for (const auto& elem : set) {
    std::print("{0} ", static_cast<char>(elem));
  }
  std::println();
}

template<BoardSymbol SymT>
template <typename T>
void Board<SymT>::print_board(std::vector<T> all_groups) {
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

template<BoardSymbol SymT>
void Board<SymT>::print_box(BoxT<SymT> box) {
  for (size_t i = 1; i < 2 * this->num_dims; ++i)
        std::print("-");
  std::println();
  for (const auto &[sym_idx, sym] : std::views::enumerate(box))
    std::print("{0}{1}", static_cast<char>(sym), (((sym_idx + 1) % this->num_dims == 0) ? '\n' : ' '));
  for (size_t i = 1; i < 2 * this->num_dims; ++i)
    std::print("-");
  std::println();
}

template<BoardSymbol SymT>
template<typename T>
void Board<SymT>::print_group(T group) {
  for (const SymT elt : group)
    std::print("{0} ", static_cast<char>(elt));
  std::println();
}

template<BoardSymbol SymT>
const std::string Board<SymT>::show_node(const BoardNode<SymT> &node) {
  return std::format("(({0},{1}): {2})", node.row_idx, node.col_idx, static_cast<char>(node.sym));
}

template<BoardSymbol SymT>
std::pair<std::size_t, std::size_t> 
Board<SymT>::box_idx_to_row_col_idx(const size_t box_row_idx, const size_t box_idx, const size_t box_sym_idx) {
  const size_t row_idx = (box_row_idx * this->num_dims) + (box_sym_idx / this->num_dims);
  const size_t col_idx = (box_idx * this->num_dims) + (box_sym_idx % this->num_dims);
  return std::make_pair(row_idx, col_idx);
}

template<BoardSymbol SymT>
std::pair<std::pair<std::size_t, std::size_t>, std::size_t> 
Board<SymT>::row_col_idx_to_box_idx(const size_t row_idx, const size_t col_idx) {
  const size_t box_row_idx = row_idx / this->num_dims;
  const size_t box_idx = col_idx / this->num_dims;
  const size_t box_sym_idx = (this->num_dims * (row_idx % this->num_dims)) + col_idx % this->num_dims;
  return std::make_pair(std::make_pair(box_row_idx, box_idx), box_sym_idx);
}

template<BoardSymbol SymT>
template<typename TGroup> // add concepts later
std::vector<std::size_t> Board<SymT>::unknown_symbols_indices(TGroup group) {
  std::vector<std::size_t> unknown_sym_indices;
  for (const auto &[sym_idx, sym] : std::views::enumerate(group))
    if (sym == SYM_UNKNOWN)
      unknown_sym_indices.push_back(sym_idx);

  return unknown_sym_indices;
}

template<BoardSymbol SymT>
bool Board<SymT>::full(const std::set<SymT> &container) {
  return !container.contains(static_cast<SymT>(SYM_UNKNOWN)) && container.size() == this->num_symbols;
}

template<BoardSymbol SymT>
void Board<SymT>::add_box_sym(const SymT sym, const size_t box_row_idx, const size_t box_idx, 
                  const size_t box_sym_idx, bool root) {
  std::println("*** add_box_sym, sym = {0}, box_row_idx = {1}, box_idx = {2}, box_sym_idx = {3}, root = {4}",
                (char)sym, box_row_idx, box_idx, box_sym_idx, root);
  this->boxes.at(box_row_idx).at(box_idx)[box_sym_idx] = sym;
  this->box_row_sets.at(box_row_idx).at(box_idx).insert(sym);
  this->sym_found = true;

  if (!root) return;
  
  const auto [row_idx, col_idx] = box_idx_to_row_col_idx(box_row_idx, box_idx, box_sym_idx);

  add_row_sym(sym, row_idx, col_idx);
  add_col_sym(sym, col_idx, row_idx);
}

template<BoardSymbol SymT>
void Board<SymT>::add_row_sym(const SymT sym, const size_t row_idx, const size_t row_sym_idx, bool root) {
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

template<BoardSymbol SymT>
void Board<SymT>::add_col_sym(const SymT sym, const size_t col_idx, const size_t col_sym_idx, bool root) {
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

template<BoardSymbol SymT>
void Board<SymT>::del_sym(const SymT sym, const size_t row_idx, const size_t col_idx) {
  const auto &[box_indices, box_sym_idx] = row_col_idx_to_box_idx(row_idx, col_idx);
  const auto &[box_row_idx, box_idx] = box_indices;
  this->boxes.at(box_row_idx).at(box_idx)[box_sym_idx] = SYM_UNKNOWN;
  this->box_row_sets.at(box_row_idx).at(box_idx).erase(sym);
  this->rows.at(row_idx)[col_idx] = SYM_UNKNOWN;
  this->row_sets.at(row_idx).erase(sym);
  this->cols.at(col_idx)[row_idx] = SYM_UNKNOWN;
  this->col_sets.at(col_idx).erase(sym);
}

template<BoardSymbol SymT>
bool Board<SymT>::is_solved() {
  for (const BoxRowT box_row : this->boxes) {
    for (const BoxT box : box_row) {
      std::set<SymT> box_symbols{box.begin(), box.end()};
      if (!(this->symbols == box_symbols)) return false;
    }
  }

  // check all rows sets are equal to the set of all symbols used
  // assume boxrows and cols sets/vectors and rows vector updated
  for (const RowT row : this->rows) {
    std::set<SymT> row_symbols{row.begin(), row.end()};
    if (!(this->symbols == row_symbols)) return false;
  }

  for (const ColT col : this->cols) {
    std::set<SymT> col_symbols{col.begin(), col.end()};
    if (!(this->symbols == col_symbols)) return false;
  }

  return true;
}

template<BoardSymbol SymT>
Board<SymT>::Board(const char *num_dims, const char *board_file) {
  parse_board_dim(num_dims);
  const std::string &sudoku_str = parse_sudoku_file(board_file);
  
  if (const auto parsed_board = parse_board_str(sudoku_str); parsed_board.has_value())
    this->boxes = parsed_board.value();
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

} // namespace Sudoku