#include <iostream>
#include <print>

#include "Solver.hpp"

int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: ./soduko <n> <infile> [outfile]" << '\n';
    return EXIT_FAILURE;
  }

  const char *outfile = (argc == 4) ? argv[3] : "result.txt";
  auto board = Sudoku::Solver<std::uint16_t>(argv[1], argv[2], outfile);
  bool solved = board.solve();
  std::println("{0}", (solved ? "Solved" : "Failed"));
  std::println("{0}", board.write_answer());

  return EXIT_SUCCESS;
}