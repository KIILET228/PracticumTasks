#pragma once

#include <boost/json.hpp>
#include <pqxx/pqxx>
#include <string>

namespace db {

// Обёртка над подключением к БД, реализующая операции над таблицей books.
// Гарантирует, что все запросы, содержащие пользовательский ввод, выполняются
// через подготовленные запросы (защита от SQL-инъекций).
class BookManager {
public:
    explicit BookManager(const std::string& db_url);

    BookManager(const BookManager&) = delete;
    BookManager& operator=(const BookManager&) = delete;

    // Добавляет книгу, описанную в payload (ключи title, author, year, ISBN).
    // ISBN может быть JSON null - в этом случае в базу записывается NULL.
    // Возвращает false, если операция завершилась ошибкой на стороне БД
    // (например, из-за нарушения уникальности ISBN).
    bool AddBook(const boost::json::object& payload);

    // Возвращает все книги, отсортированные по убыванию года, а затем по
    // возрастанию названия, автора и ISBN.
    boost::json::array GetAllBooks();

private:
    pqxx::connection connection_;

    void CreateTable();
    void PrepareStatements();
};

}  // namespace db
