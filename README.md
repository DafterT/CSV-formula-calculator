# CSV Formula Calculator

Этап 1 CSV-калькулятора на C: программа читает CSV-файл, валидирует структуру и строит внутреннюю таблицу. Формулы на этом этапе не вычисляются: поля, начинающиеся с `=`, сохраняются как raw-строки.

## Требования

- Linux или WSL.
- `gcc` или `clang`.
- CMake 3.16+.
- Только стандартная библиотека C, без сторонних зависимостей.

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

## Запуск

```bash
./build/csvreader tests/valid_basic.csv
```

При успешном чтении CLI печатает распарсенную таблицу в CSV-формате. Числа выводятся как `int64_t`, формулы выводятся в исходном raw-виде.

## Формат CSV

- Первая строка: пустая первая ячейка, затем имена колонок, например `,A,B,Cell`.
- Имя колонки: только латинские буквы и `_`, без цифр, пробелов и знаков препинания.
- Первый столбец data-строки: положительный номер строки.
- Номера строк могут идти в любом порядке, порядок входа сохраняется.
- Ячейка: число `0`, `10`, `-10` или формула, начинающаяся с `=`.
- Quoted CSV не поддерживается, любой `"` считается ошибкой.
- Разделители строк: `\n` и `\r\n`; одиночный `\r` запрещен.
- Пустые ячейки, пустые строки, разная ширина строк, дубли колонок и строк запрещены.

## Ошибки

На ошибке программа печатает в `stderr`:

```text
error: <message>
```

Код возврата при ошибке ненулевой. Парсер сам ничего не печатает и возвращает ошибку через `CsvError`.

## Тесты

Unit tests:

```bash
ctest --test-dir build --output-on-failure
```

Ручные CSV-fixtures:

- `tests/valid_basic.csv`
- `tests/valid_dependencies.csv`
- `tests/invalid_header_only.csv`
- `tests/invalid_empty_file.csv`
- `tests/invalid_bad_column.csv`
- `tests/invalid_duplicate_column.csv`
- `tests/invalid_bad_row_number.csv`
- `tests/invalid_duplicate_row.csv`
- `tests/invalid_malformed_width.csv`
- `tests/invalid_empty_cell.csv`
