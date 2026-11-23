"""End-to-end tests using only the Python standard library and a C++17 compiler."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'date,category,amount\n'


class LedgerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        external = os.environ.get('LEDGER_BIN')
        cls.exe = Path(external).resolve() if external else Path(cls.tmp.name) / ('paisa-ledger.exe' if os.name == 'nt' else 'paisa-ledger')
        if not external:
            compiler = shlex.split(os.environ.get('CXX', 'c++'))
            subprocess.run([*compiler, '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', str(ROOT / 'src/main.cpp'), '-o', str(cls.exe)], check=True)

    def invoke(self, *args, data=''):
        return subprocess.run([str(self.exe), *args], input=data.encode('utf-8'), capture_output=True, timeout=5)

    def ok(self, *args, data=''):
        p = self.invoke(*args, data=data)
        self.assertEqual(p.returncode, 0, p.stderr)
        return p.stdout.decode()

    def test_sample_matches_published_report(self):
        actual = self.ok('--month', '2026-11', '--budget', '5000', str(ROOT / 'examples/transactions.csv'))
        self.assertEqual(actual.strip(), (ROOT / 'examples/report.txt').read_text().strip())

    def test_exact_decimal_arithmetic(self):
        out = self.ok('-', '--budget', '0.30', data=HEADER + '2024-02-29,A,0.1\n2024-02-29,A,0.20\n')
        self.assertIn('Spent:        PKR 0.30', out)
        self.assertIn('Remaining:    PKR 0.00', out)

    def test_bom_crlf_and_csv_quotes(self):
        out = self.ok('-', data='\ufeff' + HEADER.replace('\n','\r\n') + '\r\n2024-01-01,"Meals, ""out""",90.5\r\n')
        self.assertIn('Meals, "out": PKR 90.50 (1)', out)

    def test_zero_budget_and_excess(self):
        out = self.ok('--budget', '0', '-', data=HEADER + '2024-01-01,Trip,1500.40\n')
        self.assertIn('Over budget:  PKR 1,500.40', out)

    def test_empty_period(self):
        out = self.ok('-', '--month', '2025-03', data=HEADER + '2025-04-01,A,5\n')
        self.assertIn('Transactions: 0', out)
        self.assertIn('No transactions in this period.', out)

    def test_century_leap_year(self):
        self.assertIn('PKR 1.00', self.ok('-', data=HEADER+'2000-02-29,A,1\n'))

    def test_bad_rows_return_line_no_and_no_partial_report(self):
        cases = ['2026-02-29,X,1', '1900-02-29,X,1', '2026-04-31,X,1', '0000-01-01,X,1',
                 '2024-12-00,X,1', '2024-13-01,X,1', '2024-01-01,,1', '2024-01-01,A,',
                 '2024-01-01,A,0', '2024-01-01,A,-1', '2024-01-01,A,1.005', '2024-01-01,A,1.',
                 '2024-01-01,A,1e3', '2024-01-01,A,1,2', '2024-01-01,"unclosed,1',
                 '2024-01-01,"name"junk,1', '2024-01-01,na"me,1', '2024-01-01,A\x1b,1',
                 '2024-01-01,A,92233720368547758.08']
        for row in cases:
            with self.subTest(row=row):
                p = self.invoke('-', data=HEADER+'2024-01-01,T,1\n'+row+'\n')
                self.assertNotEqual(p.returncode,0); self.assertIn(b'line 3:',p.stderr); self.assertFalse(p.stdout)

    def test_invalid_excluded_row_still_fails(self):
        p=self.invoke('--month','2025-01','-',data=HEADER+'2024-02-31,A,2\n')
        self.assertNotEqual(p.returncode,0); self.assertIn(b'line 2:',p.stderr)

    def test_sum_overflow_is_rejected(self):
        p=self.invoke('-',data=HEADER+'2024-01-01,A,92233720368547758.07\n2024-01-01,B,0.01\n')
        self.assertIn(b'total is too large',p.stderr); self.assertNotEqual(p.returncode,0)

    def test_equal_totals_have_stable_order(self):
        out=self.ok('-',data=HEADER+'2024-01-01,Z,1\n2024-01-01,A,1\n')
        self.assertLess(out.index('  A:'),out.index('  Z:'))

    def test_header_and_args(self):
        for args,data in [(['-'],''), (['-'],'amount,date,category\n'), ([],''), (['--budget'],''),
                          (['--month','2024-13','-'],HEADER), (['--budget','0','--budget','1','-'],HEADER),
                          (['--unknown','-'],HEADER), (['a','b'],''), ([str(Path(self.tmp.name)/'missing')],'')]:
            with self.subTest(args=args,data=data): self.assertNotEqual(self.invoke(*args,data=data).returncode,0)
        self.assertIn('Usage:',self.ok('--help'))


if __name__ == '__main__':
    unittest.main()
