// Paisa Ledger: an offline, single-currency expense report. C++17, standard library only.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ledger {
using Money = std::int64_t; // non-negative paisa, not floating-point rupees
using Error = std::runtime_error;

std::string trim(const std::string& s) {
    auto first = s.find_first_not_of(" \t\r");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r") - first + 1);
}

bool digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

Money money(const std::string& s) {
    auto dot = s.find('.');
    auto whole = s.substr(0, dot);
    auto frac = dot == std::string::npos ? std::string{} : s.substr(dot + 1);
    if (!digits(whole) || (dot != std::string::npos && (!digits(frac) || frac.size() > 2)))
        throw Error("expected a non-negative amount such as 125 or 125.50 (no commas)");
    while (frac.size() < 2) frac += '0';
    Money value = 0;
    for (char c : whole + frac) {
        const int n = c - '0';
        if (value > (std::numeric_limits<Money>::max() - n) / 10) throw Error("amount is too large");
        value = value * 10 + n;
    }
    return value;
}

Money add(Money a, Money b) {
    if (a > std::numeric_limits<Money>::max() - b) throw Error("report total is too large");
    return a + b;
}

std::string format(Money value) {
    std::string units = std::to_string(value / 100);
    for (std::size_t pos = units.size(); pos > 3; pos -= 3) units.insert(pos - 3, ",");
    std::ostringstream s;
    s << "PKR " << units << '.' << std::setfill('0') << std::setw(2) << value % 100;
    return s.str();
}

bool month_ok(const std::string& s) {
    return s.size() == 7 && s[4] == '-' && digits(s.substr(0, 4)) && s.substr(0, 4) != "0000" &&
           digits(s.substr(5, 2)) && s.substr(5, 2) >= "01" && s.substr(5, 2) <= "12";
}

bool date_ok(const std::string& s) {
    if (s.size() != 10 || s[7] != '-' || !month_ok(s.substr(0, 7)) || !digits(s.substr(8, 2))) return false;
    const int year = std::stoi(s.substr(0, 4)), month = std::stoi(s.substr(5, 2)), day = std::stoi(s.substr(8));
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int days[] = {31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return day >= 1 && day <= days[month - 1];
}

// One record per line. A quoted field may contain commas and doubled quotes.
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> result;
    std::size_t i = 0;
    for (;;) {
        std::string field;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i < line.size() && line[i] == '"') {
            ++i;
            bool closed = false;
            while (i < line.size()) {
                char c = line[i++];
                if (c != '"') field += c;
                else if (i < line.size() && line[i] == '"') { field += '"'; ++i; }
                else { closed = true; break; }
            }
            if (!closed) throw Error("unterminated quoted field (use one record per line)");
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
            if (i < line.size() && line[i] != ',') throw Error("unexpected text after closing quote");
        } else {
            while (i < line.size() && line[i] != ',') {
                if (line[i] == '"') throw Error("unexpected quote in unquoted field");
                field += line[i++];
            }
        }
        result.push_back(trim(field));
        if (i == line.size()) return result;
        ++i; // comma; keep trailing empty fields
    }
}

struct Options { std::string file; std::optional<std::string> month; std::optional<Money> budget; };
struct Bucket { Money total = 0; std::size_t count = 0; };
struct Report { Bucket all; std::map<std::string, Bucket> by_category; };

Report read(std::istream& input, const Options& opt) {
    Report report;
    bool header = false;
    std::size_t line_no = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++line_no;
        if (line_no == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (trim(line).empty()) continue;
        try {
            auto row = fields(line);
            if (!header) {
                if (row != std::vector<std::string>{"date", "category", "amount"})
                    throw Error("expected header: date,category,amount");
                header = true;
                continue;
            }
            if (row.size() != 3) throw Error("expected exactly 3 columns");
            if (!date_ok(row[0])) throw Error("expected a real calendar date in YYYY-MM-DD format");
            if (row[1].empty() || std::any_of(row[1].begin(), row[1].end(), [](unsigned char c){return c < 32 || c == 127;}))
                throw Error("category must be non-empty and must not contain control characters");
            const Money cost = money(row[2]);
            if (cost == 0) throw Error("expense must be greater than zero");
            if (opt.month && row[0].compare(0, 7, *opt.month) != 0) continue;
            report.all.total = add(report.all.total, cost);
            ++report.all.count;
            auto& bucket = report.by_category[row[1]];
            bucket.total = add(bucket.total, cost);
            ++bucket.count;
        } catch (const Error& e) { throw Error("line " + std::to_string(line_no) + ": " + e.what()); }
    }
    if (input.bad() || (!input.eof() && input.fail())) throw Error("could not read the entire input");
    if (!header) throw Error("missing header: date,category,amount");
    return report;
}

void print(const Report& report, const Options& opt) {
    std::cout << "Paisa Ledger | " << opt.month.value_or("all dates") << '\n'
              << "Transactions: " << report.all.count << "\nSpent:        " << format(report.all.total) << '\n';
    if (opt.budget) {
        std::cout << "Budget:       " << format(*opt.budget) << '\n';
        if (report.all.total <= *opt.budget) std::cout << "Remaining:    " << format(*opt.budget - report.all.total) << '\n';
        else std::cout << "Over budget:  " << format(report.all.total - *opt.budget) << '\n';
    }
    if (!report.all.count) { std::cout << "\nNo transactions in this period.\n"; return; }
    std::vector<std::pair<std::string, Bucket>> sorted(report.by_category.begin(), report.by_category.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.second.total == b.second.total ? a.first < b.first : a.second.total > b.second.total;
    });
    std::cout << "\nCategory totals\n";
    for (const auto& item : sorted)
        std::cout << "  " << item.first << ": " << format(item.second.total) << " (" << item.second.count << ")\n";
}

constexpr const char* help = "Paisa Ledger — local spending reports\n\nUsage: paisa-ledger [--month YYYY-MM] [--budget RUPEES] FILE|-\n\nInput: date,category,amount (one PKR expense per line). Use - for stdin.\n";
Options options(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--month" || arg == "--budget") {
            if (++i == argc) throw Error("missing value for " + arg);
            std::string val = argv[i];
            if (arg == "--month") {
                if (opt.month || !month_ok(val)) throw Error("supply --month once, as YYYY-MM");
                opt.month = val;
            } else {
                if (opt.budget) throw Error("supply --budget only once");
                opt.budget = money(val);
            }
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") throw Error("unknown option (see --help)");
        else if (!opt.file.empty()) throw Error("supply exactly one CSV file or - for stdin");
        else opt.file = arg;
    }
    if (opt.file.empty()) throw Error("missing input file (see --help)");
    return opt;
}
} // namespace ledger

int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << ledger::help; return 0;
    }
    try {
        auto opt = ledger::options(argc, argv);
        std::ifstream file;
        if (opt.file != "-") { file.open(opt.file); if (!file) throw ledger::Error("cannot open input file"); }
        std::istream& in = opt.file == "-" ? std::cin : static_cast<std::istream&>(file);
        auto report = ledger::read(in, opt);
        ledger::print(report, opt);
        return 0;
    } catch (const std::exception& e) { std::cerr << "paisa-ledger: " << e.what() << '\n'; return 1; }
}
