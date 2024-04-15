#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <vector>

std::vector<std::string> read_entire_file(const std::string& filename) {
  std::ifstream inputFile(filename);
  if (not inputFile.is_open()) {
    std::cout << "Error: opening the file " << filename << std::endl;
    exit(1);
  }

  std::string line;
  std::vector<std::string> lines;
  while (getline(inputFile, line)) lines.push_back(line);

  inputFile.close();
  return lines;
}

void write_to_file(const std::string& filename,
                   const std::vector<std::string> lines) {
  std::ofstream outputFile(filename);
  if (not outputFile.is_open()) {
    std::cout << "Error: opening the file " << filename << std::endl;
    exit(1);
  }

  for (auto line : lines) {
    outputFile << line << '\n';
  }

  outputFile.close();
}

const char IGNORE = '=';
const char ADD = '+';
const char REMOVE = '-';
const char SUBST = 'x';
const char WILD = '%';

struct Move {
  char action;
  int n;
  std::string line;
  Move() : n(-1) {}
  Move(char action, int n, std::string line)
      : action(action), n(n), line(line) {}
};

std::vector<Move> edit_distance(std::vector<std::string> src,
                                std::vector<std::string> dst) {
  int m1 = src.size();
  int m2 = dst.size();

  std::vector<std::vector<int>> distances(m1 + 1, std::vector<int>(m2 + 1, 0));
  std::vector<std::vector<char>> actions(m1 + 1,
                                         std::vector<char>(m2 + 1, WILD));

  distances[0][0] = 0;
  actions[0][0] = IGNORE;

  for (int j = 1; j < 1 + m2; ++j) {
    distances[0][j] = j;
    actions[0][j] = ADD;
  }

  for (int i = 1; i < 1 + m1; ++i) {
    distances[i][0] = i;
    actions[i][0] = REMOVE;
  }

  for (int i = 1; i < 1 + m1; ++i) {
    for (int j = 1; j < 1 + m2; ++j) {
      if (src[i - 1] == dst[j - 1]) {
        distances[i][j] = distances[i - 1][j - 1];
        actions[i][j] = IGNORE;

        continue;
      }

      int remove = distances[i - 1][j];
      int add = distances[i][j - 1];
      // int subst = distances[i - 1][j - 1];

      distances[i][j] = remove;
      actions[i][j] = REMOVE;

      if (distances[i][j] > add) {
        distances[i][j] = add;
        actions[i][j] = ADD;
      }

      // if (distances[i][j] > subst) {
      //   distances[i][j] = subst;
      //   actions[i][j] = SUBST;
      // }

      distances[i][j] += 1;
    }
  }
  std::vector<Move> patch;
  int i = m1, j = m2;
  while (i > 0 or j > 0) {
    char action = actions[i][j];
    if (action == ADD) {
      j--;
      patch.push_back(Move(ADD, j, dst[j]));
    } else if (action == REMOVE) {
      i--;
      patch.push_back(Move(REMOVE, i, src[i]));
    } else if (action == IGNORE) {
      i--, j--;  // patch.push_back(Move(IGNORE,src[i]));
    } else {
      assert(false && "Unreachable");
    }
  }

  std::reverse(patch.begin(), patch.end());

  return patch;
}

namespace ref {
#define head(s) (s[0])
#define tail(s) (std::string(s.begin() + 1, s.end()))

std::map<std::pair<std::string, std::string>, int> mp;

int lev(std::string src, std::string dst) {
  if (src.length() == 0) return dst.length();
  if (dst.length() == 0) return src.length();
  if (mp.count({src, dst})) return mp[{src, dst}];
  if (head(src) == head(dst))
    return mp[{src, dst}] = lev(tail(src), tail(dst));  // ignore
  int dist = 1 + std::min({
                     lev(tail(src), dst),       // remove
                     lev(src, tail(dst)),       // add
                     lev(tail(src), tail(dst))  // replace
                 });
  return mp[{src, dst}] = dist;
}
};  // namespace ref

class Subcommand {
 public:
  std::string name;
  std::string signature;
  std::string description;

  Subcommand(std::string name, std::string signature, std::string description)
      : name(name), signature(signature), description(description) {}
  virtual int run(std::string program, std::vector<std::string> args) {
    assert(false && "Not implemented");
    return 0;
  }
};

std::vector<Subcommand*> SUBCOMMANDS;

void usage(std::string program) {
  std::cout << "Usage: " << program << " <SUBCOMMAND> [OPTIONS]\n";
  std::cout << "Subcommands:\n";

  std::vector<int> lengths;
  for (auto sub_cmd : SUBCOMMANDS) {
    std::string str = sub_cmd->name + " " + sub_cmd->signature;
    lengths.push_back(str.length());
  }

  int width = *std::max_element(lengths.begin(), lengths.end());

  for (auto sub_cmd : SUBCOMMANDS) {
    std::string command = sub_cmd->name + " " + sub_cmd->signature;

    std::cout << "\t" << std::setw(width) << command << "\t"
              << sub_cmd->description << "\n";
  }
}

class DiffSubcommand : public Subcommand {
 public:
  DiffSubcommand()
      : Subcommand("diff", "<file1> <file2>",
                   "print the difference between the files to stdout") {}

