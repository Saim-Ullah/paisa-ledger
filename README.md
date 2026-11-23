# Paisa Ledger

A small C++17 command-line tool for answering two questions: **where did my rupees go, and am I still within budget?** Give it a CSV of expenses. It prints a report and never modifies the input. No accounts, bank integrations, runtime dependencies or network calls.

## Try it

From this directory, with a C++17 compiler and Make:

```sh
make demo
```

Or build directly (no Make required):

```sh
c++ -std=c++17 -O2 src/main.cpp -o paisa-ledger
./paisa-ledger --month 2026-11 --budget 5000 examples/transactions.csv
```

On Windows, use `g++` from MinGW-w64 in place of `c++`, output `paisa-ledger.exe`, and run `./paisa-ledger.exe`. The program itself only needs the C++17 standard library.

## Example report

The [fictional example CSV](examples/transactions.csv) spans two months. Filtering for November gives:

```text
Paisa Ledger | 2026-11
Transactions: 6
Spent:        PKR 4,586.50
Budget:       PKR 5,000.00
Remaining:    PKR 413.50

Category totals
  Groceries: PKR 2,506.00 (2)
  Learning: PKR 1,200.00 (1)
  Meals, out: PKR 480.50 (1)
  Transport: PKR 400.00 (2)
```

`(2)` means two transactions in that category. Categories are ordered by total, then name for ties.

## Use your own data

```csv
date,category,amount
2026-11-02,Groceries,1650.75
2026-11-07,"Meals, out",480.50
```

- Use these three columns in order. Dates must be real `YYYY-MM-DD` dates. Blank lines, UTF-8 BOMs and Windows line endings are accepted.
- Amounts are **positive expenses in Pakistani rupees**, with at most two decimal places, no grouping commas and no currency symbol. The tool converts them to integer paisa before doing arithmetic. Refunds, income and mixed currencies are out of scope.
- Category names are case-sensitive; surrounding spaces are trimmed. Quote a field containing a comma and write a literal quote as `""` within a quoted field. One record per line; multiline fields are not supported.
- Omit `--month` to summarize the whole file. `--budget` is an optional, non-negative budget for the selected period. Use `-` instead of a filename to read standard input.
- Every row is validated, even outside the selected month. A bad row prints its line number to stderr and returns a nonzero status **without printing a partial report**. An empty period is a valid zero-total report.
- Keep real financial data outside version control. `.gitignore` excludes CSV files other than the fictional fixture; it does not protect files already tracked by Git.

## Tests

```sh
make test
# Or, with Python 3 and a C++17 compiler on PATH:
python3 -m unittest discover -s tests -v
```

Tests exercise the actual executable: arithmetic, overflow, quoted CSV, sorting, invalid dates, filters and the exact [sample report](examples/report.txt). Set `CXX` to select a compiler. `src/main.cpp` contains separate parsing, aggregation and formatting functions, all in one portable translation unit.