  int run(std::string program, std::vector<std::string> args) override {
    if (args.size() < 2) {
      std::string usage = "Usage: " + program + " " + name + " " + signature;
      std::string error = "ERROR: not enough files were provided to " + name;

      std::cout << usage << '\n';
      std::cout << error << '\n';

      return -1;
    }

    std::string file_path1 = args[0];
    std::string file_path2 = args[1];

    auto lines1 = read_entire_file(file_path1);
    auto lines2 = read_entire_file(file_path2);

    auto patch = edit_distance(lines1, lines2);

    for (auto [action, n, line] : patch) {
      std::string str;
      str.append(std::string(1, action));
      str.append(" ");
      str.append(std::to_string(n));
      str.append(" ");
      str.append(line);

      std::cout << str << std::endl;
    }

    return 0;
  }
};

std::regex pattern(R"(([AR]) (\d+) ( *.*))");

class PatchSubcommand : public Subcommand {
 public:
  PatchSubcommand()
      : Subcommand("patch", "<file> <file.patch>",
                   "patch the file with the given patch") {}

  int run(std::string program, std::vector<std::string> args) override {
    if (args.size() < 2) {
      std::string usage = "Usage: " + program + " " + name + " " + signature;
      std::string error = "ERROR: not enough files were provided to " + name;

      std::cout << usage << '\n';
      std::cout << error << '\n';

      return -1;
    }

    std::string file_path = args[0];
    std::string patch_path = args[1];

    auto lines1 = read_entire_file(file_path);
    auto lines2 = read_entire_file(patch_path);

    bool ok = true;
    std::vector<Move> patch;

    for (int row = 0; row < lines2.size(); ++row) {
      auto line = lines2[row];
      if (line.size() == 0) continue;

      std::istringstream iss(line);

      Move record;

      iss >> record.action;
      iss >> record.n;

      if (record.action != ADD and record.action != REMOVE and record.n != -1) {
        std::string error = patch_path + ":" + std::to_string(row + 1) +
                            ": Invalid patch action: " + line;
        std::cout << error << '\n';
        ok = false;
        continue;
      }

      std::getline(iss, record.line);
      if (record.line[0] == ' ') record.line.erase(record.line.begin());
      patch.push_back(record);
    }

    if (not ok) {
      return -1;
    }

    std::sort(patch.begin(), patch.end(), [](auto p1, auto p2) {
      if (p1.n == p2.n) return p1.action < p2.action;
      return p1.n < p2.n;
    });

    std::reverse(patch.begin(), patch.end());

    for (auto [action, row, line] : patch) {
      if (action == REMOVE) {
        lines1.erase(lines1.begin() + row);
      } else if (action == ADD) {
        lines1.insert(lines1.begin() + row, line);
      } else {
        assert(false && "unreachable");
      }
    }

    for (auto line : lines1) {
      std::cout << line << std::endl;
    }

    write_to_file("_" + file_path, lines1);

    return 0;
  }
};

Subcommand* find_subcommand(std::string sub_cmd_name) {
  for (auto sub_cmd : SUBCOMMANDS) {
    if (sub_cmd->name == sub_cmd_name) {
      return sub_cmd;
    }
  }
  return nullptr;
}

void suggest_closet_subcommand_if_exists(std::string sub_cmd_name) {
  std::vector<std::string> candidates;
  for (auto sub_cmd : SUBCOMMANDS) {
    if (ref::lev(sub_cmd_name, sub_cmd->name) < 3) {
      candidates.push_back(sub_cmd->name);
    }
  }

  if (candidates.size() > 0) {
    std::cout << "Maybe you meant:\n";
    for (auto name : candidates) {
      std::cout << "\t" << name << "\n";
    }
  }
}

class HelpSubcommand : public Subcommand {
 public:
  HelpSubcommand()
      : Subcommand("help", "[subcommand]", "print this help message") {}

  int run(const std::string program, std::vector<std::string> args) override {
    if (args.size() == 0) {
      usage(program);
      return 0;
    }

    std::string sub_cmd_name = args[0];

    auto sub_cmd = find_subcommand(sub_cmd_name);
    if (not(sub_cmd)) {
      std::cout << "Usage: " << program << " " << sub_cmd->name << " "
                << sub_cmd->signature << '\n';
      std::cout << "\t\t" << sub_cmd->description << '\n';
      return 0;
    }

    usage(program);
    std::cout << "ERROR: Unknown subcommand " << sub_cmd_name << std::endl;
    suggest_closet_subcommand_if_exists(sub_cmd_name);

    return -1;
  }
};

int32_t main(int argc, char** argv) {
  assert(argc > 0 && "No Arguments");

  SUBCOMMANDS = {new DiffSubcommand(), new PatchSubcommand(),
                 new HelpSubcommand()};

  std::string program = argv[0];
  std::vector<std::string> args(argv + 1, argv + argc);

  if (args.size() == 0) {
    usage(program);
    std::cout << "ERROR: No Subcommand is provided" << std::endl;

    return 0;
  }

  std::string sub_cmd_name = args[0];
  args.erase(args.begin());

  auto sub_cmd = find_subcommand(sub_cmd_name);

  if (sub_cmd) {
    return sub_cmd->run(program, args);
  }

  usage(program);
  std::cout << "ERROR: Unknown subcommand " << sub_cmd_name << std::endl;
  suggest_closet_subcommand_if_exists(sub_cmd_name);

  return -1;
}